// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Native upstream raylib resource adapter. Never mix native texture/font
 * handles with the lightweight renderer or compatibility-facade handles. */
#ifdef RAYLIB_LITE_RAYLIB_COMPAT
#error "raylib_lite_rcore_assets.h requires native raylib, not the compatibility facade"
#endif
#define RAYLIB_LITE_RCORE_PLATFORM 1
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opt in to loading original PNG/FNT/audio/etc. through the asset core.
 * logical_root is a relative asset-name prefix ("" or "." selects its root),
 * not a filesystem path. All requested paths remain inside that namespace.
 * '/' separators, '.' and in-root '..' are normalized. Absolute paths,
 * backslashes, drive/protocol prefixes and paths longer than 255 bytes fail.
 * Relative FNT page paths are resolved by upstream raylib before this adapter.
 *
 * The application must exclusively own SetLoadFileDataCallback and
 * SetLoadFileTextCallback while attached, and serialize attach/detach and
 * resource loads. An already-attached adapter returns BUSY. No filesystem
 * fallback occurs while attached. detach is idempotent and restores NULL
 * callbacks (upstream exposes no getter for previous callbacks).
 *
 * Each successful read copies bytes into MemAlloc-owned memory and releases
 * its asset view immediately. Normal UnloadFileData/UnloadFileText frees the
 * copy, even after detach/unmount. Font/texture handles remain native raylib.
 * Mount/register the original bytes before loading; packed MSA1/RLF1 resources
 * instead belong to raylib_lite_font/renderer APIs. */
raylib_lite_result_t raylib_lite_rcore_assets_attach(const char *logical_root);
void raylib_lite_rcore_assets_detach(void);
/* Result of the most recent adapter read/attach; detach sets NOT_READY.
 * Check this after native LoadFont because upstream can return its default
 * font when a requested resource fails. No font-fallback policy is changed. */
raylib_lite_result_t raylib_lite_rcore_assets_last_result(void);

#ifdef __cplusplus
}
#endif
