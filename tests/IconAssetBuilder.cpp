#include "core/IconConverter.h"

#include <Windows.h>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 64;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 65;
    std::wstring error;
    const bool success = IconConverter::CreateMultiResolutionIco(argv[1], argv[2], error);
    if (!success) std::wcerr << error << L'\n';
    CoUninitialize();
    return success ? 0 : 1;
}
