#pragma once

#include <filesystem>
#include <string>

namespace WindowsUtils {
    [[nodiscard]] std::filesystem::path AppDataDirectory();
    [[nodiscard]] std::filesystem::path StartMenuProgramsDirectory();
    [[nodiscard]] std::filesystem::path StartupDirectory();
    [[nodiscard]] std::filesystem::path DesktopDirectory();
    [[nodiscard]] std::wstring BrowseForImage(void* ownerWindow);
    [[nodiscard]] std::wstring BrowseForApplicationExecutable(void* ownerWindow);
    [[nodiscard]] std::string ToUtf8(const std::wstring& value);
    [[nodiscard]] std::wstring ErrorMessage(unsigned long errorCode);
    bool RefreshIconCache(std::wstring& error);
    void OpenFolderAndSelect(const std::filesystem::path& item);
}
