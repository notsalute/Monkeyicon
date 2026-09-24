#include "core/ShortcutManager.h"
#include "platform/WindowsUtils.h"

#include <Windows.h>
#include <ShObjIdl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include <cstdint>
#include <cwctype>

using Microsoft::WRL::ComPtr;

namespace {
std::wstring SafeName(std::wstring value) {
    constexpr wchar_t invalid[] = L"<>:\"/\\|?*";
    for (auto& character : value) if (wcschr(invalid, character)) character = L'_';
    while (!value.empty() && (value.back() == L'.' || value.back() == L' ')) value.pop_back();
    return value.empty() ? L"Application" : value;
}

std::wstring AppIdentity(const ApplicationTarget& application) {
    const std::wstring value = application.executablePath.wstring();
    uint64_t hash = 1469598103934665603ull;
    for (const wchar_t character : value) {
        hash ^= static_cast<uint64_t>(towlower(character));
        hash *= 1099511628211ull;
    }
    wchar_t suffix[17]{};
    swprintf_s(suffix, L"%016llX", static_cast<unsigned long long>(hash));
    return L"Monkeyicon.App." + std::wstring(suffix);
}

std::filesystem::path CurrentExecutable() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return path;
}

std::wstring QuoteArgument(const std::filesystem::path& path) {
    std::wstring value = path.wstring();
    size_t position = 0;
    while ((position = value.find(L'"', position)) != std::wstring::npos) {
        value.insert(position, 1, L'\\');
        position += 2;
    }
    return L"\"" + value + L"\"";
}

bool BackupIfPresent(const std::filesystem::path& path, std::wstring& error) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;
    const auto backup = path.wstring() + L".backup";
    if (std::filesystem::exists(backup, ec)) return true;
    std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) { error = L"Could not back up the existing shortcut: " + path.wstring(); return false; }
    return true;
}

bool CreateShortcut(const std::filesystem::path& shortcutPath, const ApplicationTarget& installation,
                    const std::filesystem::path& iconPath, std::wstring& error) {
    ComPtr<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr)) { error = L"Windows Shell Link service is unavailable."; return false; }
    const auto target = CurrentExecutable();
    if (target.empty()) { error = L"Could not locate Monkeyicon.exe."; return false; }
    const std::wstring arguments = L"--launch-app --target-exe " + QuoteArgument(installation.executablePath) +
                                   L" --app-root " + QuoteArgument(installation.rootDirectory) +
                                   L" --app-id \"" + AppIdentity(installation) + L"\"" +
                                   L" --icon " + QuoteArgument(iconPath);
    hr = link->SetPath(target.c_str());
    if (SUCCEEDED(hr)) hr = link->SetArguments(arguments.c_str());
    if (SUCCEEDED(hr)) hr = link->SetWorkingDirectory(target.parent_path().c_str());
    if (SUCCEEDED(hr)) hr = link->SetIconLocation(iconPath.c_str(), 0);
    if (SUCCEEDED(hr)) hr = link->SetDescription(L"Launch an application with its Monkeyicon custom icon");
    if (FAILED(hr)) { error = L"Could not configure the application shortcut."; return false; }

    ComPtr<IPropertyStore> properties;
    if (SUCCEEDED(link.As(&properties))) {
        PROPVARIANT value;
        const std::wstring identity = AppIdentity(installation);
        if (SUCCEEDED(InitPropVariantFromString(identity.c_str(), &value))) {
            properties->SetValue(PKEY_AppUserModel_ID, value);
            properties->Commit();
            PropVariantClear(&value);
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(shortcutPath.parent_path(), ec);
    if (ec) { error = L"Could not create the shortcut directory."; return false; }
    ComPtr<IPersistFile> persist;
    hr = link.As(&persist);
    if (SUCCEEDED(hr)) hr = persist->Save(shortcutPath.c_str(), TRUE);
    if (FAILED(hr)) { error = L"Unable to create shortcut (0x" + std::to_wstring(static_cast<unsigned long>(hr)) + L")."; return false; }
    return true;
}

void RestoreBackupOrRemove(const std::filesystem::path& path, std::error_code& ec) {
    const std::filesystem::path backup(path.wstring() + L".backup");
    if (std::filesystem::exists(backup, ec)) {
        std::filesystem::copy_file(backup, path, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) std::filesystem::remove(backup, ec);
    } else {
        ec.clear();
        std::filesystem::remove(path, ec);
    }
}
}

std::filesystem::path ShortcutManager::CustomShortcutPath(const ApplicationTarget& application) {
    return WindowsUtils::StartMenuProgramsDirectory() / L"Monkeyicon" /
           (SafeName(application.displayName) + L" (Monkeyicon).lnk");
}
std::filesystem::path ShortcutManager::StartupShortcutPath(const ApplicationTarget& application) {
    return WindowsUtils::StartupDirectory() / (SafeName(application.displayName) + L" (Monkeyicon).lnk");
}
std::filesystem::path ShortcutManager::IconStoragePath(const ApplicationTarget& application) {
    const std::wstring identity = AppIdentity(application);
    const auto suffix = identity.substr(identity.find_last_of(L'.') + 1);
    return WindowsUtils::AppDataDirectory() / L"icons" / (SafeName(application.displayName) + L"-" + suffix + L".ico");
}

ShortcutResult ShortcutManager::Apply(const ApplicationTarget& installation,
                                      const std::filesystem::path& iconPath, bool enableStartup) {
    if (!installation.IsValid()) return {false, L"Target application not found.", {}};
    std::error_code ec;
    if (!std::filesystem::is_regular_file(iconPath, ec)) return {false, L"Generated icon file not found.", {}};
    const auto shortcut = CustomShortcutPath(installation);
    std::wstring error;
    if (!BackupIfPresent(shortcut, error)) return {false, error, {}};
    if (!CreateShortcut(shortcut, installation, iconPath, error)) return {false, error, {}};
    const auto startup = StartupShortcutPath(installation);
    if (enableStartup) {
        if (!BackupIfPresent(startup, error) || !CreateShortcut(startup, installation, iconPath, error)) return {false, error, shortcut};
    } else {
        RestoreBackupOrRemove(startup, ec);
    }
    return {true, L"Custom application shortcut created.", shortcut};
}

ShortcutResult ShortcutManager::Restore(const ApplicationTarget& application) {
    if (!application.IsValid()) return {false, L"Choose the application to restore first.", {}};
    std::error_code ec;
    RestoreBackupOrRemove(CustomShortcutPath(application), ec);
    if (ec) return {false, L"Unable to restore the previous Start menu shortcut.", {}};
    RestoreBackupOrRemove(StartupShortcutPath(application), ec);
    if (ec) return {false, L"Unable to restore the previous startup shortcut.", {}};
    return {true, L"Custom launcher removed; application data was not changed.", {}};
}
