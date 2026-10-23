// SPDX-License-Identifier: Apache-2.0
#include "rally_view.h"

#include <math.h>

#include "assets_ids.h"
#include "raylib_lite_design_view.h"
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

/* The scene keeps a 480x480 field centred in the design space. Longer panels
 * widen it by RALLY_MARGIN per side: backdrops span the full width, the road
 * spills into the margins and HUD groups are pinned to the panel edges. */
static raylib_lite_design_view_t s_view = {1.0f, 480.0f, 480.0f};
static float s_margin;
#define RALLY_LEFT ((int)-s_margin)
#define RALLY_RIGHT (RALLY_W + (int)s_margin)
#define RALLY_SPAN (RALLY_RIGHT - RALLY_LEFT)

static int font(int size) { return raylib_lite_design_font(s_view, size); }
static void draw_text(const char *text, int x, int y, int size, Color color)
{
    DrawText(text, x, y, font(size), color);
}
static int measure_text(const char *text, int size) { return MeasureText(text, font(size)); }

/* Shifts the field camera by `dx` design units; 0 restores it. */
static void pin_hud(float dx)
{
    if (raylib_lite_design_field_identity(s_view, RALLY_W, RALLY_H)) return;
    Vector2 origin = raylib_lite_design_field_origin(s_view, RALLY_W, RALLY_H);
    EndMode2D();
    BeginMode2D((Camera2D){.offset = {(origin.x + dx) * s_view.scale, origin.y * s_view.scale},
                           .zoom = s_view.scale});
}
#define RALLY_NEAR_Z 0.15f
#define RALLY_SEGMENTS 22
#define RALLY_FAR_Z 70.0f
#define RALLY_FOCAL 252.0f

static const Color C_CYAN = {44, 235, 255, 255};
static const Color C_MINT = {72, 255, 190, 255};
static const Color C_PINK = {255, 62, 181, 255};
static const Color C_GOLD = {255, 200, 70, 255};
static const Color C_RIDER[RALLY_OPPONENT_COUNT] = {
    {113, 126, 255, 255}, {255, 84, 154, 255}, {87, 240, 190, 255},
    {255, 188, 80, 255},  {186, 126, 255, 255}, {124, 230, 244, 255},
};

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
    rally_point3_t world;
} rally_point2_t;

typedef struct {
    float x, y, z;
    float right_x, right_z;
    float forward_x, forward_z;
    float roll_cos, roll_sin;
    float shake_x, shake_y;
    float focal;
} rally_camera_t;

/* The renderer already owns a single active framebuffer, so its camera can
 * be prepared once per frame instead of sampling the track and computing
 * three trigonometric pairs for every projected road vertex. */
static rally_camera_t s_camera;
static float s_segment_z[RALLY_SEGMENTS + 1];
static rally_track_pose_t s_segment_pose[RALLY_SEGMENTS + 1];
static bool s_segment_z_ready;
static unsigned s_course_id;
static int s_horizon = RALLY_HORIZON;

static float clampf(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static rally_theme_t theme_for(float progress)
{
    (void)progress;
    /* 60-30-10 palette:
     * - 60% deep charcoal-indigo tarmac & calm midnight verge
     * - 30% symmetrical horizon-matched curbs
     * - 10% warm amber / mint reflectors and focal chibi rider */
    if (s_course_id == 0U) {
        return (rally_theme_t){
            {3, 8, 31, 255}, {38, 52, 82, 255}, {21, 28, 43, 255}, {9, 14, 26, 255},
            (Color){110, 218, 242, 255}, C_GOLD};
    }
    if (s_course_id == 1U) {
        return (rally_theme_t){
            {22, 11, 35, 255}, {168, 86, 82, 255}, {38, 30, 40, 255}, {34, 18, 28, 255},
            C_GOLD, (Color){255, 118, 92, 255}};
    }
    return (rally_theme_t){
        {3, 17, 37, 255}, {118, 168, 192, 255}, {24, 36, 54, 255}, {14, 28, 44, 255},
        C_MINT, (Color){136, 218, 255, 255}};
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
    rally_track_pose_t cam, apex;
    if (!rally_track_sample_course(g->course_id, g->progress, g->lateral, &cam)) return;
    if (!rally_track_sample_course(g->course_id, g->progress + 24.0f, 0.0f, &apex)) apex = cam;
    /* Look slightly into the upcoming bend (apex anticipation) and hang the
     * camera subtly toward the inside of the rider's lean. */
    float curve_turn = cam.right_x * apex.tangent_x + cam.right_z * apex.tangent_z;
    float yaw = g->heading_error * 0.85f + curve_turn * 0.22f + g->steering * 0.06f;
    float cy = cosf(yaw), sy = sinf(yaw);
    s_camera.right_x = cam.right_x * cy + cam.tangent_x * sy;
    s_camera.right_z = cam.right_z * cy + cam.tangent_z * sy;
    s_camera.forward_x = cam.tangent_x * cy - cam.right_x * sy;
    s_camera.forward_z = cam.tangent_z * cy - cam.right_z * sy;
    float hang_off = clampf(g->lateral_velocity * 0.035f + g->steering * 0.14f, -0.32f, 0.32f);
    s_camera.x = cam.x - cam.tangent_x * 1.48f - cam.right_x * hang_off;
    float crouch = g->nitro_active ? 0.18f : (g->drifting ? 0.12f : 0.0f);
    crouch += fabsf(g->steering) * 0.08f;
    s_camera.y = cam.y + 1.72f - crouch + g->height * 0.16f;
    s_camera.z = cam.z - cam.tangent_z * 1.48f - cam.right_z * hang_off;
    float roll = -cam.bank * (g->course_id == 0U ? .22f : .48f) -
                 clampf(g->lateral_velocity * 0.022f + g->steering * 0.035f, -0.11f, 0.11f);
    s_camera.roll_cos = cosf(roll);
    s_camera.roll_sin = sinf(roll);
    float impact = g->collision_ticks ?
        clampf(g->impact_speed / RALLY_MAX_SPEED, .3f, 1.0f) : 0.0f;
    impact *= clampf(g->collision_ticks / 18.0f, 0, 1);
    float boost_fov = g->nitro_active ? 22.0f : 0.0f;
    s_camera.focal = RALLY_FOCAL - 18.0f * clampf(g->speed / RALLY_MAX_SPEED, 0, 1.35f) - boost_fov;
    s_camera.shake_x = impact * ((g->collision_ticks & 1U) ? 2.5f : -2.5f);
    s_camera.shake_y = impact * ((g->collision_ticks & 2U) ? 1.5f : -1.5f);
}

static float camera_depth(rally_point3_t point)
{
    return (point.x - s_camera.x) * s_camera.forward_x +
           (point.z - s_camera.z) * s_camera.forward_z;
}

/* Billboard size uses the same camera-space depth as its ground position.
 * Progress units are not world metres and vary along each authored course. */
static float billboard_size(rally_point3_t point, float world_height)
{
    float depth = camera_depth(point);
    return depth >= RALLY_NEAR_Z ? s_camera.focal * world_height / depth : 0;
}

static rally_point2_t project(const rally_game_t *g, rally_point3_t p)
{
    (void)g;
    float dx = p.x - s_camera.x;
    float dz = p.z - s_camera.z;
    float vx = dx * s_camera.right_x + dz * s_camera.right_z;
    float view_z = dx * s_camera.forward_x + dz * s_camera.forward_z;
    float vy = p.y - s_camera.y;
    if (view_z < RALLY_NEAR_Z)
        return (rally_point2_t){0, 0, false, p};
    float sx = 240.0f + s_camera.focal * vx / view_z;
    float sy_screen = (float)s_horizon - s_camera.focal * vy / view_z;
    float ox = sx - 240.0f, oy = sy_screen - (float)s_horizon;
    sx = 240.0f + ox * s_camera.roll_cos - oy * s_camera.roll_sin;
    sy_screen = (float)s_horizon + ox * s_camera.roll_sin + oy * s_camera.roll_cos;
    sx += s_camera.shake_x;
    sy_screen += s_camera.shake_y;
    /* Keep horizontally off-screen vertices valid. The rasterizer clips the
     * resulting triangle; rejecting them here would drop the near road quad
     * exactly when its shoulders expand past both display edges. */
    return (rally_point2_t){sx, sy_screen, true, p};
}

static void draw_sky(const rally_game_t *g)
{
    /* Broad RGB565-safe bands plus a few large silhouettes create a proper
     * authored vista without paying for a 460 KiB full-screen background. */
    rally_theme_t theme = theme_for(g->progress);
    for (int i = 0; i < 12; ++i) {
        float t = i / 11.0f;
        Color band = color_lerp(theme.sky_top, theme.sky_bottom, t * t);
        DrawRectangle(RALLY_LEFT, i * 18, RALLY_SPAN, 19, band);
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
    DrawRectangle(RALLY_LEFT, 216, RALLY_SPAN, 8, color_lerp(theme.terrain, theme.road, .3f));
    DrawRectangle(RALLY_LEFT, 216, RALLY_SPAN, 2, color_lerp(theme.accent, RAYWHITE, .18f));
}

static void draw_ground(const rally_game_t *g)
{
    rally_theme_t theme = theme_for(g->progress);
    /* Calm 60% deep verge without 2D grid noise fighting the 3D road. */
    DrawRectangle(RALLY_LEFT, s_horizon, RALLY_SPAN, RALLY_H - s_horizon, theme.terrain);
    Color distant_land = color_lerp(theme.sky_bottom, theme.terrain, .62f);
    DrawTriangle((Vector2){-80, s_horizon}, (Vector2){95, s_horizon - 22},
                 (Vector2){290, s_horizon}, distant_land);
    DrawTriangle((Vector2){100, s_horizon}, (Vector2){270, s_horizon - 15},
                 (Vector2){430, s_horizon}, distant_land);
    DrawTriangle((Vector2){285, s_horizon}, (Vector2){435, s_horizon - 24},
                 (Vector2){560, s_horizon}, distant_land);
    /* Soft atmospheric horizon transition (no harsh solid cyan line). */
    for (int y = s_horizon - 7; y <= s_horizon + 5; ++y) {
        float t = (float)(y - (s_horizon - 7)) / 12.0f;
        Color mist = color_lerp(theme.sky_bottom, theme.terrain, .35f + .65f * t);
        int inset = (g->course_id == 0U && y < s_horizon)
                        ? (int)(120.0f * (1.0f - t))
                        : RALLY_LEFT;
        DrawRectangle(inset, y, RALLY_W - inset * 2, 1, mist);
    }
}

/* Retain the visible part of a face crossing the near plane. Discarding
 * one boundary loses the near road on tight turns and makes racers float. */
static unsigned clip_near_quad(rally_point2_t a, rally_point2_t b, rally_point2_t c,
                               rally_point2_t d, rally_point2_t out[8])
{
    rally_point2_t input[4] = {a, b, d, c};
    unsigned count = 0;
    for (unsigned i = 0; i < 4; ++i) {
        rally_point2_t p = input[i], q = input[(i + 1) % 4];
        float pz = camera_depth(p.world), qz = camera_depth(q.world);
        if (pz >= RALLY_NEAR_Z)
            out[count++] = p;
        if ((pz >= RALLY_NEAR_Z) != (qz >= RALLY_NEAR_Z)) {
            float t = (RALLY_NEAR_Z + .00001f - pz) / (qz - pz);
            rally_point3_t world = {p.world.x + (q.world.x - p.world.x) * t,
                                    p.world.y + (q.world.y - p.world.y) * t,
                                    p.world.z + (q.world.z - p.world.z) * t};
            out[count++] = project(NULL, world);
        }
    }
    return count;
}

static void draw_quad(Color fill, rally_point2_t a, rally_point2_t b,
                      rally_point2_t c, rally_point2_t d)
{
    if (!(a.visible && b.visible && c.visible && d.visible)) {
        rally_point2_t polygon[8];
        unsigned count = clip_near_quad(a, b, c, d, polygon);
        for (unsigned i = 1; i + 1 < count; ++i)
            DrawTriangle((Vector2){polygon[0].x, polygon[0].y},
                         (Vector2){polygon[i].x, polygon[i].y},
                         (Vector2){polygon[i + 1].x, polygon[i + 1].y}, fill);
        return;
    }
    DrawTriangle((Vector2){a.x, a.y}, (Vector2){b.x, b.y},
                 (Vector2){c.x, c.y}, fill);
    DrawTriangle((Vector2){b.x, b.y}, (Vector2){d.x, d.y},
                 (Vector2){c.x, c.y}, fill);
}

/* Clip objects against the nearest road crest, using the road's own samples. */
static float s_road_top[RALLY_SEGMENTS + 1];
static float world_clip(float distance)
{
    float bottom = RALLY_H;
    for (int i = 0; i <= RALLY_SEGMENTS; ++i) {
        if (s_segment_z[i] * 1.05f >= distance)
            break;
        if (s_road_top[i] < bottom)
            bottom = s_road_top[i];
    }
    return bottom;
}

static void world_quad(Color color, rally_point2_t a, rally_point2_t b, rally_point2_t c,
                       rally_point2_t d, float clip)
{
    rally_point2_t input[8] = {a, b, d, c}, output[12];
    unsigned inputs = 4;
    if (!(a.visible && b.visible && c.visible && d.visible))
        inputs = clip_near_quad(a, b, c, d, input);
    unsigned count = 0;
    for (unsigned i = 0; i < inputs; ++i) {
        rally_point2_t p = input[i], q = input[(i + 1) % inputs];
        if (p.y <= clip)
            output[count++] = p;
        if ((p.y <= clip) != (q.y <= clip)) {
            float t = (clip - p.y) / (q.y - p.y);
            output[count++] = (rally_point2_t){p.x + (q.x - p.x) * t, clip, true, {0, 0, 0}};
        }
    }
    for (unsigned i = 1; i + 1 < count; ++i)
        DrawTriangle((Vector2){output[0].x, output[0].y}, (Vector2){output[i].x, output[i].y},
                     (Vector2){output[i + 1].x, output[i + 1].y}, color);
}

static rally_point2_t local_project(const rally_track_pose_t *pose, float lateral, float along,
                                    float height)
{
    return project(NULL,
                   (rally_point3_t){pose->x + pose->right_x * lateral + pose->tangent_x * along,
                                    pose->y + height,
                                    pose->z + pose->right_z * lateral + pose->tangent_z * along});
}

static void harbor_box(const rally_game_t *g, float distance, float side, bool warehouse)
{
    (void)g; (void)distance; (void)side; (void)warehouse;
}

static void harbor_post(const rally_game_t *g, float distance, float side)
{
    rally_track_pose_t p;
    rally_track_sample_course(0, g->progress + distance, side * 6.8f, &p);
    float clip = world_clip(distance);
    world_quad((Color){44, 66, 92, 255}, local_project(&p, -.10f, 0, 0),
               local_project(&p, .10f, 0, 0), local_project(&p, -.08f, 0, 2.75f),
               local_project(&p, .08f, 0, 2.75f), clip);
    /* Cantilevered cyber streetlamp arm + glowing luminaire head. */
    float arm_in = -side * .85f;
    world_quad((Color){76, 108, 138, 255}, local_project(&p, 0, 0, 2.62f),
               local_project(&p, arm_in, 0, 2.72f), local_project(&p, 0, 0, 2.76f),
               local_project(&p, arm_in, 0, 2.84f), clip);
    world_quad(C_CYAN, local_project(&p, arm_in - .28f, 0, 2.66f),
               local_project(&p, arm_in + .28f, 0, 2.66f),
               local_project(&p, arm_in - .28f, 0, 2.82f),
               local_project(&p, arm_in + .28f, 0, 2.82f), clip);
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
        /* Ground contact and authored world size use one perspective scale. */
        float size = billboard_size(center_world, 1.65f * anchor->scale);
        if (center.y - size > RALLY_H || center.y < 0 || center.x + size < RALLY_LEFT ||
            center.x - size > RALLY_RIGHT)
            return;
        Rectangle source = frame->source;
        if (sprite == RAYLIB_LITE_ASSET_ID_ROADSIDE_BEACON) {
            float turn = s_segment_pose[12].tangent_x * s_segment_pose[22].tangent_z -
                         s_segment_pose[12].tangent_z * s_segment_pose[22].tangent_x;
            if (fabsf(turn) < .08f)
                return;
            if (turn > 0)
                source.width = -source.width;
        }
        float clip = world_clip(distance);
        float top = center.y - size;
        if (top >= clip || top >= RALLY_H || size < 1)
            return;
        float height = fminf(size, clip - top);
        source.height *= height / size;
        if (center.y + size * .045f < clip)
            DrawEllipse((int)center.x, (int)center.y + 1, size * .22f, size * .045f,
                        (Color){5, 12, 22, 255});
        DrawTexturePro(art.texture, source, (Rectangle){center.x - size * .5f, top, size, height},
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

typedef struct {
    float wall_left, wall_right;
    bool fence;
} rally_roadside_t;

static rally_roadside_t roadside_for(unsigned course, float progress)
{
    progress = fmodf(progress, RALLY_TRACK_LENGTH);
    if (progress < 0)
        progress += RALLY_TRACK_LENGTH;
    if (course == 1) {
        if (progress < 280)
            return (rally_roadside_t){2.5f, 0, false};
        if (progress < 600)
            return (rally_roadside_t){3.4f, 2.4f, false};
        if (progress < 820)
            return (rally_roadside_t){.65f, 3.2f, false};
        if (progress < 1250)
            return (rally_roadside_t){2.8f, 0, false};
        return (rally_roadside_t){0, 3.1f, false};
    }
    if (progress < 340)
        return (rally_roadside_t){.75f, .75f, false};
    if (progress < 800)
        return (rally_roadside_t){1.1f, .75f, true};
    if (progress < 1100)
        return (rally_roadside_t){2.0f, .65f, false};
    if (progress < 1500)
        return (rally_roadside_t){.7f, .7f, true};
    return (rally_roadside_t){.85f, .85f, false};
}

static void draw_roadside_ribbon(const rally_game_t *g, const rally_track_pose_t *near,
                                 const rally_track_pose_t *far, float progress)
{
    rally_roadside_t zone = roadside_for(g->course_id, progress);
    for (int side = -1; side <= 1; side += 2) {
        float height = side < 0 ? zone.wall_left : zone.wall_right;
        if (height <= 0)
            continue;
        bool snow = g->course_id == 2;
        Color face = snow ? (Color){78, 124, 158, 255} : (Color){92, 46, 52, 255};
        Color mid = snow ? (Color){132, 182, 212, 255} : (Color){146, 78, 66, 255};
        Color cap = snow ? (Color){218, 238, 246, 255} : (Color){216, 138, 92, 255};
        float inner = side * (snow ? 5.9f : 6.5f), outer = side * (snow ? 7.2f : 7.7f);
        rally_point3_t p;
        track_offset(near, inner, 0, &p);
        rally_point2_t a = project(g, p);
        track_offset(far, inner, 0, &p);
        rally_point2_t b = project(g, p);
        track_offset(near, outer, height, &p);
        rally_point2_t c = project(g, p);
        track_offset(far, outer, height, &p);
        rally_point2_t d = project(g, p);
        world_quad(face, a, b, c, d, RALLY_H);
        track_offset(near, inner, height * .42f, &p);
        a = project(g, p);
        track_offset(far, inner, height * .42f, &p);
        b = project(g, p);
        world_quad(mid, a, b, c, d, RALLY_H);
        track_offset(near, inner, height * .76f, &p);
        a = project(g, p);
        track_offset(far, inner, height * .76f, &p);
        b = project(g, p);
        world_quad(cap, a, b, c, d, RALLY_H);
    }
}

/* Landmarks repeat in road space, with gaps between groups for clear sightlines. */
static void biome_landmark(const rally_game_t *g, float distance, float side, bool fence)
{
    rally_track_pose_t p;
    rally_track_sample_course(g->course_id, g->progress + distance, side * 7.3f, &p);
    float clip = world_clip(distance);
    if (g->course_id == 1) {
        /* Stepped canyon monolith with sunlit rim and glowing horizon band. */
        Color shadow = {58, 28, 38, 255}, stone = {112, 56, 56, 255}, rim = {232, 152, 98, 255};
        world_quad(shadow, local_project(&p, -.72f, 0, 0), local_project(&p, .72f, 0, 0),
                   local_project(&p, -.42f, 0, 3.6f), local_project(&p, .42f, 0, 3.6f), clip);
        float in_l = side < 0 ? -.10f : -.68f, in_r = side < 0 ? .68f : .10f;
        float top_l = side < 0 ? -.05f : -.38f, top_r = side < 0 ? .38f : .05f;
        world_quad(stone, local_project(&p, in_l, -.02f, 0), local_project(&p, in_r, -.02f, 0),
                   local_project(&p, top_l, -.02f, 3.55f), local_project(&p, top_r, -.02f, 3.55f), clip);
        world_quad(rim, local_project(&p, -.58f, -.03f, 2.35f), local_project(&p, .58f, -.03f, 2.35f),
                   local_project(&p, -.46f, -.03f, 2.68f), local_project(&p, .46f, -.03f, 2.68f), clip);
    } else {
        Color wood = {48, 74, 96, 255};
        world_quad(wood, local_project(&p, -.11f, 0, 0), local_project(&p, .11f, 0, 0),
                   local_project(&p, -.11f, 0, fence ? 2.1f : 1.8f),
                   local_project(&p, .11f, 0, fence ? 2.1f : 1.8f), clip);
        if (fence) {
            for (int i = 0; i < 2; ++i) {
                float h = .9f + i * .7f;
                world_quad((Color){142, 186, 208, 255}, local_project(&p, 0, -1.5f, h),
                           local_project(&p, 0, 1.5f, h), local_project(&p, 0, -1.5f, h + .16f),
                           local_project(&p, 0, 1.5f, h + .16f), clip);
            }
        } else {
            world_quad(C_MINT, local_project(&p, -.14f, 0, 1.45f), local_project(&p, .14f, 0, 1.45f),
                       local_project(&p, -.14f, 0, 1.80f), local_project(&p, .14f, 0, 1.80f), clip);
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
    for (int i = 0; i <= RALLY_SEGMENTS; ++i) {
        rally_track_sample_course(g->course_id, g->progress+s_segment_z[i]*1.05f,
                                  0.0f, &s_segment_pose[i]);
        rally_point2_t center = project(
            g, (rally_point3_t){s_segment_pose[i].x, s_segment_pose[i].y, s_segment_pose[i].z});
        s_road_top[i] = center.visible ? center.y : RALLY_H;
    }
    float clip_y = (float)s_horizon;
    /* The harbor's authored broad bends need fewer painted slices. Keep the full
     * sample set for precise object/crest clipping, but join pairs for road
     * faces to pay for its continuous guardrails and buildings. */
    const int stride = g->course_id == 0U ? 2 : 1;
    int order[RALLY_SEGMENTS];
    float depths[RALLY_SEGMENTS];
    unsigned strips = 0;
    for (int i = 0; i < RALLY_SEGMENTS; i += stride) {
        order[strips] = i;
        depths[strips++] = camera_depth(
            (rally_point3_t){s_segment_pose[i].x, s_segment_pose[i].y, s_segment_pose[i].z});
    }
    /* Polar hairpins can turn back toward the camera: progress order is no
     * longer depth order, and screen Y cannot cull a road face as a hill. */
    if (g->course_id == 2U)
        for (unsigned i = 1; i < strips; ++i) {
            int index = order[i];
            float depth = depths[i];
            unsigned j = i;
            while (j && depths[j - 1] < depth) {
                order[j] = order[j - 1];
                depths[j] = depths[j - 1];
                --j;
            }
            order[j] = index;
            depths[j] = depth;
        }
    for (int n = (int)strips - 1; n >= 0; --n) {
        int i = g->course_id == 2U ? order[(int)strips - 1 - n] : order[n];
        const rally_track_pose_t *p0 = &s_segment_pose[i], *p1 = &s_segment_pose[i + stride];
        rally_point3_t p;
        track_offset(p0, -5.0f, 0, &p); rally_point2_t l0 = project(g, p);
        track_offset(p0, 5.0f, 0, &p); rally_point2_t r0 = project(g, p);
        track_offset(p1, -5.0f, 0, &p); rally_point2_t l1 = project(g, p);
        track_offset(p1, 5.0f, 0, &p); rally_point2_t r1 = project(g, p);
        if (!(l0.visible || r0.visible || l1.visible || r1.visible)) continue;
        /* A shoulder can lie behind the near plane on a tight curve. Its
         * placeholder screen coordinate must not determine road visibility. */
        float near_y = s_road_top[i];
        float far_y = s_road_top[i + stride];
        if (g->course_id != 2U && (near_y <= clip_y + .35f || near_y <= far_y))
            continue;
        if (g->course_id != 2U && far_y < clip_y) {
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
        int strip_band = ((int)floorf((g->progress + s_segment_z[i] * 1.05f) * 0.28f)) & 1;
        /* 60% deep charcoal-indigo tarmac: alternating subtle slate bands let
         * the bright Chibi Neko rider and warm reflectors stand out cleanly. */
        Color dark_tarmac = g->course_id == 0U ? (Color){19, 25, 39, 255}
                          : g->course_id == 1U ? (Color){36, 28, 38, 255}
                                               : (Color){22, 34, 50, 255};
        Color light_tarmac = g->course_id == 0U ? (Color){25, 33, 50, 255}
                           : g->course_id == 1U ? (Color){46, 36, 48, 255}
                                                : (Color){30, 44, 64, 255};
        Color base_road = strip_band ? light_tarmac : dark_tarmac;
        Color asphalt =
            color_lerp(base_road, theme.sky_bottom, .26f * s_segment_z[i + stride] / RALLY_FAR_Z);
        float distance_fade = clampf((s_segment_z[i + stride] - 48.0f) / 22.0f, 0, 1);
        asphalt = color_lerp(asphalt, theme.terrain, distance_fade);
        draw_quad(asphalt, l0, r0, l1, r1);
        if (s_segment_z[i] < 46.0f) {
            /* Subtle twin lane wear lines instead of loud neon rails. */
            Color lane_wear = color_lerp(asphalt, RAYWHITE, 0.11f * (1.0f - distance_fade));
            for (int lane = -1; lane <= 1; lane += 2) {
                float lx = (float)lane * 1.80f;
                rally_point2_t nl0, nr0, nl1, nr1;
                track_offset(p0, lx - 0.06f, .02f, &p); nl0 = project(g, p);
                track_offset(p0, lx + 0.06f, .02f, &p); nr0 = project(g, p);
                track_offset(p1, lx - 0.06f, .02f, &p); nl1 = project(g, p);
                track_offset(p1, lx + 0.06f, .02f, &p); nr1 = project(g, p);
                if (nl1.y < clip_y) nl1.y = clip_y;
                if (nr1.y < clip_y) nr1.y = clip_y;
                draw_quad(lane_wear, nl0, nr0, nl1, nr1);
            }
        }

        /* 30% symmetrical horizon-matched curbs + 10% warm amber cat-eye reflectors. */
        track_offset(p0, -5.45f, .05f, &p);
        rally_point2_t ll0 = project(g, p);
        track_offset(p0, -5.00f, .05f, &p);
        rally_point2_t lr0 = project(g, p);
        track_offset(p1, -5.45f, .05f, &p);
        rally_point2_t ll1 = project(g, p);
        track_offset(p1, -5.00f, .05f, &p);
        rally_point2_t lr1 = project(g, p);
        track_offset(p0, 5.00f, .05f, &p);
        rally_point2_t rl0 = project(g, p);
        track_offset(p0, 5.45f, .05f, &p);
        rally_point2_t rr0 = project(g, p);
        track_offset(p1, 5.00f, .05f, &p);
        rally_point2_t rl1 = project(g, p);
        track_offset(p1, 5.45f, .05f, &p);
        rally_point2_t rr1 = project(g, p);
        Color curb = g->course_id == 0U   ? (strip_band ? (Color){88, 152, 188, 255} : (Color){32, 48, 72, 255})
                   : g->course_id == 1U ? (strip_band ? (Color){218, 148, 92, 255} : (Color){86, 48, 52, 255})
                                        : (strip_band ? (Color){186, 224, 238, 255} : (Color){54, 88, 116, 255});
        Color faded_curb = color_lerp(curb, theme.terrain, distance_fade);
        draw_quad(faded_curb, ll0, lr0, ll1, lr1);
        draw_quad(faded_curb, rl0, rr0, rl1, rr1);
        if (strip_band && s_segment_z[i] < 44.0f) {
            Color reflector = g->course_id == 2U ? C_MINT : (Color){255, 196, 84, 255};
            reflector = color_lerp(reflector, theme.terrain, distance_fade);
            rally_point2_t cat_l0, cat_r0, cat_l1, cat_r1;
            track_offset(p0, -4.98f, .06f, &p); cat_l0 = project(g, p);
            track_offset(p0, -4.82f, .06f, &p); cat_r0 = project(g, p);
            track_offset(p1, -4.98f, .06f, &p); cat_l1 = project(g, p);
            track_offset(p1, -4.82f, .06f, &p); cat_r1 = project(g, p);
            draw_quad(reflector, cat_l0, cat_r0, cat_l1, cat_r1);
            track_offset(p0, 4.82f, .06f, &p); cat_l0 = project(g, p);
            track_offset(p0, 4.98f, .06f, &p); cat_r0 = project(g, p);
            track_offset(p1, 4.82f, .06f, &p); cat_l1 = project(g, p);
            track_offset(p1, 4.98f, .06f, &p); cat_r1 = project(g, p);
            draw_quad(reflector, cat_l0, cat_r0, cat_l1, cat_r1);
        }

        if (g->course_id == 0U) {
            float progress = fmodf(g->progress + s_segment_z[i] * 1.05f, RALLY_TRACK_LENGTH);
            bool bridge = progress >= 500 && progress < 800;
            float height = bridge ? .65f : .35f;
            Color wall = bridge ? (Color){28, 44, 68, 255} : (Color){18, 28, 46, 255};
            Color top_trim = bridge ? (Color){255, 196, 84, 255} : (Color){64, 108, 144, 255};
            for (int side = -1; side <= 1; side += 2) {
                float lateral = side * 6.0f;
                track_offset(p0, lateral, 0, &p);
                rally_point2_t a = project(g, p);
                track_offset(p1, lateral, 0, &p);
                rally_point2_t b = project(g, p);
                track_offset(p0, lateral, height, &p);
                rally_point2_t c = project(g, p);
                track_offset(p1, lateral, height, &p);
                rally_point2_t d = project(g, p);
                world_quad(wall, a, b, c, d, RALLY_H);
                track_offset(p0, lateral, height * 0.78f, &p);
                a = project(g, p);
                track_offset(p1, lateral, height * 0.78f, &p);
                b = project(g, p);
                world_quad(color_lerp(top_trim, theme.terrain, distance_fade), a, b, c, d, RALLY_H);
            }
        }
        if (g->course_id != 0U)
            draw_roadside_ribbon(g, p0, p1, g->progress + s_segment_z[i] * 1.05f);
        clip_y = near_y;
    }
}

static void draw_lane_markings(const rally_game_t *g)
{
    /* One light centre dash. Its bend is the corner; the two old side dashes
     * competed with the curbs and did not show where the road was going. */
    const float period = 8.0f;
    const float length = 3.4f;
    const Color dash = {208, 222, 238, 255};
    float first = floorf(g->progress / period) * period;
    for (float world = first + 9.0f * period; world >= first;
         world -= period) {
        float near_distance = world - g->progress;
        float far_distance = near_distance + length;
        if (near_distance < 1.2f || far_distance > RALLY_FAR_Z) continue;
        rally_point3_t p;
        track_world(g, near_distance, -0.18f, .06f, &p);
        rally_point2_t a = project(g, p);
        track_world(g, near_distance, 0.18f, .06f, &p);
        rally_point2_t b = project(g, p);
        track_world(g, far_distance, -0.18f, .06f, &p);
        rally_point2_t c = project(g, p);
        track_world(g, far_distance, 0.18f, .06f, &p);
        rally_point2_t d = project(g, p);
        draw_quad(dash, a, b, c, d);
    }
}

static void draw_corner_rail(const rally_game_t *g)
{
    /* Opaque blocks on the outside curb of the next bend. They replace the
     * screen-space arrow, which covered the road it was trying to explain. */
    rally_track_pose_t near_pose = {0}, far_pose = {0};
    if (!rally_track_sample_course(g->course_id, g->progress + 22.0f, 0, &near_pose) ||
        !rally_track_sample_course(g->course_id, g->progress + 70.0f, 0, &far_pose)) return;
    float turn = near_pose.tangent_x * far_pose.tangent_z -
                 near_pose.tangent_z * far_pose.tangent_x;
    if (fabsf(turn) < 0.10f) return;
    float side = turn < 0.0f ? 1.0f : -1.0f;
    Color mark = fabsf(turn) > 0.35f ? C_GOLD : RAYWHITE;
    for (int i = 0; i < 5; ++i) {
        float near_distance = 12.0f + (float)i * 8.0f;
        float far_distance = near_distance + 3.0f;
        float inner = side * 4.50f;
        float outer = side * 5.65f;
        rally_point3_t p;
        if (!track_world(g, near_distance, inner, 0.07f, &p)) continue;
        rally_point2_t a = project(g, p);
        if (!track_world(g, near_distance, outer, 0.07f, &p)) continue;
        rally_point2_t b = project(g, p);
        if (!track_world(g, far_distance, inner, 0.07f, &p)) continue;
        rally_point2_t c = project(g, p);
        if (!track_world(g, far_distance, outer, 0.07f, &p)) continue;
        rally_point2_t d = project(g, p);
        draw_quad(mark, a, b, c, d);
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

/* Rider state shown on the bike itself: nitro flame, slip sparks, tail
 * light and a tag over the rival. All are a few flat shapes per rider. */
static void draw_rider_effects(const rally_game_t *g, unsigned index, float cx, float top,
                               float size, float clip, float lean)
{
    const rally_opponent_t *opponent = &g->opponents[index];
    const Color color = C_RIDER[index];
    const float offset = clampf(lean, -1, 1) * size * .10f;
    const float rear_y = top + size * .66f;
    if (size >= 10 && rear_y + 2 < clip)
        DrawRectangle((int)(cx + offset - size * .06f), (int)rear_y, (int)(size * .12f) + 1,
                      (int)fmaxf(2, size * .03f), (Color){255, 58, 96, 255});
    if (opponent->intent == RALLY_INTENT_NITRO && size >= 10) {
        const float flicker = (float)((g->tick + index * 3U) % 4U) * .03f;
        const float base_y = top + size * .80f;
        const float tip_y = fminf(base_y + size * (.20f + flicker), clip);
        if (base_y < clip) {
            const float half = size * .07f;
            DrawTriangle((Vector2){cx + offset - half, base_y}, (Vector2){cx + offset, tip_y},
                         (Vector2){cx + offset + half, base_y}, C_CYAN);
            DrawTriangle((Vector2){cx + offset - half * .45f, base_y},
                         (Vector2){cx + offset, base_y + (tip_y - base_y) * .6f},
                         (Vector2){cx + offset + half * .45f, base_y}, RAYWHITE);
        }
    }
    if (opponent->intent == RALLY_INTENT_SLIP && size >= 10) {
        const float ground = top + size * .94f;
        for (unsigned i = 0; i < 4U; ++i) {
            uint32_t seed = (g->tick + i * 7U + index * 13U) * 2654435761u;
            float sx = cx + ((float)(seed % 41U) - 20.0f) * size * .012f;
            float sy = ground - (float)((seed >> 8) % 9U) * size * .012f;
            if (sy < clip)
                DrawRectangle((int)sx, (int)sy, 2, 2, (i & 1U) ? C_GOLD : RAYWHITE);
        }
    }
    if (index == g->rival && size >= 14 && top - 10 > (float)s_horizon - 40) {
        const float y = top - 9;
        DrawTriangle((Vector2){cx, y + 3}, (Vector2){cx - 5, y - 4}, (Vector2){cx + 5, y - 4}, color);
    }
}

static void draw_opponent_car(const rally_game_t *g, float distance, unsigned index,
                              raylib_lite_atlas_t art)
{
    const rally_opponent_t *opponent = &g->opponents[index];
    const Color color = C_RIDER[index];
    rally_track_pose_t at, ahead;
    if (!rally_track_sample_course(g->course_id, g->progress + distance, opponent->lateral, &at) ||
        !rally_track_sample_course(g->course_id, g->progress + distance + 6, 0, &ahead))
        return;
    rally_point2_t center = project(g, (rally_point3_t){at.x, at.y, at.z});
    if (!center.visible)
        return;
    float turn = at.tangent_x * ahead.tangent_z - at.tangent_z * ahead.tangent_x;
    /* Lean follows the actual sideways speed, so the pose builds and
     * releases with the lane change instead of snapping at the decision. */
    float lean = opponent->lateral_velocity * .22f - turn * 1.8f;
    if (opponent->intent == RALLY_INTENT_SLIP)
        lean += ((g->tick >> 2) & 1U) ? .35f : -.35f;
    raylib_lite_asset_id_t pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT;
    if (lean < -.12f)
        pose = lean < -.45f ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_LEFT
                            : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_LEFT;
    else if (lean > .12f)
        pose = lean > .45f ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_RIGHT
                           : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_RIGHT;
    float size = billboard_size((rally_point3_t){at.x, at.y, at.z}, 1.05f);
    /* All five authored poses end at 97% of their cell height. */
    float top=center.y-size*.97f;
    if (size < 1 || top > RALLY_H || center.y < 0 || center.x + size * .5f < RALLY_LEFT ||
        center.x - size * .5f > RALLY_RIGHT)
        return;
    float clip = world_clip(distance);
    if (top >= clip) return;
    if (size < 14.0f) {
        /* Far opponent silhouette LOD: crisp geometric bike + tail lamp avoids
         * noisy sub-14px texture downsampling and saves per-pixel scaling. */
        int bw = (int)fmaxf(3.0f, size * 0.42f);
        int bh = (int)fminf(fmaxf(4.0f, size * 0.78f), clip - top);
        if (bh > 0) {
            int bx = (int)(center.x - bw * 0.5f), by = (int)top;
            DrawRectangle(bx, by, bw, bh, (Color){24, 32, 48, 255});
            DrawRectangle(bx, by + bh / 4, bw, (int)fmaxf(2.0f, bh * 0.35f), color);
            DrawRectangle((int)center.x - 1, by + bh * 2 / 3, 2, 2, (Color){255, 58, 96, 255});
        }
        return;
    }
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(art, pose);
    if (!frame)
        return;
    if (center.y + size * .06f < clip) {
        DrawEllipse((int)center.x, (int)center.y + 1, size * .22f, size * .06f,
                    (Color){4, 10, 20, 255});
        /* Subtle colored underglow on the asphalt identifies the rider cleanly. */
        DrawEllipse((int)center.x, (int)center.y + 1, size * .14f, size * .035f,
                    color_lerp(color, (Color){12, 20, 38, 255}, 0.45f));
    }
    Rectangle source = frame->source;
    float height = fminf(size, clip - top);
    source.height *= height / size;
    Color bike_tint = color_lerp(RAYWHITE, color, 0.28f);
    DrawTexturePro(art.texture, source,
                   (Rectangle){center.x - size * .5f, top, size, height},
                   (Vector2){0, 0}, 0, bike_tint);
    /* Crisp visor stripe and tail-cowl LEDs identify each rider without
     * plastering solid rectangles over the pixel-art jacket. */
    if (size >= 20) {
        float offset = clampf(lean, -1, 1) * size * .10f;
        int visor_y = (int)(top + size * .23f);
        if (visor_y + 2 < (int)clip) {
            int vw = (int)fmaxf(3.0f, size * .10f);
            DrawRectangle((int)(center.x + offset) - vw / 2, visor_y, vw, 2, color);
        }
        int cowl_y = (int)(top + size * .56f);
        if (cowl_y + 3 < (int)clip) {
            int cw = (int)fmaxf(2.0f, size * .05f);
            int gap = (int)fmaxf(3.0f, size * .09f);
            DrawRectangle((int)(center.x + offset) - gap - cw, cowl_y, cw, 3, color);
            DrawRectangle((int)(center.x + offset) + gap, cowl_y, cw, 3, color);
        }
    }
    draw_rider_effects(g, index, center.x, top, size, clip, lean);
}

static void draw_world(const rally_game_t *g, raylib_lite_atlas_t scenery,
                       raylib_lite_atlas_t bikes)
{
    enum {
        WORLD_RIDER,
        WORLD_LAMP,
        WORLD_CONTAINER,
        WORLD_WAREHOUSE,
        WORLD_SPRITE,
        WORLD_BRIDGE,
        WORLD_CHECKPOINT,
        WORLD_LANDMARK
    };
    typedef struct {
        float distance;
        unsigned kind, index;
        float side;
        float depth;
    } item_t;
    item_t items[32];
    unsigned count = 0;
    rally_track_scenery_t anchors[18];
    unsigned anchor_count = 0;
    if (g->course_id == 0) {
        /* Twin cyber streetlamps every 18 units give a clean high-speed rhythm
         * without giant harbor boxes blocking the skyline. */
        float first = floorf(g->progress / 18) * 18;
        for (int i = 0; i < 6; ++i) {
            float at = first + i * 18, dist = at - g->progress;
            if (dist <= RALLY_NEAR_Z || dist >= RALLY_FAR_Z)
                continue;
            items[count++] = (item_t){dist, WORLD_LAMP, 0, -1, 0};
            items[count++] = (item_t){dist, WORLD_LAMP, 0, 1, 0};
        }
    } else {
        float first = floorf(g->progress / 18) * 18;
        for (int i = 0; i < 5; ++i) {
            float at = first + i * 18, dist = at - g->progress;
            if (dist <= RALLY_NEAR_Z || dist >= RALLY_FAR_Z)
                continue;
            unsigned index = (unsigned)(at / 18);
            /* Paired groups are separated by open sections; avoid scatter. */
            if (g->course_id == 1 && index % 6 >= 3)
                continue;
            if (g->course_id == 2 && roadside_for(2, at).fence) {
                items[count++] = (item_t){dist, WORLD_LANDMARK, 0, -1, 0};
                items[count++] = (item_t){dist, WORLD_LANDMARK, 0, 1, 0};
            } else
                items[count++] = (item_t){dist, WORLD_LANDMARK, 0, (index & 1) ? -1 : 1, 0};
        }
        /* One low rock group every 72 progress units rather than random
         * large props on every road segment. */
        first = floorf(g->progress / 72) * 72;
        for (int i = 0; i < 2; ++i) {
            float at = first + i * 72, dist = at - g->progress;
            if (dist <= RALLY_NEAR_Z || dist >= RALLY_FAR_Z)
                continue;
            rally_track_scenery_t anchor = {0};
            anchor.anchor_progress = fmodf(at, RALLY_TRACK_LENGTH);
            anchor.lateral = ((unsigned)(at / 72) & 1) ? -8.8f : 8.8f;
            anchor.scale = .75f;
            anchor.kind = 1;
            rally_track_pose_t pose;
            rally_track_sample_course(g->course_id, at, anchor.lateral, &pose);
            anchor.world_x = pose.x;
            anchor.world_y = pose.y;
            anchor.world_z = pose.z;
            anchors[anchor_count] = anchor;
            items[count++] = (item_t){dist, WORLD_SPRITE, anchor_count++, 0, 0};
        }
    }
    if (g->course_id == 0U) {
        for (float at = 560; at <= 680; at += 120) {
            float dist = at - g->progress;
            if (dist < RALLY_NEAR_Z)
                dist += RALLY_TRACK_LENGTH;
            if (dist > RALLY_NEAR_Z && dist < RALLY_FAR_Z)
                items[count++] = (item_t){dist, WORLD_BRIDGE, 0, 0, 0};
        }
    }
    for (unsigned i = 0; i < RALLY_CHECKPOINT_COUNT - 1U; ++i) {
        float dist = rally_checkpoint_distance(i) - g->progress;
        if (dist < RALLY_NEAR_Z)
            dist += RALLY_TRACK_LENGTH;
        if (dist > RALLY_NEAR_Z && dist < RALLY_FAR_Z)
            items[count++] = (item_t){dist, WORLD_CHECKPOINT, i, 0, 0};
    }
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        if (!g->opponents[i].active)
            continue;
        float dist = g->opponents[i].progress - g->progress;
        if (dist < -RALLY_TRACK_LENGTH * .5f)
            dist += RALLY_TRACK_LENGTH;
        if (dist > RALLY_TRACK_LENGTH * .5f)
            dist -= RALLY_TRACK_LENGTH;
        if (dist > -4.0f && dist < RALLY_FAR_Z)
            items[count++] = (item_t){dist, WORLD_RIDER, i, 0, 0};
    }
    for (unsigned i = 0; i < count; ++i) {
        item_t *item = &items[i];
        rally_track_pose_t pose;
        float lateral = 0;
        if (item->kind == WORLD_RIDER)
            lateral = g->opponents[item->index].lateral;
        else if (item->kind == WORLD_LAMP)
            lateral = item->side * 6.8f;
        else if (item->kind == WORLD_LANDMARK)
            lateral = item->side * 7.3f;
        else if (item->kind == WORLD_CONTAINER)
            lateral = item->side * 9.2f;
        else if (item->kind == WORLD_WAREHOUSE)
            lateral = item->side * 10.8f;
        else if (item->kind == WORLD_SPRITE)
            lateral = anchors[item->index].lateral;
        else if (item->kind == WORLD_CHECKPOINT)
            lateral = (item->index & 1) ? 5.55f : -5.55f;
        rally_track_sample_course(g->course_id, g->progress + item->distance, lateral, &pose);
        item->depth = camera_depth((rally_point3_t){pose.x, pose.y, pose.z});
    }
    for (unsigned i = 1; i < count; ++i) {
        item_t value = items[i];
        unsigned j = i;
        while (j && items[j - 1].depth < value.depth) {
            items[j] = items[j - 1];
            --j;
        }
        items[j] = value;
    }
    for (unsigned i = 0; i < count; ++i) {
        item_t item = items[i];
        if (item.kind == WORLD_RIDER)
            draw_opponent_car(g, item.distance, item.index, bikes);
        else if (item.kind == WORLD_LAMP)
            harbor_post(g, item.distance, item.side);
        else if (item.kind == WORLD_LANDMARK)
            biome_landmark(g, item.distance, item.side,
                           roadside_for(g->course_id, g->progress + item.distance).fence);
        else if (item.kind == WORLD_CONTAINER || item.kind == WORLD_WAREHOUSE)
            harbor_box(g, item.distance, item.side, item.kind == WORLD_WAREHOUSE);
        else if (item.kind == WORLD_CHECKPOINT) {
            rally_theme_t theme = theme_for(g->progress);
            draw_billboard(g, item.distance, (item.index & 1) ? 5.55f : -5.55f,
                           (item.index & 1) ? theme.accent2 : theme.accent, true);
        } else if (item.kind == WORLD_BRIDGE) {
            rally_track_pose_t pose;
            rally_track_sample_course(0, g->progress + item.distance, 0, &pose);
            float clip = world_clip(item.distance);
            Color metal = {102, 127, 145, 255};
            for (int side = -1; side <= 1; side += 2)
                world_quad(metal, local_project(&pose, side * 6.1f, 0, 0),
                           local_project(&pose, side * 6.5f, 0, 0),
                           local_project(&pose, side * 6.1f, 0, 4.4f),
                           local_project(&pose, side * 6.5f, 0, 4.4f), clip);
            world_quad(metal, local_project(&pose, -6.5f, 0, 4.1f),
                       local_project(&pose, 6.5f, 0, 4.1f), local_project(&pose, -6.5f, 0, 4.4f),
                       local_project(&pose, 6.5f, 0, 4.4f), clip);
        } else {
            rally_track_scenery_t *anchor = &anchors[item.index];
            raylib_lite_asset_id_t sprite =
                (anchor->kind & 1) ? (g->course_id == 1 ? RAYLIB_LITE_ASSET_ID_ROADSIDE_ROCK
                                                        : RAYLIB_LITE_ASSET_ID_ROADSIDE_SNOW)
                                   : RAYLIB_LITE_ASSET_ID_ROADSIDE_BEACON;
            draw_scenery_prop(g, item.distance, anchor, theme_for(g->progress), scenery, sprite);
        }
    }
}

static void draw_collision_feedback(const rally_game_t *g)
{
    if (!g->collision_ticks) return;
    unsigned strength = (unsigned)g->collision_ticks;
    /* Crisp pixel sparks flying off the rear tire without ugly full-screen borders. */
    float bx = 240.0f + g->lateral * 7.0f;
    for (unsigned i = 0; i < 8U; ++i) {
        int dir = (i & 1U) ? 1 : -1;
        int age = (int)(18U - strength) + (int)i * 2;
        int sx = (int)bx + dir * (12 + age * 2 + (int)(i * 3U));
        int sy = 434 - (int)(i * 5U) + (age % 7) * 2;
        DrawRectangle(sx, sy, 4, 2, (i & 1U) ? C_GOLD : C_PINK);
    }
}

static void draw_vehicle(const rally_game_t *g, raylib_lite_atlas_t art,
                         const rally_view_motion_t *motion)
{
    int pose_direction = motion ? motion->pose : 0;
    float abs_pose = (float)(pose_direction < 0 ? -pose_direction : pose_direction);
    /* Spring-damped chase sway: the bike carves into the turn dynamically
     * with lateral velocity and steering impulse instead of sitting glued
     * to the screen edge whenever the rider holds an outer lane. */
    float dynamic_sway = clampf(g->lateral_velocity * 1.65f + g->steering * 9.5f, -24.0f, 24.0f);
    float x = 240.0f + g->lateral * 2.4f + dynamic_sway;
    float bob = g->grounded ? sinf((float)g->tick * .30f) *
                clampf(g->speed / RALLY_MAX_SPEED, 0.0f, 1.0f) * 1.2f : 0.0f;
    /* Tire contact patch stays planted while the chassis and helmet drop
     * naturally by cos(lean) as the rider leans into the corner. */
    float lean_drop = abs_pose == 1.0f ? 2.0f : (abs_pose >= 2.0f ? 5.0f : 0.0f);
    float ground_y = 448.0f - g->height * 8.0f + bob;
    if (g->collision_ticks) x += (g->collision_ticks & 1U) ? 4.0f : -4.0f;
    float shadow_y = 451.0f + bob;
    float shadow_size = (24.0f + abs_pose * 2.5f) / (1 + g->height * .2f);
    int shadow_shift = pose_direction * 3;
    DrawEllipse((int)x + shadow_shift, (int)shadow_y, shadow_size, 7, (Color){4, 7, 15, 255});
    /* Warm taillight & boost reflection on the dark tarmac under the Chibi bike. */
    DrawEllipse((int)x + shadow_shift, (int)shadow_y + 1, shadow_size * 0.65f, 3,
                g->nitro_active ? (Color){72, 232, 198, 160}
                                : (g->drifting ? (Color){255, 200, 70, 150}
                                               : (Color){220, 48, 110, 130}));
    if (g->drifting || g->drift_boost_ticks || abs_pose >= 2.0f) {
        int side = pose_direction != 0 ? (pose_direction < 0 ? -1 : 1)
                                       : (g->lateral_velocity < 0 ? -1 : 1);
        for (int i = 0; i < 4; ++i) {
            int phase = (int)((g->tick + i * 3) % 9);
            DrawRectangle((int)x + side * (10 + i * 4), (int)ground_y - 2 + phase, 3, 2,
                          i & 1 ? RAYWHITE : (g->drifting ? C_GOLD : C_CYAN));
        }
    }
    if (g->nitro_active) {
        int trail = 29 + (int)(g->speed * .5f);
        /* Two clean twin triangles instead of per-pixel distance-field DrawLineEx. */
        DrawTriangle((Vector2){x - 20.0f, ground_y - 7.0f},
                     (Vector2){x - 24.0f, ground_y + (float)trail},
                     (Vector2){x - 14.0f, ground_y - 7.0f}, C_CYAN);
        DrawTriangle((Vector2){x - 18.0f, ground_y - 7.0f},
                     (Vector2){x - 22.0f, ground_y + (float)trail * 0.65f},
                     (Vector2){x - 16.0f, ground_y - 7.0f}, RAYWHITE);
        DrawTriangle((Vector2){x + 14.0f, ground_y - 7.0f},
                     (Vector2){x + 24.0f, ground_y + (float)trail},
                     (Vector2){x + 20.0f, ground_y - 7.0f}, C_PINK);
        DrawTriangle((Vector2){x + 16.0f, ground_y - 7.0f},
                     (Vector2){x + 22.0f, ground_y + (float)trail * 0.65f},
                     (Vector2){x + 18.0f, ground_y - 7.0f}, RAYWHITE);
    }
    const bool native = raylib_lite_design_view_native(s_view);
    raylib_lite_asset_id_t pose = native ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT
                                         : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT_56;
    /* Pose IDs use the player's screen-space steering direction. */
    if (pose_direction <= -2)
        pose = native ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_LEFT
                      : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_LEFT_56;
    else if (pose_direction == -1)
        pose = native ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_LEFT
                      : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_LEFT_56;
    else if (pose_direction == 1)
        pose = native ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_RIGHT
                      : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_RIGHT_56;
    else if (pose_direction >= 2)
        pose = native ? RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_RIGHT
                      : RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_RIGHT_56;
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(art, pose);
    if (!frame) frame = raylib_lite_atlas_get_frame(art, RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT);
    if (frame) {
        /* Authored poses carry the lean. The 56-pixel landscape variant is
         * drawn 1:1; the square view scales the 80-pixel source to 112. */
        float tail_lag = g->drifting ? -g->lateral_velocity * .65f : 0.0f;
        const float sprite = 112.0f;
        float dx = floorf(x + tail_lag + (float)pose_direction * 2.0f - sprite * 0.5f + 0.5f);
        float dy = floorf(ground_y + lean_drop - 4.0f - sprite * 0.90f + 0.5f);
        if (!native) {
            dx = floorf(dx * 0.5f) * 2.0f;
            dy = floorf(dy * 0.5f) * 2.0f;
        }
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){dx, dy, sprite, sprite},
                       (Vector2){0.0f, 0.0f}, 0, WHITE);
    }
}

static const Color C_PANEL = {4, 9, 29, 196};
static const Color C_MUTED = {170, 188, 214, 255};

static void text_centered(const char *text, int cx, int y, int size, Color color)
{
    draw_text(text, cx - measure_text(text, size) / 2, y, size, color);
}

static const char *ordinal(unsigned rank)
{
    return rank == 1U ? "ST" : rank == 2U ? "ND" : rank == 3U ? "RD" : "TH";
}

static Color rank_color(unsigned rank)
{
    return rank == 1U ? C_GOLD : rank <= 3U ? C_MINT : C_PINK;
}

static float speed_ratio(const rally_game_t *g)
{
    return clampf(g->speed / RALLY_MAX_SPEED, 0.0f, 1.35f);
}

/* Radial streaks from the vanishing point; they skip the wedge below the
 * rider so the bike stays readable at full boost. */
static void draw_speed_lines(const rally_game_t *g)
{
    if (g->phase != RALLY_PHASE_PLAYING) return;
    float intensity = clampf((speed_ratio(g) - .62f) / .38f, 0.0f, 1.0f);
    if (g->nitro_active) intensity = 1.0f;
    if (intensity <= 0.0f) return;
    const float cx = 240.0f, cy = (float)s_horizon - 12.0f;
    const unsigned count = g->nitro_active ? 18U : 11U;
    for (unsigned i = 0; i < count; ++i) {
        uint32_t seed = (i * 2654435761u) ^ 0x9e3779b9u;
        float angle = (float)(seed % 6283U) * .001f;
        float dy = sinf(angle);
        if (dy > .55f && fabsf(cosf(angle)) < .62f) angle += 1.4f;
        float travel = (float)((seed >> 8) % 100U) * .01f +
                       (float)g->tick * (.045f + .03f * intensity);
        travel -= floorf(travel);
        float r0 = 70.0f + travel * 300.0f;
        float r1 = r0 + 18.0f + travel * 70.0f * intensity;
        Color color = g->nitro_active ? ((i & 1U) ? C_CYAN : C_PINK) : RAYWHITE;
        color.a = (unsigned char)(70.0f + 150.0f * intensity * travel);
        DrawLineEx((Vector2){cx + cosf(angle) * r0, cy + sinf(angle) * r0},
                   (Vector2){cx + cosf(angle) * r1, cy + sinf(angle) * r1},
                   1.5f, color);
    }
    if (g->nitro_active) {
        unsigned char pulse = (unsigned char)(70 + 40 * ((g->tick >> 1) & 1U));
        /* Four stepped vertical bands per side: each band is 6..10px wide so
         * DrawRectangle writes contiguous horizontal spans per row instead of
         * 30 single-pixel vertical columns x 410 rows (24,600 runs!). */
        static const int band_w[4] = {6, 7, 8, 9};
        static const int band_x[4] = {0, 6, 13, 21};
        static const unsigned num[4] = {4, 3, 2, 1};
        for (int b = 0; b < 4; ++b) {
            unsigned char a = (unsigned char)((unsigned)pulse * num[b] / 4U);
            DrawRectangle(RALLY_LEFT + band_x[b], 70, band_w[b], RALLY_H - 70,
                          (Color){44, 235, 255, a});
            DrawRectangle(RALLY_RIGHT - band_x[b] - band_w[b], 70, band_w[b], RALLY_H - 70,
                          (Color){255, 62, 181, a});
        }
    }
}

/* Frameless cyberpunk HUD: zero boxy containers, pure floating neon typography
 * with crisp 1px drop shadows so 100% of the sky vista remains visible. */
static void draw_hud_text(const char *text, int x, int y, int size, Color color)
{
    draw_text(text, x + 1, y + 1, size, (Color){2, 5, 16, 230});
    draw_text(text, x, y, size, color);
}

static void draw_hud_centered(const char *text, int cx, int y, int size, Color color)
{
    draw_hud_text(text, cx - measure_text(text, size) / 2, y, size, color);
}

static void draw_position(const rally_game_t *g)
{
    unsigned rank = g->position ? g->position : RALLY_OPPONENT_COUNT + 1U;
    Color color = rank_color(rank);
    const char *pos = TextFormat("P%u", rank);
    draw_hud_text(pos, 14, 10, 24, color);
    int px = 14 + measure_text(pos, 24) + 3;
    draw_hud_text(TextFormat("/%u", RALLY_OPPONENT_COUNT + 1U), px, 18, 10, C_MUTED);
    float hull = clampf((float)g->integrity / 100.0f, 0.0f, 1.0f);
    DrawRectangle(14, 38, 68, 3, (Color){14, 24, 44, 200});
    DrawRectangle(14, 38, (int)(68.0f * hull), 3, color_lerp(C_PINK, C_MINT, hull));
}

static void draw_lap(const rally_game_t *g)
{
    unsigned lap = g->laps_completed + 1U;
    if (lap > RALLY_TARGET_LAPS) lap = RALLY_TARGET_LAPS;
    const bool driving = g->phase == RALLY_PHASE_PLAYING || g->phase == RALLY_PHASE_COUNTDOWN;
    const bool final_lap = lap == RALLY_TARGET_LAPS;
    if (!driving) {
        draw_hud_centered(TextFormat("LAP %u/%u", lap, RALLY_TARGET_LAPS), 240, 14, 20,
                          final_lap ? C_GOLD : RAYWHITE);
        return;
    }
    const unsigned kmh = (unsigned)(g->speed * 6.0f + .5f);
    const char *spd = TextFormat("%03u", kmh);
    int sw = measure_text(spd, 26);
    int kw = measure_text("KM/H", 10);
    int sx = 240 - (sw + 4 + kw) / 2;
    draw_hud_text(spd, sx, 8, 26, g->nitro_active ? C_CYAN : RAYWHITE);
    draw_hud_text("KM/H", sx + sw + 4, 18, 10, g->nitro_active ? C_PINK : C_CYAN);
    const float ratio = speed_ratio(g);
    const int segments = 16;
    const int lit = (int)(ratio * (float)segments + .5f);
    for (int i = 0; i < segments; ++i) {
        const float t = (float)i / (float)(segments - 1);
        Color color = t < .55f ? C_MINT : (t < .85f ? C_GOLD : C_PINK);
        if (g->nitro_active) color = (i + (int)g->tick) & 1 ? C_CYAN : C_PINK;
        if (i >= lit) color = (Color){16, 26, 46, 180};
        DrawRectangle(160 + i * 10, 36, 8, 3, color);
    }
}

static void draw_score(const rally_game_t *g, const rally_view_motion_t *motion)
{
    uint32_t score = motion ? motion->shown_score : g->score;
    unsigned lap = g->laps_completed + 1U;
    if (lap > RALLY_TARGET_LAPS) lap = RALLY_TARGET_LAPS;
    const bool combo_on = g->combo > 1U;
    const char *digits = TextFormat("%06lu", (unsigned long)score);
    draw_hud_text(digits, 466 - measure_text(digits, 20), 10, 20,
                  combo_on ? C_GOLD : RAYWHITE);
    const char *sub = combo_on ? TextFormat("COMBO x%u", (unsigned)g->combo)
                               : TextFormat("LAP %u/%u", lap, RALLY_TARGET_LAPS);
    draw_hud_text(sub, 466 - measure_text(sub, 10), 32, 10,
                  combo_on ? C_GOLD : C_CYAN);
}

/* Bottom-left virtual steering pad symmetrical with the N2O button at (420, 412). */
static void draw_steer_pad(const rally_game_t *g, const rally_view_motion_t *motion)
{
    const int cx = 56, cy = 420;
    /* motion->steering has a peak scale around +-10; normalize to [-1, 1] so the
     * thumb nub glides smoothly instead of pegging at the stop on small turns. */
    const float raw = motion ? (motion->steering / 9.5f) : g->steering;
    const float steer = clampf(raw, -1.0f, 1.0f);
    const bool active = fabsf(steer) > 0.04f || g->drifting;
    const Color accent = g->drifting ? C_GOLD : (active ? C_CYAN : C_MUTED);
    DrawCircle(cx, cy, 34, (Color){4, 9, 29, 188});
    DrawCircle(cx, cy, 20, Fade(accent, active ? .22f : .08f));
    DrawCircleLines(cx, cy, 34, Fade(accent, .65f));
    DrawRectangle(cx - 22, cy - 2, 44, 4, (Color){20, 32, 56, 230});
    int fill_w = (int)(fabsf(steer) * 20.0f);
    if (fill_w > 0) {
        int fx = steer < 0 ? (cx - fill_w) : cx;
        DrawRectangle(fx, cy - 2, fill_w, 4, accent);
    }
    Color left_c = steer < -0.08f ? accent : C_MUTED;
    Color right_c = steer > 0.08f ? accent : C_MUTED;
    DrawTriangle((Vector2){(float)(cx - 28), (float)cy},
                 (Vector2){(float)(cx - 21), (float)(cy + 5)},
                 (Vector2){(float)(cx - 21), (float)(cy - 5)}, left_c);
    DrawTriangle((Vector2){(float)(cx + 28), (float)cy},
                 (Vector2){(float)(cx + 21), (float)(cy - 5)},
                 (Vector2){(float)(cx + 21), (float)(cy + 5)}, right_c);
    int thumb_x = cx + (int)(steer * 18.0f);
    DrawCircle(thumb_x, cy, 9, g->drifting ? C_GOLD : (active ? C_CYAN : (Color){186, 224, 242, 255}));
    DrawCircleLines(thumb_x, cy, 9, RAYWHITE);
}

/* The nitro touch zone (x >= 300, y >= 310) gets a visible button whose
 * ring of pips shows the remaining charge. */
static void draw_nitro_button(const rally_game_t *g)
{
    const float cx = 424.0f, cy = 420.0f;
    const float charge = clampf(g->nitro / RALLY_NITRO_MAX, 0.0f, 1.0f);
    const bool ready = charge >= .2f;
    Color ring = g->nitro_active ? C_GOLD : (ready ? C_CYAN : C_MUTED);
    DrawCircle((int)cx, (int)cy, 34, (Color){4, 9, 29, 188});
    DrawCircle((int)cx, (int)cy, 21, Fade(ring, g->nitro_active ? .28f : .10f));
    DrawCircleLines((int)cx, (int)cy, 34, Fade(ring, .65f));
    if (g->nitro_active) DrawCircleLines((int)cx, (int)cy, 38 + (int)(g->tick % 5U), Fade(C_GOLD, .6f));
    const unsigned pips = 12U;
    for (unsigned i = 0; i < pips; ++i) {
        float a = -1.5708f + 6.2832f * (float)i / (float)pips;
        bool on = (float)i < charge * (float)pips;
        DrawCircle((int)(cx + cosf(a) * 28.0f), (int)(cy + sinf(a) * 28.0f), 2,
                   on ? ring : (Color){26, 40, 68, 255});
    }
    text_centered("N2O", (int)cx, (int)cy - 5, 10, ring);
    if (g->draft_meter > 1.0f || g->draft_boost_ticks) {
        float draft = g->draft_boost_ticks ? 1.0f : clampf(g->draft_meter / 100.0f, 0.0f, 1.0f);
        DrawRectangle((int)cx - 28, (int)cy - 44, 56, 4, (Color){20, 32, 56, 255});
        DrawRectangle((int)cx - 28, (int)cy - 44, (int)(56.0f * draft), 4,
                      g->draft_boost_ticks ? C_MINT : C_CYAN);
    }
}

static void draw_hud(const rally_game_t *g, const rally_view_motion_t *motion)
{
    const bool driving = g->phase == RALLY_PHASE_PLAYING || g->phase == RALLY_PHASE_COUNTDOWN;
    draw_lap(g);
    pin_hud(-s_margin);
    draw_position(g);
    if (driving) draw_steer_pad(g, motion);
    pin_hud(s_margin);
    draw_score(g, motion);
    if (driving) draw_nitro_button(g);
    pin_hud(0);
}

static void draw_control_mode(const rally_game_t *game)
{
    /* The mode hot zone remains active during racing, so keep it visible. */
    const bool show_mode = true;
    const bool scaled = !raylib_lite_design_view_native(s_view);
    const char *mode=game->control_mode==RALLY_CONTROL_TOUCH?"TOUCH":
        game->control_mode==RALLY_CONTROL_TILT?"TILT":"KEYS";
    const int box_x = scaled ? 42 : 10;
    const int box_w = scaled ? 114 : 108;
    const int center_x = box_x + box_w / 2 + 2;
    if (show_mode) {
        DrawRectangleRounded((Rectangle){(float)box_x, 74, (float)box_w, 32}, .3f, 4, C_PANEL);
        DrawRectangle(box_x, 80, 3, 20, C_CYAN);
        text_centered(TextFormat("%s >",mode),center_x,84,12,C_CYAN);
    }
    if(game->control_mode==RALLY_CONTROL_TILT){
        const int c_x = scaled ? 338 : 362;
        const int c_w = scaled ? 114 : 108;
        DrawRectangleRounded((Rectangle){(float)c_x, 74, (float)c_w, 32}, .3f, 4, C_PANEL);
        DrawRectangle(c_x, 80, 3, 20, C_GOLD);
        text_centered("CENTER",c_x+c_w/2+2,84,12,C_GOLD);
    }
    if(game->phase==RALLY_PHASE_COUNTDOWN)
        text_centered(game->control_mode==RALLY_CONTROL_KEYBOARD?"KEYS: W GAS / S BRAKE":"AUTO GAS - STEER + BOOST",240,276,12,RAYWHITE);
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
    /* Each digit pops in large and settles over its 30-tick slot. */
    const float settle = 1.0f - (float)(g->tick % 30U) / 30.0f;
    const int radius = 42 + (int)(settle * settle * 10.0f);
    DrawCircle(240, 147, (float)radius, (Color){3, 8, 24, 226});
    DrawCircleLines(240, 147, (float)radius, color_lerp(color, RAYWHITE, .18f));
    DrawCircleLines(240, 147, (float)(radius - 6), color);
    text_centered(label, 240, label[1] ? 128 : 122, label[1] ? 40 : 52, color);
    if (g->phase != RALLY_PHASE_COUNTDOWN) return;
    static const char *names[3] = {
        "NEON LOOP", "SUNSET SPRINT", "POLAR RIFT"
    };
    static const Color course_colors[3] = {
        {44, 235, 255, 255}, {255, 146, 70, 255}, {150, 220, 255, 255}
    };
    DrawRectangleRounded((Rectangle){77, 199, 326, 64}, .3f, 6, C_PANEL);
    const Color accent = course_colors[course_id % 3U];
    DrawRectangleRounded((Rectangle){83, 205, 30, 28}, .4f, 4, Fade(accent, .25f));
    DrawRectangleRounded((Rectangle){367, 205, 30, 28}, .4f, 4, Fade(accent, .25f));
    draw_text("<", 92, 207, 22, accent);
    draw_text(">", 376, 207, 22, accent);
    text_centered(names[course_id % 3U], 240, 207, 20, RAYWHITE);
    for (unsigned i = 0; i < 3U; ++i)
        DrawCircle(228 + (int)i * 12, 232, 2, i == course_id % 3U ? accent : C_MUTED);
    const char *hint = "TAP ARROWS TO PICK A COURSE";
    if (measure_text(hint, 10) > 300) hint = "PICK A COURSE";
    text_centered(hint, 240, 248, 10, Fade(RAYWHITE, .7f));
}

static void chip(const char *text, int y, Color accent, float alpha)
{
    const int sz = raylib_lite_design_view_native(s_view) ? 20 : 10;
    const int width = measure_text(text, sz);
    DrawRectangle(240 - width / 2 - 8, y + sz + 2, width + 16, 2, Fade(accent, alpha));
    draw_hud_centered(text, 240, y, sz, Fade(accent, alpha));
}

static void draw_race_alert(const rally_game_t *g, const rally_view_motion_t *motion)
{
    if (g->phase != RALLY_PHASE_PLAYING) return;
    const int y = 64;
    if (g->offtrack) {
        if ((g->tick >> 2) & 1U) chip("OFF TRACK", y, C_PINK, 1.0f);
        return;
    }
    if (g->collision_ticks > 4U) {
        chip("CRASH -12", y, C_PINK, 1.0f);
        return;
    }
    if (motion && motion->alert_ticks && motion->callout != RALLY_CALLOUT_LAP &&
               motion->callout != RALLY_CALLOUT_FINAL_LAP) {
        const float alpha = clampf((float)motion->alert_ticks / 10.0f, 0.0f, 1.0f);
        if (motion->alert == 1U) chip("OVERTAKE  +1", y, C_MINT, alpha);
        else if (motion->alert == 2U) chip("OVERTAKEN", y, C_PINK, alpha);
        else if (motion->alert == 3U) chip("SLIPSTREAM", y, C_CYAN, alpha);
        return; /* One central cue at a time; retain the road's vanishing point. */
    }
    if (!motion || !motion->callout_ticks) return;
    const float alpha = clampf((float)motion->callout_ticks / 10.0f, 0.0f, 1.0f);
    if (motion->callout == RALLY_CALLOUT_LAP || motion->callout == RALLY_CALLOUT_FINAL_LAP) {
        const bool final_lap = motion->callout == RALLY_CALLOUT_FINAL_LAP;
        const Color accent = final_lap ? C_GOLD : C_CYAN;
        const float grow = clampf((54.0f - (float)motion->callout_ticks) / 8.0f, 0.0f, 1.0f);
        const int half = (int)(170.0f * grow);
        DrawRectangle(240 - half, 68, half * 2, 42, Fade(C_PANEL, alpha));
        DrawRectangle(240 - half, 68, half * 2, 2, Fade(accent, alpha));
        DrawRectangle(240 - half, 108, half * 2, 2, Fade(accent, alpha));
        if (grow >= 1.0f)
            text_centered(final_lap ? "FINAL LAP" : TextFormat("LAP %lu", (unsigned long)motion->callout_value),
                          240, 76, 24, Fade(accent, alpha));
        return;
    }
    if (motion->callout == RALLY_CALLOUT_NEAR_MISS)
        chip(motion->callout_value ? TextFormat("NEAR MISS +%lu", (unsigned long)motion->callout_value)
                                   : "NEAR MISS", y, C_GOLD, alpha);
    else if (motion->callout == RALLY_CALLOUT_DRIFT_BOOST)
        chip(motion->callout_value ? TextFormat("DRIFT BOOST +%lu", (unsigned long)motion->callout_value)
                                   : "DRIFT BOOST", y, C_MINT, alpha);
    else if (motion->callout == RALLY_CALLOUT_CHECKPOINT)
        chip("CHECKPOINT", y, C_CYAN, alpha);
    else if (motion->callout == RALLY_CALLOUT_RIVAL_BOOST && motion->callout_value < RALLY_OPPONENT_COUNT)
        chip(TextFormat("%s BOOSTS", rally_rider_name(motion->callout_value)), y,
             C_RIDER[motion->callout_value], alpha);
    else if (motion->callout == RALLY_CALLOUT_RIVAL_SLIP && motion->callout_value < RALLY_OPPONENT_COUNT)
        chip(TextFormat("%s SLIPS  GO!", rally_rider_name(motion->callout_value)), y, C_MINT, alpha);
}

static void draw_result(const rally_game_t *g)
{
    const float seconds = (float)g->tick / RALLY_TICK_HZ;
    if (g->phase == RALLY_PHASE_FINISHED) {
        const unsigned rank = g->position ? g->position : RALLY_OPPONENT_COUNT + 1U;
        const Color accent = rank == 1U ? C_GOLD :
                             rank == 2U ? (Color){205, 222, 240, 255} :
                             rank == 3U ? (Color){232, 142, 82, 255} : C_MUTED;
        DrawRectangle(RALLY_LEFT, 0, RALLY_SPAN, RALLY_H, (Color){2, 4, 14, 120});
        DrawRectangleRounded((Rectangle){64, 140, 352, 214}, .12f, 6, (Color){4, 10, 31, 240});
        DrawRectangle(64, 150, 352, 3, accent);
        text_centered("RIFT CLEARED", 240, 164, 20, RAYWHITE);
        DrawCircle(240, 222, 34, Fade(accent, .2f));
        DrawCircleLines(240, 222, 34, accent);
        const char *place = TextFormat("%u%s", rank, ordinal(rank));
        text_centered(place, 240, 208, 30, accent);
        text_centered(rank == 1U ? "GOLD" : rank == 2U ? "SILVER" :
                      rank == 3U ? "BRONZE" : "FINISHED", 240, 262, 10, accent);
        draw_text("TIME", 96, 282, 10, C_MUTED);
        draw_text(TextFormat("%02d:%04.1f", (int)(seconds / 60.0f), fmodf(seconds, 60.0f)),
                 96, 294, 20, RAYWHITE);
        draw_text("SCORE", 290, 282, 10, C_MUTED);
        draw_text(TextFormat("%06lu", (unsigned long)g->score), 290, 294, 20, RAYWHITE);
        const bool beat_rival = rank < g->opponents[g->rival].position;
        text_centered(TextFormat("%s RIVAL %s", beat_rival ? "BEAT" : "LOST TO",
                                 rally_rider_name(g->rival)), 240, 320, 10,
                      beat_rival ? C_MINT : C_RIDER[g->rival]);
        if ((g->tick >> 4) & 1U) text_centered("TAP TO RACE AGAIN", 240, 336, 10, C_MINT);
    } else if (g->phase == RALLY_PHASE_FAILED) {
        DrawRectangle(RALLY_LEFT, 0, RALLY_SPAN, RALLY_H, (Color){20, 2, 10, 110});
        DrawRectangleRounded((Rectangle){96, 168, 288, 120}, .14f, 6, (Color){4, 10, 31, 240});
        DrawRectangle(96, 178, 288, 3, C_PINK);
        text_centered("CRAFT DISABLED", 240, 194, 20, C_PINK);
        text_centered(TextFormat("SCORE %06lu  //  POS %u/%u", (unsigned long)g->score,
                                 (unsigned)g->position, (unsigned)(RALLY_OPPONENT_COUNT + 1U)),
                      240, 226, 10, RAYWHITE);
        if ((g->tick >> 4) & 1U) text_centered("TAP TO RETRY", 240, 256, 10, C_MINT);
    }
}

int rally_view_render(const rally_game_t *state, raylib_lite_atlas_t rally_art,
                      raylib_lite_atlas_t track_background,
                      raylib_lite_atlas_t motorcycle_art, unsigned course_id,
                      const rally_view_motion_t *motion)
{
    rally_game_t idle = {0};
    const rally_game_t *g = state ? state : &idle;
    s_course_id = course_id % 3U;
    s_horizon = 184;
    BeginDrawing();
    if (!raylib_lite_raylib_frame_available()) {
        EndDrawing();
        return 0;
    }
    s_view = raylib_lite_design_view((float)RALLY_H);
    s_margin = fmaxf(0.0f, raylib_lite_design_field_origin(s_view, RALLY_W, RALLY_H).x);
    raylib_lite_design_field_begin(s_view, RALLY_W, RALLY_H);
    prepare_camera(g);
    PROFILE_BEGIN();
    static const raylib_lite_asset_id_t skies[3] = {RAYLIB_LITE_ASSET_ID_TRACK_NEON_SKY,
                                                    RAYLIB_LITE_ASSET_ID_TRACK_SUNSET_SKY,
                                                    RAYLIB_LITE_ASSET_ID_TRACK_POLAR_SKY};
    const raylib_lite_sprite_frame_t *backdrop =
        raylib_lite_atlas_get_frame(track_background, skies[s_course_id]);
    if (backdrop) {
        Rectangle vista = backdrop->source;
        /* Water painted into the lower third has no relationship to track
         * coordinates. Retain the skyline, and let the land shelf supply the
         * foreground instead of presenting an unbridged sea/lake ahead. */
        if (s_course_id != 1U) vista.height *= .70f;
        /* Panorama parallax: scroll the skyline horizontally with the camera's
         * world heading so corners make the moon, mesas, and skyscrapers pan
         * across the windshield like a real 3D horizon. */
        float heading = atan2f(s_camera.forward_x, s_camera.forward_z);
        float shift = fmodf(-heading * (vista.width / 3.14159265f), vista.width);
        if (shift < 0.0f) shift += vista.width;
        float first_src = vista.width - shift;
        float first_dst = (float)RALLY_SPAN * (first_src / vista.width);
        if (first_dst >= (float)RALLY_SPAN - 0.5f) {
            DrawTexturePro(track_background.texture, vista,
                           (Rectangle){RALLY_LEFT, 0, RALLY_SPAN, s_horizon},
                           (Vector2){0, 0}, 0, WHITE);
        } else {
            if (first_dst >= 1.0f) {
                DrawTexturePro(track_background.texture,
                               (Rectangle){vista.x + shift, vista.y, first_src, vista.height},
                               (Rectangle){(float)RALLY_LEFT, 0, first_dst, (float)s_horizon},
                               (Vector2){0, 0}, 0, WHITE);
            }
            float second_dst = (float)RALLY_SPAN - first_dst;
            if (second_dst >= 1.0f && shift >= 1.0f) {
                DrawTexturePro(track_background.texture,
                               (Rectangle){vista.x, vista.y, shift, vista.height},
                               (Rectangle){(float)RALLY_LEFT + first_dst, 0, second_dst, (float)s_horizon},
                               (Vector2){0, 0}, 0, WHITE);
            }
        }
    } else
        draw_sky(g);
    PROFILE_PHASE(0);
    draw_ground(g);
    draw_track(g, false);
    draw_lane_markings(g);
    draw_corner_rail(g);
    PROFILE_PHASE(1);
    draw_world(g, rally_art, motorcycle_art);

    PROFILE_PHASE(2);

    draw_vehicle(g, motorcycle_art, motion);
    draw_collision_feedback(g);
    draw_speed_lines(g);
    PROFILE_PHASE(3);
    draw_hud(g, motion);
    draw_control_mode(g);
    draw_race_alert(g, motion);
    draw_start_banner(g, s_course_id);
    draw_result(g);
    PROFILE_PHASE(4);
    raylib_lite_design_field_end(s_view, RALLY_W, RALLY_H);
    EndDrawing();
    return 0;
}
