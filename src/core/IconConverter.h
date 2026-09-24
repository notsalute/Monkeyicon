#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct DecodedImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgraPixels;
};

class IconConverter {
public:
    [[nodiscard]] static bool DecodeForPreview(const std::filesystem::path& source, uint32_t maxDimension,
                                               DecodedImage& output, std::wstring& error);
    [[nodiscard]] static bool DecodeMemoryForPreview(const uint8_t* data, size_t dataSize, uint32_t maxDimension,
                                                     DecodedImage& output, std::wstring& error);
    [[nodiscard]] static bool CreateMultiResolutionIco(const std::filesystem::path& source,
                                                       const std::filesystem::path& destination,
                                                       std::wstring& error);
};
