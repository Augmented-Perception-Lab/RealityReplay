// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <memory>
#include <array>

#include "Globals.hpp"
#include "Renderer.hpp"
#include "ExampleShaders.hpp"

namespace VarjoExamples
{
//! Helper struct for object pose
struct ObjectPose {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};        //!< Object position
    glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};  //!< Object rotation
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};           //!< Object scale
};

//! Camera parameters structure to be filled with camera frame metadata from data stream
struct CameraParams {
    double exposureEV = 6.2;
    double cameraCalibrationConstant = 1.0 / 1.2;
    varjo_WBNormalizationData wbNormalizationData = {
        {1.0, 1.0, 1.0},
        {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0},
    };
};

//! Scene interface for user derived scene classes.
class Scene
{
public:
    //! Update parameters struct
    struct UpdateParams {
        CameraParams cameraParams{};  //!< Scene camera parameters
    };

    //! Virtual destructor
    virtual ~Scene();

    //! Return current frame time
    double getFrameTime() { return m_frameTime; }

    //! Return current delta time
    double getDeltaTime() { return m_deltaTime; }

    //! Return frame counter
    int64_t getFrameCount() { return m_frameCount; }

    //! Update scene frame
    void update(double frameTime, double deltaTime, int64_t frameCount, const UpdateParams& params);

    //! Render scene frame to given view
    void render(
        Renderer& renderer, Renderer::ColorDepthRenderTarget& target, const glm::mat4x4& viewMat, const glm::mat4x4& projMat, void* userData = nullptr) const;

protected:
    //! Protected constructor
    Scene();

    //! Update scene frame
    virtual void onUpdate(double frameTime, double deltaTime, int64_t frameCount, const UpdateParams& params) = 0;

    //! Render scene frame to given view
    virtual void onRender(
        Renderer& renderer, Renderer::ColorDepthRenderTarget& target, const glm::mat4x4& viewMat, const glm::mat4x4& projMat, void* userData) const = 0;

private:
    double m_frameTime = 0.0;  //!< Last update time
    double m_deltaTime = 0.0;  //!< Delta time since previous update
    int64_t m_frameCount = 0;  //!< Frame counter
};

}  // namespace VarjoExamples
