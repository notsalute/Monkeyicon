#include "core/IconPatcher.h"

#include <Windows.h>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 64;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 65;
    const auto patched = IconPatcher::Patch(argv[1], argv[2]);
    std::wcout << L"patch: " << patched.message << L'\n';
    if (!patched.success) { CoUninitialize(); return 1; }
    const auto restored = IconPatcher::Restore(argv[1]);
    std::wcout << L"restore: " << restored.message << L'\n';
    CoUninitialize();
    return restored.success ? 0 : 2;
}
