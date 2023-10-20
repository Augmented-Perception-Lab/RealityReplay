// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <array>
#include <cassert>

#include "Globals.hpp"

namespace VarjoExamples
{
class ExampleShaders;

//! Base class for renderer implementations
class Renderer
{
public:
    //! Renderer independent base class for texture render targets
    class RenderTarget
    {
    public:
        //! Virtual destructor
        virtual ~RenderTarget();

        //! Returns render target size
        const glm::ivec2& getSize() const { return m_size; }

    protected:
        //! Protected constructor
        RenderTarget(int32_t width, int32_t height)
            : m_size(width, height)
        {
        }

    private:
        glm::ivec2 m_size{0, 0};  //!< Render target size
    };

    //! Combined color depth render target
    class ColorDepthRenderTarget
    {
    public:
        //! Construct color+depth render target
        ColorDepthRenderTarget(RenderTarget& colorTarget, RenderTarget& depthTarget)
            : m_colorTarget(&colorTarget)
            , m_depthTarget(&depthTarget)
        {
            assert(colorTarget.getSize() == depthTarget.getSize());
        };

        //! Construct color-only render target
        ColorDepthRenderTarget(RenderTarget& colorTarget)
            : m_colorTarget(&colorTarget)
            , m_depthTarget(nullptr){};

        //! Return associated color render target
        RenderTarget* getColorTarget() { return m_colorTarget; }

        //! Return associated depth render target
        RenderTarget* getDepthTarget() { return m_depthTarget; }

    private:
        RenderTarget* m_colorTarget = nullptr;  //!< Color texture render target
        RenderTarget* m_depthTarget = nullptr;  //!< Depth texture render target
    };

    //! Shader base class
    class Shader
    {
    public:
        //! Default virtual destructor
        virtual ~Shader();
    };

    //! Texture type enumerations
    enum class TextureType {
        Texture2D,  //!< 2D texture
        Cubemap,    //!< Cubemap texture
    };

    //! Texture base class
    class Texture
    {
    public:
        //! Default virtual destructor
        virtual ~Texture();

        //! Initializes texture
        bool init(int32_t width, int32_t height, TextureType type = TextureType::Texture2D)
        {
            assert(m_size.x == 0 && m_size.y == 0);
            m_size = {width, height};
            m_type = type;
            return true;
        }

        //! Returns texture size
        const glm::ivec2& getSize() const { return m_size; }

        //! Returns texture type
        TextureType getType() const { return m_type; }

    protected:
        //! Protected constructor
        Texture();

    private:
        glm::ivec2 m_size{0, 0};                      //!< Texture size
        TextureType m_type = TextureType::Texture2D;  //!< Texture type
    };

    //! Primitive topology enumeration
    enum PrimitiveTopology {
        TriangleList,  //!< List of triangles
        Lines,         //!< List of lines
    };

    //! Renderer independent base class for a simple mesh object
    class Mesh
    {
    public:
        //! Default virtual destructor
        virtual ~Mesh();

    protected:
        //! Protected constructor
        Mesh(const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData, PrimitiveTopology topology);

        //! Update mesh with new data
        void update(const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData);

    protected:
        std::vector<float> m_vertices;        //!< Object vertex data
        std::vector<unsigned int> m_indices;  //!< Object index data
        PrimitiveTopology m_topology;         //!< Mesh topology
    };

    //! Default virtual destructor
    virtual ~Renderer();

    //! Create mesh object of given vertex data
    virtual std::unique_ptr<Renderer::Mesh> createMesh(
        const std::vector<float>& vertexData, int vertexStride, const std::vector<unsigned int>& indexData, PrimitiveTopology topology) = 0;

    //! Create dynamic mesh object
    virtual std::unique_ptr<Renderer::Mesh> createDynamicMesh(int maxVertexCount, int vertexStride, int maxIndexCount, PrimitiveTopology topology) = 0;

    //! Update dynamic mesh object with given vertex data
    virtual void updateMesh(Renderer::Mesh& mesh, const std::vector<float>& vertexData, const std::vector<unsigned int>& indexData) = 0;

    //! Load a texture from base64 encoded string.
    std::unique_ptr<Renderer::Texture> loadTextureFromBase64(const char* base64);

    //! Load a texture image.
    std::unique_ptr<Renderer::Texture> loadTextureFromFile(const std::string& filename);

    //! Load a texture memory.
    virtual std::unique_ptr<Renderer::Texture> loadTextureFromMemory(const uint8_t* memory, size_t size) = 0;

    //! Create a cubemap texture.
    virtual std::unique_ptr<Renderer::Texture> createHdrCubemap(int32_t resolution, varjo_TextureFormat format) = 0;

    //! Updates texture with new data.
    virtual void updateTexture(Texture* texture, const uint8_t* data, size_t rowPitch) = 0;

    //! Template function for rendering mesh
    template <typename TVSConstants, typename TPSConstants>
    void renderMesh(Renderer::Mesh& mesh, const TVSConstants& vsConstants, const TPSConstants& psConstants)
    {
        static_assert((sizeof(TVSConstants) == 0) || (sizeof(TVSConstants) % 16) == 0, "VS constants must be 16-byte aligned");
        static_assert((sizeof(TPSConstants) == 0) || (sizeof(TPSConstants) % 16) == 0, "PS constants must be 16-byte aligned");
        renderMesh(mesh, &vsConstants, sizeof(vsConstants), &psConstants, sizeof(psConstants));
    }

    //! Render mesh with given constants
    virtual void renderMesh(Renderer::Mesh& mesh, const void* vsConstants, size_t vsConstantsSize, const void* psConstants, size_t psConstantsSize) = 0;

    //! Set depth enabled
    virtual void setDepthEnabled(bool enabled) = 0;

    //! Bind render target
    virtual void bindRenderTarget(ColorDepthRenderTarget& target) = 0;

    //! Unbind currently bound render target
    virtual void unbindRenderTarget() = 0;

    //! Bind shader
    virtual void bindShader(Shader& shader) = 0;

    //! Bind textures
    virtual void bindTextures(const std::vector<Renderer::Texture*> textures) = 0;

    //! Set viewport dimensions
    virtual void setViewport(int32_t x, int32_t y, int32_t width, int32_t height) = 0;

    //! Clear given entire render target
    virtual void clear(ColorDepthRenderTarget& target, const glm::vec4& colorValue, bool clearColor = true, bool clearDepth = true, bool clearStencil = true,
        float depthValue = 1.0f, uint8_t stencilValue = 0) = 0;

    //! Return example shader library reference
    virtual const ExampleShaders& getShaders() const = 0;
};

}  // namespace VarjoExamples
