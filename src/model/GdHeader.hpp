#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace git_editor::gd_header {

using KvList = std::vector<std::pair<std::string, std::string>>;
using ChannelRecord = std::map<int, std::string>;
using ChannelMap = std::map<int, ChannelRecord>;

std::optional<KvList> parseHeader(std::string_view chunk);
std::string serializeHeader(KvList const& kv);

std::optional<ChannelMap> parseChannels(std::string_view value);
std::string serializeChannels(ChannelMap const& ch);

std::optional<std::string> mergeHeaders3Way(
    std::string_view base,
    std::string_view ours,
    std::string_view theirs,
    int& outConflicts
);

} // namespace git_editor::gd_header
