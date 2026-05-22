#pragma once

#include "store/CommitStore.hpp"

#include <Geode/Geode.hpp>
#include <cocos2d.h>

namespace git_editor {

class HistoryLayer;

namespace history_rows {

constexpr float kRowHeight = 46.f;

cocos2d::CCNode* createCommitRow(
    CommitSummary const& commit,
    float rowWidth,
    bool squashMode,
    bool selected,
    geode::Ref<HistoryLayer> layer
);

} // namespace history_rows
} // namespace git_editor
