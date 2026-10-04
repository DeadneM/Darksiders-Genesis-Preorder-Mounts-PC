#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <MinHook.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <string>

namespace {

constexpr const char* kBuild = "0.1.0-test";
constexpr const wchar_t* kIniName = L"DarksidersGenesisMod.ini";
constexpr const wchar_t* kLogName = L"DarksidersGenesisMod.log";

HMODULE g_module = nullptr;
std::wstring g_iniPath;
std::wstring g_logPath;

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

PresentFn g_originalPresent = nullptr;
ResizeBuffersFn g_originalResizeBuffers = nullptr;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
IDXGISwapChain* g_gameSwapChain = nullptr;
HWND g_hwnd = nullptr;
WNDPROC g_originalWndProc = nullptr;

std::atomic_bool g_imguiReady{false};
std::atomic_bool g_overlayVisible{false};
std::array<bool, 256> g_keyDown{};
std::string g_lastAction = "None";

enum class Action : int {
    None = 0,
    ToggleHUD,
    MovementSpeed,
    ActionRecovery,
    SkipIntroVideos,
    ThirdPerson,
    Count
};

constexpr std::array<const char*, static_cast<size_t>(Action::Count)> kActionLabels = {
    "None",
    "Toggle HUD",
    "Movement Speed",
    "Action Recovery",
    "Skip Intro Videos",
    "Third Person"
};

constexpr std::array<const wchar_t*, static_cast<size_t>(Action::Count)> kActionTokens = {
    L"None",
    L"ToggleHUD",
    L"MovementSpeed",
    L"ActionRecovery",
    L"SkipIntroVideos",
    L"ThirdPerson"
};

const char* ActionLabel(Action action) {
    const int i = static_cast<int>(action);
    if (i < 0 || i >= static_cast<int>(Action::Count)) {
        return "None";
    }
    return kActionLabels[static_cast<size_t>(i)];
}

const wchar_t* ActionToken(Action action) {
    const int i = static_cast<int>(action);
    if (i < 0 || i >= static_cast<int>(Action::Count)) {
        return L"None";
    }
    return kActionTokens[static_cast<size_t>(i)];
}

Action ParseAction(const wchar_t* text) {
    if (!text) {
        return Action::None;
    }
    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        if (_wcsicmp(text, kActionTokens[static_cast<size_t>(i)]) == 0) {
            return static_cast<Action>(i);
        }
    }
    return Action::None;
}

void InitializePaths() {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(g_module, path, MAX_PATH)) {
        return;
    }

    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        return;
    }
    *(slash + 1) = L'\0';

    g_iniPath = path;
    g_iniPath += kIniName;

    g_logPath = path;
    g_logPath += kLogName;
}

void Log(const char* format, ...) {
    if (g_logPath.empty()) {
        return;
    }

    char message[2048]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME st{};
    GetLocalTime(&st);

    char line[2300]{};
    sprintf_s(
        line,
        sizeof(line),
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        message
    );

    HANDLE file = CreateFileW(
        g_logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
        CloseHandle(file);
    }
}

struct Config {
    bool overlayEnabled = true;

    // Requested default policy: every planned feature is enabled by default.
    bool toggleHudEnabled = true;
    bool movementSpeedEnabled = true;
    bool actionRecoveryEnabled = true;
    bool skipIntroEnabled = true;
    bool thirdPersonEnabled = true;

    // Tentative user-facing tuning values. They are inert until their gameplay
    // hooks are implemented and validated.
    float movementSpeedMultiplier = 1.15f;
    float actionRecoveryMultiplier = 2.00f;

    std::array<Action, 12> hotkeys{};

    Config() {
        ResetDefaults(false);
    }

    static bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback, const std::wstring& path) {
        return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, path.c_str()) != 0;
    }

    static float ReadFloat(const wchar_t* section, const wchar_t* key, float fallback, const std::wstring& path) {
        wchar_t buffer[64]{};
        wchar_t fallbackText[64]{};
        swprintf_s(fallbackText, L"%.3f", fallback);
        GetPrivateProfileStringW(section, key, fallbackText, buffer, 64, path.c_str());
        wchar_t* end = nullptr;
        const float value = wcstof(buffer, &end);
        return (end && end != buffer) ? value : fallback;
    }

    static void WriteBool(const wchar_t* section, const wchar_t* key, bool value, const std::wstring& path) {
        WritePrivateProfileStringW(section, key, value ? L"1" : L"0", path.c_str());
    }

    static void WriteFloat(const wchar_t* section, const wchar_t* key, float value, const std::wstring& path) {
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%.3f", value);
        WritePrivateProfileStringW(section, key, buffer, path.c_str());
    }

    void ResetDefaults(bool save) {
        overlayEnabled = true;
        toggleHudEnabled = true;
        movementSpeedEnabled = true;
        actionRecoveryEnabled = true;
        skipIntroEnabled = true;
        thirdPersonEnabled = true;
        movementSpeedMultiplier = 1.15f;
        actionRecoveryMultiplier = 2.00f;

        hotkeys.fill(Action::None);
        hotkeys[0] = Action::ToggleHUD;
        hotkeys[1] = Action::MovementSpeed;
        hotkeys[2] = Action::ActionRecovery;
        hotkeys[3] = Action::SkipIntroVideos;
        hotkeys[4] = Action::ThirdPerson;

        if (save) {
            Save();
        }
    }

    void Load() {
        if (g_iniPath.empty()) {
            return;
        }

        if (GetFileAttributesW(g_iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            ResetDefaults(true);
            Log("INI not found -> wrote authoritative defaults");
            return;
        }

        overlayEnabled = ReadBool(L"Overlay", L"Enabled", true, g_iniPath);

        toggleHudEnabled = ReadBool(L"Features", L"ToggleHUD", true, g_iniPath);
        movementSpeedEnabled = ReadBool(L"Features", L"MovementSpeed", true, g_iniPath);
        actionRecoveryEnabled = ReadBool(L"Features", L"ActionRecovery", true, g_iniPath);
        skipIntroEnabled = ReadBool(L"Features", L"SkipIntroVideos", true, g_iniPath);
        thirdPersonEnabled = ReadBool(L"Features", L"ThirdPerson", true, g_iniPath);

        movementSpeedMultiplier = ReadFloat(L"Values", L"MovementSpeedMultiplier", 1.15f, g_iniPath);
        actionRecoveryMultiplier = ReadFloat(L"Values", L"ActionRecoveryMultiplier", 2.00f, g_iniPath);

        for (int i = 0; i < 12; ++i) {
            wchar_t key[8]{};
            swprintf_s(key, L"F%d", i + 1);

            wchar_t value[64]{};
            GetPrivateProfileStringW(
                L"Hotkeys",
                key,
                ActionToken(hotkeys[static_cast<size_t>(i)]),
                value,
                64,
                g_iniPath.c_str()
            );
            hotkeys[static_cast<size_t>(i)] = ParseAction(value);
        }

        Log("INI loaded");
    }

    void Save() const {
        if (g_iniPath.empty()) {
            return;
        }

        WriteBool(L"Overlay", L"Enabled", overlayEnabled, g_iniPath);
        WritePrivateProfileStringW(L"Overlay", L"MenuKey", L"Insert", g_iniPath.c_str());

        WriteBool(L"Features", L"ToggleHUD", toggleHudEnabled, g_iniPath);
        WriteBool(L"Features", L"MovementSpeed", movementSpeedEnabled, g_iniPath);
        WriteBool(L"Features", L"ActionRecovery", actionRecoveryEnabled, g_iniPath);
        WriteBool(L"Features", L"SkipIntroVideos", skipIntroEnabled, g_iniPath);
        WriteBool(L"Features", L"ThirdPerson", thirdPersonEnabled, g_iniPath);

        WriteFloat(L"Values", L"MovementSpeedMultiplier", movementSpeedMultiplier, g_iniPath);
        WriteFloat(L"Values", L"ActionRecoveryMultiplier", actionRecoveryMultiplier, g_iniPath);

        for (int i = 0; i < 12; ++i) {
            wchar_t key[8]{};
            swprintf_s(key, L"F%d", i + 1);
            WritePrivateProfileStringW(
                L"Hotkeys",
                key,
                ActionToken(hotkeys[static_cast<size_t>(i)]),
                g_iniPath.c_str()
            );
        }

        Log("INI saved");
    }
};

Config g_config;

bool IsFeatureEnabled(Action action) {
    switch (action) {
    case Action::ToggleHUD:
        return g_config.toggleHudEnabled;
    case Action::MovementSpeed:
        return g_config.movementSpeedEnabled;
    case Action::ActionRecovery:
        return g_config.actionRecoveryEnabled;
    case Action::SkipIntroVideos:
        return g_config.skipIntroEnabled;
    case Action::ThirdPerson:
        return g_config.thirdPersonEnabled;
    default:
        return true;
    }
}

bool KeyPressed(int vk) {
    if (vk < 0 || vk >= static_cast<int>(g_keyDown.size())) {
        return false;
    }

    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool pressed = down && !g_keyDown[static_cast<size_t>(vk)];
    g_keyDown[static_cast<size_t>(vk)] = down;
    return pressed;
}

void TriggerAction(Action action, int functionKey) {
    if (action == Action::None) {
        return;
    }

    const char* label = ActionLabel(action);
    if (!IsFeatureEnabled(action)) {
        g_lastAction = std::string(label) + " disabled in config";
        Log("F%d -> %s ignored (feature disabled)", functionKey, label);
        return;
    }

    g_lastAction = std::string(label) + " [hook pending]";
    Log("F%d -> %s (input OK, gameplay hook pending)", functionKey, label);
}

void ProcessInput() {
    if (KeyPressed(VK_INSERT) && g_config.overlayEnabled) {
        const bool newState = !g_overlayVisible.load();
        g_overlayVisible.store(newState);
        Log("Overlay %s", newState ? "OPEN" : "CLOSED");
    }

    if (g_overlayVisible.load()) {
        return;
    }

    for (int i = 0; i < 12; ++i) {
        if (KeyPressed(VK_F1 + i)) {
            TriggerAction(g_config.hotkeys[static_cast<size_t>(i)], i + 1);
        }
    }
}

void CreateRenderTarget() {
    if (!g_gameSwapChain || !g_device || g_rtv) {
        return;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(g_gameSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
        backBuffer->Release();
    }
}

void ReleaseRenderTarget() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_imguiReady.load() && g_overlayVisible.load()) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

        ImGuiIO& io = ImGui::GetIO();
        const bool mouseMessage =
            msg == WM_MOUSEMOVE ||
            msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK ||
            msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP || msg == WM_MBUTTONDBLCLK ||
            msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK ||
            msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL ||
            msg == WM_SETCURSOR;

        const bool keyboardMessage =
            msg == WM_KEYDOWN || msg == WM_KEYUP ||
            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
            msg == WM_CHAR;

        if ((mouseMessage && io.WantCaptureMouse) ||
            (keyboardMessage && io.WantCaptureKeyboard) ||
            msg == WM_INPUT) {
            return TRUE;
        }
    }

    return g_originalWndProc
        ? CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool InitializeImGui(IDXGISwapChain* swapChain) {
    if (g_imguiReady.load()) {
        return true;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow) {
        return false;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(desc.OutputWindow, &pid);
    if (pid != GetCurrentProcessId()) {
        return false;
    }

    ID3D11Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device) {
        return false;
    }

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context) {
        device->Release();
        return false;
    }

    g_device = device;
    g_context = context;
    g_gameSwapChain = swapChain;
    g_hwnd = desc.OutputWindow;

    CreateRenderTarget();
    if (!g_rtv) {
        g_context->Release();
        g_context = nullptr;
        g_device->Release();
        g_device = nullptr;
        g_gameSwapChain = nullptr;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 7.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);

    if (!ImGui_ImplWin32_Init(g_hwnd) || !ImGui_ImplDX11_Init(g_device, g_context)) {
        ImGui::DestroyContext();
        ReleaseRenderTarget();
        g_context->Release();
        g_context = nullptr;
        g_device->Release();
        g_device = nullptr;
        g_gameSwapChain = nullptr;
        Log("ImGui backend initialization FAILED");
        return false;
    }

    SetLastError(0);
    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OverlayWndProc))
    );

    if (!g_originalWndProc && GetLastError() != 0) {
        Log("WndProc hook FAILED error=%lu", GetLastError());
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        ReleaseRenderTarget();
        g_context->Release();
        g_context = nullptr;
        g_device->Release();
        g_device = nullptr;
        g_gameSwapChain = nullptr;
        return false;
    }

    g_imguiReady.store(true);
    Log("ImGui D3D11 overlay READY hwnd=%p", g_hwnd);
    return true;
}

void DrawFeatureRow(const char* label, bool* enabled, const char* note) {
    ImGui::Checkbox(label, enabled);
    ImGui::SameLine(260.0f);
    ImGui::TextDisabled("%s", note);
}

void DrawOverlay() {
    ImGui::SetNextWindowSize(ImVec2(780.0f, 570.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_FirstUseEver);

    bool open = true;
    if (!ImGui::Begin("Darksiders Genesis Enhanced", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) {
            g_overlayVisible.store(false);
        }
        return;
    }

    ImGui::Text("ASI Overlay  v%s", kBuild);
    ImGui::SameLine();
    ImGui::TextDisabled("| Insert to close");
    ImGui::Separator();

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("General")) {
            ImGui::Spacing();
            ImGui::Text("Renderer");
            ImGui::BulletText("DXGI proxy loader: active");
            ImGui::BulletText("D3D11 Present hook: active");
            ImGui::BulletText("Mouse capture: active while menu is open");
            ImGui::BulletText("F1-F12 gameplay input: suppressed while menu is open");

            ImGui::Spacing();
            ImGui::Text("Configuration");
            if (ImGui::Button("Save", ImVec2(110.0f, 0.0f))) {
                g_config.Save();
                g_lastAction = "Configuration saved";
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload", ImVec2(110.0f, 0.0f))) {
                g_config.Load();
                g_lastAction = "Configuration reloaded";
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Defaults", ImVec2(140.0f, 0.0f))) {
                g_config.ResetDefaults(true);
                g_lastAction = "Defaults restored";
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextWrapped(
                "V0.1 validates the loader, in-game overlay, mouse input, INI persistence "
                "and configurable F1-F12 action map. Gameplay hooks are deliberately not "
                "pretended: each planned feature is enabled by default but marked pending "
                "until its game-native implementation is verified."
            );
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Features")) {
            ImGui::Spacing();
            ImGui::Text("Default policy: all requested features are enabled.");
            ImGui::Spacing();

            DrawFeatureRow("Toggle HUD", &g_config.toggleHudEnabled, "Hook pending");
            DrawFeatureRow("Movement Speed", &g_config.movementSpeedEnabled, "Hook pending");
            if (g_config.movementSpeedEnabled) {
                ImGui::Indent();
                ImGui::SetNextItemWidth(260.0f);
                ImGui::SliderFloat("Multiplier##Movement", &g_config.movementSpeedMultiplier, 1.00f, 2.50f, "%.2fx");
                ImGui::Unindent();
            }

            DrawFeatureRow("Action Recovery", &g_config.actionRecoveryEnabled, "Hook pending");
            if (g_config.actionRecoveryEnabled) {
                ImGui::Indent();
                ImGui::SetNextItemWidth(260.0f);
                ImGui::SliderFloat("Recovery Multiplier", &g_config.actionRecoveryMultiplier, 1.00f, 5.00f, "%.2fx");
                ImGui::Unindent();
            }

            DrawFeatureRow("Skip Intro Videos", &g_config.skipIntroEnabled, "UE4 MoviePlayer audit started");
            DrawFeatureRow("Third Person", &g_config.thirdPersonEnabled, "Camera hook pending");

            ImGui::Spacing();
            ImGui::TextDisabled("Values above are stored now; gameplay application arrives feature-by-feature.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Hotkeys")) {
            ImGui::Spacing();
            ImGui::TextWrapped(
                "F1-F12 are fixed physical slots, Q Protocol style. "
                "Each slot can be reassigned to any mod action or None."
            );
            ImGui::Spacing();

            if (ImGui::BeginTable("HotkeyTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (int i = 0; i < 12; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("F%d", i + 1);

                    ImGui::TableSetColumnIndex(1);
                    char comboId[32]{};
                    sprintf_s(comboId, sizeof(comboId), "##HotkeyF%d", i + 1);

                    int current = static_cast<int>(g_config.hotkeys[static_cast<size_t>(i)]);
                    if (current < 0 || current >= static_cast<int>(Action::Count)) {
                        current = 0;
                    }

                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo(comboId, kActionLabels[static_cast<size_t>(current)])) {
                        for (int a = 0; a < static_cast<int>(Action::Count); ++a) {
                            const bool selected = current == a;
                            if (ImGui::Selectable(kActionLabels[static_cast<size_t>(a)], selected)) {
                                g_config.hotkeys[static_cast<size_t>(i)] = static_cast<Action>(a);
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Default: F1 HUD | F2 Movement | F3 Recovery | F4 Skip Intro | F5 Third Person");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("About")) {
            ImGui::Spacing();
            ImGui::Text("Darksiders Genesis Enhanced - experimental ASI core");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Target executable audited for this branch: DarksidersGenesis-Win64-Shipping.exe"
            );
            ImGui::TextWrapped(
                "SHA-256: 9f4702024df5eea1d51df7745b0ad1ea95b97009982f73ddc1218c53dff33d54"
            );
            ImGui::TextWrapped("Size: 62,113,280 bytes");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "The executable imports DXGI and D3D11 directly. It also contains UE4 MoviePlayer "
                "symbols including StartupMovies and WindowsMoviePlayer, which is the active lead "
                "for the intro-video feature."
            );
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Text("Last action: %s", g_lastAction.c_str());

    ImGui::End();

    if (!open) {
        g_overlayVisible.store(false);
    }
}

HRESULT __stdcall HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    ProcessInput();

    if (!g_imguiReady.load()) {
        InitializeImGui(swapChain);
    }

    if (g_imguiReady.load() && swapChain == g_gameSwapChain && g_overlayVisible.load()) {
        // UE4 can clip or hide the cursor during gameplay. Release clipping every
        // overlay frame and let ImGui draw its own pointer.
        ClipCursor(nullptr);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::GetIO().MouseDrawCursor = true;
        DrawOverlay();

        ImGui::Render();

        if (g_rtv) {
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    } else if (g_imguiReady.load()) {
        ImGui::GetIO().MouseDrawCursor = false;
    }

    return g_originalPresent
        ? g_originalPresent(swapChain, syncInterval, flags)
        : S_OK;
}

HRESULT __stdcall HookResizeBuffers(
    IDXGISwapChain* swapChain,
    UINT bufferCount,
    UINT width,
    UINT height,
    DXGI_FORMAT newFormat,
    UINT swapChainFlags
) {
    if (g_imguiReady.load() && swapChain == g_gameSwapChain) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ReleaseRenderTarget();
    }

    const HRESULT hr = g_originalResizeBuffers
        ? g_originalResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags)
        : E_FAIL;

    if (SUCCEEDED(hr) && g_imguiReady.load() && swapChain == g_gameSwapChain) {
        CreateRenderTarget();
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    return hr;
}

LRESULT CALLBACK ProbeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool DiscoverAndHookD3D11() {
    const wchar_t* className = L"DarksidersGenesisModProbeWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ProbeWndProc;
    wc.hInstance = g_module;
    wc.lpszClassName = className;

    RegisterClassExW(&wc);

    HWND window = CreateWindowExW(
        0,
        className,
        L"DG probe",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        g_module,
        nullptr
    );

    if (!window) {
        Log("Probe window creation FAILED error=%lu", GetLastError());
        UnregisterClassW(className, g_module);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 100;
    sd.BufferDesc.Height = 100;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL requested[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    IDXGISwapChain* probeSwap = nullptr;
    ID3D11Device* probeDevice = nullptr;
    ID3D11DeviceContext* probeContext = nullptr;
    D3D_FEATURE_LEVEL obtained{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        requested,
        ARRAYSIZE(requested),
        D3D11_SDK_VERSION,
        &sd,
        &probeSwap,
        &probeDevice,
        &obtained,
        &probeContext
    );

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            requested,
            ARRAYSIZE(requested),
            D3D11_SDK_VERSION,
            &sd,
            &probeSwap,
            &probeDevice,
            &obtained,
            &probeContext
        );
    }

    if (FAILED(hr) || !probeSwap) {
        Log("D3D11 probe creation FAILED hr=0x%08lX", static_cast<unsigned long>(hr));
        if (probeContext) probeContext->Release();
        if (probeDevice) probeDevice->Release();
        DestroyWindow(window);
        UnregisterClassW(className, g_module);
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(probeSwap);
    void* presentAddress = vtable[8];
    void* resizeBuffersAddress = vtable[13];

    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        Log("MinHook initialize FAILED status=%d", static_cast<int>(initStatus));
        probeSwap->Release();
        probeContext->Release();
        probeDevice->Release();
        DestroyWindow(window);
        UnregisterClassW(className, g_module);
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        presentAddress,
        reinterpret_cast<LPVOID>(&HookPresent),
        reinterpret_cast<LPVOID*>(&g_originalPresent)
    );
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Log("Present hook creation FAILED status=%d", static_cast<int>(status));
        probeSwap->Release();
        probeContext->Release();
        probeDevice->Release();
        DestroyWindow(window);
        UnregisterClassW(className, g_module);
        return false;
    }

    status = MH_CreateHook(
        resizeBuffersAddress,
        reinterpret_cast<LPVOID>(&HookResizeBuffers),
        reinterpret_cast<LPVOID*>(&g_originalResizeBuffers)
    );
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Log("ResizeBuffers hook creation FAILED status=%d", static_cast<int>(status));
        probeSwap->Release();
        probeContext->Release();
        probeDevice->Release();
        DestroyWindow(window);
        UnregisterClassW(className, g_module);
        return false;
    }

    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        Log("MinHook enable FAILED status=%d", static_cast<int>(status));
        probeSwap->Release();
        probeContext->Release();
        probeDevice->Release();
        DestroyWindow(window);
        UnregisterClassW(className, g_module);
        return false;
    }

    Log("D3D11 hooks installed Present=%p ResizeBuffers=%p", presentAddress, resizeBuffersAddress);

    probeSwap->Release();
    probeContext->Release();
    probeDevice->Release();
    DestroyWindow(window);
    UnregisterClassW(className, g_module);
    return true;
}

DWORD WINAPI MainThread(LPVOID) {
    InitializePaths();
    Log("============================================================");
    Log("Darksiders Genesis Enhanced ASI %s starting", kBuild);
    Log("Target EXE audit SHA256=9f4702024df5eea1d51df7745b0ad1ea95b97009982f73ddc1218c53dff33d54");
    Log("Architecture: DXGI proxy -> ASI -> D3D11 Present/ResizeBuffers -> Dear ImGui");

    g_config.Load();

    if (!DiscoverAndHookD3D11()) {
        Log("Overlay hook setup FAILED. Mod stays fail-open; game should continue normally.");
        return 0;
    }

    Log("Core initialization complete. Press Insert after the first game frame.");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
