// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_single_player.h"

#include "lottie/lottie_frame_renderer.h"

namespace Lottie {

SinglePlayer::SinglePlayer(
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements,
	std::shared_ptr<FrameRenderer> renderer)
: _timer([=] { checkNextFrameRender(); })
, _renderer(renderer ? renderer : FrameRenderer::Instance())
, _animation(this, content, request, quality, replacements) {
}

SinglePlayer::SinglePlayer(
	FnMut<void(FnMut<void(QByteArray &&cached)>)> get, // Main thread.
	FnMut<void(QByteArray &&cached)> put, // Unknown thread.
	const QByteArray &content,
	const FrameRequest &request,
	Quality quality,
	const ColorReplacements *replacements,
	std::shared_ptr<FrameRenderer> renderer)
#ifdef LOTTIE_USE_CACHE
: _timer([=] { checkNextFrameRender(); })
, _renderer(renderer ? renderer : FrameRenderer::Instance())
, _animation(
	this,
	std::move(get),
	std::move(put),
	content,
	request,
	quality,
	replacements) {
#else // LOTTIE_USE_CACHE
: SinglePlayer(
	content,
	request,
	quality,
	replacements,
	std::move(renderer)) {
#endif // LOTTIE_USE_CACHE
}

SinglePlayer::~SinglePlayer() {
	if (_state) {
		_renderer->remove(_state);
	}
}

void SinglePlayer::start(
		not_null<Animation*> animation,
		std::unique_ptr<SharedState> state) {
	Expects(animation == &_animation);

	_state = state.get();
	auto information = state->information();
	state->start(this, crl::now());
	const auto request = state->frameForPaint()->request;
	_renderer->append(std::move(state), request);
	_updates.fire({ std::move(information) });

	crl::on_main_update_requests(
	) | rpl::start_with_next([=] {
		checkStep();
	}, _lifetime);
}

void SinglePlayer::failed(not_null<Animation*> animation, Error error) {
	Expects(animation == &_animation);

	_updates.fire_error(std::move(error));
}

rpl::producer<Update, Error> SinglePlayer::updates() const {
	return _updates.events();
}

bool SinglePlayer::ready() const {
	return _animation.ready();
}

QImage SinglePlayer::frame() const {
	return _animation.frame();
}

QImage SinglePlayer::frame(const FrameRequest &request) const {
	return _animation.frame(request);
}

Animation::FrameInfo SinglePlayer::frameInfo(
		const FrameRequest &request) const {
	return _animation.frameInfo(request);
}

Information SinglePlayer::information() const {
	return _animation.information();
}

void SinglePlayer::checkStep() {
	if (_nextFrameTime == kFrameDisplayTimeAlreadyDone) {
		return;
	} else if (_nextFrameTime != kTimeUnknown) {
		checkNextFrameRender();
	} else {
		checkNextFrameAvailability();
	}
}

void SinglePlayer::checkNextFrameAvailability() {
	Expects(_state != nullptr);
	Expects(_nextFrameTime == kTimeUnknown);

	_nextFrameTime = _state->nextFrameDisplayTime();
	Assert(_nextFrameTime != kFrameDisplayTimeAlreadyDone);
	if (_nextFrameTime != kTimeUnknown) {
		checkNextFrameRender();
	}
}

void SinglePlayer::checkNextFrameRender() {
	Expects(_nextFrameTime != kTimeUnknown);

	const auto now = crl::now();
	if (now < _nextFrameTime) {
		if (!_timer.isActive()) {
			_timer.callOnce(_nextFrameTime - now);
		}
	} else {
		_timer.cancel();

		_state->markFrameDisplayed(now);
		_state->addTimelineDelay(now - _nextFrameTime);

		_nextFrameTime = kFrameDisplayTimeAlreadyDone;
		_updates.fire({ DisplayFrameRequest() });
	}
}

void SinglePlayer::updateFrameRequest(
		not_null<const Animation*> animation,
		const FrameRequest &request) {
	Expects(animation == &_animation);
	Expects(_state != nullptr);

	_renderer->updateFrameRequest(_state, request);
}

bool SinglePlayer::markFrameShown() {
	Expects(_state != nullptr);

	if (_nextFrameTime == kFrameDisplayTimeAlreadyDone) {
		_nextFrameTime = kTimeUnknown;
	}
	if (_state->markFrameShown()) {
		_renderer->frameShown();
		return true;
	}
	return false;
}

} // namespace Lottie
