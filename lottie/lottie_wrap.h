// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

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
#if __has_include(<glib.h>) && defined LOTTIE_USE_PACKAGED_RLOTTIE
	[[maybe_unused]] static auto logged = [&] {
		g_warning(
			"rlottie is incompatible, expect animations with color issues.");
		return true;
	}();
#endif
	return rlottie::Animation::loadFromData(
		std::move(jsonData),
		key,
		resourcePath,
		cachePolicy
#ifndef LOTTIE_USE_PACKAGED_RLOTTIE
		,std::move(colorReplacements),
		fitzModifier
#endif // !LOTTIE_USE_PACKAGED_RLOTTIE
	);
}

} // namespace Lottie
