#include "core/RuntimeIconManager.h"

#include "core/ApplicationTarget.h"

#include <Windows.h>
#include <Psapi.h>
#include <ShObjIdl.h>
#include <TlHelp32.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
struct LauncherOptions {
    std::filesystem::path appRoot;
    std::filesystem::path targetExecutable;
    std::filesystem::path iconPath;
    std::wstring appId;
};

bool SamePath(const std::filesystem::path& candidate, const std::filesystem::path& target) {
    std::error_code ec;
    auto left = std::filesystem::weakly_canonical(candidate, ec).wstring();
    ec.clear();
    auto right = std::filesystem::weakly_canonical(target, ec).wstring();
    if (left.empty() || right.empty()) return false;
    std::transform(left.begin(), left.end(), left.begin(), towlower);
    std::transform(right.begin(), right.end(), right.begin(), towlower);
    return left == right;
}

bool ParseOptions(LauncherOptions& options) {
    int count = 0;
    wchar_t** values = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!values) return false;
    const bool launcherMode = count >= 2 && _wcsicmp(values[1], L"--launch-app") == 0;
    if (launcherMode) {
        for (int i = 2; i + 1 < count; ++i) {
            if (_wcsicmp(values[i], L"--app-root") == 0) options.appRoot = values[++i];
            else if (_wcsicmp(values[i], L"--target-exe") == 0) options.targetExecutable = values[++i];
            else if (_wcsicmp(values[i], L"--app-id") == 0) options.appId = values[++i];
            else if (_wcsicmp(values[i], L"--icon") == 0) options.iconPath = values[++i];
        }
    }
    LocalFree(values);
    return launcherMode;
}

std::vector<DWORD> FindTargetProcesses(const std::filesystem::path& executable) {
    std::vector<DWORD> result;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32W entry{sizeof(entry)};
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, executable.filename().c_str()) != 0) continue;
            const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) continue;
            std::wstring path(32768, L'\0');
            DWORD length = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &length)) {
                path.resize(length);
                if (SamePath(path, executable)) result.push_back(entry.th32ProcessID);
            }
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

struct WindowUpdateContext {
    const std::vector<DWORD>* processIds;
    HICON largeIcon;
    HICON smallIcon;
    const std::wstring* appId;
    bool changed = false;
};

BOOL CALLBACK UpdateWindowIcon(HWND window, LPARAM value) {
    auto& context = *reinterpret_cast<WindowUpdateContext*>(value);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (std::find(context.processIds->begin(), context.processIds->end(), processId) == context.processIds->end()) return TRUE;
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) return TRUE;

    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(context.largeIcon),
                        SMTO_ABORTIFHUNG, 1000, &ignored);
    SendMessageTimeoutW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(context.smallIcon),
                        SMTO_ABORTIFHUNG, 1000, &ignored);

    ComPtr<IPropertyStore> properties;
    if (!context.appId->empty() && SUCCEEDED(SHGetPropertyStoreForWindow(window, IID_PPV_ARGS(&properties)))) {
        PROPVARIANT appId;
        if (SUCCEEDED(InitPropVariantFromString(context.appId->c_str(), &appId))) {
            if (SUCCEEDED(properties->SetValue(PKEY_AppUserModel_ID, appId))) properties->Commit();
            PropVariantClear(&appId);
        }
    }
    context.changed = true;
    return TRUE;
}

bool StartApplication(const ApplicationTarget& installation) {
    const auto target = installation.LaunchTarget();
    const auto arguments = installation.LaunchArguments();
    const auto workingDirectory = installation.WorkingDirectory();
    SHELLEXECUTEINFOW launch{sizeof(launch)};
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    launch.lpFile = target.c_str();
    launch.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    launch.lpDirectory = workingDirectory.c_str();
    launch.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&launch)) return false;
    if (launch.hProcess) CloseHandle(launch.hProcess);
    return true;
}
}

bool RuntimeIconManager::HandleLauncherCommandLine(int& exitCode) {
    LauncherOptions options;
    if (!ParseOptions(options)) return false;
    exitCode = 1;
    std::error_code ec;
    if (options.iconPath.empty() || !std::filesystem::is_regular_file(options.iconPath, ec)) return true;

    ApplicationTarget installation = ApplicationDetector::FromExecutable(options.targetExecutable);
    if (!installation.IsValid() && _wcsicmp(options.targetExecutable.filename().c_str(), L"Discord.exe") == 0)
        installation = ApplicationDetector::DetectDiscordFromRoot(options.appRoot);
    if (!installation.IsValid() || !StartApplication(installation)) return true;

    HICON largeIcon = static_cast<HICON>(LoadImageW(nullptr, options.iconPath.c_str(), IMAGE_ICON, 64, 64, LR_LOADFROMFILE));
    HICON smallIcon = static_cast<HICON>(LoadImageW(nullptr, options.iconPath.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
    if (!largeIcon || !smallIcon) {
        if (largeIcon) DestroyIcon(largeIcon);
        if (smallIcon) DestroyIcon(smallIcon);
        return true;
    }

    bool changed = false;
    bool sawApplication = false;
    int emptyChecks = 0;
    int startupChecks = 0;
    for (;;) {
        const auto processes = FindTargetProcesses(installation.executablePath);
        if (processes.empty()) {
            ++emptyChecks;
            ++startupChecks;
            if ((sawApplication && emptyChecks >= 10) || (!sawApplication && startupChecks >= 60)) break;
        } else {
            sawApplication = true;
            emptyChecks = 0;
            WindowUpdateContext context{&processes, largeIcon, smallIcon, &options.appId};
            EnumWindows(UpdateWindowIcon, reinterpret_cast<LPARAM>(&context));
            changed = changed || context.changed;
        }
        Sleep(1000);
    }
    DestroyIcon(largeIcon);
    DestroyIcon(smallIcon);
    exitCode = changed ? 0 : 2;
    return true;
}
