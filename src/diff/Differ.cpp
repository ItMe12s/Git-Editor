#include "Differ.hpp"

#include <algorithm>

namespace git_editor {

namespace {

// Missing key modeled as empty string, absent and empty-string merge. True presence distinction would need extra state.
std::map<int, FieldChange> diffFields(FieldMap const& a, FieldMap const& b) {
    std::map<int, FieldChange> out;

    auto itA = a.begin();
    auto itB = b.begin();
    while (itA != a.end() || itB != b.end()) {
        if (itA == a.end() || (itB != b.end() && itB->first < itA->first)) {
            out.emplace(itB->first, FieldChange{ "", itB->second });
            ++itB;
        } else if (itB == b.end() || itA->first < itB->first) {
            out.emplace(itA->first, FieldChange{ itA->second, "" });
            ++itA;
        } else {
            if (itA->second != itB->second) {
                out.emplace(itA->first, FieldChange{ itA->second, itB->second });
            }
            ++itA; ++itB;
        }
    }
    return out;
}

} // namespace

Delta diff(LevelState const& prev, LevelState const& next) {
    Delta d;
    d.headerChanges = diffFields(prev.header, next.header);
    if (prev.rawHeader != next.rawHeader) {
        d.rawHeaderChange = FieldChange{ prev.rawHeader, next.rawHeader };
    }

    for (auto const& [uuid, prevObj] : prev.objects) {
        auto it = next.objects.find(uuid);
        if (it == next.objects.end()) {
            d.removes.push_back(prevObj);
            continue;
        }
        auto fieldChanges = diffFields(prevObj.fields, it->second.fields);
        if (!fieldChanges.empty()) {
            d.modifies.push_back({ uuid, std::move(fieldChanges) });
        }
    }

    for (auto const& [uuid, nextObj] : next.objects) {
        if (!prev.objects.contains(uuid)) {
            d.adds.push_back(nextObj);
        }
    }

    return d;
}

Delta inverse(Delta const& d) {
    Delta out;

    for (auto const& [k, c] : d.headerChanges) {
        out.headerChanges.emplace(k, FieldChange{ c.after, c.before });
    }
    if (d.rawHeaderChange.has_value()) {
        out.rawHeaderChange = FieldChange{ d.rawHeaderChange->after, d.rawHeaderChange->before };
    }

    out.removes = d.adds;
    out.adds    = d.removes;

    for (auto const& m : d.modifies) {
        Delta::Modify inv;
        inv.uuid = m.uuid;
        for (auto const& [k, c] : m.fields) {
            inv.fields.emplace(k, FieldChange{ c.after, c.before });
        }
        out.modifies.push_back(std::move(inv));
    }

    return out;
}

LevelState apply(LevelState base, Delta const& d, std::vector<Conflict>* out) {
    auto report = [&](Conflict c) {
        if (out) out->push_back(std::move(c));
    };

    // Header: apply even if before mismatches (avoid blocking checkout), still report stale.
    for (auto const& [k, c] : d.headerChanges) {
        auto it = base.header.find(k);
        std::string current = (it != base.header.end()) ? it->second : "";
        if (current != c.before) {
            report({ Conflict::Kind::ModifyStale, 0, k,
                "header field " + std::to_string(k) + " drifted" });
        }
        if (c.after.empty()) base.header.erase(k);
        else                 base.header[k] = c.after;
    }
    if (d.rawHeaderChange.has_value()) {
        if (base.rawHeader != d.rawHeaderChange->before) {
            report({ Conflict::Kind::ModifyStale, 0, 0, "raw header drifted" });
        }
        base.rawHeader = d.rawHeaderChange->after;
    }

    for (auto const& o : d.adds) {
        if (base.objects.contains(o.uuid)) {
            report({ Conflict::Kind::AddAlreadyExists, o.uuid, 0,
                "object already present, add skipped" });
            continue;
        }
        base.objects.emplace(o.uuid, o);
    }

    for (auto const& o : d.removes) {
        auto it = base.objects.find(o.uuid);
        if (it == base.objects.end()) {
            report({ Conflict::Kind::RemoveMissing, o.uuid, 0,
                "object already removed, remove skipped" });
            continue;
        }
        base.objects.erase(it);
    }

    for (auto const& m : d.modifies) {
        auto it = base.objects.find(m.uuid);
        if (it == base.objects.end()) {
            report({ Conflict::Kind::ModifyMissing, m.uuid, 0,
                "target of modify is gone, modify skipped" });
            continue;
        }
        auto& fields = it->second.fields;
        for (auto const& [k, c] : m.fields) {
            auto fit = fields.find(k);
            std::string current = (fit != fields.end()) ? fit->second : "";
            if (current != c.before) {
                report({ Conflict::Kind::ModifyStale, m.uuid, k,
                    "field " + std::to_string(k) + " drifted since commit" });
                continue;
            }
            if (c.after.empty()) fields.erase(k);
            else                 fields[k] = c.after;
        }
    }

    return base;
}

} // namespace git_editor
