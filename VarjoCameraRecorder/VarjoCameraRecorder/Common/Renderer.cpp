// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#include "Renderer.hpp"

#include <fstream>

namespace
{
uint8_t base64ToBinary(uint8_t c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    } else if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    } else if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    } else if (c == '+') {
        return 62;
    } else if (c == '/') {
        return 63;
    }
    return 0;
}

std::array<char, 3> decodeBase64Group(const char group[4])
{
    uint8_t c0 = base64ToBinary(group[0]);
    uint8_t c1 = base64ToBinary(group[1]);
    uint8_t c2 = base64ToBinary(group[2]);
    uint8_t c3 = base64ToBinary(group[3]);

    union {
        uint32_t val;
        std::array<char, 3> arr;
    } value;
    value.val = c0 << 18 | c1 << 12 | c2 << 6 | c3;
    std::swap(value.arr[0], value.arr[2]);
    return value.arr;
}
}  // namespace

namespace VarjoExamples
{
Renderer::RenderTarget::~RenderTarget() = default;
Renderer::Shader::~Shader() = default;
Renderer::Texture::Texture() = default;
Renderer::Texture::~Texture() = default;
Renderer::Mesh::~Mesh() = default;
Renderer::~Renderer() = default;

Renderer::Mesh::Mesh(const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData, Renderer::PrimitiveTopology topology)
    : m_vertices(vertexData)
    , m_indices(indexData)
    , m_topology(topology)
{
}

void Renderer::Mesh::update(const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData)
{
    m_vertices = vertexData;
    m_indices = indexData;
}

std::unique_ptr<Renderer::Texture> Renderer::loadTextureFromBase64(const char* base64)
{
    size_t stringLength = strlen(base64);

    std::vector<uint8_t> data((stringLength / 4) * 3);

    size_t index = 0;
    for (size_t i = 0; i < stringLength; i += 4) {
        const auto arr = decodeBase64Group(&base64[i]);
        data[index++] = arr[0];
        data[index++] = arr[1];
        data[index++] = arr[2];
    }

    return loadTextureFromMemory(data.data(), data.size());
}

std::unique_ptr<Renderer::Texture> Renderer::loadTextureFromFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (file.good()) {
        size_t size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<char> data(size);
        if (file.read(data.data(), size)) {
            return loadTextureFromMemory(reinterpret_cast<const uint8_t*>(data.data()), data.size());
        } else {
            LOG_ERROR("Loading texture file failed: %s", filename.c_str());
        }
    } else {
        LOG_ERROR("Opening texture file failed: %s", filename.c_str());
    }

    return {};
}

}  // namespace VarjoExamples
