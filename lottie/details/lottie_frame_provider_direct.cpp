// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_direct.h"

#include "lottie/details/lottie_frame_renderer.h"
#include "lottie/details/lottie_instance.h"
#include "ui/image/image_prepare.h"

namespace Lottie {
namespace {

// A 60fps animation is played at half the rate unless asked otherwise, so
// every second frame of it is the one actually rendered.
int GetLottieFrameMultiplier(not_null<Instance*> instance, Quality quality) {
	const auto rate = int(qRound(instance->frameRate()));
	return (quality == Quality::Default && rate == 60) ? 2 : 1;
}

} // namespace

FrameProviderDirect::FrameProviderDirect(Quality quality)
: _quality(quality) {
}

FrameProviderDirect::~FrameProviderDirect() = default;

bool FrameProviderDirect::load(
		const QByteArray &content,
		const ColorReplacements *replacements) {
	_information = Information();

	const auto string = ReadUtf8(Images::UnpackGzip(content));
	if (string.size() > kMaxFileSize) {
		return false;
	}

	_instance = Instance::Create(string, replacements);
	if (!_instance) {
		return false;
	}
	_multiplier = GetLottieFrameMultiplier(_instance.get(), _quality);
	const auto rate = int(qRound(_instance->frameRate()));
	const auto count = _instance->framesCount();
	return setInformation({
		.size = _instance->size(),
		.frameRate = rate / _multiplier,
		.framesCount = (count + _multiplier - 1) / _multiplier,
	});
}

bool FrameProviderDirect::loaded() const {
	return (_instance != nullptr);
}

void FrameProviderDirect::unload() {
	_instance = nullptr;
}

bool FrameProviderDirect::setInformation(Information information) {
	if (information.size.isEmpty()
		|| information.size.width() > kMaxSize
		|| information.size.height() > kMaxSize
		|| !information.frameRate
		|| information.frameRate > kMaxFrameRate
		|| !information.framesCount
		|| information.framesCount > kMaxFramesCount) {
		return false;
	}
	_information = information;
	return true;
}

const Information &FrameProviderDirect::information() {
	return _information;
}

bool FrameProviderDirect::valid() {
	return _information.framesCount > 0;
}

QImage FrameProviderDirect::construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) {
	auto cover = QImage();
	render(token, cover, request, 0);
	return cover;
}

int FrameProviderDirect::sizeRounding() {
	return 2;
}

bool FrameProviderDirect::render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) {
	if (token && !token->exclusive) {
		token->result = FrameRenderResult::NotReady;
		return false;
	} else if (!valid()) {
		return false;
	}
	const auto original = information().size;
	const auto size = request.box.isEmpty()
		? original
		: request.size(original, sizeRounding());
	if (!GoodStorageForFrame(to, size)) {
		to = CreateFrameStorage(size);
	}
	renderToPrepared(to, index);
	return true;
}

void FrameProviderDirect::renderToPrepared(
		QImage &to,
		int index) const {
	_instance->renderToPrepared(to, index * _multiplier);
}

} // namespace Lottie
