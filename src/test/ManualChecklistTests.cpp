#include "AutomatedTestHarness.hpp"

namespace git_editor {

void appendManualSkips(ReportBuilder& R) {
    struct Item {
        char const* name;
        char const* reason;
    };
    static constexpr Item kItems[] = {
        { "checkout_editor_verify", "needs live LevelEditorLayer apply and visual verify" },
        { "revert_conflict_popup", "needs UI conflict summary layer" },
        { "revert_20plus_lag", "needs manual perf observation" },
        { "import_plan_popup", "needs UI plan popup" },
        { "import_state_visual", "needs editor state compare" },
        { "collab_visual", "needs editor verify" },
        { "edge_scroll_tap_lag", "needs manual UI interaction" },
        { "geode_log_first_launch", "needs external log inspection" },
        { "node_ids_devtools", "needs geode.node-ids querySelector" },
    };
    for (auto const& it : kItems) {
        R.addAction(kSuiteManual, std::string("SKIP ") + it.name + ": " + it.reason);
        ScopedTimer rowT;
        R.addSkip(kSuiteManual, it.name, it.reason, rowT.ms());
    }
}

} // namespace git_editor
