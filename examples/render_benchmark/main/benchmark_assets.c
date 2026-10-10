// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_assets.h"
/* Test data is generated in RAM; accidental file loads fail explicitly. */
raylib_lite_result_t raylib_lite_asset_open(const char *name,raylib_lite_asset_view_t *out)
{ (void)name;(void)out;return RAYLIB_LITE_NOT_SUPPORTED; }
void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{ if(view){view->data=NULL;view->size=0;} }
