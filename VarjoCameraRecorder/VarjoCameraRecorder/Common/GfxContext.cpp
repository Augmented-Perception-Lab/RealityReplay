// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#include "GfxContext.hpp"
#include "Globals.hpp"

namespace VarjoExamples
{
GfxContext::GfxContext(HWND hwnd)
{
    LOG_INFO("Initializing graphics..");

    // Initializing window
    m_hwnd = hwnd;
    m_hdc = GetDC(m_hwnd);
    if (m_hdc == NULL) {
        CRITICAL("Failed to get DC.");
    }
}

void GfxContext::init(IDXGIAdapter* adapter)
{
    // Initialize D3D11 context
    initD3D11(adapter);
}

void GfxContext::initD3D11(IDXGIAdapter* adapter)
{
    LOG_INFO("Initializing D3D11 context..");

    HRESULT hr = D3D11CreateDevice(adapter, adapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &m_d3d11Device, nullptr, &m_d3d11DeviceContext);
    if (FAILED(hr)) {
        CRITICAL("Creating D3D11 device failed.");
    }
}

}  // namespace VarjoExamples