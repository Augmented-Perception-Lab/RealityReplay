
#pragma once

#include <wrl/client.h>
#include <functional>
#include <imgui.h>

#include "Globals.hpp"

// Forward declarations
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

using Microsoft::WRL::ComPtr;

namespace VarjoExamples
{
//! Generic user interface wrapper class for D3D11 based ImGui
class UI
{
public:
    //! Frame callback type
    using FrameCallback = std::function<bool(UI&)>;

    //! Keyboard callback type
    using KeyCallback = std::function<void(UI&, unsigned int)>;

public:
    //! Constructor
    UI(const FrameCallback& frameCallback, const KeyCallback& keyCallback, const std::wstring& title, int width, int height);

    //! Destructor
    ~UI();

    //! Run. Starts message loop.
    void run();

    //! Called on window resize
    void onResize(int width, int height);

    //! Called on key press
    void onKey(int keyCode);

    //! Write log message
    void writeLogEntry(LogLevel logLevel, const std::string& logLine);

    //! Draw log buffers
    void drawLog();

    //! Returns window handle
    HWND getWindowHandle() const { return m_hWnd; }

private:
    //! Create window
    void createWindow(const std::wstring& title, int width, int height);

    //! Initialize ui
    void initializeUi();

    //! Create rendertarget
    void createRenderTarget();

    //! Create swapchain
    void createSwapchain();

private:
    FrameCallback m_frameCallback;                         //!< Frame update callback
    KeyCallback m_keyCallback;                             //!< Keyboard callback
    WNDCLASSEX m_wc;                                       //!< Window class
    HWND m_hWnd;                                           //!< Window handle
    ComPtr<ID3D11Device> m_d3dDevice;                      //!< D3D device
    ComPtr<ID3D11DeviceContext> m_d3dDeviceContext;        //!< D3D device context
    ComPtr<IDXGISwapChain> m_d3dSwapChain;                 //!< Swap chain
    ComPtr<ID3D11RenderTargetView> m_d3dRenderTargetView;  //!< Render target
    ImGuiTextBuffer m_logBuf;                              //!< Log buffer
    bool m_scrollLog = true;                               //!< Scroll log to end flag
};

}  // namespace VarjoExamples
