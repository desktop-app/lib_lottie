// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ui/style/style_core_types.h"
#include "ui/effects/animations.h"
#include "base/weak_ptr.h"

#include <crl/crl_time.h>
#include <QtCore/QByteArray>
#include <optional>

namespace Lottie {

struct IconDescriptor {
	QString path;
	QByteArray json;
	const style::color &color;
	QSize sizeOverride;
	int frame = 0;
};

class Icon final : public base::has_weak_ptr {
public:
	explicit Icon(IconDescriptor &&descriptor);
	Icon(const Icon &other) = delete;
	Icon &operator=(const Icon &other) = delete;

	[[nodiscard]] bool valid() const;
	[[nodiscard]] int frameIndex() const;
	[[nodiscard]] QImage frame() const;
	[[nodiscard]] int width() const;
	[[nodiscard]] int height() const;
	[[nodiscard]] QSize size() const;

	void paint(
		QPainter &p,
		int x,
		int y,
		std::optional<QColor> colorOverride = std::nullopt);
	void animate(
		Fn<void()> update,
		int frameFrom,
		int frameTo,
		std::optional<crl::time> duration = std::nullopt);
	void jumpTo(int frame, Fn<void()> update);
	[[nodiscard]] bool animating() const;

private:
	struct Frame;
	class Inner;
	friend class Inner;

	void wait() const;
	[[nodiscard]] int wantedFrameIndex() const;
	void preloadNextFrame() const;
	void frameJumpFinished();

	std::shared_ptr<Inner> _inner;
	const style::color &_color;
	Ui::Animations::Simple _animation;
	int _animationFrameTo = 0;
	Fn<void()> _repaint;

};

} // namespace Lottie
