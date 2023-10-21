
#include "Scene.hpp"

namespace VarjoExamples
{
Scene::Scene() = default;

Scene::~Scene() = default;

void Scene::update(double frameTime, double deltaTime, int64_t frameCount, const UpdateParams& params)
{
    m_frameTime = frameTime;
    m_deltaTime = deltaTime;
    m_frameCount = frameCount;

    // Call implementing class update
    onUpdate(frameTime, deltaTime, frameCount, params);
}

void Scene::render(Renderer& renderer, Renderer::ColorDepthRenderTarget& target, const glm::mat4x4& viewMat, const glm::mat4x4& projMat, void* userData) const
{
    // Call implementing class render
    onRender(renderer, target, viewMat, projMat, userData);
}

}  // namespace VarjoExamples
