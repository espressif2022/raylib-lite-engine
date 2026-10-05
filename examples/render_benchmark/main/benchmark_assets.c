// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_assets.h"
/* Test data is generated in RAM; accidental file loads fail explicitly. */
esp_err_t mosaico_game_asset_open(const char *name,mosaico_asset_view_t *out)
{ (void)name;(void)out;return ESP_ERR_NOT_SUPPORTED; }
void mosaico_game_asset_release(mosaico_asset_view_t *view)
{ if(view){view->data=NULL;view->size=0;} }
