#include "core/IconPatcher.h"
#include "platform/WindowsUtils.h"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <set>
#include <vector>

namespace {
#pragma pack(push, 1)
struct IconHeader { uint16_t reserved; uint16_t type; uint16_t count; };
struct IconEntry {
    uint8_t width; uint8_t height; uint8_t colorCount; uint8_t reserved;
    uint16_t planes; uint16_t bitCount; uint32_t bytesInResource; uint32_t imageOffset;
};
struct GroupIconEntry {
    uint8_t width; uint8_t height; uint8_t colorCount; uint8_t reserved;
    uint16_t planes; uint16_t bitCount; uint32_t bytesInResource; uint16_t resourceId;
};
#pragma pack(pop)

struct ResourceName {
    bool integer = true;
    WORD id = 0;
    std::wstring text;
    LPCWSTR Value() const { return integer ? MAKEINTRESOURCEW(id) : text.c_str(); }
};

struct GroupResource {
    ResourceName name;
    std::vector<WORD> languages;
};

BOOL CALLBACK CollectLanguages(HMODULE, LPCWSTR, LPCWSTR, WORD language, LONG_PTR context) {
    reinterpret_cast<std::vector<WORD>*>(context)->push_back(language);
    return TRUE;
}

BOOL CALLBACK CollectGroups(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR context) {
    auto& groups = *reinterpret_cast<std::vector<GroupResource>*>(context);
    GroupResource group;
    if (IS_INTRESOURCE(name)) group.name.id = static_cast<WORD>(reinterpret_cast<ULONG_PTR>(name));
    else { group.name.integer = false; group.name.text = name; }
    EnumResourceLanguagesW(module, type, name, CollectLanguages,
                           reinterpret_cast<LONG_PTR>(&group.languages));
    if (group.languages.empty()) group.languages.push_back(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    groups.push_back(std::move(group));
    return TRUE;
}

bool ReadIcon(const std::filesystem::path& path, std::vector<uint8_t>& bytes,
              const IconHeader*& header, const IconEntry*& entries, std::wstring& error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) { error = L"Unable to open the generated icon."; return false; }
    const auto length = stream.tellg();
    if (length < static_cast<std::streamoff>(sizeof(IconHeader))) { error = L"The generated icon is invalid."; return false; }
    bytes.resize(static_cast<size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!stream) { error = L"Unable to read the generated icon."; return false; }
    header = reinterpret_cast<const IconHeader*>(bytes.data());
    if (header->reserved != 0 || header->type != 1 || header->count == 0 ||
        sizeof(IconHeader) + static_cast<size_t>(header->count) * sizeof(IconEntry) > bytes.size()) {
        error = L"The generated icon directory is invalid.";
        return false;
    }
    entries = reinterpret_cast<const IconEntry*>(bytes.data() + sizeof(IconHeader));
    for (uint16_t i = 0; i < header->count; ++i) {
        if (entries[i].imageOffset > bytes.size() || entries[i].bytesInResource > bytes.size() - entries[i].imageOffset) {
            error = L"An icon frame is outside the icon file.";
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> BuildGroupData(const IconHeader& header, const IconEntry* entries,
                                    const std::vector<WORD>& resourceIds) {
    std::vector<uint8_t> data(sizeof(IconHeader) + static_cast<size_t>(header.count) * sizeof(GroupIconEntry));
    *reinterpret_cast<IconHeader*>(data.data()) = header;
    auto* output = reinterpret_cast<GroupIconEntry*>(data.data() + sizeof(IconHeader));
    for (uint16_t i = 0; i < header.count; ++i) {
        output[i] = {entries[i].width, entries[i].height, entries[i].colorCount, entries[i].reserved,
                     entries[i].planes, entries[i].bitCount, entries[i].bytesInResource, resourceIds[i]};
    }
    return data;
}
}

std::filesystem::path IconPatcher::BackupPath(const std::filesystem::path& executable) {
    return std::filesystem::path(executable.wstring() + L".monkeyicon.bak");
}

IconPatchResult IconPatcher::Patch(const std::filesystem::path& executable,
                                   const std::filesystem::path& iconFile) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(executable, ec)) return {false, false, L"The target executable was not found."};
    std::vector<uint8_t> iconBytes;
    const IconHeader* header = nullptr;
    const IconEntry* entries = nullptr;
    std::wstring error;
    if (!ReadIcon(iconFile, iconBytes, header, entries, error)) return {false, false, error};

    const auto backup = BackupPath(executable);
    const bool alreadyBackedUp = std::filesystem::is_regular_file(backup, ec);
    if (!alreadyBackedUp) {
        ec.clear();
        std::filesystem::copy_file(executable, backup, std::filesystem::copy_options::none, ec);
        if (ec) return {false, false, L"Could not back up the executable: " + WindowsUtils::ErrorMessage(ec.value())};
    }

    HMODULE module = LoadLibraryExW(executable.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module) return {false, true, L"Unable to inspect executable resources. Fully quit the target app and try again."};
    std::vector<GroupResource> groups;
    EnumResourceNamesW(module, RT_GROUP_ICON, CollectGroups, reinterpret_cast<LONG_PTR>(&groups));
    FreeLibrary(module);
    if (groups.empty()) {
        GroupResource primary;
        primary.name.id = 1;
        primary.languages.push_back(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
        groups.push_back(std::move(primary));
    }

    HANDLE update = BeginUpdateResourceW(executable.c_str(), FALSE);
    if (!update) return {false, true, L"Unable to open the executable for modification. Fully quit the target app first. " + WindowsUtils::ErrorMessage(GetLastError())};
    bool ok = true;
    std::vector<WORD> resourceIds(header->count);
    for (uint16_t i = 0; i < header->count; ++i) resourceIds[i] = static_cast<WORD>(60000 + i);
    const auto groupData = BuildGroupData(*header, entries, resourceIds);
    std::set<WORD> writtenLanguages;
    for (const auto& group : groups) {
        for (const WORD language : group.languages) {
            if (writtenLanguages.insert(language).second) {
                for (uint16_t i = 0; i < header->count && ok; ++i) {
                    ok = UpdateResourceW(update, RT_ICON, MAKEINTRESOURCEW(resourceIds[i]), language,
                                         iconBytes.data() + entries[i].imageOffset,
                                         entries[i].bytesInResource) != FALSE;
                }
            }
            if (ok) ok = UpdateResourceW(update, RT_GROUP_ICON, group.name.Value(), language,
                                         const_cast<uint8_t*>(groupData.data()),
                                         static_cast<DWORD>(groupData.size())) != FALSE;
            if (!ok) break;
        }
        if (!ok) break;
    }
    if (!ok) {
        const DWORD code = GetLastError();
        EndUpdateResourceW(update, TRUE);
        return {false, true, L"Writing application icon resources failed: " + WindowsUtils::ErrorMessage(code)};
    }
    if (!EndUpdateResourceW(update, FALSE)) {
        return {false, true, L"Saving the patched executable failed: " + WindowsUtils::ErrorMessage(GetLastError())};
    }
    return {true, true, L"Executable icon resources patched. Its digital signature is now invalid until restored or updated."};
}

IconPatchResult IconPatcher::Restore(const std::filesystem::path& executable) {
    const auto backup = BackupPath(executable);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(backup, ec)) return {true, false, L"No Monkeyicon executable backup was present."};
    std::filesystem::copy_file(backup, executable, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return {false, true, L"Unable to restore the executable. Fully quit the target app first: " + WindowsUtils::ErrorMessage(ec.value())};
    std::filesystem::remove(backup, ec);
    return {true, true, L"Original executable restored from backup."};
}
