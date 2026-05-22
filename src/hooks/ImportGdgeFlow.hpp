#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace git_editor::import_gdge_flow {

void startImportGdgeFlow(
    geode::Ref<EditorPauseLayer> alive,
    geode::Ref<LevelEditorLayer> editorRef,
    std::string levelKey,
    std::vector<std::filesystem::path> paths
);

} // namespace git_editor::import_gdge_flow
