// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_cached_multi.h"

#include "base/assertion.h"

#include <range/v3/numeric/accumulate.hpp>

namespace Lottie {

FrameProviderCachedMulti::FrameProviderCachedMulti(
	const QByteArray &content,
	FnMut<void(int index, QByteArray &&cached)> put,
	std::vector<QByteArray> caches,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements)
: _direct(quality)
, _put(std::move(put))
, _content(content)
, _replacements(replacements) {
	Expects(!caches.empty());

	_caches.reserve(caches.size());
	const auto emplace = [&](const QByteArray &cached) {
		const auto index = int(_caches.size());
		_caches.emplace_back(cached, request, [=](QByteArray &&v) {
			// We capture reference to _put, so the provider is not movable.
			_put(index, std::move(v));
		});
	};
	const auto load = [&] {
		if (_direct.loaded() || _direct.load(content, replacements)) {
			return true;
		}
		_caches.clear();
		return false;
	};
	const auto fill = [&] {
		if (!load()) {
			return false;
		}
		while (_caches.size() < caches.size()) {
			emplace({});
		}
		return true;
	};
	for (const auto &cached : caches) {
		emplace(cached);
		auto &cache = _caches.back();
		const auto &first = _caches.front();
		Assert(cache.sizeRounding() == first.sizeRounding());

		if (!cache.framesCount()) {
			if (!fill()) {
				return;
			}
			break;
		} else if (cache.framesReady() < cache.framesCount() && !load()) {
			return;
		} else if (cache.frameRate() != first.frameRate()
			|| cache.originalSize() != first.originalSize()) {
			_caches.pop_back();
			if (!fill()) {
				return;
			}
			break;
		}
	}
	if (!_direct.loaded()) {
		_direct.setInformation({
			.size = _caches.front().originalSize(),
			.frameRate = _caches.front().frameRate(),
			.framesCount = ranges::accumulate(
				_caches,
				0,
				std::plus<>(),
				&Cache::framesCount),
		});
	}
	if (!validateFramesPerCache() && _framesPerCache > 0) {
		fill();
	}
}

bool FrameProviderCachedMulti::validateFramesPerCache() {
	const auto &info = information();
	const auto count = int(_caches.size());
	_framesPerCache = (info.framesCount + count - 1) / count;
	if (!_framesPerCache
		|| (info.framesCount <= (count - 1) * _framesPerCache)) {
		_framesPerCache = 0;
		return false;
	}
	for (auto i = 0; i != count; ++i) {
		const auto cacheFramesCount = _caches[i].framesCount();
		if (!cacheFramesCount) {
			break;
		}
		const auto shouldBe = (i + 1 == count
			? (info.framesCount - (count - 1) * _framesPerCache)
			: _framesPerCache);
		if (cacheFramesCount != shouldBe) {
			_caches.erase(begin(_caches) + i, end(_caches));
			return false;
		}
	}
	return true;
}

QImage FrameProviderCachedMulti::construct(
		const FrameRequest &request) {
	if (!_framesPerCache) {
		return QImage();
	}
	auto cover = QImage();
	const auto &info = information();
	const auto count = int(_caches.size());
	for (auto i = 0; i != count; ++i) {
		auto cacheCover = _caches[i].takeFirstFrame();
		if (cacheCover.isNull()) {
			_caches[i].init(
				info.size,
				info.frameRate,
				(i + 1 == count
					? (info.framesCount - (count - 1) * _framesPerCache)
					: _framesPerCache),
				request);
		} else if (!i) {
			cover = std::move(cacheCover);
		}
	}
	if (!cover.isNull()) {
		return cover;
	}
	render(cover, request, 0);
	return cover;
}

const Information &FrameProviderCachedMulti::information() {
	return _direct.information();
}

bool FrameProviderCachedMulti::valid() {
	return _direct.valid() && (_framesPerCache > 0);
}

int FrameProviderCachedMulti::sizeRounding() {
	return _caches.front().sizeRounding();
}

void FrameProviderCachedMulti::render(
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
	const auto cacheIndex = index / _framesPerCache;
	const auto indexInCache = index % _framesPerCache;
	Assert(cacheIndex < _caches.size());
	auto &cache = _caches[cacheIndex];
	if (cache.renderFrame(to, request, indexInCache)) {
		return;
	} else if (!_direct.loaded()
		&& !_direct.load(_content, _replacements)) {
		_direct.setInformation({});
		return;
	}
	_direct.renderToPrepared(to, index);
	cache.appendFrame(to, request, indexInCache);
	if (cache.framesReady() == cache.framesCount()
		&& cacheIndex + 1 == _caches.size()) {
		_direct.unload();
	}
}

} // namespace Lottie
