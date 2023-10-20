// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#pragma once

#include "Globals.hpp"
#include "MultiLayerView.hpp"
#include "D3D11Renderer.hpp"

namespace VarjoExamples
{
class D3D11MultiLayerView;

//! Layer view implementation for D3D11 renderer
class D3D11MultiLayerView final : public MultiLayerView
{
public:
    // Layer implementation for D3D11 renderer
    class D3D11Layer final : public MultiLayerView::Layer
    {
    public:
        //! Layer configuration structure
        struct Config {
            int contextDivider{1};                                          //!< Context texture size divider from full view size
            int focusDivider{1};                                            //!< Context texture size divider from full view size
            varjo_TextureFormat format{varjo_TextureFormat_R8G8B8A8_SRGB};  //!< Layer texture buffer format
        };

        //! Constructor
        D3D11Layer(D3D11MultiLayerView& view, const Config& config);

        //! Destructor
        ~D3D11Layer();

    private:
        D3D11MultiLayerView& m_d3d11View;  //!< D3D11 view multi layer view instance
    };

    //! Constructor
    D3D11MultiLayerView(varjo_Session* session, D3D11Renderer& renderer);

    //! Constructor with layer config
    D3D11MultiLayerView(varjo_Session* session, D3D11Renderer& renderer, const D3D11Layer::Config& layerConfig);

    //! Constructor with configs for multiple layers
    D3D11MultiLayerView(varjo_Session* session, D3D11Renderer& renderer, const std::vector<D3D11Layer::Config>& layerConfigs);

    //! Destructor
    ~D3D11MultiLayerView();

    //! Static function for getting DXGI adapter used by Varjo compositor
    static ComPtr<IDXGIAdapter> getAdapter(varjo_Session* session);

private:
    D3D11Renderer& m_d3d11Renderer;  //!< D3D11 renderer instance
};

}  // namespace VarjoExamples