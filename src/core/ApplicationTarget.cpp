#include "core/ApplicationTarget.h"

#include <Windows.h>
#include <algorithm>
#include <optional>
#include <vector>

namespace {
std::optional<std::filesystem::path> LocalAppData() {
    DWORD needed = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (needed == 0) return std::nullopt;
    std::wstring value(needed, L'\0');
    const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), needed);
    if (written == 0 || written >= needed) return std::nullopt;
    value.resize(written);
    return std::filesystem::path(value);
}

std::vector<int> VersionParts(std::wstring value) {
    if (value.rfind(L"app-", 0) == 0) value.erase(0, 4);
    std::vector<int> parts;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t end = value.find(L'.', start);
        const auto part = value.substr(start, end == std::wstring::npos ? value.size() - start : end - start);
        try { parts.push_back(std::stoi(part)); } catch (...) { parts.push_back(0); }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return parts;
}

bool NewerVersion(const std::filesystem::path& a, const std::filesystem::path& b) {
    return VersionParts(a.filename().wstring()) > VersionParts(b.filename().wstring());
}
}

bool ApplicationTarget::IsValid() const noexcept {
    std::error_code ec;
    return !executablePath.empty() && std::filesystem::is_regular_file(executablePath, ec);
}

std::filesystem::path ApplicationTarget::LaunchTarget() const {
    std::error_code ec;
    if (!updateExecutable.empty() && std::filesystem::is_regular_file(updateExecutable, ec)) return updateExecutable;
    return executablePath;
}

std::filesystem::path ApplicationTarget::WorkingDirectory() const {
    std::error_code ec;
    if (!updateExecutable.empty() && std::filesystem::is_regular_file(updateExecutable, ec)) return rootDirectory;
    return executablePath.parent_path();
}

std::wstring ApplicationTarget::LaunchArguments() const {
    std::error_code ec;
    if (!updateExecutable.empty() && std::filesystem::is_regular_file(updateExecutable, ec)) {
        return L"--processStart Discord.exe";
    }
    return {};
}

ApplicationTarget ApplicationDetector::DetectSuggested() {
    const auto local = LocalAppData();
    if (!local) return {};
    return DetectDiscordFromRoot(*local / L"Discord");
}

ApplicationTarget ApplicationDetector::DetectDiscordFromRoot(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return {};

    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        const auto name = it->path().filename().wstring();
        if (name.rfind(L"app-", 0) == 0 && std::filesystem::is_regular_file(it->path() / L"Discord.exe", ec)) {
            candidates.push_back(it->path());
        }
    }
    if (candidates.empty()) return {};
    std::sort(candidates.begin(), candidates.end(), NewerVersion);
    const auto app = candidates.front();
    return {root, app / L"Discord.exe", root / L"Update.exe", app.filename().wstring().substr(4), L"Discord"};
}

ApplicationTarget ApplicationDetector::FromExecutable(const std::filesystem::path& executable) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(executable, ec) || _wcsicmp(executable.extension().c_str(), L".exe") != 0) return {};
    ApplicationTarget result;
    result.executablePath = std::filesystem::absolute(executable, ec);
    const auto app = result.executablePath.parent_path();
    result.rootDirectory = app;
    result.displayName = result.executablePath.stem().wstring();
    const auto folder = app.filename().wstring();
    if (_wcsicmp(result.executablePath.filename().c_str(), L"Discord.exe") == 0 && folder.rfind(L"app-", 0) == 0) {
        result.rootDirectory = app.parent_path();
        result.updateExecutable = result.rootDirectory / L"Update.exe";
        result.version = folder.substr(4);
        result.displayName = L"Discord";
    }
    return result;
}
