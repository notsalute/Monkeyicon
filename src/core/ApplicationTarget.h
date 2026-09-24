#pragma once

#include <filesystem>
#include <string>

struct ApplicationTarget {
    std::filesystem::path rootDirectory;
    std::filesystem::path executablePath;
    std::filesystem::path updateExecutable;
    std::wstring version;
    std::wstring displayName;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::filesystem::path LaunchTarget() const;
    [[nodiscard]] std::filesystem::path WorkingDirectory() const;
    [[nodiscard]] std::wstring LaunchArguments() const;
};

class ApplicationDetector {
public:
    [[nodiscard]] static ApplicationTarget DetectSuggested();
    [[nodiscard]] static ApplicationTarget DetectDiscordFromRoot(const std::filesystem::path& root);
    [[nodiscard]] static ApplicationTarget FromExecutable(const std::filesystem::path& executable);
};
