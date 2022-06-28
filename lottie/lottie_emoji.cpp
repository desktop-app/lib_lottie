// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_emoji.h"

#include "lottie/lottie_common.h"
#include "ui/image/image_prepare.h"

#include <rlottie.h>

namespace Lottie {

EmojiGenerator::EmojiGenerator(const QByteArray &bytes)
: _rlottie(
	rlottie::Animation::loadFromData(
		ReadUtf8(Images::UnpackGzip(bytes)),
		std::string(),
		std::string(),
		false)) {
	if (_rlottie) {
		auto width = size_t();
		auto height = size_t();
		_rlottie->size(width, height);
		_size = QSize(width, height);
		_framesCount = _rlottie->totalFrame();
		const auto rate = _rlottie->frameRate();
		_frameDuration = (rate > 0) ? (1000 / rate) : 0;
	}
	if (!_framesCount || !_frameDuration || _size.isEmpty()) {
		_rlottie = nullptr;
		_framesCount = _frameDuration = 0;
		_size = QSize();
	}
}

EmojiGenerator::~EmojiGenerator() = default;

int EmojiGenerator::count() {
	return _framesCount;
}

EmojiGenerator::Frame EmojiGenerator::renderNext(
		QImage storage,
		QSize size,
		Qt::AspectRatioMode mode) {
	if (!_framesCount || _frameIndex == _framesCount) {
		return {};
	}
	const auto index = _frameIndex++;
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
	_rlottie->renderSync(index, std::move(surface));
	return {
		.image = std::move(storage),
		.duration = _frameDuration,
	};
}

} // namespace Lottie
