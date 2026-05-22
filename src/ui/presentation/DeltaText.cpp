#include "DeltaText.hpp"

#include "util/format/Shorten.hpp"

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
    if (d.headerChanges.empty() && !d.rawHeaderChange.has_value()
        && d.adds.empty() && d.removes.empty() && d.modifies.empty()) {
        return "No recorded changes (empty delta).\n";
    }
    std::string out;
    out.reserve(2048);
    auto appendStr = [&](std::string const& s) {
        out += s;
        out += '\n';
    };
    for (auto const& [k, ch] : d.headerChanges) {
        appendStr("Header " + fieldKeyName(k) + ": " + shorten(ch.before, kMaxField) + " -> "
            + shorten(ch.after, kMaxField));
    }
    if (d.rawHeaderChange.has_value()) {
        appendStr("Header raw: " + shorten(d.rawHeaderChange->before, kMaxField) + " -> "
            + shorten(d.rawHeaderChange->after, kMaxField));
    }
    for (auto const& o : d.adds) {
        std::string line = "+ object " + std::to_string(o.uuid);
        if (auto it = o.fields.find(key::kType); it != o.fields.end()) {
            line += " (type=";
            line += shorten(it->second, 40);
            line += ')';
        }
        appendStr(line);
    }
    for (auto const& o : d.removes) {
        appendStr("- object " + std::to_string(o.uuid));
    }
    for (auto const& m : d.modifies) {
        appendStr("~ object " + std::to_string(m.uuid));
        for (auto const& [k, ch] : m.fields) {
            appendStr("  " + fieldKeyName(k) + ": " + shorten(ch.before, kMaxField) + " -> "
                + shorten(ch.after, kMaxField));
        }
    }
    return out;
}

} // namespace git_editor
