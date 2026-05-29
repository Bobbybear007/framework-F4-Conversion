#pragma once

#include <MinHook.h>
#include <dxgi.h>

namespace Hooks {
    struct D3DHooks {
        using PresentFunc       = HRESULT(APIENTRY*)(IDXGISwapChain*, UINT, UINT);
        using ResizeBuffersFunc = HRESULT(APIENTRY*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

        static inline PresentFunc       oPresent       = nullptr;
        static inline ResizeBuffersFunc oResizeBuffers = nullptr;

        static HRESULT APIENTRY HookPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        static HRESULT APIENTRY HookResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                   UINT Width, UINT Height,
                                                   DXGI_FORMAT NewFormat, UINT SwapChainFlags);

        static void Install();
        static void Uninstall();
    };
}
