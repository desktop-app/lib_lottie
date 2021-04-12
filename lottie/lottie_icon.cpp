// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_icon.h"

#include "lottie/lottie_common.h"
#include "ui/style/style_core.h"

#include <QtGui/QPainter>
#include <crl/crl_async.h>
#include <crl/crl_semaphore.h>
#include <crl/crl_on_main.h>
#include <rlottie.h>

namespace Lottie {
namespace {

[[nodiscard]] std::unique_ptr<rlottie::Animation> CreateFromContent(
		const QByteArray &content,
		QColor replacement) {
	auto string = ReadUtf8(content);
#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE
	auto list = std::vector<std::pair<std::uint32_t, std::uint32_t>>();
	if (replacement != Qt::white) {
		const auto value = (uint32_t(replacement.red()) << 16)
			| (uint32_t(replacement.green() << 8))
			| (uint32_t(replacement.blue()));
		list.push_back({ 0xFFFFFFU, value });
	}
	auto result = rlottie::Animation::loadFromData(
		std::move(string),
		std::string(),
		std::string(),
		false,
		std::move(list));
#else
	auto result = rlottie::Animation::loadFromData(
		std::move(string),
		std::string(),
		std::string(),
		false);
#endif
	return result;
}

[[nodiscard]] QColor RealRenderedColor(QColor color) {
#ifndef DESKTOP_APP_USE_PACKAGED_RLOTTIE
	return QColor(color.red(), color.green(), color.blue(), 255);
#else
	return Qt::white;
#endif
}

} // namespace

struct Icon::Frame {
	int index = 0;
	QImage renderedImage;
	QImage colorizedImage;
	QColor renderedColor;
	QColor colorizedColor;
};

class Icon::Inner final : public std::enable_shared_from_this<Inner> {
public:
	Inner(int frameIndex, base::weak_ptr<Icon> weak);

	void prepareFromAsync(
		const QString &path,
		const QByteArray &json,
		QSize sizeOverride,
		QColor color);
	void waitTillPrepared() const;

	[[nodiscard]] bool valid() const;
	[[nodiscard]] QSize size() const;
	[[nodiscard]] Frame &frame();
	[[nodiscard]] const Frame &frame() const;

	[[nodiscard]] crl::time animationDuration(
		int frameFrom,
		int frameTo) const;
	void moveToFrame(int frame, QColor color);

private:
	enum class PreloadState {
		None,
		Preloading,
		Ready,
	};
	std::unique_ptr<rlottie::Animation> _rlottie;
	Frame _current;
	Frame _preloaded;
	std::atomic<PreloadState> _preloadState = PreloadState::None;
	base::weak_ptr<Icon> _weak;
	mutable crl::semaphore _semaphore;
	mutable bool _ready = false;

};

Icon::Inner::Inner(int frameIndex, base::weak_ptr<Icon> weak)
: _current{ .index = frameIndex }
, _weak(weak) {
}

void Icon::Inner::prepareFromAsync(
		const QString &path,
		const QByteArray &json,
		QSize sizeOverride,
		QColor color) {
	const auto guard = gsl::finally([&] { _semaphore.release(); });
	if (!_weak) {
		return;
	}
	_rlottie = CreateFromContent(ReadContent(json, path), color);
	if (!_rlottie || !_weak) {
		return;
	}
	auto width = size_t();
	auto height = size_t();
	_rlottie->size(width, height);
	const auto size = sizeOverride.isEmpty()
		? style::ConvertScale(QSize{ int(width), int(height) })
		: sizeOverride;
	auto &image = _current.renderedImage;
	image = CreateFrameStorage(size * style::DevicePixelRatio());
	image.fill(Qt::transparent);
	auto surface = rlottie::Surface(
		reinterpret_cast<uint32_t*>(image.bits()),
		image.width(),
		image.height(),
		image.bytesPerLine());
	_rlottie->renderSync(_current.index, std::move(surface));
	_current.renderedColor = RealRenderedColor(color);
	_current.renderedImage = std::move(image);
}

void Icon::Inner::waitTillPrepared() const {
	if (!_ready) {
		_semaphore.acquire();
		_ready = true;
	}
}

bool Icon::Inner::valid() const {
	waitTillPrepared();
	return (_rlottie != nullptr);
}

QSize Icon::Inner::size() const {
	return valid()
		? (_current.renderedImage.size() / style::DevicePixelRatio())
		: QSize();
}

Icon::Frame &Icon::Inner::frame() {
	waitTillPrepared();
	return _current;
}

const Icon::Frame &Icon::Inner::frame() const {
	waitTillPrepared();
	return _current;
}

crl::time Icon::Inner::animationDuration(int frameFrom, int frameTo) const {
	waitTillPrepared();
	const auto rate = _rlottie ? _rlottie->frameRate() : 0.;
	const auto frames = std::abs(frameTo - frameFrom);
	return (rate >= 1.)
		? crl::time(std::round(frames / rate * 1000.))
		: 0;
}

void Icon::Inner::moveToFrame(int frame, QColor color) {
	waitTillPrepared();
	const auto state = _preloadState.load();
	const auto shown = _current.index;
	if (shown == frame || !_rlottie || state == PreloadState::Preloading) {
		return;
	} else if (state == PreloadState::Ready) {
		if (_preloaded.index == frame) {
			std::swap(_current, _preloaded);
			return;
		} else if ((shown < _preloaded.index && _preloaded.index < frame)
			|| (shown > _preloaded.index && _preloaded.index > frame)) {
			std::swap(_current, _preloaded);
		}
	}
	_preloaded.index = frame;
	_preloadState = PreloadState::Preloading;
	crl::async([
		this,
		guard = shared_from_this(),
		color = RealRenderedColor(color)
	] {
		if (!_weak) {
			return;
		}
		const auto size = _current.renderedImage.size();

		auto &image = _preloaded.renderedImage;
		if (!GoodStorageForFrame(image, size)) {
			image = CreateFrameStorage(size);
		}
		image.fill(Qt::transparent);
		auto surface = rlottie::Surface(
			reinterpret_cast<uint32_t*>(image.bits()),
			image.width(),
			image.height(),
			image.bytesPerLine());
		_rlottie->renderSync(_preloaded.index, std::move(surface));
		_preloaded.renderedColor = color;
		_preloaded.renderedImage = std::move(image);
		_preloadState = PreloadState::Ready;
		crl::on_main(_weak, [=] {
			_weak->frameJumpFinished();
		});
	});
}

Icon::Icon(IconDescriptor &&descriptor)
: _inner(std::make_shared<Inner>(descriptor.frame, base::make_weak(this)))
, _color(descriptor.color)
, _animationFrameTo(descriptor.frame) {
	crl::async([
		inner = _inner,
		path = descriptor.path,
		bytes = descriptor.json,
		sizeOverride = descriptor.sizeOverride,
		color = _color->c
	] {
		inner->prepareFromAsync(path, bytes, sizeOverride, color);
	});
}

void Icon::wait() const {
	_inner->waitTillPrepared();
}

bool Icon::valid() const {
	return _inner->valid();
}

int Icon::frameIndex() const {
	preloadNextFrame();
	return _inner->frame().index;
}

QImage Icon::frame() const {
	preloadNextFrame();
	auto &frame = _inner->frame();
	if (frame.renderedImage.isNull()) {
		return frame.renderedImage;
	}
	const auto color = _color->c;
	if (color == frame.renderedColor) {
		return frame.renderedImage;
	} else if (!frame.colorizedImage.isNull()
		&& color == frame.colorizedColor) {
		return frame.colorizedImage;
	}
	if (frame.colorizedImage.isNull()) {
		frame.colorizedImage = CreateFrameStorage(
			frame.renderedImage.size());
	}
	frame.colorizedColor = color;
	style::colorizeImage(frame.renderedImage, color, &frame.colorizedImage);
	return frame.colorizedImage;
}

int Icon::width() const {
	return size().width();
}

int Icon::height() const {
	return size().height();
}

QSize Icon::size() const {
	return _inner->size();
}

void Icon::paint(
		QPainter &p,
		int x,
		int y,
		std::optional<QColor> colorOverride) {
	preloadNextFrame();
	auto &frame = _inner->frame();
	const auto color = colorOverride.value_or(_color->c);
	if (frame.renderedImage.isNull() || color.alpha() == 0) {
		return;
	}
	const auto rect = QRect{ QPoint(x, y), size() };
	if (color == frame.renderedColor) {
		p.drawImage(rect, frame.renderedImage);
	} else if (color.alphaF() < 1.
		&& (QColor(color.red(), color.green(), color.blue())
			== frame.renderedColor)) {
		const auto o = p.opacity();
		p.setOpacity(o * color.alphaF());
		p.drawImage(rect, frame.renderedImage);
		p.setOpacity(o);
	} else if (!frame.colorizedImage.isNull()
		&& color == frame.colorizedColor) {
		p.drawImage(rect, frame.colorizedImage);
	} else if (!frame.colorizedImage.isNull()
		&& color.alphaF() < 1.
		&& (QColor(color.red(), color.green(), color.blue())
			== frame.colorizedColor)) {
		const auto o = p.opacity();
		p.setOpacity(o * color.alphaF());
		p.drawImage(rect, frame.colorizedImage);
		p.setOpacity(o);
	} else {
		if (frame.colorizedImage.isNull()) {
			frame.colorizedImage = CreateFrameStorage(
				frame.renderedImage.size());
		}
		frame.colorizedColor = color;
		style::colorizeImage(
			frame.renderedImage,
			color,
			&frame.colorizedImage);
		p.drawImage(rect, frame.colorizedImage);
	}
}

void Icon::animate(
		Fn<void()> update,
		int frameFrom,
		int frameTo,
		std::optional<crl::time> duration) {
	jumpTo(frameFrom, std::move(update));
	if (frameFrom != frameTo) {
		_animationFrameTo = frameTo;
		_animation.start(
			[=] { preloadNextFrame(); if (_repaint) _repaint(); },
			frameFrom,
			frameTo,
			(duration
				? *duration
				: _inner->animationDuration(frameFrom, frameTo)));
	}
}

void Icon::jumpTo(int frame, Fn<void()> update) {
	_animation.stop();
	_repaint = std::move(update);
	_animationFrameTo = frame;
	preloadNextFrame();
}

void Icon::frameJumpFinished() {
	if (_repaint && !animating()) {
		_repaint();
		_repaint = nullptr;
	}
}

int Icon::wantedFrameIndex() const {
	return int(std::round(_animation.value(_animationFrameTo)));
}

void Icon::preloadNextFrame() const {
	_inner->moveToFrame(wantedFrameIndex(), _color->c);
}

bool Icon::animating() const {
	return _animation.animating();
}

} // namespace Lottie
