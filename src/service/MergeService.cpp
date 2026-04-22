#include "MergeService.hpp"

#include "../model/GdHeader.hpp"

#include <set>

namespace git_editor {

std::optional<LevelState> mergeStates3Way(
    LevelState const& base,
    LevelState const& ours,
    LevelState const& theirs,
    int& conflictCount
) {
    conflictCount = 0;
    LevelState merged = ours;

    auto mergeFieldMap = [&](FieldMap& target, FieldMap const& b, FieldMap const& o, FieldMap const& t) {
        std::set<int> keys;
        for (auto const& [k, _] : b) keys.insert(k);
        for (auto const& [k, _] : o) keys.insert(k);
        for (auto const& [k, _] : t) keys.insert(k);
        for (int k : keys) {
            auto get = [](FieldMap const& m, int key) -> std::string {
                auto it = m.find(key);
                return it == m.end() ? "" : it->second;
            };
            auto vb = get(b, k);
            auto vo = get(o, k);
            auto vt = get(t, k);
            if (vo == vt) {
                if (vo.empty()) target.erase(k);
                else target[k] = vo;
                continue;
            }
            if (vo == vb) {
                if (vt.empty()) target.erase(k);
                else target[k] = vt;
                continue;
            }
            if (vt == vb) {
                if (vo.empty()) target.erase(k);
                else target[k] = vo;
                continue;
            }
            conflictCount++;
        }
    };

    mergeFieldMap(merged.header, base.header, ours.header, theirs.header);
    int headerConflicts = 0;
    auto structured = gd_header::mergeHeaders3Way(
        base.rawHeader, ours.rawHeader, theirs.rawHeader, headerConflicts
    );
    if (structured) {
        merged.rawHeader = std::move(*structured);
        conflictCount += headerConflicts;
    } else {
        if (ours.rawHeader == theirs.rawHeader) merged.rawHeader = ours.rawHeader;
        else if (ours.rawHeader == base.rawHeader) merged.rawHeader = theirs.rawHeader;
        else if (theirs.rawHeader == base.rawHeader) merged.rawHeader = ours.rawHeader;
        else {
            merged.rawHeader = ours.rawHeader;
            conflictCount++;
        }
    }

    std::set<ObjectUuid> ids;
    for (auto const& [id, _] : base.objects) ids.insert(id);
    for (auto const& [id, _] : ours.objects) ids.insert(id);
    for (auto const& [id, _] : theirs.objects) ids.insert(id);
    ObjectUuid nextUuid = 1;
    for (auto const& [id, _] : merged.objects) {
        if (id >= nextUuid) nextUuid = id + 1;
    }

    for (auto id : ids) {
        auto b = base.objects.find(id);
        auto o = ours.objects.find(id);
        auto t = theirs.objects.find(id);
        bool hasB = b != base.objects.end();
        bool hasO = o != ours.objects.end();
        bool hasT = t != theirs.objects.end();

        if (!hasO && !hasT) {
            merged.objects.erase(id);
            continue;
        }
        if (!hasB) {
            if (!hasO && hasT) { merged.objects[id] = t->second; continue; }
            if (hasO && !hasT) { continue; }
            if (o->second.fields == t->second.fields) {
                merged.objects[id] = o->second;
                continue;
            }
            auto importedObj = t->second;
            importedObj.uuid = nextUuid++;
            merged.objects[importedObj.uuid] = std::move(importedObj);
            continue;
        }

        if (hasB && !hasO && hasT) {
            if (t->second.fields == b->second.fields) merged.objects.erase(id);
            else conflictCount++;
            continue;
        }
        if (hasB && hasO && !hasT) {
            if (o->second.fields == b->second.fields) merged.objects.erase(id);
            else conflictCount++;
            continue;
        }
        if (hasO && hasT) {
            auto mergedObj = o->second;
            mergeFieldMap(mergedObj.fields, b->second.fields, o->second.fields, t->second.fields);
            merged.objects[id] = std::move(mergedObj);
        }
    }

    return merged;
}

} // namespace git_editor
