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
    /* Asphalt stays a clear step lighter than the verge so the ribbon reads
     * before the bike. Accents stay fully saturated for the curbs. */
    if (s_course_id == 0U) {
        return (rally_theme_t){
            {3, 8, 31, 255}, {59, 78, 112, 255}, {34, 46, 65, 255}, {10, 21, 34, 255}, C_CYAN,
            C_PINK};
    }
    if (s_course_id == 1U) {
        return (rally_theme_t){{22, 11, 35, 255},
                               {218, 111, 100, 255},
                               {81, 65, 73, 255},
                               {71, 36, 46, 255},
                               C_GOLD,
                               C_PINK};
    }
    return (rally_theme_t){{3, 17, 37, 255},
                           {157, 209, 225, 255},
                           {34, 61, 85, 255},
                           {63, 102, 130, 255},
                           C_MINT,
                           C_CYAN};
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
    /* Chase height leaves the next bend and both curbs above the bike. */
    s_camera.y = cam.y + 2.05f + g->height * 0.16f;
    s_camera.z = cam.z - cam.tangent_z * 1.40f;
    float roll = -cam.bank * (g->course_id == 0U ? .14f : .42f);
    s_camera.roll_cos = cosf(roll);
    s_camera.roll_sin = sinf(roll);
    float impact = g->collision_ticks ?
        clampf(g->impact_speed / RALLY_MAX_SPEED, .3f, 1.0f) : 0.0f;
    impact *= clampf(g->collision_ticks / 18.0f, 0, 1);
    s_camera.focal = RALLY_FOCAL - 12.0f * clampf(g->speed / RALLY_MAX_SPEED, 0, 1.35f);
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
    /* Clear the lower half exactly once. Road segments then cover only their
     * own trapezoids; this avoids the old per-segment full-width overdraw. */
    DrawRectangle(RALLY_LEFT, s_horizon, RALLY_SPAN, RALLY_H - s_horizon, theme.terrain);
    /* The vista contains water (harbor/lake) or a painted canyon floor, not
     * the driving surface. A distant land shelf joins it to the world-space
     * verge. Keep harbor water at the sides of the causeway, never directly
     * underneath the road's vanishing point. Opaque bands avoid alpha cost. */
    Color distant_land = color_lerp(theme.sky_bottom, theme.terrain, .65f);
    DrawTriangle((Vector2){-80, s_horizon}, (Vector2){95, s_horizon - 23},
                 (Vector2){290, s_horizon}, distant_land);
    DrawTriangle((Vector2){100, s_horizon}, (Vector2){270, s_horizon - 16},
                 (Vector2){430, s_horizon}, distant_land);
    DrawTriangle((Vector2){285, s_horizon}, (Vector2){435, s_horizon - 25},
                 (Vector2){560, s_horizon}, distant_land);
    for (int y = s_horizon - 8; y < s_horizon; ++y) {
        float t = (float)(y - (s_horizon - 8)) / 7.0f;
        Color land = color_lerp(theme.sky_bottom, theme.terrain, .65f + .35f * t);
        int inset = g->course_id == 0U ? (int)(150.0f * (1.0f - t)) : RALLY_LEFT;
        DrawRectangle(inset, y, RALLY_W - inset * 2, 1, land);
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
    rally_track_pose_t p;
    rally_track_sample_course(0, g->progress + distance, side * (warehouse ? 10.8f : 9.2f), &p);
    float w = warehouse ? 4.0f : 3.0f, depth = warehouse ? 4.5f : 2.5f;
    float h = warehouse ? 4.8f : 2.5f, clip = world_clip(fmaxf(0, distance - depth * 5.0f));
    rally_point2_t a = local_project(&p, -w, -depth, 0), b = local_project(&p, w, -depth, 0);
    rally_point2_t c = local_project(&p, -w, -depth, h), d = local_project(&p, w, -depth, h);
    rally_point2_t e = local_project(&p, side * w, depth, 0),
                   f = local_project(&p, side * w, depth, h);
    rally_point2_t back = local_project(&p, -side * w, depth, h);
    float minx = fminf(fminf(a.x, b.x), e.x), maxx = fmaxf(fmaxf(a.x, b.x), e.x);
    if (maxx < RALLY_LEFT || minx > RALLY_RIGHT)
        return;
    Color front = warehouse ? (Color){60, 75, 88, 255} : (Color){48, 85, 103, 255};
    world_quad(front, a, b, c, d, clip);
    world_quad(color_lerp(front, BLACK, .32f), side < 0 ? a : b, e, side < 0 ? c : d, f, clip);
    world_quad(color_lerp(front, RAYWHITE, .20f), c, d, side < 0 ? f : back, side < 0 ? back : f,
               clip);
    /* Inset door/panel and a physical light band, tied to the object face. */
    float door = warehouse ? 1.4f : .8f;
    world_quad((Color){20, 34, 46, 255}, local_project(&p, -door, -depth - .01f, 0),
               local_project(&p, door, -depth - .01f, 0),
               local_project(&p, -door, -depth - .01f, h * .72f),
               local_project(&p, door, -depth - .01f, h * .72f), clip);
    world_quad(warehouse ? C_GOLD : C_CYAN, local_project(&p, -w, -depth - .02f, h * .82f),
               local_project(&p, w, -depth - .02f, h * .82f),
               local_project(&p, -w, -depth - .02f, h * .88f),
               local_project(&p, w, -depth - .02f, h * .88f), clip);
}

static void harbor_post(const rally_game_t *g, float distance, float side)
{
    rally_track_pose_t p;
    rally_track_sample_course(0, g->progress + distance, side * 6.8f, &p);
    float clip = world_clip(distance);
    world_quad((Color){100, 128, 146, 255}, local_project(&p, -.09f, 0, 0),
               local_project(&p, .09f, 0, 0), local_project(&p, -.09f, 0, 2.6f),
               local_project(&p, .09f, 0, 2.6f), clip);
    world_quad(C_GOLD, local_project(&p, -.35f, 0, 2.55f), local_project(&p, .35f, 0, 2.55f),
               local_project(&p, -.35f, 0, 2.7f), local_project(&p, .35f, 0, 2.7f), clip);
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
        Color face = snow ? (Color){104, 150, 180, 255} : (Color){123, 71, 62, 255};
        Color cap = snow ? (Color){211, 230, 237, 255} : (Color){185, 118, 86, 255};
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
        track_offset(near, inner, height * .70f, &p);
        a = project(g, p);
        track_offset(far, inner, height * .70f, &p);
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
        Color stone = {96, 51, 53, 255}, light = {195, 132, 96, 255};
        world_quad(stone, local_project(&p, -.65f, 0, 0), local_project(&p, .65f, 0, 0),
                   local_project(&p, -.35f, 0, 3.6f), local_project(&p, .35f, 0, 3.6f), clip);
        world_quad(light, local_project(&p, -.65f, 0, 2.4f), local_project(&p, .65f, 0, 2.4f),
                   local_project(&p, -.35f, 0, 2.7f), local_project(&p, .35f, 0, 2.7f), clip);
    } else {
        Color wood = {64, 89, 106, 255};
        world_quad(wood, local_project(&p, -.10f, 0, 0), local_project(&p, .10f, 0, 0),
                   local_project(&p, -.10f, 0, fence ? 2.1f : 1.7f),
                   local_project(&p, .10f, 0, fence ? 2.1f : 1.7f), clip);
        if (fence) {
            for (int i = 0; i < 2; ++i) {
                float h = .9f + i * .7f;
                world_quad((Color){124, 151, 163, 255}, local_project(&p, 0, -1.5f, h),
                           local_project(&p, 0, 1.5f, h), local_project(&p, 0, -1.5f, h + .15f),
                           local_project(&p, 0, 1.5f, h + .15f), clip);
            }
        } else {
            world_quad(C_GOLD, local_project(&p, -.10f, 0, 1.4f), local_project(&p, .10f, 0, 1.4f),
                       local_project(&p, -.10f, 0, 1.7f), local_project(&p, .10f, 0, 1.7f), clip);
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
        /* Keep the asphalt continuous; roadside and lane markings carry the
         * speed cue and advance in world distance rather than segment index. */
        Color asphalt =
            color_lerp(theme.road, theme.sky_bottom, .38f * s_segment_z[i + stride] / RALLY_FAR_Z);
        /* A finite road mesh must disappear into its surrounding land rather
         * than terminate in a contrasting horizontal asphalt edge. This only
         * affects the distant tail; nearby bends and lane widths stay intact. */
        float distance_fade = clampf((s_segment_z[i + stride] - 45.0f) / 25.0f, 0, 1);
        asphalt = color_lerp(asphalt, theme.terrain, distance_fade);
        draw_quad(asphalt, l0, r0, l1, r1);

        /* A metre-wide opaque curb. A one-pixel line disappears once RGB565
         * quantizes the dark asphalt and the verge to the same value. */
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
        Color left_curb = g->course_id == 0   ? (Color){126, 171, 181, 255}
                  : g->course_id == 1 ? (Color){209, 171, 127, 255}
                                      : (Color){207, 225, 234, 255};
        Color right_curb = g->course_id == 0   ? (Color){193, 199, 188, 255}
                  : g->course_id == 1 ? (Color){153, 104, 85, 255}
                                      : (Color){144, 178, 196, 255};
        draw_quad(color_lerp(left_curb, theme.terrain, distance_fade),
                  ll0, lr0, ll1, lr1);
        draw_quad(color_lerp(right_curb, theme.terrain, distance_fade),
                  rl0, rr0, rl1, rr1);

        if (g->course_id == 0U) {
            float progress = fmodf(g->progress + s_segment_z[i] * 1.05f, RALLY_TRACK_LENGTH);
            bool bridge = progress >= 500 && progress < 800;
            float height = bridge ? .65f : .35f;
            Color wall = bridge ? (Color){91, 111, 128, 255} : (Color){49, 67, 81, 255};
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
    const Color dash = {236, 240, 246, 255};
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
        DrawTriangle((Vector2){cx, y - 4}, (Vector2){cx - 4, y}, (Vector2){cx, y + 4}, color);
        DrawTriangle((Vector2){cx, y - 4}, (Vector2){cx, y + 4}, (Vector2){cx + 4, y}, color);
        if (size >= 30) {
            const char *name = rally_rider_name(index);
            const int width = measure_text(name, 10);
            DrawRectangle((int)cx - width / 2 - 3, (int)y - 19, width + 6, 12, (Color){4, 9, 29, 200});
            draw_text(name, (int)cx - width / 2, (int)y - 18, 10, color);
        }
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
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(art, pose);
    if (!frame)
        return;
    if (center.y + size * .06f < clip)
        DrawEllipse((int)center.x, (int)center.y + 1, size * .21f, size * .06f,
                    (Color){4, 10, 20, 255});
    Rectangle source = frame->source;
    float height = fminf(size, clip - top);
    source.height *= height / size;
    DrawTexturePro(art.texture, source,
                   (Rectangle){center.x - size * .5f, top, size, height},
                   (Vector2){0, 0}, 0, WHITE);
    /* Helmet and race bib identify each rider without per-texel tinting.
     * Both follow the authored lean and the same hill cut as the texture. */
    if (size >= 18) {
        float offset = clampf(lean, -1, 1) * size * .10f;
        float bib_y = top + size * .45f;
        int bib_h = (int)fminf(size * .10f, clip - bib_y);
        if (bib_h > 0)
            DrawRectangle((int)(center.x + offset - size * .12f), (int)bib_y,
                          (int)(size * .24f) + 1, bib_h, color);
        float helmet_y = top + size * .22f;
        int helmet_h = (int)fminf(size * .04f, clip - helmet_y);
        if (helmet_h > 0)
            DrawRectangle((int)(center.x + offset - size * .07f), (int)helmet_y,
                          (int)(size * .14f) + 1, helmet_h, color);
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
        float first = floorf(g->progress / 18) * 18;
        for (int i = 0; i < 5; ++i) {
            float at = first + i * 18, dist = at - g->progress;
            if (dist <= RALLY_NEAR_Z || dist >= RALLY_FAR_Z)
                continue;
            items[count++] = (item_t){dist, WORLD_LAMP, 0, ((int)(at / 18) & 1) ? -1 : 1, 0};
        }
        first = floorf(g->progress / 48) * 48;
        for (int i = 0; i < 3; ++i) {
            float at = first + i * 48, dist = at - g->progress;
            if (dist <= RALLY_NEAR_Z || dist >= RALLY_FAR_Z)
                continue;
            float wrapped = fmodf(at, RALLY_TRACK_LENGTH);
            if (wrapped >= 500 && wrapped < 800)
                continue;
            items[count++] =
                (item_t){dist, wrapped >= 300 && wrapped < 500 ? WORLD_WAREHOUSE : WORLD_CONTAINER,
                         0, -1, 0};
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
    float impact = clampf(g->impact_speed / RALLY_MAX_SPEED, .3f, 1.0f);
    Color flash = {255, 112, 125,
                   (unsigned char)(strength * (5.0f + 5.0f * impact))};
    DrawRectangle(RALLY_LEFT, 68, RALLY_SPAN, 5, flash);
    DrawRectangle(RALLY_LEFT, RALLY_H - 11, RALLY_SPAN, 11, flash);
    DrawRectangle(RALLY_LEFT, 68, 7, RALLY_H - 79, flash);
    DrawRectangle(RALLY_RIGHT - 7, 68, 7, RALLY_H - 79, flash);
    int kick = (strength & 1U) ? 9 : -9;
    DrawLine(240 + kick, 335, 201 + kick, 314, C_PINK);
    DrawLine(240 + kick, 335, 279 + kick, 314, C_GOLD);
    if (strength > 4U) {
        DrawRectangleRounded((Rectangle){176, 160, 128, 30}, .5f, 6, (Color){30, 5, 20, 220});
        DrawRectangle(184, 168, 3, 14, C_PINK);
        draw_text("CRASH -12", 196, 166, 20, RAYWHITE);
    }
}

static void draw_vehicle(const rally_game_t *g, raylib_lite_atlas_t art,
                         const rally_view_motion_t *motion)
{
    int pose_direction = motion ? motion->pose : 0;
    float x = 240.0f + g->lateral * 7.0f;
    float bob = g->grounded ? sinf((float)g->tick * .30f) *
                clampf(g->speed / RALLY_MAX_SPEED, 0.0f, 1.0f) * 1.2f : 0.0f;
    float ground_y = 448.0f - g->height * 8.0f + bob;
    if (g->collision_ticks) x += (g->collision_ticks & 1U) ? 4.0f : -4.0f;
    float shadow_y = 451.0f + bob;
    float shadow_size = 20.0f / (1 + g->height * .2f);
    DrawEllipse((int)x, (int)shadow_y, shadow_size, 5, (Color){3, 8, 17, 255});
    if (g->drifting || g->drift_boost_ticks) {
        for (int i = 0; i < 3; ++i) {
            int phase = (int)((g->tick + i * 3) % 9);
            int side = g->lateral_velocity < 0 ? -1 : 1;
            DrawRectangle((int)x + side * (12 + i * 4), (int)ground_y + phase, 3, 2,
                          i & 1 ? RAYWHITE : C_GOLD);
        }
    }
    if (g->nitro_active) {
        int trail = 29 + (int)(g->speed * .5f);
        DrawLineEx((Vector2){x - 17, ground_y - 7},
                   (Vector2){x - 23, ground_y + trail}, 5, C_CYAN);
        DrawLineEx((Vector2){x + 17, ground_y - 7},
                   (Vector2){x + 23, ground_y + trail}, 5, C_PINK);
    }
    raylib_lite_asset_id_t pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_STRAIGHT;
    /* Pose IDs use the player's screen-space steering direction. */
    if (pose_direction <= -2) pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_LEFT;
    else if (pose_direction == -1) pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_LEFT;
    else if (pose_direction == 1) pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_LIGHT_RIGHT;
    else if (pose_direction >= 2) pose = RAYLIB_LITE_ASSET_ID_MOTORCYCLE_HEAVY_RIGHT;
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(art, pose);
    if (frame) {
        /* Authored poses carry the lean; fractional rotation would invoke
         * the slow affine sampler and introduce subpixel shimmer. */
        float tail_lag = g->drifting ? -g->lateral_velocity * .65f : 0.0f;
        const float sprite = 112.0f;
        DrawTexturePro(art.texture, frame->source,
                       (Rectangle){x + tail_lag, ground_y - 4.0f, sprite, sprite},
                       (Vector2){sprite * 0.5f, sprite * 0.90f}, 0, WHITE);
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
                   g->nitro_active ? 2.0f : 1.0f, color);
    }
    if (g->nitro_active) {
        unsigned char pulse = (unsigned char)(70 + 40 * ((g->tick >> 1) & 1U));
        DrawRectangleGradientH(RALLY_LEFT, 70, 30, RALLY_H - 70, (Color){44, 235, 255, pulse},
                               (Color){44, 235, 255, 0});
        DrawRectangleGradientH(RALLY_RIGHT - 30, 70, 30, RALLY_H - 70,
                               (Color){255, 62, 181, 0}, (Color){255, 62, 181, pulse});
    }
}

static void draw_position(const rally_game_t *g)
{
    unsigned rank = g->position ? g->position : RALLY_OPPONENT_COUNT + 1U;
    Color color = rank_color(rank);
    DrawRectangleRounded((Rectangle){10, 10, 108, 62}, .22f, 4, C_PANEL);
    DrawRectangle(10, 18, 3, 46, color);
    const char *digit = TextFormat("%u", rank);
    draw_text(digit, 22, 14, 40, color);
    int x = 22 + measure_text(digit, 40) + 4;
    draw_text(ordinal(rank), x, 18, 10, color);
    draw_text(TextFormat("OF %u", RALLY_OPPONENT_COUNT + 1U), x, 34, 10, C_MUTED);
    /* Hull integrity under the rank; it turns from mint to pink as it drains. */
    float hull = clampf((float)g->integrity / 100.0f, 0.0f, 1.0f);
    DrawRectangle(22, 60, 86, 4, (Color){20, 32, 56, 255});
    DrawRectangle(22, 60, (int)(86.0f * hull), 4, color_lerp(C_PINK, C_MINT, hull));
}

static float race_distance_of(float progress, unsigned laps)
{
    return (float)laps * RALLY_TRACK_LENGTH + progress;
}

/* The rival's name, place and gap sit under the player's position. */
static void draw_rival(const rally_game_t *g)
{
    if (g->phase != RALLY_PHASE_PLAYING) return;
    const rally_opponent_t *rival = &g->opponents[g->rival];
    const Color color = C_RIDER[g->rival];
    const float gap = race_distance_of(rival->progress, rival->laps_completed) -
                      race_distance_of(g->progress, g->laps_completed);
    const char *gap_text = TextFormat("%s%dm", gap >= 0 ? "+" : "-", (int)fabsf(gap));
    if (!raylib_lite_design_view_native(s_view)) {
        /* Scaled text: name over place and gap. */
        DrawRectangleRounded((Rectangle){10, 76, 108, 50}, .3f, 4, C_PANEL);
        DrawRectangle(10, 82, 3, 38, color);
        draw_text(rally_rider_name(g->rival), 20, 80, 10, color);
        draw_text(TextFormat("P%u %s", (unsigned)rival->position, gap_text), 20, 102, 10,
                  gap >= 0 ? C_PINK : C_MINT);
        return;
    }
    DrawRectangleRounded((Rectangle){10, 76, 108, 30}, .3f, 4, C_PANEL);
    DrawRectangle(10, 82, 3, 18, color);
    draw_text("RIVAL", 20, 80, 10, C_MUTED);
    draw_text(rally_rider_name(g->rival), 20, 92, 10, color);
    const char *place = TextFormat("P%u", (unsigned)rival->position);
    draw_text(place, 110 - measure_text(place, 10), 80, 10, RAYWHITE);
    draw_text(gap_text, 110 - measure_text(gap_text, 10), 92, 10, gap >= 0 ? C_PINK : C_MINT);
}

static void draw_lap(const rally_game_t *g)
{
    unsigned lap = g->laps_completed + 1U;
    if (lap > RALLY_TARGET_LAPS) lap = RALLY_TARGET_LAPS;
    DrawRectangleRounded((Rectangle){172, 10, 136, 46}, .3f, 4, C_PANEL);
    const bool final_lap = lap == RALLY_TARGET_LAPS;
    text_centered(TextFormat("LAP %u/%u", lap, RALLY_TARGET_LAPS), 240, 16, 20,
                  final_lap ? C_GOLD : RAYWHITE);
    float lap_progress = clampf(g->progress / RALLY_TRACK_LENGTH, 0.0f, 1.0f);
    DrawRectangle(184, 42, 112, 4, (Color){20, 32, 56, 255});
    DrawRectangle(184, 42, (int)(112.0f * lap_progress), 4, final_lap ? C_GOLD : C_CYAN);
    for (unsigned i = 1; i < RALLY_CHECKPOINT_COUNT; ++i)
        DrawRectangle(184 + (int)(112U * i / RALLY_CHECKPOINT_COUNT), 40, 1, 8, C_MUTED);
}

static void draw_score(const rally_game_t *g, const rally_view_motion_t *motion)
{
    uint32_t score = motion ? motion->shown_score : g->score;
    DrawRectangleRounded((Rectangle){362, 10, 108, 62}, .22f, 4, C_PANEL);
    const bool combo_on = g->combo > 1U;
    const bool scaled = !raylib_lite_design_view_native(s_view);
    /* Scaled text has room for one label: the combo replaces SCORE. */
    if (!scaled || !combo_on) draw_text("SCORE", 374, 16, 10, C_MUTED);
    draw_text(TextFormat("%06lu", (unsigned long)score), 374, scaled ? 34 : 28, 20, RAYWHITE);
    if (combo_on) {
        float left = clampf((float)g->combo_timer / 90.0f, 0.0f, 1.0f);
        const char *combo = TextFormat(scaled ? "COMBO x%u" : "x%u", (unsigned)g->combo);
        draw_text(combo, scaled ? 374 : 458 - measure_text(combo, 10), 16, 10, C_GOLD);
        DrawRectangle(374, 56, (int)(84.0f * left), 4, C_GOLD);
    }
}

static void draw_speedometer(const rally_game_t *g)
{
    const float ratio = speed_ratio(g);
    const unsigned kmh = (unsigned)(g->speed * 6.0f + .5f);
    const int segments = 14;
    const int lit = (int)(ratio * (float)segments + .5f);
    for (int i = 0; i < segments; ++i) {
        const int h = 4 + i;
        const float t = (float)i / (float)(segments - 1);
        Color color = t < .55f ? C_MINT : (t < .85f ? C_GOLD : C_PINK);
        if (g->nitro_active) color = (i + (int)g->tick) & 1 ? C_CYAN : C_PINK;
        if (i >= lit) color = (Color){20, 32, 56, 220};
        DrawRectangle(18 + i * 9, 418 - h, 6, h, color);
    }
    const char *readout = TextFormat("%u", kmh);
    draw_text(readout, 18, 426, 30, g->nitro_active ? C_CYAN : RAYWHITE);
    draw_text("KM/H", 24 + measure_text(readout, 30), 444, 10, C_MUTED);
}

/* The nitro touch zone (x >= 300, y >= 310) gets a visible button whose
 * ring of pips shows the remaining charge. */
static void draw_nitro_button(const rally_game_t *g)
{
    const float cx = 420.0f, cy = 412.0f;
    const float charge = clampf(g->nitro / RALLY_NITRO_MAX, 0.0f, 1.0f);
    const bool ready = charge >= .2f;
    Color ring = g->nitro_active ? C_GOLD : (ready ? C_CYAN : C_MUTED);
    DrawCircle((int)cx, (int)cy, 40, (Color){4, 9, 29, 170});
    DrawCircleLines((int)cx, (int)cy, 40, Fade(ring, .55f));
    if (g->nitro_active) DrawCircleLines((int)cx, (int)cy, 44 + (int)(g->tick % 6U), Fade(C_GOLD, .5f));
    const unsigned pips = 16U;
    for (unsigned i = 0; i < pips; ++i) {
        float a = -1.5708f + 6.2832f * (float)i / (float)pips;
        bool on = (float)i < charge * (float)pips;
        DrawCircle((int)(cx + cosf(a) * 33.0f), (int)(cy + sinf(a) * 33.0f), 3,
                   on ? ring : (Color){26, 40, 68, 255});
    }
    text_centered("N2O", (int)cx, (int)cy - 10, 10, ring);
    text_centered(TextFormat("%u", (unsigned)(charge * 100.0f + .5f)), (int)cx, (int)cy + 2, 10,
                  RAYWHITE);
    if (g->draft_meter > 1.0f || g->draft_boost_ticks) {
        float draft = g->draft_boost_ticks ? 1.0f : clampf(g->draft_meter / 100.0f, 0.0f, 1.0f);
        draw_text("SLIP", 374, 352, 10, C_MUTED);
        DrawRectangle(400, 354, 66, 4, (Color){20, 32, 56, 255});
        DrawRectangle(400, 354, (int)(66.0f * draft), 4,
                      g->draft_boost_ticks ? C_MINT : C_CYAN);
    }
}

static void draw_hud(const rally_game_t *g, const rally_view_motion_t *motion)
{
    const bool driving = g->phase == RALLY_PHASE_PLAYING || g->phase == RALLY_PHASE_COUNTDOWN;
    draw_lap(g);
    pin_hud(-s_margin);
    draw_position(g);
    draw_rival(g);
    if (driving) draw_speedometer(g);
    pin_hud(s_margin);
    draw_score(g, motion);
    if (driving) draw_nitro_button(g);
    pin_hud(0);
}

static void draw_control_mode(const rally_game_t *game)
{
    const char *mode=game->control_mode==RALLY_CONTROL_TOUCH?"TOUCH":
        game->control_mode==RALLY_CONTROL_TILT?"TILT":"KEYS";
    DrawRectangle(12,70,146,40,C_PANEL);
    DrawRectangleLines(12,70,146,40,C_CYAN);
    text_centered(TextFormat("%s >",mode),85,82,14,C_CYAN);
    if(game->control_mode==RALLY_CONTROL_TILT){
        DrawRectangle(338,70,130,40,C_PANEL);
        DrawRectangleLines(338,70,130,40,C_GOLD);
        text_centered("CENTER",403,82,14,C_GOLD);
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
    const int width = measure_text(text, 20) + 28;
    DrawRectangleRounded((Rectangle){240.0f - width / 2.0f, (float)y, (float)width, 30},
                         .5f, 6, Fade(C_PANEL, alpha));
    DrawRectangle(240 - width / 2 + 8, y + 8, 3, 14, Fade(accent, alpha));
    text_centered(text, 244, y + 6, 20, Fade(accent, alpha));
}

static void draw_race_alert(const rally_game_t *g, const rally_view_motion_t *motion)
{
    if (g->phase != RALLY_PHASE_PLAYING) return;
    if (g->offtrack) {
        if ((g->tick >> 2) & 1U) chip("OFF TRACK", 118, C_PINK, 1.0f);
        return;
    } else if (motion && motion->alert_ticks && motion->callout != RALLY_CALLOUT_LAP &&
               motion->callout != RALLY_CALLOUT_FINAL_LAP) {
        const float alpha = clampf((float)motion->alert_ticks / 10.0f, 0.0f, 1.0f);
        const int y = 118;
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
        DrawRectangle(240 - half, 128, half * 2, 46, Fade(C_PANEL, alpha));
        DrawRectangle(240 - half, 128, half * 2, 2, Fade(accent, alpha));
        DrawRectangle(240 - half, 172, half * 2, 2, Fade(accent, alpha));
        if (grow >= 1.0f)
            text_centered(final_lap ? "FINAL LAP" : TextFormat("LAP %lu", (unsigned long)motion->callout_value),
                          240, 138, 30, Fade(accent, alpha));
        return;
    }
    const int y = 118;
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
        DrawTexturePro(track_background.texture, vista,
                       (Rectangle){RALLY_LEFT, 0, RALLY_SPAN, s_horizon}, (Vector2){0, 0}, 0, WHITE);
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
