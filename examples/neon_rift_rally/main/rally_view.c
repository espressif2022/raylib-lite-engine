// SPDX-License-Identifier: Apache-2.0
#include "rally_view.h"

#include <math.h>

#include "assets_ids.h"
#include "raylib_lite_raylib.h"
#if defined(RAYLIB_LITE_HOST_SIMULATION)
#include "raylib_lite_clock.h"
static uint32_t s_host_phase_us[5];

/* Host profiling only: sky, road, scenery, actors/effects, HUD. */
void rally_view_get_host_profile(uint32_t out[5])
{
    for (unsigned i = 0; i < 5; ++i) out[i] = s_host_phase_us[i];
}
#define PROFILE_BEGIN() uint64_t profile_started = raylib_lite_time_us()
#define PROFILE_PHASE(index) do { \
    uint64_t now = raylib_lite_time_us(); \
    s_host_phase_us[index] = (uint32_t)(now - profile_started); \
    profile_started = now; \
} while (0)
#else
#define PROFILE_BEGIN() ((void)0)
#define PROFILE_PHASE(index) ((void)0)
#endif

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

typedef struct {
    float x, y, z;
    float right_x, right_z;
    float forward_x, forward_z;
    float roll_cos, roll_sin;
    float shake_x, shake_y;
} rally_camera_t;

/* The renderer already owns a single active framebuffer, so its camera can
 * be prepared once per frame instead of sampling the track and computing
 * three trigonometric pairs for every projected road vertex. */
static rally_camera_t s_camera;
static float s_segment_z[RALLY_SEGMENTS + 1];
static rally_track_pose_t s_segment_pose[RALLY_SEGMENTS + 1];
static bool s_segment_z_ready;
static unsigned s_course_id;

static float clampf(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static rally_theme_t theme_for(float progress)
{
    (void)progress;
    if (s_course_id == 0U) {
        return (rally_theme_t){{3, 8, 31, 255}, {92, 35, 106, 255},
                               {18, 27, 47, 255}, {31, 22, 62, 255},
                               C_CYAN, C_PINK};
    }
    if (s_course_id == 1U) {
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

static void track_offset(const rally_track_pose_t *pose, float lateral,
                         float height, rally_point3_t *out)
{
    out->x = pose->x + pose->right_x * lateral;
    out->y = pose->y + height;
    out->z = pose->z + pose->right_z * lateral;
}

static bool track_world(const rally_game_t *g, float distance, float lateral,
                        float height, rally_point3_t *out)
{
    rally_track_pose_t pose;
    if (!rally_track_sample_course(g->course_id, g->progress + distance, 0.0f, &pose)) return false;
    track_offset(&pose, lateral, height, out);
    return true;
}

static void prepare_camera(const rally_game_t *g)
{
    rally_track_pose_t cam;
    if (!rally_track_sample_course(g->course_id, g->progress, g->lateral, &cam)) return;
    float yaw = g->heading_error;
    float cy = cosf(yaw), sy = sinf(yaw);
    s_camera.right_x = cam.right_x * cy + cam.tangent_x * sy;
    s_camera.right_z = cam.right_z * cy + cam.tangent_z * sy;
    s_camera.forward_x = cam.tangent_x * cy - cam.right_x * sy;
    s_camera.forward_z = cam.tangent_z * cy - cam.right_z * sy;
    /* A short projection-space chase offset allows the nearest ribbon slice
     * to grow all the way below the cockpit instead of ending in a static
     * lower-screen road fill. */
    s_camera.x = cam.x - cam.tangent_x * 1.40f;
    /* Low chase camera: the car fills the lower frame while the road still
     * exposes enough horizon to read the next bend. */
    s_camera.y = cam.y + 1.55f + g->height * 0.16f;
    s_camera.z = cam.z - cam.tangent_z * 1.40f;
    float roll = -cam.bank * 0.42f;
    s_camera.roll_cos = cosf(roll);
    s_camera.roll_sin = sinf(roll);
    float impact = g->collision_ticks ?
        clampf(g->impact_speed / RALLY_MAX_SPEED, .3f, 1.0f) : 0.0f;
    s_camera.shake_x = impact * ((g->collision_ticks & 1U) ? 5.0f : -5.0f);
    s_camera.shake_y = impact * ((g->collision_ticks & 2U) ? 3.0f : -3.0f);
}

static rally_point2_t project(const rally_game_t *g, rally_point3_t p)
{
    (void)g;
    float dx = p.x - s_camera.x;
    float dz = p.z - s_camera.z;
    float vx = dx * s_camera.right_x + dz * s_camera.right_z;
    float view_z = dx * s_camera.forward_x + dz * s_camera.forward_z;
    float vy = p.y - s_camera.y;
    if (view_z < RALLY_NEAR_Z) return (rally_point2_t){0, 0, false};
    float sx = 240.0f + RALLY_FOCAL * vx / view_z;
    float sy_screen = (float)RALLY_HORIZON - RALLY_FOCAL * vy / view_z;
    float ox = sx - 240.0f, oy = sy_screen - (float)RALLY_HORIZON;
    sx = 240.0f + ox * s_camera.roll_cos - oy * s_camera.roll_sin;
    sy_screen = (float)RALLY_HORIZON + ox * s_camera.roll_sin + oy * s_camera.roll_cos;
    sx += s_camera.shake_x;
    sy_screen += s_camera.shake_y;
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
                              rally_theme_t theme, raylib_lite_atlas_t art,
                              raylib_lite_asset_id_t sprite)
{
    rally_point3_t center_world = {anchor->world_x, anchor->world_y,
                                   anchor->world_z};
    rally_point2_t center = project(g, center_world);
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(art, sprite);
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

static void draw_track_scenery(const rally_game_t *g, raylib_lite_atlas_t art)
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
            if (!rally_track_scenery_course(g->course_id, segment, slot, &anchor)) continue;
            float distance = anchor.anchor_progress - wrapped;
            while (distance < RALLY_NEAR_Z) distance += RALLY_TRACK_LENGTH;
            if (distance >= RALLY_FAR_Z) continue;
            raylib_lite_asset_id_t sprite = (anchor.kind & 1U) ?
                RAYLIB_LITE_ASSET_ID_ROADSIDE_ROCK :
                RAYLIB_LITE_ASSET_ID_ROADSIDE_BEACON;
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
    if (!s_segment_z_ready) {
        for (int index = 0; index <= RALLY_SEGMENTS; ++index) {
            float t = (float)index / (float)RALLY_SEGMENTS;
            s_segment_z[index] = RALLY_NEAR_Z + powf(t, 1.55f) *
                                 (RALLY_FAR_Z - RALLY_NEAR_Z);
        }
        s_segment_z_ready = true;
    }
    /* Each boundary's center and tangent are shared by asphalt, shoulders,
     * edge stripes and curb highlights. Sample once per frame and boundary. */
    for(int i=0;i<=RALLY_SEGMENTS;++i)
        rally_track_sample_course(g->course_id, g->progress+s_segment_z[i]*1.05f,
                                  0.0f, &s_segment_pose[i]);
    float clip_y = (float)RALLY_HORIZON;
    for (int i = RALLY_SEGMENTS - 1; i >= 0; --i) {
        const rally_track_pose_t *p0=&s_segment_pose[i],*p1=&s_segment_pose[i+1];
        rally_point3_t p;
        track_offset(p0, -5.0f, 0, &p); rally_point2_t l0 = project(g, p);
        track_offset(p0, 5.0f, 0, &p); rally_point2_t r0 = project(g, p);
        track_offset(p1, -5.0f, 0, &p); rally_point2_t l1 = project(g, p);
        track_offset(p1, 5.0f, 0, &p); rally_point2_t r1 = project(g, p);
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
        track_offset(p0, -7.2f, -.04f, &p); rally_point2_t so0 = project(g, p);
        track_offset(p1, -7.2f, -.04f, &p); rally_point2_t so1 = project(g, p);
        track_offset(p0, 7.2f, -.04f, &p); rally_point2_t eo0 = project(g, p);
        track_offset(p1, 7.2f, -.04f, &p); rally_point2_t eo1 = project(g, p);
        if (so1.y < clip_y) so1.y = clip_y;
        if (eo1.y < clip_y) eo1.y = clip_y;
        Color shoulder = color_lerp(theme.terrain, theme.road, .18f);
        draw_quad(shoulder, so0, l0, so1, l1);
        draw_quad(shoulder, r0, eo0, r1, eo1);
        if (near_only && l0.y < 258.0f && r0.y < 258.0f &&
            l1.y < 258.0f && r1.y < 258.0f) continue;
        /* Keep the asphalt continuous; roadside and lane markings carry the
         * speed cue and advance in world distance rather than segment index. */
        draw_quad(theme.road, l0, r0, l1, r1);

        track_offset(p0, -5.36f, .04f, &p); rally_point2_t ll0 = project(g, p);
        track_offset(p0, -5.08f, .04f, &p); rally_point2_t lr0 = project(g, p);
        track_offset(p1, -5.36f, .04f, &p); rally_point2_t ll1 = project(g, p);
        track_offset(p1, -5.08f, .04f, &p); rally_point2_t lr1 = project(g, p);
        track_offset(p0, 5.08f, .04f, &p); rally_point2_t rl0 = project(g, p);
        track_offset(p0, 5.36f, .04f, &p); rally_point2_t rr0 = project(g, p);
        track_offset(p1, 5.08f, .04f, &p); rally_point2_t rl1 = project(g, p);
        track_offset(p1, 5.36f, .04f, &p); rally_point2_t rr1 = project(g, p);
        draw_quad(theme.accent, ll0, lr0, ll1, lr1);
        draw_quad(theme.accent2, rl0, rr0, rl1, rr1);

        /* Thin inner curb highlights separate asphalt from terrain even when
         * both surfaces quantize to similar dark RGB565 values. */
        track_offset(p0, -4.98f, .045f, &p); rally_point2_t ci0 = project(g, p);
        track_offset(p1, -4.98f, .045f, &p); rally_point2_t ci1 = project(g, p);
        DrawLineEx((Vector2){ci0.x, ci0.y}, (Vector2){ci1.x, ci1.y}, 1.5f,
                   color_lerp(theme.accent, RAYWHITE, .38f));
        track_offset(p0, 4.98f, .045f, &p); ci0 = project(g, p);
        track_offset(p1, 4.98f, .045f, &p); ci1 = project(g, p);
        DrawLineEx((Vector2){ci0.x, ci0.y}, (Vector2){ci1.x, ci1.y}, 1.5f,
                   color_lerp(theme.accent2, RAYWHITE, .38f));

        clip_y = near_y;
    }
}

static void draw_lane_markings(const rally_game_t *g)
{
    /* A dash occupies a fixed world-space interval, so its length and gaps
     * remain coherent as the camera moves between ribbon segments. */
    const float period = 8.0f;
    const float length = 3.2f;
    float first = floorf(g->progress / period) * period;
    for (float world = first + 9.0f * period; world >= first;
         world -= period) {
        float near_distance = world - g->progress;
        float far_distance = near_distance + length;
        if (near_distance < 1.0f || far_distance > RALLY_FAR_Z) continue;
        for (int side = -1; side <= 1; side += 2) {
            float lane = (float)side * 1.38f;
            rally_point3_t p;
            track_world(g, near_distance, lane - .07f, .035f, &p);
            rally_point2_t a = project(g, p);
            track_world(g, near_distance, lane + .07f, .035f, &p);
            rally_point2_t b = project(g, p);
            track_world(g, far_distance, lane - .07f, .035f, &p);
            rally_point2_t c = project(g, p);
            track_world(g, far_distance, lane + .07f, .035f, &p);
            rally_point2_t d = project(g, p);
            draw_quad(side < 0 ? C_CYAN : C_PINK, a, b, c, d);
        }
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
                              float lateral, Color color, raylib_lite_atlas_t art)
{
    rally_point3_t world;
    if (!track_world(g, distance, lateral, 0.35f, &world)) return;
    rally_point2_t center = project(g, world);
    if (!center.visible) return;
    float size = clampf(1280.0f / (distance + 8.0f), 18.0f, 125.0f);
    DrawEllipse((int)center.x, (int)center.y, size * .30f,
                size * .06f, (Color){5, 9, 25, 150});
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(
        art, RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT);
    if (frame) {
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){center.x - size * .5f, center.y - size,
                                   size, size},
                       (Vector2){0, 0}, 0, color);
    }
}

static bool draw_opponents(const rally_game_t *g, raylib_lite_atlas_t art)
{
    static const Color colors[RALLY_OPPONENT_COUNT] = {
        {113, 126, 255, 255}, {255, 84, 154, 255},
        {87, 240, 190, 255}, {255, 188, 80, 255},
        {186, 126, 255, 255}, {124, 230, 244, 255}
    };
    unsigned order[RALLY_OPPONENT_COUNT];
    float distances[RALLY_OPPONENT_COUNT];
    unsigned visible = 0;
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        const rally_opponent_t *opponent = &g->opponents[i];
        if (!opponent->active) continue;
        float distance = opponent->progress - g->progress;
        if (distance < -RALLY_TRACK_LENGTH * .5f) distance += RALLY_TRACK_LENGTH;
        if (distance > RALLY_TRACK_LENGTH * .5f) distance -= RALLY_TRACK_LENGTH;
        if (distance <= RALLY_NEAR_Z || distance >= RALLY_FAR_Z) continue;
        unsigned slot = visible++;
        while (slot && distances[slot - 1] < distance) {
            distances[slot] = distances[slot - 1];
            order[slot] = order[slot - 1];
            --slot;
        }
        distances[slot] = distance;
        order[slot] = i;
    }
    for (unsigned slot = 0; slot < visible; ++slot) {
        unsigned i = order[slot];
        draw_opponent_car(g, distances[slot], g->opponents[i].lateral,
                          colors[i], art);
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

static void draw_collision_feedback(const rally_game_t *g)
{
    if (!g->collision_ticks) return;
    unsigned strength = (unsigned)g->collision_ticks;
    float impact = clampf(g->impact_speed / RALLY_MAX_SPEED, .3f, 1.0f);
    Color flash = {255, 112, 125,
                   (unsigned char)(strength * (5.0f + 5.0f * impact))};
    DrawRectangle(0, 68, RALLY_W, 5, flash);
    DrawRectangle(0, RALLY_H - 11, RALLY_W, 11, flash);
    DrawRectangle(0, 68, 7, RALLY_H - 79, flash);
    DrawRectangle(RALLY_W - 7, 68, 7, RALLY_H - 79, flash);
    int kick = (strength & 1U) ? 9 : -9;
    DrawLine(240 + kick, 335, 201 + kick, 314, C_PINK);
    DrawLine(240 + kick, 335, 279 + kick, 314, C_GOLD);
    if (strength > 4U) {
        DrawRectangle(172, 104, 136, 30, (Color){30, 5, 20, 220});
        DrawRectangle(172, 104, 4, 30, C_PINK);
        DrawText("CRASH  -12", 191, 112, 16, RAYWHITE);
    }
}

static void draw_vehicle(const rally_game_t *g, raylib_lite_atlas_t art)
{
    float steer = g->heading_error * 32.0f +
                  g->lateral_velocity * (g->drifting ? 1.4f : .7f);
    float x = 240.0f + g->lateral * 7.0f;
    float bob = g->grounded ? sinf((float)g->tick * .30f) *
                clampf(g->speed / RALLY_MAX_SPEED, 0.0f, 1.0f) * 1.2f : 0.0f;
    float ground_y = 451.0f - g->height * 8.0f + bob;
    if (g->collision_ticks) x += (g->collision_ticks & 1U) ? 4.0f : -4.0f;
    DrawEllipse((int)x, 451, 48.0f, 8.0f, (Color){4, 9, 25, 180});
    if (g->nitro_active) {
        int trail = 29 + (int)(g->speed * .5f);
        DrawLineEx((Vector2){x - 17, ground_y - 7},
                   (Vector2){x - 23, ground_y + trail}, 5, C_CYAN);
        DrawLineEx((Vector2){x + 17, ground_y - 7},
                   (Vector2){x + 23, ground_y + trail}, 5, C_PINK);
    }
    raylib_lite_asset_id_t pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT;
    if (steer < -2.5f) pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LEFT;
    if (steer > 2.5f) pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_RIGHT;
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(art, pose);
    if (frame)
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){x - 88.0f, ground_y - 177.0f, 176.0f, 176.0f},
                       (Vector2){0, 0}, 0,
                       g->collision_ticks ? (Color){255, 166, 177, 255} : WHITE);
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

    /* Track coordinates are metres and simulation speed is metres/second. */
    int speed = (int)clampf(g->speed * 3.6f, 0, 999);
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
    DrawText(TextFormat("BIKE %03u", (unsigned)g->integrity), 16, 439, 9,
             g->integrity < 35 ? C_PINK : C_MINT);
    if (g->combo > 0)
        DrawText(TextFormat("DRIFT  X%u", (unsigned)(g->combo + 1U)), 187, 438, 9,
                 g->drifting ? C_GOLD : theme.accent2);
}

static void draw_start_banner(const rally_game_t *g, unsigned course_id)
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
    static const char *names[3] = {
        "NEON LOOP", "SUNSET SPRINT", "POLAR RIFT"
    };
    DrawRectangle(77, 187, 326, 32, (Color){4, 9, 29, 224});
    DrawText("<", 101, 191, 22, C_CYAN);
    DrawText(names[course_id % 3U], 173, 196, 12, RAYWHITE);
    DrawText(">", 369, 191, 22, C_PINK);
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

int rally_view_render(const rally_game_t *state, raylib_lite_atlas_t rally_art,
                      raylib_lite_atlas_t track_background,
                      raylib_lite_atlas_t motorcycle_art, unsigned course_id)
{
    rally_game_t idle = {0};
    const rally_game_t *g = state ? state : &idle;
    s_course_id = course_id % 3U;
    prepare_camera(g);
    BeginDrawing();
    PROFILE_BEGIN();
    const raylib_lite_sprite_frame_t *backdrop = raylib_lite_atlas_get_frame(
        track_background, RAYLIB_LITE_ASSET_ID_TRACK_CANYON);
    if (backdrop) {
        /* Sample only the sky.  The source also contains a fixed gate and
         * road, neither of which can follow the simulated track. */
        Rectangle sky = backdrop->source;
        float sky_width = sky.width * .92f;
        float pan = clampf(g->heading_error * 2.0f +
                           sinf(g->progress * .012f) * .5f, -1.0f, 1.0f);
        sky.x += (sky.width - sky_width) * (.5f + .5f * pan);
        sky.width = sky_width;
        sky.height *= 0.19f;
        Color sky_tint = s_course_id == 0U ? (Color){137, 173, 255, 255} :
                         s_course_id == 2U ? (Color){136, 229, 238, 255} : WHITE;
        DrawTexturePro(track_background.texture, sky,
                       (Rectangle){0, 0, RALLY_W, RALLY_HORIZON},
                       (Vector2){0, 0}, 0, sky_tint);
    } else {
        draw_sky(g);
    }
    PROFILE_PHASE(0);
    draw_ground(g);
    draw_track(g, false);
    draw_lane_markings(g);
    PROFILE_PHASE(1);
    draw_track_scenery(g, rally_art);
    draw_route_props(g);
    PROFILE_PHASE(2);
    bool near_miss = draw_opponents(g, motorcycle_art);
    draw_speed_lines(g, theme_for(g->progress));
    draw_drift_sparks(g, theme_for(g->progress));
    draw_vehicle(g, motorcycle_art);
    draw_collision_feedback(g);
    PROFILE_PHASE(3);
    draw_hud(g);
    draw_race_alert(g, near_miss, theme_for(g->progress));
    draw_start_banner(g, s_course_id);
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
    PROFILE_PHASE(4);
    EndDrawing();
    return 0;
}
