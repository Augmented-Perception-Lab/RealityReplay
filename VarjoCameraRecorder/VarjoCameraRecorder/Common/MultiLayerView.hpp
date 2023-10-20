// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <array>

#include <Varjo_layers.h>
#include <Varjo_types_layers.h>

#include "Globals.hpp"
#include "Renderer.hpp"
#include "SyncView.hpp"
#include "Scene.hpp"

namespace VarjoExamples
{
//! Graphics API independent base class for multi layer view
class MultiLayerView : public SyncView
{
public:
    //! Graphics API independent base class for MultiLayerView layer.
    class Layer
    {
    public:
        //! Clearing parameters structure
        struct ClearParams {
            //! Default constructor
            ClearParams() = default;

            //! Convinience constructor for clearing with given color
            ClearParams(const glm::vec4& colorClearValue)
                : colorValue(colorClearValue)
            {
            }

            //! Convinience constructor for setting clear flags and optionally color
            ClearParams(bool clear, const glm::vec4& colorClearValue = {0.0f, 0.0f, 0.0f, 0.0f})
                : clearColor(clear)
                , clearDepth(clear)
                , clearStencil(clear)
                , colorValue(colorClearValue)
            {
            }

            bool clearColor{true};                         //!< Clear color buffer flag
            bool clearDepth{true};                         //!< Clear depth buffer flag
            bool clearStencil{true};                       //!< Clear stencil buffer flag
            glm::vec4 colorValue{0.0f, 0.0f, 0.0f, 0.0f};  //!< Color clear value
            float depthValue{1.0f};                        //!< Depth clear value
            uint8_t stencilValue{0};                       //!< Stencil clear value
        };

        //! Render parameters struct
        struct RenderParams {
            //! Default constructor
            RenderParams() = default;

            //! Constructor for setting passId and user data
            RenderParams(int passId, void* userData = nullptr)
                : passId(passId)
                , userData(userData)
            {
            }

            int passId{0};            //!< Render pass ID to be passed to scene
            void* userData{nullptr};  //!< User data pointer to be passed to scene render function
        };

        //! Layer frame submit parameters
        struct SubmitParams {
            bool submitColor{true};                                    //!< Enable color swapchain submit
            bool submitDepth{true};                                    //!< Enable depth swapchain submit (requires color)
            bool alphaBlend{true};                                     //!< Enable alpha blending for submitted VR layer
            bool depthTestEnabled{false};                              //!< Enable depth testing in compositor for submitted VR layer
            bool chromaKeyEnabled{false};                              //!< Enable chroma key masking in compositor for submitted VR layer
            bool depthTestRangeEnabled{false};                         //!< Enable depth test range limits for submitted VR layer
            std::array<double, 2> depthTestRangeLimits{0.0, DBL_MAX};  //!< Depth test minimum and maximum limits for submitted VR layer
            std::vector<varjo_ViewExtension*> viewExtensions;          //!< Optional pointers to external view extension header chains
            varjo_LayerFlags layerFlags{0};                            //!< Optional additional layer flags
        };

        //! Destructor
        virtual ~Layer();

        //! Return rendertarget size
        glm::ivec2 getSize() const { return m_colorRenderTargets[0]->getSize(); }

        //! Return viewport for given view index
        const varjo_Viewport& getViewport(int i) const { return m_viewports.at(i); }

        //! Return projection for given view index
        const varjo_Matrix& getProjection(int i) const { return m_multiProjViews.at(i).projection; }

        //! Begin layer rendering for frame submit. This must be called before rendering to view rendertarget using renderScene() or clear().
        void begin(const SubmitParams& params);

        //! End layer rendering for frame submit. This must be called after view rendering is finished.
        void end();

        //! Render scene views to Varjo view render target. This requires that begin() was called before.
        void renderScene(const Scene& scene, const RenderParams& renderParams = {}) const;

        //! Clear Varjo view render target. This requires that begin() was called before.
        void clear(const ClearParams& clearParams = {});

        // NOTICE!
        // If the following functions are used for rendering to external render target, begin()/end() calls are not needed.
        // This is because in that case layer is not submitted to Varjo view. MultiLayerView::syncFrame() must have been
        // called though, so that layer views get updated before.

        //! Render scene views to given possibly external render target using view, clipping and projections from this layer.
        void renderSceneToTarget(Renderer::ColorDepthRenderTarget& target, const Scene& scene, const RenderParams& renderParams = {}) const;

        //! Clear given given possibly external render target using view, clipping and projections from this layer.
        void clearTarget(Renderer::ColorDepthRenderTarget& target, const ClearParams& clearParams = {});

    protected:
        //! Protected constructor
        Layer(MultiLayerView& view, int contextDivider, int focusDivider);

        //! Setup multi view structures. Called from implementing class constructor.
        void setupViews();

        //! Returns size of a texture which can cover all specified viewports
        static glm::ivec2 getTotalSize(const std::vector<varjo_Viewport>& viewports);

    private:
        //! Update layer views for new frame
        void updateViews(const varjo_FrameInfo* frameInfo);

        //! Get combined render target for given color and depth swapchain indices
        Renderer::ColorDepthRenderTarget combineRenderTargets(int32_t colorSwapchainIndex, int32_t depthSwapchainIndex) const;

        //! Calculates viewports of all views in an atlas, 2 views per row
        std::vector<varjo_Viewport> calculateViewports(varjo_Session* session, int contextDivider, int focusDivider) const;

        //! Check if target is external
        bool isExternalTarget(Renderer::ColorDepthRenderTarget& target) const;

    protected:
        static const int32_t c_invalidIndex{-1};  //!< Invalid swap chain index constant
        const double c_nearClipPlane{0.1};        //!< Near clip plane constant
        const double c_farClipPlane{1000.0};      //!< Far clip plane constant

        MultiLayerView& m_view;              //!< View instance
        varjo_LayerMultiProj m_multiProj{};  //!< Multi projection layer data

        varjo_SwapChain* m_colorSwapChain{nullptr};                                                //!< Swapchain for color buffers
        varjo_SwapChain* m_depthSwapChain{nullptr};                                                //!< Swapchain for client depth
        std::vector<std::unique_ptr<VarjoExamples::Renderer::RenderTarget>> m_colorRenderTargets;  //!< Render target for each color swapchain image
        std::vector<std::unique_ptr<VarjoExamples::Renderer::RenderTarget>> m_depthRenderTargets;  //!< Render target for each depth swapchain image

        // View specific data
        std::vector<varjo_Viewport> m_viewports;                                  //!< View port layout for each view
        std::vector<varjo_LayerMultiProjView> m_multiProjViews;                   //!< Multi projection views for each view
        std::vector<varjo_ViewExtensionDepth> m_extDepthViews;                    //!< Client depth extension for each view
        std::vector<varjo_ViewExtensionDepthTestRange> m_extDepthTestRangeViews;  //!< Client depth test range extension for each view

        //! Update state enumeration
        enum class State {
            Invalid = 0,   //!< Invalidated state
            Synchronized,  //!< Frame synchronized, but submit not begun
            Rendering,     //!< Layer submit begun
            Finished,      //!< Layer submit finished
        };

        //! Update state structure
        struct {
            State state{State::Invalid};                  //!< Layer update state
            std::vector<int> views;                       //!< List of updated views to be rendered
            int32_t colorSwapChainIndex{c_invalidIndex};  //!< Swap chain index for rendering color
            int32_t depthSwapChainIndex{c_invalidIndex};  //!< Swap chain index for rendering depth
            SubmitParams params{};                        //!< Layer submit parameters
        } m_updateState;

        friend class MultiLayerView;
    };

    //! Destructor
    ~MultiLayerView() override;

    // Disable copy, move and assign
    MultiLayerView(const MultiLayerView& other) = delete;
    MultiLayerView(const MultiLayerView&& other) = delete;
    MultiLayerView& operator=(const MultiLayerView& other) = delete;
    MultiLayerView& operator=(const MultiLayerView&& other) = delete;

    //! Return view count
    int getViewCount() { return m_viewCount; }

    //! Get layer instance for given index
    MultiLayerView::Layer& getLayer(int index);

    //! From SyncFrame. Called to sync varjo frame before rendering.
    void syncFrame() override;

    //! Begin varjo frame. This must be called after syncFrame() and before rendering any layers
    void beginFrame();

    //! End varjo frame. This must be called after rendering to submit the frame.
    void endFrame();

    //! Submit empty frame to invalidate client rendering
    void invalidateFrame();

protected:
    //! Protected constructor
    MultiLayerView(varjo_Session* session, Renderer& renderer);

protected:
    std::vector<std::unique_ptr<MultiLayerView::Layer>> m_layers;  //!< Layer instances

private:
    Renderer& m_renderer;       //!< Renderer reference
    int32_t m_viewCount{0};     //!< Number of views
    bool m_invalidated{false};  //!< Frame was invalidated

    friend class MultiLayerView::Layer;
};

}  // namespace VarjoExamples
