// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_common.h"

#include "base/algorithm.h"

#include <QFile>

namespace Lottie {
namespace {

QByteArray ReadFile(const QString &filepath) {
	auto f = QFile(filepath);
	return (f.size() <= kMaxFileSize && f.open(QIODevice::ReadOnly))
		? f.readAll()
		: QByteArray();
}

} // namespace

QSize FrameRequest::size(const QSize &original, bool useCache) const {
	Expects(!empty());

	const auto divider = useCache ? 8 : 2;
	const auto result = original.scaled(box, Qt::KeepAspectRatio);
	const auto skipw = result.width() % divider;
	const auto skiph = result.height() % divider;
	return QSize(
		std::max(result.width() - skipw, divider),
		std::max(result.height() - skiph, divider));
}

QByteArray ReadContent(const QByteArray &data, const QString &filepath) {
	return data.isEmpty() ? ReadFile(filepath) : base::duplicate(data);
}

bool GoodStorageForFrame(const QImage &storage, QSize size) {
	return !storage.isNull()
		&& (storage.format() == kImageFormat)
		&& (storage.size() == size)
		&& storage.isDetached();
}

QImage CreateFrameStorage(QSize size) {
	return QImage(size, kImageFormat);
}

} // namespace Lottie
