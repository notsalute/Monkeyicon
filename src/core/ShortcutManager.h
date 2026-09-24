#pragma once

#include "core/ApplicationTarget.h"

#include <filesystem>
#include <string>

struct ShortcutResult {
    bool success = false;
    std::wstring message;
    std::filesystem::path shortcutPath;
};

class ShortcutManager {
public:
    [[nodiscard]] static std::filesystem::path CustomShortcutPath(const ApplicationTarget& application);
    [[nodiscard]] static std::filesystem::path StartupShortcutPath(const ApplicationTarget& application);
    [[nodiscard]] static std::filesystem::path IconStoragePath(const ApplicationTarget& application);
    [[nodiscard]] static ShortcutResult Apply(const ApplicationTarget& installation,
                                              const std::filesystem::path& iconPath,
                                              bool enableStartup);
    [[nodiscard]] static ShortcutResult Restore(const ApplicationTarget& application);
};
