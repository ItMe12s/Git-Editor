#include "DbZip.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>

#include <array>
#include <cstring>
#include <fstream>

namespace git_editor {

namespace {

constexpr std::string_view kSqliteMagic = "SQLite format 3\000";
constexpr std::size_t      kSqliteMagicLen = 16;
constexpr std::array<std::uint8_t, 4> kZipMagic = { 0x50, 0x4B, 0x03, 0x04 };

} // namespace

DbFileForm peekDbFileForm(std::filesystem::path const& path) {
    // Use path (native) to construct ifstream, not UTF-8 narrow, so Windows non-ASCII paths
    // open correctly. Only 16 B read, do not use readBinary (full file) for a magic sniff.
    std::ifstream f(path, std::ios::binary);
    if (!f) return DbFileForm::Unknown;

    std::array<char, 16> buf{};
    f.read(buf.data(), 16);
    auto const nread = static_cast<std::size_t>(f.gcount());

    if (nread >= 4 &&
        static_cast<std::uint8_t>(buf[0]) == kZipMagic[0] &&
        static_cast<std::uint8_t>(buf[1]) == kZipMagic[1] &&
        static_cast<std::uint8_t>(buf[2]) == kZipMagic[2] &&
        static_cast<std::uint8_t>(buf[3]) == kZipMagic[3]) {
        return DbFileForm::Zip;
    }

    if (nread >= kSqliteMagicLen &&
        std::memcmp(buf.data(), kSqliteMagic.data(), kSqliteMagicLen) == 0) {
        return DbFileForm::Sqlite;
    }

    return DbFileForm::Unknown;
}

bool writeZipAtomic(std::filesystem::path const& outZip,
                    std::string const&            entryName,
                    ByteVector const&             data) {
    auto tmpPath = outZip;
    tmpPath += ".tmp";

    auto zip = geode::utils::file::Zip::create(tmpPath);
    if (zip.isErr()) {
        geode::log::error("writeZipAtomic: Zip::create failed: {}", zip.unwrapErr());
        return false;
    }

    geode::ByteSpan span(data.data(), data.size());
    auto addRes = zip.unwrap().add(entryName, span);
    if (addRes.isErr()) {
        geode::log::error("writeZipAtomic: Zip::add failed: {}", addRes.unwrapErr());
        std::error_code ec;
        // Geode utils::file has no generic path remove; error_code overload avoids throw.
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, outZip, ec);
    if (ec) {
        geode::log::error("writeZipAtomic: rename failed: {}", ec.message());
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

Result<ByteVector> readZipEntry(std::filesystem::path const& inZip,
                                std::string const&            entryName) {
    Result<ByteVector> out;

    auto unzipRes = geode::utils::file::Unzip::create(inZip);
    if (unzipRes.isErr()) {
        out.error = "readZipEntry: Unzip::create failed: " + unzipRes.unwrapErr();
        return out;
    }
    auto unzip = std::move(unzipRes).unwrap();

    std::filesystem::path target;
    if (entryName.empty()) {
        auto entries = unzip.getEntries();
        if (entries.empty()) {
            out.error = "readZipEntry: zip has no entries";
            return out;
        }
        target = entries[0];
    } else {
        target = entryName;
    }

    if (!unzip.hasEntry(target)) {
        out.error = "readZipEntry: entry '"
            + geode::utils::string::pathToString(target) + "' not found";
        return out;
    }

    auto bytesRes = unzip.extract(target);
    if (bytesRes.isErr()) {
        out.error = "readZipEntry: extract failed: " + bytesRes.unwrapErr();
        return out;
    }

    out.value = std::move(bytesRes).unwrap();
    out.ok = true;
    return out;
}

bool extractZipToFile(std::filesystem::path const& inZip,
                      std::filesystem::path const& outFile,
                      std::string const&            entryName) {
    auto res = readZipEntry(inZip, entryName);
    if (!res.ok) {
        geode::log::error("extractZipToFile: {}", res.error);
        return false;
    }

    auto writeRes = geode::utils::file::writeBinary(outFile, res.value);
    if (writeRes.isErr()) {
        geode::log::error("extractZipToFile: writeBinary failed: {}", writeRes.unwrapErr());
        return false;
    }
    return true;
}

} // namespace git_editor
