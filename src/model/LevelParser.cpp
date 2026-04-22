#include "LevelParser.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace git_editor {

namespace {

std::vector<std::string_view> splitView(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.emplace_back(s.data() + start, i - start);
            start = i + 1;
        }
    }
    return out;
}

bool parseKey(std::string_view s, int& out) {
    if (s.empty()) return false;
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) return false;
    out = value;
    return true;
}

FieldMap readKvChunk(std::string_view chunk) {
    FieldMap out;
    if (chunk.empty()) return out;

    auto tokens = splitView(chunk, ',');
    if (!tokens.empty() && tokens.back().empty()) tokens.pop_back();

    for (std::size_t i = 0; i + 1 < tokens.size(); i += 2) {
        int k = 0;
        if (!parseKey(tokens[i], k)) continue;
        out.emplace(k, std::string(tokens[i + 1]));
    }
    return out;
}

std::string serializeFields(FieldMap const& m) {
    std::string out;
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) out.push_back(',');
        first = false;
        out.append(std::to_string(k));
        out.push_back(',');
        out.append(v);
    }
    return out;
}

} // namespace

LevelState parseLevelString(std::string_view raw) {
    LevelState state;

    auto chunks = splitView(raw, ';');
    if (chunks.empty()) return state;

    state.header = readKvChunk(chunks.front());
    // rawHeader is residue: only populated when parse->serialize doesn't round-trip
    // (e.g. GD kS/kA string-keyed headers). Normal numeric-keyed headers leave it empty
    // so header edits merge granularly via header FieldMap.
    if (serializeFields(state.header) == chunks.front()) {
        state.rawHeader.clear();
    } else {
        state.rawHeader = std::string(chunks.front());
    }

    for (std::size_t i = 1; i < chunks.size(); ++i) {
        if (chunks[i].empty()) continue;
        Object obj;
        obj.fields = readKvChunk(chunks[i]);
        if (obj.fields.empty()) continue;
        // Placeholder uuid = chunk index, Matcher rewrites before persist (stable order for matching).
        obj.uuid = static_cast<ObjectUuid>(i);
        state.objects.emplace(obj.uuid, std::move(obj));
    }

    return state;
}

std::string serializeLevelString(LevelState const& state) {
    std::string out;
    out.reserve(256 + state.objects.size() * 96);

    // rawHeader is authoritative only when non-empty (residue path for non-round-trippable
    // GD headers). Normal numeric-keyed headers serialize from state.header.
    if (!state.rawHeader.empty()) {
        out.append(state.rawHeader);
    } else {
        out.append(serializeFields(state.header));
    }
    out.push_back(';');

    std::vector<ObjectUuid> ids;
    ids.reserve(state.objects.size());
    for (auto const& [id, _] : state.objects) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (auto id : ids) {
        auto const& obj = state.objects.at(id);
        out.append(serializeFields(obj.fields));
        out.push_back(';');
    }

    return out;
}

} // namespace git_editor
