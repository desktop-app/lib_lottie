// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "lottie/details/lottie_frame_provider.h"
#include "lottie/lottie_common.h"

namespace rlottie {
class Animation;
} // namespace rlottie

namespace Lottie {

class FrameProviderDirect final : public FrameProvider {
public:
	explicit FrameProviderDirect(Quality quality);
	~FrameProviderDirect();

	bool load(
		const QByteArray &content,
		const ColorReplacements *replacements);
	[[nodiscard]] bool loaded() const;
	void unload();

	bool setInformation(Information information);

	QImage construct(const FrameRequest &request) override;
	const Information &information() override;
	bool valid() override;

	int sizeRounding() override;

	void render(
		QImage &to,
		const FrameRequest &request,
		int index) override;
	void renderToPrepared(QImage &to, int index);

private:
	std::unique_ptr<rlottie::Animation> _animation;
	Information _information;
	Quality _quality = Quality::Default;

};

} // namespace Lottie
