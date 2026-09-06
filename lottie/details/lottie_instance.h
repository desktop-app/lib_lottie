// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/lottie_common.h"

#include <QImage>
#include <QSize>
#include <memory>
#include <string>
#include <vector>

struct TLottieInstance;

namespace Lottie {

// One parsed animation, ready to render frames.
//
// The animation is parsed once, and the color replacements a caller asks for
// are applied to the model while it is parsed rather than to every frame, so
// two callers wanting two skin tones of the same sticker need two instances.
// An instance renders one frame at a time and is not shared between threads,
// though instances of different animations render independently.
class Instance final {
public:
	[[nodiscard]] static std::unique_ptr<Instance> Create(
		const std::string &json,
		const ColorReplacements *replacements);

	Instance(const Instance &other) = delete;
	Instance &operator=(const Instance &other) = delete;
	~Instance();

	[[nodiscard]] QSize size() const {
		return _size;
	}
	[[nodiscard]] double frameRate() const {
		return _frameRate;
	}
	[[nodiscard]] int framesCount() const {
		return _framesCount;
	}

	// Writes the frame into every pixel of the image, scaled to fit inside it
	// with the aspect ratio kept and centered on the leftover, which is left
	// transparent. Nothing shows through, so the caller does not clear it.
	void renderToPrepared(QImage &to, int index);

private:
	explicit Instance(not_null<TLottieInstance*> instance);

	not_null<TLottieInstance*> _instance;
	std::vector<uint32> _scratch;
	QSize _size;
	double _frameRate = 0.;
	int _framesCount = 0;

};

} // namespace Lottie
