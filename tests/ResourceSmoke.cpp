#include <Windows.h>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 64;
    HMODULE module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module) return 1;
    const HRSRC logo = FindResourceW(module, MAKEINTRESOURCEW(201), RT_RCDATA);
    const HRSRC icon = FindResourceW(module, MAKEINTRESOURCEW(101), RT_GROUP_ICON);
    const DWORD logoBytes = logo ? SizeofResource(module, logo) : 0;
    FreeLibrary(module);
    std::wcout << L"embedded_logo_bytes=" << logoBytes << L" embedded_icon=" << (icon != nullptr) << L'\n';
    return logoBytes > 0 && icon ? 0 : 2;
}
