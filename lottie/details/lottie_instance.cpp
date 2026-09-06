// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_instance.h"

#include "base/assertion.h"

#include <tlottie.h>

#include <cmath>
#include <cstring>

namespace Lottie {
namespace {

[[nodiscard]] uint32 MapModifier(SkinModifier modifier) {
	switch (modifier) {
	case SkinModifier::None: return TLOTTIE_FITZ_NONE;
	case SkinModifier::Color1: return TLOTTIE_FITZ_TYPE_12;
	case SkinModifier::Color2: return TLOTTIE_FITZ_TYPE_3;
	case SkinModifier::Color3: return TLOTTIE_FITZ_TYPE_4;
	case SkinModifier::Color4: return TLOTTIE_FITZ_TYPE_5;
	case SkinModifier::Color5: return TLOTTIE_FITZ_TYPE_6;
	}
	Unexpected("Unexpected modifier in MapModifier.");
}

// Places a frame that was rendered on its own into the middle of a larger
// image. A frame owns every pixel of the image it is asked for, the margins
// the fit leaves over included, so each of them is written exactly once here:
// the rows above and below the frame in one span each, and the columns beside
// it as part of the row they belong to.
void PlaceCentered(QImage &to, const uint32 *from, QSize fit) {
	const auto perLine = int(to.bytesPerLine() / sizeof(uint32));
	const auto bits = reinterpret_cast<uint32*>(to.bits());
	const auto width = to.width();
	const auto height = to.height();
	const auto skipx = (width - fit.width()) / 2;
	const auto skipy = (height - fit.height()) / 2;
	const auto below = skipy + fit.height();
	const auto after = skipx + fit.width();
	if (skipy > 0) {
		memset(bits, 0, size_t(skipy) * perLine * sizeof(uint32));
	}
	if (below < height) {
		memset(
			bits + size_t(below) * perLine,
			0,
			size_t(height - below) * perLine * sizeof(uint32));
	}
	for (auto y = 0; y != fit.height(); ++y) {
		const auto line = bits + size_t(skipy + y) * perLine;
		if (skipx > 0) {
			memset(line, 0, size_t(skipx) * sizeof(uint32));
		}
		memcpy(
			line + skipx,
			from + size_t(y) * fit.width(),
			size_t(fit.width()) * sizeof(uint32));
		if (after < width) {
			memset(line + after, 0, size_t(width - after) * sizeof(uint32));
		}
	}
}

} // namespace

std::unique_ptr<Instance> Instance::Create(
		const std::string &json,
		const ColorReplacements *replacements) {
	auto colors = std::vector<TLottieColorReplacement>();
	if (replacements) {
		colors.reserve(replacements->replacements.size());
		for (const auto &[from, to] : replacements->replacements) {
			colors.push_back({ .source_color = from, .target_color = to });
		}
	}
	const auto instance = tlottie_new_with_options(
		reinterpret_cast<const uint8_t*>(json.data()),
		json.size(),
		(replacements
			? MapModifier(replacements->modifier)
			: uint32(TLOTTIE_FITZ_NONE)),
		nullptr,
		0,
		(colors.empty() ? nullptr : colors.data()),
		colors.size(),
		TLOTTIE_CHANNEL_BGRA);
	return instance
		? std::unique_ptr<Instance>(new Instance(instance))
		: nullptr;
}

Instance::Instance(not_null<TLottieInstance*> instance)
: _instance(instance)
, _size(int(tlottie_width(instance)), int(tlottie_height(instance)))
, _frameRate(double(tlottie_frame_rate(instance)))
, _framesCount(int(tlottie_frame_count(instance))) {
}

Instance::~Instance() {
	tlottie_drop(_instance);
}

void Instance::renderToPrepared(QImage &to, int index) {
	Expects(to.format() == kImageFormat);

	const auto width = to.width();
	const auto height = to.height();
	const auto original = _size;
	if (width <= 0 || height <= 0) {
		return;
	} else if (original.isEmpty() || to.bytesPerLine() % sizeof(uint32)) {
		to.fill(Qt::transparent);
		return;
	}

	// The renderer stretches the composition onto whatever it is given, so
	// the fit that rlottie applied on its own is computed here instead: the
	// larger uniform scale that still fits, aligned to the center.
	const auto scale = std::min(
		width / double(original.width()),
		height / double(original.height()));
	const auto fit = QSize(
		std::clamp(int(std::lround(original.width() * scale)), 1, width),
		std::clamp(int(std::lround(original.height() * scale)), 1, height));
	// A fit keeps one of the two dimensions whole, so whenever it is the
	// width the frame covers entire rows and can be rendered into them where
	// they already are, leaving only the rows above and below to clear.
	const auto perLine = int(to.bytesPerLine() / sizeof(uint32));
	const auto bits = reinterpret_cast<uint32*>(to.bits());
	if (perLine == width && fit.width() == width) {
		const auto skipy = (height - fit.height()) / 2;
		const auto below = skipy + fit.height();
		if (skipy > 0) {
			memset(bits, 0, size_t(skipy) * width * sizeof(uint32));
		}
		if (below < height) {
			memset(
				bits + size_t(below) * width,
				0,
				size_t(height - below) * width * sizeof(uint32));
		}
		const auto frame = bits + size_t(skipy) * width;
		const auto count = size_t(width) * fit.height();
		if (tlottie_render(
				_instance,
				float(index),
				uint32(width),
				uint32(fit.height()),
				frame,
				count,
				1) != TLOTTIE_OK) {
			memset(frame, 0, count * sizeof(uint32));
		}
		return;
	}
	_scratch.resize(size_t(fit.width()) * fit.height());
	if (tlottie_render(
			_instance,
			float(index),
			uint32(fit.width()),
			uint32(fit.height()),
			_scratch.data(),
			_scratch.size(),
			1) != TLOTTIE_OK) {
		to.fill(Qt::transparent);
		return;
	}
	PlaceCentered(to, _scratch.data(), fit);
}

} // namespace Lottie
