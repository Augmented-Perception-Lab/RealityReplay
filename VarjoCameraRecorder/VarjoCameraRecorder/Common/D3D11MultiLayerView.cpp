// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#include "D3D11MultiLayerView.hpp"

#include <Varjo_d3d11.h>

namespace VarjoExamples
{
D3D11MultiLayerView::D3D11Layer::D3D11Layer(D3D11MultiLayerView& view, const D3D11Layer::Config& config)
    : MultiLayerView::Layer(view, config.contextDivider, config.focusDivider)
    , m_d3d11View(view)
{
    // Create color texture swap chain for DX11
    varjo_SwapChainConfig2 colorConfig{};
    colorConfig.numberOfTextures = 4;
    colorConfig.textureArraySize = 1;
    colorConfig.textureFormat = config.format;

    const auto totalSize = getTotalSize(m_viewports);
    colorConfig.textureWidth = static_cast<int32_t>(totalSize.x);
    colorConfig.textureHeight = static_cast<int32_t>(totalSize.y);

    m_colorSwapChain = varjo_D3D11CreateSwapChain(m_view.getSession(), m_d3d11View.m_d3d11Renderer.getD3DDevice(), &colorConfig);
    CHECK_VARJO_ERR(m_view.getSession());

    // Create a DX11 render target for each color swap chain texture
    for (int i = 0; i < colorConfig.numberOfTextures; ++i) {
        // Get swap chain textures for render target
        const varjo_Texture colorTexture = varjo_GetSwapChainImage(m_colorSwapChain, i);
        CHECK_VARJO_ERR(m_view.getSession());

        // Create render target instance
        m_colorRenderTargets.emplace_back(std::make_unique<D3D11Renderer::ColorRenderTarget>(
            m_d3d11View.m_d3d11Renderer.getD3DDevice(), colorConfig.textureWidth, colorConfig.textureHeight, varjo_ToD3D11Texture(colorTexture)));
    }

    // Create depth texture swap chain for DX11
    varjo_SwapChainConfig2 depthConfig{colorConfig};
    depthConfig.textureFormat = varjo_DepthTextureFormat_D32_FLOAT;

    m_depthSwapChain = varjo_D3D11CreateSwapChain(m_view.getSession(), m_d3d11View.m_d3d11Renderer.getD3DDevice(), &depthConfig);
    CHECK_VARJO_ERR(m_view.getSession());

    // Create a DX11 render target for each depth swap chain texture
    for (int i = 0; i < depthConfig.numberOfTextures; ++i) {
        const varjo_Texture depthTexture = varjo_GetSwapChainImage(m_depthSwapChain, i);
        CHECK_VARJO_ERR(m_view.getSession());

        // Create render target instance
        m_depthRenderTargets.emplace_back(std::make_unique<D3D11Renderer::DepthRenderTarget>(
            m_d3d11View.m_d3d11Renderer.getD3DDevice(), depthConfig.textureWidth, depthConfig.textureHeight, varjo_ToD3D11Texture(depthTexture)));
    }

    // Setup views
    setupViews();
}

D3D11MultiLayerView::D3D11Layer::~D3D11Layer() = default;

// --------------------------------------------------------------------------

D3D11MultiLayerView::D3D11MultiLayerView(varjo_Session* session, D3D11Renderer& renderer)
    : MultiLayerView(session, renderer)
    , m_d3d11Renderer(renderer)
{
    // Create layer instance
    D3D11Layer::Config layerConfig{};
    m_layers.emplace_back(std::make_unique<D3D11Layer>(*this, layerConfig));
}

D3D11MultiLayerView::D3D11MultiLayerView(varjo_Session* session, D3D11Renderer& renderer, const D3D11Layer::Config& layerConfig)
    : MultiLayerView(session, renderer)
    , m_d3d11Renderer(renderer)
{
    // Create layer instance
    m_layers.emplace_back(std::make_unique<D3D11Layer>(*this, layerConfig));
}

D3D11MultiLayerView::D3D11MultiLayerView(varjo_Session* session, D3D11Renderer& renderer, const std::vector<D3D11Layer::Config>& layerConfigs)
    : MultiLayerView(session, renderer)
    , m_d3d11Renderer(renderer)
{
    // Create layer instances
    for (const auto& layerConfig : layerConfigs) {
        m_layers.emplace_back(std::make_unique<D3D11Layer>(*this, layerConfig));
    }
}

D3D11MultiLayerView::~D3D11MultiLayerView() = default;

ComPtr<IDXGIAdapter> D3D11MultiLayerView::getAdapter(varjo_Session* session)
{
    varjo_Luid luid = varjo_D3D11GetLuid(session);

    ComPtr<IDXGIFactory> factory = nullptr;
    const HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) {
        UINT i = 0;
        while (true) {
            ComPtr<IDXGIAdapter> adapter = nullptr;
            if (factory->EnumAdapters(i++, &adapter) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc)) && desc.AdapterLuid.HighPart == luid.high && desc.AdapterLuid.LowPart == luid.low) {
                return adapter;
            }
        }
    }

    LOG_WARNING("Could not get DXGI adapter.");
    return nullptr;
}

}  // namespace VarjoExamples
