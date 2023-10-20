// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#include "D3D11Shaders.hpp"
#include "D3D11Renderer.hpp"

namespace
{
// Generic vertex shader source for just doing the transform
constexpr char* simpleVsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
};

struct VsInput {
    float3 position : POSITION;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float3 txcoord : TEXCOORD;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);
    output.txcoord = input.position;
    return output;
}
)src";
}  // namespace

namespace VarjoExamples
{
// ----------------------------------------------------------------

namespace RainbowCube
{
// Vertex shader source
constexpr char* vsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    float4 objectColor;
    float3 objectScale;
    float vtxColorFactor;
};

struct VsInput {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float4 txcoord : TEXCOORD;
    float4 color : COLOR;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0f);
    output.txcoord = float4(output.position.xyz * objectScale, 0.0f);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);
    output.color = float4(lerp(objectColor.rgb, input.color.rgb, vtxColorFactor), objectColor.a);
    return output;
}
)src";

// Pixel shader source
constexpr char* psSource = R"src(

struct WBNormalizationData {
    float4 wbGains;
    float4x4 invCCM;
    float4x4 ccm;
};

float3 normalizeWhiteBalance(in float3 inRGB, in WBNormalizationData data)
{
    float3 outRGB = mul(inRGB, (float3x3)data.invCCM);
    outRGB.rgb *= data.wbGains.rgb;
    outRGB = saturate(outRGB);
    outRGB.rgb = mul(outRGB.rgb, (float3x3)data.ccm);
    return outRGB;
}

cbuffer ConstantBuffer : register(b0) {
    float3 ambientLight;
    float exposureGain;
    WBNormalizationData wbNormalizationData;
};

struct PsInput {
    float4 position : SV_POSITION;
    float4 txcoord : TEXCOORD;
    float4 color : COLOR;
    bool isFrontFace : SV_IsFrontFace;
};

float4 main(PsInput input) : SV_TARGET {
    float3 c = input.txcoord.xyz * 10.0f;
    float3 surfColor = 0.2f + 0.8f * frac(c.x + c.y) * frac(c.y + c.z) * frac(c.x + c.z);
    float3 faceColor = (input.isFrontFace ? input.color.rgb : float3(0.7,0.01,0.1) * (float3(0.2,0.1,0.2)+input.color.gbr));
    float4 color = float4(exposureGain * ambientLight * faceColor.rgb * surfColor.rgb, input.color.a);
    color.rgb = normalizeWhiteBalance(color.rgb, wbNormalizationData);
    return color;
}
)src";

// Init parameters
static const D3D11Renderer::Shader::InitParams initParams = {
    "RainbowCube",
    vsSource,
    psSource,
    sizeof(ExampleShaders::RainbowCubeConstants::vs),
    sizeof(ExampleShaders::RainbowCubeConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace RainbowCube

// ----------------------------------------------------------------

namespace CubemappedCube
{
// Vertex shader source
constexpr char* vsSource = simpleVsSource;

// Pixel shader source
constexpr char* psSource = R"src(

TextureCube cubeTex : register(t0);
SamplerState samplerState: register(s0);

struct WBNormalizationData {
    float4 wbGains;
    float4x4 invCCM;
    float4x4 ccm;
};

float3 normalizeWhiteBalance(in float3 inRGB, in WBNormalizationData data)
{
    float3 outRGB = mul(inRGB, (float3x3)data.invCCM);
    outRGB.rgb *= data.wbGains.rgb;
    outRGB = saturate(outRGB);
    outRGB.rgb = mul(outRGB.rgb, (float3x3)data.ccm);
    return outRGB;
}

cbuffer ConstantBuffer : register(b0) {
    float3 ambientLight;
    float exposureGain;
    WBNormalizationData wbNormalizationData;
};

struct PsInput {
    float4 position : SV_POSITION;
    float3 txcoord : TEXCOORD;
    bool isFrontFace : SV_IsFrontFace;
};

float4 main(PsInput input) : SV_TARGET {
    float3 cubeCoord = normalize(float3(-1.0,-1.0,1.0) * input.txcoord.xyz);
    float3 cubemapColor = cubeTex.Sample(samplerState, cubeCoord).rgb * 100.0f;
    float3 faceColor = input.isFrontFace ? 1.0 : 0.5 ;
    float4 color = float4(exposureGain * faceColor.rgb * cubemapColor, 1.0);
    color.rgb = normalizeWhiteBalance(color.rgb, wbNormalizationData);
    return color;
}
)src";

// Init parameters
static const D3D11Renderer::Shader::InitParams initParams = {
    "CubemappedCube",
    vsSource,
    psSource,
    sizeof(ExampleShaders::CubemappedCubeConstants::vs),
    sizeof(ExampleShaders::CubemappedCubeConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace CubemappedCube

// ----------------------------------------------------------------

namespace MarkerPlane
{
// Vertex shader source
constexpr char* vsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
};

struct VsInput {
    float3 position : POSITION;
    float2 txcoord : TEXCOORD;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float2 txcoord : TEXCOORD;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);
    output.txcoord = input.txcoord;
    return output;
}
)src";

// Pixel shader source
constexpr char* psSource = R"src(

Texture2D numberAtlas: register(t0);
SamplerState samplerState: register(s0);

cbuffer ConstantBuffer : register(b0) {
    int markerId;
    int3 _padding;
};

struct PsInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float map(float val, float min0, float max0, float min1, float max1) {
    float t = (val - min0) / (max0 - min0);
    return t * (max1 - min1) + min1;
}

void drawLetter(int letter, float2 texCoord, float2 min, float2 max, inout float4 color) {
    float letterSize = 0.1;

    texCoord.x = map(texCoord.x, min.x, max.x, 0.0, 1.0);
    texCoord.y = map(texCoord.y, min.y, max.y, 0.0, 1.0);

    if (texCoord.x < 0.01 || texCoord.x > 0.99 ||
        texCoord.y < 0.01 || texCoord.y > 0.99) {
        return;
    }

    texCoord.x *= letterSize;
    texCoord.x += letterSize * float(letter);

    color *= numberAtlas.Sample(samplerState, texCoord);
}

float4 main(PsInput input) : SV_TARGET {
    float4 color = float4(1.0, 1.0, 1.0, 1.0);

    uint id = markerId;
    uint thousands = id / 1000;
    id -= thousands * 1000;

    uint hundreds = id / 100;
    id -= hundreds * 100;

    uint tens = id / 10;
    id -= tens * 10;

    drawLetter(thousands, input.texCoord, float2(0.0, 0.5), float2(0.25, 0.75), color);
    drawLetter(hundreds, input.texCoord, float2(0.25,0.5), float2(0.5, 0.75), color);
    drawLetter(tens, input.texCoord, float2(0.5, 0.5), float2(0.75, 0.75), color);
    drawLetter(id, input.texCoord, float2(0.75, 0.5), float2(1.0, 0.75), color);

    return float4(color.rgb, 1.0);
}
)src";

// Init parameters
static const D3D11Renderer::Shader::InitParams initParams = {
    "MarkerPlane",
    vsSource,
    psSource,
    sizeof(ExampleShaders::MarkerPlaneConstants::vs),
    sizeof(ExampleShaders::MarkerPlaneConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace MarkerPlane

// ----------------------------------------------------------------

namespace MarkerAxis
{
// Vertex shader source
constexpr char* vsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
};

struct VsInput {
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : TEXCOORD;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);

    float4 normal = float4(input.normal, 0.0f);
    normal = mul(world, normal);
    normal = mul(view, normal);
    normal = mul(projection, normal);

    output.position.xy += normalize(float2(normal.x, normal.y)) * 0.001;

    output.color = input.color;
    return output;
}
)src";

// Pixel shader source
constexpr char* psSource = R"src(

Texture2D numberAtlas: register(t0);
SamplerState samplerState: register(s0);

cbuffer ConstantBuffer : register(b0) {
    float4 _padding;
};

struct PsInput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

float4 main(PsInput input) : SV_TARGET {
    return float4(input.color.rgb, 1.0);
}
)src";

// Init parameters
const D3D11Renderer::Shader::InitParams initParams = {
    "LineSegment",
    vsSource,
    psSource,
    sizeof(ExampleShaders::MarkerAxisConstants::vs),
    sizeof(ExampleShaders::MarkerAxisConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace MarkerAxis

// ----------------------------------------------------------------

namespace SolidCube
{
// Vertex shader source
constexpr char* vsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    float4 objectColor;
    float vtxColorFactor;
    float3 _padding;
};

struct VsInput {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);
    output.color = float4(lerp(objectColor.rgb, input.color.rgb, vtxColorFactor), objectColor.a);
    return output;
}
)src";

// Pixel shader source
constexpr char* psSource = R"src(
cbuffer ConstantBuffer : register(b0) {
    float4 _padding;
};

struct PsInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PsInput input) : SV_TARGET {
    return input.color;
}
)src";

// Init parameters
static const D3D11Renderer::Shader::InitParams initParams = {
    "SolidCube",
    vsSource,
    psSource,
    sizeof(ExampleShaders::RainbowCubeConstants::vs),
    sizeof(ExampleShaders::RainbowCubeConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace SolidCube

// ----------------------------------------------------------------

namespace LitSolid
{
// Vertex shader source
constexpr char* vsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    float4 objectColor;
    float vtxColorFactor;
    float3 _padding;
};

struct VsInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);
    output.normal = mul(world, float4(input.normal, 0.0f)).xyz;
    return output;
}
)src";

// Pixel shader source
constexpr char* psSource = R"src(
cbuffer ConstantBuffer : register(b0) {
    float4 objectColor;
    float3 lightDirection;
    float4 lightColor;
    float _padding;
};

struct PsInput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

float4 main(PsInput input) : SV_TARGET {
    float d = lerp(0.2, 1.0, saturate(dot(lightDirection, input.normal)));
    return float4(objectColor.rgb * d, 1.0f);
}
)src";

// Init parameters
static const D3D11Renderer::Shader::InitParams initParams = {
    "LitSolid",
    vsSource,
    psSource,
    sizeof(ExampleShaders::LitSolidConstants::vs),
    sizeof(ExampleShaders::LitSolidConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace LitSolid

// ----------------------------------------------------------------

namespace TexturedPlane
{
// Vertex shader source
constexpr char* vsSource = R"src(

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    float dstAspect;
    float texAspect;
    float2 _padding;
};

struct VsInput {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VsOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VsOutput main(VsInput input) {
    VsOutput output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(world, output.position);
    output.position = mul(view, output.position);
    output.position = mul(projection, output.position);

    float2 uv = input.texCoord.xy - 0.5;
    if (dstAspect > texAspect)
    {
        output.texCoord = float2(0.5, 0.5) + float2(uv.x, uv.y * texAspect / dstAspect);
    }
    else
    {
        output.texCoord = float2(0.5, 0.5) + float2(uv.x * dstAspect / texAspect, uv.y);
    }
    return output;
}
)src";

// Pixel shader source
constexpr char* psSource = R"src(

Texture2D imageTexture: register(t0);
SamplerState samplerState: register(s0);

cbuffer ConstantBuffer : register(b0) {
    float4 colorCorrection;
};

struct PsInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PsInput input) : SV_TARGET {
    float4 color = float4(1.0, 1.0, 1.0, 1.0);
    color.rgba = imageTexture.Sample(samplerState, input.texCoord);
    return float4(colorCorrection.a * colorCorrection.rgb * color.rgb, 1.0);
}
)src";

const D3D11Renderer::Shader::InitParams initParams = {
    "TexturedPlane",
    TexturedPlane::vsSource,
    TexturedPlane::psSource,
    sizeof(ExampleShaders::TexturedPlaneConstants::vs),
    sizeof(ExampleShaders::TexturedPlaneConstants::ps),
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    },
};

}  // namespace TexturedPlane

// ----------------------------------------------------------------

std::unique_ptr<Renderer::Shader> D3D11ExampleShaders::createShader(ExampleShaders::ShaderType type) const
{
    return D3D11ExampleShaders::createShader(m_d3d11Renderer, type);
};

std::unique_ptr<Renderer::Shader> D3D11ExampleShaders::createShader(Renderer& renderer, ExampleShaders::ShaderType type)
{
    D3D11Renderer& d3d11Renderer = reinterpret_cast<D3D11Renderer&>(renderer);

    switch (type) {
        case ExampleShaders::ShaderType::RainbowCube: {
            return d3d11Renderer.createShader(RainbowCube::initParams);
        }
        case ExampleShaders::ShaderType::CubemappedCube: {
            return d3d11Renderer.createShader(CubemappedCube::initParams);
        }
        case ExampleShaders::ShaderType::SolidCube: {
            return d3d11Renderer.createShader(SolidCube::initParams);
        }
        case ExampleShaders::ShaderType::LitSolid: {
            return d3d11Renderer.createShader(LitSolid::initParams);
        }
        case ExampleShaders::ShaderType::MarkerPlane: {
            return d3d11Renderer.createShader(MarkerPlane::initParams);
        }
        case ExampleShaders::ShaderType::MarkerAxis: {
            return d3d11Renderer.createShader(MarkerAxis::initParams);
        }
        case ExampleShaders::ShaderType::TexturedPlane: {
            return d3d11Renderer.createShader(TexturedPlane::initParams);
        }
        default: {
            LOG_ERROR("Unknown shader type: %d", type);
        } break;
    }
    return nullptr;
}

}  // namespace VarjoExamples
