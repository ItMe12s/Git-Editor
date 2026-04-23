#include "GdHeader.hpp"
#include "../util/Parsing.hpp"

#include <set>
#include <unordered_map>

namespace git_editor::gd_header {

namespace {

template <class T>
bool eq(std::optional<T> const& a, std::optional<T> const& b) {
    return a == b;
}

} // namespace

std::optional<KvList> parseHeader(std::string_view chunk) {
    KvList out;
    auto tokens = parsing::splitView(chunk, ',');
    if (!tokens.empty() && tokens.back().empty()) tokens.pop_back();
    if (tokens.size() % 2 != 0) return std::nullopt;

    std::set<std::string> seen;
    out.reserve(tokens.size() / 2);
    for (std::size_t i = 0; i < tokens.size(); i += 2) {
        std::string key(tokens[i]);
        if (!seen.insert(key).second) return std::nullopt;
        out.emplace_back(std::move(key), std::string(tokens[i + 1]));
    }
    return out;
}

std::string serializeHeader(KvList const& kv) {
    std::string out;
    for (std::size_t i = 0; i < kv.size(); ++i) {
        if (i > 0) out.push_back(',');
        out.append(kv[i].first);
        out.push_back(',');
        out.append(kv[i].second);
    }
    return out;
}

std::optional<ChannelMap> parseChannels(std::string_view value) {
    ChannelMap out;
    if (value.empty()) return out;

    auto records = parsing::splitView(value, '|');
    for (auto record : records) {
        if (record.empty()) continue;
        auto tokens = parsing::splitView(record, '_');
        if (tokens.size() % 2 != 0) return std::nullopt;

        ChannelRecord fields;
        for (std::size_t i = 0; i < tokens.size(); i += 2) {
            int key = 0;
            if (!parsing::parseInt(tokens[i], key)) return std::nullopt;
            fields[key] = std::string(tokens[i + 1]);
        }

        auto idIt = fields.find(6);
        if (idIt == fields.end()) return std::nullopt;
        int channelId = 0;
        if (!parsing::parseInt(idIt->second, channelId)) return std::nullopt;
        if (out.contains(channelId)) return std::nullopt;
        out.emplace(channelId, std::move(fields));
    }
    return out;
}

std::string serializeChannels(ChannelMap const& ch) {
    std::string out;
    bool firstRecord = true;
    for (auto const& [channelId, record] : ch) {
        if (!firstRecord) out.push_back('|');
        firstRecord = false;
        bool firstKv = true;
        for (auto const& [key, value] : record) {
            if (!firstKv) out.push_back('_');
            firstKv = false;
            out.append(std::to_string(key));
            out.push_back('_');
            out.append(value);
        }
        (void)channelId;
    }
    return out;
}

std::optional<std::string> mergeHeaders3Way(
    std::string_view base,
    std::string_view ours,
    std::string_view theirs,
    int& outConflicts
) {
    outConflicts = 0;
    auto bParsed = parseHeader(base);
    auto oParsed = parseHeader(ours);
    auto tParsed = parseHeader(theirs);
    if (!bParsed || !oParsed || !tParsed) return std::nullopt;

    std::unordered_map<std::string, std::string> bMap;
    std::unordered_map<std::string, std::string> oMap;
    std::unordered_map<std::string, std::string> tMap;
    for (auto const& [k, v] : *bParsed) bMap[k] = v;
    for (auto const& [k, v] : *oParsed) oMap[k] = v;
    for (auto const& [k, v] : *tParsed) tMap[k] = v;

    std::vector<std::string> keyOrder;
    std::set<std::string> keySeen;
    auto pushKey = [&](std::string const& key) {
        if (keySeen.insert(key).second) keyOrder.push_back(key);
    };
    for (auto const& [k, _] : *oParsed) pushKey(k);
    for (auto const& [k, _] : *tParsed) pushKey(k);
    for (auto const& [k, _] : *bParsed) pushKey(k);

    KvList mergedKv;
    for (auto const& key : keyOrder) {
        std::optional<std::string> b = bMap.contains(key) ? std::optional<std::string>(bMap.at(key)) : std::nullopt;
        std::optional<std::string> o = oMap.contains(key) ? std::optional<std::string>(oMap.at(key)) : std::nullopt;
        std::optional<std::string> t = tMap.contains(key) ? std::optional<std::string>(tMap.at(key)) : std::nullopt;

        if (key == "kS38") {
            auto parseOpt = [](std::optional<std::string> const& v) -> std::optional<std::optional<ChannelMap>> {
                if (!v.has_value()) return std::optional<ChannelMap>(std::nullopt);
                auto parsed = parseChannels(*v);
                if (!parsed) return std::nullopt;
                return std::optional<ChannelMap>(*parsed);
            };
            auto bCh = parseOpt(b);
            auto oCh = parseOpt(o);
            auto tCh = parseOpt(t);
            if (!bCh || !oCh || !tCh) {
                outConflicts++;
                if (o.has_value()) mergedKv.emplace_back(key, *o);
                continue;
            }

            std::optional<ChannelMap> mergedChannels;
            if (eq(*oCh, *tCh)) mergedChannels = *oCh;
            else if (eq(*oCh, *bCh)) mergedChannels = *tCh;
            else if (eq(*tCh, *bCh)) mergedChannels = *oCh;
            else {
                std::set<int> ids;
                if (*bCh) for (auto const& [id, _] : **bCh) ids.insert(id);
                if (*oCh) for (auto const& [id, _] : **oCh) ids.insert(id);
                if (*tCh) for (auto const& [id, _] : **tCh) ids.insert(id);
                ChannelMap outMap;
                for (int id : ids) {
                    std::optional<ChannelRecord> bRec = (*bCh && (**bCh).contains(id)) ? std::optional<ChannelRecord>((**bCh).at(id)) : std::nullopt;
                    std::optional<ChannelRecord> oRec = (*oCh && (**oCh).contains(id)) ? std::optional<ChannelRecord>((**oCh).at(id)) : std::nullopt;
                    std::optional<ChannelRecord> tRec = (*tCh && (**tCh).contains(id)) ? std::optional<ChannelRecord>((**tCh).at(id)) : std::nullopt;

                    std::optional<ChannelRecord> mergedRec;
                    if (eq(oRec, tRec)) mergedRec = oRec;
                    else if (eq(oRec, bRec)) mergedRec = tRec;
                    else if (eq(tRec, bRec)) mergedRec = oRec;
                    else {
                        outConflicts++;
                        mergedRec = oRec;
                    }
                    if (mergedRec) outMap[id] = *mergedRec;
                }
                mergedChannels = std::move(outMap);
            }

            if (mergedChannels) mergedKv.emplace_back(key, serializeChannels(*mergedChannels));
            continue;
        }

        std::optional<std::string> mergedValue;
        if (eq(o, t)) mergedValue = o;
        else if (eq(o, b)) mergedValue = t;
        else if (eq(t, b)) mergedValue = o;
        else {
            outConflicts++;
            mergedValue = o;
        }
        if (mergedValue) mergedKv.emplace_back(key, *mergedValue);
    }

    return serializeHeader(mergedKv);
}

} // namespace git_editor::gd_header
