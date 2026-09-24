#pragma once

#include "core/ApplicationTarget.h"

#include <d3d11.h>
#include <filesystem>
#include <string>
#include <vector>

struct ImFont;
struct DecodedImage;

struct LogEntry {
    bool success;
    std::string text;
};

class UI {
public:
    UI(HWND window, ID3D11Device* device);
    ~UI();
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void Initialize();
    void Render();

private:
    void DetectSuggestedApplication();
    void SelectImage();
    void BrowseApplication();
    void ApplyIcon();
    void PerformApply();
    void RestoreDefault();
    void RefreshIconCache();
    void LoadPreview(const std::filesystem::path& path);
    bool UploadTexture(const DecodedImage& image, ID3D11ShaderResourceView*& destination);
    void AddLog(bool success, std::string message);
    void ShowToast(bool success, std::string message);
    void DrawTitleBar();
    void DrawHeader();
    void DrawStepper();
    void DrawApplyPanel();
    void DrawWizard();
    void DrawActivityLog();
    void DrawToasts();

    HWND window_{};
    ID3D11Device* device_{};
    ID3D11ShaderResourceView* previewTexture_{};
    ID3D11ShaderResourceView* logoTexture_{};
    ImFont* titleFont_{};
    ImFont* headingFont_{};
    ImFont* bodyFont_{};
    int previewWidth_ = 0;
    int previewHeight_ = 0;
    ApplicationTarget installation_;
    std::filesystem::path selectedImage_;
    std::filesystem::path generatedIcon_;
    std::filesystem::path shortcutPath_;
    std::vector<LogEntry> logs_;
    bool launchAtStartup_ = false;
    bool patchExecutable_ = false;
    bool showActivityLog_ = false;
    bool showInstructions_ = true;
    bool confirmRefresh_ = false;
    bool confirmPatch_ = false;
    float headerFade_ = 0.0f;
    float propellerSpin_ = 0.0f;
    int currentStep_ = 0;
    struct Toast { bool success; std::string message; double created; };
    std::vector<Toast> toasts_;
};
