// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_frame_generator.h"

#include "lottie/details/lottie_frame_renderer.h"
#include "lottie/details/lottie_instance.h"
#include "lottie/lottie_common.h"
#include "ui/image/image_prepare.h"

#include <QtGui/QPainter>

namespace Lottie {

FrameGenerator::FrameGenerator(const QByteArray &bytes)
: _instance(Instance::Create(ReadUtf8(Images::UnpackGzip(bytes)), nullptr)) {
	if (_instance) {
		const auto rate = _instance->frameRate();
		_multiplier = (rate == 60) ? 2 : 1;
		_size = _instance->size();
		_framesCount = (_instance->framesCount() + _multiplier - 1)
			/ _multiplier;
		_frameDuration = (rate > 0) ? (1000 * _multiplier / rate) : 0;
	}
	// The animation states its own size and frame count, and a hostile one
	// can state a frame count large enough that reserving a cache for it
	// exhausts memory, so the same bounds the sticker path applies hold here.
	if (!_framesCount
		|| _framesCount > kMaxFramesCount
		|| !_frameDuration
		|| _size.isEmpty()
		|| _size.width() > kMaxSize
		|| _size.height() > kMaxSize) {
		_instance = nullptr;
		_framesCount = _frameDuration = 0;
		_size = QSize();
	}
}

FrameGenerator::~FrameGenerator() = default;

int FrameGenerator::count() {
	return _framesCount;
}

double FrameGenerator::rate() {
	return _instance ? (_instance->frameRate() / _multiplier) : 0.;
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

	// Not just the format and the size: a storage the caller still shares
	// would be detached, and copied, by the first write into it.
	if (!GoodStorageForFrame(storage, size)) {
		storage = CreateFrameStorage(size);
	}

	// The frame is fitted inside whatever image it is rendered into, so only
	// a mode that asks for more than the box needs an intermediate to crop.
	const auto render = _size.scaled(size, mode);
	if (render.width() <= size.width() && render.height() <= size.height()) {
		_instance->renderToPrepared(storage, index * _multiplier);
	} else {
		if (!GoodStorageForFrame(_expanded, render)) {
			_expanded = CreateFrameStorage(render);
		}
		_instance->renderToPrepared(_expanded, index * _multiplier);

		// The crop covers the storage whole, so it replaces the pixels
		// rather than blending onto ones nothing has written yet.
		auto p = QPainter(&storage);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.drawImage(
			QPoint(
				(size.width() - render.width()) / 2,
				size.height() - render.height()),
			_expanded);
	}
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
