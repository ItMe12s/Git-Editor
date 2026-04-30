#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace git_editor {

struct AutomatedTestCaseResult {
    std::string suite;
    std::string name;
    std::string status;
    std::string detail;
    double      elapsedMs = 0;
};

struct AutomatedTestSummary {
    int                 passCount = 0;
    int                 failCount = 0;
    int                 skipCount = 0;
    std::string         reportText;
    std::vector<std::string> actionLog;
    std::vector<AutomatedTestCaseResult> rows;
};

AutomatedTestSummary runAutomatedTests(std::filesystem::path const& saveDir, std::string const& modId);

bool writeTextFileUtf8(std::filesystem::path const& path, std::string const& utf8);

} // namespace git_editor
