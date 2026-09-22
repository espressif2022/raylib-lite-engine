// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Raylib-compatible 2D API backed by the ESP-Mosaico RGB565 surface. Keep the
 * mapping explicit: unsupported upstream APIs fail at link time instead of
 * silently pulling a software OpenGL renderer into device firmware. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

void MosaicoFastInitWindow(int width, int height, const char *title);
void MosaicoFastCloseWindow(void);
bool MosaicoFastWindowShouldClose(void);
bool MosaicoFastIsWindowReady(void);
int MosaicoFastGetScreenWidth(void);
int MosaicoFastGetScreenHeight(void);
int MosaicoFastGetRenderWidth(void);
int MosaicoFastGetRenderHeight(void);
void MosaicoFastSetTargetFPS(int fps);
float MosaicoFastGetFrameTime(void);
double MosaicoFastGetTime(void);
int MosaicoFastGetFPS(void);

bool MosaicoFastIsKeyPressed(int key);
bool MosaicoFastIsKeyDown(int key);
bool MosaicoFastIsKeyReleased(int key);
bool MosaicoFastIsKeyUp(int key);
bool MosaicoFastIsMouseButtonPressed(int button);
bool MosaicoFastIsMouseButtonDown(int button);
bool MosaicoFastIsMouseButtonReleased(int button);
bool MosaicoFastIsMouseButtonUp(int button);
int MosaicoFastGetMouseX(void);
int MosaicoFastGetMouseY(void);
Vector2 MosaicoFastGetMousePosition(void);
int MosaicoFastGetTouchX(void);
int MosaicoFastGetTouchY(void);
Vector2 MosaicoFastGetTouchPosition(int index);
int MosaicoFastGetTouchPointId(int index);
int MosaicoFastGetTouchPointCount(void);
Vector3 MosaicoFastGetImuAcceleration(void);

/* Platform adapters feed the same state queried through the Raylib API. */
void MosaicoFastInjectKey(int key, bool pressed);
void MosaicoFastInjectAction(int action, bool pressed);
void MosaicoFastInjectPointer(int track_id, int x, int y, bool pressed);
void MosaicoFastInjectImu(float x, float y, float z);

void MosaicoFastBeginDrawing(void);
bool MosaicoFastFrameAvailable(void);
void MosaicoFastEndDrawing(void);
/* Consume one-shot edges after a logic tick; held input remains active. */
void MosaicoFastConsumeInputEdges(void);
void MosaicoFastBeginMode2D(Camera2D camera);
void MosaicoFastEndMode2D(void);
Vector2 MosaicoFastGetWorldToScreen2D(Vector2 position, Camera2D camera);
Vector2 MosaicoFastGetScreenToWorld2D(Vector2 position, Camera2D camera);
void MosaicoFastBeginScissorMode(int x, int y, int width, int height);
void MosaicoFastEndScissorMode(void);

void MosaicoFastClearBackground(Color color);
void MosaicoFastDrawPixel(int x, int y, Color color);
void MosaicoFastDrawPixelV(Vector2 position, Color color);
void MosaicoFastDrawLine(int start_x, int start_y, int end_x, int end_y, Color color);
void MosaicoFastDrawLineV(Vector2 start, Vector2 end, Color color);
void MosaicoFastDrawLineEx(Vector2 start, Vector2 end, float thick, Color color);
void MosaicoFastDrawLineStrip(const Vector2 *points, int point_count, Color color);
void MosaicoFastDrawLineDashed(Vector2 start, Vector2 end, int dash_size,
                               int space_size, Color color);
void MosaicoFastDrawCircle(int center_x, int center_y, float radius, Color color);
void MosaicoFastDrawCircleV(Vector2 center, float radius, Color color);
void MosaicoFastDrawCircleLines(int center_x, int center_y, float radius, Color color);
void MosaicoFastDrawCircleLinesV(Vector2 center, float radius, Color color);
void MosaicoFastDrawEllipse(int center_x, int center_y, float radius_h,
                            float radius_v, Color color);
void MosaicoFastDrawEllipseV(Vector2 center, float radius_h, float radius_v, Color color);
void MosaicoFastDrawEllipseLines(int center_x, int center_y, float radius_h,
                                 float radius_v, Color color);
void MosaicoFastDrawEllipseLinesV(Vector2 center, float radius_h,
                                  float radius_v, Color color);
void MosaicoFastDrawRectangle(int x, int y, int width, int height, Color color);
void MosaicoFastDrawRectangleV(Vector2 position, Vector2 size, Color color);
void MosaicoFastDrawRectangleRec(Rectangle rectangle, Color color);
void MosaicoFastDrawRectanglePro(Rectangle rectangle, Vector2 origin,
                                 float rotation, Color color);
void MosaicoFastDrawRectangleGradientV(int x, int y, int width, int height,
                                       Color top, Color bottom);
void MosaicoFastDrawRectangleGradientH(int x, int y, int width, int height,
                                       Color left, Color right);
void MosaicoFastDrawRectangleLines(int x, int y, int width, int height, Color color);
void MosaicoFastDrawRectangleLinesEx(Rectangle rectangle, float thick, Color color);
void MosaicoFastDrawRectangleRounded(Rectangle rectangle, float roundness,
                                     int segments, Color color);
void MosaicoFastDrawRectangleRoundedLines(Rectangle rectangle, float roundness,
                                          int segments, Color color);
void MosaicoFastDrawTriangle(Vector2 a, Vector2 b, Vector2 c, Color color);
void MosaicoFastDrawTriangleLines(Vector2 a, Vector2 b, Vector2 c, Color color);
void MosaicoFastDrawTriangleFan(const Vector2 *points, int point_count, Color color);
void MosaicoFastDrawTriangleStrip(const Vector2 *points, int point_count, Color color);
void MosaicoFastDrawPoly(Vector2 center, int sides, float radius,
                         float rotation, Color color);
void MosaicoFastDrawPolyLines(Vector2 center, int sides, float radius,
                              float rotation, Color color);
void MosaicoFastDrawPolyLinesEx(Vector2 center, int sides, float radius,
                                float rotation, float thick, Color color);

Texture2D MosaicoFastLoadTexture(const char *asset_path);
void MosaicoFastUnloadTexture(Texture2D texture);
void MosaicoFastDrawTexture(Texture2D texture, int x, int y, Color tint);
void MosaicoFastDrawTextureV(Texture2D texture, Vector2 position, Color tint);
void MosaicoFastDrawTextureRec(Texture2D texture, Rectangle source,
                               Vector2 position, Color tint);
void MosaicoFastDrawTextureEx(Texture2D texture, Vector2 position,
                              float rotation, float scale, Color tint);
void MosaicoFastDrawTexturePro(Texture2D texture, Rectangle source,
                               Rectangle dest, Vector2 origin,
                               float rotation, Color tint);

void MosaicoFastDrawText(const char *text, int x, int y, int font_size, Color color);
int MosaicoFastMeasureText(const char *text, int font_size);
const char *MosaicoFastTextFormatV(const char *format, va_list args);
const char *MosaicoFastTextFormat(const char *format, ...);
uint16_t *MosaicoFastGetFramebuffer(int *width, int *height,
                                    size_t *stride_pixels);

bool MosaicoFastCheckCollisionRecs(Rectangle first, Rectangle second);
bool MosaicoFastCheckCollisionCircles(Vector2 first, float first_radius,
                                      Vector2 second, float second_radius);
bool MosaicoFastCheckCollisionPointRec(Vector2 point, Rectangle rectangle);
bool MosaicoFastCheckCollisionCircleRec(Vector2 center, float radius, Rectangle rectangle);
bool MosaicoFastCheckCollisionPointCircle(Vector2 point, Vector2 center, float radius);
bool MosaicoFastCheckCollisionPointTriangle(Vector2 point, Vector2 a,
                                            Vector2 b, Vector2 c);
Rectangle MosaicoFastGetCollisionRec(Rectangle first, Rectangle second);
Color MosaicoFastFade(Color color, float alpha);
Color MosaicoFastColorAlpha(Color color, float alpha);
Color MosaicoFastColorTint(Color color, Color tint);
Color MosaicoFastColorBrightness(Color color, float factor);

#ifdef __cplusplus
}
#endif

#define InitWindow MosaicoFastInitWindow
#define CloseWindow MosaicoFastCloseWindow
#define WindowShouldClose MosaicoFastWindowShouldClose
#define IsWindowReady MosaicoFastIsWindowReady
#define GetScreenWidth MosaicoFastGetScreenWidth
#define GetScreenHeight MosaicoFastGetScreenHeight
#define GetRenderWidth MosaicoFastGetRenderWidth
#define GetRenderHeight MosaicoFastGetRenderHeight
#define SetTargetFPS MosaicoFastSetTargetFPS
#define GetFrameTime MosaicoFastGetFrameTime
#define GetTime MosaicoFastGetTime
#define GetFPS MosaicoFastGetFPS
#define IsKeyPressed MosaicoFastIsKeyPressed
#define IsKeyDown MosaicoFastIsKeyDown
#define IsKeyReleased MosaicoFastIsKeyReleased
#define IsKeyUp MosaicoFastIsKeyUp
#define IsMouseButtonPressed MosaicoFastIsMouseButtonPressed
#define IsMouseButtonDown MosaicoFastIsMouseButtonDown
#define IsMouseButtonReleased MosaicoFastIsMouseButtonReleased
#define IsMouseButtonUp MosaicoFastIsMouseButtonUp
#define GetMouseX MosaicoFastGetMouseX
#define GetMouseY MosaicoFastGetMouseY
#define GetMousePosition MosaicoFastGetMousePosition
#define GetTouchX MosaicoFastGetTouchX
#define GetTouchY MosaicoFastGetTouchY
#define GetTouchPosition MosaicoFastGetTouchPosition
#define GetTouchPointId MosaicoFastGetTouchPointId
#define GetTouchPointCount MosaicoFastGetTouchPointCount
#define BeginDrawing MosaicoFastBeginDrawing
#define EndDrawing MosaicoFastEndDrawing
#define BeginMode2D MosaicoFastBeginMode2D
#define EndMode2D MosaicoFastEndMode2D
#define GetWorldToScreen2D MosaicoFastGetWorldToScreen2D
#define GetScreenToWorld2D MosaicoFastGetScreenToWorld2D
#define BeginScissorMode MosaicoFastBeginScissorMode
#define EndScissorMode MosaicoFastEndScissorMode
#define ClearBackground MosaicoFastClearBackground
#define DrawPixel MosaicoFastDrawPixel
#define DrawPixelV MosaicoFastDrawPixelV
#define DrawLine MosaicoFastDrawLine
#define DrawLineV MosaicoFastDrawLineV
#define DrawLineEx MosaicoFastDrawLineEx
#define DrawLineStrip MosaicoFastDrawLineStrip
#define DrawLineDashed MosaicoFastDrawLineDashed
#define DrawCircle MosaicoFastDrawCircle
#define DrawCircleV MosaicoFastDrawCircleV
#define DrawCircleLines MosaicoFastDrawCircleLines
#define DrawCircleLinesV MosaicoFastDrawCircleLinesV
#define DrawEllipse MosaicoFastDrawEllipse
#define DrawEllipseV MosaicoFastDrawEllipseV
#define DrawEllipseLines MosaicoFastDrawEllipseLines
#define DrawEllipseLinesV MosaicoFastDrawEllipseLinesV
#define DrawRectangle MosaicoFastDrawRectangle
#define DrawRectangleV MosaicoFastDrawRectangleV
#define DrawRectangleRec MosaicoFastDrawRectangleRec
#define DrawRectanglePro MosaicoFastDrawRectanglePro
#define DrawRectangleGradientV MosaicoFastDrawRectangleGradientV
#define DrawRectangleGradientH MosaicoFastDrawRectangleGradientH
#define DrawRectangleLines MosaicoFastDrawRectangleLines
#define DrawRectangleLinesEx MosaicoFastDrawRectangleLinesEx
#define DrawRectangleRounded MosaicoFastDrawRectangleRounded
#define DrawRectangleRoundedLines MosaicoFastDrawRectangleRoundedLines
#define DrawTriangle MosaicoFastDrawTriangle
#define DrawTriangleLines MosaicoFastDrawTriangleLines
#define DrawTriangleFan MosaicoFastDrawTriangleFan
#define DrawTriangleStrip MosaicoFastDrawTriangleStrip
#define DrawPoly MosaicoFastDrawPoly
#define DrawPolyLines MosaicoFastDrawPolyLines
#define DrawPolyLinesEx MosaicoFastDrawPolyLinesEx
#define LoadTexture MosaicoFastLoadTexture
#define UnloadTexture MosaicoFastUnloadTexture
#define DrawTexture MosaicoFastDrawTexture
#define DrawTextureV MosaicoFastDrawTextureV
#define DrawTextureRec MosaicoFastDrawTextureRec
#define DrawTextureEx MosaicoFastDrawTextureEx
#define DrawTexturePro MosaicoFastDrawTexturePro
#define DrawText MosaicoFastDrawText
#define MeasureText MosaicoFastMeasureText
#define TextFormat MosaicoFastTextFormat
#define CheckCollisionRecs MosaicoFastCheckCollisionRecs
#define CheckCollisionCircles MosaicoFastCheckCollisionCircles
#define CheckCollisionPointRec MosaicoFastCheckCollisionPointRec
#define CheckCollisionCircleRec MosaicoFastCheckCollisionCircleRec
#define CheckCollisionPointCircle MosaicoFastCheckCollisionPointCircle
#define CheckCollisionPointTriangle MosaicoFastCheckCollisionPointTriangle
#define GetCollisionRec MosaicoFastGetCollisionRec
#define Fade MosaicoFastFade
#define ColorAlpha MosaicoFastColorAlpha
#define ColorTint MosaicoFastColorTint
#define ColorBrightness MosaicoFastColorBrightness
