// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <memory>

#include "Globals.hpp"
#include "Renderer.hpp"

// NOTICE! Shader constant data structures here are assumed to map directly to
// constant buffers used in corresponding shaders.

namespace VarjoExamples
{
//! Class wrapping common shaders used in examples
class ExampleShaders
{
public:
    //! Shader type enumeration
    enum class ShaderType {
        RainbowCube,
        CubemappedCube,
        SolidCube,
        LitSolid,
        MarkerPlane,
        MarkerAxis,
        TexturedPlane,
    };

    //! Create a shader for given type
    virtual std::unique_ptr<Renderer::Shader> createShader(ShaderType type) const = 0;

    //! Transform data constants
    struct TransformData {
        TransformData() = default;
        TransformData(const glm::mat4x4& world, const glm::mat4x4& view, const glm::mat4x4& proj)
            : world(world)
            , view(view)
            , projection(proj)
        {
        }
        glm::mat4x4 world{1.0};        //!< Model world matrix
        glm::mat4x4 view{1.0f};        //!< View matrix
        glm::mat4x4 projection{1.0f};  //!< Projection matrix
    };

    //! Data structure for whitebalance normaization contants. vec3 and mat4x4 used for shader compatibility.
    struct WBNormalizationData {
        WBNormalizationData() = default;
        WBNormalizationData(const varjo_WBNormalizationData& wb)
            : wbGains(wb.whiteBalanceColorGains[0], wb.whiteBalanceColorGains[1], wb.whiteBalanceColorGains[2], 0)
            , invCCM(wb.invCCM.value[0], wb.invCCM.value[1], wb.invCCM.value[2], 0,  //
                  wb.invCCM.value[3], wb.invCCM.value[4], wb.invCCM.value[5], 0,     //
                  wb.invCCM.value[6], wb.invCCM.value[7], wb.invCCM.value[8], 0,     //
                  0, 0, 0, 0)                                                        //
            , ccm(wb.ccm.value[0], wb.ccm.value[1], wb.ccm.value[2], 0,              //
                  wb.ccm.value[3], wb.ccm.value[4], wb.ccm.value[5], 0,              //
                  wb.ccm.value[6], wb.ccm.value[7], wb.ccm.value[8], 0,              //
                  0, 0, 0, 0)
        {
        }

        glm::vec4 wbGains = glm::vec4(1.0f);     //!< White balance color gains.
        glm::mat4x4 invCCM = glm::mat4x4(1.0f);  //!< Inverse CCM for reverting current CCM.
        glm::mat4x4 ccm = glm::mat4x4(1.0f);     //!< Final CCM.
    };

    //! Lighting values
    struct LightingData {
        glm::vec3 ambientLight = {1.0f, 1.0f, 1.0f};  //!< Scene ambient light
    };

    //! Rainbow cube shader constants
    struct RainbowCubeConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
            glm::vec4 objectColor = glm::vec4(1.0f);
            glm::vec3 objectScale = glm::vec3(1.0f);
            float vtxColorFactor = 1.0f;
        } vs;

        //! Pixel shader constants
        struct {
            LightingData lighting{};
            float exposureGain = 1.0f;
            WBNormalizationData wbNormalization{};
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

    //! Cubemapped cube shader constants
    struct CubemappedCubeConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
        } vs;

        //! Pixel shade constants
        struct {
            LightingData lighting{};
            float exposureGain = 1.0f;
            WBNormalizationData wbNormalization{};
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

    //! Solid cube shader constants
    struct SolidCubeConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
            glm::vec4 objectColor = glm::vec4(1.0f);
            float vtxColorFactor = 1.0f;
            glm::vec3 _padding;
        } vs;

        //! Pixel shader constants
        struct {
            glm::vec4 _padding;
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

    //! Lit solid shader constants
    struct LitSolidConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
        } vs;

        //! Pixel shader constants
        struct {
            glm::vec4 objectColor = glm::vec4(1.0f);
            glm::vec3 lightDirection = glm::vec3(-1, -1, 1);
            glm::vec4 lightColor = glm::vec4(1.0f);
            float _padding;
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

    //! Marker plane shader constants
    struct MarkerPlaneConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
        } vs;

        //! Pixel shade contants
        struct {
            int markerId = 0;
            int _padding[3];
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

    //! Marker axis shader constants
    struct MarkerAxisConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
        } vs;

        //! Pixel shade contants
        struct {
            float _padding[4];
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

    //! Textured plane shader constants
    struct TexturedPlaneConstants {
        //! Vertex shader constants
        struct {
            TransformData transform{};
            float dstAspect = 1.0f;
            float texAspect = 1.0f;
            float _padding[2];
        } vs;

        //! Pixel shade contants
        struct {
            glm::vec4 colorCorrection;
        } ps;

        // Check constant buffer sizes
        static_assert(sizeof(vs) % 16 == 0, "Invalid constant buffer size.");
        static_assert(sizeof(ps) % 16 == 0, "Invalid constant buffer size.");
    };

protected:
    //! Protected constructor
    ExampleShaders() = default;
};

}  // namespace VarjoExamples
