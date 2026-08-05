#include "Delta.hpp"

#include <Geode/loader/Log.hpp>
#include <matjson.hpp>
#include <matjson/stl_serialize.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace git_editor {

namespace {

bool parseUInt64Full(std::string_view s, std::uint64_t& out) {
    if (s.empty()) return false;
    std::uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) return false;
    out = value;
    return true;
}

FieldMap fieldMapFromJson(matjson::Value const& v) {
    FieldMap out;
    if (!v.isObject()) return out;
    for (auto const& entry : v) {
        auto key = entry.getKey();
        if (!key) continue;
        auto asStr = entry.asString();
        if (asStr.isOk()) out.emplace(std::string(key->data(), key->size()), asStr.unwrap());
    }
    return out;
}

} // namespace

} // namespace git_editor

namespace matjson {

template <>
struct Serialize<git_editor::FieldChange> {
    static Value toJson(git_editor::FieldChange const& change) {
        auto value = Value::object();
        value.set("b", change.before);
        value.set("a", change.after);
        return value;
    }

    static geode::Result<git_editor::FieldChange> fromJson(Value const& value) {
        git_editor::FieldChange change;
        if (auto before = value.get<std::string>("b"); before.isOk()) {
            change.before = std::move(before).unwrap();
        }
        if (auto after = value.get<std::string>("a"); after.isOk()) {
            change.after = std::move(after).unwrap();
        }
        return geode::Ok(std::move(change));
    }
};

template <>
struct Serialize<git_editor::Object> {
    static Value toJson(git_editor::Object const& object) {
        auto value = Value::object();
        value.set("uuid", std::to_string(object.uuid));
        value.set("fields", object.fields);
        return value;
    }

    static geode::Result<git_editor::Object> fromJson(Value const& value) {
        git_editor::Object object;
        if (auto uuid = value.get<std::string>("uuid"); uuid.isOk()) {
            git_editor::parseUInt64Full(uuid.unwrap(), object.uuid);
        }
        if (auto fields = value.get("fields"); fields.isOk()) {
            object.fields = git_editor::fieldMapFromJson(fields.unwrap());
        }
        return geode::Ok(std::move(object));
    }
};

template <>
struct Serialize<git_editor::Delta::Modify> {
    static Value toJson(git_editor::Delta::Modify const& modify) {
        auto value = Value::object();
        value.set("uuid", std::to_string(modify.uuid));
        value.set("fields", modify.fields);
        return value;
    }

    static geode::Result<git_editor::Delta::Modify> fromJson(Value const& value) {
        git_editor::Delta::Modify modify;
        if (auto uuid = value.get<std::string>("uuid"); uuid.isOk()) {
            git_editor::parseUInt64Full(uuid.unwrap(), modify.uuid);
        }
        if (auto fields = value.get<std::map<std::string, git_editor::FieldChange>>("fields"); fields.isOk()) {
            modify.fields = std::move(fields).unwrap();
        }
        return geode::Ok(std::move(modify));
    }
};

template <>
struct Serialize<git_editor::Delta> {
    static Value toJson(git_editor::Delta const& delta) {
        auto value = Value::object();
        value.set("h", delta.headerChanges);
        if (delta.rawHeaderChange.has_value()) {
            value.set("hr", *delta.rawHeaderChange);
        }
        value.set("+", delta.adds);
        value.set("-", delta.removes);
        value.set("~", delta.modifies);
        return value;
    }

    static geode::Result<git_editor::Delta> fromJson(Value const& value) {
        if (!value.isObject()) return geode::Err("root is not an object");

        git_editor::Delta delta;
        if (auto changes = value.get<std::map<std::string, git_editor::FieldChange>>("h"); changes.isOk()) {
            delta.headerChanges = std::move(changes).unwrap();
        }
        if (auto raw = value.get<git_editor::FieldChange>("hr"); raw.isOk()) {
            delta.rawHeaderChange = std::move(raw).unwrap();
        }
        if (auto adds = value.get<std::vector<git_editor::Object>>("+"); adds.isOk()) {
            delta.adds = std::move(adds).unwrap();
        }
        if (auto removes = value.get<std::vector<git_editor::Object>>("-"); removes.isOk()) {
            delta.removes = std::move(removes).unwrap();
        }
        if (auto modifies = value.get<std::vector<git_editor::Delta::Modify>>("~"); modifies.isOk()) {
            delta.modifies = std::move(modifies).unwrap();
        }
        return geode::Ok(std::move(delta));
    }
};

} // namespace matjson

namespace git_editor {

std::string dumpDelta(Delta const& d) {
    return matjson::Value(d).dump(matjson::NO_INDENTATION);
}

std::optional<Delta> parseDelta(std::string const& blob) {
    auto parsed = matjson::Value::parse(blob);
    if (parsed.isErr()) {
        geode::log::error(
            "parseDelta failed: {}",
            std::string(parsed.unwrapErr())
        );
        return std::nullopt;
    }
    auto delta = std::move(parsed).unwrap().as<Delta>();
    if (delta.isErr()) {
        geode::log::error("parseDelta: root is not an object");
        return std::nullopt;
    }
    return std::move(delta).unwrap();
}

} // namespace git_editor
