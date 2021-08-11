// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/lottie_common.h"

#include "base/algorithm.h"
#include "zlib.h"

#include <QFile>
#include <QTextCodec>

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

[[nodiscard]] QByteArray UnpackGzip(const QByteArray &bytes) {
	z_stream stream;
	stream.zalloc = nullptr;
	stream.zfree = nullptr;
	stream.opaque = nullptr;
	stream.avail_in = 0;
	stream.next_in = nullptr;
	int res = inflateInit2(&stream, 16 + MAX_WBITS);
	if (res != Z_OK) {
		return bytes;
	}
	const auto guard = gsl::finally([&] { inflateEnd(&stream); });

	auto result = QByteArray(kMaxFileSize + 1, char(0));
	stream.avail_in = bytes.size();
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.data()));
	stream.avail_out = 0;
	while (!stream.avail_out) {
		stream.avail_out = result.size();
		stream.next_out = reinterpret_cast<Bytef*>(result.data());
		int res = inflate(&stream, Z_NO_FLUSH);
		if (res != Z_OK && res != Z_STREAM_END) {
			return bytes;
		} else if (!stream.avail_out) {
			return bytes;
		}
	}
	result.resize(result.size() - stream.avail_out);
	return result;
}

std::string ReadUtf8(const QByteArray &data) {
	//00 00 FE FF  UTF-32BE
	//FF FE 00 00  UTF-32LE
	//FE FF        UTF-16BE
	//FF FE        UTF-16LE
	//EF BB BF     UTF-8
	if (data.size() < 4) {
		return data.toStdString();
	}
	struct Info {
		int skip = 0;
		const char *codec = nullptr;
	};
	const auto info = [&]() -> Info {
		const auto bom = uint32(uint8(data[0]))
			| (uint32(uint8(data[1])) << 8)
			| (uint32(uint8(data[2])) << 16)
			| (uint32(uint8(data[3])) << 24);
		if (bom == 0xFFFE0000U) {
			return { 4, "UTF-32BE" };
		} else if (bom == 0x0000FEFFU) {
			return { 4, "UTF-32LE" };
		} else if ((bom & 0xFFFFU) == 0xFFFEU) {
			return { 2, "UTF-16BE" };
		} else if ((bom & 0xFFFFU) == 0xFEFFU) {
			return { 2, "UTF-16LE" };
		} else if ((bom & 0xFFFFFFU) == 0xBFBBEFU) {
			return { 3 };
		}
		return {};
	}();
	const auto bytes = data.data() + info.skip;
	const auto length = data.size() - info.skip;
	// Old RapidJSON didn't convert encoding, just skipped BOM.
	// We emulate old behavior here, so don't convert as well.
	if (!info.codec || true) {
		return std::string(bytes, length);
	}
	const auto codec = QTextCodec::codecForName(info.codec);
	return codec->toUnicode(bytes, length).toUtf8().toStdString();
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
