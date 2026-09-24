#include "platform/WindowsUtils.h"

#include <Windows.h>
#include <ShlObj.h>
#include <ShObjIdl.h>
#include <shellapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
std::filesystem::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &path))) return {};
    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

std::wstring BrowseFile(HWND owner, const COMDLG_FILTERSPEC* filters, UINT filterCount, const wchar_t* title) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    dialog->SetTitle(title);
    dialog->SetFileTypes(filterCount, filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    if (FAILED(dialog->Show(owner))) return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return {};
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return {};
    std::wstring result(path);
    CoTaskMemFree(path);
    return result;
}
}

std::filesystem::path WindowsUtils::AppDataDirectory() { return KnownFolder(FOLDERID_LocalAppData) / L"Monkeyicon"; }
std::filesystem::path WindowsUtils::StartMenuProgramsDirectory() { return KnownFolder(FOLDERID_Programs); }
std::filesystem::path WindowsUtils::StartupDirectory() { return KnownFolder(FOLDERID_Startup); }
std::filesystem::path WindowsUtils::DesktopDirectory() { return KnownFolder(FOLDERID_Desktop); }

std::wstring WindowsUtils::BrowseForImage(void* ownerWindow) {
    const COMDLG_FILTERSPEC filters[]{{L"Supported images", L"*.png;*.jpg;*.jpeg;*.bmp;*.ico"},
                                      {L"PNG image", L"*.png"}, {L"JPEG image", L"*.jpg;*.jpeg"},
                                      {L"Bitmap image", L"*.bmp"}, {L"Windows icon", L"*.ico"}};
    return BrowseFile(static_cast<HWND>(ownerWindow), filters, ARRAYSIZE(filters), L"Choose a custom application icon");
}

std::wstring WindowsUtils::BrowseForApplicationExecutable(void* ownerWindow) {
    const COMDLG_FILTERSPEC filters[]{{L"Windows application", L"*.exe"}, {L"All files", L"*.*"}};
    return BrowseFile(static_cast<HWND>(ownerWindow), filters, ARRAYSIZE(filters), L"Choose a Windows application");
}

std::string WindowsUtils::ToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring WindowsUtils::ErrorMessage(unsigned long errorCode) {
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, errorCode, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring result = buffer ? buffer : L"Unknown Windows error";
    if (buffer) LocalFree(buffer);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
    return result;
}

bool WindowsUtils::RefreshIconCache(std::wstring& error) {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    const auto system = KnownFolder(FOLDERID_System);
    const auto helper = system / L"ie4uinit.exe";
    std::error_code ec;
    if (std::filesystem::is_regular_file(helper, ec)) {
        SHELLEXECUTEINFOW info{sizeof(info)};
        info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        info.lpFile = helper.c_str();
        info.lpParameters = L"-show";
        info.nShow = SW_HIDE;
        if (ShellExecuteExW(&info) && info.hProcess) {
            WaitForSingleObject(info.hProcess, 3000);
            CloseHandle(info.hProcess);
        }
    }
    error.clear();
    return true;
}

void WindowsUtils::OpenFolderAndSelect(const std::filesystem::path& item) {
    PIDLIST_ABSOLUTE list = ILCreateFromPathW(item.c_str());
    if (!list) return;
    SHOpenFolderAndSelectItems(list, 0, nullptr, 0);
    ILFree(list);
}
