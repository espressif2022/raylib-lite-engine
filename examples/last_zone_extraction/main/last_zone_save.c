// SPDX-License-Identifier: Apache-2.0
#include "last_zone_save.h"
#include <string.h>
#include "raylib_lite_clock.h"
#include "raylib_lite_save.h"

#define LAST_ZONE_SAVE_VERSION 2U

static raylib_lite_save_t s_save;
static bool s_ready;

static uint64_t now_ms(void)
{
    return raylib_lite_time_us() / 1000U;
}

static raylib_lite_result_t migrate_v1(uint16_t old_version, const void *old_data, size_t old_size,
                            void *new_data, size_t new_size)
{
    last_zone_campaign_t *campaign = new_data;
    memset(campaign, 0, new_size);
    if (old_version == 1U && old_size >= sizeof(uint32_t) && old_data)
        campaign->best_ticks = *(const uint32_t *)old_data;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t ensure_ready(void)
{
    if (s_ready) return RAYLIB_LITE_OK;
    raylib_lite_save_config_t config = {
        .storage = raylib_lite_save_nvs_storage(),
        .storage_namespace = "neon_maze",
        .key = "best",
        .version = LAST_ZONE_SAVE_VERSION,
        .payload_size = sizeof(last_zone_campaign_t),
        .debounce_ms = 750,
        .migrate = migrate_v1,
    };
    raylib_lite_result_t result = raylib_lite_save_init(&s_save, &config);
    s_ready = result == RAYLIB_LITE_OK;
    return result;
}

void last_zone_campaign_from_game(const last_zone_game_t *game,
                                 last_zone_campaign_t *campaign)
{
    if (!campaign) return;
    memset(campaign, 0, sizeof(*campaign));
    if (!game) return;
    campaign->best_ticks = game->best_ticks;
    memcpy(campaign->layout_best, game->layout_best, sizeof(campaign->layout_best));
    campaign->layout = game->layout;
    campaign->unlocked = game->unlocked;
    campaign->grades[game->layout < LAST_ZONE_LAYOUTS ? game->layout : 0] =
        (uint8_t)last_zone_grade(game);
}

void last_zone_campaign_apply(last_zone_game_t *game,
                              const last_zone_campaign_t *campaign)
{
    if (!game || !campaign) return;
    game->best_ticks = campaign->best_ticks;
    memcpy(game->layout_best, campaign->layout_best, sizeof(game->layout_best));
    game->layout = campaign->layout < LAST_ZONE_LAYOUTS ? campaign->layout : 0;
    game->unlocked = campaign->unlocked;
}

raylib_lite_result_t last_zone_save_load(last_zone_campaign_t *campaign)
{
    if (!campaign) return RAYLIB_LITE_INVALID_ARGUMENT;
    memset(campaign, 0, sizeof(*campaign));
    raylib_lite_result_t result = ensure_ready();
    if (result != RAYLIB_LITE_OK) return result;
    result = raylib_lite_save_load(&s_save, campaign, NULL);
    if (result == RAYLIB_LITE_INVALID_CRC || result == RAYLIB_LITE_INVALID_VERSION) {
        memset(campaign, 0, sizeof(*campaign));
        return RAYLIB_LITE_OK;
    }
    return result;
}

raylib_lite_result_t last_zone_save_campaign(const last_zone_campaign_t *campaign)
{
    if (!campaign) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_result_t result = ensure_ready();
    return result == RAYLIB_LITE_OK ? raylib_lite_save_request(&s_save, campaign, now_ms()) : result;
}

raylib_lite_result_t last_zone_save_flush(void)
{
    return s_ready ? raylib_lite_save_flush(&s_save, now_ms(), false) : RAYLIB_LITE_OK;
}
