// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <wrl.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "Globals.hpp"
#include "Renderer.hpp"

#include "D3D11Shaders.hpp"

namespace VarjoExamples
{
//! Renderer implementation for D3D11 graphics
class D3D11Renderer final : public Renderer
{
public:
    //! Color render target implementation for D3D11 renderer
    class ColorRenderTarget final : public Renderer::RenderTarget
    {
    public:
        //! Construct render target instance using given textures
        ColorRenderTarget(ID3D11Device* device, int32_t width, int32_t height, ID3D11Texture2D* colorTexture);

        //! Returns render target view for color
        ID3D11RenderTargetView* getRenderTargetView() const { return m_renderTargetView.Get(); }

    private:
        ComPtr<ID3D11RenderTargetView> m_renderTargetView;  //!< Render target view instance
    };

    //! Depth render target implementation for D3D11 renderer
    class DepthRenderTarget final : public Renderer::RenderTarget
    {
    public:
        //! Construct render target instance using given textures
        DepthRenderTarget(ID3D11Device* device, int32_t width, int32_t height, ID3D11Texture2D* depthTexture);

        //! Returns render target depth stencil view
        ID3D11DepthStencilView* getDepthStencilView() const { return m_depthStencilView.Get(); }

    private:
        ComPtr<ID3D11DepthStencilView> m_depthStencilView;  //!< Depth stencil view instance
    };

    //! Vertex data for mesh geometry
    struct Geometry {
        ComPtr<ID3D11Buffer> vertexBuffer;     //!< Vertex data buffer
        ComPtr<ID3D11Buffer> indexBuffer;      //!< Index data buffer
        int vertexStride = 0;                  //!< Vertex stride
        int indexCount = 0;                    //!< Index count
        Renderer::PrimitiveTopology topology;  //!< Primitive topology
    };

    //! Shader implementation for D3D11 renderer
    class Shader final : public Renderer::Shader
    {
    public:
        //! Shader init parameters structure
        struct InitParams {
            std::string name;                                     //!< Shader name
            const char* vsSource = nullptr;                       //!< Vertex shader source code
            const char* psSource = nullptr;                       //!< Pixel shader source code
            UINT vsConstantBufferSize = 0;                        //!< Vertex shader constants size
            UINT psConstantBufferSize = 0;                        //!< Pixel shader constants size
            std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements;  //!< Input elements
        };

        ComPtr<ID3D11InputLayout> inputLayout;    //!< Input data layout
        ComPtr<ID3D11VertexShader> vertexShader;  //!< Vertex shader binding
        ComPtr<ID3D11PixelShader> pixelShader;    //!< Pixel shader binding
        ComPtr<ID3D11Buffer> vsConstantBuffer;    //!< Constant data buffer for vertex shader
        ComPtr<ID3D11Buffer> psConstantBuffer;    //!< Constant data buffer for pixel shader
    };

    //! Texture implementation for D3D11 renderer
    class Texture final : public Renderer::Texture
    {
    public:
        ComPtr<ID3D11Texture2D> texture;         //!< GPU texture
        ComPtr<ID3D11Texture2D> stagingTexture;  //!< CPU staging texture
        ComPtr<ID3D11SamplerState> sampler;      //!< Texture sampler state
        ComPtr<ID3D11ShaderResourceView> srv;    //!< Shader resource view
    };

    //! Mesh implementation for D3D11 renderer
    class Mesh final : public Renderer::Mesh
    {
    public:
        //! Construct scene object
        Mesh(ID3D11Device* device, const std::vector<float>& vertexData, int vertexStride, const std::vector<unsigned int>& indexData,
            Renderer::PrimitiveTopology topology);

        //! Construct dynamic scene object
        Mesh(ID3D11Device* device, int maxVertexCount, int vertexStride, int maxIndexCount, Renderer::PrimitiveTopology topology);

        //! Update geometry
        void updateGeometry(ID3D11DeviceContext* deviceContext, const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData);

        //! Render object
        void render(ID3D11DeviceContext* context);

    private:
        D3D11Renderer::Geometry m_geometry;  //!< Geometry mesh for rendering object
    };

    //! Construct D3D11 renderer instance
    D3D11Renderer(IDXGIAdapter* adapter);

    //! Create mesh object of given vertex data
    std::unique_ptr<Renderer::Mesh> createMesh(
        const std::vector<float>& vertexData, int vertexStride, const std::vector<unsigned int>& indexData, PrimitiveTopology topology) override;

    //! Create a dynamic mesh object
    std::unique_ptr<Renderer::Mesh> createDynamicMesh(int maxVertexCount, int vertexStride, int maxIndexCount, PrimitiveTopology topology) override;

    //! Update dynamic mesh object with given vertex data
    void updateMesh(Renderer::Mesh& mesh, const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData) override;

    //! Load a texture memory.
    std::unique_ptr<Renderer::Texture> loadTextureFromMemory(const uint8_t* memory, size_t size) override;

    //! Create an HDR cubemap texture.
    std::unique_ptr<Renderer::Texture> createHdrCubemap(int32_t resolution, varjo_TextureFormat format);

    //! Create shader from given data
    std::unique_ptr<Renderer::Shader> createShader(const Shader::InitParams& params);

    //! Updates a texture with new data.
    void updateTexture(Renderer::Texture* texture, const uint8_t* data, size_t rowPitch);

    //! Render mesh with given constants
    void renderMesh(Renderer::Mesh& mesh, const void* vsConstants, size_t vsConstantsSize, const void* psConstants, size_t psConstantsSize) override;

    //! Set depth enabled
    void setDepthEnabled(bool enabled) override;

    //! Bind given render target
    void bindRenderTarget(Renderer::ColorDepthRenderTarget& target) override;

    //! Unbind currently bound render target
    void unbindRenderTarget() override;

    //! Bind a shader
    void bindShader(Renderer::Shader& shader) override;

    //! Bind textures
    void bindTextures(const std::vector<Renderer::Texture*> textures) override;

    //! Set viewport dimensions
    void setViewport(int32_t x, int32_t y, int32_t width, int32_t height) override;

    //! Clear given entire render target
    void clear(Renderer::ColorDepthRenderTarget& target, const glm::vec4& colorValue, bool clearColor, bool clearDepth, bool clearStencil, float depthValue,
        uint8_t stencilValue) override;

    //! Returns D3D11 device pointer
    ID3D11Device* getD3DDevice() const;

    //! Return example shader library reference
    const ExampleShaders& getShaders() const override { return m_exampleShaders; }

    //! Compile shader from source data
    static ComPtr<ID3DBlob> compileShader(const std::string& name, const char* sourceData, const char* target);

    //! Create vertex shader instance from a compiled blob
    static ComPtr<ID3D11VertexShader> createVertexShader(ID3D11Device* device, ID3DBlob* blob);

    //! Create pixel shader instance from a compiled blob
    static ComPtr<ID3D11PixelShader> createPixelShader(ID3D11Device* device, ID3DBlob* blob);

    //! Create compute shader instance from a compiled blob
    static ComPtr<ID3D11ComputeShader> createComputeShader(ID3D11Device* device, ID3DBlob* blob);

private:
    ComPtr<ID3D11Device> m_device;                          //!< D3D11 device instance
    ComPtr<ID3D11DeviceContext> m_context;                  //!< D3D11 device context instance
    ComPtr<ID3D11DepthStencilState> m_defaultDepthStencil;  //!< Default depth stencil state
    ComPtr<ID3D11DepthStencilState> m_depthDisableState;    //!< Depth stencil state with depth testing and writing disabled
    ComPtr<ID3D11Buffer> m_vsConstantBuffer;                //!< Current vertex shader constant buffer
    ComPtr<ID3D11Buffer> m_psConstantBuffer;                //!< Current pixel shader constant buffer
    D3D11ExampleShaders m_exampleShaders;                   //!< Example shaers library
};

}  // namespace VarjoExamples
