// SPDX-License-Identifier: Apache-2.0
#include "mosaico_raylib_fast.h"

#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mosaico_game.h"
#include "mosaico_game_2d.h"
#include "mosaico_raylib_port.h"
#include "mosaico_rgb565.h"

static uint16_t *s_pixels;
static size_t s_stride;
static Camera2D s_camera;
static bool s_camera_active;
static bool s_window_ready;
static bool s_window_should_close;
static int s_screen_width = MOSAICO_GAME_WIDTH;
static int s_screen_height = MOSAICO_GAME_HEIGHT;
static int s_target_fps = 30;
static uint64_t s_presented_frames;
static bool s_scissor_active;
static int s_scissor_x0, s_scissor_y0, s_scissor_x1, s_scissor_y1;
#define MOSAICO_FAST_KEY_COUNT 512
#define MOSAICO_FAST_POINTER_COUNT 2
typedef struct {
    int track_id, x, y;
    bool active, pressed, released;
} fast_pointer_t;
static bool s_key_down[MOSAICO_FAST_KEY_COUNT];
static bool s_key_pressed[MOSAICO_FAST_KEY_COUNT];
static bool s_key_released[MOSAICO_FAST_KEY_COUNT];
static fast_pointer_t s_pointers[MOSAICO_FAST_POINTER_COUNT];
static Vector2 s_imu_xy;
static float s_imu_z;

Vector2 MosaicoFastGetWorldToScreen2D(Vector2 position, Camera2D camera)
{
    float zoom = camera.zoom == 0 ? 1.0f : camera.zoom;
    float radians = camera.rotation * (3.14159265358979323846f / 180.0f);
    float cs = cosf(radians), sn = sinf(radians);
    float x = (position.x - camera.target.x) * zoom;
    float y = (position.y - camera.target.y) * zoom;
    return (Vector2){camera.offset.x + x*cs - y*sn,
                     camera.offset.y + x*sn + y*cs};
}

Vector2 MosaicoFastGetScreenToWorld2D(Vector2 position, Camera2D camera)
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
    return s_camera_active ? MosaicoFastGetWorldToScreen2D(position, s_camera)
                           : position;
}

void MosaicoFastBeginMode2D(Camera2D camera)
{
    s_camera = camera;
    if (s_camera.zoom == 0) s_camera.zoom = 1.0f;
    s_camera_active = true;
}

void MosaicoFastEndMode2D(void) { s_camera_active = false; }

static inline uint16_t rgb565(Color c)
{
    return (uint16_t)(((uint16_t)(c.r & 0xf8U) << 8) |
                      ((uint16_t)(c.g & 0xfcU) << 3) | (c.b >> 3));
}

/* Constant-color spans share preparation across rows. Keep the exact /255
 * blend used by put_pixel; quantizing alpha would change layered UI output. */
typedef struct {
    uint16_t pixel;
    unsigned alpha, inverse, red, green, blue;
} span_paint_t;

static span_paint_t span_paint(Color c)
{
    return (span_paint_t){rgb565(c), c.a, 255U-c.a,
                          c.r*c.a, c.g*c.a, c.b*c.a};
}

static void fill_span(int y, int x0, int x1, const span_paint_t *paint)
{
    if (!s_pixels || !paint->alpha || (unsigned)y >= MOSAICO_GAME_HEIGHT) return;
    if (x0 < 0) x0 = 0;
    if (x1 > MOSAICO_GAME_WIDTH) x1 = MOSAICO_GAME_WIDTH;
    if (s_scissor_active) {
        if (y < s_scissor_y0 || y >= s_scissor_y1) return;
        if (x0 < s_scissor_x0) x0 = s_scissor_x0;
        if (x1 > s_scissor_x1) x1 = s_scissor_x1;
    }
    if (x0 >= x1) return;
    uint16_t *dst = s_pixels + (size_t)y*s_stride + x0;
    int count = x1-x0;
    if (paint->alpha == 255) {
        uint16_t pixel = paint->pixel;
        uint32_t pair = (uint32_t)pixel | ((uint32_t)pixel << 16);
        if ((uintptr_t)dst & 3U) { *dst++ = pixel; --count; }
        while (count >= 2) { memcpy(dst, &pair, sizeof(pair)); dst += 2; count -= 2; }
        if (count) *dst = pixel;
    } else {
        for (int i = 0; i < count; ++i) {
            unsigned old = dst[i];
            unsigned r = ((old >> 11)*8U*paint->inverse + paint->red)/255U;
            unsigned g = (((old >> 5)&63U)*4U*paint->inverse + paint->green)/255U;
            unsigned b = ((old&31U)*8U*paint->inverse + paint->blue)/255U;
            dst[i] = (uint16_t)(((r&0xf8U)<<8) | ((g&0xfcU)<<3) | (b>>3));
        }
    }
}

static inline void put_pixel(int x, int y, Color color)
{
    if (!s_pixels || (unsigned)x >= MOSAICO_GAME_WIDTH ||
            (unsigned)y >= MOSAICO_GAME_HEIGHT) return;
    if (s_scissor_active && (x < s_scissor_x0 || y < s_scissor_y0 ||
            x >= s_scissor_x1 || y >= s_scissor_y1)) return;
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

void MosaicoFastInitWindow(int width, int height, const char *title)
{
    (void)title;
    s_screen_width = width > 0 ? width : MOSAICO_GAME_WIDTH;
    s_screen_height = height > 0 ? height : MOSAICO_GAME_HEIGHT;
    s_window_ready = true;
    s_window_should_close = false;
    s_presented_frames = 0;
    memset(s_key_down, 0, sizeof(s_key_down));
    memset(s_key_pressed, 0, sizeof(s_key_pressed));
    memset(s_key_released, 0, sizeof(s_key_released));
    memset(s_pointers, 0, sizeof(s_pointers));
    s_imu_xy = (Vector2){0};
    s_imu_z = 0;
}

void MosaicoFastCloseWindow(void)
{
    s_window_should_close = true;
    s_window_ready = false;
}

bool MosaicoFastWindowShouldClose(void) { return s_window_should_close; }
bool MosaicoFastIsWindowReady(void) { return s_window_ready; }
int MosaicoFastGetScreenWidth(void) { return s_screen_width; }
int MosaicoFastGetScreenHeight(void) { return s_screen_height; }
int MosaicoFastGetRenderWidth(void) { return s_screen_width; }
int MosaicoFastGetRenderHeight(void) { return s_screen_height; }
void MosaicoFastSetTargetFPS(int fps) { if (fps > 0) s_target_fps = fps; }
float MosaicoFastGetFrameTime(void) { return 1.0f / (float)s_target_fps; }
double MosaicoFastGetTime(void)
{ return (double)s_presented_frames / (double)s_target_fps; }
int MosaicoFastGetFPS(void) { return s_target_fps; }

static bool valid_key(int key)
{ return key >= 0 && key < MOSAICO_FAST_KEY_COUNT; }

void MosaicoFastInjectKey(int key, bool pressed)
{
    if (!valid_key(key) || s_key_down[key] == pressed) return;
    s_key_down[key] = pressed;
    if (pressed) s_key_pressed[key] = true;
    else s_key_released[key] = true;
}

void MosaicoFastInjectAction(int action, bool pressed)
{
    static const int primary[] = {KEY_LEFT, KEY_RIGHT, KEY_SPACE, KEY_P, KEY_ENTER};
    static const int alias[] = {KEY_A, KEY_D, KEY_SPACE, KEY_P, KEY_ENTER};
    if ((unsigned)action >= sizeof(primary)/sizeof(primary[0])) return;
    MosaicoFastInjectKey(primary[action], pressed);
    MosaicoFastInjectKey(alias[action], pressed);
}

void MosaicoFastInjectPointer(int track_id, int x, int y, bool pressed)
{
    fast_pointer_t *slot = NULL;
    for (int i = 0; i < MOSAICO_FAST_POINTER_COUNT; ++i)
        if (s_pointers[i].active && s_pointers[i].track_id == track_id) slot = &s_pointers[i];
    if (!slot && pressed) for (int i = 0; i < MOSAICO_FAST_POINTER_COUNT; ++i)
        if (!s_pointers[i].active) { slot = &s_pointers[i]; break; }
    if (!slot) return;
    slot->track_id = track_id; slot->x = x; slot->y = y;
    if (pressed && !slot->active) slot->pressed = true;
    if (!pressed && slot->active) slot->released = true;
    slot->active = pressed;
}

void MosaicoFastInjectImu(float x, float y, float z)
{ s_imu_xy = (Vector2){x, y}; s_imu_z = z; }

bool MosaicoFastIsKeyPressed(int key) { return valid_key(key) && s_key_pressed[key]; }
bool MosaicoFastIsKeyDown(int key) { return valid_key(key) && s_key_down[key]; }
bool MosaicoFastIsKeyReleased(int key) { return valid_key(key) && s_key_released[key]; }
bool MosaicoFastIsKeyUp(int key) { return !MosaicoFastIsKeyDown(key); }
bool MosaicoFastIsMouseButtonPressed(int button)
{ return button == MOUSE_BUTTON_LEFT && s_pointers[0].pressed; }
bool MosaicoFastIsMouseButtonDown(int button)
{ return button == MOUSE_BUTTON_LEFT && s_pointers[0].active; }
bool MosaicoFastIsMouseButtonReleased(int button)
{ return button == MOUSE_BUTTON_LEFT && s_pointers[0].released; }
bool MosaicoFastIsMouseButtonUp(int button)
{ return !MosaicoFastIsMouseButtonDown(button); }
int MosaicoFastGetMouseX(void) { return s_pointers[0].x; }
int MosaicoFastGetMouseY(void) { return s_pointers[0].y; }
Vector2 MosaicoFastGetMousePosition(void)
{ return (Vector2){(float)s_pointers[0].x, (float)s_pointers[0].y}; }
int MosaicoFastGetTouchPointCount(void)
{
    int count = 0;
    for (int i = 0; i < MOSAICO_FAST_POINTER_COUNT; ++i) count += s_pointers[i].active;
    return count;
}
static fast_pointer_t *active_pointer(int index)
{
    for (int i = 0; i < MOSAICO_FAST_POINTER_COUNT; ++i)
        if (s_pointers[i].active && index-- == 0) return &s_pointers[i];
    return NULL;
}
Vector2 MosaicoFastGetTouchPosition(int index)
{
    fast_pointer_t *pointer = active_pointer(index);
    return pointer ? (Vector2){(float)pointer->x, (float)pointer->y} : (Vector2){-1, -1};
}
int MosaicoFastGetTouchPointId(int index)
{
    fast_pointer_t *pointer = active_pointer(index);
    return pointer ? pointer->track_id : -1;
}
int MosaicoFastGetTouchX(void) { return (int)MosaicoFastGetTouchPosition(0).x; }
int MosaicoFastGetTouchY(void) { return (int)MosaicoFastGetTouchPosition(0).y; }
Vector3 MosaicoFastGetImuAcceleration(void)
{ return (Vector3){s_imu_xy.x, s_imu_xy.y, s_imu_z}; }

void MosaicoFastBeginDrawing(void)
{
    s_pixels = NULL;
    s_stride = 0;
    (void)mosaico_raylib_port_begin_frame(&s_pixels, &s_stride);
    mosaico_game_2d_set_target(s_pixels, s_stride, MOSAICO_GAME_WIDTH,
                               MOSAICO_GAME_HEIGHT);
    mosaico_game_2d_reset_raster_stats();
}

bool MosaicoFastFrameAvailable(void) { return s_pixels != NULL; }

void MosaicoFastConsumeInputEdges(void)
{
    memset(s_key_pressed, 0, sizeof(s_key_pressed));
    memset(s_key_released, 0, sizeof(s_key_released));
    for (int i = 0; i < MOSAICO_FAST_POINTER_COUNT; ++i) {
        s_pointers[i].pressed = false;
        s_pointers[i].released = false;
    }
}

void MosaicoFastEndDrawing(void)
{
    if (s_pixels) (void)mosaico_raylib_port_present_frame();
    s_pixels = NULL;
    s_stride = 0;
    mosaico_game_2d_set_target(NULL, 0, 0, 0);
    s_camera_active = false;
    s_scissor_active = false;
    ++s_presented_frames;
    MosaicoFastConsumeInputEdges();
}

void MosaicoFastBeginScissorMode(int x, int y, int width, int height)
{
    s_scissor_x0 = x < 0 ? 0 : x;
    s_scissor_y0 = y < 0 ? 0 : y;
    s_scissor_x1 = x + width > MOSAICO_GAME_WIDTH ? MOSAICO_GAME_WIDTH : x + width;
    s_scissor_y1 = y + height > MOSAICO_GAME_HEIGHT ? MOSAICO_GAME_HEIGHT : y + height;
    s_scissor_active = width > 0 && height > 0 &&
        s_scissor_x0 < s_scissor_x1 && s_scissor_y0 < s_scissor_y1;
    mosaico_game_2d_set_clip(s_scissor_x0, s_scissor_y0,
        s_scissor_x1 - s_scissor_x0, s_scissor_y1 - s_scissor_y0);
}

void MosaicoFastEndScissorMode(void)
{
    s_scissor_active = false;
    mosaico_game_2d_set_clip(0, 0, MOSAICO_GAME_WIDTH, MOSAICO_GAME_HEIGHT);
}

Texture2D MosaicoFastLoadTexture(const char *asset_path)
{ return Mosaico2DLoadTexture(asset_path); }
void MosaicoFastUnloadTexture(Texture2D texture)
{ Mosaico2DUnloadTexture(texture); }
void MosaicoFastDrawTexturePro(Texture2D texture, Rectangle source,
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
    Mosaico2DDrawTexturePro(texture, source, dest, origin, rotation, tint);
}
void MosaicoFastDrawTexture(Texture2D texture, int x, int y, Color tint)
{
    MosaicoFastDrawTexturePro(texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},
        (Rectangle){(float)x,(float)y,(float)texture.width,(float)texture.height},
        (Vector2){0,0},0,tint);
}
void MosaicoFastDrawTextureV(Texture2D texture, Vector2 position, Color tint)
{ MosaicoFastDrawTexture(texture,(int)position.x,(int)position.y,tint); }
void MosaicoFastDrawTextureRec(Texture2D texture, Rectangle source,
                               Vector2 position, Color tint)
{
    MosaicoFastDrawTexturePro(texture,source,
        (Rectangle){position.x,position.y,fabsf(source.width),fabsf(source.height)},
        (Vector2){0,0},0,tint);
}

void MosaicoFastDrawTextureEx(Texture2D texture, Vector2 position,
                              float rotation, float scale, Color tint)
{
    MosaicoFastDrawTexturePro(texture,
        (Rectangle){0, 0, (float)texture.width, (float)texture.height},
        (Rectangle){position.x, position.y, texture.width*scale, texture.height*scale},
        (Vector2){0, 0}, rotation, tint);
}

void MosaicoFastClearBackground(Color color)
{
    if (!s_pixels) return;
    uint16_t px = rgb565(color);
    if (s_stride == (size_t)MOSAICO_GAME_WIDTH) {
        mosaico_fill_rgb565(s_pixels, px,
            (size_t)MOSAICO_GAME_WIDTH * (size_t)MOSAICO_GAME_HEIGHT);
        return;
    }
    for (int y = 0; y < MOSAICO_GAME_HEIGHT; ++y)
        mosaico_fill_rgb565(s_pixels + (size_t)y * s_stride, px,
            (size_t)MOSAICO_GAME_WIDTH);
}

void MosaicoFastDrawPixel(int x, int y, Color color)
{
    Vector2 p = active_to_screen((Vector2){(float)x, (float)y});
    put_pixel((int)p.x, (int)p.y, color);
}

void MosaicoFastDrawPixelV(Vector2 position, Color color)
{ MosaicoFastDrawPixel((int)position.x, (int)position.y, color); }

void MosaicoFastDrawRectangle(int x, int y, int width, int height, Color color)
{
    if (s_camera_active) {
        Vector2 p = active_to_screen((Vector2){(float)x, (float)y});
        x = (int)p.x; y = (int)p.y;
        width = (int)(width*s_camera.zoom); height = (int)(height*s_camera.zoom);
    }
    if (!s_pixels || width <= 0 || height <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width > MOSAICO_GAME_WIDTH ? MOSAICO_GAME_WIDTH : x + width;
    int y1 = y + height > MOSAICO_GAME_HEIGHT ? MOSAICO_GAME_HEIGHT : y + height;
    if (s_scissor_active) {
        if (x0 < s_scissor_x0) x0 = s_scissor_x0;
        if (y0 < s_scissor_y0) y0 = s_scissor_y0;
        if (x1 > s_scissor_x1) x1 = s_scissor_x1;
        if (y1 > s_scissor_y1) y1 = s_scissor_y1;
    }
    if (x0 >= x1 || y0 >= y1) return;
    span_paint_t paint = span_paint(color);
    for (int yy=y0; yy<y1; ++yy) fill_span(yy, x0, x1, &paint);
}

void MosaicoFastDrawRectangleV(Vector2 position,Vector2 size,Color color)
{ MosaicoFastDrawRectangle((int)position.x,(int)position.y,(int)size.x,(int)size.y,color); }

void MosaicoFastDrawRectangleRec(Rectangle rectangle,Color color)
{ MosaicoFastDrawRectangle((int)rectangle.x,(int)rectangle.y,
    (int)rectangle.width,(int)rectangle.height,color); }

void MosaicoFastDrawRectanglePro(Rectangle r,Vector2 origin,float rotation,Color color)
{
    float angle=rotation*0.01745329252f,cs=cosf(angle),sn=sinf(angle);
    Vector2 local[4]={{-origin.x,-origin.y},{r.width-origin.x,-origin.y},
        {r.width-origin.x,r.height-origin.y},{-origin.x,r.height-origin.y}};
    Vector2 point[4];
    for(int i=0;i<4;++i)point[i]=(Vector2){
        r.x+local[i].x*cs-local[i].y*sn,
        r.y+local[i].x*sn+local[i].y*cs};
    MosaicoFastDrawTriangle(point[0],point[1],point[2],color);
    MosaicoFastDrawTriangle(point[0],point[2],point[3],color);
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

void MosaicoFastDrawRectangleGradientV(int x,int y,int width,int height,
                                       Color top,Color bottom)
{
    if(height<=0)return;
    unsigned limit=(unsigned)(height>1?height-1:1);
    for(int row=0;row<height;++row)
        MosaicoFastDrawRectangle(x,y+row,width,1,mix_color(top,bottom,(unsigned)row,limit));
}

void MosaicoFastDrawRectangleGradientH(int x,int y,int width,int height,
                                       Color left,Color right)
{
    if(width<=0)return;
    unsigned limit=(unsigned)(width>1?width-1:1);
    for(int column=0;column<width;++column)
        MosaicoFastDrawRectangle(x+column,y,1,height,mix_color(left,right,(unsigned)column,limit));
}

void MosaicoFastDrawLine(int x0, int y0, int x1, int y1, Color color)
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

void MosaicoFastDrawLineV(Vector2 start, Vector2 end, Color color)
{ MosaicoFastDrawLine((int)start.x, (int)start.y, (int)end.x, (int)end.y, color); }

void MosaicoFastDrawLineEx(Vector2 start, Vector2 end, float thick, Color color)
{
    start = active_to_screen(start);
    end = active_to_screen(end);
    if (s_camera_active) thick *= s_camera.zoom;
    if (thick <= 1.6f) {
        bool camera = s_camera_active;
        s_camera_active = false;
        MosaicoFastDrawLine((int)start.x, (int)start.y, (int)end.x, (int)end.y, color);
        if (thick > 1.05f) {
            float dx=end.x-start.x, dy=end.y-start.y;
            if (fabsf(dx) >= fabsf(dy))
                MosaicoFastDrawLine((int)start.x, (int)start.y+1, (int)end.x, (int)end.y+1, color);
            else
                MosaicoFastDrawLine((int)start.x+1, (int)start.y, (int)end.x+1, (int)end.y, color);
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

void MosaicoFastDrawLineStrip(const Vector2 *points,int count,Color color)
{ if(!points)return;for(int i=1;i<count;++i)MosaicoFastDrawLineV(points[i-1],points[i],color); }

void MosaicoFastDrawLineDashed(Vector2 start,Vector2 end,int dash,int space,Color color)
{
    float dx=end.x-start.x,dy=end.y-start.y,length=sqrtf(dx*dx+dy*dy);
    if(length<=0||dash<=0)return;
    if(space<0)space=0;
    float ux=dx/length,uy=dy/length,step=(float)(dash+space);
    for(float at=0;at<length;at+=step){
        float stop=fminf(at+dash,length);
        MosaicoFastDrawLineV((Vector2){start.x+ux*at,start.y+uy*at},
            (Vector2){start.x+ux*stop,start.y+uy*stop},color);
    }
}

void MosaicoFastDrawCircle(int center_x, int center_y, float radius, Color color)
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

void MosaicoFastDrawCircleV(Vector2 center,float radius,Color color)
{ MosaicoFastDrawCircle((int)center.x,(int)center.y,radius,color); }

void MosaicoFastDrawCircleLines(int center_x,int center_y,float radius,Color color)
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

void MosaicoFastDrawCircleLinesV(Vector2 center,float radius,Color color)
{ MosaicoFastDrawCircleLines((int)center.x,(int)center.y,radius,color); }

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

void MosaicoFastDrawEllipse(int x,int y,float rh,float rv,Color color)
{ draw_ellipse((Vector2){(float)x,(float)y},rh,rv,color,false); }
void MosaicoFastDrawEllipseV(Vector2 center,float rh,float rv,Color color)
{ draw_ellipse(center,rh,rv,color,false); }
void MosaicoFastDrawEllipseLines(int x,int y,float rh,float rv,Color color)
{ draw_ellipse((Vector2){(float)x,(float)y},rh,rv,color,true); }
void MosaicoFastDrawEllipseLinesV(Vector2 center,float rh,float rv,Color color)
{ draw_ellipse(center,rh,rv,color,true); }

void MosaicoFastDrawRectangleLines(int x, int y, int width, int height,
                                   Color color)
{
    MosaicoFastDrawRectangle(x, y, width, 1, color);
    MosaicoFastDrawRectangle(x, y + height - 1, width, 1, color);
    MosaicoFastDrawRectangle(x, y + 1, 1, height - 2, color);
    MosaicoFastDrawRectangle(x + width - 1, y + 1, 1, height - 2, color);
}

void MosaicoFastDrawRectangleLinesEx(Rectangle r,float thick,Color color)
{
    int t=(int)ceilf(thick);if(t<1)t=1;
    MosaicoFastDrawRectangle((int)r.x,(int)r.y,(int)r.width,t,color);
    MosaicoFastDrawRectangle((int)r.x,(int)(r.y+r.height)-t,(int)r.width,t,color);
    MosaicoFastDrawRectangle((int)r.x,(int)r.y+t,t,(int)r.height-2*t,color);
    MosaicoFastDrawRectangle((int)(r.x+r.width)-t,(int)r.y+t,t,(int)r.height-2*t,color);
}

static float rounded_radius(Rectangle r,float roundness)
{
    if (roundness < 0) roundness = 0;
    if (roundness > 1) roundness = 1;
    return fminf(fabsf(r.width),fabsf(r.height))*roundness*.5f;
}

void MosaicoFastDrawRectangleRounded(Rectangle r,float roundness,int segments,
                                     Color color)
{
    (void)segments;
    float radius=rounded_radius(r,roundness);
    if(radius<1){MosaicoFastDrawRectangleRec(r,color);return;}
    int rad=(int)ceilf(radius);
    MosaicoFastDrawRectangle((int)r.x+rad,(int)r.y,(int)r.width-2*rad,(int)r.height,color);
    MosaicoFastDrawRectangle((int)r.x,(int)r.y+rad,(int)r.width,(int)r.height-2*rad,color);
    MosaicoFastDrawCircle((int)r.x+rad,(int)r.y+rad,radius,color);
    MosaicoFastDrawCircle((int)(r.x+r.width)-rad-1,(int)r.y+rad,radius,color);
    MosaicoFastDrawCircle((int)r.x+rad,(int)(r.y+r.height)-rad-1,radius,color);
    MosaicoFastDrawCircle((int)(r.x+r.width)-rad-1,(int)(r.y+r.height)-rad-1,radius,color);
}

void MosaicoFastDrawRectangleRoundedLines(Rectangle r,float roundness,int segments,
                                          Color color)
{
    float radius=rounded_radius(r,roundness);
    if(radius<1){MosaicoFastDrawRectangleLines((int)r.x,(int)r.y,(int)r.width,(int)r.height,color);return;}
    int rad=(int)ceilf(radius),steps=segments>0?segments/4:6;
    if(steps<2)steps=2;
    float left=r.x,right=r.x+r.width-1,top=r.y,bottom=r.y+r.height-1;
    MosaicoFastDrawLineV((Vector2){left+rad,top},(Vector2){right-rad,top},color);
    MosaicoFastDrawLineV((Vector2){right,top+rad},(Vector2){right,bottom-rad},color);
    MosaicoFastDrawLineV((Vector2){right-rad,bottom},(Vector2){left+rad,bottom},color);
    MosaicoFastDrawLineV((Vector2){left,bottom-rad},(Vector2){left,top+rad},color);
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
            MosaicoFastDrawLineV(previous,next,color);
            previous=next;
        }
    }
}

static inline int edge(int ax, int ay, int bx, int by, int px, int py)
{
    return (px-ax)*(by-ay) - (py-ay)*(bx-ax);
}

void MosaicoFastDrawTriangle(Vector2 av, Vector2 bv, Vector2 cv, Color color)
{
    av=active_to_screen(av); bv=active_to_screen(bv); cv=active_to_screen(cv);
    int ax=(int)av.x, ay=(int)av.y, bx=(int)bv.x, by=(int)bv.y;
    int cx=(int)cv.x, cy=(int)cv.y;
    int minx=ax<bx?(ax<cx?ax:cx):(bx<cx?bx:cx);
    int maxx=ax>bx?(ax>cx?ax:cx):(bx>cx?bx:cx);
    int miny=ay<by?(ay<cy?ay:cy):(by<cy?by:cy);
    int maxy=ay>by?(ay>cy?ay:cy):(by>cy?by:cy);
    if(minx<0) minx=0;
    if(miny<0) miny=0;
    if(maxx>=MOSAICO_GAME_WIDTH) maxx=MOSAICO_GAME_WIDTH-1;
    if(maxy>=MOSAICO_GAME_HEIGHT) maxy=MOSAICO_GAME_HEIGHT-1;
    if(s_scissor_active){
        if(minx<s_scissor_x0)minx=s_scissor_x0;
        if(miny<s_scissor_y0)miny=s_scissor_y0;
        if(maxx>=s_scissor_x1)maxx=s_scissor_x1-1;
        if(maxy>=s_scissor_y1)maxy=s_scissor_y1-1;
    }
    int area=edge(ax,ay,bx,by,cx,cy);
    for(int y=miny;y<=maxy;++y) for(int x=minx;x<=maxx;++x){
        int w0=edge(bx,by,cx,cy,x,y), w1=edge(cx,cy,ax,ay,x,y);
        int w2=edge(ax,ay,bx,by,x,y);
        if((area>=0&&w0>=0&&w1>=0&&w2>=0)||
           (area<0&&w0<=0&&w1<=0&&w2<=0)) put_pixel(x,y,color);
    }
}

void MosaicoFastDrawTriangleLines(Vector2 a,Vector2 b,Vector2 c,Color color)
{
    MosaicoFastDrawLineV(a,b,color);
    MosaicoFastDrawLineV(b,c,color);
    MosaicoFastDrawLineV(c,a,color);
}

void MosaicoFastDrawTriangleFan(const Vector2 *points,int count,Color color)
{
    if(!points)return;
    for(int i=1;i+1<count;++i)
        MosaicoFastDrawTriangle(points[0],points[i],points[i+1],color);
}

void MosaicoFastDrawTriangleStrip(const Vector2 *points,int count,Color color)
{
    if(!points)return;
    for(int i=0;i+2<count;++i)
        MosaicoFastDrawTriangle(points[i],points[i+1],points[i+2],color);
}

static Vector2 polygon_point(Vector2 center,int sides,float radius,
                             float rotation,int index)
{
    float angle=(rotation+360.0f*(float)index/(float)sides)*0.01745329252f;
    return(Vector2){center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
}

void MosaicoFastDrawPoly(Vector2 center,int sides,float radius,float rotation,
                         Color color)
{
    if(sides<3)return;
    for(int i=0;i<sides;++i)
        MosaicoFastDrawTriangle(center,polygon_point(center,sides,radius,rotation,i),
            polygon_point(center,sides,radius,rotation,i+1),color);
}

void MosaicoFastDrawPolyLinesEx(Vector2 center,int sides,float radius,
                                float rotation,float thick,Color color)
{
    if(sides<3)return;
    for(int i=0;i<sides;++i)
        MosaicoFastDrawLineEx(polygon_point(center,sides,radius,rotation,i),
            polygon_point(center,sides,radius,rotation,i+1),thick,color);
}

void MosaicoFastDrawPolyLines(Vector2 center,int sides,float radius,
                              float rotation,Color color)
{ MosaicoFastDrawPolyLinesEx(center,sides,radius,rotation,1,color); }

static const uint8_t DIGITS[10][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};
static const uint8_t LETTERS[26][7] = {
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

static const uint8_t *glyph(char ch)
{
    static const uint8_t slash[7]={1,2,2,4,8,8,16};
    static const uint8_t dash[7]={0,0,0,31,0,0,0};
    static const uint8_t colon[7]={0,4,4,0,4,4,0};
    static const uint8_t dot[7]={0,0,0,0,0,6,6};
    if(ch>='0'&&ch<='9') return DIGITS[ch-'0'];
    if(ch>='A'&&ch<='Z') return LETTERS[ch-'A'];
    if(ch>='a'&&ch<='z') return LETTERS[ch-'a'];
    if(ch=='/') return slash;
    if(ch=='-') return dash;
    if(ch==':') return colon;
    if(ch=='.') return dot;
    return NULL;
}

void MosaicoFastDrawText(const char *text, int x, int y, int font_size,
                         Color color)
{
    if(!text) return;
    bool restore_camera=s_camera_active;
    if(s_camera_active){Vector2 p=active_to_screen((Vector2){(float)x,(float)y});x=(int)p.x;y=(int)p.y;font_size=(int)(font_size*s_camera.zoom);s_camera_active=false;}
    int scale=font_size/8; if(scale<1)scale=1;
    for(;*text;++text,x+=6*scale){
        const uint8_t *rows=glyph(*text); if(!rows) continue;
        for(int yy=0;yy<7;++yy){
            int xx=0;
            while(xx<5){
                while(xx<5&&!(rows[yy]&(1U<<(4-xx))))++xx;
                int start=xx;
                while(xx<5&&(rows[yy]&(1U<<(4-xx))))++xx;
                if(start<xx)MosaicoFastDrawRectangle(x+start*scale,y+yy*scale,
                    (xx-start)*scale,scale,color);
            }
        }
    }
    s_camera_active=restore_camera;
}

int MosaicoFastMeasureText(const char *text, int font_size)
{
    if(!text||!*text) return 0;
    int scale=font_size/8; if(scale<1)scale=1;
    return (int)strlen(text)*6*scale-scale;
}

const char *MosaicoFastTextFormat(const char *format, ...)
{
    static char buffers[2][64];
    static unsigned index;
    char *out = buffers[index++ & 1U];
    va_list args;
    va_start(args, format);
    vsnprintf(out, sizeof(buffers[0]), format, args);
    va_end(args);
    return out;
}

bool MosaicoFastCheckCollisionRecs(Rectangle a, Rectangle b)
{
    return a.x < b.x+b.width && a.x+a.width > b.x &&
           a.y < b.y+b.height && a.y+a.height > b.y;
}

bool MosaicoFastCheckCollisionCircles(Vector2 a,float ar,Vector2 b,float br)
{
    float dx=a.x-b.x,dy=a.y-b.y,r=ar+br;
    return dx*dx+dy*dy <= r*r;
}

bool MosaicoFastCheckCollisionPointRec(Vector2 p,Rectangle r)
{
    return p.x>=r.x && p.x<=r.x+r.width && p.y>=r.y && p.y<=r.y+r.height;
}

bool MosaicoFastCheckCollisionCircleRec(Vector2 center,float radius,Rectangle r)
{
    float x=fmaxf(r.x,fminf(center.x,r.x+r.width));
    float y=fmaxf(r.y,fminf(center.y,r.y+r.height));
    float dx=center.x-x,dy=center.y-y;
    return dx*dx+dy*dy<=radius*radius;
}

bool MosaicoFastCheckCollisionPointCircle(Vector2 point,Vector2 center,float radius)
{
    float dx=point.x-center.x,dy=point.y-center.y;
    return dx*dx+dy*dy<=radius*radius;
}

bool MosaicoFastCheckCollisionPointTriangle(Vector2 p,Vector2 a,Vector2 b,Vector2 c)
{
    float d1=(p.x-b.x)*(a.y-b.y)-(a.x-b.x)*(p.y-b.y);
    float d2=(p.x-c.x)*(b.y-c.y)-(b.x-c.x)*(p.y-c.y);
    float d3=(p.x-a.x)*(c.y-a.y)-(c.x-a.x)*(p.y-a.y);
    bool negative=d1<0||d2<0||d3<0;
    bool positive=d1>0||d2>0||d3>0;
    return !(negative&&positive);
}

Rectangle MosaicoFastGetCollisionRec(Rectangle a,Rectangle b)
{
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    float right=fminf(a.x+a.width,b.x+b.width);
    float bottom=fminf(a.y+a.height,b.y+b.height);
    if(right<=x||bottom<=y)return(Rectangle){0};
    return(Rectangle){x,y,right-x,bottom-y};
}

static uint8_t clamp_byte(float value)
{ return(uint8_t)(value<0?0:value>255?255:value+.5f); }

Color MosaicoFastColorAlpha(Color color,float alpha)
{
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;
    color.a=clamp_byte(alpha*255);
    return color;
}

Color MosaicoFastFade(Color color,float alpha)
{ return MosaicoFastColorAlpha(color,alpha); }

Color MosaicoFastColorTint(Color color,Color tint)
{
    return(Color){(uint8_t)(color.r*tint.r/255U),(uint8_t)(color.g*tint.g/255U),
        (uint8_t)(color.b*tint.b/255U),(uint8_t)(color.a*tint.a/255U)};
}

Color MosaicoFastColorBrightness(Color color,float factor)
{
    if (factor < -1) factor = -1;
    if (factor > 1) factor = 1;
    float add=factor*255;
    color.r=clamp_byte(color.r+add);
    color.g=clamp_byte(color.g+add);
    color.b=clamp_byte(color.b+add);
    return color;
}
