// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/lottie_common.h"

namespace Lottie {

class FrameProvider {
public:
	virtual QImage construct(const FrameRequest &request) = 0;
	[[nodiscard]] virtual const Information &information() = 0;
	[[nodiscard]] virtual bool valid() = 0;

	[[nodiscard]] virtual int sizeRounding() = 0;

	virtual void render(
		QImage &to,
		const FrameRequest &request,
		int index) = 0;

};

} // namespace Lottie
