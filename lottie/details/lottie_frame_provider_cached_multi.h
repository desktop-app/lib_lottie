// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/details/lottie_frame_provider_direct.h"
#include "lottie/details/lottie_cache.h"

namespace Lottie {

class FrameProviderCachedMulti final : public FrameProvider {
public:
	FrameProviderCachedMulti(
		const QByteArray &content,
		FnMut<void(int index, QByteArray &&cached)> put,
		std::vector<QByteArray> caches,
		const FrameRequest &request,
		Quality quality,
		const ColorReplacements *replacements);

	FrameProviderCachedMulti(const FrameProviderCachedMulti &) = delete;
	FrameProviderCachedMulti &operator=(const FrameProviderCachedMulti &)
		= delete;

	QImage construct(const FrameRequest &request) override;
	const Information &information() override;
	bool valid() override;

	int sizeRounding() override;

	void render(
		QImage &to,
		const FrameRequest &request,
		int index) override;

private:
	bool validateFramesPerCache();

	std::vector<Cache> _caches;
	FrameProviderDirect _direct;
	FnMut<void(int index, QByteArray &&cached)> _put;
	int _framesPerCache = 0;
	const QByteArray _content;
	const ColorReplacements *_replacements = nullptr;

};

} // namespace Lottie
