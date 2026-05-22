#pragma once

#include "diff/Differ.hpp"
#include "model/LevelState.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace git_editor {

struct RevertPayload {
    LevelState            state;
    std::vector<Conflict> conflicts;
};

struct ImportManyPayload {
    LevelState state;
    int        mergedCount     = 0;
    int        skippedCount    = 0;
    int        conflictCount   = 0;
    int        smartCount      = 0;
    int        sequentialCount = 0;
};

struct InvalidImport {
    std::filesystem::path path;
    std::string           reason;
};

struct ImportPlan {
    std::vector<std::filesystem::path> smart;
    std::vector<std::filesystem::path> sequential;
    std::vector<InvalidImport>         invalid;
    std::string localRootHash;
    bool noLocalCommits = false;
};

} // namespace git_editor
