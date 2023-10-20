// Copyright 2019 Varjo Technologies Oy. All rights reserved.

#ifndef VARJO_D3D11_H
#define VARJO_D3D11_H

#include "Varjo.h"
#include "Varjo_types_d3d11.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Sets up the Direct3D device that the SDK will use for texture communication.
 *
 * If the application is submitting manually created swapchains using #varjo_EndFrameWithLayers,
 * calling this function (or #varjo_GLInit) is not necessary and is not recommended.
 *
 * All varjo_D3D* functions may use D3D immediate device context and should thus
 * be either invoked from the rendering thread or synchronized otherwise. The
 * D3D device may be switched to another one once the old one has been shut
 * down.
 *
 * Varjo system automatically frees the returned varjo_GraphicsInfo struct
 * when #varjo_D3D11ShutDown or #varjo_SessionShutDown is called.
 *
 * @param session Varjo session handle.
 * @param dev D3D11 device.
 * @param format Texture format.
 * @param config Swap chain config. If null, uses the default config.
 * @return Information required to setup rendering and per-frame data. Instance of
 *         varjo_GraphicsInfo is owned by runtime and doesn't have to be freed.
 * @deprecated use varjo_D3D11CreateSwapChain()
 */
VARJO_DEPRECATED_API struct varjo_GraphicsInfo* varjo_D3D11Init(
    struct varjo_Session* session, struct ID3D11Device* dev, varjo_TextureFormat format, struct varjo_SwapChainConfig* config);

/**
 * Converts a Direct3D 11 texture to #varjo_Texture.
 */
VARJO_API struct varjo_Texture varjo_FromD3D11Texture(struct ID3D11Texture2D* texture);

/**
 * Converts a Varjo texture to Direct3D 11 texture.
 * This is a deprecated version of #varjo_ToD3D11Texture.
 */
VARJO_DEPRECATED_API struct ID3D11Texture2D* varjo_ToD3D11texture(struct varjo_Texture texture);

/**
 * Converts a Varjo texture to Direct3D 11 texture.
 */
VARJO_API struct ID3D11Texture2D* varjo_ToD3D11Texture(struct varjo_Texture texture);

/**
 * Closes the Direct3D connection.
 *
 * Frees the memory allocated for varjo_D3D11Init graphics info.
 * This function should only be called if the application has previously called
 * #varjo_D3D11Init on this varjo_Session.
 *
 * @param session Varjo session handle.
 * @deprecated use varjo_FreeSwapChain()
 */
VARJO_DEPRECATED_API void varjo_D3D11ShutDown(struct varjo_Session* session);

/**
 * Retrieves LUID of the device which is used by compositor.
 *
 * Application has to use the same device to work correctly.
 *
 * @param session Varjo session handle.
 * @return LUID of DirectX device which compositor is using.
 */
VARJO_API struct varjo_Luid varjo_D3D11GetLuid(struct varjo_Session* session);

#if defined __cplusplus
}
#endif

#if defined DIRECTX_MATH_VERSION
/**
 * Convert Varjo double matrix to DirectX XMMATRIX.
 */
static DirectX::XMMATRIX varjo_DoubleArrayToXMMatrix(double* m)
{
    return DirectX::XMMATRIX{
        (float)m[0],
        (float)m[1],
        (float)m[2],
        (float)m[3],
        (float)m[4],
        (float)m[5],
        (float)m[6],
        (float)m[7],
        (float)m[8],
        (float)m[9],
        (float)m[10],
        (float)m[11],
        (float)m[12],
        (float)m[13],
        (float)m[14],
        (float)m[15],
    };
}

/**
 * Convert Varjo 3x3 double matrix to DirectX XMMATRIX.
 */
static DirectX::XMMATRIX varjo_DoubleArray3x3ToXMMatrix(double* m)
{
    return DirectX::XMMATRIX{
        (float)m[0],
        (float)m[1],
        (float)m[2],
        0.0f,
        (float)m[3],
        (float)m[4],
        (float)m[5],
        0.0f,
        (float)m[6],
        (float)m[7],
        (float)m[8],
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
    };
}
#endif

#endif
