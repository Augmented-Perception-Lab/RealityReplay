
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <dxgi1_2.h>
#include <tchar.h>
#include <stdexcept>
#include <array>
#include <unordered_map>
#include <iostream>

#include "UI.hpp"

#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

namespace
{
static std::unordered_map<HWND, VarjoExamples::UI*> g_instances;
}  // namespace

// --------------------------------------------------------------------------

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    VarjoExamples::UI* ui = nullptr;
    if (g_instances.count(hWnd)) {
        ui = g_instances.at(hWnd);
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
        case WM_SIZE: {
            auto width = (UINT)LOWORD(lParam);
            auto height = (UINT)HIWORD(lParam);
            if (wParam != SIZE_MINIMIZED) {
                if (ui) {
                    ui->onResize(width, height);
                }
            }
            return 0;
        } break;

        case WM_SYSCOMMAND: {
            // Disable ALT application menu
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
        } break;

        case WM_DESTROY: {
            ::PostQuitMessage(0);
            return 0;
        } break;

        case WM_KEYDOWN: {
            if (ui) {
                ui->onKey(static_cast<int>(wParam));
            }
        } break;

        default: {
            // Ignore
        } break;
    }

    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// --------------------------------------------------------------------------

namespace VarjoExamples
{
UI::UI(const FrameCallback& frameCallback, const KeyCallback& keyCallback, const std::wstring& title, int width, int height)
    : m_frameCallback(frameCallback)
    , m_keyCallback(keyCallback)
{
    // Create appliction window
    createWindow(title, width, height);

    // Create device and context
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL featureLevel;
    std::array<D3D_FEATURE_LEVEL, 1> featureLevelArray = {D3D_FEATURE_LEVEL_11_0};
    CHECK_HRESULT(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray.data(),
        static_cast<UINT>(featureLevelArray.size()), D3D11_SDK_VERSION, &m_d3dDevice, &featureLevel, &m_d3dDeviceContext));

    // Create swapchain
    createSwapchain();

    // Create rendertarget
    createRenderTarget();

    // Initialize
    initializeUi();

    // Add instance
    g_instances.emplace(std::make_pair(m_hWnd, this));

    LOG_DEBUG("UI initialized.");
}

UI::~UI()
{
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_d3dRenderTargetView.Reset();
    m_d3dSwapChain.Reset();
    m_d3dDeviceContext.Reset();
    m_d3dDevice.Reset();

    ::DestroyWindow(m_hWnd);
    ::UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);

    g_instances.erase(m_hWnd);
}

void UI::createWindow(const std::wstring& title, int width, int height)
{
    // Register window class
    m_wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(NULL),
        NULL,
        NULL,
        NULL,
        NULL,
        _T("Varjo Application"),
        NULL,
    };
    ::RegisterClassEx(&m_wc);

    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, dwStyle, FALSE);
    auto w = rect.right - rect.left;
    auto h = rect.bottom - rect.top;

    // Create application window
    m_hWnd = ::CreateWindow(m_wc.lpszClassName, title.data(), dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, w, h, NULL, NULL, m_wc.hInstance, NULL);
}

void UI::createRenderTarget()
{
    ComPtr<ID3D11Texture2D> pBackBuffer;
    CHECK_HRESULT(m_d3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)));
    CHECK_HRESULT(m_d3dDevice->CreateRenderTargetView(pBackBuffer.Get(), NULL, &m_d3dRenderTargetView));
}

void UI::createSwapchain()
{
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;    // DXGI device pointer
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;   // DXGI adapter pointer
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;  // DXGI factory pointer

    // Create the DXGI device object to use in other factories, such as Direct2D.
    m_d3dDevice.As(&dxgiDevice);

    // Get adapter
    CHECK_HRESULT(dxgiDevice->GetAdapter(&dxgiAdapter));

    // Get factory pointer
    CHECK_HRESULT(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

    // Setup swapchain
    DXGI_SWAP_CHAIN_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = m_hWnd;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Windowed = TRUE;

    // Create swapchain
    CHECK_HRESULT(dxgiFactory->CreateSwapChain(m_d3dDevice.Get(), &desc, &m_d3dSwapChain));
}

void UI::initializeUi()
{
    // Show the window
    ::ShowWindow(m_hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hWnd);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dDeviceContext.Get());
}

void UI::run()
{
    LOG_DEBUG("UI main loop started.");

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Do frame callback
        if (m_frameCallback != nullptr) {
            if (!m_frameCallback(*this)) {
                LOG_DEBUG("Got quit from frame callback.");
                break;
            }
        }

        // Rendering
        ImGui::Render();
        std::array<ID3D11RenderTargetView*, 1> renderTargets = {m_d3dRenderTargetView.Get()};
        m_d3dDeviceContext->OMSetRenderTargets(static_cast<UINT>(renderTargets.size()), renderTargets.data(), NULL);
        std::array<float, 4> clearColor = {0, 0, 0, 0};
        m_d3dDeviceContext->ClearRenderTargetView(m_d3dRenderTargetView.Get(), clearColor.data());
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present with vsync OFF (We sync to Varjo API instead).
        constexpr bool vsync = false;
        m_d3dSwapChain->Present(vsync ? 1 : 0, 0);
    }

    LOG_DEBUG("UI main loop finished.");
}

void UI::onResize(int width, int height)
{
    if (m_d3dDevice != nullptr) {
        m_d3dRenderTargetView.Reset();
        CHECK_HRESULT(m_d3dSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
        createRenderTarget();
    }
}

void UI::onKey(int keyCode)
{
    if (m_keyCallback) {
        m_keyCallback(*this, keyCode);
    }
}

void UI::drawLog()
{
    ImGui::TextUnformatted(m_logBuf.begin());
    if (m_scrollLog) {
        ImGui::SetScrollHereY(1.0f);
    }
    m_scrollLog = false;
}

void UI::writeLogEntry(LogLevel logLevel, const std::string& line)
{
    // Write to ui log buffer
    m_logBuf.appendf(line.c_str());
    m_logBuf.appendf("\n");
    m_scrollLog = true;
}

}  // namespace VarjoExamples
