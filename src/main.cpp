#include "ui/UI.h"
#include "core/RuntimeIconManager.h"
#include "resource.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <Windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstdlib>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace {
ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_renderTarget;

HICON CreateBrandIcon() {
    constexpr int size = 64;
    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = size;
    header.bV5Height = -size;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;
    void* rawPixels = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS,
                                     &rawPixels, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !rawPixels) return nullptr;
    auto* pixels = static_cast<uint32_t*>(rawPixels);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int cx = x < 12 ? 12 : (x > 51 ? 51 : x);
            const int cy = y < 12 ? 12 : (y > 51 ? 51 : y);
            const bool inside = (x - cx) * (x - cx) + (y - cy) * (y - cy) <= 144;
            if (!inside) { pixels[y * size + x] = 0; continue; }
            const uint8_t r = static_cast<uint8_t>(118 - y * 35 / size);
            const uint8_t g = static_cast<uint8_t>(96 + x * 35 / size);
            pixels[y * size + x] = 0xFF000000u | (r << 16) | (g << 8) | 255u;
            const int d1 = (x - 23) * (x - 23) + (y - 27) * (y - 27);
            const int d2 = (x - 41) * (x - 41) + (y - 37) * (y - 37);
            const bool ring = (d1 >= 45 && d1 <= 82) || (d2 >= 45 && d2 <= 82);
            const bool bridge = x >= 27 && x <= 37 && y >= 28 && y <= 36 && std::abs((y - 32) - (x - 32) / 2) <= 2;
            if (ring || bridge) pixels[y * size + x] = 0xFFFFFFFFu;
        }
    }
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO info{TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

void CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_renderTarget);
    }
}

void CleanupRenderTarget() { g_renderTarget.Reset(); }

bool CreateDevice(HWND window) {
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected{};
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                               levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &description,
                                               &g_swapChain, &g_device, &selected, &g_context);
#ifdef _DEBUG
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                           levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &description,
                                           &g_swapChain, &g_device, &selected, &g_context);
    }
#endif
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return g_renderTarget != nullptr;
}

}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return true;
    switch (message) {
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(window, message, wParam, lParam);
        if (hit != HTCLIENT) return hit;
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(window, &point);
        RECT bounds{};
        GetClientRect(window, &bounds);
        if (point.y < 50 && point.x < bounds.right - 110) return HTCAPTION;
        constexpr int border = 6;
        const bool left = point.x < border, right = point.x >= bounds.right - border;
        const bool top = point.y < border, bottom = point.y >= bounds.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (bottom) return HTBOTTOM;
        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {820, 560};
        return 0;
    }
    case WM_SIZE:
        if (g_device && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    int launcherExitCode = 0;
    if (RuntimeIconManager::HandleLauncherCommandLine(launcherExitCode)) {
        CoUninitialize();
        return launcherExitCode;
    }

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    HICON brandIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_MONKEYICON), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR));
    if (!brandIcon) brandIcon = CreateBrandIcon();
    HICON smallBrandIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_MONKEYICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    if (!smallBrandIcon) smallBrandIcon = brandIcon;
    windowClass.hIcon = brandIcon;
    windowClass.hIconSm = smallBrandIcon;
    windowClass.lpszClassName = L"MonkeyiconWindow";
    RegisterClassExW(&windowClass);

    const int width = 900, height = 640;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, windowClass.lpszClassName, L"Monkeyicon",
                                  WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX, x, y, width, height,
                                  nullptr, nullptr, instance, nullptr);
    if (!window || !CreateDevice(window)) {
        MessageBoxW(nullptr, L"DirectX 11 initialization failed.", L"Monkeyicon", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());
    UI ui(window, g_device.Get());
    ui.Initialize();

    bool running = true;
    while (running) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) running = false;
        }
        if (!running) break;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ui.Render();
        ImGui::Render();
        constexpr float clearColor[]{0.071f, 0.055f, 0.043f, 1.0f};
        g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
        g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupRenderTarget();
    g_swapChain.Reset();
    g_context.Reset();
    g_device.Reset();
    DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName, instance);
    if (smallBrandIcon && smallBrandIcon != brandIcon) DestroyIcon(smallBrandIcon);
    if (brandIcon) DestroyIcon(brandIcon);
    CoUninitialize();
    return 0;
}
