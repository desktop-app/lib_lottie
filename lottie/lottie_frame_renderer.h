// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"
#include "base/weak_ptr.h"
#include "lottie/lottie_common.h"

#include <QImage>
#include <QSize>
#include <crl/crl_time.h>
#include <crl/crl_object_on_queue.h>
#include <limits>

namespace rlottie {
class Animation;
} // namespace rlottie

namespace Lottie {

// Frame rate can be 1, 2, ... , 29, 30 or 60.
inline constexpr auto kNormalFrameRate = 30;
inline constexpr auto kMaxFrameRate = 60;
inline constexpr auto kMaxSize = 4096;
inline constexpr auto kMaxFramesCount = 210;
inline constexpr auto kFrameDisplayTimeAlreadyDone
	= std::numeric_limits<crl::time>::max();
inline constexpr auto kDisplayedInitial = crl::time(-1);

class Player;
class Cache;

struct Frame {
	QImage original;
	crl::time displayed = kDisplayedInitial;
	crl::time display = kTimeUnknown;
	int index = 0;
	bool useCache = false;

	FrameRequest request;
	QImage prepared;
};

QImage PrepareFrameByRequest(
	not_null<Frame*> frame,
	bool useExistingPrepared);

class SharedState {
public:
	SharedState(
		std::unique_ptr<rlottie::Animation> animation,
		const FrameRequest &request,
		Quality quality);

#ifdef LOTTIE_USE_CACHE
	SharedState(
		const QByteArray &content,
		const ColorReplacements *replacements,
		std::unique_ptr<rlottie::Animation> animation,
		std::unique_ptr<Cache> cache,
		const FrameRequest &request,
		Quality quality);
#endif // LOTTIE_USE_CACHE

	void start(
		not_null<Player*> owner,
		crl::time now,
		crl::time delay = 0,
		int skippedFrames = 0);

	[[nodiscard]] Information information() const;
	[[nodiscard]] bool initialized() const;

	[[nodiscard]] not_null<Frame*> frameForPaint();
	[[nodiscard]] int framesCount() const;
	[[nodiscard]] crl::time nextFrameDisplayTime() const;
	void addTimelineDelay(crl::time delayed, int skippedFrames = 0);
	void markFrameDisplayed(crl::time now);
	bool markFrameShown();

	void renderFrame(QImage &image, const FrameRequest &request, int index);

	struct RenderResult {
		bool rendered = false;
		base::weak_ptr<Player> notify;
	};
	[[nodiscard]] RenderResult renderNextFrame(const FrameRequest &request);

	~SharedState();

private:
	static Information CalculateInformation(
		Quality quality,
		rlottie::Animation *animation,
		Cache *cache);

	void construct(const FrameRequest &request);
	bool isValid() const;
	void init(QImage cover, const FrameRequest &request);
	void renderNextFrame(
		not_null<Frame*> frame,
		const FrameRequest &request);
	bool renderFromCache(QImage &to, const FrameRequest &request, int index);
	[[nodiscard]] bool useCache() const;
	[[nodiscard]] crl::time countFrameDisplayTime(int index) const;
	[[nodiscard]] not_null<Frame*> getFrame(int index);
	[[nodiscard]] not_null<const Frame*> getFrame(int index) const;
	[[nodiscard]] int counter() const;

	// crl::queue changes 0,2,4,6 to 1,3,5,7.
	// main thread changes 1,3,5,7 to 2,4,6,0.
	static constexpr auto kCounterUninitialized = -1;
	std::atomic<int> _counter = kCounterUninitialized;

	static constexpr auto kFramesCount = 4;
	std::array<Frame, kFramesCount> _frames;

	base::weak_ptr<Player> _owner;
	crl::time _started = kTimeUnknown;

	// (_counter % 2) == 1 main thread can write _delay.
	// (_counter % 2) == 0 crl::queue can read _delay.
	crl::time _delay = kTimeUnknown;

	int _frameIndex = 0;
	int _skippedFrames = 0;
	const Information _info;
	const Quality _quality = Quality::Default;

#ifdef LOTTIE_USE_CACHE
	const std::unique_ptr<Cache> _cache;
#endif // LOTTIE_USE_CACHE

	std::unique_ptr<rlottie::Animation> _animation;
	const QByteArray _content;
	const ColorReplacements *_replacements = nullptr;

};

class FrameRendererObject;

class FrameRenderer final {
public:
	static std::shared_ptr<FrameRenderer> CreateIndependent();
	static std::shared_ptr<FrameRenderer> Instance();

	void append(
		std::unique_ptr<SharedState> entry,
		const FrameRequest &request);

	void updateFrameRequest(
		not_null<SharedState*> entry,
		const FrameRequest &request);
	void frameShown();
	void remove(not_null<SharedState*> state);

private:
	using Implementation = FrameRendererObject;
	crl::object_on_queue<Implementation> _wrapped;

};

} // namespace Lottie
