// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <d3d11.h>

#include "Globals.hpp"

namespace VarjoExamples
{
//! Wrapper class for graphics contexts used in this example.
class GfxContext
{
public:
    //! Constructor
    GfxContext(HWND hwnd);

    //! Destructor
    ~GfxContext() = default;

    //! Initialize gfx sessions
    virtual void init(IDXGIAdapter* adapter);

    //! Returns D3D11 device
    virtual ComPtr<ID3D11Device> getD3D11Device() const { return m_d3d11Device; }

    //! Return device context handle
    HDC getDC() const { return m_hdc; }

private:
    //! Initialize D3D11
    void initD3D11(IDXGIAdapter* adapter);

private:
    HWND m_hwnd{NULL};                                 //!< Window handle
    HDC m_hdc{NULL};                                   //!< Device context handle
    ComPtr<ID3D11Device> m_d3d11Device;                //!< D3D11 device
    ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;  //!< D3D11 device context
};

}  // namespace VarjoExamples