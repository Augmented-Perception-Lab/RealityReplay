// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <vector>
#include <d3d11.h>

#include "ExampleShaders.hpp"

namespace VarjoExamples
{
class D3D11Renderer;

//! D3D11 example shaders
class D3D11ExampleShaders final : public ExampleShaders
{
public:
    //! Constructor
    D3D11ExampleShaders(D3D11Renderer& d3d11Renderer)
        : m_d3d11Renderer(d3d11Renderer)
    {
    }

    //! Create a shader of given type
    std::unique_ptr<Renderer::Shader> createShader(ExampleShaders::ShaderType type) const override;

    //! Static helper to create a shader of give type
    static std::unique_ptr<Renderer::Shader> createShader(Renderer& renderer, ExampleShaders::ShaderType type);

private:
    D3D11Renderer& m_d3d11Renderer;  //!< D3D11 renderer reference
};

}  // namespace VarjoExamples
