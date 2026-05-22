#pragma once

#include "GitUiActionRunner.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <alphalaneous.alphas-ui-pack/include/API.hpp>

#include <cstdint>
#include <functional>

namespace git_editor::scroll_list_popup {

struct Layout {
    static constexpr float kWidth     = 420.f;
    static constexpr float kHeight    = 280.f;
    static constexpr float kPadX      = 20.f;
    static constexpr float kPadTop    = 36.f;
    static constexpr float kPadBottom = 16.f;
};

struct ListState {
    bool        closing    = false;
    std::uint64_t loadSerial = 0;
};

float innerWidth();
float innerHeight();

alpha::ui::AdvancedScrollLayer* attachScrollList(
    geode::Popup* popup,
    cocos2d::CCNode* mainLayer,
    char const* scrollId
);

void markClosing(ListState& state, alpha::ui::AdvancedScrollLayer*& scroll);

bool isStaleLoad(ListState const& state, std::uint64_t serial);

void showCenteredLabel(
    cocos2d::CCNode* content,
    char const* text,
    char const* labelId,
    float opacity = 160.f
);

void resetScrollTop(alpha::ui::AdvancedScrollLayer* scroll);

std::uint64_t beginLoading(
    ListState& state,
    alpha::ui::AdvancedScrollLayer* scroll,
    char const* loadingText,
    char const* loadingId
);

template <class TResult, class Loader, class OnLoaded, class IsCurrent>
void loadAsync(
    ListState& state,
    alpha::ui::AdvancedScrollLayer* scroll,
    char const* loadingText,
    char const* loadingId,
    Loader&& loader,
    IsCurrent&& isCurrent,
    OnLoaded&& onLoaded
) {
    if (state.closing || !scroll) return;

    auto const serial = beginLoading(state, scroll, loadingText, loadingId);
    git_editor::ui_action_runner::runWorkerResult<TResult>(
        std::forward<Loader>(loader),
        [isCurrent = std::forward<IsCurrent>(isCurrent),
         serial,
         onLoaded = std::forward<OnLoaded>(onLoaded)](TResult result) mutable {
            if (!isCurrent(serial)) return;
            onLoaded(std::move(result));
        }
    );
}

} // namespace git_editor::scroll_list_popup
