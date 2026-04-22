#include "Delta.hpp"

#include <Geode/loader/Log.hpp>
#include <matjson.hpp>

#include <charconv>
#include <cstdint>
#include <string_view>

namespace git_editor {

namespace {

bool parseIntKey(std::string_view s, int& out) {
    if (s.empty()) return false;
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) return false;
    out = value;
    return true;
}

bool parseUInt64Full(std::string_view s, std::uint64_t& out) {
    if (s.empty()) return false;
    std::uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) return false;
    out = value;
    return true;
}

matjson::Value fieldMapToJson(FieldMap const& m) {
    auto obj = matjson::Value::object();
    for (auto const& [k, v] : m) {
        obj.set(std::to_string(k), v);
    }
    return obj;
}

FieldMap fieldMapFromJson(matjson::Value const& v) {
    FieldMap out;
    if (!v.isObject()) return out;
    for (auto const& entry : v) {
        auto key = entry.getKey();
        if (!key) continue;
        int k = 0;
        if (!parseIntKey(std::string_view(key->data(), key->size()), k)) {
            continue;
        }
        auto asStr = entry.asString();
        if (asStr.isOk()) out.emplace(k, asStr.unwrap());
    }
    return out;
}

matjson::Value objectToJson(Object const& o) {
    auto obj = matjson::Value::object();
    obj.set("uuid",   std::to_string(o.uuid));
    obj.set("fields", fieldMapToJson(o.fields));
    return obj;
}

Object objectFromJson(matjson::Value const& v) {
    Object o;
    if (auto r = v.get("uuid"); r.isOk()) {
        auto asStr = r.unwrap().asString();
        if (asStr.isOk()) {
            auto const& us = asStr.unwrap();
            std::uint64_t u = 0;
            if (parseUInt64Full(std::string_view(us.data(), us.size()), u)) {
                o.uuid = u;
            } else {
                o.uuid = 0;
            }
        }
    }
    if (auto r = v.get("fields"); r.isOk()) {
        o.fields = fieldMapFromJson(r.unwrap());
    }
    return o;
}

matjson::Value fieldChangeToJson(FieldChange const& c) {
    auto obj = matjson::Value::object();
    obj.set("b", c.before);
    obj.set("a", c.after);
    return obj;
}

FieldChange fieldChangeFromJson(matjson::Value const& v) {
    FieldChange c;
    if (auto r = v.get("b"); r.isOk()) {
        auto s = r.unwrap().asString();
        if (s.isOk()) c.before = s.unwrap();
    }
    if (auto r = v.get("a"); r.isOk()) {
        auto s = r.unwrap().asString();
        if (s.isOk()) c.after = s.unwrap();
    }
    return c;
}

matjson::Value headerChangesToJson(std::map<int, FieldChange> const& hc) {
    auto obj = matjson::Value::object();
    for (auto const& [k, c] : hc) {
        obj.set(std::to_string(k), fieldChangeToJson(c));
    }
    return obj;
}

std::map<int, FieldChange> headerChangesFromJson(matjson::Value const& v) {
    std::map<int, FieldChange> out;
    if (!v.isObject()) return out;
    for (auto const& entry : v) {
        auto key = entry.getKey();
        if (!key) continue;
        int k = 0;
        if (!parseIntKey(std::string_view(key->data(), key->size()), k)) {
            continue;
        }
        out.emplace(k, fieldChangeFromJson(entry));
    }
    return out;
}

matjson::Value modifyToJson(Delta::Modify const& m) {
    auto obj = matjson::Value::object();
    obj.set("uuid", std::to_string(m.uuid));
    auto fields = matjson::Value::object();
    for (auto const& [k, c] : m.fields) {
        fields.set(std::to_string(k), fieldChangeToJson(c));
    }
    obj.set("fields", fields);
    return obj;
}

Delta::Modify modifyFromJson(matjson::Value const& v) {
    Delta::Modify m;
    if (auto r = v.get("uuid"); r.isOk()) {
        auto s = r.unwrap().asString();
        if (s.isOk()) {
            auto const& us = s.unwrap();
            std::uint64_t u = 0;
            if (parseUInt64Full(std::string_view(us.data(), us.size()), u)) {
                m.uuid = u;
            } else {
                m.uuid = 0;
            }
        }
    }
    if (auto r = v.get("fields"); r.isOk()) {
        auto const& fields = r.unwrap();
        if (fields.isObject()) {
            for (auto const& entry : fields) {
                auto key = entry.getKey();
                if (!key) continue;
                int k = 0;
                if (!parseIntKey(std::string_view(key->data(), key->size()), k)) {
                    continue;
                }
                m.fields.emplace(k, fieldChangeFromJson(entry));
            }
        }
    }
    return m;
}

} // namespace

std::string dumpDelta(Delta const& d) {
    auto root = matjson::Value::object();

    root.set("h", headerChangesToJson(d.headerChanges));

    auto adds = matjson::Value::array();
    for (auto const& o : d.adds) adds.push(objectToJson(o));
    root.set("+", adds);

    auto removes = matjson::Value::array();
    for (auto const& o : d.removes) removes.push(objectToJson(o));
    root.set("-", removes);

    auto mods = matjson::Value::array();
    for (auto const& m : d.modifies) mods.push(modifyToJson(m));
    root.set("~", mods);

    return root.dump(matjson::NO_INDENTATION);
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
    auto root = parsed.unwrap();
    if (!root.isObject()) {
        geode::log::error("parseDelta: root is not an object");
        return std::nullopt;
    }

    Delta out;
    if (auto r = root.get("h"); r.isOk()) {
        out.headerChanges = headerChangesFromJson(r.unwrap());
    }
    if (auto r = root.get("+"); r.isOk()) {
        auto const& arr = r.unwrap();
        if (arr.isArray()) {
            for (auto const& v : arr) {
                out.adds.push_back(objectFromJson(v));
            }
        }
    }
    if (auto r = root.get("-"); r.isOk()) {
        auto const& arr = r.unwrap();
        if (arr.isArray()) {
            for (auto const& v : arr) {
                out.removes.push_back(objectFromJson(v));
            }
        }
    }
    if (auto r = root.get("~"); r.isOk()) {
        auto const& arr = r.unwrap();
        if (arr.isArray()) {
            for (auto const& v : arr) {
                out.modifies.push_back(modifyFromJson(v));
            }
        }
    }
    return out;
}

} // namespace git_editor
