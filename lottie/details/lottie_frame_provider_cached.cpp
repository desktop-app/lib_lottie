// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_cached.h"

namespace Lottie {

FrameProviderCached::FrameProviderCached(
	const QByteArray &content,
	FnMut<void(QByteArray &&cached)> put,
	const QByteArray &cached,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
: _cache(cached, request, std::move(put))
, _direct(quality)
, _content(content)
, _replacements(replacements) {
	if (!_cache.framesCount()
		|| (_cache.framesReady() < _cache.framesCount())) {
		if (!_direct.load(content, replacements)) {
			return;
		}
	} else {
		_direct.setInformation({
			.size = _cache.originalSize(),
			.frameRate = _cache.frameRate(),
			.framesCount = _cache.framesCount(),
		});
	}
}

QImage FrameProviderCached::construct(
		const FrameRequest &request) {
	auto cover = _cache.takeFirstFrame();
	if (!cover.isNull()) {
		return cover;
	}
	const auto &info = information();
	_cache.init(
		info.size,
		info.frameRate,
		info.framesCount,
		request);
	render(cover, request, 0);
	return cover;
}

const Information &FrameProviderCached::information() {
	return _direct.information();
}

bool FrameProviderCached::valid() {
	return _direct.valid();
}

int FrameProviderCached::sizeRounding() {
	return _cache.sizeRounding();
}

void FrameProviderCached::render(
		QImage &to,
		const FrameRequest &request,
		int index) {
	if (!valid()) {
		return;
	}

	const auto original = information().size;
	const auto size = request.box.isEmpty()
		? original
		: request.size(original, sizeRounding());
	if (!GoodStorageForFrame(to, size)) {
		to = CreateFrameStorage(size);
	}
	if (_cache.renderFrame(to, request, index)) {
		return;
	} else if (!_direct.loaded()
		&& !_direct.load(_content, _replacements)) {
		_direct.setInformation({});
		return;
	}
	_direct.renderToPrepared(to, index);
	_cache.appendFrame(to, request, index);
	if (_cache.framesReady() == _cache.framesCount()) {
		_direct.unload();
	}
}

} // namespace Lottie
