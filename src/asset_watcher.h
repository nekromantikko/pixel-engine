#pragma once
#ifdef EDITOR

#include "typedef.h"
#include <filesystem>
#include <unordered_map>
#include <string>

namespace AssetWatcher {
    // File change types
    enum ChangeType {
        ADDED,
        DELETED,
        MODIFIED,
        RENAMED
    };

    // Structure to track file information
    struct FileInfo {
        std::filesystem::file_time_type lastWriteTime;
        std::uintmax_t size;
        bool exists;
    };

    // Structure to represent a file change event
    struct FileChangeEvent {
        std::filesystem::path path;
        ChangeType changeType;
        std::filesystem::path oldPath;  // For renames
    };

    // Initialize the watcher with the assets source directory
    void Init(const std::filesystem::path& assetsDir);

    // Cleanup the watcher
    void Free();

    // Check for file changes and process them (call from editor update loop)
    void Update();

    // Get the current watch directory
    const std::filesystem::path& GetWatchDirectory();

    // Check if watcher is active
    bool IsWatching();
}

#endif // EDITOR