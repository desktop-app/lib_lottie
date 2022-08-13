// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ui/effects/frame_generator.h"

#include <QtGui/QImage>
#include <memory>

namespace rlottie {
class Animation;
} // namespace rlottie

namespace Lottie {

class EmojiGenerator final : public Ui::FrameGenerator {
public:
	explicit EmojiGenerator(const QByteArray &bytes);
	~EmojiGenerator();

	int count() override;
	Frame renderNext(
		QImage storage,
		QSize size,
		Qt::AspectRatioMode mode = Qt::IgnoreAspectRatio) override;

private:
	std::unique_ptr<rlottie::Animation> _rlottie;
	QSize _size;
	int _multiplier = 1;
	int _frameDuration = 0;
	int _framesCount = 0;
	int _frameIndex = 0;

};

} // namespace Lottie
