// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "lottie/details/lottie_cache.h"

#include "lottie/details/lottie_frame_renderer.h"
#include "ffmpeg/ffmpeg_utility.h"
#include "base/bytes.h"
#include "base/assertion.h"

#include <QDataStream>
#include <range/v3/numeric/accumulate.hpp>

namespace Lottie {
namespace {

// Must not exceed max database allowed entry size.
constexpr auto kMaxCacheSize = 10 * 1024 * 1024;

} // namespace

Cache::Cache(
	const QByteArray &data,
	const FrameRequest &request,
	FnMut<void(QByteArray &&cached)> put)
: _data(data)
, _put(std::move(put)) {
	if (!readHeader(request)) {
		_framesReady = 0;
		_data = QByteArray();
	}
}

Cache::Cache(Cache &&) = default;

Cache &Cache::operator=(Cache&&) = default;

Cache::~Cache() {
	finalizeEncoding();
}

void Cache::init(
		QSize original,
		int frameRate,
		int framesCount,
		const FrameRequest &request) {
	_size = request.size(original, sizeRounding());
	_original = original;
	_frameRate = frameRate;
	_framesCount = framesCount;
	_framesReady = 0;
	prepareBuffers();
}

int Cache::sizeRounding() const {
	return 8;
}

int Cache::frameRate() const {
	return _frameRate;
}

int Cache::framesReady() const {
	return _framesReady;
}

int Cache::framesCount() const {
	return _framesCount;
}

QSize Cache::originalSize() const {
	return _original;
}

bool Cache::readHeader(const FrameRequest &request) {
	if (_data.isEmpty()) {
		return false;

	}
	QDataStream stream(&_data, QIODevice::ReadOnly);

	auto encoder = qint32(0);
	stream >> encoder;
	if (static_cast<Encoder>(encoder) != Encoder::YUV420A4_LZ4) {
		return false;
	}
	auto size = QSize();
	auto original = QSize();
	auto frameRate = qint32(0);
	auto framesCount = qint32(0);
	auto framesReady = qint32(0);
	stream
		>> size
		>> original
		>> frameRate
		>> framesCount
		>> framesReady;
	if (stream.status() != QDataStream::Ok
		|| original.isEmpty()
		|| (original.width() > kMaxSize)
		|| (original.height() > kMaxSize)
		|| (frameRate <= 0)
		|| (frameRate > kNormalFrameRate && frameRate != kMaxFrameRate)
		|| (framesCount <= 0)
		|| (framesCount > kMaxFramesCount)
		|| (framesReady <= 0)
		|| (framesReady > framesCount)
		|| request.size(original, sizeRounding()) != size) {
		return false;
	}
	_encoder = static_cast<Encoder>(encoder);
	_size = size;
	_original = original;
	_frameRate = frameRate;
	_framesCount = framesCount;
	_framesReady = framesReady;
	prepareBuffers();
	return renderFrame(_firstFrame, request, 0);
}

QImage Cache::takeFirstFrame() {
	return std::move(_firstFrame);
}

bool Cache::renderFrame(
		QImage &to,
		const FrameRequest &request,
		int index) {
	Expects(index >= _framesReady
		|| index == _offsetFrameIndex
		|| index == 0);

	if (index >= _framesReady) {
		return false;
	} else if (request.size(_original, sizeRounding()) != _size) {
		return false;
	} else if (index == 0) {
		_offset = headerSize();
		_offsetFrameIndex = 0;
	}
	const auto [ok, xored] = readCompressedFrame();
	if (!ok || (xored && index == 0)) {
		_framesReady = 0;
		_data = QByteArray();
		return false;
	} else if (index + 1 == _framesReady && _data.size() > _offset) {
		_data.resize(_offset);
	}
	if (xored) {
		Xor(_previous, _uncompressed);
	} else {
		std::swap(_uncompressed, _previous);
	}
	Decode(to, _previous, _size, _decodeContext);
	return true;
}

void Cache::appendFrame(
		const QImage &frame,
		const FrameRequest &request,
		int index) {
	if (request.size(_original, sizeRounding()) != _size) {
		_framesReady = 0;
		_data = QByteArray();
	}
	if (index != _framesReady) {
		return;
	}
	if (index == 0) {
		_size = request.size(_original, sizeRounding());
		_encode = EncodeFields();
		_encode.compressedFrames.reserve(_framesCount);
		prepareBuffers();
	}
	Assert(frame.size() == _size);
	Encode(_uncompressed, frame, _encode.cache, _encode.context);
	CompressAndSwapFrame(
		_encode.compressBuffer,
		(index != 0) ? &_encode.xorCompressBuffer : nullptr,
		_uncompressed,
		_previous);
	const auto compressed = _encode.compressBuffer;
	const auto nowSize = (_data.isEmpty() ? headerSize() : _data.size())
		+ _encode.totalSize;
	const auto totalSize = nowSize + compressed.size();
	if (nowSize <= kMaxCacheSize && totalSize > kMaxCacheSize) {
		// Write to cache while we still can.
		finalizeEncoding();
	}
	_encode.totalSize += compressed.size();
	_encode.compressedFrames.push_back(compressed);
	_encode.compressedFrames.back().detach();
	if (++_framesReady == _framesCount) {
		finalizeEncoding();
	}
}

void Cache::finalizeEncoding() {
	if (_encode.compressedFrames.empty() || !_put) {
		return;
	}
	const auto size = (_data.isEmpty() ? headerSize() : _data.size())
		+ _encode.totalSize;
	if (_data.isEmpty()) {
		_data.reserve(size);
		writeHeader();
	} else {
		updateFramesReadyCount();
	}
	const auto offset = _data.size();
	_data.resize(size);
	auto to = _data.data() + offset;
	for (const auto &block : _encode.compressedFrames) {
		const auto amount = qint32(block.size());
		memcpy(to, block.data(), amount);
		to += amount;
	}
	if (_data.size() <= kMaxCacheSize) {
		_put(QByteArray(_data));
	}
	_encode = EncodeFields();
}

int Cache::headerSize() const {
	return 8 * sizeof(qint32);
}

void Cache::writeHeader() {
	Expects(_data.isEmpty());

	QDataStream stream(&_data, QIODevice::WriteOnly);

	stream
		<< static_cast<qint32>(_encoder)
		<< _size
		<< _original
		<< qint32(_frameRate)
		<< qint32(_framesCount)
		<< qint32(_framesReady);
}

void Cache::updateFramesReadyCount() {
	Expects(_data.size() >= headerSize());

	QDataStream stream(&_data, QIODevice::ReadWrite);
	stream.device()->seek(headerSize() - sizeof(qint32));
	stream << qint32(_framesReady);
}

void Cache::prepareBuffers() {
	// 12 bit per pixel in YUV420P.
	const auto bytesPerLine = _size.width();

	_uncompressed.allocate(bytesPerLine, _size.height());
	_previous.allocate(bytesPerLine, _size.height());
}

Cache::ReadResult Cache::readCompressedFrame() {
	if (_data.size() < _offset) {
		return { false };
	}
	auto length = qint32(0);
	const auto part = bytes::make_span(_data).subspan(_offset);
	if (part.size() < sizeof(length)) {
		return { false };
	}
	bytes::copy(
		bytes::object_as_span(&length),
		part.subspan(0, sizeof(length)));
	const auto bytes = part.subspan(sizeof(length));

	const auto xored = (length < 0);
	if (xored) {
		length = -length;
	}
	_offset += sizeof(length) + length;
	++_offsetFrameIndex;
	const auto ok = (length <= bytes.size())
		? UncompressToRaw(_uncompressed, bytes.subspan(0, length))
		: false;
	return { ok, xored };
}

} // namespace Lottie
