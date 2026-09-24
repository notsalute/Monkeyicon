#pragma once

#include <filesystem>
#include <string>

struct IconPatchResult {
    bool success = false;
    bool backupFound = false;
    std::wstring message;
};

class IconPatcher {
public:
    [[nodiscard]] static std::filesystem::path BackupPath(const std::filesystem::path& executable);
    [[nodiscard]] static IconPatchResult Patch(const std::filesystem::path& executable,
                                               const std::filesystem::path& iconFile);
    [[nodiscard]] static IconPatchResult Restore(const std::filesystem::path& executable);
};
