#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace GGST {

class DX11Hook {
public:
    static bool Initialize();
    static void Shutdown();

    static bool IsMenuVisible() { return s_ShowMenu; }
    static void SetMenuVisible(bool visible) { s_ShowMenu = visible; }

    static bool IsWaitingForHotkey() { return s_WaitingForHotkey; }
    static void StartWaitingForHotkey() { s_WaitingForHotkey = true; }
    static void CancelWaitingForHotkey() { s_WaitingForHotkey = false; }

    static HWND GetGameWindow() { return s_Hwnd; }

private:
    static HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    static HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static bool InitD3D11(IDXGISwapChain* pSwapChain);
    static void CleanupRenderTarget();
    static void CreateRenderTarget(IDXGISwapChain* pSwapChain);

    static bool s_Initialized;
    static bool s_ShowMenu;
    static bool s_WaitingForHotkey;
    static HWND s_Hwnd;
    static WNDPROC s_OriginalWndProc;

    static ID3D11Device* s_Device;
    static ID3D11DeviceContext* s_Context;
    static ID3D11RenderTargetView* s_RenderTargetView;

    typedef HRESULT(WINAPI* PresentFn)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(WINAPI* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    static PresentFn s_OriginalPresent;
    static ResizeBuffersFn s_OriginalResizeBuffers;
};

} // namespace GGST
