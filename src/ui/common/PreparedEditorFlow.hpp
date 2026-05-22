#pragma once

#include "service/PendingOps.hpp"
#include "core/Result.hpp"
#include "GitUiActionRunner.hpp"
#include "UiAction.hpp"

#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/ui/Notification.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace git_editor::prepared_editor_flow {

struct Guards {
    bool& busy;
    bool& closing;
};

struct OutcomeHandlers {
    std::function<void()> onSuccess;
    std::function<void(std::string const& error)> onPrepareError;
    std::function<void()> onApplyFailed;
    std::function<void(std::string const& error)> onFinalizeError;
    std::function<void()> onAppliedOnly;
};

inline void resumePauseIfNeeded(
    geode::Ref<EditorPauseLayer> pause,
    bool closedOrClosing
) {
    if (!closedOrClosing || !pause) return;
    auto* layer = pause.data();
    if (!layer || !layer->getParent() || !layer->isRunning()) return;
    layer->onResume(nullptr);
}

template <class TPayload, class TPending, class TFinalizeResult>
void run(
    Guards guards,
    std::function<Prepared<TPayload>()> prepareWorker,
    std::function<bool(Prepared<TPayload> const& prep)> applyEditor,
    std::function<std::optional<TPending>(Prepared<TPayload> const& prep)> extractPending,
    std::function<Result<TFinalizeResult>(TPending pending, TPayload const& payload)> finalizeWorker,
    OutcomeHandlers handlers
) {
    ui_action_runner::runWorkerResult<Prepared<TPayload>>(
        std::move(prepareWorker),
        [guards,
         applyEditor = std::move(applyEditor),
         extractPending = std::move(extractPending),
         finalizeWorker = std::move(finalizeWorker),
         handlers = std::move(handlers)](Prepared<TPayload> prep) mutable {
            if (exitBusyIfClosing(guards.busy, guards.closing)) return;
            if (!prep.result.ok) {
                finishBusyAction(guards.busy);
                if (handlers.onPrepareError) handlers.onPrepareError(prep.result.error);
                return;
            }
            if (!applyEditor(prep)) {
                finishBusyAction(guards.busy);
                if (handlers.onApplyFailed) handlers.onApplyFailed();
                return;
            }
            auto pendingOpt = extractPending(prep);
            if (!pendingOpt) {
                finishBusyAction(guards.busy);
                if (handlers.onAppliedOnly) handlers.onAppliedOnly();
                else if (handlers.onSuccess) handlers.onSuccess();
                return;
            }
            TPending pending = std::move(*pendingOpt);
            TPayload payload = prep.result.value;
            ui_action_runner::runWorkerResult<Result<TFinalizeResult>>(
                [pending = std::move(pending),
                 payload = std::move(payload),
                 finalizeWorker = std::move(finalizeWorker)]() mutable {
                    return finalizeWorker(std::move(pending), payload);
                },
                [guards, handlers = std::move(handlers)](Result<TFinalizeResult> fin) mutable {
                    finishBusyAction(guards.busy);
                    if (guards.closing) return;
                    if (!fin.ok) {
                        if (handlers.onFinalizeError) handlers.onFinalizeError(fin.error);
                        return;
                    }
                    if (handlers.onSuccess) handlers.onSuccess();
                }
            );
        }
    );
}

} // namespace git_editor::prepared_editor_flow
