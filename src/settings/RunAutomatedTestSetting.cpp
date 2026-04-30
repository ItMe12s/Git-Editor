#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/ui/Notification.hpp>

#include "../test/AutomatedTestRunner.hpp"
#include "../util/GitWorker.hpp"
#include "../util/PathUtf8.hpp"

#include <fmt/format.h>

#include <atomic>
#include <string>

using namespace geode::prelude;

namespace {

std::atomic_bool s_automatedTestRunning{ false };

} // namespace

$execute {
    ButtonSettingPressedEventV3(Mod::get(), "run-automated-test")
        .listen([](std::string_view buttonKey) {
            if (buttonKey != "run") {
                return;
            }
            if (s_automatedTestRunning.exchange(true)) {
                queueInMainThread([] {
                    Notification::create(
                        "Automated test already running",
                        NotificationIcon::Warning
                    )->show();
                });
                return;
            }

            auto* mod = Mod::get();
            if (!mod) {
                s_automatedTestRunning = false;
                queueInMainThread([] {
                    Notification::create("Mod not available", NotificationIcon::Error)->show();
                });
                return;
            }

            auto const dir = mod->getSaveDir();
            std::string const modId = mod->getID();

            git_editor::postToGitWorker([dir, modId]() {
                struct RunningFlag {
                    ~RunningFlag() { s_automatedTestRunning = false; }
                } runningFlag;

                auto summary = git_editor::runAutomatedTests(dir, modId);
                auto const outPath = dir / "test-result.txt";
                if (!git_editor::writeTextFileUtf8(outPath, summary.reportText)) {
                    ++summary.failCount;
                    summary.reportText += "\nFAIL | ReportWrite | could not write test-result.txt\n";
                    static_cast<void>(git_editor::writeTextFileUtf8(outPath, summary.reportText));
                }

                int const passC = summary.passCount;
                int const failC = summary.failCount;
                int const skipC = summary.skipCount;
                std::string const pathStr = git_editor::pathUtf8(outPath);
                bool const failed = failC > 0;

                queueInMainThread([passC, failC, skipC, pathStr, failed] {
                    auto const msg = fmt::format(
                        "Automated test: PASS {} FAIL {} SKIP {}. {}",
                        passC,
                        failC,
                        skipC,
                        pathStr
                    );
                    Notification::create(
                        msg,
                        failed ? NotificationIcon::Error : NotificationIcon::Success
                    )->show();
                });
            });
        })
        .leak();
}
