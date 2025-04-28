// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/debug_log.h"

#include <rlottie.h>

#if __has_include(<glib.h>)
#include <glib.h>
#endif

namespace Lottie {

inline std::unique_ptr<rlottie::Animation> LoadAnimationFromData(
		std::string jsonData,
		const std::string &key,
		const std::string &resourcePath = "",
		bool cachePolicy = true,
		const std::vector<std::pair<std::uint32_t, std::uint32_t>> &colorReplacements = {},
		rlottie::FitzModifier fitzModifier = rlottie::FitzModifier::None) {
#ifdef LOTTIE_DISABLE_RECOLORING
	[[maybe_unused]] static auto logged = [&] {
		const auto text = "Lottie recoloring is disabled by the distributor, "
			"expect animations with color issues.";
		LOG((text));
#if __has_include(<glib.h>)
		g_warning(text);
#endif // __has_include(<glib.h>)
		return true;
	}();
#endif // LOTTIE_DISABLE_RECOLORING
	return rlottie::Animation::loadFromData(
		std::move(jsonData),
		key,
		resourcePath,
		cachePolicy
#ifndef LOTTIE_DISABLE_RECOLORING
		,std::move(colorReplacements),
		fitzModifier
#endif // !LOTTIE_DISABLE_RECOLORING
	);
}

} // namespace Lottie
