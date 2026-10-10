// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Raylib-compatible 2D API backed by a platform-provided RGB565 surface. Keep the
 * mapping explicit: unsupported upstream APIs fail at link time instead of
 * silently pulling a software OpenGL renderer into device firmware. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib.h"
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

void raylib_lite_raylib_init_window(int width, int height, const char *title);
void raylib_lite_raylib_close_window(void);
bool raylib_lite_raylib_window_should_close(void);
bool raylib_lite_raylib_is_window_ready(void);
int raylib_lite_raylib_get_screen_width(void);
int raylib_lite_raylib_get_screen_height(void);
int raylib_lite_raylib_get_render_width(void);
int raylib_lite_raylib_get_render_height(void);
void raylib_lite_raylib_set_target_fps(int fps);
float raylib_lite_raylib_get_frame_time(void);
double raylib_lite_raylib_get_time(void);
int raylib_lite_raylib_get_fps(void);

bool raylib_lite_raylib_is_key_pressed(int key);
bool raylib_lite_raylib_is_key_down(int key);
bool raylib_lite_raylib_is_key_released(int key);
bool raylib_lite_raylib_is_key_up(int key);
bool raylib_lite_raylib_is_mouse_button_pressed(int button);
bool raylib_lite_raylib_is_mouse_button_down(int button);
bool raylib_lite_raylib_is_mouse_button_released(int button);
bool raylib_lite_raylib_is_mouse_button_up(int button);
int raylib_lite_raylib_get_mouse_x(void);
int raylib_lite_raylib_get_mouse_y(void);
Vector2 raylib_lite_raylib_get_mouse_position(void);
int raylib_lite_raylib_get_touch_x(void);
int raylib_lite_raylib_get_touch_y(void);
Vector2 raylib_lite_raylib_get_touch_position(int index);
int raylib_lite_raylib_get_touch_point_id(int index);
int raylib_lite_raylib_get_touch_point_count(void);
Vector3 raylib_lite_raylib_get_imu_acceleration(void);

/* Platform adapters feed the same state queried through the Raylib API. */
void raylib_lite_raylib_inject_key(int key, bool pressed);
void raylib_lite_raylib_inject_action(int action, bool pressed);
void raylib_lite_raylib_inject_pointer(int track_id, int x, int y, bool pressed);
void raylib_lite_raylib_inject_imu(float x, float y, float z);

void raylib_lite_raylib_begin_drawing(void);
bool raylib_lite_raylib_frame_available(void);
void raylib_lite_raylib_end_drawing(void);
raylib_lite_result_t raylib_lite_raylib_get_last_acquire_result(void);
raylib_lite_result_t raylib_lite_raylib_get_last_present_result(void);
/* Consume one-shot edges after a logic tick; held input remains active. */
void raylib_lite_raylib_consume_input_edges(void);
void raylib_lite_raylib_begin_mode2_d(Camera2D camera);
void raylib_lite_raylib_end_mode2_d(void);
Vector2 raylib_lite_raylib_get_world_to_screen2_d(Vector2 position, Camera2D camera);
Vector2 raylib_lite_raylib_get_screen_to_world2_d(Vector2 position, Camera2D camera);
void raylib_lite_raylib_begin_scissor_mode(int x, int y, int width, int height);
void raylib_lite_raylib_end_scissor_mode(void);

void raylib_lite_raylib_clear_background(Color color);
void raylib_lite_raylib_draw_pixel(int x, int y, Color color);
void raylib_lite_raylib_draw_pixel_v(Vector2 position, Color color);
void raylib_lite_raylib_draw_line(int start_x, int start_y, int end_x, int end_y, Color color);
void raylib_lite_raylib_draw_line_v(Vector2 start, Vector2 end, Color color);
void raylib_lite_raylib_draw_line_ex(Vector2 start, Vector2 end, float thick, Color color);
void raylib_lite_raylib_draw_line_strip(const Vector2 *points, int point_count, Color color);
void raylib_lite_raylib_draw_line_dashed(Vector2 start, Vector2 end, int dash_size,
                               int space_size, Color color);
void raylib_lite_raylib_draw_circle(int center_x, int center_y, float radius, Color color);
void raylib_lite_raylib_draw_circle_v(Vector2 center, float radius, Color color);
void raylib_lite_raylib_draw_circle_lines(int center_x, int center_y, float radius, Color color);
void raylib_lite_raylib_draw_circle_lines_v(Vector2 center, float radius, Color color);
void raylib_lite_raylib_draw_ellipse(int center_x, int center_y, float radius_h,
                            float radius_v, Color color);
void raylib_lite_raylib_draw_ellipse_v(Vector2 center, float radius_h, float radius_v, Color color);
void raylib_lite_raylib_draw_ellipse_lines(int center_x, int center_y, float radius_h,
                                 float radius_v, Color color);
void raylib_lite_raylib_draw_ellipse_lines_v(Vector2 center, float radius_h,
                                  float radius_v, Color color);
void raylib_lite_raylib_draw_rectangle(int x, int y, int width, int height, Color color);
void raylib_lite_raylib_draw_rectangle_v(Vector2 position, Vector2 size, Color color);
void raylib_lite_raylib_draw_rectangle_rec(Rectangle rectangle, Color color);
void raylib_lite_raylib_draw_rectangle_pro(Rectangle rectangle, Vector2 origin,
                                 float rotation, Color color);
void raylib_lite_raylib_draw_rectangle_gradient_v(int x, int y, int width, int height,
                                       Color top, Color bottom);
void raylib_lite_raylib_draw_rectangle_gradient_h(int x, int y, int width, int height,
                                       Color left, Color right);
void raylib_lite_raylib_draw_rectangle_lines(int x, int y, int width, int height, Color color);
void raylib_lite_raylib_draw_rectangle_lines_ex(Rectangle rectangle, float thick, Color color);
void raylib_lite_raylib_draw_rectangle_rounded(Rectangle rectangle, float roundness,
                                     int segments, Color color);
void raylib_lite_raylib_draw_rectangle_rounded_lines(Rectangle rectangle, float roundness,
                                          int segments, Color color);
void raylib_lite_raylib_draw_triangle(Vector2 a, Vector2 b, Vector2 c, Color color);
void raylib_lite_raylib_draw_triangle_lines(Vector2 a, Vector2 b, Vector2 c, Color color);
void raylib_lite_raylib_draw_triangle_fan(const Vector2 *points, int point_count, Color color);
void raylib_lite_raylib_draw_triangle_strip(const Vector2 *points, int point_count, Color color);
void raylib_lite_raylib_draw_poly(Vector2 center, int sides, float radius,
                         float rotation, Color color);
void raylib_lite_raylib_draw_poly_lines(Vector2 center, int sides, float radius,
                              float rotation, Color color);
void raylib_lite_raylib_draw_poly_lines_ex(Vector2 center, int sides, float radius,
                                float rotation, float thick, Color color);

Texture2D raylib_lite_raylib_load_texture(const char *asset_path);
void raylib_lite_raylib_unload_texture(Texture2D texture);
void raylib_lite_raylib_draw_texture(Texture2D texture, int x, int y, Color tint);
void raylib_lite_raylib_draw_texture_v(Texture2D texture, Vector2 position, Color tint);
void raylib_lite_raylib_draw_texture_rec(Texture2D texture, Rectangle source,
                               Vector2 position, Color tint);
void raylib_lite_raylib_draw_texture_ex(Texture2D texture, Vector2 position,
                              float rotation, float scale, Color tint);
void raylib_lite_raylib_draw_texture_pro(Texture2D texture, Rectangle source,
                               Rectangle dest, Vector2 origin,
                               float rotation, Color tint);

void raylib_lite_raylib_draw_text(const char *text, int x, int y, int font_size, Color color);
int raylib_lite_raylib_measure_text(const char *text, int font_size);
const char *raylib_lite_raylib_text_format_v(const char *format, va_list args);
const char *raylib_lite_raylib_text_format(const char *format, ...);
uint16_t *raylib_lite_raylib_get_framebuffer(int *width, int *height,
                                    size_t *stride_pixels);

bool raylib_lite_raylib_check_collision_recs(Rectangle first, Rectangle second);
bool raylib_lite_raylib_check_collision_circles(Vector2 first, float first_radius,
                                      Vector2 second, float second_radius);
bool raylib_lite_raylib_check_collision_point_rec(Vector2 point, Rectangle rectangle);
bool raylib_lite_raylib_check_collision_circle_rec(Vector2 center, float radius, Rectangle rectangle);
bool raylib_lite_raylib_check_collision_point_circle(Vector2 point, Vector2 center, float radius);
bool raylib_lite_raylib_check_collision_point_triangle(Vector2 point, Vector2 a,
                                            Vector2 b, Vector2 c);
Rectangle raylib_lite_raylib_get_collision_rec(Rectangle first, Rectangle second);
Color raylib_lite_raylib_fade(Color color, float alpha);
Color raylib_lite_raylib_color_alpha(Color color, float alpha);
Color raylib_lite_raylib_color_tint(Color color, Color tint);
Color raylib_lite_raylib_color_brightness(Color color, float factor);

#ifdef __cplusplus
}
#endif
