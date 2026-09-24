#include "core/IconConverter.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <Wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <fstream>

using Microsoft::WRL::ComPtr;

namespace {
std::wstring HrMessage(HRESULT hr) {
    wchar_t* text = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring result = text ? text : L"Unknown Windows imaging error";
    if (text) LocalFree(text);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
    return result;
}

bool CreateFactory(ComPtr<IWICImagingFactory>& factory, std::wstring& error) {
    const HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { error = HrMessage(hr); return false; }
    return true;
}

bool ScaleAndConvert(IWICImagingFactory* factory, IWICBitmapSource* source, UINT width, UINT height,
                     ComPtr<IWICBitmapSource>& converted, std::wstring& error);

bool OpenFrame(IWICImagingFactory* factory, const std::filesystem::path& source,
               ComPtr<IWICBitmapFrameDecode>& frame, std::wstring& error) {
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(source.c_str(), nullptr, GENERIC_READ,
                                                     WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { error = L"Could not decode the selected image: " + HrMessage(hr); return false; }
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { error = L"Could not read the first image frame: " + HrMessage(hr); return false; }
    return true;
}

bool OpenMemoryFrame(IWICImagingFactory* factory, const uint8_t* data, size_t dataSize,
                     ComPtr<IWICBitmapFrameDecode>& frame, std::wstring& error) {
    if (!data || dataSize == 0 || dataSize > UINT_MAX) { error = L"Embedded image data is invalid."; return false; }
    ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(const_cast<BYTE*>(data), static_cast<DWORD>(dataSize));
    ComPtr<IWICBitmapDecoder> decoder;
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { error = L"Could not decode the embedded logo: " + HrMessage(hr); return false; }
    return true;
}

bool DecodeFrameForPreview(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame, uint32_t maxDimension,
                           DecodedImage& output, std::wstring& error) {
    UINT originalWidth = 0, originalHeight = 0;
    HRESULT hr = frame->GetSize(&originalWidth, &originalHeight);
    if (FAILED(hr) || originalWidth == 0 || originalHeight == 0) { error = L"The image has invalid dimensions."; return false; }
    const double scale = std::min(1.0, static_cast<double>(maxDimension) / std::max(originalWidth, originalHeight));
    const UINT width = std::max(1u, static_cast<UINT>(originalWidth * scale));
    const UINT height = std::max(1u, static_cast<UINT>(originalHeight * scale));
    ComPtr<IWICBitmapSource> converted;
    if (!ScaleAndConvert(factory, frame, width, height, converted, error)) return false;
    output.width = width;
    output.height = height;
    output.bgraPixels.resize(static_cast<size_t>(width) * height * 4);
    hr = converted->CopyPixels(nullptr, width * 4, static_cast<UINT>(output.bgraPixels.size()), output.bgraPixels.data());
    if (FAILED(hr)) { error = L"Could not copy preview pixels: " + HrMessage(hr); output = {}; return false; }
    return true;
}

bool ScaleAndConvert(IWICImagingFactory* factory, IWICBitmapSource* source, UINT width, UINT height,
                     ComPtr<IWICBitmapSource>& converted, std::wstring& error) {
    ComPtr<IWICBitmapScaler> scaler;
    HRESULT hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) { error = HrMessage(hr); return false; }
    hr = scaler->Initialize(source, width, height, WICBitmapInterpolationModeFant);
    if (FAILED(hr)) { error = L"Could not resize image: " + HrMessage(hr); return false; }
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { error = HrMessage(hr); return false; }
    hr = converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { error = L"Could not preserve image transparency: " + HrMessage(hr); return false; }
    converted = converter;
    return true;
}

bool EncodePng(IWICImagingFactory* factory, IWICBitmapSource* bitmap, std::vector<uint8_t>& png, std::wstring& error) {
    ComPtr<IStream> stream;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    if (FAILED(hr)) { error = HrMessage(hr); return false; }
    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    ComPtr<IWICBitmapFrameEncode> frame;
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) hr = bitmap->GetSize(&width, &height);
    if (SUCCEEDED(hr)) hr = frame->SetSize(width, height);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    if (SUCCEEDED(hr)) hr = frame->WriteSource(bitmap, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    if (FAILED(hr)) { error = L"Could not encode an icon frame: " + HrMessage(hr); return false; }
    STATSTG statistics{};
    hr = stream->Stat(&statistics, STATFLAG_NONAME);
    if (FAILED(hr) || statistics.cbSize.QuadPart <= 0 || statistics.cbSize.QuadPart > UINT32_MAX) {
        error = L"The encoded icon frame has an invalid size.";
        return false;
    }
    HGLOBAL memory = nullptr;
    hr = GetHGlobalFromStream(stream.Get(), &memory);
    if (FAILED(hr) || !memory) { error = HrMessage(hr); return false; }
    const SIZE_T size = static_cast<SIZE_T>(statistics.cbSize.QuadPart);
    const void* data = GlobalLock(memory);
    if (!data) { error = L"Could not access encoded icon memory."; return false; }
    png.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    GlobalUnlock(memory);
    return true;
}

bool CreateSquareFrame(IWICImagingFactory* factory, IWICBitmapSource* source, UINT size,
                       ComPtr<IWICBitmapSource>& square, std::wstring& error) {
    UINT originalWidth = 0, originalHeight = 0;
    HRESULT hr = source->GetSize(&originalWidth, &originalHeight);
    if (FAILED(hr) || originalWidth == 0 || originalHeight == 0) {
        error = L"The source image has invalid dimensions.";
        return false;
    }
    const double scale = std::min(static_cast<double>(size) / originalWidth,
                                  static_cast<double>(size) / originalHeight);
    const UINT width = std::max(1u, static_cast<UINT>(originalWidth * scale + 0.5));
    const UINT height = std::max(1u, static_cast<UINT>(originalHeight * scale + 0.5));
    ComPtr<IWICBitmapSource> converted;
    if (!ScaleAndConvert(factory, source, width, height, converted, error)) return false;
    std::vector<uint8_t> scaled(static_cast<size_t>(width) * height * 4);
    hr = converted->CopyPixels(nullptr, width * 4, static_cast<UINT>(scaled.size()), scaled.data());
    if (FAILED(hr)) { error = L"Could not read a resized icon frame: " + HrMessage(hr); return false; }
    std::vector<uint8_t> canvas(static_cast<size_t>(size) * size * 4, 0);
    const UINT offsetX = (size - width) / 2;
    const UINT offsetY = (size - height) / 2;
    for (UINT y = 0; y < height; ++y) {
        const auto* input = scaled.data() + static_cast<size_t>(y) * width * 4;
        auto* output = canvas.data() + (static_cast<size_t>(y + offsetY) * size + offsetX) * 4;
        std::copy_n(input, static_cast<size_t>(width) * 4, output);
    }
    ComPtr<IWICBitmap> bitmap;
    hr = factory->CreateBitmapFromMemory(size, size, GUID_WICPixelFormat32bppBGRA, size * 4,
                                         static_cast<UINT>(canvas.size()), canvas.data(), &bitmap);
    if (FAILED(hr)) { error = L"Could not create a square icon frame: " + HrMessage(hr); return false; }
    square = bitmap;
    return true;
}

#pragma pack(push, 1)
struct IconHeader { uint16_t reserved; uint16_t type; uint16_t count; };
struct IconEntry {
    uint8_t width; uint8_t height; uint8_t colorCount; uint8_t reserved;
    uint16_t planes; uint16_t bitCount; uint32_t bytesInResource; uint32_t imageOffset;
};
#pragma pack(pop)
}

bool IconConverter::DecodeForPreview(const std::filesystem::path& source, uint32_t maxDimension,
                                     DecodedImage& output, std::wstring& error) {
    output = {};
    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory, error)) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (!OpenFrame(factory.Get(), source, frame, error)) return false;
    return DecodeFrameForPreview(factory.Get(), frame.Get(), maxDimension, output, error);
}

bool IconConverter::DecodeMemoryForPreview(const uint8_t* data, size_t dataSize, uint32_t maxDimension,
                                           DecodedImage& output, std::wstring& error) {
    output = {};
    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory, error)) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (!OpenMemoryFrame(factory.Get(), data, dataSize, frame, error)) return false;
    return DecodeFrameForPreview(factory.Get(), frame.Get(), maxDimension, output, error);
}

bool IconConverter::CreateMultiResolutionIco(const std::filesystem::path& source,
                                             const std::filesystem::path& destination,
                                             std::wstring& error) {
    constexpr std::array<UINT, 7> sizes{16, 24, 32, 48, 64, 128, 256};
    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory, error)) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (!OpenFrame(factory.Get(), source, frame, error)) return false;
    std::vector<std::vector<uint8_t>> frames;
    frames.reserve(sizes.size());
    for (const UINT size : sizes) {
        ComPtr<IWICBitmapSource> converted;
        if (!CreateSquareFrame(factory.Get(), frame.Get(), size, converted, error)) return false;
        std::vector<uint8_t> png;
        if (!EncodePng(factory.Get(), converted.Get(), png, error)) return false;
        frames.push_back(std::move(png));
    }
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) { error = L"Could not create the icon directory: " + destination.parent_path().wstring(); return false; }
    const auto temporary = destination.wstring() + L".tmp";
    std::ofstream file(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
    if (!file) { error = L"Could not create the icon file."; return false; }
    const IconHeader header{0, 1, static_cast<uint16_t>(sizes.size())};
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    uint32_t offset = sizeof(IconHeader) + static_cast<uint32_t>(sizeof(IconEntry) * sizes.size());
    for (size_t i = 0; i < sizes.size(); ++i) {
        const IconEntry entry{static_cast<uint8_t>(sizes[i] == 256 ? 0 : sizes[i]),
                              static_cast<uint8_t>(sizes[i] == 256 ? 0 : sizes[i]), 0, 0, 1, 32,
                              static_cast<uint32_t>(frames[i].size()), offset};
        file.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
        offset += entry.bytesInResource;
    }
    for (const auto& bytes : frames) file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) { std::filesystem::remove(temporary, ec); error = L"Writing the icon file failed."; return false; }
    std::filesystem::rename(temporary, destination, ec);
    if (ec) {
        std::filesystem::remove(destination, ec);
        ec.clear();
        std::filesystem::rename(temporary, destination, ec);
    }
    if (ec) { error = L"Could not replace the previous icon file."; return false; }
    return true;
}
