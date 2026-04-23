#include "LevelParser.hpp"
#include "../util/Parsing.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace git_editor {

namespace {

FieldMap readKvChunk(std::string_view chunk) {
    FieldMap out;
    if (chunk.empty()) return out;

    auto tokens = parsing::splitView(chunk, ',');
    if (!tokens.empty() && tokens.back().empty()) tokens.pop_back();

    for (std::size_t i = 0; i + 1 < tokens.size(); i += 2) {
        int k = 0;
        if (!parsing::parseInt(tokens[i], k)) continue;
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

    auto chunks = parsing::splitView(raw, ';');
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
