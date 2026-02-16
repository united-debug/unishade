#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <thread>
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cmath> // For animation math

// Link libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

// RESOURCE ID - MUST MATCH Resource.rc (101 ZIP "UniShade.zip")
#define IDR_UNISHADE_ZIP 101 

namespace fs = std::filesystem;

// ================= ENUMS & STATE =================
enum class Page { Start, License, Installing, Finish };

Page g_CurrentPage = Page::Start;
std::string g_StatusText = "Waiting to start...";

// ANIMATION VARIABLES
float g_TargetProgress = 0.0f;  // Actual status
float g_CurrentProgress = 0.0f; // Visual animation
int g_InstallStep = 0;

// Checkboxes
bool g_ChkRoblox = true;
bool g_ChkYoutube = true;
bool g_ChkDiscord = true;
bool g_ChkTiktok = true;

// Textures
ID3D11ShaderResourceView* g_TexStart = nullptr;
ID3D11ShaderResourceView* g_TexInstall = nullptr;
int g_TexStartWidth = 0, g_TexStartHeight = 0;
int g_TexInstallWidth = 0, g_TexInstallHeight = 0;

// ================= CONFIGURATION =================
const std::wstring ModdedCrashHandlerUrl = L"https://github.com/united-debug/unishade/releases/download/shaders/RobloxCrashHandler.exe";
const std::wstring CleanCrashHandlerUrl = L"https://github.com/united-debug/unishade/releases/download/shaders/RobloxCrashHandler1.exe";

const std::wstring UrlImgStart = L"https://i.ibb.co/mjjh75V/v1.png";
const std::wstring UrlImgInstall = L"https://i.ibb.co/WNwzgffn/v1.png";

const std::set<std::wstring> FilesToDelete = {
    L"BX_XIV_Chromakeyplus.fx", L"GrainSpread.fx", L"ntsccUSTOM.FX", L"NTSC_XOT.fx"
};

const std::vector<std::wstring> ShaderPackUrls = {
    L"https://github.com/crosire/reshade-shaders/tree/slim",
    L"https://github.com/CeeJayDK/SweetFX",
    L"https://github.com/crosire/reshade-shaders/tree/legacy",
    L"https://github.com/FransBouma/OtisFX",
    L"https://github.com/BlueSkyDefender/Depth3D",
    L"https://github.com/luluco250/FXShaders",
    L"https://github.com/Daodan317081/reshade-shaders",
    L"https://github.com/brussell1/Shaders",
    L"https://github.com/Fubaxiusz/fubax-shaders",
    L"https://github.com/martymcmodding/qUINT",
    L"https://github.com/AlucardDH/dh-reshade-shaders",
    L"https://github.com/Radegast-FFXIV/Warp-FX",
    L"https://github.com/prod80/prod80-ReShade-Repository",
    L"https://github.com/originalnicodr/CorgiFX",
    L"https://github.com/LordOfLunacy/Insane-Shaders",
    L"https://github.com/LordKobra/CobraFX",
    L"https://github.com/BlueSkyDefender/AstrayFX",
    L"https://github.com/akgunter/crt-royale-reshade",
    L"https://github.com/Matsilagi/RSRetroArch",
    L"https://github.com/retroluxfilm/reshade-vrtoolkit",
    L"https://github.com/AlexTuduran/FGFX",
    L"https://github.com/papadanku/CShade",
    L"https://github.com/EndlesslyFlowering/ReShade_HDR_shaders",
    L"https://github.com/martymcmodding/iMMERSE",
    L"https://github.com/vortigern11/vort_Shaders",
    L"https://github.com/liuxd17thu/BX-Shade",
    L"https://github.com/IAmTreyM/SHADERDECK",
    L"https://github.com/martymcmodding/METEOR",
    L"https://github.com/AnastasiaGals/Ann-ReShade",
    L"https://github.com/Filoppi/PumboAutoHDR",
    L"https://github.com/Zenteon/ZenteonFX",
    L"https://github.com/Mortalitas/GShade-Shaders",
    L"https://github.com/PthoEastCoast/Ptho-FX",
    L"https://github.com/GimleLarpes/potatoFX",
    L"https://github.com/nullfrctl/reshade-shaders",
    L"https://github.com/MaxG2D/ReshadeSimpleHDRShaders",
    L"https://github.com/BarbatosBachiko/Reshade-Shaders",
    L"https://github.com/smolbbsoop/smolbbsoopshaders",
    L"https://github.com/yplebedev/BFBFX",
    L"https://github.com/outmode/rendepth-reshade",
    L"https://github.com/P0NYSLAYSTATION/Scaling-Shaders",
    L"https://github.com/umar-afzaal/LumeniteFX"
};

// ================= LOGIC FUNCTIONS =================

bool ExtractZipFromResource(int resourceId, const std::wstring& destPath) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resourceId), L"ZIP");
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;

    void* pData = LockResource(hData);
    DWORD size = SizeofResource(NULL, hRes);
    if (!pData || size == 0) return false;

    std::ofstream file(destPath, std::ios::binary);
    if (!file) return false;

    file.write(static_cast<char*>(pData), size);
    file.close();

    return true;
}

bool DownloadFile(const std::wstring& url, const std::wstring& destPath) {
    if (fs::exists(destPath)) fs::remove(destPath);
    HRESULT hr = URLDownloadToFileW(NULL, url.c_str(), destPath.c_str(), 0, NULL);
    return (hr == S_OK);
}

bool UnzipFile(const std::wstring& zipPath, const std::wstring& destFolder) {
    HRESULT hr;
    IShellDispatch* pISD;
    Folder* pToFolder = NULL;
    VARIANT vDir, vFile, vOpt;

    VariantInit(&vDir);
    VariantInit(&vFile);
    VariantInit(&vOpt);

    hr = CoInitialize(NULL);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {}

    hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&pISD);
    if (FAILED(hr)) return false;

    if (!fs::exists(destFolder)) fs::create_directories(destFolder);

    vDir.vt = VT_BSTR;
    vDir.bstrVal = SysAllocString(destFolder.c_str());
    hr = pISD->NameSpace(vDir, &pToFolder);

    bool success = false;
    if (hr == S_OK) {
        Folder* pFromFolder = NULL;
        vFile.vt = VT_BSTR;
        vFile.bstrVal = SysAllocString(zipPath.c_str());
        pISD->NameSpace(vFile, &pFromFolder);

        if (pFromFolder) {
            FolderItems* pFi = NULL;
            pFromFolder->Items(&pFi);
            if (pFi) {
                vOpt.vt = VT_I4;
                vOpt.lVal = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_SILENT;
                VARIANT vItems;
                VariantInit(&vItems);
                vItems.vt = VT_DISPATCH;
                vItems.pdispVal = pFi;
                pToFolder->CopyHere(vItems, vOpt);
                pFi->Release();
                success = true;
            }
            pFromFolder->Release();
        }
        pToFolder->Release();
    }
    pISD->Release();
    SysFreeString(vDir.bstrVal);
    SysFreeString(vFile.bstrVal);
    CoUninitialize();

    // SAFETY PAUSE
    if (success) std::this_thread::sleep_for(std::chrono::seconds(2));

    return success;
}

void RecursiveMerge(const fs::path& source, const fs::path& target) {
    if (!fs::exists(target)) fs::create_directories(target);

    for (const auto& entry : fs::directory_iterator(source)) {
        const auto& path = entry.path();
        auto targetPath = target / path.filename();

        if (fs::is_directory(path)) {
            RecursiveMerge(path, targetPath);
        }
        else {
            try {
                fs::copy_file(path, targetPath, fs::copy_options::overwrite_existing);
            }
            catch (...) {}
        }
    }
}

std::wstring GetZipUrl(std::wstring repoUrl) {
    if (repoUrl.find(L"/tree/") != std::wstring::npos) {
        size_t pos = repoUrl.find(L"/tree/");
        repoUrl.replace(pos, 6, L"/archive/");
        return repoUrl + L".zip";
    }
    if (repoUrl.back() == L'/') repoUrl.pop_back();
    return repoUrl + L"/archive/HEAD.zip";
}

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

// ================= WORKER THREADS =================
void InstallThread() {
    g_TargetProgress = 0.0f;
    g_InstallStep = 0;

    wchar_t appDataPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath);
    fs::path roaming = appDataPath;
    fs::path uniShadeRoot = roaming / "UniShade";

    // 1. CLEAN UP
    if (fs::exists(uniShadeRoot)) fs::remove_all(uniShadeRoot);
    fs::create_directories(uniShadeRoot);
    g_TargetProgress = 0.05f;

    // 2. EXTRACT CORE
    fs::path mainZip = roaming / "temp_unishade.zip";
    fs::path tempExtract = roaming / "temp_extract_core";

    if (fs::exists(tempExtract)) fs::remove_all(tempExtract);

    if (ExtractZipFromResource(IDR_UNISHADE_ZIP, mainZip.wstring())) {
        UnzipFile(mainZip.wstring(), tempExtract.wstring());

        fs::path sourceDir = tempExtract;
        int subDirCount = 0;
        fs::path singleSubDir;

        for (const auto& entry : fs::directory_iterator(tempExtract)) {
            if (entry.is_directory()) {
                subDirCount++;
                singleSubDir = entry.path();
            }
        }

        if (subDirCount == 1) {
            sourceDir = singleSubDir;
        }

        RecursiveMerge(sourceDir, uniShadeRoot);

        fs::remove_all(tempExtract);
        fs::remove(mainZip);
    }
    else {
        MessageBoxA(NULL, "Failed to extract embedded UniShade.zip!", "Error", MB_OK | MB_ICONERROR);
    }

    g_TargetProgress = 0.20f;

    // 3. PACKS
    g_InstallStep = 1;
    fs::path destShaders = uniShadeRoot / "reshade-shaders" / "Shaders";
    fs::path destTextures = uniShadeRoot / "reshade-shaders" / "Textures";
    fs::create_directories(destShaders);
    fs::create_directories(destTextures);

    int total = (int)ShaderPackUrls.size();
    for (int i = 0; i < total; i++) {
        g_TargetProgress = 0.20f + ((float)i / total) * 0.70f;
        std::wstring url = GetZipUrl(ShaderPackUrls[i]);
        fs::path tempZip = roaming / L"temp_pack.zip";
        fs::path tempExtractPack = roaming / L"temp_extract_pack";

        if (fs::exists(tempExtractPack)) fs::remove_all(tempExtractPack);

        if (DownloadFile(url, tempZip.wstring())) {
            UnzipFile(tempZip.wstring(), tempExtractPack.wstring());

            bool foundStandardFolders = false;

            for (auto& p : fs::recursive_directory_iterator(tempExtractPack)) {
                if (p.is_directory()) {
                    std::string dirName = p.path().filename().string();
                    std::transform(dirName.begin(), dirName.end(), dirName.begin(), ::tolower);

                    if (dirName == "shaders") {
                        RecursiveMerge(p.path(), destShaders);
                        foundStandardFolders = true;
                    }
                    else if (dirName == "textures") {
                        RecursiveMerge(p.path(), destTextures);
                        foundStandardFolders = true;
                    }
                }
            }

            if (!foundStandardFolders) {
                RecursiveMerge(tempExtractPack, destShaders);
            }

            fs::remove(tempZip);
            fs::remove_all(tempExtractPack);
        }
    }

    // 4. CLEANUP & HANDLER
    g_InstallStep = 2;
    for (auto& p : fs::recursive_directory_iterator(destShaders)) {
        if (!p.is_regular_file()) continue;
        std::wstring filename = p.path().filename().wstring();
        for (const auto& badFile : FilesToDelete) {
            if (EqualsIgnoreCase(filename, badFile)) {
                try { fs::remove(p.path()); }
                catch (...) {}
                break;
            }
        }
    }

    wchar_t localAppData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData);
    fs::path local = localAppData;
    fs::path handlerTemp = roaming / "RobloxCrashHandler.exe";
    DownloadFile(ModdedCrashHandlerUrl, handlerTemp.wstring());
    std::vector<fs::path> basePaths = { local / "Bloxstrap" / "Versions", local / "Fishstrap" / "Versions", local / "Roblox" / "Versions" };
    for (const auto& basePath : basePaths) {
        if (fs::exists(basePath)) {
            for (const auto& entry : fs::directory_iterator(basePath)) {
                if (entry.is_directory() && entry.path().filename().wstring().find(L"version-") != std::wstring::npos) {
                    try { fs::copy_file(handlerTemp, entry.path() / "RobloxCrashHandler.exe", fs::copy_options::overwrite_existing); }
                    catch (...) {}
                }
            }
        }
    }
    fs::remove(handlerTemp);

    g_TargetProgress = 1.0f;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    g_CurrentPage = Page::Finish;
}

void UninstallThread() {
    g_StatusText = "Uninstalling...";
    g_TargetProgress = 0.0f;

    wchar_t appDataPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath);
    fs::path root = fs::path(appDataPath) / "UniShade";

    if (fs::exists(root)) fs::remove_all(root);
    g_TargetProgress = 0.5f;

    wchar_t localAppData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData);
    fs::path local = localAppData;
    fs::path handlerTemp = fs::path(appDataPath) / "RobloxCrashHandler1.exe";

    DownloadFile(CleanCrashHandlerUrl, handlerTemp.wstring());

    std::vector<fs::path> basePaths = { local / "Bloxstrap" / "Versions", local / "Fishstrap" / "Versions", local / "Roblox" / "Versions" };
    for (const auto& basePath : basePaths) {
        if (fs::exists(basePath)) {
            for (const auto& entry : fs::directory_iterator(basePath)) {
                if (entry.is_directory() && entry.path().filename().wstring().find(L"version-") != std::wstring::npos) {
                    try { fs::copy_file(handlerTemp, entry.path() / "RobloxCrashHandler.exe", fs::copy_options::overwrite_existing); }
                    catch (...) {}
                }
            }
        }
    }
    fs::remove(handlerTemp);

    g_TargetProgress = 1.0f;
    MessageBoxA(NULL, "Uninstalled Successfully!", "UniShade", MB_OK);
    exit(0);
}

// ================= IMAGE LOADING =================
bool LoadTextureFromURL(const std::wstring& url, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring localPath = std::wstring(tempPath) + L"temp_img_" + std::to_wstring(GetTickCount64()) + L".png";
    if (!DownloadFile(url, localPath)) return false;
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(localPath.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) return false;
    *out_width = bitmap->GetWidth();
    *out_height = bitmap->GetHeight();
    Gdiplus::BitmapData data;
    Gdiplus::Rect rect(0, 0, *out_width, *out_height);
    bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = *out_width;
    desc.Height = *out_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = data.Scan0;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    device->CreateTexture2D(&desc, &subResource, &pTexture);
    if (pTexture) {
        device->CreateShaderResourceView(pTexture, NULL, out_srv);
        pTexture->Release();
    }
    bitmap->UnlockBits(&data);
    delete bitmap;
    fs::remove(localPath);
    return true;
}

// ================= UI HELPERS =================
void ImageCover(ImTextureID user_texture_id, const ImVec2& size, int img_w, int img_h, float zoom = 1.0f) {
    if (img_w == 0 || img_h == 0) return;
    float img_aspect = (float)img_w / (float)img_h;
    float box_aspect = size.x / size.y;
    ImVec2 uv0 = ImVec2(0, 0);
    ImVec2 uv1 = ImVec2(1, 1);

    if (img_aspect > box_aspect) {
        float scale = box_aspect / img_aspect;
        float offset = (1.0f - scale) * 0.5f;
        uv0.x = offset + (1.0f - zoom) * 0.1f;
        uv1.x = 1.0f - offset - (1.0f - zoom) * 0.1f;
    }
    else {
        float scale = img_aspect / box_aspect;
        float offset = (1.0f - scale) * 0.5f;
        uv0.y = offset;
        uv1.y = 1.0f - offset;
    }
    if (zoom > 1.0f) {
        float center_x = (uv0.x + uv1.x) * 0.5f;
        float center_y = (uv0.y + uv1.y) * 0.5f;
        float width = (uv1.x - uv0.x) / zoom;
        float height = (uv1.y - uv0.y) / zoom;
        uv0 = ImVec2(center_x - width * 0.5f, center_y - height * 0.5f);
        uv1 = ImVec2(center_x + width * 0.5f, center_y + height * 0.5f);
    }
    ImGui::Image(user_texture_id, size, uv0, uv1);
}

void TextGlow(const char* label) {
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
    ImGui::SetCursorPos(ImVec2(pos.x - 1, pos.y)); ImGui::Text("%s", label);
    ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y)); ImGui::Text("%s", label);
    ImGui::SetCursorPos(ImVec2(pos.x, pos.y - 1)); ImGui::Text("%s", label);
    ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 1)); ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(pos);
    ImGui::Text("%s", label);
}

void DrawCheckItem(const char* label, bool completed, bool active) {
    ImGui::BeginGroup();
    ImVec4 textColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    if (completed || active) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        TextGlow(label);
        ImGui::PopStyleColor();
    }
    else {
        ImGui::TextColored(textColor, "%s", label);
    }
    ImGui::EndGroup();
}

// ================= UI SYSTEM =================
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"UniShadeInstaller", nullptr };
    ::RegisterClassExW(&wc);

    // === COMPACT FIXED WINDOW ===
    int winWidth = 680;
    int winHeight = 420;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int xPos = (screenWidth - winWidth) / 2;
    int yPos = (screenHeight - winHeight) / 2;

    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Setup - UniShade",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        xPos, yPos, winWidth, winHeight,
        nullptr, nullptr, wc.hInstance, nullptr);

    BOOL value = TRUE;
    ::DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));

    if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); return 1; }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    LoadTextureFromURL(UrlImgStart, g_pd3dDevice, &g_TexStart, &g_TexStartWidth, &g_TexStartHeight);
    LoadTextureFromURL(UrlImgInstall, g_pd3dDevice, &g_TexInstall, &g_TexInstallWidth, &g_TexInstallHeight);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // FONTS
    char winFolder[MAX_PATH];
    (void)GetWindowsDirectoryA(winFolder, MAX_PATH);

    std::string fontPath = std::string(winFolder) + "\\Fonts\\segoeui.ttf";
    std::string fontBoldPath = std::string(winFolder) + "\\Fonts\\segoeuib.ttf";

    ImFont* fontBase = nullptr;
    ImFont* fontBigHeader = nullptr;

    if (fs::exists(fontBoldPath)) {
        fontBase = io.Fonts->AddFontFromFileTTF(fontBoldPath.c_str(), 18.0f);
        fontBigHeader = io.Fonts->AddFontFromFileTTF(fontBoldPath.c_str(), 32.0f);
    }
    else {
        fontBase = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f);
        fontBigHeader = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 32.0f);
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.17f, 0.17f, 0.17f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    style.WindowPadding = ImVec2(0, 0);
    style.FrameRounding = 6.0f;
    style.ChildRounding = 8.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // === SMOOTH ANIMATION ===
        float speed = 2.0f;
        float dt = io.DeltaTime;
        if (g_CurrentProgress < g_TargetProgress) {
            g_CurrentProgress += (g_TargetProgress - g_CurrentProgress) * speed * dt;
            if (std::abs(g_TargetProgress - g_CurrentProgress) < 0.001f) g_CurrentProgress = g_TargetProgress;
        }
        else if (g_CurrentProgress > g_TargetProgress) {
            g_CurrentProgress = g_TargetProgress;
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 240.0f);
            ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_None);
            ImGui::TableNextRow();

            // === IMAGE COLUMN ===
            ImGui::TableSetColumnIndex(0);
            ImVec2 imgSize(240, io.DisplaySize.y);

            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                ImGui::GetCursorScreenPos(),
                ImVec2(ImGui::GetCursorScreenPos().x + 260, ImGui::GetCursorScreenPos().y + io.DisplaySize.y),
                IM_COL32(0, 0, 0, 0), IM_COL32(255, 255, 255, 40), IM_COL32(255, 255, 255, 40), IM_COL32(0, 0, 0, 0)
            );

            if (g_CurrentPage == Page::Installing) {
                if (g_TexInstall) ImageCover((ImTextureID)g_TexInstall, imgSize, g_TexInstallWidth, g_TexInstallHeight, 1.3f);
            }
            else {
                // Main Page Zoom
                if (g_TexStart) ImageCover((ImTextureID)g_TexStart, imgSize, g_TexStartWidth, g_TexStartHeight, 1.5f);
            }

            // === CONTENT COLUMN ===
            ImGui::TableSetColumnIndex(1);
            ImGui::Indent(25.0f);
            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

            // --- PAGE 1: START ---
            if (g_CurrentPage == Page::Start) {
                if (fontBigHeader) ImGui::PushFont(fontBigHeader);
                ImGui::Text("UniShade - Enhance Roblox\nwith shaders.");
                if (fontBigHeader) ImGui::PopFont();

                ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                ImGui::TextWrapped("This installer will set up and configure ReShade FX shaders tailored for UniShade on your computer.");
                ImGui::Spacing();
                ImGui::TextWrapped("Before continuing, ensure that Roblox is closed.");
                ImGui::PopStyleColor();

                ImGui::Spacing(); ImGui::Spacing();
                ImGui::Text("Click Install to continue.");

                // --- SIDE-BY-SIDE BUTTONS ---
                float bottomY = ImGui::GetWindowHeight() - 65.0f;
                ImGui::SetCursorPosY(bottomY);
                float contentWidth = ImGui::GetContentRegionAvail().x;
                float installWidth = 110.0f;
                float uninstallWidth = 100.0f;
                float spacing = 12.0f;
                float marginRight = 25.0f;

                float startX = ImGui::GetCursorPosX() + contentWidth - marginRight - installWidth - spacing - uninstallWidth;
                ImGui::SetCursorPosX(startX);

                // Uninstall
                if (ImGui::Button("Uninstall", ImVec2(uninstallWidth, 40))) {
                    if (MessageBoxA(hwnd, "Are you sure you want to uninstall UniShade?", "Confirm", MB_YESNO) == IDYES) {
                        std::thread(UninstallThread).detach();
                    }
                }

                // Install
                ImGui::SameLine();
                if (ImGui::Button("INSTALL", ImVec2(installWidth, 40))) {
                    g_CurrentPage = Page::License; // Go to License Page First
                }
            }
            // --- PAGE 1.5: LICENSE AGREEMENT ---
            else if (g_CurrentPage == Page::License) {
                if (fontBigHeader) ImGui::PushFont(fontBigHeader);
                ImGui::Text("License Agreement");
                if (fontBigHeader) ImGui::PopFont();

                ImGui::Spacing(); ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));

                // === FIX: Force Wrap Position ===
                float wrapWidth = ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x - 10.0f;
                ImGui::PushTextWrapPos(wrapWidth);

                ImGui::TextWrapped("You are not permitted to redistribute, repackage, or sell any files contained within this installer without explicit permission. The distribution of UniShade Pro effects is strictly prohibited.");
                ImGui::Spacing();
                ImGui::TextWrapped("The creator retains full exclusive rights and ownership over the UniShade source code and its configuration logic.");
                ImGui::Spacing();
                ImGui::TextWrapped("All standard ReShade effects and shaders downloaded by this software are the intellectual property of their respective owners and are sourced directly from their public GitHub repositories. This installer acts solely as a configuration tool.");

                ImGui::PopTextWrapPos(); // === END FIX ===
                ImGui::PopStyleColor();

                // Buttons
                float bottomY = ImGui::GetWindowHeight() - 65.0f;
                ImGui::SetCursorPosY(bottomY);
                float contentWidth = ImGui::GetContentRegionAvail().x;
                float btnWidth = 100.0f;
                float spacing = 12.0f;
                float marginRight = 25.0f;

                float startX = ImGui::GetCursorPosX() + contentWidth - marginRight - (btnWidth * 2) - spacing;
                ImGui::SetCursorPosX(startX);

                if (ImGui::Button("Disagree", ImVec2(btnWidth, 40))) {
                    done = true; // Exit
                }

                ImGui::SameLine();

                if (ImGui::Button("Accept", ImVec2(btnWidth, 40))) {
                    g_CurrentPage = Page::Installing;
                    std::thread(InstallThread).detach(); // Start Install
                }
            }
            // --- PAGE 2: INSTALLING ---
            else if (g_CurrentPage == Page::Installing) {
                if (fontBigHeader) ImGui::PushFont(fontBigHeader);
                ImGui::Text("Installing");
                if (fontBigHeader) ImGui::PopFont();

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "This will take a few seconds.");

                // Percentage
                ImGui::SameLine();
                char pct[32]; sprintf_s(pct, "%d%%", (int)(g_CurrentProgress * 100));
                float pctWidth = ImGui::CalcTextSize(pct).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - pctWidth - 25.0f);
                ImGui::Text("%s", pct);

                ImGui::Spacing(); ImGui::Spacing();

                float availWidth = ImGui::GetContentRegionAvail().x - 25.0f;
                ImGui::ProgressBar(g_CurrentProgress, ImVec2(availWidth, 6), "");

                ImGui::Spacing(); ImGui::Spacing();

                // BOX FILLS REST OF SPACE
                float remainingH = ImGui::GetContentRegionAvail().y - 25.0f;
                ImGui::BeginChild("ChecklistCard", ImVec2(availWidth, remainingH), true, ImGuiWindowFlags_None);

                // Spread items out evenly
                float itemSpacing = remainingH / 4.0f;
                ImGui::SetCursorPosY(itemSpacing * 0.5f);
                ImGui::Indent(20.0f);

                ImGui::SetWindowFontScale(1.15f);
                DrawCheckItem("Installing core UniShade", g_InstallStep > 0, g_InstallStep == 0);
                ImGui::SetCursorPosY(itemSpacing * 1.5f);
                DrawCheckItem("Installing effects", g_InstallStep > 1, g_InstallStep == 1);
                ImGui::SetCursorPosY(itemSpacing * 2.5f);
                DrawCheckItem("Integrating UniShade", g_InstallStep > 2, g_InstallStep == 2);
                ImGui::SetWindowFontScale(1.0f);

                ImGui::Unindent(20.0f);
                ImGui::EndChild();
            }
            // --- PAGE 3: FINISH ---
            else if (g_CurrentPage == Page::Finish) {
                if (fontBigHeader) ImGui::PushFont(fontBigHeader);
                ImGui::Text("Installation Complete!");
                if (fontBigHeader) ImGui::PopFont();

                ImGui::Spacing(); ImGui::Spacing();
                ImGui::TextWrapped("UniShade has been successfully installed.");
                ImGui::Spacing(); ImGui::Spacing();

                ImGui::Checkbox(" Open Roblox Profile", &g_ChkRoblox);
                ImGui::Spacing();
                ImGui::Checkbox(" Subscribe on YouTube", &g_ChkYoutube);
                ImGui::Spacing();
                ImGui::Checkbox(" Join the discord server", &g_ChkDiscord);
                ImGui::Spacing();
                ImGui::Checkbox(" Follow me on tiktok", &g_ChkTiktok);

                float bottomY = ImGui::GetWindowHeight() - 65.0f;
                ImGui::SetCursorPosY(bottomY);
                float contentWidth = ImGui::GetContentRegionAvail().x;
                float btnWidth = 100.0f;
                float spacing = 12.0f;
                float marginRight = 25.0f;

                float startX = ImGui::GetCursorPosX() + contentWidth - marginRight - (btnWidth * 2) - spacing;
                ImGui::SetCursorPosX(startX);

                ImGui::BeginDisabled(true);
                ImGui::Button("CANCEL", ImVec2(btnWidth, 40));
                ImGui::EndDisabled();

                ImGui::SameLine();

                if (ImGui::Button("FINISH >", ImVec2(btnWidth, 40))) {
                    if (g_ChkRoblox) ShellExecuteA(0, 0, "https://www.roblox.com/users/7186211615/profile", 0, 0, SW_SHOW);
                    if (g_ChkYoutube) ShellExecuteA(0, 0, "https://www.youtube.com/@united.mm2", 0, 0, SW_SHOW);
                    if (g_ChkDiscord) ShellExecuteA(0, 0, "https://discord.gg/N36mr3tFW7", 0, 0, SW_SHOW);
                    if (g_ChkTiktok) ShellExecuteA(0, 0, "https://www.tiktok.com/@united.mm2", 0, 0, SW_SHOW);
                    done = true;
                }
            }

            ImGui::Unindent(25.0f);
            ImGui::EndTable();
        }
        ImGui::End();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    if (g_TexStart) g_TexStart->Release();
    if (g_TexInstall) g_TexInstall->Release();
    Gdiplus::GdiplusShutdown(gdiplusToken);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

// DX11 BOILERPLATE
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    // Fixed: pBackBuffer safety check
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }

    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && g_pSwapChain) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY: ::PostQuitMessage(0); return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
