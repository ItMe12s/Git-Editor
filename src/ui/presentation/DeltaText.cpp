#include "DeltaText.hpp"

#include "UiText.hpp"

#include <string>

namespace git_editor {

namespace {

std::string fieldKeyName(std::string const& k) {
    if (k == key::kType)     return "type";
    if (k == key::kX)        return "x";
    if (k == key::kY)        return "y";
    if (k == key::kRotation) return "rotation";
    if (k == key::kGroups)   return "groups";
    return "field " + k;
}

} // namespace

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
        appendStr("Header " + fieldKeyName(k) + ": " + shorten(ch.before, kMaxField) + " -> "
            + shorten(ch.after, kMaxField));
    }
    if (d.rawHeaderChange.has_value() && !truncated) {
        appendStr("Header raw: " + shorten(d.rawHeaderChange->before, kMaxField) + " -> "
            + shorten(d.rawHeaderChange->after, kMaxField));
    }
    for (auto const& o : d.adds) {
        if (truncated) break;
        std::string line = "+ object " + std::to_string(o.uuid);
        if (auto it = o.fields.find(key::kType); it != o.fields.end()) {
            line += " (type=";
            line += shorten(it->second, 40);
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
            appendStr("  " + fieldKeyName(k) + ": " + shorten(ch.before, kMaxField) + " -> "
                + shorten(ch.after, kMaxField));
        }
    }
    if (truncated) {
        appendStr("[Output truncated.]");
    }
    return out;
}

} // namespace git_editor
