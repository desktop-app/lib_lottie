// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"
#include "base/variant.h"

#include <QSize>
#include <QColor>
#include <crl/crl_time.h>
#include <vector>

namespace Lottie {

inline constexpr auto kTimeUnknown = std::numeric_limits<crl::time>::min();
inline constexpr auto kMaxFileSize = 2 * 1024 * 1024;

class Animation;

struct Information {
	int frameRate = 0;
	int framesCount = 0;
	QSize size;
};

enum class Error {
	ParseFailed,
	NotSupported,
};

struct FrameRequest {
	QSize box;
	std::optional<QColor> colored;

	[[nodiscard]] bool empty() const {
		return box.isEmpty();
	}
	[[nodiscard]] QSize size(const QSize &original, bool useCache) const;

	[[nodiscard]] bool operator==(const FrameRequest &other) const {
		return (box == other.box)
			&& (colored == other.colored);
	}
	[[nodiscard]] bool operator!=(const FrameRequest &other) const {
		return !(*this == other);
	}
};

enum class Quality : char {
	Default,
	High,
	Synchronous
};

struct ColorReplacements {
	std::vector<std::pair<std::uint32_t, std::uint32_t>> replacements;
	uint8 tag = 0;
};

QByteArray ReadContent(const QByteArray &data, const QString &filepath);

} // namespace Lottie
