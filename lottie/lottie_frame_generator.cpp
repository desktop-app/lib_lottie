// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_frame_generator.h"

#include "lottie/lottie_common.h"
#include "lottie/lottie_wrap.h"
#include "ui/image/image_prepare.h"

#include <rlottie.h>

namespace Lottie {

FrameGenerator::FrameGenerator(const QByteArray &bytes)
: _rlottie(
	LoadAnimationFromData(
		ReadUtf8(Images::UnpackGzip(bytes)),
		std::string(),
		std::string(),
		false)) {
	if (_rlottie) {
		const auto rate = _rlottie->frameRate();
		_multiplier = (rate == 60) ? 2 : 1;
		auto width = size_t();
		auto height = size_t();
		_rlottie->size(width, height);
		_size = QSize(width, height);
		_framesCount = (_rlottie->totalFrame() + _multiplier - 1)
			/ _multiplier;
		_frameDuration = (rate > 0) ? (1000 * _multiplier / rate) : 0;
	}
	if (!_framesCount || !_frameDuration || _size.isEmpty()) {
		_rlottie = nullptr;
		_framesCount = _frameDuration = 0;
		_size = QSize();
	}
}

FrameGenerator::~FrameGenerator() = default;

int FrameGenerator::count() {
	return _framesCount;
}

double FrameGenerator::rate() {
	return _rlottie ? (_rlottie->frameRate() / _multiplier) : 0.;
}

FrameGenerator::Frame FrameGenerator::renderNext(
		QImage storage,
		QSize size,
		Qt::AspectRatioMode mode) {
	if (!_framesCount || _frameIndex == _framesCount) {
		return {};
	}
	++_frameIndex;
	return renderCurrent(std::move(storage), size, mode);
}

FrameGenerator::Frame FrameGenerator::renderCurrent(
		QImage storage,
		QSize size,
		Qt::AspectRatioMode mode) {
	Expects(_frameIndex > 0);

	const auto index = _frameIndex - 1;
	if (storage.format() != kImageFormat
		|| storage.size() != size) {
		storage = CreateFrameStorage(size);
	}
	storage.fill(Qt::transparent);
	const auto scaled = _size.scaled(size, mode);
	const auto render = QSize(
		std::max(scaled.width(), size.width()),
		std::max(scaled.height(), size.height()));
	const auto xskip = (size.width() - render.width()) / 2;
	const auto yskip = (size.height() - render.height());
	const auto skip = (yskip * storage.bytesPerLine() / 4) + xskip;
	auto surface = rlottie::Surface(
		reinterpret_cast<uint32_t*>(storage.bits()) + skip,
		render.width(),
		render.height(),
		storage.bytesPerLine());
	_rlottie->renderSync(index * _multiplier, std::move(surface));
	return {
		.duration = _frameDuration,
		.image = std::move(storage),
		.last = (_frameIndex == _framesCount),
	};
}

void FrameGenerator::jumpToStart() {
	_frameIndex = 0;
}

} // namespace Lottie
