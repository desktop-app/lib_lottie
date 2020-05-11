// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/lottie_common.h"

#ifdef LOTTIE_USE_SKOTTIE

#include "include/core/SkRefCnt.h"
#include "include/core/SkCanvas.h"

namespace skottie {
class Animation;
} // namespace skottie

#else // LOTTIE_USE_SKOTTIE

namespace rlottie {
class Animation;
} // namespace rlottie

#endif // LOTTIE_USE_SKOTTIE

namespace Lottie {
namespace details {

#ifdef LOTTIE_USE_SKOTTIE

struct BackendClass {
	explicit BackendClass(sk_sp<skottie::Animation> data);
	~BackendClass();

	sk_sp<skottie::Animation> data;
	std::optional<SkCanvas> canvas;
	void *bytes = nullptr;
	int width = 0;
	int height = 0;
	int bytesPerLine = 0;
};

#else // LOTTIE_USE_SKOTTIE

using BackendClass = rlottie::Animation;

#endif // LOTTIE_USE_SKOTTIE

[[nodiscard]] std::unique_ptr<BackendClass> CreateFromContent(
	const QByteArray &content,
	const ColorReplacements *replacements);

[[nodiscard]] QSize GetLottieFrameSize(not_null<BackendClass*> animation);
[[nodiscard]] int GetLottieFrameRate(not_null<BackendClass*> animation);
[[nodiscard]] int GetLottieFramesCount(not_null<BackendClass*> animation);

void RenderLottieFrame(
	not_null<BackendClass*> animation,
	int index,
	void *bytes,
	int width,
	int height,
	int bytesPerLine);

} // namespace details
} // namespace Lottie
