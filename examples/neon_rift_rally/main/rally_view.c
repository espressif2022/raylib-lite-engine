// SPDX-License-Identifier: Apache-2.0
#include "rally_view.h"

#include <math.h>

#include "assets_ids.h"
#include "mosaico_raylib_fast.h"

#define RALLY_W 480
#define RALLY_H 480
#define RALLY_HORIZON 224
#define RALLY_NEAR_Z 0.15f
#define RALLY_SEGMENTS 22
#define RALLY_FAR_Z 70.0f
#define RALLY_FOCAL 252.0f

static const Color C_CYAN = {44, 235, 255, 255};
static const Color C_MINT = {72, 255, 190, 255};
static const Color C_PINK = {255, 62, 181, 255};
static const Color C_GOLD = {255, 200, 70, 255};

typedef struct {
    Color sky_top;
    Color sky_bottom;
    Color road;
    Color terrain;
    Color accent;
    Color accent2;
} rally_theme_t;

typedef struct {
    float x, y, z;
} rally_point3_t;

typedef struct {
    float x, y;
    bool visible;
} rally_point2_t;

static float clampf(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static rally_theme_t theme_for(float progress)
{
    /* Each third of the closed course changes the light language.  The track
     * mesh stays identical, so this is effectively free geometry-wise. */
    float p = fmodf(progress, RALLY_TRACK_LENGTH);
    if (p < 0.0f) p += RALLY_TRACK_LENGTH;
    if (p < RALLY_TRACK_LENGTH * .34f) {
        return (rally_theme_t){{3, 8, 31, 255}, {92, 35, 106, 255},
                               {18, 27, 47, 255}, {31, 22, 62, 255},
                               C_CYAN, C_PINK};
    }
    if (p < RALLY_TRACK_LENGTH * .68f) {
        return (rally_theme_t){{22, 11, 35, 255}, {198, 73, 65, 255},
                               {39, 32, 43, 255}, {69, 31, 54, 255},
                               C_GOLD, C_PINK};
    }
    return (rally_theme_t){{3, 17, 37, 255}, {27, 105, 126, 255},
                           {17, 34, 48, 255}, {19, 55, 65, 255},
                           C_MINT, C_CYAN};
}

static Color color_lerp(Color a, Color b, float t)
{
    t = clampf(t, 0.0f, 1.0f);
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t),
                   (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), 255};
}

static bool track_world(const rally_game_t *g, float distance, float lateral,
                        float height, rally_point3_t *out)
{
    rally_track_pose_t pose;
    if (!rally_track_sample(g->progress + distance, lateral, &pose)) return false;
    out->x = pose.x;
    out->y = pose.y + height;
    out->z = pose.z;
    return true;
}

static rally_point2_t project(const rally_game_t *g, rally_point3_t p)
{
    rally_track_pose_t cam;
    if (!rally_track_sample(g->progress, g->lateral, &cam))
        return (rally_point2_t){0, 0, false};
    float yaw = g->heading_error;
    float right_x = cam.right_x * cosf(yaw) + cam.tangent_x * sinf(yaw);
    float right_z = cam.right_z * cosf(yaw) + cam.tangent_z * sinf(yaw);
    float fwd_x = cam.tangent_x * cosf(yaw) - cam.right_x * sinf(yaw);
    float fwd_z = cam.tangent_z * cosf(yaw) - cam.right_z * sinf(yaw);
    /* A short projection-space chase offset allows the nearest ribbon slice
     * to grow all the way below the cockpit instead of ending in a static
     * lower-screen road fill. */
    float dx = p.x - (cam.x - cam.tangent_x * 1.40f);
    /* Low chase camera: the car fills the lower frame while the road still
     * exposes enough horizon to read the next bend. */
    float dy = p.y - (cam.y + 1.55f + g->height * 0.16f);
    float dz = p.z - (cam.z - cam.tangent_z * 1.40f);
    float vx = dx * right_x + dz * right_z;
    float view_z = dx * fwd_x + dz * fwd_z;
    float vy = dy;
    if (view_z < RALLY_NEAR_Z) return (rally_point2_t){0, 0, false};
    float sx = 240.0f + RALLY_FOCAL * vx / view_z;
    float sy_screen = (float)RALLY_HORIZON - RALLY_FOCAL * vy / view_z;
    float roll = -cam.bank * 0.42f;
    float cr = cosf(roll), sr = sinf(roll);
    float ox = sx - 240.0f, oy = sy_screen - (float)RALLY_HORIZON;
    sx = 240.0f + ox * cr - oy * sr;
    sy_screen = (float)RALLY_HORIZON + ox * sr + oy * cr;
    /* Keep horizontally off-screen vertices valid. The rasterizer clips the
     * resulting triangle; rejecting them here would drop the near road quad
     * exactly when its shoulders expand past both display edges. */
    return (rally_point2_t){sx, sy_screen,
                            sy_screen > -100.0f && sy_screen < 540.0f};
}

static void draw_sky(const rally_game_t *g)
{
    /* Broad RGB565-safe bands plus a few large silhouettes create a proper
     * authored vista without paying for a 460 KiB full-screen background. */
    rally_theme_t theme = theme_for(g->progress);
    for (int i = 0; i < 12; ++i) {
        float t = i / 11.0f;
        Color band = color_lerp(theme.sky_top, theme.sky_bottom, t * t);
        DrawRectangle(0, i * 18, RALLY_W, 19, band);
    }

    float phase = fmodf(g->progress * .025f, 480.0f);
    int sun_x = 98 + (int)(phase * .08f);
    Color sun = color_lerp(theme.accent2, C_GOLD, .62f);
    DrawCircle(sun_x, 139, 28, (Color){sun.r, sun.g, sun.b, 255});
    DrawCircle(sun_x, 139, 20, color_lerp(sun, RAYWHITE, .42f));
    for (int i = 0; i < 5; ++i)
        DrawRectangle(sun_x - 31, 128 + i * 9, 62, 3 + i,
                      color_lerp(theme.sky_bottom, theme.terrain, .35f));

    /* Three depth layers, with the skyline offset very slightly by progress,
     * are enough to imply parallax while retaining large contiguous spans. */
    DrawTriangle((Vector2){-32, 203}, (Vector2){75, 126}, (Vector2){191, 203},
                 color_lerp(theme.terrain, theme.sky_bottom, .30f));
    DrawTriangle((Vector2){130, 203}, (Vector2){266, 112}, (Vector2){408, 203},
                 color_lerp(theme.terrain, theme.sky_bottom, .18f));
    DrawTriangle((Vector2){315, 203}, (Vector2){426, 137}, (Vector2){520, 203},
                 color_lerp(theme.terrain, theme.sky_bottom, .27f));
    DrawTriangle((Vector2){-50, 218}, (Vector2){115, 158}, (Vector2){290, 218},
                 theme.terrain);
    DrawTriangle((Vector2){214, 218}, (Vector2){356, 151}, (Vector2){535, 218},
                 color_lerp(theme.terrain, theme.road, .18f));

    int city_shift = (int)phase % 19;
    for (int i = 0; i < 13; ++i) {
        int x = 276 + ((i * 23 + city_shift) % 210);
        int h = 18 + (i * 17) % 56;
        int w = 8 + (i * 7) % 12;
        DrawRectangle(x, 214 - h, w, h,
                      color_lerp(theme.terrain, theme.sky_top, .45f));
        DrawRectangle(x + w / 2, 208 - h, 2, 7, theme.accent);
        if (w > 11) DrawRectangle(x + 3, 219 - h, 2, 2, theme.accent2);
    }
    DrawRectangle(0, 216, RALLY_W, 8, color_lerp(theme.terrain, theme.road, .3f));
    DrawRectangle(0, 216, RALLY_W, 2, color_lerp(theme.accent, RAYWHITE, .18f));
}

static void draw_ground(const rally_game_t *g)
{
    rally_theme_t theme = theme_for(g->progress);
    /* Clear the lower half exactly once. Road segments then cover only their
     * own trapezoids; this avoids the old per-segment full-width overdraw. */
    DrawRectangle(0, RALLY_HORIZON, RALLY_W, RALLY_H - RALLY_HORIZON,
                  theme.terrain);
    DrawRectangle(0, RALLY_HORIZON, RALLY_W, 2,
                  color_lerp(theme.accent, theme.sky_bottom, .42f));
}

static void draw_quad(Color fill, rally_point2_t a, rally_point2_t b,
                      rally_point2_t c, rally_point2_t d)
{
    if (!(a.visible || b.visible || c.visible || d.visible)) return;
    DrawTriangle((Vector2){a.x, a.y}, (Vector2){b.x, b.y},
                 (Vector2){c.x, c.y}, fill);
    DrawTriangle((Vector2){b.x, b.y}, (Vector2){d.x, d.y},
                 (Vector2){c.x, c.y}, fill);
}

static void draw_scenery_prop(const rally_game_t *g, float distance,
                              const rally_track_scenery_t *anchor,
                              rally_theme_t theme, MosaicoAtlas art,
                              mosaico_asset_id_t sprite)
{
    rally_point3_t center_world = {anchor->world_x, anchor->world_y,
                                   anchor->world_z};
    rally_point2_t center = project(g, center_world);
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(art, sprite);
    if (frame && center.visible) {
        /* The authored asset becomes a readable 80–140px landmark at the
         * near edge, while the same world anchor shrinks naturally into the
         * horizon.  Clamp after applying the authored scale so randomised
         * scenery never grows into a screen-filling billboard. */
        float size = clampf(2200.0f / (distance + 10.0f) * anchor->scale,
                            9.0f, 140.0f);
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){center.x - size * .5f,
                                  center.y - size * 1.35f,
                                  size, size * 1.35f},
                       (Vector2){0, 0}, 0, WHITE);
        return;
    }
    float side = (float)anchor->side;
    float lateral = anchor->lateral;
    float h = 2.0f * anchor->scale + fmodf(distance * .11f, 1.8f);
    float width = .42f;
    rally_point3_t p;
    track_world(g, distance, lateral - width, 0, &p); rally_point2_t a = project(g, p);
    track_world(g, distance, lateral + width, 0, &p); rally_point2_t b = project(g, p);
    track_world(g, distance, lateral - width, h, &p); rally_point2_t c = project(g, p);
    track_world(g, distance, lateral + width, h, &p); rally_point2_t d = project(g, p);
    Color rock = color_lerp(theme.road, theme.sky_bottom, .24f);
    draw_quad(rock, a, b, c, d);
    track_world(g, distance, lateral - width, h * .72f, &p); a = project(g, p);
    track_world(g, distance, lateral + width, h * .72f, &p); b = project(g, p);
    track_world(g, distance, lateral + width, h * .82f, &p); d = project(g, p);
    track_world(g, distance, lateral - width, h * .82f, &p); c = project(g, p);
    draw_quad(side < 0 ? theme.accent2 : theme.accent, a, b, c, d);
}

static void draw_track_scenery(const rally_game_t *g, MosaicoAtlas art)
{
    rally_theme_t theme = theme_for(g->progress);
    float wrapped = fmodf(g->progress, RALLY_TRACK_LENGTH);
    if (wrapped < 0.0f) wrapped += RALLY_TRACK_LENGTH;
    unsigned current = (unsigned)(wrapped / RALLY_TRACK_SEGMENT_LENGTH);
    const int visible_segments = 9;

    /* The reverse loop is the painter's order: scenery behind the car is
     * placed first, then nearby props naturally cover it.  Anchors come from
     * the gameplay track, not from a camera-relative decoration loop. */
    for (int offset = visible_segments - 1; offset >= 0; --offset) {
        unsigned segment = (current + (unsigned)offset) %
                           RALLY_TRACK_SEGMENT_COUNT;
        for (unsigned slot = 0; slot < RALLY_TRACK_SCENERY_SLOTS; ++slot) {
            rally_track_scenery_t anchor;
            if (!rally_track_scenery(segment, slot, &anchor)) continue;
            float distance = anchor.anchor_progress - wrapped;
            while (distance < RALLY_NEAR_Z) distance += RALLY_TRACK_LENGTH;
            if (distance >= RALLY_FAR_Z) continue;
            mosaico_asset_id_t sprite = (anchor.kind & 1U) ?
                MOSAICO_ASSET_ID_ROADSIDE_ROCK :
                MOSAICO_ASSET_ID_ROADSIDE_BEACON;
            draw_scenery_prop(g, distance, &anchor, theme, art, sprite);
        }
    }
}

static void draw_track(const rally_game_t *g, bool near_only)
{
    /* OutRun-style painter: far-to-near projected strips with a monotonic
     * screen-space clip line. Near strips receive more samples than the far
     * horizon, eliminating the giant first quad and reducing total overdraw. */
    rally_theme_t theme = theme_for(g->progress);
    float clip_y = (float)RALLY_HORIZON;
    for (int i = RALLY_SEGMENTS - 1; i >= 0; --i) {
        float t0 = (float)i / (float)RALLY_SEGMENTS;
        float t1 = (float)(i + 1) / (float)RALLY_SEGMENTS;
        float z0 = RALLY_NEAR_Z + powf(t0, 1.55f) *
                   (RALLY_FAR_Z - RALLY_NEAR_Z);
        float z1 = RALLY_NEAR_Z + powf(t1, 1.55f) *
                   (RALLY_FAR_Z - RALLY_NEAR_Z);
        rally_point3_t p;
        track_world(g, z0 * 1.05f, -5.0f, 0, &p); rally_point2_t l0 = project(g, p);
        track_world(g, z0 * 1.05f, 5.0f, 0, &p); rally_point2_t r0 = project(g, p);
        track_world(g, z1 * 1.05f, -5.0f, 0, &p); rally_point2_t l1 = project(g, p);
        track_world(g, z1 * 1.05f, 5.0f, 0, &p); rally_point2_t r1 = project(g, p);
        if (!(l0.visible || r0.visible || l1.visible || r1.visible)) continue;
        float near_y = (l0.y + r0.y) * .5f;
        float far_y = (l1.y + r1.y) * .5f;
        if (near_y <= clip_y + .35f || near_y <= far_y) continue;
        if (far_y < clip_y) {
            float dy = near_y - far_y;
            float mix = dy > .01f ? (clip_y - far_y) / dy : 0.0f;
            l1.x += (l0.x - l1.x) * mix; r1.x += (r0.x - r1.x) * mix;
            l1.y = r1.y = clip_y;
        }
        track_world(g, z0 * 1.05f, -7.2f, -.04f, &p); rally_point2_t so0 = project(g, p);
        track_world(g, z1 * 1.05f, -7.2f, -.04f, &p); rally_point2_t so1 = project(g, p);
        track_world(g, z0 * 1.05f, 7.2f, -.04f, &p); rally_point2_t eo0 = project(g, p);
        track_world(g, z1 * 1.05f, 7.2f, -.04f, &p); rally_point2_t eo1 = project(g, p);
        if (so1.y < clip_y) so1.y = clip_y;
        if (eo1.y < clip_y) eo1.y = clip_y;
        int seam_phase = (int)(g->progress * .10f);
        Color shoulder = ((i + seam_phase) & 1) ?
                         color_lerp(theme.terrain, theme.accent2, .09f) :
                         color_lerp(theme.terrain, theme.accent, .07f);
        draw_quad(shoulder, so0, l0, so1, l1);
        draw_quad(shoulder, r0, eo0, r1, eo1);
        if (near_only && l0.y < 258.0f && r0.y < 258.0f &&
            l1.y < 258.0f && r1.y < 258.0f) continue;
        /* One continuous asphalt mass reads as a road. Dense alternating
         * strips read as a debug grid on a 480 px display. */
        Color road = (((i + seam_phase) % 7) == 0) ?
                     color_lerp(theme.road, RAYWHITE, .045f) : theme.road;
        draw_quad(road, l0, r0, l1, r1);

        track_world(g, z0 * 1.05f, -5.36f, .04f, &p); rally_point2_t ll0 = project(g, p);
        track_world(g, z0 * 1.05f, -5.08f, .04f, &p); rally_point2_t lr0 = project(g, p);
        track_world(g, z1 * 1.05f, -5.36f, .04f, &p); rally_point2_t ll1 = project(g, p);
        track_world(g, z1 * 1.05f, -5.08f, .04f, &p); rally_point2_t lr1 = project(g, p);
        track_world(g, z0 * 1.05f, 5.08f, .04f, &p); rally_point2_t rl0 = project(g, p);
        track_world(g, z0 * 1.05f, 5.36f, .04f, &p); rally_point2_t rr0 = project(g, p);
        track_world(g, z1 * 1.05f, 5.08f, .04f, &p); rally_point2_t rl1 = project(g, p);
        track_world(g, z1 * 1.05f, 5.36f, .04f, &p); rally_point2_t rr1 = project(g, p);
        draw_quad((i & 1) ? theme.accent : theme.accent2, ll0, lr0, ll1, lr1);
        draw_quad((i & 1) ? theme.accent2 : theme.accent, rl0, rr0, rl1, rr1);

        /* Thin inner curb highlights separate asphalt from terrain even when
         * both surfaces quantize to similar dark RGB565 values. */
        track_world(g, z0 * 1.05f, -4.98f, .045f, &p); rally_point2_t ci0 = project(g, p);
        track_world(g, z1 * 1.05f, -4.98f, .045f, &p); rally_point2_t ci1 = project(g, p);
        DrawLineEx((Vector2){ci0.x, ci0.y}, (Vector2){ci1.x, ci1.y}, 1.5f,
                   color_lerp(theme.accent, RAYWHITE, .38f));
        track_world(g, z0 * 1.05f, 4.98f, .045f, &p); ci0 = project(g, p);
        track_world(g, z1 * 1.05f, 4.98f, .045f, &p); ci1 = project(g, p);
        DrawLineEx((Vector2){ci0.x, ci0.y}, (Vector2){ci1.x, ci1.y}, 1.5f,
                   color_lerp(theme.accent2, RAYWHITE, .38f));

        if ((i & 3) == 0) {
            float lane = 1.38f;
            track_world(g, z0 * 1.05f, -lane, .03f, &p); rally_point2_t a = project(g, p);
            track_world(g, z0 * 1.05f, -lane + .08f, .03f, &p); rally_point2_t b = project(g, p);
            track_world(g, z1 * 1.05f, -lane, .03f, &p); rally_point2_t c = project(g, p);
            track_world(g, z1 * 1.05f, -lane + .08f, .03f, &p); rally_point2_t d = project(g, p);
            draw_quad((Color){111, 220, 255, 205}, a, b, c, d);
            track_world(g, z0 * 1.05f, lane - .08f, .03f, &p); a = project(g, p);
            track_world(g, z0 * 1.05f, lane, .03f, &p); b = project(g, p);
            track_world(g, z1 * 1.05f, lane - .08f, .03f, &p); c = project(g, p);
            track_world(g, z1 * 1.05f, lane, .03f, &p); d = project(g, p);
            draw_quad((Color){255, 108, 209, 205}, a, b, c, d);
        }
        clip_y = near_y;
    }
}

static void draw_billboard(const rally_game_t *g, float distance, float side,
                           Color color, bool gate)
{
    float z = distance;
    if (z < RALLY_NEAR_Z || z > RALLY_FAR_Z) return;
    float w = gate ? 0.20f : .36f;
    float h = gate ? 2.65f : 2.1f;
    rally_point3_t p;
    track_world(g, z, side - w, 0, &p); rally_point2_t a = project(g, p);
    track_world(g, z, side + w, 0, &p); rally_point2_t b = project(g, p);
    track_world(g, z, side - w, h, &p); rally_point2_t c = project(g, p);
    track_world(g, z, side + w, h, &p); rally_point2_t d = project(g, p);
    draw_quad(color_lerp(color, (Color){8, 17, 38, 255}, .42f), a, b, c, d);
    if (!gate) {
        DrawCircle((int)((a.x + b.x) * .5f), (int)(c.y + 2), 2.0f, C_GOLD);
        return;
    }
    /* The opposite post and beam turn every twelfth segment into a visible
     * gate, but remain just three line spans instead of a mesh. */
    float other = -side;
    track_world(g, z, other - w, 0, &p); rally_point2_t oa = project(g, p);
    track_world(g, z, other + w, 0, &p); rally_point2_t ob = project(g, p);
    track_world(g, z, other - w, h, &p); rally_point2_t oc = project(g, p);
    track_world(g, z, other + w, h, &p); rally_point2_t od = project(g, p);
    draw_quad(color_lerp(color, (Color){8, 17, 38, 255}, .42f), oa, ob, oc, od);
    /* A solid header gives the checkpoint architectural weight; one-pixel
     * lines look like editor guides on the device. */
    Color frame = color_lerp(color, (Color){9, 17, 39, 255}, .35f);
    DrawTriangle((Vector2){c.x, c.y}, (Vector2){oc.x, oc.y},
                 (Vector2){d.x, d.y + 9}, frame);
    DrawTriangle((Vector2){d.x, d.y + 9}, (Vector2){oc.x, oc.y},
                 (Vector2){od.x, od.y + 9}, frame);
    DrawLineEx((Vector2){c.x, c.y + 3}, (Vector2){oc.x, oc.y + 3}, 3,
               color_lerp(color, RAYWHITE, .28f));
    DrawLineEx((Vector2){c.x, c.y + 8}, (Vector2){oc.x, oc.y + 8}, 2, color);
}

static void draw_route_props(const rally_game_t *g)
{
    rally_theme_t theme = theme_for(g->progress);
    /* Gates are tied to actual fixed checkpoint anchors, not camera-relative
     * distances. They enter/leave the view naturally as progress advances. */
    for (unsigned checkpoint = 0; checkpoint < RALLY_CHECKPOINT_COUNT - 1U;
         ++checkpoint) {
        float distance = rally_checkpoint_distance(checkpoint) - g->progress;
        while (distance < RALLY_NEAR_Z) distance += RALLY_TRACK_LENGTH;
        if (distance < RALLY_FAR_Z)
            draw_billboard(g, distance,
                           (checkpoint & 1U) ? 5.55f : -5.55f,
                           (checkpoint & 1U) ? theme.accent2 : theme.accent,
                           true);
    }
}

static void draw_opponent_car(const rally_game_t *g, float distance,
                              float lateral, Color color, MosaicoAtlas art,
                              mosaico_asset_id_t sprite)
{
    rally_point3_t world;
    if (!track_world(g, distance, lateral, 0.35f, &world)) return;
    rally_point2_t center = project(g, world);
    if (!center.visible) return;
    /* Rivals are gameplay subjects, not horizon confetti.  Keep even the far
     * silhouettes readable and let a near pass become a dramatic large car. */
    float scale = clampf(285.0f / (distance + 7.0f), 3.2f, 23.0f);
    float x = center.x;
    float y = center.y - scale * .55f;
    DrawEllipse((int)x, (int)(center.y + scale * .2f), scale * 1.55f,
                scale * .31f, (Color){5, 9, 25, 150});
    DrawTriangle((Vector2){x - scale * 1.4f, y + scale * .55f},
                 (Vector2){x + scale * 1.4f, y + scale * .55f},
                 (Vector2){x + scale * .8f, y - scale * .20f}, color);
    DrawTriangle((Vector2){x - scale * 1.4f, y + scale * .55f},
                 (Vector2){x + scale * .8f, y - scale * .20f},
                 (Vector2){x - scale * .75f, y - scale * .27f},
                 color_lerp(color, RAYWHITE, .22f));
    DrawRectangle((int)(x - scale * 1.1f), (int)(y + scale * .38f),
                  (int)(scale * .42f), (int)(scale * .16f), C_GOLD);
    DrawRectangle((int)(x + scale * .68f), (int)(y + scale * .38f),
                  (int)(scale * .42f), (int)(scale * .16f), C_GOLD);
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(art, sprite);
    if (frame) {
        float size = scale * 6.1f;
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){x - size * .5f, y - size * .48f,
                                   size, size * .90f},
                       (Vector2){0, 0}, 0, WHITE);
    }
}

static bool draw_opponents(const rally_game_t *g, MosaicoAtlas art)
{
    static const Color colors[3] = {
        {113, 126, 255, 255}, {255, 84, 154, 255}, {87, 240, 190, 255}
    };
    static const mosaico_asset_id_t sprites[3] = {
        MOSAICO_ASSET_ID_CRAFT_RIVAL_CYAN, MOSAICO_ASSET_ID_CRAFT_RIVAL_PINK,
        MOSAICO_ASSET_ID_CRAFT_RIVAL_GOLD
    };
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        const rally_opponent_t *opponent = &g->opponents[i];
        if (!opponent->active) continue;
        float distance = opponent->progress - g->progress;
        if (distance < -RALLY_TRACK_LENGTH * .5f) distance += RALLY_TRACK_LENGTH;
        if (distance > RALLY_TRACK_LENGTH * .5f) distance -= RALLY_TRACK_LENGTH;
        if (distance > RALLY_NEAR_Z && distance < RALLY_FAR_Z)
            draw_opponent_car(g, distance, opponent->lateral, colors[i], art,
                              sprites[i]);
    }
    return (g->event_flags & RALLY_EVENT_NEAR_MISS) != 0;
}

static void draw_speed_lines(const rally_game_t *g, rally_theme_t theme)
{
    if (g->speed < 11.0f) return;
    int count = g->nitro_active ? 12 : 7;
    int travel = (int)(g->tick * (g->nitro_active ? 9U : 5U));
    for (int i = 0; i < count; ++i) {
        int side = (i & 1) ? 1 : -1;
        int x = side > 0 ? 408 + (i * 17) % 58 : 14 - (i * 19) % 48;
        int y = 270 + ((i * 43 + travel) % 166);
        int length = 7 + (int)(g->speed * .35f) + (i & 3);
        DrawLine(x, y, x + side * length, y + length / 2,
                 (i & 1) ? theme.accent : theme.accent2);
    }
}

static void draw_drift_sparks(const rally_game_t *g, rally_theme_t theme)
{
    if (!g->drifting) return;
    float x = 240.0f + g->lateral * 10.0f;
    float y = 425.0f - g->height * 7.0f;
    for (int i = 0; i < 6; ++i) {
        int side = (i & 1) ? 1 : -1;
        int sx = (int)x + side * (31 + (i / 2) * 7);
        int sy = (int)y + 3 + (i % 3) * 5;
        int length = 5 + (int)(g->drift_meter * .08f) + (i & 1);
        DrawLine(sx, sy, sx + side * length, sy + 3 + (i & 1),
                 (i & 2) ? theme.accent2 : C_GOLD);
    }
}

static void draw_vehicle(const rally_game_t *g, MosaicoAtlas art)
{
    float bob = sinf((float)g->tick * .24f) * .035f;
    float x = 240.0f + g->lateral * 10.0f;
    float y = 397.0f - g->height * 7.0f + bob * 20.0f;
    Color glow = g->nitro_active ? C_GOLD : C_CYAN;
    /* Wide converging exhaust strokes give the large foreground craft a
     * sense of thrust without an alpha-heavy particle system. */
    int trail = g->nitro_active ? 66 : (int)clampf(g->speed * 1.15f, 16, 42);
    Color trail_color = g->nitro_active ? C_GOLD : C_CYAN;
    DrawLineEx((Vector2){x - 49, y + 42},
               (Vector2){x - 72, y + 42 + trail}, 7, (Color){18, 89, 119, 170});
    DrawLineEx((Vector2){x + 49, y + 42},
               (Vector2){x + 72, y + 42 + trail}, 7, (Color){18, 89, 119, 170});
    DrawLineEx((Vector2){x - 49, y + 42},
               (Vector2){x - 65, y + 36 + trail}, 3, trail_color);
    DrawLineEx((Vector2){x + 49, y + 42},
               (Vector2){x + 65, y + 36 + trail}, 3,
               g->nitro_active ? C_PINK : C_MINT);
    DrawEllipse((int)x, (int)(y + 43), 66.0f + (g->nitro_active ? 11.0f : 0.0f),
                12.0f, (Color){14, 91, 126, 150});
    DrawTriangle((Vector2){x - 66, y + 42}, (Vector2){x + 66, y + 42},
                 (Vector2){x + 42, y + 9}, (Color){16, 63, 93, 255});
    DrawTriangle((Vector2){x - 66, y + 42}, (Vector2){x + 42, y + 9},
                 (Vector2){x - 35, y + 5}, (Color){25, 107, 141, 255});
    DrawTriangle((Vector2){x - 38, y + 7}, (Vector2){x + 38, y + 7},
                 (Vector2){x + 17, y - 19}, (Color){59, 184, 207, 255});
    DrawTriangle((Vector2){x - 38, y + 7}, (Vector2){x + 17, y - 19},
                 (Vector2){x - 19, y - 22}, (Color){102, 235, 242, 255});
    DrawTriangle((Vector2){x - 19, y - 22}, (Vector2){x + 17, y - 19},
                 (Vector2){x + 5, y - 5}, (Color){8, 26, 59, 255});
    DrawLine((int)x - 66, (int)y + 42, (int)x - 32, (int)y + 20, C_MINT);
    DrawLine((int)x + 66, (int)y + 42, (int)x + 32, (int)y + 20, C_PINK);
    DrawCircleLines((int)x - 49, (int)y + 44, 8.0f, glow);
    DrawCircleLines((int)x + 49, (int)y + 44, 8.0f, glow);
    DrawRectangle((int)x - 61, (int)y + 36, 25, 6, glow);
    DrawRectangle((int)x + 36, (int)y + 36, 25, 6, glow);
    if (g->nitro_active) {
        DrawTriangle((Vector2){x - 29, y + 35}, (Vector2){x - 6, y + 35},
                     (Vector2){x - 16, y + 65}, C_GOLD);
        DrawTriangle((Vector2){x + 6, y + 35}, (Vector2){x + 29, y + 35},
                     (Vector2){x + 16, y + 65}, C_PINK);
    }
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(
        art, MOSAICO_ASSET_ID_CRAFT_PLAYER);
    if (frame)
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){x - 120.0f, y - 75.0f, 240.0f, 180.0f},
                       (Vector2){0, 0}, 0, WHITE);
}

static void draw_hud(const rally_game_t *g)
{
    rally_theme_t theme = theme_for(g->progress);
    Color glass = {3, 8, 22, 218};
    Color muted = {135, 174, 201, 255};
    int rank = g->position ? g->position : RALLY_OPPONENT_COUNT + 1;
    float elapsed = (float)g->tick / (float)RALLY_TICK_HZ;
    DrawRectangle(0, 0, RALLY_W, 68, glass);
    DrawRectangle(0, 66, RALLY_W, 2,
                  color_lerp(theme.accent, theme.accent2, .50f));

    DrawText("POSITION", 17, 10, 9, muted);
    DrawText(TextFormat("%d", rank), 16, 25, 30,
             rank == 1 ? C_MINT : (rank == 2 ? C_GOLD : C_PINK));
    DrawText(TextFormat("/%d", RALLY_OPPONENT_COUNT + 1), 43, 40, 11, muted);

    int speed = (int)clampf(g->speed * 25.0f, 0, 999);
    DrawText(TextFormat("%03d", speed), 190, 12, 30, RAYWHITE);
    DrawText("KM/H", 269, 38, 9, C_GOLD);

    DrawText(TextFormat("LAP %u/%u", (unsigned)(g->laps_completed + 1U),
                        (unsigned)RALLY_TARGET_LAPS), 365, 11, 10, RAYWHITE);
    DrawText(TextFormat("%02d:%04.1f", (int)(elapsed / 60.0f),
                        fmodf(elapsed, 60.0f)), 365, 29, 10, muted);
    DrawText(TextFormat("CP %u/%u", (unsigned)g->next_checkpoint,
                        (unsigned)(RALLY_CHECKPOINT_COUNT - 1U)),
             365, 47, 9, theme.accent);

    DrawText("N2O", 311, 438, 9, muted);
    DrawRectangle(338, 439, 126, 8, (Color){13, 25, 45, 230});
    DrawRectangle(338, 439,
                  (int)(126 * clampf(g->nitro / RALLY_NITRO_MAX, 0, 1)), 8,
                  g->nitro_active ? C_GOLD : C_MINT);
    DrawText(TextFormat("HULL %03u", (unsigned)g->integrity), 16, 439, 9,
             g->integrity < 35 ? C_PINK : C_MINT);
    if (g->combo > 0)
        DrawText(TextFormat("DRIFT  X%u", (unsigned)(g->combo + 1U)), 187, 438, 9,
                 g->drifting ? C_GOLD : theme.accent2);
}

static void draw_start_banner(const rally_game_t *g)
{
    if (g->phase == RALLY_PHASE_FINISHED || g->tick >= 120U) return;
    const char *label;
    Color color;
    if (g->tick < 30U) { label = "3"; color = C_PINK; }
    else if (g->tick < 60U) { label = "2"; color = C_GOLD; }
    else if (g->tick < 90U) { label = "1"; color = C_CYAN; }
    else { label = "GO"; color = C_MINT; }
    DrawCircle(240, 147, 42, (Color){3, 8, 24, 226});
    DrawCircleLines(240, 147, 42, color_lerp(color, RAYWHITE, .18f));
    DrawCircleLines(240, 147, 36, color);
    DrawText(label, label[1] ? 213 : 229, 122, label[1] ? 40 : 52, color);
    DrawRectangle(159, 194, 162, 18, (Color){4, 9, 29, 224});
    DrawText("TAP TOP  <  SELECT  >", 176, 199, 9,
             (Color){183, 220, 233, 255});
}

static void draw_race_alert(const rally_game_t *g, bool near_miss,
                            rally_theme_t theme)
{
    if (near_miss) {
        DrawRectangle(157, 92, 166, 19, (Color){24, 13, 47, 225});
        DrawRectangle(157, 92, 3, 19, theme.accent2);
        DrawRectangle(320, 92, 3, 19, theme.accent2);
        DrawText("NEAR MISS // +DRIFT", 174, 97, 10, theme.accent2);
    } else if (g->event_flags & RALLY_EVENT_OFFTRACK) {
        DrawText("OFF TRACK", 199, 96, 12, C_PINK);
    } else if (g->drifting && g->drift_meter > 65.0f) {
        DrawText("SLIPSTREAM", 194, 96, 12, C_GOLD);
    }
}

int rally_view_render(const rally_game_t *state, MosaicoAtlas rally_art,
                      MosaicoAtlas track_background)
{
    rally_game_t idle = {0};
    const rally_game_t *g = state ? state : &idle;
    BeginDrawing();
    const MosaicoSpriteFrame *backdrop = MosaicoAtlasGetFrame(
        track_background, MOSAICO_ASSET_ID_TRACK_CANYON);
    if (backdrop) {
        DrawTexturePro(track_background.texture, backdrop->source,
                       (Rectangle){0, 0, RALLY_W, RALLY_H},
                       (Vector2){0, 0}, 0, WHITE);
    } else {
        draw_sky(g);
        draw_ground(g);
        draw_track(g, false);
        draw_track_scenery(g, rally_art);
        draw_route_props(g);
    }
    bool near_miss = draw_opponents(g, rally_art);
    draw_speed_lines(g, theme_for(g->progress));
    draw_drift_sparks(g, theme_for(g->progress));
    draw_vehicle(g, rally_art);
    draw_hud(g);
    draw_race_alert(g, near_miss, theme_for(g->progress));
    draw_start_banner(g);
    if (g->phase == RALLY_PHASE_FINISHED) {
        Color accent = C_GOLD;
        DrawRectangle(72, 177, 336, 102, (Color){4, 10, 31, 236});
        DrawRectangle(72, 177, 336, 3, accent);
        DrawRectangle(72, 276, 336, 3, accent);
        DrawText("RIFT CLEARED",
                 147, 199, 24, RAYWHITE);
        DrawText(TextFormat("LAP %u  //  TIME %02d:%04.1f",
                            (unsigned)g->laps_completed,
                            (int)(((float)g->tick / RALLY_TICK_HZ) / 60.0f),
                            fmodf((float)g->tick / RALLY_TICK_HZ, 60.0f)),
                 143, 240, 12, accent);
        DrawText("TAP TO RACE AGAIN", 170, 260, 10, C_MINT);
    } else if (g->phase == RALLY_PHASE_FAILED) {
        DrawRectangle(126, 84, 228, 54, (Color){4, 10, 31, 226});
        DrawRectangle(126, 84, 228, 3, C_PINK);
        DrawText("CRAFT DISABLED", 161, 96, 16, RAYWHITE);
        DrawText(TextFormat("SCORE %06lu  //  POS %u/%u",
                            (unsigned long)g->score, (unsigned)g->position,
                            (unsigned)(RALLY_OPPONENT_COUNT + 1U)),
                 157, 119, 9, C_PINK);
    }
    EndDrawing();
    return 0;
}
