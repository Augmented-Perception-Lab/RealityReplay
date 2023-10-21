// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#include "MultiLayerView.hpp"

#include <cassert>
#include <Varjo_math.h>

#include "Scene.hpp"

namespace
{
void linkExtensions(varjo_ViewExtension*& extensionChain, varjo_ViewExtension* extensions, bool breakChain)
{
    // Link only one extension, break link chain
    if (breakChain) {
        extensions->next = nullptr;
    }

    if (extensionChain == nullptr) {
        extensionChain = extensions;
    } else {
        varjo_ViewExtension* last = extensionChain;
        while (last->next) {
            last = last->next;
        }
        last->next = extensions;
    }
}

}  // namespace

namespace VarjoExamples
{
MultiLayerView::Layer::Layer(MultiLayerView& view, int contextDivider, int focusDivider)
    : m_view(view)
{
    // Calculate layer viewports
    m_viewports = calculateViewports(m_view.getSession(), contextDivider, focusDivider);

    // Allocate view specific data
    m_multiProjViews.resize(m_view.getViewCount());
    m_extDepthViews.resize(m_view.getViewCount());
    m_extDepthTestRangeViews.resize(m_view.getViewCount());
}

MultiLayerView::Layer::~Layer()
{
    // These just free allocated resource, so no session or error check needed
    varjo_FreeSwapChain(m_colorSwapChain);
    varjo_FreeSwapChain(m_depthSwapChain);
}

void MultiLayerView::Layer::setupViews()
{
    // Setup views
    for (int i = 0; i < m_view.getViewCount(); ++i) {
        // Viewport layout
        const varjo_Viewport& viewport = m_viewports[i];
        m_multiProjViews[i].viewport = varjo_SwapChainViewport{m_colorSwapChain, viewport.x, viewport.y, viewport.width, viewport.height, 0};

        // Notice that we set extension links nullptr here and do the actual linking later
        // in render call according to the render parameters.

        // Depth extension
        m_extDepthViews[i].header.type = varjo_ViewExtensionDepthType;
        m_extDepthViews[i].header.next = nullptr;
        m_extDepthViews[i].minDepth = 0.0;
        m_extDepthViews[i].maxDepth = 1.0;
        m_extDepthViews[i].nearZ = c_nearClipPlane;
        m_extDepthViews[i].farZ = c_farClipPlane;
        m_extDepthViews[i].viewport = varjo_SwapChainViewport{m_depthSwapChain, viewport.x, viewport.y, viewport.width, viewport.height, 0};

        // Depth test range extension
        m_extDepthTestRangeViews[i].header.type = varjo_ViewExtensionDepthTestRangeType;
        m_extDepthTestRangeViews[i].header.next = nullptr;
        m_extDepthTestRangeViews[i].nearZ = 0.0;
        m_extDepthTestRangeViews[i].farZ = 1.0;
    }
}

std::vector<varjo_Viewport> MultiLayerView::Layer::calculateViewports(varjo_Session* session, int contextDivider, int focusDivider) const
{
    constexpr int viewsPerRow = 2;

    const int32_t viewCount = varjo_GetViewCount(session);
    CHECK_VARJO_ERR(m_view.getSession());

    std::vector<varjo_Viewport> viewports;
    viewports.reserve(viewCount);

    int x = 0, y = 0;
    for (int i = 0; i < viewCount; i++) {
        const varjo_ViewDescription viewDesc = varjo_GetViewDescription(session, static_cast<int32_t>(i));
        CHECK_VARJO_ERR(m_view.getSession());

        const int div = std::max(1, (viewDesc.display == varjo_DisplayType_Focus) ? focusDivider : contextDivider);
        const int32_t viewW = static_cast<int32_t>(viewDesc.width) / div;
        const int32_t viewH = static_cast<int32_t>(viewDesc.height) / div;

        const varjo_Viewport viewport{x, y, viewW, viewH};
        viewports.push_back(viewport);
        x += viewport.width;
        if (i > 0 && viewports.size() % viewsPerRow == 0) {
            x = 0;
            y += viewport.height;
        }
    }
    return viewports;
}

Renderer::ColorDepthRenderTarget MultiLayerView::Layer::combineRenderTargets(int32_t colorSwapchainIndex, int32_t depthSwapchainIndex) const
{
    assert(colorSwapchainIndex != c_invalidIndex);

    // Get render targets for swapchain indices, depth being optional
    Renderer::RenderTarget& colorTarget = *m_colorRenderTargets[colorSwapchainIndex];
    Renderer::RenderTarget* depthTarget = (depthSwapchainIndex != c_invalidIndex) ? m_depthRenderTargets[depthSwapchainIndex].get() : nullptr;

    // Return combined target
    return depthTarget ? Renderer::ColorDepthRenderTarget(colorTarget, *depthTarget) : Renderer::ColorDepthRenderTarget(colorTarget);
}

void MultiLayerView::Layer::renderScene(const Scene& scene, const RenderParams& params) const
{
    // Check that update state is valid
    assert(m_updateState.state == State::Rendering);

    // Early return if no color buffer
    if (m_updateState.colorSwapChainIndex == c_invalidIndex) {
        return;
    }

    // Create combined render target
    Renderer::ColorDepthRenderTarget target = combineRenderTargets(m_updateState.colorSwapChainIndex, m_updateState.depthSwapChainIndex);

    // Render updated scene views to target
    renderSceneToTarget(target, scene, params);
}

bool MultiLayerView::Layer::isExternalTarget(Renderer::ColorDepthRenderTarget& target) const
{
    bool isExternal = true;

    for (auto& colorTarget : m_colorRenderTargets) {
        if (target.getColorTarget() == colorTarget.get()) {
            isExternal = false;
            break;
        }
    }

    return isExternal;
}

void MultiLayerView::Layer::renderSceneToTarget(Renderer::ColorDepthRenderTarget& target, const Scene& scene, const RenderParams& params) const
{
    // Update state can be other than State::Rendering here if we use external target as then we dont need view swapchain image
    assert(m_updateState.state == State::Rendering || (isExternalTarget(target) && m_updateState.state == State::Synchronized));

    // Early return if no color buffer
    if (!target.getColorTarget()) {
        return;
    }

    // Bind render target
    m_view.m_renderer.bindRenderTarget(target);

    // Iterate updated views for render
    for (auto i : m_updateState.views) {
        // Set viewport
        const varjo_Viewport& viewport = m_viewports[i];
        m_view.m_renderer.setViewport(viewport.x, viewport.y, viewport.width, viewport.height);

        // Render viewport
        scene.render(m_view.m_renderer, target, fromVarjoMatrix(m_multiProjViews[i].view), fromVarjoMatrix(m_multiProjViews[i].projection), params.userData);
    }

    // Unbind render target
    m_view.m_renderer.unbindRenderTarget();
}

void MultiLayerView::Layer::clear(const ClearParams& params)
{
    // Check that update state is valid
    assert(m_updateState.state == State::Rendering);

    // Early return if no color buffer
    if (m_updateState.colorSwapChainIndex == c_invalidIndex) {
        return;
    }

    // Create combined render target
    Renderer::ColorDepthRenderTarget target = combineRenderTargets(m_updateState.colorSwapChainIndex, m_updateState.depthSwapChainIndex);

    // Render updated scene views to target
    clearTarget(target, params);
}

void MultiLayerView::Layer::clearTarget(Renderer::ColorDepthRenderTarget& target, const ClearParams& params)
{
    // Update state can be other than State::Rendering here if we use external target as then we dont need view swapchain image
    assert(m_updateState.state == State::Rendering || (isExternalTarget(target) && m_updateState.state == State::Synchronized));

    // Early return if no color buffer
    if (!target.getColorTarget()) {
        return;
    }

    // Clear render targets
    m_view.m_renderer.clear(target, params.colorValue, params.clearColor, params.clearDepth, params.clearStencil, params.depthValue, params.stencilValue);
}

void MultiLayerView::Layer::updateViews(const varjo_FrameInfo* frameInfo)
{
    assert(m_updateState.state == State::Invalid);

    // Reset update state
    m_updateState = {};

    // Iterate views to update
    for (int i = 0; i < m_view.getViewCount(); i++) {
        // Get the view information for this frame.
        varjo_ViewInfo view = frameInfo->views[i];
        if (!view.enabled) {
            continue;  // Skip a view if it is not enabled.
        }
        m_updateState.views.push_back(i);

        // Change the near and far clip distances in projection matrix
        varjo_UpdateNearFarPlanes(view.projectionMatrix, varjo_ClipRangeZeroToOne, Layer::c_nearClipPlane, Layer::c_farClipPlane);

        // Copy view and projection matrices to multiproj views
        std::copy(std::begin(view.projectionMatrix), std::end(view.projectionMatrix), std::begin(m_multiProjViews[i].projection.value));
        std::copy(std::begin(view.viewMatrix), std::end(view.viewMatrix), std::begin(m_multiProjViews[i].view.value));

        // Reset optional view extensions
        m_multiProjViews[i].extension = nullptr;
    }

    // Set update state
    m_updateState.state = State::Synchronized;
}

void MultiLayerView::Layer::begin(const SubmitParams& params)
{
    // Check that frame synchronized, but not beginned yet
    assert(m_updateState.state == State::Synchronized);

    // Store params
    m_updateState.params = params;

    // Handle optional view extensions
    for (int i = 0; i < m_view.getViewCount(); i++) {
        // Reset view extensions
        m_multiProjViews[i].extension = nullptr;

        // Add depth view extension if depth submit enabled
        if (m_updateState.params.submitDepth) {
            linkExtensions(m_multiProjViews[i].extension, &m_extDepthViews[i].header, true);

            // Add depth test range extension if enabled
            if (m_updateState.params.depthTestRangeEnabled) {
                m_extDepthTestRangeViews[i].nearZ = m_updateState.params.depthTestRangeLimits[0];
                m_extDepthTestRangeViews[i].farZ = m_updateState.params.depthTestRangeLimits[1];

                linkExtensions(m_multiProjViews[i].extension, &m_extDepthTestRangeViews[i].header, true);
            }
        }

        // Add view extension chain from parameters
        if (params.viewExtensions.size() == m_view.getViewCount()) {
            linkExtensions(m_multiProjViews[i].extension, params.viewExtensions[i], false);
        }
    }

    // Get swap chain index only if layer submit is enabled. Depth index is always got with
    // color regardless if it gets submitted or not. This is required for using depth buffer
    // when rendering the color buffer.
    m_updateState.colorSwapChainIndex = m_updateState.depthSwapChainIndex = Layer::c_invalidIndex;
    if (m_updateState.params.submitColor) {
        varjo_AcquireSwapChainImage(m_colorSwapChain, &m_updateState.colorSwapChainIndex);
        varjo_AcquireSwapChainImage(m_depthSwapChain, &m_updateState.depthSwapChainIndex);
    }

    // Set update state
    m_updateState.state = State::Rendering;
}

void MultiLayerView::Layer::end()
{
    // Check that update state is valid
    assert(m_updateState.state == State::Rendering);

    // Only fill in submit info if we actually rendered layer
    if ((m_updateState.colorSwapChainIndex != Layer::c_invalidIndex) && (m_updateState.depthSwapChainIndex != Layer::c_invalidIndex)) {
        // Send the textures and viewport information to the compositor.
        // Also give the view information the frame was rendered with.
        varjo_LayerHeader header{};
        header.type = varjo_LayerMultiProjType;
        header.flags = 0;

        // Add additional layer flags from submit parameters
        header.flags |= m_updateState.params.layerFlags;

        // We need to set this flag to enable Varjo compositor to alpha blend
        // the VR content over VST image (or other VR layers). If we render opaque
        // VR background we dont want to set it.
        if (m_updateState.params.alphaBlend) {
            header.flags |= varjo_LayerFlag_BlendMode_AlphaBlend;
        }

        // Enable Varjo compositor to depth test this layer agains other layers (e.g. VST)
        // with depth information. Depth layer must be submitted for this to work.
        if (m_updateState.params.depthTestEnabled && m_updateState.params.submitDepth) {
            header.flags |= varjo_LayerFlag_DepthTesting;
        }

        // Enable chroma keying for this layer
        if (m_updateState.params.chromaKeyEnabled) {
            header.flags |= varjo_LayerFlag_ChromaKeyMasking;
        }

        m_multiProj.header = header;
        m_multiProj.space = varjo_SpaceLocal;
        m_multiProj.viewCount = m_view.m_viewCount;
        m_multiProj.views = m_multiProjViews.data();

        // Release swap chain images after rendering
        if (m_updateState.colorSwapChainIndex != c_invalidIndex) {
            varjo_ReleaseSwapChainImage(m_colorSwapChain);
            CHECK_VARJO_ERR(m_view.getSession());
        }
        if (m_updateState.depthSwapChainIndex != c_invalidIndex) {
            varjo_ReleaseSwapChainImage(m_depthSwapChain);
            CHECK_VARJO_ERR(m_view.getSession());
        }
    }

    // Set to finished state
    m_updateState.state = State::Finished;
}

glm::ivec2 MultiLayerView::Layer::getTotalSize(const std::vector<varjo_Viewport>& viewports)
{
    const int viewsPerRow = 2;
    int maxWidth = 0;
    for (size_t i = 0; i < viewports.size() / viewsPerRow; i++) {
        int rowWidth = 0;
        for (size_t k = 0; k < viewsPerRow; k++) {
            rowWidth += viewports[i * viewsPerRow + k].width;
        }

        maxWidth = std::max(maxWidth, rowWidth);
    }

    int32_t totalHeight = 0;
    for (size_t i = 0; i < viewports.size() / viewsPerRow; i++) {
        totalHeight += viewports[i * viewsPerRow].height;
    }

    return glm::ivec2(maxWidth, totalHeight);
}

// --------------------------------------------------------------------------

MultiLayerView::MultiLayerView(varjo_Session* session, Renderer& renderer)
    : SyncView(session, true)
    , m_renderer(renderer)
{
    // Number of views to render
    m_viewCount = varjo_GetViewCount(getSession());
}

MultiLayerView::~MultiLayerView()
{
    // Delete layers
    m_layers.clear();
}

void MultiLayerView::syncFrame()
{
    // Handle frame timing in base class
    SyncView::syncFrame();

    // Get frame info
    const varjo_FrameInfo* frameInfo = getFrameInfo();

    // Reset states and update layer views for new frame
    for (auto& layer : m_layers) {
        layer->m_updateState = {};
        layer->updateViews(frameInfo);
    }
}

void MultiLayerView::beginFrame()
{
    m_invalidated = false;

    // Begin rendering frame
    varjo_BeginFrameWithLayers(getSession());
    if (CHECK_VARJO_ERR(getSession()) != varjo_NoError) {
        return;
    }
}

MultiLayerView::Layer& MultiLayerView::getLayer(int index)
{
    // Get layer instance
    assert(index >= 0 && index < static_cast<int>(m_layers.size()));
    MultiLayerView::Layer& layer = *m_layers.at(index).get();

    // Return layer instance for rendering
    return layer;
}

void MultiLayerView::endFrame()
{
    m_invalidated = false;

    // Submit info structures
    varjo_SubmitInfoLayers submitInfoLayers{};
    std::vector<varjo_LayerHeader*> renderedLayers;

    // Reset submit info
    submitInfoLayers.frameNumber = getFrameInfo()->frameNumber;
    submitInfoLayers.layerCount = 0;
    submitInfoLayers.layers = nullptr;

    // End frame for each layer that has begun rendering
    for (auto& layer : m_layers) {
        assert(layer->m_updateState.state == Layer::State::Synchronized || layer->m_updateState.state == Layer::State::Finished);

        if (layer->m_updateState.state == Layer::State::Rendering) {
            // If layer was not ended, we'll end it now. Shound not ever happen, assert above will catch this on debug.
            layer->end();
        } else if (layer->m_updateState.state == Layer::State::Finished) {
            // Add finished layer header
            renderedLayers.emplace_back(&layer->m_multiProj.header);
        }

        // Reset layer state
        layer->m_updateState.state = {};
    }

    // Add frame layers to submit info
    submitInfoLayers.layerCount = static_cast<int32_t>(renderedLayers.size());
    submitInfoLayers.layers = renderedLayers.data();

    // Finish frame and submit layers info
    varjo_EndFrameWithLayers(getSession(), &submitInfoLayers);
    CHECK_VARJO_ERR(getSession());
}

void MultiLayerView::invalidateFrame()
{
    // No op if already invalidated
    if (m_invalidated) {
        return;
    }

    // Submit empty frame
    syncFrame();
    beginFrame();
    endFrame();

    m_invalidated = true;
}

}  // namespace VarjoExamples
