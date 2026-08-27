// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_frame_provider_shared.h"

#include "base/assertion.h"

#include <crl/crl_on_main.h>

namespace Lottie {

FrameProviderShared::FrameProviderShared(
		FnMut<void(FnMut<void(std::unique_ptr<FrameProvider>)>)> factory) {
	_mutex.lockForWrite();
	factory(crl::guard(this, [=](std::unique_ptr<FrameProvider> shared) {
		_shared = std::move(shared);
		if (_shared) {
			// Copied once, under the write lock: information() must not hand
			// out a reference into _shared, which a failing render() on
			// another thread can destroy the moment the read lock is
			// released - before the caller has copied from it.
			_information = _shared->information();
			_sizeRounding = _shared->sizeRounding();
		}
		_mutex.unlock();
	}));
}

QImage FrameProviderShared::construct(
		std::unique_ptr<FrameProviderToken> &token,
		const FrameRequest &request) {
	QWriteLocker lock(&_mutex);
	if (!_shared) {
		// valid() was checked without holding the lock, so a failing render()
		// on another thread could have destroyed the provider since then.
		return QImage();
	}
	token = createToken();
	if (token) {
		token->exclusive = !_constructed;
	}
	auto result = _shared->construct(token, request);
	_constructed = true;
	return result;
}

const Information &FrameProviderShared::information() {
	QReadLocker lock(&_mutex);
	return _information;
}

bool FrameProviderShared::valid() {
	QReadLocker lock(&_mutex);
	return _shared && _shared->valid();
}

int FrameProviderShared::sizeRounding() {
	QReadLocker lock(&_mutex);

	// Constant for a provider and copied once, so it stays available after
	// a failing render() has destroyed the provider on another thread.
	return _sizeRounding;
}

std::unique_ptr<FrameProviderToken> FrameProviderShared::createToken() {
	return _shared ? _shared->createToken() : nullptr;
}

bool FrameProviderShared::render(
		const std::unique_ptr<FrameProviderToken> &token,
		QImage &to,
		const FrameRequest &request,
		int index) {
	QReadLocker readLock(&_mutex);
	if (!_shared) {
		if (token) {
			token->result = FrameRenderResult::Failed;
		}
		return false;
	}

	if (token) {
		token->exclusive = false;
		_shared->render(token, to, request, index);
		if (token->result == FrameRenderResult::Ok) {
			return true;
		}
	}
	readLock.unlock();

	QWriteLocker lock(&_mutex);
	if (!_shared) {
		if (token) {
			token->result = FrameRenderResult::Failed;
		}
		return false;
	}
	if (token) {
		_shared->render(token, to, request, index);
		if (token->result == FrameRenderResult::Ok) {
			return true;
		} else if (token->result == FrameRenderResult::Failed) {
			_shared = nullptr;
			return false;
		}
		token->exclusive = true;
	}
	return _shared->render(token, to, request, index);
}

} // namespace Lottie
