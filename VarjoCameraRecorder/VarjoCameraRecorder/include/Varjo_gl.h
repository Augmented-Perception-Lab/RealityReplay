// Copyright 2019-2021 Varjo Technologies Oy. All rights reserved.

#ifndef VARJO_GL_H
#define VARJO_GL_H

#include "Varjo.h"
#include "Varjo_types_gl.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Initializes OpenGL rendering system.
 *
 * If the application is submitting manually created swapchains using #varjo_EndFrameWithLayers,
 * calling this function (or #varjo_D3D11Init) is not necessary and is not recommended.
 *
 * All varjo_GL* functions may use OpenGL context and should thus
 * be either invoked from the rendering thread.
 *
 * Varjo system automatically frees the returned varjo_GraphicsInfo struct
 * when #varjo_GLShutDown or #varjo_SessionShutDown is called.
 *
 * @param session Varjo session handle.
 * @param format Texture format.
 * @param config Swap chain config. If null, uses the default config.
 * @return Information required to setup rendering and per-frame data.
 * @deprecated use varjo_GLCreateSwapChain()
 */
VARJO_DEPRECATED_API struct varjo_GraphicsInfo* varjo_GLInit(struct varjo_Session* session, varjo_TextureFormat format, struct varjo_SwapChainConfig* config);

/**
 * Converts an OpenGL texture to #varjo_Texture.
 */
VARJO_API struct varjo_Texture varjo_FromGLTexture(GLuint texture);

/**
 * Converts a Varjo texture to OpenGL texture.
 */
VARJO_API GLuint varjo_ToGLTexture(struct varjo_Texture texture);

/**
 * Closes the OpenGL connection.
 *
 * This function should only be called if the application has previously
 * called #varjo_GLInit on this varjo_Session
 * @param session Varjo session handle.
 * @deprecated use varjo_FreeSwapChain()
 */
VARJO_DEPRECATED_API void varjo_GLShutDown(struct varjo_Session* session);

#if defined __cplusplus
}
#endif

#endif  // VARJO_GL_H
