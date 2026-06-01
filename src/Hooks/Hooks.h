#pragma once

#include <MinHook.h>
#include <REX/W32/DXGI.h>

namespace Hooks {
    struct D3DHooks {
        using PresentFunc       = HRESULT(APIENTRY*)(REX::W32::IDXGISwapChain*, UINT, UINT);
        using ResizeBuffersFunc = HRESULT(APIENTRY*)(REX::W32::IDXGISwapChain*, UINT, UINT, UINT, REX::W32::DXGI_FORMAT, UINT);

        static inline PresentFunc       oPresent       = nullptr;
        static inline ResizeBuffersFunc oResizeBuffers = nullptr;

        static HRESULT APIENTRY HookPresent(REX::W32::IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        static HRESULT APIENTRY HookResizeBuffers(REX::W32::IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                   UINT Width, UINT Height,
                                                   REX::W32::DXGI_FORMAT NewFormat, UINT SwapChainFlags);

        static void Install();
        static void Uninstall();
    };
}
