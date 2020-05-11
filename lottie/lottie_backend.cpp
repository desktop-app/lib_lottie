// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_backend.h"

#include "zlib.h"
#include "base/integration.h"

#ifdef LOTTIE_USE_CACHE
#include "lottie/lottie_cache.h"
#endif // LOTTIE_USE_CACHE

#include <QFile>
#include <crl/crl_async.h>
#include <crl/crl_on_main.h>

#ifdef LOTTIE_USE_SKOTTIE

#ifdef foreach
#undef foreach
#endif // foreach

#include "modules/skottie/include/Skottie.h"
#include "include/core/SkColor.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"

#else // LOTTIE_USE_SKOTTIE

#include <rlottie.h>

#endif // LOTTIE_USE_SKOTTIE

namespace Lottie {
namespace details {
namespace {

static auto Count = 0;
static auto Time = crl::profile_time();

[[nodiscard]] std::string UnpackGzip(const QByteArray &bytes) {
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

} // namespace

#ifdef LOTTIE_USE_SKOTTIE

BackendClass::BackendClass(sk_sp<skottie::Animation> data)
: data(std::move(data)) {
}

BackendClass::~BackendClass() = default;

std::unique_ptr<BackendClass> CreateFromContent(
		const QByteArray &content,
		const ColorReplacements *replacements) {
	const auto string = UnpackGzip(content);
	Assert(string.size() <= kMaxFileSize);

	return std::make_unique<BackendClass>(
		skottie::Animation::Make(string.data(), string.length()));
}

QSize GetLottieFrameSize(not_null<BackendClass*> animation) {
	const auto size = animation->data->size();
	return { int(qRound(size.fWidth)), int(qRound(size.fHeight)) };
}

int GetLottieFrameRate(not_null<BackendClass*> animation) {
	return int(qRound(animation->data->fps()));
}

int GetLottieFramesCount(not_null<BackendClass*> animation) {
	return int(qRound(animation->data->duration() * animation->data->fps()));
}

void RenderLottieFrame(
		not_null<BackendClass*> animation,
		int index,
		void *bytes,
		int width,
		int height,
		int bytesPerLine) {
	const auto start = crl::profile();
	if (animation->bytes != bytes
		|| animation->width != width
		|| animation->height != height
		|| animation->bytesPerLine != bytesPerLine) {
		animation->bytes = bytes;
		animation->width = width;
		animation->height = height;
		animation->bytesPerLine = bytesPerLine;
		auto bitmap = SkBitmap();
		bitmap.installPixels(
			SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType),
			bytes,
			bytesPerLine);
		animation->canvas.emplace(bitmap);
	}
	const auto destination = SkRect::MakeWH(width, height);
	animation->data->seekFrame(double(index));
	animation->data->render(
		&*animation->canvas,
		&destination,
		skottie::Animation::kSkipTopLevelIsolation);
	if (width == 456 && height == 456) {
		const auto delta = crl::profile() - start;
		++Count;
		Time += delta;
		base::Integration::Instance().logMessage(QString("FRAME: %1 (AVG: %2)").arg(delta).arg(Time / Count));
	}
}

#else // LOTTIE_USE_SKOTTIE

std::unique_ptr<BackendClass> CreateFromContent(
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
#else // DESKTOP_APP_USE_PACKAGED_RLOTTIE
	auto result = rlottie::Animation::loadFromData(
		string,
		std::string(),
		std::string(),
		false);
#endif // DESKTOP_APP_USE_PACKAGED_RLOTTIE

	return result;
}

QSize GetLottieFrameSize(not_null<BackendClass*> animation) {
	auto width = size_t(0);
	auto height = size_t(0);
	animation->size(width, height);
	return { int(width), int(height) };
}

int GetLottieFrameRate(not_null<BackendClass*> animation) {
	return int(qRound(animation->frameRate()));
}

int GetLottieFramesCount(not_null<BackendClass*> animation) {
	return int(animation->totalFrame());
}

void RenderLottieFrame(
		not_null<BackendClass*> animation,
		int index,
		void *bytes,
		int width,
		int height,
		int bytesPerLine) {
	const auto start = crl::profile();
	animation->renderSync(
		index,
		rlottie::Surface(
			static_cast<uint32_t*>(bytes),
			width,
			height,
			bytesPerLine));
	if (width == 456 && height == 456) {
		const auto delta = crl::profile() - start;
		++Count;
		Time += delta;
		base::Integration::Instance().logMessage(QString("FRAME: %1 (AVG: %2)").arg(delta).arg(Time / Count));
	}
}

#endif // LOTTIE_USE_SKOTTIE

} // namespace details
} // namespace Lottie
