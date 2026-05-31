#include "BlobCodec.hpp"

#include <Geode/loader/Log.hpp>
#include <zlib.h>

#include <cstdint>
#include <cstring>

namespace git_editor {

bool isBlobFootprintTooLarge(std::uint64_t rawSize) {
    return rawSize > kMaxBlobFootprintBytes;
}

std::string blobFootprintLimitMessage(std::uint64_t rawSize) {
    return "delta payload size " + std::to_string(rawSize)
        + " exceeds blob footprint cap " + std::to_string(kMaxBlobFootprintBytes);
}

std::optional<std::string> compressBlob(std::string const& raw) {
    if (isBlobFootprintTooLarge(raw.size())) {
        geode::log::error("compressBlob: {}", blobFootprintLimitMessage(raw.size()));
        return std::nullopt;
    }
    uLongf dstLen = compressBound(static_cast<uLong>(raw.size()));
    std::string out(4 + dstLen, '\0');
    std::uint32_t n = static_cast<std::uint32_t>(raw.size());
    std::memcpy(out.data(), &n, 4);
    int rc = compress2(
        reinterpret_cast<Bytef*>(out.data() + 4), &dstLen,
        reinterpret_cast<Bytef const*>(raw.data()), static_cast<uLong>(raw.size()),
        Z_BEST_COMPRESSION
    );
    if (rc != Z_OK) {
        geode::log::error("compressBlob: compress2 rc={}", rc);
        return std::nullopt;
    }
    out.resize(4 + dstLen);
    return out;
}

std::optional<std::string> decompressBlob(std::string const& bytes) {
    if (bytes.size() < 4) {
        geode::log::error("decompressBlob: payload too short (size={})", bytes.size());
        return std::nullopt;
    }
    std::uint32_t n = 0;
    std::memcpy(&n, bytes.data(), 4);
    if (n > kMaxBlobFootprintBytes) {
        geode::log::error("decompressBlob: uncompressed size {} exceeds cap {}", n, kMaxBlobFootprintBytes);
        return std::nullopt;
    }
    std::string out(n, '\0');
    uLongf dstLen = n;
    int rc = uncompress(
        reinterpret_cast<Bytef*>(out.data()), &dstLen,
        reinterpret_cast<Bytef const*>(bytes.data() + 4),
        static_cast<uLong>(bytes.size() - 4)
    );
    if (rc != Z_OK) {
        geode::log::error("decompressBlob: uncompress rc={}", rc);
        return std::nullopt;
    }
    if (dstLen != n) {
        geode::log::error("decompressBlob: size mismatch {} != {}", dstLen, n);
        return std::nullopt;
    }
    out.resize(dstLen);
    return out;
}

} // namespace git_editor
