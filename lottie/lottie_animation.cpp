// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_animation.h"

#include "lottie/lottie_frame_renderer.h"
#include "lottie/lottie_player.h"
#include "base/algorithm.h"
#include "zlib.h"

#ifdef LOTTIE_USE_CACHE
#include "lottie/lottie_cache.h"
#endif // LOTTIE_USE_CACHE

#include <QFile>
#include <rlottie.h>
#include <crl/crl_async.h>
#include <crl/crl_on_main.h>

namespace Lottie {
namespace {

const auto kIdealSize = QSize(512, 512);

std::string UnpackGzip(const QByteArray &bytes) {
	const auto original = [&] {
		return std::string(bytes.constData(), bytes.size());
	};
	z_stream stream;
	stream.zalloc = nullptr;
	stream.zfree = nullptr;
	stream.opaque = nullptr;
	stream.avail_in = 0;
	stream.next_in = nullptr;
	int res = inflateInit2(&stream, 16 + MAX_WBITS);
	if (res != Z_OK) {
		return original();
	}
	const auto guard = gsl::finally([&] { inflateEnd(&stream); });

	auto result = std::string(kMaxFileSize + 1, char(0));
	stream.avail_in = bytes.size();
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.data()));
	stream.avail_out = 0;
	while (!stream.avail_out) {
		stream.avail_out = result.size();
		stream.next_out = reinterpret_cast<Bytef*>(result.data());
		int res = inflate(&stream, Z_NO_FLUSH);
		if (res != Z_OK && res != Z_STREAM_END) {
			return original();
		} else if (!stream.avail_out) {
			return original();
		}
	}
	result.resize(result.size() - stream.avail_out);
	return result;
}

std::optional<Error> ContentError(const QByteArray &content) {
	if (content.size() > kMaxFileSize) {
		return Error::ParseFailed;
	}
	return std::nullopt;
}

details::InitData CheckSharedState(std::unique_ptr<SharedState> state) {
	Expects(state != nullptr);

	auto information = state->information();
	if (!information.frameRate
		|| information.framesCount <= 0
		|| information.size.isEmpty()) {
		return Error::NotSupported;
	}
	return state;
}

details::InitData Init(
		const QByteArray &content,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements) {
	if (const auto error = ContentError(content)) {
		return *error;
	}
	auto animation = details::CreateFromContent(content, replacements);
	return animation
		? CheckSharedState(std::make_unique<SharedState>(
			std::move(animation),
			request.empty() ? FrameRequest{ kIdealSize } : request,
			quality))
		: Error::ParseFailed;
}

#ifdef LOTTIE_USE_CACHE
details::InitData Init(
		const QByteArray &content,
		FnMut<void(QByteArray &&cached)> put,
		const QByteArray &cached,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements) {
	Expects(!request.empty());

	if (const auto error = ContentError(content)) {
		return *error;
	}
	auto cache = std::make_unique<Cache>(cached, request, std::move(put));
	const auto prepare = !cache->framesCount()
		|| (cache->framesReady() < cache->framesCount());
	auto animation = prepare
		? details::CreateFromContent(content, replacements)
		: nullptr;
	return (!prepare || animation)
		? CheckSharedState(std::make_unique<SharedState>(
			content,
			replacements,
			std::move(animation),
			std::move(cache),
			request,
			quality))
		: Error::ParseFailed;
}
#endif // LOTTIE_USE_CACHE

} // namespace

namespace details {

std::unique_ptr<rlottie::Animation> CreateFromContent(
		const QByteArray &content,
		const ColorReplacements *replacements) {
	const auto string = UnpackGzip(content);
	Assert(string.size() <= kMaxFileSize);

#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE
	auto result = rlottie::Animation::loadFromData(
		string,
		std::string(),
		std::string(),
		false,
		(replacements
			? replacements->replacements
			: std::vector<std::pair<std::uint32_t, std::uint32_t>>()));
#else
	auto result = rlottie::Animation::loadFromData(
		string,
		std::string(),
		std::string(),
		false);
#endif
	return result;
}

} // namespace details

std::shared_ptr<FrameRenderer> MakeFrameRenderer() {
	return FrameRenderer::CreateIndependent();
}

QImage ReadThumbnail(const QByteArray &content) {
	return Init(content, FrameRequest(), Quality::High, nullptr).match([](
			const std::unique_ptr<SharedState> &state) {
		return state->frameForPaint()->original;
	}, [](Error) {
		return QImage();
	});
}

Animation::Animation(
	not_null<Player*> player,
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
: _player(player) {
	if (quality == Quality::Synchronous) {
		initDone(Init(content, request, quality, replacements));
	} else {
		const auto weak = base::make_weak(this);
		crl::async([=] {
			auto result = Init(content, request, quality, replacements);
			crl::on_main(weak, [=, data = std::move(result)]() mutable {
				initDone(std::move(data));
			});
		});
	}
}

Animation::Animation(
	not_null<Player*> player,
	FnMut<void(FnMut<void(QByteArray &&cached)>)> get, // Main thread.
	FnMut<void(QByteArray &&cached)> put, // Unknown thread.
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
#ifdef LOTTIE_USE_CACHE
: _player(player) {
	const auto weak = base::make_weak(this);
	get([=, put = std::move(put)](QByteArray &&cached) mutable {
		crl::async([=, put = std::move(put)]() mutable {
			auto result = Init(
				content,
				std::move(put),
				cached,
				request,
				quality,
				replacements);
			crl::on_main(weak, [=, data = std::move(result)]() mutable {
				initDone(std::move(data));
			});
		});
	});
#else // LOTTIE_USE_CACHE
: Animation(player, content, request, quality, replacements) {
#endif // LOTTIE_USE_CACHE
}

bool Animation::ready() const {
	return (_state != nullptr);
}

void Animation::initDone(details::InitData &&data) {
	data.match([&](std::unique_ptr<SharedState> &state) {
		parseDone(std::move(state));
	}, [&](Error error) {
		parseFailed(error);
	});
}

void Animation::parseDone(std::unique_ptr<SharedState> state) {
	Expects(state != nullptr);

	_state = state.get();
	_player->start(this, std::move(state));
}

void Animation::parseFailed(Error error) {
	_player->failed(this, error);
}

QImage Animation::frame() const {
	Expects(_state != nullptr);

	return PrepareFrameByRequest(_state->frameForPaint(), true);
}

QImage Animation::frame(const FrameRequest &request) const {
	Expects(_state != nullptr);

	const auto frame = _state->frameForPaint();
	const auto changed = (frame->request != request);
	if (changed) {
		frame->request = request;
		_player->updateFrameRequest(this, request);
	}
	return PrepareFrameByRequest(frame, !changed);
}

auto Animation::frameInfo(const FrameRequest &request) const -> FrameInfo {
	Expects(_state != nullptr);

	const auto frame = _state->frameForPaint();
	const auto changed = (frame->request != request);
	if (changed) {
		frame->request = request;
		_player->updateFrameRequest(this, request);
	}
	return {
		PrepareFrameByRequest(frame, !changed),
		frame->index % _state->framesCount()
	};
}

Information Animation::information() const {
	Expects(_state != nullptr);

	return _state->information();
}

} // namespace Lottie
