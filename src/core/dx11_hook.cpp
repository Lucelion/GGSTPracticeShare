#include "dx11_hook.h"
#include "config.h"
#include "../game/game_memory.h"
#include "../ui/ui.h"
#include "../vendor/minhook/include/MinHook.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/backends/imgui_impl_win32.h"
#include "../vendor/imgui/backends/imgui_impl_dx11.h"
#include <imm.h>
#include <cctype>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace GGST {

bool DX11Hook::s_Initialized = false;
bool DX11Hook::s_ShowMenu = false; // 기본 닫힘 상태 (설정한 단축키 기본 F11 입력 시 토글)
bool DX11Hook::s_WaitingForHotkey = false;
HWND DX11Hook::s_Hwnd = nullptr;
WNDPROC DX11Hook::s_OriginalWndProc = nullptr;

ID3D11Device* DX11Hook::s_Device = nullptr;
ID3D11DeviceContext* DX11Hook::s_Context = nullptr;
ID3D11RenderTargetView* DX11Hook::s_RenderTargetView = nullptr;

DX11Hook::PresentFn DX11Hook::s_OriginalPresent = nullptr;
DX11Hook::ResizeBuffersFn DX11Hook::s_OriginalResizeBuffers = nullptr;

void DX11Hook::CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
        s_Device->CreateRenderTargetView(pBackBuffer, NULL, &s_RenderTargetView);
        pBackBuffer->Release();
    }
}

void DX11Hook::CleanupRenderTarget() {
    if (s_RenderTargetView) {
        s_RenderTargetView->Release();
        s_RenderTargetView = nullptr;
    }
}

bool DX11Hook::InitD3D11(IDXGISwapChain* pSwapChain) {
    if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&s_Device))) {
        return false;
    }
    s_Device->GetImmediateContext(&s_Context);

    DXGI_SWAP_CHAIN_DESC desc;
    pSwapChain->GetDesc(&desc);
    s_Hwnd = desc.OutputWindow;

    CreateRenderTarget(pSwapChain);

    // WndProc 후킹
    s_OriginalWndProc = (WNDPROC)SetWindowLongPtr(s_Hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);

    // ImGui 초기화
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // UI 모듈 초기화 (스타일 및 폰트)
    UI::Initialize();

    ImGui_ImplWin32_Init(s_Hwnd);
    ImGui_ImplDX11_Init(s_Device, s_Context);

    s_Initialized = true;
    return true;
}

LRESULT CALLBACK DX11Hook::HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // 1. 단축키 변경 대기 중인 경우
    if (s_WaitingForHotkey && (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)) {
        DWORD vk = (DWORD)wParam;
        if (vk == VK_PROCESSKEY) {
            vk = ImmGetVirtualKey(hWnd);
        }
        if (vk == VK_ESCAPE) {
            s_WaitingForHotkey = false;
        } else if (vk != 0 && vk != VK_HANGUL && vk != VK_RMENU && vk != VK_MENU) {
            Config::SetToggleKey(vk);
            s_WaitingForHotkey = false;
        }
        return 1; // 입력 소비
    }

    // 2. 오버레이 토글 키 감지
    if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
        DWORD vk = (wParam == VK_PROCESSKEY) ? ImmGetVirtualKey(hWnd) : (DWORD)wParam;
        if (vk == Config::GetToggleKey() && !(lParam & 0x40000000)) {
            s_ShowMenu = !s_ShowMenu;
            UI::ResetHangulState();
            if (s_ShowMenu) {
                // OS IME가 키 입력을 가로채서 삼키는 현상을 방지하고 내장 두벌식 오토마타가 직접 처리하도록 설정
                ImmAssociateContext(hWnd, nullptr);
            }
            return 1; // 게임에 토글 키 입력 전달 방지
        }
    }

    // 3. ImGui 입력 및 내장 한글 오토마타 처리
    if (s_ShowMenu) {
        // OS IME 컨텍스트 분리 유지 (항상 깨끗한 WM_KEYDOWN / WM_CHAR 보장)
        if (ImmGetContext(hWnd) != nullptr) {
            ImmAssociateContext(hWnd, nullptr);
        }

        // 마우스 클릭 시 현재 조합 중인 한글 음절 확정
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN) {
            UI::ResetHangulState();
        }

        // 키보드 다운 이벤트 처리 (한/영 전환, 조합 중 백스페이스, 커서 이동 키 등)
        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
            bool isRepeat = (lParam & 0x40000000) != 0;
            DWORD realVk = (wParam == VK_PROCESSKEY) ? ImmGetVirtualKey(hWnd) : (DWORD)wParam;

            // 한/영 키 (VK_HANGUL, 우측 Alt, Shift+Space) 감지
            bool isHangulToggle = (realVk == VK_HANGUL) ||
                                  (realVk == VK_RMENU) ||
                                  (realVk == VK_MENU && (HIWORD(lParam) & KF_EXTENDED)) ||
                                  (realVk == VK_SPACE && (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            if (isHangulToggle) {
                if (!isRepeat) {
                    UI::ToggleKoreanMode();
                }
                return 1;
            }

            // 한글 조합 중 백스페이스 입력 시 자모 단위로 복원
            if (realVk == VK_BACK && UI::IsKoreanMode() && UI::IsHangulComposing()) {
                UI::OnHangulBackspace();
                return 1;
            }

            // 방향키, 엔터, 탭, 홈/엔드, 삭제 등 입력 시 현재 조합 중인 한글 확정
            if (realVk == VK_LEFT || realVk == VK_RIGHT || realVk == VK_UP || realVk == VK_DOWN ||
                realVk == VK_HOME || realVk == VK_END || realVk == VK_DELETE ||
                realVk == VK_RETURN || realVk == VK_TAB || realVk == VK_ESCAPE ||
                realVk == VK_PRIOR || realVk == VK_NEXT) {
                UI::ResetHangulState();
            }

            // 만약 OS IME가 첫 키를 VK_PROCESSKEY로 가로챈 경우에도 즉시 복원하여 내장 오토마타로 전달
            if (wParam == VK_PROCESSKEY && realVk >= 'A' && realVk <= 'Z' &&
                (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_MENU) & 0x8000) == 0) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                char ch = shift ? (char)realVk : (char)std::tolower((int)realVk);
                if (UI::IsKoreanMode()) {
                    UI::OnHangulChar(ch);
                } else {
                    ImGui::GetIO().AddInputCharacter((unsigned int)ch);
                }
                return 1;
            }
        }

        // 문자 입력(WM_CHAR) 시 한글 모드면 내장 두벌식 오토마타로 실시간 조합
        if (uMsg == WM_CHAR) {
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_MENU) & 0x8000) == 0) {
                if (UI::IsKoreanMode() && ((wParam >= 'a' && wParam <= 'z') || (wParam >= 'A' && wParam <= 'Z'))) {
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    char ch = (char)std::tolower((int)wParam);
                    if (shift) ch = (char)std::toupper((int)ch);
                    UI::OnHangulChar(ch);
                    return 1;
                } else if (wParam >= 0x20) {
                    // 공백, 숫자, 특수문자 입력 시 기존 한글 조합을 확정하고 그대로 뒤이어 입력
                    UI::ResetHangulState();
                }
            }
        }

        // 잔여 IME 메시지 차단
        if (uMsg == WM_IME_COMPOSITION || uMsg == WM_IME_STARTCOMPOSITION ||
            uMsg == WM_IME_ENDCOMPOSITION || uMsg == WM_IME_CHAR) {
            return 1;
        }

        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        ImGuiIO& io = ImGui::GetIO();
        // 메뉴가 켜져 있을 때 ImGui 위젯 위에서 마우스 클릭이나 키보드 입력 시 게임 전달 차단
        if (io.WantCaptureMouse && (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)) {
            return 1;
        }
        if (io.WantCaptureKeyboard && (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)) {
            return 1;
        }
    }

    return CallWindowProc(s_OriginalWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT WINAPI DX11Hook::HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!s_Initialized) {
        if (!InitD3D11(pSwapChain)) {
            return s_OriginalPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    static bool s_WasInPractice = false;
    bool inPractice = GameMemory::IsPracticeMode();
    if (s_WasInPractice && !inPractice) {
        // 프랙티스 모드 종료 시 오버레이 및 키 바인딩 대기를 즉시 닫아 다른 모드 간섭 원천 차단
        s_ShowMenu = false;
        s_WaitingForHotkey = false;
    }
    s_WasInPractice = inPractice;

    if (s_ShowMenu && s_RenderTargetView) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::GetIO().MouseDrawCursor = s_ShowMenu;

        // UI 메인 렌더링 호출
        UI::Render();

        ImGui::Render();
        s_Context->OMSetRenderTargets(1, &s_RenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return s_OriginalPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT WINAPI DX11Hook::HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    CleanupRenderTarget();
    HRESULT hr = s_OriginalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    if (s_Device) {
        CreateRenderTarget(pSwapChain);
    }
    return hr;
}

bool DX11Hook::Initialize() {
    if (MH_Initialize() != MH_OK) {
        return false;
    }

    // 더미 윈도우 및 더미 SwapChain 생성으로 VTable 주소 구하기
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0L, 0L, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL, L"GGSTDummyDX", NULL };
    RegisterClassExW(&wc);
    HWND dummyHwnd = CreateWindowExW(0, wc.lpszClassName, L"GGSTDummyDX", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = dummyHwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    ID3D11Device* dummyDevice = nullptr;
    ID3D11DeviceContext* dummyContext = nullptr;
    IDXGISwapChain* dummySwapChain = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        featureLevels,
        2,
        D3D11_SDK_VERSION,
        &scd,
        &dummySwapChain,
        &dummyDevice,
        &featureLevel,
        &dummyContext
    );

    if (FAILED(hr)) {
        DestroyWindow(dummyHwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** vtbl = *(void***)dummySwapChain;
    void* presentAddr = vtbl[8];
    void* resizeBuffersAddr = vtbl[13];

    MH_CreateHook(presentAddr, &HookedPresent, reinterpret_cast<void**>(&s_OriginalPresent));
    MH_CreateHook(resizeBuffersAddr, &HookedResizeBuffers, reinterpret_cast<void**>(&s_OriginalResizeBuffers));

    MH_EnableHook(MH_ALL_HOOKS);

    dummySwapChain->Release();
    dummyContext->Release();
    dummyDevice->Release();
    DestroyWindow(dummyHwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return true;
}

void DX11Hook::Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (s_OriginalWndProc && s_Hwnd) {
        SetWindowLongPtr(s_Hwnd, GWLP_WNDPROC, (LONG_PTR)s_OriginalWndProc);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupRenderTarget();

    if (s_Context) { s_Context->Release(); s_Context = nullptr; }
    if (s_Device) { s_Device->Release(); s_Device = nullptr; }
}

} // namespace GGST
