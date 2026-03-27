// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_toast_icon.h"

#include "lottie/lottie_icon.h"
#include "ui/painter.h"
#include "ui/toast/toast.h"
#include "styles/style_widgets.h"

#include <crl/crl_async.h>
#include <crl/crl_on_main.h>
#include <memory>

namespace Lottie {
namespace {

using namespace Ui;
using namespace Ui::Toast;

[[nodiscard]] object_ptr<RpWidget> MakeLottieToastIcon(
		not_null<RpWidget*> parent,
		const Config &config) {
	if (config.iconLottie.isEmpty()) {
		return { nullptr };
	}

	auto descriptor = Lottie::IconDescriptor{
		.name = config.iconLottie,
		.sizeOverride = config.iconLottieSize.value_or(
			st::defaultToastLottieSize),
	};
	descriptor.limitFps = true;

	const auto iconSize = descriptor.sizeOverride;
	auto result = object_ptr<RpWidget>(parent);
	const auto raw = result.data();
	raw->setAttribute(Qt::WA_TransparentForMouseEvents);
	raw->resize(iconSize);

	const auto owned = std::shared_ptr<Lottie::Icon>(
		Lottie::MakeIcon(std::move(descriptor)).release());
	const auto icon = owned.get();
	if (!icon) {
		return { nullptr };
	}

	const auto weak = QPointer<RpWidget>(raw);
	raw->lifetime().add([kept = owned] {});
	const auto ready = raw->lifetime().make_state<bool>(false);
	const auto looped = raw->lifetime().make_state<bool>(
		config.iconLottieRepeat == anim::repeat::loop);
	const auto start = [=] {
		icon->animate([=] {
			raw->update();
		}, 0, icon->framesCount() - 1);
	};
	raw->paintRequest() | rpl::on_next([=] {
		if (!*ready) {
			return;
		}
		auto p = QPainter(raw);
		icon->paint(p, 0, 0);
		if (!icon->animating() && icon->frameIndex() > 0 && *looped) {
			start();
		}
	}, raw->lifetime());
	crl::async([=, kept = owned] {
		const auto valid = kept->valid();
		crl::on_main(weak, [=] {
			*ready = valid;
			if (!valid) {
				return;
			}
			raw->update();
			if (icon->framesCount() > 1) {
				start();
			}
		});
	});

	return result;
}

const auto kToastLottieIconFactoryRegistered = [] {
	AddIconFactory(MakeLottieToastIcon);
	return true;
}();

} // namespace

void EnsureToastIconFactory() {
	[[maybe_unused]] volatile auto touch
		= &kToastLottieIconFactoryRegistered;
}

} // namespace Lottie
