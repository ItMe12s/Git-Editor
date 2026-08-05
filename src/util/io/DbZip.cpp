#include "DbZip.hpp"

#include "FileAtomic.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>
#include <array>
#include <cstring>
#include <fstream>
#include <string_view>

namespace git_editor {

    namespace {

        constexpr std::string_view kSqliteMagic = "SQLite format 3\000";
        constexpr std::size_t kSqliteMagicLen = 16;
        constexpr std::array<std::uint8_t, 4> kZipMagic = {0x50, 0x4B, 0x03, 0x04};

    } // namespace

    DbFileForm peekDbFileForm(std::filesystem::path const& path) {
        // Use ifstream(path) on Windows for native paths.
        // Read at most 16 bytes for a sniff. Do not read the whole file.
        std::ifstream f(path, std::ios::binary);
        if (!f) return DbFileForm::Unknown;

        std::array<char, 16> buf{};
        f.read(buf.data(), 16);
        auto const nread = static_cast<std::size_t>(f.gcount());

        if (nread >= kZipMagic.size() &&
            std::memcmp(buf.data(), kZipMagic.data(), kZipMagic.size()) == 0) {
            return DbFileForm::Zip;
        }

        if (nread >= kSqliteMagicLen &&
            std::memcmp(buf.data(), kSqliteMagic.data(), kSqliteMagicLen) == 0) {
            return DbFileForm::Sqlite;
        }

        return DbFileForm::Unknown;
    }

    bool writeZipAtomic(
        std::filesystem::path const& outZip, std::string const& entryName, ByteVector const& data
    ) {
        auto tmpPath = outZip;
        tmpPath += ".tmp";

        auto zipRes = geode::utils::file::Zip::create(tmpPath);
        if (zipRes.isErr()) {
            geode::log::error("writeZipAtomic: Zip::create failed: {}", zipRes.unwrapErr());
            return false;
        }

        {
            auto z = std::move(zipRes).unwrap();
            geode::ByteSpan span(data.data(), data.size());
            auto addRes = z.add(entryName, span);
            if (addRes.isErr()) {
                geode::log::error("writeZipAtomic: Zip::add failed: {}", addRes.unwrapErr());
                std::error_code remEc;
                std::filesystem::remove(tmpPath, remEc);
                return false;
            }
        }

        if (!replaceFileAtomic(tmpPath, outZip)) {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
        return true;
    }

    Result<ByteVector> readZipEntry(std::filesystem::path const& inZip, std::string const& entryName) {
        auto unzipRes = geode::utils::file::Unzip::create(inZip);
        if (unzipRes.isErr()) {
            return std::unexpected("readZipEntry: Unzip::create failed: " + unzipRes.unwrapErr());
        }
        auto unzip = std::move(unzipRes).unwrap();

        std::filesystem::path target;
        if (entryName.empty()) {
            auto entries = unzip.getEntries();
            if (entries.empty()) {
                return std::unexpected("readZipEntry: zip has no entries");
            }
            if (entries.size() != 1) {
                return std::unexpected(
                    "readZipEntry: zip must contain exactly one entry when no name given"
                );
            }
            target = entries[0];
        }
        else {
            target = entryName;
        }

        if (!unzip.hasEntry(target)) {
            return std::unexpected(
                "readZipEntry: entry '" + geode::utils::string::pathToString(target) + "' not found"
            );
        }

        auto bytesRes = unzip.extract(target);
        if (bytesRes.isErr()) {
            return std::unexpected("readZipEntry: extract failed: " + bytesRes.unwrapErr());
        }
        return std::move(bytesRes).unwrap();
    }

    Result<void> extractZipToFile(
        std::filesystem::path const& inZip, std::filesystem::path const& outFile,
        std::string const& entryName
    ) {
        auto res = readZipEntry(inZip, entryName);
        if (!res) {
            geode::log::error("extractZipToFile: {}", res.error());
            return std::unexpected(res.error());
        }

        auto writeRes = geode::utils::file::writeBinary(outFile, *res);
        if (writeRes.isErr()) {
            auto error = "writeBinary failed: " + writeRes.unwrapErr();
            geode::log::error("extractZipToFile: {}", error);
            return std::unexpected(std::move(error));
        }
        return {};
    }

} // namespace git_editor
