#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/UI.h"

#include "core/IconConverter.h"
#include "core/IconPatcher.h"
#include "core/ShortcutManager.h"
#include "platform/WindowsUtils.h"
#include "resource.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
constexpr ImVec4 kBg{0.071f, 0.055f, 0.043f, 1.0f};
constexpr ImVec4 kPanel{0.106f, 0.082f, 0.063f, 1.0f};
constexpr ImVec4 kRaised{0.145f, 0.114f, 0.086f, 1.0f};
constexpr ImVec4 kRaisedHover{0.196f, 0.153f, 0.114f, 1.0f};
constexpr ImVec4 kBorder{0.255f, 0.196f, 0.145f, 1.0f};
constexpr ImVec4 kText{0.965f, 0.933f, 0.875f, 1.0f};
constexpr ImVec4 kMuted{0.690f, 0.612f, 0.518f, 1.0f};
constexpr ImVec4 kAccent{1.0f, 0.788f, 0.235f, 1.0f};
constexpr ImVec4 kAccentHover{1.0f, 0.847f, 0.400f, 1.0f};
constexpr ImVec4 kAccentActive{0.902f, 0.682f, 0.122f, 1.0f};
constexpr ImVec4 kOnAccent{0.165f, 0.102f, 0.031f, 1.0f};
constexpr ImVec4 kSuccess{0.337f, 0.761f, 0.443f, 1.0f};
constexpr ImVec4 kError{0.941f, 0.322f, 0.310f, 1.0f};
constexpr ImVec4 kWarning{0.949f, 0.608f, 0.220f, 1.0f};
constexpr ImVec4 kCapColors[4]{
    {0.910f, 0.271f, 0.235f, 1.0f}, {0.969f, 0.765f, 0.192f, 1.0f},
    {0.231f, 0.667f, 0.361f, 1.0f}, {0.184f, 0.498f, 0.847f, 1.0f}};
constexpr float kPi = 3.14159265f;

ImFont* g_semibold = nullptr;

ImU32 Col(const ImVec4& c, float alpha = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alpha));
}

ImVec2 Measure(ImFont* font, float size, const char* text) { return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text); }

void TextMuted(const char* text) { ImGui::TextColored(kMuted, "%s", text); }

void HandCursorIfHovered() {
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

bool AccentButton(const char* label, ImVec2 size) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(p, p + size);
    auto* draw = ImGui::GetWindowDrawList();
    for (int i = 3; i >= 1; --i) {
        const float g = i * 3.0f;
        draw->AddRectFilled(ImVec2(p.x - g, p.y - g + 4), ImVec2(p.x + size.x + g, p.y + size.y + g + 4),
                            Col(kAccent, hovered ? 0.06f : 0.03f), 14.0f + g);
    }
    ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentActive);
    ImGui::PushStyleColor(ImGuiCol_Border, kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_Text, kOnAccent);
    if (g_semibold) ImGui::PushFont(g_semibold, 17.0f);
    const bool pressed = ImGui::Button(label, size);
    if (g_semibold) ImGui::PopFont();
    ImGui::PopStyleColor(5);
    HandCursorIfHovered();
    return pressed;
}

bool GhostButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
    const bool pressed = ImGui::Button(label, size);
    HandCursorIfHovered();
    return pressed;
}

std::string Ellipsize(const std::string& text, size_t length = 70) {
    if (text.size() <= length) return text;
    return text.substr(0, length / 2 - 2) + "..." + text.substr(text.size() - length / 2);
}

void DrawStatusDot(bool success) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float y = p.y + ImGui::GetTextLineHeight() * 0.5f;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 5, y), 7.0f, Col(success ? kSuccess : kError, 0.18f));
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 5, y), 4.0f, Col(success ? kSuccess : kError));
    ImGui::Dummy(ImVec2(14, ImGui::GetTextLineHeight()));
}

void DrawCheck(ImDrawList* draw, ImVec2 c, float s, ImU32 color, float thickness) {
    const ImVec2 points[3]{{c.x - s * 0.5f, c.y + s * 0.02f}, {c.x - s * 0.12f, c.y + s * 0.4f}, {c.x + s * 0.55f, c.y - s * 0.38f}};
    draw->AddPolyline(points, 3, color, 0, thickness);
}

void DrawCapRing(ImDrawList* draw, ImVec2 c, float r, float thickness, float spin) {
    constexpr float quarter = kPi * 0.5f;
    for (int i = 0; i < 4; ++i) {
        const float a0 = spin + quarter * i + 0.12f;
        draw->PathArcTo(c, r, a0, a0 + quarter - 0.24f, 16);
        draw->PathStroke(Col(kCapColors[i]), 0, thickness);
    }
}

void DrawCapStripe(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha = 1.0f) {
    const float w = (max.x - min.x) / 4.0f;
    for (int i = 0; i < 4; ++i)
        draw->AddRectFilled(ImVec2(min.x + w * i, min.y), ImVec2(min.x + w * (i + 1), max.y), Col(kCapColors[i], alpha));
}

void DrawLogoBadge(ImDrawList* draw, ImTextureID logo, ImVec2 c, float r, float spin) {
    draw->AddCircleFilled(c, r + 6.0f, Col(kAccent, 0.07f));
    draw->AddCircleFilled(c, r, Col(kRaised));
    if (logo) {
        const float inset = r - 2.0f;
        draw->AddImageRounded(logo, ImVec2(c.x - inset, c.y - inset), ImVec2(c.x + inset, c.y + inset),
                              ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, inset);
    }
    DrawCapRing(draw, c, r + 3.0f, std::max(2.0f, r * 0.09f), spin);
}

void DrawLeaf(ImDrawList* draw, ImVec2 base, float angle, float length, float width, ImU32 fill, ImU32 vein) {
    const ImVec2 dir(std::cos(angle), std::sin(angle));
    const ImVec2 normal(-dir.y, dir.x);
    const ImVec2 tip = base + dir * length;
    const ImVec2 mid = base + dir * (length * 0.45f);
    draw->PathLineTo(base);
    draw->PathBezierQuadraticCurveTo(mid + normal * width, tip, 12);
    draw->PathBezierQuadraticCurveTo(mid - normal * width, base, 12);
    draw->PathFillConvex(fill);
    draw->AddLine(base, base + dir * (length * 0.85f), vein, 1.2f);
}

void DrawBanana(ImDrawList* draw, ImVec2 c, float s) {
    const ImVec2 outer(c.x - s * 0.35f, c.y - s * 0.55f);
    const ImVec2 inner(outer.x - s * 0.18f, outer.y - s * 0.32f);
    const float a0 = kPi * 0.02f, a1 = kPi * 0.72f;
    draw->PathArcTo(outer, s, a0, a1, 20);
    draw->PathArcTo(inner, s * 0.98f, a1 - 0.08f, a0 + 0.08f, 20);
    draw->PathFillConcave(Col(kAccent));
    const ImVec2 tip(outer.x + std::cos(a0) * s, outer.y + std::sin(a0) * s);
    draw->AddLine(tip, ImVec2(tip.x + s * 0.12f, tip.y - s * 0.32f), IM_COL32(110, 78, 36, 255), std::max(2.0f, s * 0.18f));
    const ImVec2 end(outer.x + std::cos(a1) * s, outer.y + std::sin(a1) * s);
    draw->AddCircleFilled(end, std::max(1.2f, s * 0.08f), IM_COL32(110, 78, 36, 255));
}

void DrawAppGlyph(ImDrawList* draw, ImVec2 c, ImU32 color) {
    const ImVec2 min(c.x - 13, c.y - 11), max(c.x + 13, c.y + 11);
    draw->AddRect(min, max, color, 5.0f, 0, 2.0f);
    draw->AddLine(ImVec2(min.x + 1, min.y + 7), ImVec2(max.x - 1, min.y + 7), color, 2.0f);
    draw->AddCircleFilled(ImVec2(min.x + 5, min.y + 3.5f), 1.5f, color);
    draw->AddCircleFilled(ImVec2(min.x + 10, min.y + 3.5f), 1.5f, color);
}

void DrawDashedRect(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 color, float r, float thickness = 1.4f) {
    const auto dashed = [&](ImVec2 p0, ImVec2 p1) {
        const ImVec2 delta = p1 - p0;
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length <= 0.0f) return;
        const ImVec2 dir = delta / length;
        for (float s = 0.0f; s < length; s += 12.0f)
            draw->AddLine(p0 + dir * s, p0 + dir * std::min(s + 7.0f, length), color, thickness);
    };
    dashed(ImVec2(a.x + r, a.y), ImVec2(b.x - r, a.y));
    dashed(ImVec2(a.x + r, b.y), ImVec2(b.x - r, b.y));
    dashed(ImVec2(a.x, a.y + r), ImVec2(a.x, b.y - r));
    dashed(ImVec2(b.x, a.y + r), ImVec2(b.x, b.y - r));
    const ImVec2 corners[4]{{a.x + r, a.y + r}, {b.x - r, a.y + r}, {b.x - r, b.y - r}, {a.x + r, b.y - r}};
    for (int i = 0; i < 4; ++i) {
        const float start = kPi + i * kPi * 0.5f;
        draw->PathArcTo(corners[i], r, start, start + kPi * 0.5f, 6);
        draw->PathStroke(color, 0, thickness);
    }
}

float ChipWidth(ImFont* font, const char* text, bool dot) {
    return (dot ? 24.0f : 11.0f) + Measure(font, 13.5f, text).x + 11.0f;
}

float DrawChip(ImDrawList* draw, ImFont* font, ImVec2 pos, const char* text, const ImVec4* dot, ImU32 background, ImU32 foreground) {
    const ImVec2 textSize = Measure(font, 13.5f, text);
    const float height = 23.0f;
    const float width = ChipWidth(font, text, dot != nullptr);
    draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), background, height * 0.5f);
    if (dot) draw->AddCircleFilled(ImVec2(pos.x + 13, pos.y + height * 0.5f), 3.5f, Col(*dot));
    draw->AddText(font, 13.5f, ImVec2(pos.x + (dot ? 24.0f : 11.0f), pos.y + (height - textSize.y) * 0.5f), foreground, text);
    return width;
}

bool Toggle(const char* id, bool* value) {
    const float width = 44.0f;
    const float height = 24.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    if (clicked) *value = !*value;
    HandCursorIfHovered();
    float& t = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetItemID(), *value ? 1.0f : 0.0f);
    t += ((*value ? 1.0f : 0.0f) - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), Col(ImLerp(kBorder, kAccent, t)), height * 0.5f);
    draw->AddCircleFilled(ImVec2(p.x + 12.0f + t * 20.0f, p.y + 12.0f), 8.5f, Col(ImLerp(kText, kOnAccent, t)));
    return clicked;
}

bool ModeCard(const char* id, const char* title, const char* description, const char* badge, bool selected,
              const ImVec4& tint, ImVec2 size, ImFont* titleFont, ImFont* bodyFont) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    HandCursorIfHovered();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 max = p + size;
    draw->AddRectFilled(p, max, selected ? Col(tint, 0.10f) : Col(hovered ? kRaisedHover : kRaised), 14.0f);
    draw->AddRect(p, max, selected ? Col(tint, 0.9f) : Col(kBorder), 14.0f, 0, selected ? 1.6f : 1.0f);
    const ImVec2 radio(p.x + 24, p.y + size.y * 0.5f);
    draw->AddCircle(radio, 9.0f, selected ? Col(tint) : Col(kMuted, 0.7f), 0, 1.6f);
    if (selected) draw->AddCircleFilled(radio, 5.0f, Col(tint));
    draw->AddText(titleFont, 16.5f, ImVec2(p.x + 44, p.y + 12), Col(kText), title);
    draw->AddText(bodyFont, 14.0f, ImVec2(p.x + 44, p.y + 36), Col(kMuted), description);
    if (badge) {
        const float w = ChipWidth(bodyFont, badge, false);
        DrawChip(draw, bodyFont, ImVec2(max.x - w - 12, p.y + 11), badge, nullptr, Col(tint, 0.16f), Col(tint));
    }
    return clicked;
}
}

UI::UI(HWND window, ID3D11Device* device) : window_(window), device_(device) {}
UI::~UI() {
    if (previewTexture_) previewTexture_->Release();
    if (logoTexture_) logoTexture_->Release();
}

void UI::Initialize() {
    ImGuiIO& io = ImGui::GetIO();
    const char* regular = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* semibold = "C:\\Windows\\Fonts\\seguisb.ttf";
    bodyFont_ = io.Fonts->AddFontFromFileTTF(regular, 17.0f);
    headingFont_ = io.Fonts->AddFontFromFileTTF(semibold, 21.0f);
    titleFont_ = io.Fonts->AddFontFromFileTTF(semibold, 30.0f);
    if (!bodyFont_) bodyFont_ = io.FontDefault;
    if (!headingFont_) headingFont_ = bodyFont_;
    if (!titleFont_) titleFont_ = headingFont_;
    io.FontDefault = bodyFont_;
    g_semibold = headingFont_;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(14, 9);
    style.ItemSpacing = ImVec2(10, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.WindowRounding = 14.0f;
    style.FrameRounding = 12.0f;
    style.ChildRounding = 18.0f;
    style.PopupRounding = 14.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = kBg;
    colors[ImGuiCol_ChildBg] = kPanel;
    colors[ImGuiCol_PopupBg] = kPanel;
    colors[ImGuiCol_Border] = kBorder;
    colors[ImGuiCol_Text] = kText;
    colors[ImGuiCol_TextDisabled] = kMuted;
    colors[ImGuiCol_Button] = kRaised;
    colors[ImGuiCol_ButtonHovered] = kRaisedHover;
    colors[ImGuiCol_ButtonActive] = kBorder;
    colors[ImGuiCol_Header] = kRaised;
    colors[ImGuiCol_HeaderHovered] = kRaisedHover;
    colors[ImGuiCol_Separator] = kBorder;
    colors[ImGuiCol_CheckMark] = kAccent;
    colors[ImGuiCol_TitleBg] = kRaised;
    colors[ImGuiCol_TitleBgActive] = kRaised;
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.03f, 0.02f, 0.01f, 0.70f);
    colors[ImGuiCol_NavCursor] = kAccent;
    colors[ImGuiCol_ScrollbarBg] = kBg;
    colors[ImGuiCol_ScrollbarGrab] = kBorder;
    colors[ImGuiCol_ScrollbarGrabHovered] = kRaisedHover;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.30f);

    generatedIcon_ = WindowsUtils::AppDataDirectory() / L"preview.ico";
    if (const HRSRC resource = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDR_MONKEYICON_LOGO), RT_RCDATA)) {
        const HGLOBAL loaded = LoadResource(GetModuleHandleW(nullptr), resource);
        const auto* bytes = static_cast<const uint8_t*>(LockResource(loaded));
        const DWORD length = SizeofResource(GetModuleHandleW(nullptr), resource);
        DecodedImage logo;
        std::wstring logoError;
        if (IconConverter::DecodeMemoryForPreview(bytes, length, 256, logo, logoError)) UploadTexture(logo, logoTexture_);
    }
    AddLog(true, "Monkeyicon is ready");
    DetectSuggestedApplication();
}

void UI::DetectSuggestedApplication() {
    installation_ = ApplicationDetector::DetectSuggested();
    if (installation_.IsValid()) AddLog(true, "Suggested application detected: Discord");
    else AddLog(true, "Choose any Windows application to begin");
}

void UI::AddLog(bool success, std::string message) { logs_.push_back({success, std::move(message)}); }
void UI::ShowToast(bool success, std::string message) {
    toasts_.push_back({success, std::move(message), ImGui::GetTime()});
}

bool UI::UploadTexture(const DecodedImage& image, ID3D11ShaderResourceView*& destination) {
    D3D11_TEXTURE2D_DESC description{};
    description.Width = image.width;
    description.Height = image.height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{image.bgraPixels.data(), image.width * 4, 0};
    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device_->CreateTexture2D(&description, &data, &texture);
    if (FAILED(hr)) return false;
    ID3D11ShaderResourceView* view = nullptr;
    hr = device_->CreateShaderResourceView(texture, nullptr, &view);
    texture->Release();
    if (FAILED(hr)) return false;
    if (destination) destination->Release();
    destination = view;
    return true;
}

void UI::LoadPreview(const std::filesystem::path& path) {
    DecodedImage image;
    std::wstring error;
    if (!IconConverter::DecodeForPreview(path, 512, image, error)) {
        AddLog(false, WindowsUtils::ToUtf8(error));
        ShowToast(false, "Unable to load image");
        return;
    }
    if (!UploadTexture(image, previewTexture_)) {
        AddLog(false, "Unable to create image preview texture");
        ShowToast(false, "Unable to display image");
        return;
    }
    previewWidth_ = static_cast<int>(image.width);
    previewHeight_ = static_cast<int>(image.height);
}

void UI::SelectImage() {
    const std::wstring path = WindowsUtils::BrowseForImage(window_);
    if (path.empty()) return;
    selectedImage_ = path;
    if (installation_.IsValid()) generatedIcon_ = ShortcutManager::IconStoragePath(installation_);
    LoadPreview(selectedImage_);
    if (!previewTexture_) return;
    AddLog(true, "Selected image loaded");
    std::wstring error;
    if (IconConverter::CreateMultiResolutionIco(selectedImage_, generatedIcon_, error)) {
        AddLog(true, "ICO generated successfully (16-256 px)");
        ShowToast(true, "Image ready");
        currentStep_ = 2;
    } else {
        AddLog(false, WindowsUtils::ToUtf8(error));
        ShowToast(false, "ICO generation failed");
    }
}

void UI::BrowseApplication() {
    const std::wstring path = WindowsUtils::BrowseForApplicationExecutable(window_);
    if (path.empty()) return;
    auto manual = ApplicationDetector::FromExecutable(path);
    if (!manual.IsValid()) {
        AddLog(false, "The selected file is not a valid Windows executable");
        ShowToast(false, "Invalid application");
        return;
    }
    installation_ = std::move(manual);
    AddLog(true, "Target application selected");
    ShowToast(true, "Application ready");
    currentStep_ = 1;
}

void UI::ApplyIcon() {
    if (patchExecutable_) {
        confirmPatch_ = true;
        return;
    }
    PerformApply();
}

void UI::PerformApply() {
    if (!installation_.IsValid()) { ShowToast(false, "Choose an application first"); AddLog(false, "Target application not selected"); return; }
    if (selectedImage_.empty()) { ShowToast(false, "Select an image first"); AddLog(false, "No image selected"); return; }
    generatedIcon_ = ShortcutManager::IconStoragePath(installation_);
    std::wstring error;
    if (!IconConverter::CreateMultiResolutionIco(selectedImage_, generatedIcon_, error)) {
        AddLog(false, WindowsUtils::ToUtf8(error)); ShowToast(false, "ICO generation failed"); return;
    }
    const ShortcutResult result = ShortcutManager::Apply(installation_, generatedIcon_, launchAtStartup_);
    AddLog(result.success, WindowsUtils::ToUtf8(result.message));
    if (result.success) {
        shortcutPath_ = result.shortcutPath;
        AddLog(true, "Custom icon applied to launcher");
        ShowToast(true, "Custom launcher created");
        showInstructions_ = true;
    } else { ShowToast(false, "Unable to create shortcut"); return; }

    if (patchExecutable_) {
        const auto patch = IconPatcher::Patch(installation_.executablePath, generatedIcon_);
        AddLog(patch.success, WindowsUtils::ToUtf8(patch.message));
        ShowToast(patch.success, patch.success ? "Application icon patched" : "Executable patch failed");
    }
}

void UI::RestoreDefault() {
    const auto executableRestore = installation_.IsValid()
        ? IconPatcher::Restore(installation_.executablePath)
        : IconPatchResult{true, false, L"No target application selected; executable backup was not checked."};
    AddLog(executableRestore.success, WindowsUtils::ToUtf8(executableRestore.message));
    const ShortcutResult result = ShortcutManager::Restore(installation_);
    AddLog(result.success, WindowsUtils::ToUtf8(result.message));
    const bool success = result.success && executableRestore.success;
    ShowToast(success, success ? "Original application restored" : "Restore failed - close the target app first");
    if (success) shortcutPath_.clear();
}

void UI::RefreshIconCache() {
    std::wstring error;
    const bool ok = WindowsUtils::RefreshIconCache(error);
    AddLog(ok, ok ? "Windows icon cache refreshed" : WindowsUtils::ToUtf8(error));
    ShowToast(ok, ok ? "Icon display refreshed" : "Icon refresh failed");
}

void UI::DrawTitleBar() {
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();

    const ImVec2 logoCenter(origin.x + 36, origin.y + 27);
    const bool logoHovered = ImGui::IsMouseHoveringRect(logoCenter - ImVec2(20, 20), logoCenter + ImVec2(20, 20));
    propellerSpin_ += ImGui::GetIO().DeltaTime * (logoHovered ? 5.0f : 0.6f);
    DrawLogoBadge(draw, reinterpret_cast<ImTextureID>(logoTexture_), logoCenter, 16.0f, propellerSpin_);
    draw->AddText(headingFont_, 20.0f, ImVec2(origin.x + 64, origin.y + 14), Col(kText), "Monkeyicon");
    const float nameWidth = Measure(headingFont_, 20.0f, "Monkeyicon").x;
    DrawChip(draw, bodyFont_, ImVec2(origin.x + 64 + nameWidth + 12, origin.y + 16), "icon studio", nullptr,
             Col(kAccent, 0.12f), Col(kAccent));

    const auto windowButton = [&](const char* id, float x, bool close) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + x, origin.y + 10));
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(38, 32));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const ImVec2 c = (min + max) * 0.5f;
        if (hovered) draw->AddRectFilled(min, max, close ? Col(kError, 0.85f) : Col(kRaisedHover), 10.0f);
        const ImU32 glyph = Col(hovered ? kText : kMuted);
        if (close) {
            draw->AddLine(ImVec2(c.x - 5, c.y - 5), ImVec2(c.x + 5, c.y + 5), glyph, 1.6f);
            draw->AddLine(ImVec2(c.x - 5, c.y + 5), ImVec2(c.x + 5, c.y - 5), glyph, 1.6f);
        } else {
            draw->AddLine(ImVec2(c.x - 6, c.y), ImVec2(c.x + 6, c.y), glyph, 1.6f);
        }
        return pressed;
    };
    if (windowButton("##minimize", width - 92, false)) ShowWindow(window_, SW_MINIMIZE);
    if (windowButton("##close", width - 50, true)) PostMessageW(window_, WM_CLOSE, 0, 0);

    DrawCapStripe(draw, ImVec2(origin.x, origin.y + 52), ImVec2(origin.x + width, origin.y + 54), 0.85f);
}

void UI::DrawHeader() {
    headerFade_ = std::min(1.0f, headerFade_ + ImGui::GetIO().DeltaTime * 2.5f);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, 100.0f);
    const ImVec2 max = min + size;
    const float alpha = headerFade_;

    const int firstVertex = draw->VtxBuffer.Size;
    draw->AddRectFilled(min, max, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 18.0f);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, firstVertex, draw->VtxBuffer.Size, min, ImVec2(max.x, max.y + 120),
                                                  IM_COL32(28, 58, 36, 255), IM_COL32(64, 44, 24, 255));
    draw->AddRect(min, max, Col(kAccent, 0.16f * alpha), 18.0f, 0, 1.0f);

    draw->PushClipRect(min, max, true);
    const ImVec2 v0(max.x - 360, min.y - 4), v1(max.x - 290, min.y + 58), v2(max.x - 190, min.y + 62), v3(max.x - 118, min.y - 4);
    draw->AddBezierCubic(v0, v1, v2, v3, IM_COL32(86, 128, 62, static_cast<int>(200 * alpha)), 2.5f, 32);
    const float leafTs[]{0.12f, 0.3f, 0.48f, 0.66f, 0.84f};
    for (int i = 0; i < 5; ++i) {
        const float t = leafTs[i], u = 1.0f - t;
        const ImVec2 point = v0 * (u * u * u) + v1 * (3 * u * u * t) + v2 * (3 * u * t * t) + v3 * (t * t * t);
        const float angle = (i % 2 == 0 ? 0.55f : 2.3f) * kPi * 0.5f + std::sin(static_cast<float>(ImGui::GetTime()) * 1.2f + i) * 0.06f;
        DrawLeaf(draw, point, angle, 26.0f - (i % 2) * 5.0f, 9.0f,
                 IM_COL32(72 + i * 8, 150 + i * 4, 82, static_cast<int>(170 * alpha)), IM_COL32(28, 70, 40, static_cast<int>(160 * alpha)));
    }
    DrawLeaf(draw, ImVec2(max.x - 30, max.y + 6), kPi * 1.25f, 58.0f, 20.0f, IM_COL32(52, 110, 60, static_cast<int>(90 * alpha)), IM_COL32(24, 60, 34, static_cast<int>(90 * alpha)));
    DrawLeaf(draw, ImVec2(max.x - 6, max.y - 30), kPi * 1.05f, 46.0f, 16.0f, IM_COL32(64, 128, 70, static_cast<int>(80 * alpha)), IM_COL32(24, 60, 34, static_cast<int>(80 * alpha)));
    draw->PopClipRect();

    draw->AddText(headingFont_, 22.0f, ImVec2(min.x + 24, min.y + 15), Col(kText, alpha), "Monkey around with your icons");
    draw->AddText(bodyFont_, 16.0f, ImVec2(min.x + 24, min.y + 44), IM_COL32(222, 208, 182, static_cast<int>(255 * alpha)),
                  "Pick an app, pick an image, and Monkeyicon swings through every Windows icon size.");
    float chipX = min.x + 24;
    const char* chips[]{"100% local", "Private", "Reversible"};
    const ImVec4* dots[]{&kCapColors[2], &kCapColors[3], &kCapColors[1]};
    for (int i = 0; i < 3; ++i)
        chipX += DrawChip(draw, bodyFont_, ImVec2(chipX, min.y + 68), chips[i], dots[i],
                          IM_COL32(0, 0, 0, static_cast<int>(70 * alpha)), Col(kText, 0.9f * alpha)) + 8.0f;

    ImGui::Dummy(size);
}

void UI::DrawStepper() {
    auto* draw = ImGui::GetWindowDrawList();
    const float gap = 10.0f;
    const float width = (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f;
    const float height = 56.0f;
    const char* titles[]{"Pick the app", "Pick the icon", "Go bananas"};
    const char* captions[]{"Find your tree", "Grab a banana", "Apply the new look"};
    for (int i = 0; i < 3; ++i) {
        const bool unlocked = i == 0 || (i == 1 && installation_.IsValid()) ||
                              (i == 2 && installation_.IsValid() && previewTexture_);
        const bool active = i == currentStep_;
        const bool done = !active && ((i == 0 && installation_.IsValid()) || (i == 1 && previewTexture_));
        ImGui::PushID(i);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("step", ImVec2(width, height)) && unlocked) currentStep_ = i;
        const bool hovered = ImGui::IsItemHovered();
        if (hovered && unlocked && !active) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (hovered && !unlocked) ImGui::SetTooltip("Finish the previous step first");
        const ImVec2 max(p.x + width, p.y + height);
        draw->AddRectFilled(p, max, Col(active ? kRaised : (hovered && unlocked ? kRaisedHover : kBg), active ? 1.0f : 0.8f), 14.0f);
        draw->AddRect(p, max, active ? Col(kAccent, 0.85f) : Col(kBorder, 0.7f), 14.0f, 0, active ? 1.6f : 1.0f);

        const ImVec2 c(p.x + 28, p.y + height * 0.5f);
        if (active) {
            draw->AddCircleFilled(c, 18.0f, Col(kAccent, 0.15f));
            draw->AddCircleFilled(c, 14.0f, Col(kAccent));
        } else if (done) {
            draw->AddCircleFilled(c, 14.0f, Col(kSuccess));
        } else {
            draw->AddCircle(c, 14.0f, Col(kMuted, unlocked ? 0.8f : 0.4f), 0, 1.5f);
        }
        if (done) {
            DrawCheck(draw, c, 10.0f, Col(kOnAccent), 2.2f);
        } else {
            const char number[2]{static_cast<char>('1' + i), 0};
            const ImVec2 ns = Measure(headingFont_, 15.0f, number);
            draw->AddText(headingFont_, 15.0f, c - ns * 0.5f, active ? Col(kOnAccent) : Col(kMuted, unlocked ? 1.0f : 0.5f), number);
        }
        const float textAlpha = active ? 1.0f : (unlocked ? 0.85f : 0.45f);
        draw->AddText(headingFont_, 16.0f, ImVec2(p.x + 54, p.y + 9), Col(kText, textAlpha), titles[i]);
        draw->AddText(bodyFont_, 13.5f, ImVec2(p.x + 54, p.y + 31), Col(active ? kAccent : kMuted, textAlpha), captions[i]);
        ImGui::PopID();
        if (i != 2) ImGui::SameLine(0, gap);
    }
}

void UI::DrawApplyPanel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::BeginChild("apply", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::PushFont(nullptr, 13.5f);
    TextMuted("APPLY MODE");
    ImGui::PopFont();
    const float cardWidth = (ImGui::GetContentRegionAvail().x - 12.0f) * 0.5f;
    if (ModeCard("##safe", "Safe launcher", "Non-destructive custom shortcut", "Recommended", !patchExecutable_,
                 kSuccess, ImVec2(cardWidth, 64), headingFont_, bodyFont_)) patchExecutable_ = false;
    ImGui::SameLine(0, 12);
    if (ModeCard("##patch", "Direct patch", "Edits the .exe, backup made first", nullptr, patchExecutable_,
                 kWarning, ImVec2(cardWidth, 64), headingFont_, bodyFont_)) patchExecutable_ = true;

    ImGui::Dummy(ImVec2(0, 2));
    Toggle("##startup", &launchAtStartup_);
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);
    ImGui::TextUnformatted("Launch at sign-in");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(patchExecutable_ ? kWarning : kMuted,
        patchExecutable_ ? "Backup created; digital signature changes" : "Nothing on disk is modified");

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 58);
    if (AccentButton("Go bananas!", ImVec2(176, 44))) ApplyIcon();
    ImGui::SameLine(0, 12);
    if (GhostButton("Restore", ImVec2(110, 44))) RestoreDefault();
    ImGui::SameLine();
    if (GhostButton("Refresh cache", ImVec2(140, 44))) confirmRefresh_ = true;
    ImGui::SameLine();
    if (GhostButton("Setup guide", ImVec2(128, 44))) showInstructions_ = true;
    ImGui::SameLine();
    if (GhostButton("Activity", ImVec2(106, 44))) showActivityLog_ = true;

    if (confirmRefresh_) ImGui::OpenPopup("Refresh icon display?");
    confirmRefresh_ = false;
    ImGui::SetNextWindowSize(ImVec2(440, 0));
    if (ImGui::BeginPopupModal("Refresh icon display?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Windows will be asked to refresh shell icons. Explorer will not be killed or restarted.");
        ImGui::Spacing();
        if (AccentButton("Refresh", ImVec2(120, 38))) { RefreshIconCache(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(0, 12);
        if (GhostButton("Cancel", ImVec2(100, 38))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (confirmPatch_) ImGui::OpenPopup("Patch application executable?");
    confirmPatch_ = false;
    ImGui::SetNextWindowSize(ImVec2(500, 0));
    if (ImGui::BeginPopupModal("Patch application executable?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(kError, "This changes the selected executable.");
        ImGui::TextWrapped("Monkeyicon creates a backup first. Patching invalidates the file's digital signature, and an application update may overwrite the icon. Fully close the target application before continuing.");
        ImGui::Spacing();
        if (AccentButton("Patch and apply", ImVec2(160, 40))) { ImGui::CloseCurrentPopup(); PerformApply(); }
        ImGui::SameLine(0, 12);
        if (GhostButton("Cancel", ImVec2(100, 40))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (showInstructions_) ImGui::OpenPopup("Taskbar setup guide");
    showInstructions_ = false;
    ImGui::SetNextWindowSize(ImVec2(540, 0));
    if (ImGui::BeginPopupModal("Taskbar setup guide", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(headingFont_);
        ImGui::TextUnformatted("A clean taskbar handoff");
        ImGui::PopFont();
        ImGui::TextWrapped("Some applications replace their window icon after launch. Direct patch mode is more persistent, but app updates may restore the original executable.");
        ImGui::Spacing();
        const char* steps[]{"Fully close the target application.", "Unpin its existing taskbar entry.", "Apply the new icon here.",
                            "Launch it from the Monkeyicon shortcut in Start.", "Pin the new taskbar entry."};
        for (int i = 0; i < 5; ++i) {
            ImGui::TextColored(kAccent, "%d", i + 1);
            ImGui::SameLine(0, 12);
            ImGui::TextUnformatted(steps[i]);
        }
        ImGui::Spacing();
        if (!shortcutPath_.empty()) {
            if (AccentButton("Show custom shortcut", ImVec2(200, 38))) WindowsUtils::OpenFolderAndSelect(shortcutPath_);
            ImGui::SameLine(0, 12);
        }
        if (GhostButton("Close", ImVec2(100, 38))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndChild();
}

void UI::DrawWizard() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));
    ImGui::BeginChild("wizard", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    auto* draw = ImGui::GetWindowDrawList();
    DrawStepper();

    const auto heading = [&](const char* title, const char* subtitle) {
        ImGui::SetCursorPosY(92);
        ImGui::PushFont(titleFont_, 28.0f);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        TextMuted(subtitle);
        ImGui::Dummy(ImVec2(0, 6));
    };

    if (currentStep_ == 0) {
        heading(installation_.IsValid() ? "App locked in" : "Which app should we monkey with?",
                "Choose any classic Windows .exe. Nothing changes until the final step.");

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, 92);
        const ImVec2 max = p + size;
        if (ImGui::InvisibleButton("##appCard", size)) BrowseApplication();
        const bool hovered = ImGui::IsItemHovered();
        HandCursorIfHovered();
        const bool valid = installation_.IsValid();
        const ImVec4& tint = valid ? kSuccess : kAccent;
        draw->AddRectFilled(p, max, Col(hovered ? kRaised : kBg), 16.0f);
        if (valid) draw->AddRect(p, max, Col(kSuccess, hovered ? 0.7f : 0.4f), 16.0f, 0, 1.4f);
        else DrawDashedRect(draw, p, max, Col(kAccent, hovered ? 0.8f : 0.45f), 16.0f);
        const ImVec2 tile(p.x + 18, p.y + 18);
        draw->AddRectFilled(tile, tile + ImVec2(56, 56), Col(tint, 0.13f), 14.0f);
        DrawAppGlyph(draw, tile + ImVec2(28, 28), Col(tint));
        if (valid) {
            const ImVec2 badge = tile + ImVec2(52, 52);
            draw->AddCircleFilled(badge, 10.0f, Col(kBg));
            draw->AddCircleFilled(badge, 8.0f, Col(kSuccess));
            DrawCheck(draw, badge, 6.5f, Col(kOnAccent), 1.8f);
        }
        const char* pill = valid ? "Ready" : "Click to browse";
        const float pillWidth = ChipWidth(bodyFont_, pill, true);
        const float textRight = max.x - pillWidth - 30;
        draw->PushClipRect(p, ImVec2(textRight, max.y), true);
        if (valid) {
            draw->AddText(headingFont_, 19.0f, ImVec2(p.x + 92, p.y + 21), Col(kText), WindowsUtils::ToUtf8(installation_.displayName).c_str());
            const std::string path = WindowsUtils::ToUtf8(installation_.executablePath.wstring());
            draw->AddText(bodyFont_, 15.0f, ImVec2(p.x + 92, p.y + 50), Col(kMuted), Ellipsize(path, 80).c_str());
        } else {
            draw->AddText(headingFont_, 19.0f, ImVec2(p.x + 92, p.y + 21), Col(kText), "No app picked yet");
            draw->AddText(bodyFont_, 15.0f, ImVec2(p.x + 92, p.y + 50), Col(kMuted), "Browse to the .exe you normally use to launch it.");
        }
        draw->PopClipRect();
        DrawChip(draw, bodyFont_, ImVec2(max.x - pillWidth - 18, p.y + (size.y - 23) * 0.5f), pill, valid ? &kSuccess : &kAccent,
                 Col(tint, 0.12f), Col(tint));

        ImGui::Dummy(ImVec2(0, 4));
        const ImVec2 tipMin = ImGui::GetCursorScreenPos();
        const ImVec2 tipMax = tipMin + ImVec2(ImGui::GetContentRegionAvail().x, 46);
        draw->AddRectFilled(tipMin, tipMax, Col(kAccent, 0.06f), 12.0f);
        draw->AddRect(tipMin, tipMax, Col(kAccent, 0.18f), 12.0f);
        DrawBanana(draw, tipMin + ImVec2(24, 24), 11.0f);
        draw->AddText(headingFont_, 15.5f, tipMin + ImVec2(48, 13), Col(kAccent), "Monkey tip");
        draw->AddText(bodyFont_, 15.5f, tipMin + ImVec2(48 + Measure(headingFont_, 15.5f, "Monkey tip").x + 10, 13), Col(kMuted),
                      "Right-click a desktop shortcut and pick Open file location to find its .exe.");
        ImGui::Dummy(tipMax - tipMin);

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 66);
        if (AccentButton(valid ? "Choose a different app" : "Browse applications", ImVec2(230, 46))) BrowseApplication();
        if (valid) {
            ImGui::SameLine(0, 12);
            if (GhostButton("Continue  >", ImVec2(150, 46))) currentStep_ = 1;
        }
    } else if (currentStep_ == 1) {
        heading(previewTexture_ ? "Looking sharp!" : "Choose the new look",
                "A square PNG with transparency gives the cleanest result.");

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 area(160, 160);
        const ImVec2 max = p + area;
        if (ImGui::InvisibleButton("##previewTile", area)) SelectImage();
        const bool hovered = ImGui::IsItemHovered();
        HandCursorIfHovered();
        draw->AddRectFilled(p, max, Col(hovered ? kRaised : kBg), 24.0f);
        const ImVec2 center = (p + max) * 0.5f;
        if (previewTexture_) {
            for (int i = 4; i >= 1; --i) draw->AddCircleFilled(center, 30.0f + i * 12.0f, Col(kAccent, 0.025f));
            draw->AddRect(p, max, Col(kBorder), 24.0f);
            const float scale = std::min(122.0f / previewWidth_, 122.0f / previewHeight_);
            const ImVec2 half(previewWidth_ * scale * 0.5f, previewHeight_ * scale * 0.5f);
            draw->AddImage(reinterpret_cast<ImTextureID>(previewTexture_), center - half, center + half);
        } else {
            DrawDashedRect(draw, p, max, Col(kAccent, hovered ? 0.8f : 0.45f), 24.0f);
            DrawBanana(draw, center - ImVec2(0, 22), 18.0f);
            const char* formats = "PNG  JPG  BMP  ICO";
            const char* action = "Click to browse";
            draw->AddText(bodyFont_, 14.0f, ImVec2(center.x - Measure(bodyFont_, 14.0f, formats).x * 0.5f, center.y + 18), Col(kMuted), formats);
            draw->AddText(headingFont_, 15.0f, ImVec2(center.x - Measure(headingFont_, 15.0f, action).x * 0.5f, center.y + 38), Col(kAccent), action);
        }

        const float columnX = max.x + 32;
        draw->AddText(bodyFont_, 13.5f, ImVec2(columnX, p.y), Col(kMuted), "EVERY SIZE, HANDLED");
        const int sizes[]{48, 32, 24, 16};
        for (int i = 0; i < 4; ++i) {
            const ImVec2 box(columnX + i * 76.0f, p.y + 24);
            draw->AddRectFilled(box, box + ImVec2(64, 64), Col(kBg), 12.0f);
            draw->AddRect(box, box + ImVec2(64, 64), Col(kBorder), 12.0f);
            const float s = static_cast<float>(sizes[i]);
            const ImVec2 c = box + ImVec2(32, 32);
            if (previewTexture_) draw->AddImage(reinterpret_cast<ImTextureID>(previewTexture_), c - ImVec2(s, s) * 0.5f, c + ImVec2(s, s) * 0.5f);
            else DrawDashedRect(draw, c - ImVec2(s, s) * 0.5f, c + ImVec2(s, s) * 0.5f, Col(kMuted, 0.4f), std::min(4.0f, s * 0.2f), 1.0f);
            const std::string label = std::to_string(sizes[i]) + " px";
            draw->AddText(bodyFont_, 13.0f, ImVec2(c.x - Measure(bodyFont_, 13.0f, label.c_str()).x * 0.5f, box.y + 70), Col(kMuted), label.c_str());
        }
        draw->AddText(bodyFont_, 13.5f, ImVec2(columnX, p.y + 106), Col(kMuted), "ON YOUR TASKBAR");
        const ImVec2 bar(columnX, p.y + 126);
        const ImVec2 barMax = bar + ImVec2(292, 42);
        draw->AddRectFilled(bar, barMax, IM_COL32(30, 30, 34, 255), 10.0f);
        draw->AddRect(bar, barMax, IM_COL32(58, 58, 64, 255), 10.0f);
        for (int i = 0; i < 6; ++i) {
            const ImVec2 c(bar.x + 26 + i * 48.0f, bar.y + 21);
            if (i == 0) {
                for (int q = 0; q < 4; ++q) {
                    const ImVec2 o(c.x - 9 + (q % 2) * 10.0f, c.y - 9 + (q / 2) * 10.0f);
                    draw->AddRectFilled(o, o + ImVec2(8, 8), IM_COL32(96, 170, 240, 255), 1.5f);
                }
            } else if (i == 3) {
                draw->AddRectFilled(c - ImVec2(18, 17), c + ImVec2(18, 17), IM_COL32(255, 255, 255, 18), 6.0f);
                if (previewTexture_) draw->AddImage(reinterpret_cast<ImTextureID>(previewTexture_), c - ImVec2(12, 13), c + ImVec2(12, 11));
                else draw->AddRect(c - ImVec2(12, 13), c + ImVec2(12, 11), Col(kAccent, 0.6f), 4.0f);
                draw->AddRectFilled(ImVec2(c.x - 8, bar.y + 37), ImVec2(c.x + 8, bar.y + 40), Col(kAccent), 2.0f);
            } else {
                draw->AddRectFilled(c - ImVec2(11, 11), c + ImVec2(11, 11), IM_COL32(78, 78, 86, 255), 5.0f);
            }
        }

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 66);
        if (GhostButton("<  Back", ImVec2(110, 46))) currentStep_ = 0;
        ImGui::SameLine(0, 12);
        if (AccentButton(previewTexture_ ? "Replace image" : "Choose image", ImVec2(190, 46))) SelectImage();
        if (previewTexture_) {
            ImGui::SameLine(0, 12);
            if (GhostButton("Continue  >", ImVec2(150, 46))) currentStep_ = 2;
        }
    } else {
        ImGui::SetCursorPosY(86);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 max = p + ImVec2(ImGui::GetContentRegionAvail().x, 74);
        draw->AddRectFilled(p, max, Col(kAccent, 0.07f), 16.0f);
        draw->AddRect(p, max, Col(kAccent, 0.25f), 16.0f);
        const ImVec2 thumb(p.x + 14, p.y + 13);
        draw->AddRectFilled(thumb, thumb + ImVec2(48, 48), Col(kBg), 12.0f);
        if (previewTexture_) draw->AddImage(reinterpret_cast<ImTextureID>(previewTexture_), thumb + ImVec2(6, 6), thumb + ImVec2(42, 42));
        const std::string title = "Ready to go bananas on " + WindowsUtils::ToUtf8(installation_.displayName);
        draw->AddText(headingFont_, 19.0f, ImVec2(p.x + 76, p.y + 14), Col(kText), title.c_str());
        draw->AddText(bodyFont_, 15.0f, ImVec2(p.x + 76, p.y + 41), Col(kMuted),
                      "Your icon is built in every size from 16 to 256 px. Pick how to apply it.");
        ImGui::Dummy(max - p);
        ImGui::Dummy(ImVec2(0, 2));
        DrawApplyPanel();
    }

    ImGui::EndChild();
}

void UI::DrawActivityLog() {
    if (showActivityLog_) ImGui::OpenPopup("Activity log");
    showActivityLog_ = false;
    ImGui::SetNextWindowSize(ImVec2(560, 340), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Activity log", nullptr, ImGuiWindowFlags_NoResize)) {
        TextMuted("Recent Monkeyicon operations");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kBg);
        ImGui::BeginChild("log", ImVec2(0, 230), ImGuiChildFlags_Borders);
        for (const auto& entry : logs_) {
            DrawStatusDot(entry.success);
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.text.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (GhostButton("Close", ImVec2(110, 38))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void UI::DrawToasts() {
    const double now = ImGui::GetTime();
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [now](const Toast& t) { return now - t.created > 3.4; }), toasts_.end());
    float y = 72.0f;
    for (const auto& toast : toasts_) {
        const float age = static_cast<float>(now - toast.created);
        const float alpha = std::clamp(age < 0.2f ? age / 0.2f : (age > 2.9f ? (3.4f - age) / 0.5f : 1.0f), 0.0f, 1.0f);
        ImGui::SetNextWindowBgAlpha(0.97f * alpha);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 28, y - (1.0f - alpha) * 8.0f), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kRaised);
        ImGui::PushStyleColor(ImGuiCol_Border, toast.success ? ImVec4(kSuccess.x, kSuccess.y, kSuccess.z, 0.5f)
                                                             : ImVec4(kError.x, kError.y, kError.z, 0.5f));
        ImGui::Begin(("##toast" + std::to_string(&toast - toasts_.data())).c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
        DrawStatusDot(toast.success);
        ImGui::SameLine();
        ImGui::TextUnformatted(toast.message.c_str());
        y += ImGui::GetWindowHeight() + 8;
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}

void UI::Render() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("Monkeyicon", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    auto* background = ImGui::GetWindowDrawList();
    const ImVec2 corner(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
    for (int i = 6; i >= 1; --i) background->AddCircleFilled(corner, 60.0f * i, Col(kAccent, 0.008f));

    DrawTitleBar();
    ImGui::SetCursorPos(ImVec2(22, 68));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("content", ImVec2(ImGui::GetWindowWidth() - 44, ImGui::GetWindowHeight() - 84), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    DrawHeader();
    ImGui::Dummy(ImVec2(0, 2));
    DrawWizard();
    ImGui::EndChild();
    DrawActivityLog();
    ImGui::End();
    DrawToasts();
}
