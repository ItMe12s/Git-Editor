#pragma once

#include "CommitStore.hpp"
#include "GdgePackage.hpp"
#include "../core/Result.hpp"

#include <string>
#include <vector>

namespace git_editor {

Result<GdgePackageData> buildGdgePackageFromCommits(
    LevelKey const&               levelKey,
    CommitId                      head,
    std::string const&            rootHash,
    std::vector<CommitRow> const& commitsNewestFirst
);

} // namespace git_editor
