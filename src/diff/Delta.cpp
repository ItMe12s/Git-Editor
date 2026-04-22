#include "Delta.hpp"

#include <Geode/loader/Log.hpp>
#include <matjson.hpp>

#include <charconv>
#include <cstdint>
#include <string>
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

std::string fieldKeyName(int k) {
    using namespace key;
    if (k == kType)     return "type";
    if (k == kX)        return "x";
    if (k == kY)        return "y";
    if (k == kRotation) return "rotation";
    if (k == kGroups)   return "groups";
    return "field " + std::to_string(k);
}

std::string truncForDisplay(std::string const& s, std::size_t maxCh) {
    if (s.size() <= maxCh) return s;
    if (maxCh <= 3) return s.substr(0, maxCh);
    return s.substr(0, maxCh - 3) + "...";
}

} // namespace

std::string dumpDelta(Delta const& d) {
    auto root = matjson::Value::object();

    root.set("h", headerChangesToJson(d.headerChanges));
    if (d.rawHeaderChange.has_value()) {
        root.set("hr", fieldChangeToJson(*d.rawHeaderChange));
    }

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
    if (auto r = root.get("hr"); r.isOk()) {
        out.rawHeaderChange = fieldChangeFromJson(r.unwrap());
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

DeltaStats computeStats(Delta const& d) {
    DeltaStats out;
    out.headerChanges = static_cast<int>(d.headerChanges.size())
                      + (d.rawHeaderChange.has_value() ? 1 : 0);
    out.adds          = static_cast<int>(d.adds.size());
    out.modifies      = static_cast<int>(d.modifies.size());
    out.removes       = static_cast<int>(d.removes.size());
    return out;
}

std::string describeDeltaText(Delta const& d) {
    constexpr std::size_t kMaxField = 160;
    constexpr std::size_t kMaxOut  = 32000;
    if (d.headerChanges.empty() && !d.rawHeaderChange.has_value()
        && d.adds.empty() && d.removes.empty() && d.modifies.empty()) {
        return "No recorded changes (empty delta).\n";
    }
    std::string out;
    out.reserve(2048);
    bool truncated = false;
    auto appendStr = [&](std::string const& s) {
        if (out.size() + s.size() + 1 > kMaxOut) {
            truncated = true;
            return;
        }
        out += s;
        out += '\n';
    };
    for (auto const& [k, ch] : d.headerChanges) {
        if (truncated) break;
        appendStr("Header " + fieldKeyName(k) + ": " + truncForDisplay(ch.before, kMaxField) + " -> "
            + truncForDisplay(ch.after, kMaxField));
    }
    if (d.rawHeaderChange.has_value() && !truncated) {
        appendStr("Header raw: " + truncForDisplay(d.rawHeaderChange->before, kMaxField) + " -> "
            + truncForDisplay(d.rawHeaderChange->after, kMaxField));
    }
    for (auto const& o : d.adds) {
        if (truncated) break;
        std::string line = "+ object " + std::to_string(o.uuid);
        if (auto it = o.fields.find(key::kType); it != o.fields.end()) {
            line += " (type=";
            line += truncForDisplay(it->second, 40);
            line += ')';
        }
        appendStr(line);
    }
    for (auto const& o : d.removes) {
        if (truncated) break;
        appendStr("- object " + std::to_string(o.uuid));
    }
    for (auto const& m : d.modifies) {
        if (truncated) break;
        appendStr("~ object " + std::to_string(m.uuid));
        for (auto const& [k, ch] : m.fields) {
            if (truncated) break;
            appendStr("  " + fieldKeyName(k) + ": " + truncForDisplay(ch.before, kMaxField) + " -> "
                + truncForDisplay(ch.after, kMaxField));
        }
    }
    if (truncated) {
        appendStr("[Output truncated.]");
    }
    return out;
}

} // namespace git_editor
