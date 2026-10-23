// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_raylib_impl.h"

#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib_lite_2d.h"
#include "raylib_lite_raylib_port.h"
#include "raylib_lite_rgb565.h"
#include "raylib_lite_primitives_internal.h"

static uint16_t *s_pixels;
static raylib_lite_primitive_surface_t s_primitive_surface;
static uint32_t s_primitive_pixels,s_primitive_runs,s_clear_pixels;
static size_t s_stride;
static Camera2D s_camera;
static bool s_camera_active;
static bool s_window_ready;
static bool s_window_should_close;
static int s_screen_width;
static int s_screen_height;
static int s_target_fps = 30;
static uint32_t s_logic_hz;
static uint64_t s_logic_ticks;
static uint64_t s_fps_window_start_us;
static uint32_t s_fps_window_frames;
static int s_measured_fps;
static raylib_lite_result_t s_last_acquire = RAYLIB_LITE_NOT_READY;
static raylib_lite_result_t s_last_present = RAYLIB_LITE_NOT_READY;
static bool s_scissor_active;
static int s_scissor_x0, s_scissor_y0, s_scissor_x1, s_scissor_y1;
/* Rows with (y & s_row_mask) set are never displayed by the video backend. */
static int s_row_mask;
#define RAYLIB_LITE_RAYLIB_KEY_COUNT 512
#define RAYLIB_LITE_RAYLIB_POINTER_COUNT 2
typedef struct {
    int track_id, x, y;
    bool active, pressed, released;
} fast_pointer_t;
static bool s_key_down[RAYLIB_LITE_RAYLIB_KEY_COUNT];
static bool s_key_pressed[RAYLIB_LITE_RAYLIB_KEY_COUNT];
static bool s_key_released[RAYLIB_LITE_RAYLIB_KEY_COUNT];
static fast_pointer_t s_pointers[RAYLIB_LITE_RAYLIB_POINTER_COUNT];
static Vector2 s_imu_xy;
static float s_imu_z;

Vector2 raylib_lite_raylib_get_world_to_screen2_d(Vector2 position, Camera2D camera)
{
    float zoom = camera.zoom == 0 ? 1.0f : camera.zoom;
    float radians = camera.rotation * (3.14159265358979323846f / 180.0f);
    float cs = cosf(radians), sn = sinf(radians);
    float x = (position.x - camera.target.x) * zoom;
    float y = (position.y - camera.target.y) * zoom;
    return (Vector2){camera.offset.x + x*cs - y*sn,
                     camera.offset.y + x*sn + y*cs};
}

Vector2 raylib_lite_raylib_get_screen_to_world2_d(Vector2 position, Camera2D camera)
{
    float zoom = camera.zoom == 0 ? 1.0f : camera.zoom;
    float radians = -camera.rotation * (3.14159265358979323846f / 180.0f);
    float cs = cosf(radians), sn = sinf(radians);
    float x = position.x - camera.offset.x;
    float y = position.y - camera.offset.y;
    return (Vector2){camera.target.x + (x*cs - y*sn)/zoom,
                     camera.target.y + (x*sn + y*cs)/zoom};
}

static Vector2 active_to_screen(Vector2 position)
{
    return s_camera_active ? raylib_lite_raylib_get_world_to_screen2_d(position, s_camera)
                           : position;
}

void raylib_lite_raylib_begin_mode2_d(Camera2D camera)
{
    s_camera = camera;
    if (s_camera.zoom == 0) s_camera.zoom = 1.0f;
    s_camera_active = true;
}

void raylib_lite_raylib_end_mode2_d(void) { s_camera_active = false; }

static inline uint16_t rgb565(Color c)
{
    return (uint16_t)(((uint16_t)(c.r & 0xf8U) << 8) |
                      ((uint16_t)(c.g & 0xfcU) << 3) | (c.b >> 3));
}

/* Shared screen-space paint and writer; camera handling stays in this facade. */
typedef raylib_lite_primitive_paint_t span_paint_t;
static span_paint_t span_paint(Color color)
{
    return raylib_lite_primitive_paint(
        (raylib_lite_renderer_color_t){color.r,color.g,color.b,color.a});
}
static void fill_span(int y, int x0, int x1, const span_paint_t *paint)
{
    raylib_lite_primitive_fill_span(&s_primitive_surface,y,x0,x1,paint);
}

static inline void put_pixel(int x, int y, Color color)
{
    if (!s_pixels || (unsigned)x >= (unsigned)s_screen_width ||
            (unsigned)y >= (unsigned)s_screen_height) return;
    if (s_scissor_active && (x < s_scissor_x0 || y < s_scissor_y0 ||
            x >= s_scissor_x1 || y >= s_scissor_y1)) return;
    if (y & s_row_mask) return;
    if(color.a){++s_primitive_pixels;++s_primitive_runs;}
    uint16_t *dst = &s_pixels[(size_t)y*s_stride + x];
    if (color.a == 255) {
        *dst = rgb565(color);
    } else if (color.a) {
        uint16_t old = *dst;
        unsigned a = color.a, ia = 255U - a;
        unsigned r = ((((old >> 11) & 31U) << 3)*ia + color.r*a)/255U;
        unsigned g = ((((old >> 5) & 63U) << 2)*ia + color.g*a)/255U;
        unsigned b = (((old & 31U) << 3)*ia + color.b*a)/255U;
        *dst = (uint16_t)(((r & 0xf8U) << 8) | ((g & 0xfcU) << 3) | (b >> 3));
    }
}

void raylib_lite_raylib_init_window(int width, int height, const char *title)
{
    (void)title;
    s_screen_width = width > 0 ? width : 0;
    s_screen_height = height > 0 ? height : 0;
    s_window_ready = true;
    s_window_should_close = false;
    s_logic_ticks = 0;
    s_fps_window_start_us = 0;
    s_fps_window_frames = 0;
    s_measured_fps = 0;
    s_last_acquire = RAYLIB_LITE_NOT_READY;
    s_last_present = RAYLIB_LITE_NOT_READY;
    memset(s_key_down, 0, sizeof(s_key_down));
    memset(s_key_pressed, 0, sizeof(s_key_pressed));
    memset(s_key_released, 0, sizeof(s_key_released));
    memset(s_pointers, 0, sizeof(s_pointers));
    s_imu_xy = (Vector2){0};
    s_imu_z = 0;
}

void raylib_lite_raylib_close_window(void)
{
    s_window_should_close = true;
    s_window_ready = false;
}

bool raylib_lite_raylib_window_should_close(void) { return s_window_should_close; }
bool raylib_lite_raylib_is_window_ready(void) { return s_window_ready; }
int raylib_lite_raylib_get_screen_width(void) { return s_screen_width; }
int raylib_lite_raylib_get_screen_height(void) { return s_screen_height; }
int raylib_lite_raylib_get_render_width(void) { return s_screen_width; }
int raylib_lite_raylib_get_render_height(void) { return s_screen_height; }
void raylib_lite_raylib_set_target_fps(int fps) { if (fps > 0) s_target_fps = fps; }
int raylib_lite_raylib_get_target_fps(void) { return s_target_fps; }

void raylib_lite_raylib_attach_runtime(uint32_t logic_hz) { s_logic_hz = logic_hz; }
void raylib_lite_raylib_detach_runtime(void) { s_logic_hz = 0; }

static uint32_t tick_hz(void)
{ return s_logic_hz ? s_logic_hz : (uint32_t)s_target_fps; }

/* Game time advances per logic tick, never per presented frame, so replays
 * stay deterministic when the display drops or rejects frames. */
float raylib_lite_raylib_get_frame_time(void) { return 1.0f / (float)tick_hz(); }
double raylib_lite_raylib_get_time(void)
{ return (double)s_logic_ticks / (double)tick_hz(); }
int raylib_lite_raylib_get_fps(void) { return s_measured_fps; }

static void note_presented_frame(void)
{
    uint64_t now = raylib_lite_raylib_port_now_us();
    if (!now) return;
    if (!s_fps_window_start_us) {
        s_fps_window_start_us = now;
        return;
    }
    ++s_fps_window_frames;
    uint64_t elapsed = now - s_fps_window_start_us;
    if (elapsed >= 1000000U) {
        s_measured_fps = (int)((s_fps_window_frames * 1000000ULL +
                                elapsed / 2U) / elapsed);
        s_fps_window_start_us = now;
        s_fps_window_frames = 0;
    }
}

static bool valid_key(int key)
{ return key >= 0 && key < RAYLIB_LITE_RAYLIB_KEY_COUNT; }

void raylib_lite_raylib_cancel_input(void)
{
    memset(s_key_down, 0, sizeof(s_key_down));
    memset(s_key_pressed, 0, sizeof(s_key_pressed));
    memset(s_key_released, 0, sizeof(s_key_released));
    memset(s_pointers, 0, sizeof(s_pointers));
    s_imu_xy = (Vector2){0};
    s_imu_z = 0;
}

void raylib_lite_raylib_inject_key(int key, bool pressed)
{
    if (!valid_key(key) || s_key_down[key] == pressed) return;
    s_key_down[key] = pressed;
    if (pressed) s_key_pressed[key] = true;
    else s_key_released[key] = true;
}

void raylib_lite_raylib_inject_action(int action, bool pressed)
{
    static const int primary[] = {KEY_LEFT, KEY_RIGHT, KEY_SPACE, KEY_P, KEY_ENTER};
    static const int alias[] = {KEY_A, KEY_D, KEY_SPACE, KEY_P, KEY_ENTER};
    if ((unsigned)action >= sizeof(primary)/sizeof(primary[0])) return;
    raylib_lite_raylib_inject_key(primary[action], pressed);
    raylib_lite_raylib_inject_key(alias[action], pressed);
}

void raylib_lite_raylib_inject_pointer(int track_id, int x, int y, bool pressed)
{
    fast_pointer_t *slot = NULL;
    for (int i = 0; i < RAYLIB_LITE_RAYLIB_POINTER_COUNT; ++i)
        if (s_pointers[i].active && s_pointers[i].track_id == track_id) slot = &s_pointers[i];
    if (!slot && pressed) for (int i = 0; i < RAYLIB_LITE_RAYLIB_POINTER_COUNT; ++i)
        if (!s_pointers[i].active) { slot = &s_pointers[i]; break; }
    if (!slot) return;
    slot->track_id = track_id; slot->x = x; slot->y = y;
    if (pressed && !slot->active) slot->pressed = true;
    if (!pressed && slot->active) slot->released = true;
    slot->active = pressed;
}

void raylib_lite_raylib_inject_imu(float x, float y, float z)
{ s_imu_xy = (Vector2){x, y}; s_imu_z = z; }

bool raylib_lite_raylib_is_key_pressed(int key) { return valid_key(key) && s_key_pressed[key]; }
bool raylib_lite_raylib_is_key_down(int key) { return valid_key(key) && s_key_down[key]; }
bool raylib_lite_raylib_is_key_released(int key) { return valid_key(key) && s_key_released[key]; }
bool raylib_lite_raylib_is_key_up(int key) { return !raylib_lite_raylib_is_key_down(key); }
bool raylib_lite_raylib_is_mouse_button_pressed(int button)
{ return button == MOUSE_BUTTON_LEFT && s_pointers[0].pressed; }
bool raylib_lite_raylib_is_mouse_button_down(int button)
{ return button == MOUSE_BUTTON_LEFT && s_pointers[0].active; }
bool raylib_lite_raylib_is_mouse_button_released(int button)
{ return button == MOUSE_BUTTON_LEFT && s_pointers[0].released; }
bool raylib_lite_raylib_is_mouse_button_up(int button)
{ return !raylib_lite_raylib_is_mouse_button_down(button); }
int raylib_lite_raylib_get_mouse_x(void) { return s_pointers[0].x; }
int raylib_lite_raylib_get_mouse_y(void) { return s_pointers[0].y; }
Vector2 raylib_lite_raylib_get_mouse_position(void)
{ return (Vector2){(float)s_pointers[0].x, (float)s_pointers[0].y}; }
int raylib_lite_raylib_get_touch_point_count(void)
{
    int count = 0;
    for (int i = 0; i < RAYLIB_LITE_RAYLIB_POINTER_COUNT; ++i) count += s_pointers[i].active;
    return count;
}
static fast_pointer_t *active_pointer(int index)
{
    for (int i = 0; i < RAYLIB_LITE_RAYLIB_POINTER_COUNT; ++i)
        if (s_pointers[i].active && index-- == 0) return &s_pointers[i];
    return NULL;
}
Vector2 raylib_lite_raylib_get_touch_position(int index)
{
    fast_pointer_t *pointer = active_pointer(index);
    return pointer ? (Vector2){(float)pointer->x, (float)pointer->y} : (Vector2){-1, -1};
}
int raylib_lite_raylib_get_touch_point_id(int index)
{
    fast_pointer_t *pointer = active_pointer(index);
    return pointer ? pointer->track_id : -1;
}
int raylib_lite_raylib_get_touch_x(void) { return (int)raylib_lite_raylib_get_touch_position(0).x; }
int raylib_lite_raylib_get_touch_y(void) { return (int)raylib_lite_raylib_get_touch_position(0).y; }
Vector3 raylib_lite_raylib_get_imu_acceleration(void)
{ return (Vector3){s_imu_xy.x, s_imu_xy.y, s_imu_z}; }

void raylib_lite_raylib_begin_drawing(void)
{
    s_pixels = NULL;
    s_stride = 0;
    s_last_acquire = raylib_lite_raylib_port_begin_frame(&s_pixels, &s_stride);
    s_last_present = RAYLIB_LITE_NOT_READY;
    uint16_t backend_width = 0, backend_height = 0;
    raylib_lite_raylib_port_get_dimensions(&backend_width, &backend_height);
    if (backend_width && backend_height) {
        s_screen_width = backend_width;
        s_screen_height = backend_height;
    }
    raylib_lite_renderer_set_target(s_pixels, s_stride, s_screen_width,
                               s_screen_height);
    const int row_step = raylib_lite_raylib_port_sample_row_step();
    s_row_mask = row_step - 1;
    raylib_lite_renderer_set_row_step((unsigned)row_step);
    raylib_lite_renderer_reset_raster_stats();
    s_primitive_pixels=s_primitive_runs=s_clear_pixels=0;
    s_primitive_surface=(raylib_lite_primitive_surface_t){
        .pixels=s_pixels,.stride=s_stride,.width=s_screen_width,.height=s_screen_height,
        .clip_active=s_scissor_active,
        .clip_x0=s_scissor_active?s_scissor_x0:0,
        .clip_y0=s_scissor_active?s_scissor_y0:0,
        .clip_x1=s_scissor_active?s_scissor_x1:s_screen_width,
        .clip_y1=s_scissor_active?s_scissor_y1:s_screen_height,
        .row_mask=s_row_mask};
}

bool raylib_lite_raylib_frame_available(void) { return s_pixels != NULL; }
raylib_lite_result_t raylib_lite_raylib_get_last_acquire_result(void)
{ return s_last_acquire; }
raylib_lite_result_t raylib_lite_raylib_get_last_present_result(void)
{ return s_last_present; }

uint16_t *raylib_lite_raylib_get_framebuffer(int *width, int *height,
                                    size_t *stride_pixels)
{
    if (width) *width = s_screen_width;
    if (height) *height = s_screen_height;
    if (stride_pixels) *stride_pixels = s_stride;
    return s_pixels;
}

void raylib_lite_raylib_consume_input_edges(void)
{
    memset(s_key_pressed, 0, sizeof(s_key_pressed));
    memset(s_key_released, 0, sizeof(s_key_released));
    for (int i = 0; i < RAYLIB_LITE_RAYLIB_POINTER_COUNT; ++i) {
        s_pointers[i].pressed = false;
        s_pointers[i].released = false;
    }
}

void raylib_lite_raylib_end_logic_tick(void)
{
    ++s_logic_ticks;
    raylib_lite_raylib_consume_input_edges();
}

void raylib_lite_raylib_end_drawing(void)
{
    if (s_pixels) {
        raylib_lite_renderer_note_primitives(s_primitive_pixels+s_primitive_surface.pixel_count,
            s_primitive_runs+s_primitive_surface.run_count,s_clear_pixels);
        s_last_present = raylib_lite_raylib_port_present_frame();
    }
    s_pixels = NULL;
    s_primitive_surface.pixels = NULL;
    s_stride = 0;
    raylib_lite_renderer_set_target(NULL, 0, 0, 0);
    s_camera_active = false;
    s_scissor_active = false;
    if (s_last_present == RAYLIB_LITE_OK) note_presented_frame();
    /* An attached runtime ends ticks itself; a render-only pass must not
     * drop edges that the next logic tick has not observed yet. */
    if (!s_logic_hz) raylib_lite_raylib_end_logic_tick();
}

void raylib_lite_raylib_begin_scissor_mode(int x, int y, int width, int height)
{
    s_scissor_x0 = x < 0 ? 0 : x;
    s_scissor_y0 = y < 0 ? 0 : y;
    s_scissor_x1 = x + width > s_screen_width ? s_screen_width : x + width;
    s_scissor_y1 = y + height > s_screen_height ? s_screen_height : y + height;
    s_scissor_active = width > 0 && height > 0 &&
        s_scissor_x0 < s_scissor_x1 && s_scissor_y0 < s_scissor_y1;
    s_primitive_surface.clip_active=s_scissor_active;
    s_primitive_surface.clip_x0=s_scissor_active?s_scissor_x0:0;
    s_primitive_surface.clip_y0=s_scissor_active?s_scissor_y0:0;
    s_primitive_surface.clip_x1=s_scissor_active?s_scissor_x1:s_screen_width;
    s_primitive_surface.clip_y1=s_scissor_active?s_scissor_y1:s_screen_height;
    raylib_lite_renderer_set_clip(s_scissor_x0, s_scissor_y0,
        s_scissor_x1 - s_scissor_x0, s_scissor_y1 - s_scissor_y0);
}

void raylib_lite_raylib_end_scissor_mode(void)
{
    s_scissor_active = false;
    s_primitive_surface.clip_active=false;
    s_primitive_surface.clip_x0=s_primitive_surface.clip_y0=0;
    s_primitive_surface.clip_x1=s_screen_width;s_primitive_surface.clip_y1=s_screen_height;
    raylib_lite_renderer_set_clip(0, 0, s_screen_width, s_screen_height);
}

Texture2D raylib_lite_raylib_load_texture(const char *asset_path)
{ return raylib_lite_2d_load_texture(asset_path); }
void raylib_lite_raylib_unload_texture(Texture2D texture)
{ raylib_lite_2d_unload_texture(texture); }
void raylib_lite_raylib_draw_texture_pro(Texture2D texture, Rectangle source,
                               Rectangle dest, Vector2 origin,
                               float rotation, Color tint)
{
    if (s_camera_active) {
        Vector2 screen = active_to_screen((Vector2){dest.x, dest.y});
        float zoom = s_camera.zoom;
        dest = (Rectangle){screen.x, screen.y, dest.width*zoom, dest.height*zoom};
        origin = (Vector2){origin.x*zoom, origin.y*zoom};
        rotation += s_camera.rotation;
    }
    raylib_lite_2d_draw_texture_pro(texture, source, dest, origin, rotation, tint);
}
void raylib_lite_raylib_draw_texture(Texture2D texture, int x, int y, Color tint)
{
    raylib_lite_raylib_draw_texture_pro(texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},
        (Rectangle){(float)x,(float)y,(float)texture.width,(float)texture.height},
        (Vector2){0,0},0,tint);
}
void raylib_lite_raylib_draw_texture_v(Texture2D texture, Vector2 position, Color tint)
{ raylib_lite_raylib_draw_texture(texture,(int)position.x,(int)position.y,tint); }
void raylib_lite_raylib_draw_texture_rec(Texture2D texture, Rectangle source,
                               Vector2 position, Color tint)
{
    raylib_lite_raylib_draw_texture_pro(texture,source,
        (Rectangle){position.x,position.y,fabsf(source.width),fabsf(source.height)},
        (Vector2){0,0},0,tint);
}

void raylib_lite_raylib_draw_texture_ex(Texture2D texture, Vector2 position,
                              float rotation, float scale, Color tint)
{
    raylib_lite_raylib_draw_texture_pro(texture,
        (Rectangle){0, 0, (float)texture.width, (float)texture.height},
        (Rectangle){position.x, position.y, texture.width*scale, texture.height*scale},
        (Vector2){0, 0}, rotation, tint);
}

void raylib_lite_raylib_clear_background(Color color)
{
    if (!s_pixels) return;
    s_clear_pixels+=(uint32_t)s_screen_width*(uint32_t)s_screen_height;
    uint16_t px = rgb565(color);
    if (s_stride == (size_t)s_screen_width && !s_row_mask) {
        raylib_lite_rgb565_fill(s_pixels, px,
            (size_t)s_screen_width * (size_t)s_screen_height);
        return;
    }
    for (int y = 0; y < s_screen_height; y += s_row_mask + 1)
        raylib_lite_rgb565_fill(s_pixels + (size_t)y * s_stride, px,
            (size_t)s_screen_width);
}

void raylib_lite_raylib_draw_pixel(int x, int y, Color color)
{
    Vector2 p = active_to_screen((Vector2){(float)x, (float)y});
    put_pixel((int)p.x, (int)p.y, color);
}

void raylib_lite_raylib_draw_pixel_v(Vector2 position, Color color)
{ raylib_lite_raylib_draw_pixel((int)position.x, (int)position.y, color); }

void raylib_lite_raylib_draw_rectangle(int x, int y, int width, int height, Color color)
{
    if (s_camera_active) {
        /* Snap both edges so zoomed tiles stay seamless and hairlines survive. */
        if (width <= 0 || height <= 0) return;
        Vector2 a = active_to_screen((Vector2){(float)x, (float)y});
        Vector2 b = active_to_screen((Vector2){(float)(x + width), (float)(y + height)});
        int x0 = (int)floorf(fminf(a.x, b.x)), x1 = (int)floorf(fmaxf(a.x, b.x));
        int y0 = (int)floorf(fminf(a.y, b.y)), y1 = (int)floorf(fmaxf(a.y, b.y));
        x = x0; y = y0;
        width = x1 > x0 ? x1 - x0 : 1; height = y1 > y0 ? y1 - y0 : 1;
    }
    raylib_lite_primitive_draw_rectangle(&s_primitive_surface,x,y,width,height,
        (raylib_lite_renderer_color_t){color.r,color.g,color.b,color.a});
}

void raylib_lite_raylib_draw_rectangle_v(Vector2 position,Vector2 size,Color color)
{ raylib_lite_raylib_draw_rectangle((int)position.x,(int)position.y,(int)size.x,(int)size.y,color); }

void raylib_lite_raylib_draw_rectangle_rec(Rectangle rectangle,Color color)
{ raylib_lite_raylib_draw_rectangle((int)rectangle.x,(int)rectangle.y,
    (int)rectangle.width,(int)rectangle.height,color); }

void raylib_lite_raylib_draw_rectangle_pro(Rectangle r,Vector2 origin,float rotation,Color color)
{
    float angle=rotation*0.01745329252f,cs=cosf(angle),sn=sinf(angle);
    Vector2 local[4]={{-origin.x,-origin.y},{r.width-origin.x,-origin.y},
        {r.width-origin.x,r.height-origin.y},{-origin.x,r.height-origin.y}};
    Vector2 point[4];
    for(int i=0;i<4;++i)point[i]=(Vector2){
        r.x+local[i].x*cs-local[i].y*sn,
        r.y+local[i].x*sn+local[i].y*cs};
    raylib_lite_raylib_draw_triangle(point[0],point[1],point[2],color);
    raylib_lite_raylib_draw_triangle(point[0],point[2],point[3],color);
}

static Color mix_color(Color a,Color b,unsigned value,unsigned limit)
{
    if(!limit)return a;
    unsigned inverse=limit-value;
    return(Color){
        (uint8_t)((a.r*inverse+b.r*value)/limit),
        (uint8_t)((a.g*inverse+b.g*value)/limit),
        (uint8_t)((a.b*inverse+b.b*value)/limit),
        (uint8_t)((a.a*inverse+b.a*value)/limit)};
}

void raylib_lite_raylib_draw_rectangle_gradient_v(int x,int y,int width,int height,
                                       Color top,Color bottom)
{
    if(height<=0)return;
    unsigned limit=(unsigned)(height>1?height-1:1);
    for(int row=0;row<height;++row)
        raylib_lite_raylib_draw_rectangle(x,y+row,width,1,mix_color(top,bottom,(unsigned)row,limit));
}

void raylib_lite_raylib_draw_rectangle_gradient_h(int x,int y,int width,int height,
                                       Color left,Color right)
{
    if(width<=0||height<=0)return;
    Vector2 start=active_to_screen((Vector2){(float)x,(float)y});
    int sx=(int)start.x,sy=(int)start.y;
    int sw=s_camera_active?(int)((float)width*s_camera.zoom):width;
    int sh=s_camera_active?(int)((float)height*s_camera.zoom):height;
    if(sw<=0||sh<=0)return;
    int y0=sy<0?0:sy,y1=sy+sh>s_screen_height?s_screen_height:sy+sh;
    if(s_scissor_active){
        if(y0<s_scissor_y0)y0=s_scissor_y0;
        if(y1>s_scissor_y1)y1=s_scissor_y1;
    }
    if(y0>=y1)return;
    unsigned limit=(unsigned)(width>1?width-1:1);
    for(int column=0;column<width;++column){
        int cx0=sx+(s_camera_active?(int)((float)column*s_camera.zoom):column);
        int cx1=sx+(s_camera_active?(int)((float)(column+1)*s_camera.zoom):(column+1));
        if(cx1<=cx0)cx1=cx0+1;
        if(cx0<0)cx0=0;
        if(cx1>s_screen_width)cx1=s_screen_width;
        if(s_scissor_active){
            if(cx0<s_scissor_x0)cx0=s_scissor_x0;
            if(cx1>s_scissor_x1)cx1=s_scissor_x1;
        }
        if(cx0>=cx1)continue;
        Color c=mix_color(left,right,(unsigned)column,limit);
        if(!c.a)continue;
        span_paint_t paint=span_paint(c);
        for(int row=y0;row<y1;++row)
            fill_span(row,cx0,cx1,&paint);
    }
}

void raylib_lite_raylib_draw_line(int x0, int y0, int x1, int y1, Color color)
{
    Vector2 a = active_to_screen((Vector2){(float)x0, (float)y0});
    Vector2 b = active_to_screen((Vector2){(float)x1, (float)y1});
    x0=(int)a.x; y0=(int)a.y; x1=(int)b.x; y1=(int)b.y;
    int dx=abs(x1-x0), sx=x0<x1?1:-1, dy=-abs(y1-y0), sy=y0<y1?1:-1;
    int error=dx+dy;
    for (;;) {
        put_pixel(x0,y0,color);
        if (x0==x1 && y0==y1) break;
        int twice=2*error;
        if (twice>=dy) { error+=dy; x0+=sx; }
        if (twice<=dx) { error+=dx; y0+=sy; }
    }
}

void raylib_lite_raylib_draw_line_v(Vector2 start, Vector2 end, Color color)
{ raylib_lite_raylib_draw_line((int)start.x, (int)start.y, (int)end.x, (int)end.y, color); }

void raylib_lite_raylib_draw_line_ex(Vector2 start, Vector2 end, float thick, Color color)
{
    start = active_to_screen(start);
    end = active_to_screen(end);
    if (s_camera_active) thick *= s_camera.zoom;
    if (thick <= 1.6f) {
        bool camera = s_camera_active;
        s_camera_active = false;
        raylib_lite_raylib_draw_line((int)start.x, (int)start.y, (int)end.x, (int)end.y, color);
        if (thick > 1.05f) {
            float dx=end.x-start.x, dy=end.y-start.y;
            if (fabsf(dx) >= fabsf(dy))
                raylib_lite_raylib_draw_line((int)start.x, (int)start.y+1, (int)end.x, (int)end.y+1, color);
            else
                raylib_lite_raylib_draw_line((int)start.x+1, (int)start.y, (int)end.x+1, (int)end.y, color);
        }
        s_camera_active = camera;
        return;
    }
    float dx=end.x-start.x,dy=end.y-start.y,length2=dx*dx+dy*dy;
    float radius=thick*.5f;
    int x0=(int)floorf(fminf(start.x,end.x)-radius);
    int x1=(int)ceilf(fmaxf(start.x,end.x)+radius);
    int y0=(int)floorf(fminf(start.y,end.y)-radius);
    int y1=(int)ceilf(fmaxf(start.y,end.y)+radius);
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
        float t=length2>0?(((x+.5f-start.x)*dx+(y+.5f-start.y)*dy)/length2):0;
        if(t<0)t=0;else if(t>1)t=1;
        float px=start.x+t*dx,py=start.y+t*dy,ox=x+.5f-px,oy=y+.5f-py;
        if(ox*ox+oy*oy<=radius*radius)put_pixel(x,y,color);
    }
}

void raylib_lite_raylib_draw_line_strip(const Vector2 *points,int count,Color color)
{ if(!points)return;for(int i=1;i<count;++i)raylib_lite_raylib_draw_line_v(points[i-1],points[i],color); }

void raylib_lite_raylib_draw_line_dashed(Vector2 start,Vector2 end,int dash,int space,Color color)
{
    float dx=end.x-start.x,dy=end.y-start.y,length=sqrtf(dx*dx+dy*dy);
    if(length<=0||dash<=0)return;
    if(space<0)space=0;
    float ux=dx/length,uy=dy/length,step=(float)(dash+space);
    for(float at=0;at<length;at+=step){
        float stop=fminf(at+dash,length);
        raylib_lite_raylib_draw_line_v((Vector2){start.x+ux*at,start.y+uy*at},
            (Vector2){start.x+ux*stop,start.y+uy*stop},color);
    }
}

void raylib_lite_raylib_draw_circle(int center_x, int center_y, float radius, Color color)
{
    Vector2 center=active_to_screen((Vector2){(float)center_x,(float)center_y});
    int r=(int)(radius*(s_camera_active?s_camera.zoom:1.0f));
    int rr=r*r;
    span_paint_t paint = span_paint(color);
    for(int y=-r;y<=r;++y){
        int span=(int)sqrtf((float)(rr-y*y));
        fill_span((int)center.y+y, (int)center.x-span, (int)center.x+span+1, &paint);
    }
}

void raylib_lite_raylib_draw_circle_v(Vector2 center,float radius,Color color)
{ raylib_lite_raylib_draw_circle((int)center.x,(int)center.y,radius,color); }

void raylib_lite_raylib_draw_circle_lines(int center_x,int center_y,float radius,Color color)
{
    Vector2 center=active_to_screen((Vector2){(float)center_x,(float)center_y});
    float zoom=s_camera_active?s_camera.zoom:1.0f;
    float r=radius*zoom;
    int outer=(int)ceilf(r+.6f),inner=(int)floorf(fmaxf(0,r-.6f));
    int outer2=outer*outer,inner2=inner*inner;
    for(int y=-outer;y<=outer;++y)for(int x=-outer;x<=outer;++x){
        int distance=x*x+y*y;
        if(distance<=outer2&&distance>=inner2)
            put_pixel((int)center.x+x,(int)center.y+y,color);
    }
}

void raylib_lite_raylib_draw_circle_lines_v(Vector2 center,float radius,Color color)
{ raylib_lite_raylib_draw_circle_lines((int)center.x,(int)center.y,radius,color); }

static void draw_ellipse(Vector2 center,float rh,float rv,Color color,bool outline)
{
    center=active_to_screen(center);
    float zoom=s_camera_active?s_camera.zoom:1.0f;
    rh=fabsf(rh*zoom);rv=fabsf(rv*zoom);
    if(rh<.5f||rv<.5f)return;
    int hy=(int)ceilf(rv);
    span_paint_t paint = span_paint(color);
    for(int y=-hy;y<=hy;++y){
        float ny=(y+.5f)/rv,remain=1-ny*ny;
        if(remain<0)continue;
        int span=(int)floorf(rh*sqrtf(remain));
        if(outline){
            put_pixel((int)center.x-span,(int)center.y+y,color);
            put_pixel((int)center.x+span,(int)center.y+y,color);
        }else fill_span((int)center.y+y, (int)center.x-span, (int)center.x+span+1, &paint);
    }
}

void raylib_lite_raylib_draw_ellipse(int x,int y,float rh,float rv,Color color)
{ draw_ellipse((Vector2){(float)x,(float)y},rh,rv,color,false); }
void raylib_lite_raylib_draw_ellipse_v(Vector2 center,float rh,float rv,Color color)
{ draw_ellipse(center,rh,rv,color,false); }
void raylib_lite_raylib_draw_ellipse_lines(int x,int y,float rh,float rv,Color color)
{ draw_ellipse((Vector2){(float)x,(float)y},rh,rv,color,true); }
void raylib_lite_raylib_draw_ellipse_lines_v(Vector2 center,float rh,float rv,Color color)
{ draw_ellipse(center,rh,rv,color,true); }

void raylib_lite_raylib_draw_rectangle_lines(int x, int y, int width, int height,
                                   Color color)
{
    raylib_lite_raylib_draw_rectangle(x, y, width, 1, color);
    raylib_lite_raylib_draw_rectangle(x, y + height - 1, width, 1, color);
    raylib_lite_raylib_draw_rectangle(x, y + 1, 1, height - 2, color);
    raylib_lite_raylib_draw_rectangle(x + width - 1, y + 1, 1, height - 2, color);
}

void raylib_lite_raylib_draw_rectangle_lines_ex(Rectangle r,float thick,Color color)
{
    int t=(int)ceilf(thick);if(t<1)t=1;
    raylib_lite_raylib_draw_rectangle((int)r.x,(int)r.y,(int)r.width,t,color);
    raylib_lite_raylib_draw_rectangle((int)r.x,(int)(r.y+r.height)-t,(int)r.width,t,color);
    raylib_lite_raylib_draw_rectangle((int)r.x,(int)r.y+t,t,(int)r.height-2*t,color);
    raylib_lite_raylib_draw_rectangle((int)(r.x+r.width)-t,(int)r.y+t,t,(int)r.height-2*t,color);
}

static float rounded_radius(Rectangle r,float roundness)
{
    if (roundness < 0) roundness = 0;
    if (roundness > 1) roundness = 1;
    return fminf(fabsf(r.width),fabsf(r.height))*roundness*.5f;
}

void raylib_lite_raylib_draw_rectangle_rounded(Rectangle r,float roundness,int segments,
                                     Color color)
{
    (void)segments;
    float radius=rounded_radius(r,roundness);
    if(radius<1){raylib_lite_raylib_draw_rectangle_rec(r,color);return;}
    if(!color.a)return;
    int x=(int)r.x,y=(int)r.y,width=(int)r.width,height=(int)r.height;
    if(s_camera_active){
        Vector2 p=active_to_screen((Vector2){(float)x,(float)y});
        x=(int)p.x;y=(int)p.y;
        width=(int)(width*s_camera.zoom);height=(int)(height*s_camera.zoom);
        radius*=s_camera.zoom;
    }
    /* One span per row, so translucent fills blend each pixel once. The
     * covered set is two cross rectangles plus four corner circles, clipped
     * to the rectangle (corner circles can overshoot when a side is ~2*radius). */
    int rad=(int)ceilf(radius),cr=(int)radius,rr=cr*cr;
    int left_cx=x+rad,right_cx=x+width-rad-1;
    int top_cy=y+rad,bottom_cy=y+height-rad-1;
    span_paint_t paint=span_paint(color);
    for(int row=y;row<y+height;++row){
        if(row&s_row_mask)continue;
        int lo[4],hi[4],count=0,span=-1;
        if(height>2*rad&&row>=y+rad&&row<y+height-rad){lo[0]=x;hi[0]=x+width;count=1;}
        else{
            if(width>2*rad){lo[count]=x+rad;hi[count++]=x+width-rad;}
            const int centers[2]={top_cy,bottom_cy};
            for(int i=0;i<2;++i){
                int dy=row-centers[i];
                if(dy>=-cr&&dy<=cr){
                    int s=(int)sqrtf((float)(rr-dy*dy));
                    if(s>span)span=s;
                }
            }
            if(span>=0){
                lo[count]=left_cx-span;hi[count++]=left_cx+span+1;
                lo[count]=right_cx-span;hi[count++]=right_cx+span+1;
            }
        }
        for(int i=1;i<count;++i)for(int j=i;j>0&&lo[j]<lo[j-1];--j){
            int t=lo[j];lo[j]=lo[j-1];lo[j-1]=t;t=hi[j];hi[j]=hi[j-1];hi[j-1]=t;
        }
        for(int i=0;i<count;){
            int first=lo[i],last=hi[i++];
            while(i<count&&lo[i]<=last){if(hi[i]>last)last=hi[i];++i;}
            if(first<x)first=x;
            if(last>x+width)last=x+width;
            if(first<last)raylib_lite_primitive_draw_span(&s_primitive_surface,row,first,last,&paint);
        }
    }
}

void raylib_lite_raylib_draw_rectangle_rounded_lines(Rectangle r,float roundness,int segments,
                                          Color color)
{
    float radius=rounded_radius(r,roundness);
    if(radius<1){raylib_lite_raylib_draw_rectangle_lines((int)r.x,(int)r.y,(int)r.width,(int)r.height,color);return;}
    int rad=(int)ceilf(radius),steps=segments>0?segments/4:6;
    if(steps<2)steps=2;
    float left=r.x,right=r.x+r.width-1,top=r.y,bottom=r.y+r.height-1;
    raylib_lite_raylib_draw_line_v((Vector2){left+rad,top},(Vector2){right-rad,top},color);
    raylib_lite_raylib_draw_line_v((Vector2){right,top+rad},(Vector2){right,bottom-rad},color);
    raylib_lite_raylib_draw_line_v((Vector2){right-rad,bottom},(Vector2){left+rad,bottom},color);
    raylib_lite_raylib_draw_line_v((Vector2){left,bottom-rad},(Vector2){left,top+rad},color);
    const Vector2 centers[4]={{left+rad,top+rad},{right-rad,top+rad},
        {right-rad,bottom-rad},{left+rad,bottom-rad}};
    const float starts[4]={180,270,0,90};
    for(int corner=0;corner<4;++corner){
        float first=starts[corner]*0.01745329252f;
        Vector2 previous={centers[corner].x+cosf(first)*radius,
                          centers[corner].y+sinf(first)*radius};
        for(int i=1;i<=steps;++i){
            float angle=(starts[corner]+90.0f*(float)i/(float)steps)*0.01745329252f;
            Vector2 next={centers[corner].x+cosf(angle)*radius,
                          centers[corner].y+sinf(angle)*radius};
            raylib_lite_raylib_draw_line_v(previous,next,color);
            previous=next;
        }
    }
}

void raylib_lite_raylib_draw_triangle(Vector2 a, Vector2 b, Vector2 c, Color color)
{
    a=active_to_screen(a);b=active_to_screen(b);c=active_to_screen(c);
    raylib_lite_primitive_draw_triangle(&s_primitive_surface,
        (int)a.x,(int)a.y,(int)b.x,(int)b.y,(int)c.x,(int)c.y,
        (raylib_lite_renderer_color_t){color.r,color.g,color.b,color.a});
}

void raylib_lite_raylib_draw_triangle_lines(Vector2 a,Vector2 b,Vector2 c,Color color)
{
    raylib_lite_raylib_draw_line_v(a,b,color);
    raylib_lite_raylib_draw_line_v(b,c,color);
    raylib_lite_raylib_draw_line_v(c,a,color);
}

void raylib_lite_raylib_draw_triangle_fan(const Vector2 *points,int count,Color color)
{
    if(!points)return;
    for(int i=1;i+1<count;++i)
        raylib_lite_raylib_draw_triangle(points[0],points[i],points[i+1],color);
}

void raylib_lite_raylib_draw_triangle_strip(const Vector2 *points,int count,Color color)
{
    if(!points)return;
    for(int i=0;i+2<count;++i)
        raylib_lite_raylib_draw_triangle(points[i],points[i+1],points[i+2],color);
}

static Vector2 polygon_point(Vector2 center,int sides,float radius,
                             float rotation,int index)
{
    float angle=(rotation+360.0f*(float)index/(float)sides)*0.01745329252f;
    return(Vector2){center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
}

void raylib_lite_raylib_draw_poly(Vector2 center,int sides,float radius,float rotation,
                         Color color)
{
    if(sides<3)return;
    for(int i=0;i<sides;++i)
        raylib_lite_raylib_draw_triangle(center,polygon_point(center,sides,radius,rotation,i),
            polygon_point(center,sides,radius,rotation,i+1),color);
}

void raylib_lite_raylib_draw_poly_lines_ex(Vector2 center,int sides,float radius,
                                float rotation,float thick,Color color)
{
    if(sides<3)return;
    for(int i=0;i<sides;++i)
        raylib_lite_raylib_draw_line_ex(polygon_point(center,sides,radius,rotation,i),
            polygon_point(center,sides,radius,rotation,i+1),thick,color);
}

void raylib_lite_raylib_draw_poly_lines(Vector2 center,int sides,float radius,
                              float rotation,Color color)
{ raylib_lite_raylib_draw_poly_lines_ex(center,sides,radius,rotation,1,color); }

#include "raylib_lite_default_font.h"

/* Decode UTF-8 without reading past NUL; unsupported codepoints use '?'. */
static unsigned text_codepoint(const char **text)
{
    const unsigned char *p=(const unsigned char *)*text;
    unsigned cp=*p++, count=0, minimum=0;
    if(cp>=0xc2&&cp<=0xdf){cp&=31;count=1;minimum=0x80;}
    else if(cp>=0xe0&&cp<=0xef){cp&=15;count=2;minimum=0x800;}
    else if(cp>=0xf0&&cp<=0xf4){cp&=7;count=3;minimum=0x10000;}
    else if(cp>=0x80){++*text;return '?';}
    for(unsigned i=0;i<count;++i){
        if((*p&0xc0)!=0x80){++*text;return '?';}
        cp=(cp<<6)|(*p++&63);
    }
    if(count&&(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))){
        ++*text;return '?';
    }
    *text=(const char *)p;
    return cp;
}

static unsigned text_glyph(unsigned cp)
{ return cp>=32&&cp<=255?cp-32:'?'-32; }

/* ceil(value / 10), including negative coordinates, without floating point. */
static int text_ceil_tenths(int64_t value)
{ return (int)(value>=0 ? (value+9)/10 : value/10); }

void raylib_lite_raylib_draw_text(const char *text, int x, int y, int font_size,
                         Color color)
{
    if(!text||!color.a)return;
    if(font_size<10)font_size=10;
    double scale=font_size/10.0, spacing=font_size/10;
    double base_x=x,base_y=y,offset_x=0,offset_y=0;
    int64_t offset_tenths=0;
    if(s_camera_active){
        Vector2 p=active_to_screen((Vector2){base_x,base_y});
        base_x=p.x;base_y=p.y;scale*=s_camera.zoom;spacing*=s_camera.zoom;
    }
    if(scale<=0)return;
    span_paint_t paint=span_paint(color);
    while(*text){
        unsigned cp=text_codepoint(&text),g=text_glyph(cp);
        if(cp=='\n'){offset_x=0;offset_tenths=0;offset_y+=font_size+2;continue;}
        double left=base_x+offset_x,top=base_y+offset_y*(s_camera_active?s_camera.zoom:1);
        int width=compat_font_width[g];
        int64_t left_tenths=(int64_t)x*10+offset_tenths;
        bool integer_edges=!s_camera_active && font_size<=512 &&
            left_tenths>=-1000000000 && left_tenths<=1000000000;
        int top_tenths=(int)lround(top*10);
        float inverse_scale=(float)(1.0/scale);
        if(cp!=' '&&cp!='\t'){
            int x0=integer_edges?text_ceil_tenths(left_tenths-5):(int)ceil(left-.5-1e-9);
            int x1=integer_edges?text_ceil_tenths(left_tenths+width*font_size-5):
                (int)ceil(left+width*scale-.5-1e-9);
            int y0=(int)ceil(top-.5-1e-9),y1=(int)ceil(top+10*scale-.5-1e-9);
            /* Clip before sampling; a large off-screen glyph costs no work. */
            if(x0<0)x0=0;
            if(x1>s_screen_width)x1=s_screen_width;
            if(y0<0)y0=0;
            if(y1>s_screen_height)y1=s_screen_height;
            if(s_scissor_active){
                if(x0<s_scissor_x0)x0=s_scissor_x0;
                if(x1>s_scissor_x1)x1=s_scissor_x1;
                if(y0<s_scissor_y0)y0=s_scissor_y0;
                if(y1>s_scissor_y1)y1=s_scissor_y1;
            }
            if(x0>=x1||y0>=y1){
                offset_x+=width*scale+spacing;
                offset_tenths+=(int64_t)width*font_size+(font_size/10)*10;
                continue;
            }
            /* Without a camera, pen advances are exact tenths of a pixel.
             * Integer ceil preserves the atlas sample boundaries. Retain
             * the floating path for camera zoom and unusually large text.
             * The default atlas is at most nine columns wide. */
            int edges[10];
            for(int col=0;col<=width;++col){
                int edge=integer_edges?text_ceil_tenths(left_tenths+col*font_size-5):
                    (int)ceil(left+col*scale-.5-1e-9);
                if(edge<x0)edge=x0;
                if(edge>x1)edge=x1;
                edges[col]=edge;
            }
            int cached_sy=-1,run_count=0;
            int run_start[5],run_end[5];
            for(int row=y0;row<y1;++row){
                if(row&s_row_mask)continue;
                int sy=s_camera_active?(int)((row+.5f-(float)top)*inverse_scale):
                    ((2*row+1)*5-top_tenths)/font_size;
                if((unsigned)sy>=10)continue;
                if(sy!=cached_sy){
                    int first=-1;
                    run_count=0;cached_sy=sy;
                    /* A scaled source row may cover several target rows.
                     * Decode its ink runs once; preserve each row write and
                     * blend so translucent pixels are still touched once. */
                    for(int col=0;col<=width;++col){
                        bool ink=col<width&&(compat_font_rows[g][sy]&(1U<<col));
                        if(ink&&first<0)first=col;
                        if(!ink&&first>=0){
                            int start=edges[first];
                            int end=edges[col];
                            run_start[run_count]=start;run_end[run_count++]=end;first=-1;
                        }
                    }
                }
                for(int run=0;run<run_count;++run)
                    fill_span(row,run_start[run],run_end[run],&paint);
            }
        }
        offset_x+=width*scale+spacing;
        offset_tenths+=(int64_t)width*font_size+(font_size/10)*10;
    }
}

int raylib_lite_raylib_measure_text(const char *text, int font_size)
{
    if(!text||!*text)return 0;
    if(font_size<10)font_size=10;
    int width=0,max_width=0,count=0,max_count=0;
    while(*text){
        unsigned cp=text_codepoint(&text);++count;
        if(cp=='\n'){
            if(width>max_width)max_width=width;
            width=0;count=0;
        }else width+=compat_font_width[text_glyph(cp)];
        if(count>max_count)max_count=count;
    }
    if(width>max_width)max_width=width;
    return (int)(max_width*(font_size/10.0f)+(max_count-1)*(font_size/10));
}

static bool bmfont_field(const char *line, const char *key, int *out)
{
    const char *p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p || v < -65536 || v > 0x10ffff) return false;
    *out = (int)v;
    return true;
}

static int bmfont_find_codepoint(Font font, int cp)
{
    int lo = 0, hi = font.glyphCount;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        int val = font.glyphs[mid].value;
        if (val == cp) return mid;
        if (val < cp) lo = mid + 1;
        else hi = mid;
    }
    for (int i = 0; i < font.glyphCount; ++i) {
        if (font.glyphs[i].value == cp) return i;
    }
    return -1;
}

static int bmfont_glyph_index(Font font, int cp)
{
    int idx = bmfont_find_codepoint(font, cp);
    if (idx >= 0) return idx;
    idx = bmfont_find_codepoint(font, '?');
    return idx >= 0 ? idx : 0;
}

Font raylib_lite_raylib_load_font(const char *asset_path)
{
    Font font = {0};
    if (!asset_path || !*asset_path) return font;
    char fnt_name[128], atlas_name[128];
    size_t len = strlen(asset_path);
    if (len + 7 >= sizeof(fnt_name)) return font;
    memcpy(fnt_name, asset_path, len + 1);
    const char *dot = strrchr(fnt_name, '.');
    size_t stem = dot ? (size_t)(dot - fnt_name) : len;
    if (!dot) {
        memcpy(fnt_name + stem, ".fnt", 5);
    }
    memcpy(atlas_name, fnt_name, stem);
    memcpy(atlas_name + stem, ".atlas", 7);

    raylib_lite_asset_view_t view = {0};
    if (raylib_lite_asset_open(fnt_name, &view) != RAYLIB_LITE_OK || !view.data || !view.size) {
        raylib_lite_asset_release(&view);
        return font;
    }
    char *text = malloc(view.size + 1);
    if (!text) {
        raylib_lite_asset_release(&view);
        return font;
    }
    memcpy(text, view.data, view.size);
    text[view.size] = '\0';
    raylib_lite_asset_release(&view);

    int line_height = 0, scale_w = 0, scale_h = 0, pages = 1, count = 0;
    char *cursor = text;
    while (*cursor) {
        char *line = cursor;
        char *nl = strchr(cursor, '\n');
        if (nl) { *nl = '\0'; cursor = nl + 1; }
        else cursor += strlen(cursor);
        if (strncmp(line, "common ", 7) == 0) {
            (void)bmfont_field(line, "lineHeight=", &line_height);
            (void)bmfont_field(line, "scaleW=", &scale_w);
            (void)bmfont_field(line, "scaleH=", &scale_h);
            (void)bmfont_field(line, "pages=", &pages);
        } else if (strncmp(line, "chars ", 6) == 0) {
            (void)bmfont_field(line, "count=", &count);
            break;
        }
    }
    if (line_height <= 0 || line_height > 4096 || pages != 1 || count <= 0 || count > 65535) {
        free(text);
        return font;
    }
    Texture2D tex = raylib_lite_2d_load_texture(atlas_name);
    if (!tex.id || (scale_w && tex.width != scale_w) || (scale_h && tex.height != scale_h)) {
        if (tex.id) raylib_lite_2d_unload_texture(tex);
        free(text);
        return font;
    }
    GlyphInfo *glyphs = calloc((size_t)count, sizeof(GlyphInfo));
    Rectangle *recs = calloc((size_t)count, sizeof(Rectangle));
    if (!glyphs || !recs) {
        free(glyphs);
        free(recs);
        raylib_lite_2d_unload_texture(tex);
        free(text);
        return font;
    }
    int parsed = 0;
    while (*cursor && parsed < count) {
        char *line = cursor;
        char *nl = strchr(cursor, '\n');
        if (nl) { *nl = '\0'; cursor = nl + 1; }
        else cursor += strlen(cursor);
        if (strncmp(line, "char ", 5) != 0) continue;
        int id = 0, x = 0, y = 0, w = 0, h = 0, ox = 0, oy = 0, adv = 0, page = 0;
        if (!bmfont_field(line, "id=", &id) || !bmfont_field(line, "x=", &x) ||
            !bmfont_field(line, "y=", &y) || !bmfont_field(line, "width=", &w) ||
            !bmfont_field(line, "height=", &h) || !bmfont_field(line, "xoffset=", &ox) ||
            !bmfont_field(line, "yoffset=", &oy) || !bmfont_field(line, "xadvance=", &adv) ||
            (bmfont_field(line, "page=", &page) && page != 0) ||
            x < 0 || y < 0 || w < 0 || h < 0 || x + w > tex.width || y + h > tex.height) {
            free(glyphs);
            free(recs);
            raylib_lite_2d_unload_texture(tex);
            free(text);
            return (Font){0};
        }
        recs[parsed] = (Rectangle){(float)x, (float)y, (float)w, (float)h};
        glyphs[parsed] = (GlyphInfo){
            .value = id, .offsetX = ox, .offsetY = oy, .advanceX = adv,
        };
        ++parsed;
    }
    free(text);
    if (parsed != count) {
        free(glyphs);
        free(recs);
        raylib_lite_2d_unload_texture(tex);
        return (Font){0};
    }
    /* Sort by codepoint if unordered so binary search always works. */
    for (int i = 1; i < count; ++i) {
        GlyphInfo g = glyphs[i];
        Rectangle r = recs[i];
        int j = i;
        while (j > 0 && glyphs[j - 1].value > g.value) {
            glyphs[j] = glyphs[j - 1];
            recs[j] = recs[j - 1];
            --j;
        }
        glyphs[j] = g;
        recs[j] = r;
    }
    font.baseSize = line_height;
    font.glyphCount = count;
    font.glyphPadding = 0;
    font.texture = tex;
    font.recs = recs;
    font.glyphs = glyphs;
    return font;
}

void raylib_lite_raylib_unload_font(Font font)
{
    if (font.texture.id) raylib_lite_2d_unload_texture(font.texture);
    free(font.glyphs);
    free(font.recs);
}

void raylib_lite_raylib_draw_text_ex(Font font, const char *text, Vector2 position,
                                     float font_size, float spacing, Color tint)
{
    if (!text || !tint.a) return;
    if (!font.texture.id || !font.glyphs || !font.recs || font.glyphCount <= 0 || font.baseSize <= 0) {
        raylib_lite_raylib_draw_text(text, (int)position.x, (int)position.y,
                                     (int)(font_size > 0 ? font_size : 10), tint);
        return;
    }
    float effective_size = font_size > 0 ? font_size : (float)font.baseSize;
    float scale = effective_size / (float)font.baseSize;
    float ox = 0.0f, oy = 0.0f;
    while (*text) {
        if (*text == '\r') {
            ++text;
            if (*text == '\n') ++text;
            ox = 0.0f;
            oy += effective_size + 2.0f;
            continue;
        }
        unsigned cp = text_codepoint(&text);
        if (cp == '\n') {
            ox = 0.0f;
            oy += effective_size + 2.0f;
            continue;
        }
        int idx = bmfont_glyph_index(font, (int)cp);
        Rectangle src = font.recs[idx];
        GlyphInfo g = font.glyphs[idx];
        if (cp != ' ' && cp != '\t' && src.width > 0 && src.height > 0) {
            Rectangle dst = {
                position.x + ox + (float)g.offsetX * scale,
                position.y + oy + (float)g.offsetY * scale,
                src.width * scale,
                src.height * scale,
            };
            /* Snap 1:1 unrotated glyphs in screen space to integer pixels so the
             * renderer's M2D_ATLAS_BINARY_ALPHA fast path runs without scaling. */
            float zoom = s_camera_active ? s_camera.zoom : 1.0f;
            float rot = s_camera_active ? s_camera.rotation : 0.0f;
            if (fabsf(scale * zoom - 1.0f) < 0.001f && fabsf(rot) < 0.001f) {
                if (s_camera_active) {
                    Vector2 sp = active_to_screen((Vector2){dst.x, dst.y});
                    raylib_lite_2d_draw_texture_pro(
                        font.texture, src,
                        (Rectangle){floorf(sp.x + 0.5f), floorf(sp.y + 0.5f),
                                    src.width, src.height},
                        (Vector2){0, 0}, 0.0f, tint);
                } else {
                    dst.x = floorf(dst.x + 0.5f);
                    dst.y = floorf(dst.y + 0.5f);
                    raylib_lite_raylib_draw_texture_pro(font.texture, src, dst,
                                                        (Vector2){0, 0}, 0.0f, tint);
                }
            } else {
                raylib_lite_raylib_draw_texture_pro(font.texture, src, dst,
                                                    (Vector2){0, 0}, 0.0f, tint);
            }
        }
        float adv = g.advanceX > 0 ? (float)g.advanceX : (src.width + (float)g.offsetX);
        ox += adv * scale + spacing;
    }
}

Vector2 raylib_lite_raylib_measure_text_ex(Font font, const char *text,
                                           float font_size, float spacing)
{
    if (!text || !*text) return (Vector2){0, 0};
    if (!font.texture.id || !font.glyphs || !font.recs || font.glyphCount <= 0 || font.baseSize <= 0) {
        int sz = (int)(font_size >= 10 ? font_size : 10);
        float h = (float)sz;
        for (const char *p = text; *p; ++p) {
            if (*p == '\n') h += (float)sz + 2.0f;
        }
        return (Vector2){(float)raylib_lite_raylib_measure_text(text, sz), h};
    }
    float effective_size = font_size > 0 ? font_size : (float)font.baseSize;
    float scale = effective_size / (float)font.baseSize;
    float width = 0.0f, max_width = 0.0f, height = effective_size;
    int chars = 0, max_chars = 0;
    while (*text) {
        if (*text == '\r') {
            ++text;
            if (*text == '\n') ++text;
            if (width > max_width) max_width = width;
            if (chars > max_chars) max_chars = chars;
            width = 0.0f;
            chars = 0;
            height += effective_size + 2.0f;
            continue;
        }
        unsigned cp = text_codepoint(&text);
        if (cp == '\n') {
            if (width > max_width) max_width = width;
            if (chars > max_chars) max_chars = chars;
            width = 0.0f;
            chars = 0;
            height += effective_size + 2.0f;
            continue;
        }
        int idx = bmfont_glyph_index(font, (int)cp);
        GlyphInfo g = font.glyphs[idx];
        Rectangle src = font.recs[idx];
        width += g.advanceX > 0 ? (float)g.advanceX : (src.width + (float)g.offsetX);
        ++chars;
    }
    if (width > max_width) max_width = width;
    if (chars > max_chars) max_chars = chars;
    float total_w = max_width * scale + (max_chars > 0 ? (float)(max_chars - 1) * spacing : 0.0f);
    return (Vector2){total_w, height};
}

const char *raylib_lite_raylib_text_format_v(const char *format, va_list args)
{
    /* MAX_TEXTFORMAT_BUFFERS and MAX_TEXT_BUFFER_LENGTH from third_party/raylib config.h. */
    static char buffers[4][512];
    static unsigned index;
    char *out = buffers[index];
    memset(out, 0, sizeof(buffers[0]));
    if(format){
        int required=vsnprintf(out, sizeof(buffers[0]), format, args);
        if(required >= (int)sizeof(buffers[0]))
            memcpy(out+sizeof(buffers[0])-4, "...", 4);
        index=(index+1)&3U;
    }
    return out;
}

const char *raylib_lite_raylib_text_format(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const char *out = raylib_lite_raylib_text_format_v(format, args);
    va_end(args);
    return out;
}

/* Collision and color helpers must return exactly what raylib 6.0
 * rshapes.c/rtextures.c return, including edge and rounding behaviour. */
bool raylib_lite_raylib_check_collision_recs(Rectangle a, Rectangle b)
{
    return a.x < b.x+b.width && a.x+a.width > b.x &&
           a.y < b.y+b.height && a.y+a.height > b.y;
}

bool raylib_lite_raylib_check_collision_circles(Vector2 a,float ar,Vector2 b,float br)
{
    float dx=a.x-b.x,dy=a.y-b.y,r=ar+br;
    return dx*dx+dy*dy <= r*r;
}

bool raylib_lite_raylib_check_collision_point_rec(Vector2 p,Rectangle r)
{
    return p.x>=r.x && p.x<r.x+r.width && p.y>=r.y && p.y<r.y+r.height;
}

bool raylib_lite_raylib_check_collision_circle_rec(Vector2 center,float radius,Rectangle r)
{
    float hw=r.width/2.0f,hh=r.height/2.0f;
    float dx=fabsf(center.x-(r.x+hw)),dy=fabsf(center.y-(r.y+hh));
    if(dx>hw+radius||dy>hh+radius)return false;
    if(dx<=hw||dy<=hh)return true;
    return (dx-hw)*(dx-hw)+(dy-hh)*(dy-hh)<=radius*radius;
}

bool raylib_lite_raylib_check_collision_point_circle(Vector2 point,Vector2 center,float radius)
{
    float dx=point.x-center.x,dy=point.y-center.y;
    return dx*dx+dy*dy<=radius*radius;
}

bool raylib_lite_raylib_check_collision_point_triangle(Vector2 p,Vector2 a,Vector2 b,Vector2 c)
{
    float denominator=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
    float alpha=((b.y-c.y)*(p.x-c.x)+(c.x-b.x)*(p.y-c.y))/denominator;
    float beta=((c.y-a.y)*(p.x-c.x)+(a.x-c.x)*(p.y-c.y))/denominator;
    float gamma=1.0f-alpha-beta;
    return alpha>0&&beta>0&&gamma>0;
}

Rectangle raylib_lite_raylib_get_collision_rec(Rectangle a,Rectangle b)
{
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    float right=fminf(a.x+a.width,b.x+b.width);
    float bottom=fminf(a.y+a.height,b.y+b.height);
    if(right<=x||bottom<=y)return(Rectangle){0};
    return(Rectangle){x,y,right-x,bottom-y};
}

Color raylib_lite_raylib_color_alpha(Color color,float alpha)
{
    if (alpha < 0) alpha = 0;
    else if (alpha > 1) alpha = 1;
    color.a=(uint8_t)(255.0f*alpha);
    return color;
}

Color raylib_lite_raylib_fade(Color color,float alpha)
{ return raylib_lite_raylib_color_alpha(color,alpha); }

Color raylib_lite_raylib_color_tint(Color color,Color tint)
{
    return(Color){(uint8_t)(color.r*tint.r/255U),(uint8_t)(color.g*tint.g/255U),
        (uint8_t)(color.b*tint.b/255U),(uint8_t)(color.a*tint.a/255U)};
}

Color raylib_lite_raylib_color_brightness(Color color,float factor)
{
    if (factor > 1) factor = 1;
    else if (factor < -1) factor = -1;
    float r=color.r,g=color.g,b=color.b;
    if (factor < 0) {
        factor += 1.0f;
        r*=factor; g*=factor; b*=factor;
    } else {
        r+=(255-r)*factor; g+=(255-g)*factor; b+=(255-b)*factor;
    }
    color.r=(uint8_t)r;
    color.g=(uint8_t)g;
    color.b=(uint8_t)b;
    return color;
}
