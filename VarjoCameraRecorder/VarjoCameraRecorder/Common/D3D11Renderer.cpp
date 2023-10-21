// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#include "D3D11Renderer.hpp"

#include <array>
#include <fstream>
#include <Varjo_d3d11.h>

#include <wincodec.h>

namespace
{
using namespace VarjoExamples;

// Create hardware buffer
void createBuffer(ID3D11Device* device, const void* bufdata, UINT datasize, UINT bindFlags, bool dynamic, ID3D11Buffer** buf)
{
    CD3D11_BUFFER_DESC vDesc(datasize, bindFlags, dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT, dynamic ? D3D11_CPU_ACCESS_WRITE : 0);
    D3D11_SUBRESOURCE_DATA subresourceData{};
    subresourceData.pSysMem = bufdata;
    subresourceData.SysMemPitch = 0;
    subresourceData.SysMemSlicePitch = 0;
    auto hr = device->CreateBuffer(&vDesc, bufdata ? &subresourceData : nullptr, buf);
    CHECK_HRESULT(hr);
}

// Initialize geometry buffers
void initStaticGeometry(ID3D11Device* device, const std::vector<float>& vertexData, int vertexStride, const std::vector<unsigned int>& indexData,
    Renderer::PrimitiveTopology topology, D3D11Renderer::Geometry& geometry)
{
    geometry.vertexStride = vertexStride;
    geometry.topology = topology;
    geometry.indexCount = static_cast<int>(indexData.size());
    createBuffer(device, vertexData.data(), static_cast<int>(sizeof(float) * vertexData.size()), D3D11_BIND_VERTEX_BUFFER, false, &geometry.vertexBuffer);
    createBuffer(device, indexData.data(), static_cast<int>(sizeof(unsigned int) * indexData.size()), D3D11_BIND_INDEX_BUFFER, false, &geometry.indexBuffer);
}

void initDynamicGeometry(
    ID3D11Device* device, int maxVertexCount, int vertexStride, int maxIndexCount, Renderer::PrimitiveTopology topology, D3D11Renderer::Geometry& geometry)
{
    geometry.vertexStride = vertexStride;
    geometry.topology = topology;
    geometry.indexCount = 0;
    createBuffer(device, nullptr, vertexStride * maxVertexCount, D3D11_BIND_VERTEX_BUFFER, true, &geometry.vertexBuffer);
    createBuffer(device, nullptr, static_cast<int>(sizeof(unsigned int) * maxIndexCount), D3D11_BIND_INDEX_BUFFER, true, &geometry.indexBuffer);
}

void updateDynamicGeometry(
    ID3D11DeviceContext* deviceContext, const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData, D3D11Renderer::Geometry& geometry)
{
    // Update vertex buffer
    D3D11_MAPPED_SUBRESOURCE vertexResource;
    CHECK_HRESULT(deviceContext->Map(geometry.vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexResource));
    memcpy(vertexResource.pData, vertexData.data(), sizeof(float) * vertexData.size());
    deviceContext->Unmap(geometry.vertexBuffer.Get(), 0);

    // Update index buffer
    D3D11_MAPPED_SUBRESOURCE indexResource;
    CHECK_HRESULT(deviceContext->Map(geometry.indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &indexResource));
    memcpy(indexResource.pData, indexData.data(), sizeof(unsigned int) * indexData.size());
    deviceContext->Unmap(geometry.indexBuffer.Get(), 0);

    // Update index count
    geometry.indexCount = static_cast<int>(indexData.size());
}

D3D11Renderer::Shader createShader(ID3D11Device* device, const D3D11Renderer::Shader::InitParams& createData)
{
    D3D11Renderer::Shader shader;

    ComPtr<ID3DBlob> vertexShader = D3D11Renderer::compileShader(createData.name + "_vs", createData.vsSource, "vs_4_0");
    ComPtr<ID3DBlob> pixelShader = D3D11Renderer::compileShader(createData.name + "_ps", createData.psSource, "ps_4_0");

    shader.vertexShader = D3D11Renderer::createVertexShader(device, vertexShader.Get());
    shader.pixelShader = D3D11Renderer::createPixelShader(device, pixelShader.Get());

    HRESULT hr = S_OK;
    hr = device->CreateInputLayout(createData.inputElements.data(), static_cast<UINT>(createData.inputElements.size()), vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize(), &shader.inputLayout);
    CHECK_HRESULT(hr);

    // Create buffer for vertex shader constants
    createBuffer(device, nullptr, createData.vsConstantBufferSize, D3D11_BIND_CONSTANT_BUFFER, false, &shader.vsConstantBuffer);

    // Create buffer for pixel shader constants
    createBuffer(device, nullptr, createData.psConstantBufferSize, D3D11_BIND_CONSTANT_BUFFER, false, &shader.psConstantBuffer);

    return shader;
}

D3D11Renderer::Texture loadTextureFromMemory(ID3D11Device* device, ID3D11DeviceContext* context, const uint8_t* memory, size_t size)
{
    D3D11Renderer::Texture texture;

    CHECK_HRESULT(CoInitialize(nullptr));
    {
        ComPtr<IWICImagingFactory> imagingFactory;
        CHECK_HRESULT(CoCreateInstance(
            CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void**>(imagingFactory.GetAddressOf())));

        ComPtr<IWICStream> stream;
        CHECK_HRESULT(imagingFactory->CreateStream(stream.GetAddressOf()));

        CHECK_HRESULT(stream->InitializeFromMemory(const_cast<uint8_t*>(memory), static_cast<DWORD>(size)));

        ComPtr<IWICBitmapDecoder> decoder;
        CHECK_HRESULT(imagingFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()));

        ComPtr<IWICBitmapFrameDecode> frameDecode;
        CHECK_HRESULT(decoder->GetFrame(0, frameDecode.GetAddressOf()));

        UINT width, height;
        CHECK_HRESULT(frameDecode->GetSize(&width, &height));

        texture.init(width, height);

        WICPixelFormatGUID pixelFormat;
        CHECK_HRESULT(frameDecode->GetPixelFormat(&pixelFormat));

        ComPtr<IWICComponentInfo> componentInfo;
        CHECK_HRESULT(imagingFactory->CreateComponentInfo(pixelFormat, componentInfo.GetAddressOf()));

        ComPtr<IWICPixelFormatInfo> pixelFormatInfo;
        CHECK_HRESULT(componentInfo->QueryInterface(pixelFormatInfo.GetAddressOf()));

        UINT channelCount;
        pixelFormatInfo->GetChannelCount(&channelCount);

        std::vector<uint8_t> pixelData(width * height * channelCount);
        CHECK_HRESULT(frameDecode->CopyPixels(nullptr, width * channelCount, static_cast<UINT>(pixelData.size()), pixelData.data()));

        if (channelCount == 3) {
            std::vector<uint8_t> data(width * height * 4);

            size_t index = 0;
            for (size_t i = 0; i < pixelData.size(); i += 3) {
                data[index++] = pixelData[i + 2];
                data[index++] = pixelData[i + 1];
                data[index++] = pixelData[i + 0];
                data[index++] = 255;
            }

            std::swap(pixelData, data);
        }

        CD3D11_TEXTURE2D_DESC textureDesc{
            DXGI_FORMAT_R8G8B8A8_TYPELESS,
            width,
            height,
            1,
            1,
        };

        D3D11_SUBRESOURCE_DATA initialData{};
        initialData.pSysMem = pixelData.data();
        initialData.SysMemPitch = width * 4;
        CHECK_HRESULT(device->CreateTexture2D(&textureDesc, &initialData, &texture.texture));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        CHECK_HRESULT(device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, texture.srv.GetAddressOf()));

        // Mipmaps
        context->GenerateMips(texture.srv.Get());

        // Sampler state
        D3D11_SAMPLER_DESC samplerDescr = {};
        samplerDescr.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescr.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDescr.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDescr.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDescr.MipLODBias = 0;
        samplerDescr.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDescr.MinLOD = 0;  // -D3D11_FLOAT32_MAX;
        samplerDescr.MaxLOD = D3D11_FLOAT32_MAX;
        samplerDescr.MaxAnisotropy = 1;
        samplerDescr.BorderColor[0] = 0;
        samplerDescr.BorderColor[1] = 0;
        samplerDescr.BorderColor[2] = 0;
        samplerDescr.BorderColor[3] = 0;
        CHECK_HRESULT(device->CreateSamplerState(&samplerDescr, texture.sampler.GetAddressOf()));
    }
    CoUninitialize();
    return texture;
}

D3D11Renderer::Texture loadTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& path)
{
    std::ifstream file(path, std::ios_base::binary);
    if (!file.is_open()) {
        CRITICAL("Failed to open file %s", path.c_str());
    }

    file.seekg(0, std::ios_base::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios_base::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        CRITICAL("Failed to read texture from file %s", path.c_str());
    }

    return loadTextureFromMemory(device, context, data.data(), size);
}

void drawGeometry(ID3D11DeviceContext* context, const D3D11Renderer::Geometry& geometry)
{
    if (geometry.indexCount == 0) {
        return;
    }

    // Set geometry buffers
    const UINT stride = geometry.vertexStride;
    const UINT offset = 0;
    ID3D11Buffer* bufs[]{geometry.vertexBuffer.Get()};
    context->IASetVertexBuffers(0, sizeof(bufs) / sizeof(*(bufs)), bufs, &stride, &offset);
    context->IASetIndexBuffer(geometry.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    switch (geometry.topology) {
        case Renderer::PrimitiveTopology::TriangleList: {
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        } break;
        case Renderer::PrimitiveTopology::Lines: {
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        } break;
    }

    // Draw geometry
    context->DrawIndexed(geometry.indexCount, 0, 0);
}

}  // namespace

namespace VarjoExamples
{
//---------------------------------------------------------------------------

D3D11Renderer::ColorRenderTarget::ColorRenderTarget(ID3D11Device* device, int32_t width, int32_t height, ID3D11Texture2D* colorTexture)
    : Renderer::RenderTarget(width, height)
{
    HRESULT result = S_OK;

    // Create render target view
    {
        assert(colorTexture);

        D3D11_TEXTURE2D_DESC desc{};
        colorTexture->GetDesc(&desc);

        assert(desc.Width == width);
        assert(desc.Height == height);
        assert(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || desc.Format == DXGI_FORMAT_A8_UNORM);

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = (desc.Format == DXGI_FORMAT_A8_UNORM) ? DXGI_FORMAT_A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        result = device->CreateRenderTargetView(colorTexture, &rtvDesc, &m_renderTargetView);
        CHECK_HRESULT(result);
    }
}

//---------------------------------------------------------------------------

D3D11Renderer::DepthRenderTarget::DepthRenderTarget(ID3D11Device* device, int32_t width, int32_t height, ID3D11Texture2D* depthTexture)
    : Renderer::RenderTarget(width, height)
{
    HRESULT result = S_OK;

    // Create depth stencil view
    if (depthTexture) {
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D, depthFormat);

        result = device->CreateDepthStencilView(depthTexture, &depthStencilViewDesc, &m_depthStencilView);
        CHECK_HRESULT(result);
    }
}

//---------------------------------------------------------------------------

D3D11Renderer::Mesh::Mesh(ID3D11Device* device, const std::vector<float>& vertexData, int vertexStride, const std::vector<unsigned int>& indexData,
    Renderer::PrimitiveTopology topology)
    : Renderer::Mesh(vertexData, indexData, topology)
{
    initStaticGeometry(device, m_vertices, vertexStride, m_indices, m_topology, m_geometry);
}

D3D11Renderer::Mesh::Mesh(ID3D11Device* device, int maxVertexCount, int vertexStride, int maxIndexCount, Renderer::PrimitiveTopology topology)
    : Renderer::Mesh({}, {}, topology)
{
    initDynamicGeometry(device, maxVertexCount, vertexStride, maxIndexCount, m_topology, m_geometry);
}

void D3D11Renderer::Mesh::render(ID3D11DeviceContext* context)
{
    // Draw scene object
    drawGeometry(context, m_geometry);
}

void D3D11Renderer::Mesh::updateGeometry(ID3D11DeviceContext* deviceContext, const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData)
{
    update(vertexData, indexData);
    updateDynamicGeometry(deviceContext, vertexData, indexData, m_geometry);
}

//---------------------------------------------------------------------------

D3D11Renderer::D3D11Renderer(IDXGIAdapter* adapter)
    : m_exampleShaders(*this)
{
    UINT deviceFlags = 0;
#ifdef _DEBUG
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    auto driverType = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    CHECK_HRESULT(D3D11CreateDevice(adapter, driverType, 0, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, &m_device, nullptr, &m_context));

    if (m_device == nullptr) {
        LOG_ERROR("Could not create D3D11 device.");
    }

    if (m_context == nullptr) {
        LOG_ERROR("Could not create D3D11 device context.");
    }

    if (m_device == nullptr || m_context == nullptr) {
        CRITICAL("Creating renderer failed.");
    }

    // Disable backface culling
    {
        D3D11_RASTERIZER_DESC rdesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
        rdesc.CullMode = D3D11_CULL_NONE;
        ComPtr<ID3D11RasterizerState> rstate;
        CHECK_HRESULT(m_device->CreateRasterizerState(&rdesc, &rstate));
        m_context->RSSetState(rstate.Get());

        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = true;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        desc.StencilEnable = false;

        CHECK_HRESULT(m_device->CreateDepthStencilState(&desc, m_defaultDepthStencil.GetAddressOf()));

        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

        CHECK_HRESULT(m_device->CreateDepthStencilState(&desc, m_depthDisableState.GetAddressOf()));
    }

    setDepthEnabled(true);
}

std::unique_ptr<Renderer::Mesh> D3D11Renderer::createMesh(
    const std::vector<float>& vertexData, int vertexStride, const std::vector<unsigned int>& indexData, PrimitiveTopology topology)
{
    return std::make_unique<D3D11Renderer::Mesh>(m_device.Get(), vertexData, vertexStride, indexData, topology);
}

std::unique_ptr<Renderer::Texture> D3D11Renderer::loadTextureFromMemory(const uint8_t* memory, size_t size)
{
    return std::make_unique<D3D11Renderer::Texture>(::loadTextureFromMemory(m_device.Get(), m_context.Get(), memory, size));
}

std::unique_ptr<Renderer::Texture> D3D11Renderer::createHdrCubemap(int32_t resolution, varjo_TextureFormat format)
{
    DXGI_FORMAT d3dFormat;

    switch (format) {
        case varjo_TextureFormat_RGBA16_FLOAT: d3dFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;

        // Other formats not supported.
        default: assert(false); return nullptr;
    }

    auto texture = std::make_unique<D3D11Renderer::Texture>();
    texture->init(resolution, resolution, TextureType::Cubemap);

    // Create texture and shader resource view.
    CD3D11_TEXTURE2D_DESC textureDesc(d3dFormat, resolution, resolution, 6, 1);
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    CHECK_HRESULT(m_device->CreateTexture2D(&textureDesc, nullptr, &texture->texture));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = d3dFormat;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    CHECK_HRESULT(m_device->CreateShaderResourceView(texture->texture.Get(), &srvDesc, texture->srv.GetAddressOf()));

    // Create staging texture for dynamic texture updates.
    textureDesc.BindFlags = 0;
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECK_HRESULT(m_device->CreateTexture2D(&textureDesc, nullptr, &texture->stagingTexture));

    // Create sampler state
    D3D11_SAMPLER_DESC samplerDescr = {};
    samplerDescr.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDescr.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDescr.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDescr.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDescr.MipLODBias = 0;
    samplerDescr.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDescr.MinLOD = 0;  // -D3D11_FLOAT32_MAX;
    samplerDescr.MaxLOD = D3D11_FLOAT32_MAX;
    samplerDescr.MaxAnisotropy = 1;
    samplerDescr.BorderColor[0] = 0;
    samplerDescr.BorderColor[1] = 0;
    samplerDescr.BorderColor[2] = 0;
    samplerDescr.BorderColor[3] = 0;
    CHECK_HRESULT(m_device->CreateSamplerState(&samplerDescr, texture->sampler.GetAddressOf()));

    return texture;
}

std::unique_ptr<Renderer::Shader> D3D11Renderer::createShader(const Shader::InitParams& params)
{
    return std::make_unique<D3D11Renderer::Shader>(::createShader(m_device.Get(), params));
}

void D3D11Renderer::updateTexture(Renderer::Texture* texture, const uint8_t* data, size_t rowPitch)
{
    assert(texture);
    const D3D11Renderer::Texture* d3d11Texture = reinterpret_cast<const D3D11Renderer::Texture*>(texture);

    // Update staging texture with CPU data.
    switch (d3d11Texture->getType()) {
        case TextureType::Cubemap: {
            size_t faceDataSize = d3d11Texture->getSize().x * rowPitch;

            // Update each cubemap face.
            for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
                unsigned int subresIndex = D3D11CalcSubresource(0, faceIndex, 1);  // Assuming only 1 mip level for now.

                D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                CHECK_HRESULT(m_context->Map(d3d11Texture->stagingTexture.Get(), subresIndex, D3D11_MAP_WRITE, 0, &mappedSubresource));

                // Currently not supporting differences in row pitch.
                assert(rowPitch == mappedSubresource.RowPitch);
                memcpy(mappedSubresource.pData, data + faceIndex * faceDataSize, faceDataSize);

                m_context->Unmap(d3d11Texture->stagingTexture.Get(), subresIndex);
            }
        } break;

        case TextureType::Texture2D: {
            size_t dataSize = d3d11Texture->getSize().y * rowPitch;
            unsigned int subresIndex = D3D11CalcSubresource(0, 0, 1);  // Assuming only 1 mip level for now.

            D3D11_MAPPED_SUBRESOURCE mappedSubresource;
            CHECK_HRESULT(m_context->Map(d3d11Texture->stagingTexture.Get(), subresIndex, D3D11_MAP_WRITE, 0, &mappedSubresource));

            // Currently not supporting differences in row pitch.
            assert(rowPitch == mappedSubresource.RowPitch);
            memcpy(mappedSubresource.pData, data, dataSize);

            m_context->Unmap(d3d11Texture->stagingTexture.Get(), subresIndex);
            break;
        }
    }

    // Copy to the GPU texture.
    m_context->CopyResource(d3d11Texture->texture.Get(), d3d11Texture->stagingTexture.Get());
}

void D3D11Renderer::renderMesh(Renderer::Mesh& mesh, const void* vsConstants, size_t vsConstantsSize, const void* psConstants, size_t psConstantsSize)
{
    assert(m_vsConstantBuffer.Get());
    assert(m_psConstantBuffer.Get());
    assert(vsConstantsSize == 0 || vsConstantsSize % 16 == 0);
    assert(psConstantsSize == 0 || psConstantsSize % 16 == 0);

    // Copy constant buffer data
    if (vsConstantsSize) {
        m_context->UpdateSubresource(m_vsConstantBuffer.Get(), 0, nullptr, vsConstants, 0, 0);
    }
    if (psConstantsSize) {
        m_context->UpdateSubresource(m_psConstantBuffer.Get(), 0, nullptr, psConstants, 0, 0);
    }

    // Render mesh
    auto& d3d11Mesh = reinterpret_cast<D3D11Renderer::Mesh&>(mesh);
    d3d11Mesh.render(m_context.Get());
}

void D3D11Renderer::setDepthEnabled(bool enabled)
{
    if (enabled) {
        m_context->OMSetDepthStencilState(m_defaultDepthStencil.Get(), 0);
    } else {
        m_context->OMSetDepthStencilState(m_depthDisableState.Get(), 0);
    }
}

void D3D11Renderer::bindRenderTarget(Renderer::ColorDepthRenderTarget& target)
{
    auto d3d11ColorTarget = reinterpret_cast<D3D11Renderer::ColorRenderTarget*>(target.getColorTarget());
    auto d3d11DepthTarget = reinterpret_cast<D3D11Renderer::DepthRenderTarget*>(target.getDepthTarget());

    ID3D11RenderTargetView* rtv = d3d11ColorTarget->getRenderTargetView();
    ID3D11DepthStencilView* dsv = d3d11DepthTarget ? d3d11DepthTarget->getDepthStencilView() : nullptr;

    ID3D11RenderTargetView* targets[]{rtv};
    m_context->OMSetRenderTargets(1, targets, dsv);
}

void D3D11Renderer::unbindRenderTarget()
{
    ID3D11RenderTargetView* nullTgts[]{nullptr};
    m_context->OMSetRenderTargets(1, nullTgts, nullptr);
}

void D3D11Renderer::bindShader(Renderer::Shader& shader)
{
    D3D11Renderer::Shader& d3d11Shader = reinterpret_cast<D3D11Renderer::Shader&>(shader);

    assert(d3d11Shader.inputLayout.Get());
    assert(d3d11Shader.vertexShader.Get());
    assert(d3d11Shader.pixelShader.Get());
    assert(d3d11Shader.vsConstantBuffer.Get());
    assert(d3d11Shader.psConstantBuffer.Get());

    m_context->IASetInputLayout(d3d11Shader.inputLayout.Get());
    m_context->VSSetShader(d3d11Shader.vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(d3d11Shader.pixelShader.Get(), nullptr, 0);

    // Store shader constant buffers
    m_vsConstantBuffer = d3d11Shader.vsConstantBuffer;
    m_psConstantBuffer = d3d11Shader.psConstantBuffer;

    ID3D11Buffer* vsBuffers[] = {m_vsConstantBuffer.Get()};
    ID3D11Buffer* psBuffers[] = {m_psConstantBuffer.Get()};
    m_context->VSSetConstantBuffers(0, 1, vsBuffers);
    m_context->PSSetConstantBuffers(0, 1, psBuffers);
}

void D3D11Renderer::bindTextures(const std::vector<Renderer::Texture*> textures)
{
    std::vector<ID3D11ShaderResourceView*> srvs;
    std::vector<ID3D11SamplerState*> samplers;

    for (const auto& texture : textures) {
        assert(texture);
        const D3D11Renderer::Texture* d3d11Texture = reinterpret_cast<const D3D11Renderer::Texture*>(texture);
        assert(d3d11Texture->srv.Get());
        assert(d3d11Texture->sampler.Get());
        srvs.push_back(d3d11Texture->srv.Get());
        samplers.push_back(d3d11Texture->sampler.Get());
    }

    m_context->PSSetShaderResources(0, static_cast<UINT>(srvs.size()), srvs.data());
    m_context->PSSetSamplers(0, static_cast<UINT>(samplers.size()), samplers.data());
}

void D3D11Renderer::setViewport(int32_t x, int32_t y, int32_t width, int32_t height)
{
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = static_cast<float>(x);
    viewport.TopLeftY = static_cast<float>(y);
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &viewport);
}

void D3D11Renderer::clear(Renderer::ColorDepthRenderTarget& target, const glm::vec4& colorValue, bool clearColor, bool clearDepth, bool clearStencil,
    float depthValue, uint8_t stencilValue)
{
    // Notice that in DX11 and later this will always clear the full rendertarget regardless of the viewport

    auto d3d11ColorTarget = reinterpret_cast<D3D11Renderer::ColorRenderTarget*>(target.getColorTarget());
    auto d3d11DepthTarget = reinterpret_cast<D3D11Renderer::DepthRenderTarget*>(target.getDepthTarget());

    if (clearColor) {
        ID3D11RenderTargetView* rtv = d3d11ColorTarget->getRenderTargetView();
        assert(rtv);
        m_context->ClearRenderTargetView(rtv, (FLOAT*)&colorValue);
    }

    if (clearDepth || clearStencil) {
        ID3D11DepthStencilView* dsv = d3d11DepthTarget ? d3d11DepthTarget->getDepthStencilView() : nullptr;
        if (dsv) {
            UINT flags = (clearDepth ? D3D11_CLEAR_DEPTH : 0) | (clearStencil ? D3D11_CLEAR_STENCIL : 0);
            m_context->ClearDepthStencilView(dsv, flags, depthValue, stencilValue);
        }
    }
}

ID3D11Device* D3D11Renderer::getD3DDevice() const
{
    // Return device pointer
    return m_device.Get();
}

ComPtr<ID3DBlob> D3D11Renderer::compileShader(const std::string& name, const char* sourceData, const char* target)
{
    ComPtr<ID3DBlob> compiledShader = nullptr;
    ComPtr<ID3DBlob> compilerMsgs = nullptr;
    HRESULT hr = D3DCompile(
        sourceData, strlen(sourceData) + 1, name.c_str(), nullptr, nullptr, "main", target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &compiledShader, &compilerMsgs);
    if (compilerMsgs != nullptr) {
        LOG_WARNING("Shader compile warnings: name=\"%s\"\n%.*s", name.c_str(), static_cast<int>(compilerMsgs->GetBufferSize()),
            reinterpret_cast<char*>(compilerMsgs->GetBufferPointer()));
    }
    CHECK_HRESULT(hr);
    assert(compilerMsgs == nullptr);
    return compiledShader;
}

ComPtr<ID3D11VertexShader> D3D11Renderer::createVertexShader(ID3D11Device* device, ID3DBlob* blob)
{
    ComPtr<ID3D11VertexShader> ret;
    HRESULT hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ret.GetAddressOf());
    CHECK_HRESULT(hr);
    return ret;
}

ComPtr<ID3D11PixelShader> D3D11Renderer::createPixelShader(ID3D11Device* device, ID3DBlob* blob)
{
    ComPtr<ID3D11PixelShader> ret;
    HRESULT hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ret.GetAddressOf());
    CHECK_HRESULT(hr);
    return ret;
}

ComPtr<ID3D11ComputeShader> D3D11Renderer::createComputeShader(ID3D11Device* device, ID3DBlob* blob)
{
    ComPtr<ID3D11ComputeShader> ret;
    HRESULT hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ret.GetAddressOf());
    CHECK_HRESULT(hr);
    return ret;
}

std::unique_ptr<Renderer::Mesh> D3D11Renderer::createDynamicMesh(int maxVertexCount, int vertexStride, int maxIndexCount, PrimitiveTopology topology)
{
    return std::make_unique<D3D11Renderer::Mesh>(m_device.Get(), maxVertexCount, vertexStride, maxIndexCount, topology);
}

void D3D11Renderer::updateMesh(Renderer::Mesh& mesh, const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData)
{
    D3D11Renderer::Mesh& d3dMesh = static_cast<D3D11Renderer::Mesh&>(mesh);
    d3dMesh.updateGeometry(m_context.Get(), vertexData, indexData);
}

}  // namespace VarjoExamples
