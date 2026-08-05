#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace git_editor {

    using ObjectUuid = std::uint64_t;

    // GD key/value pairs. Keys stay as raw strings.
    // Object data uses kA* and kS* string keys alongside numeric keys.
    using FieldMap = std::unordered_map<std::string, std::string>;

    namespace key {
        inline std::string const kType = "1";
        inline std::string const kX = "2";
        inline std::string const kY = "3";
        inline std::string const kRotation = "6";
        inline std::string const kGroups = "57";
    } // namespace key

    struct Object {
        ObjectUuid uuid = 0;
        FieldMap fields;
    };

    struct LevelState {
        std::string rawHeader;
        FieldMap header;
        std::unordered_map<ObjectUuid, Object> objects;
    };

    using LevelStatePtr = std::shared_ptr<LevelState const>;

} // namespace git_editor
