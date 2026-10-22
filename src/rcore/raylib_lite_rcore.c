// SPDX-License-Identifier: Apache-2.0
/* raylib platform backend over Raylib Lite video, clock and input contracts.
 * Upstream platform files are #included by rcore.c because they write CORE,
 * whose type is private to rcore.c; this file includes rcore.c for the same
 * reason. Build with PLATFORM_CUSTOM and GRAPHICS_API_OPENGL_SOFTWARE. */
#include "rcore.c"

#include <stdbool.h>
#include <string.h>
#include "raylib_lite_rcore.h"
#include "raylib_lite_raylib_port.h"

#if !defined(PLATFORM_CUSTOM) || !defined(GRAPHICS_API_OPENGL_SOFTWARE)
#error "raylib_lite_rcore requires PLATFORM_CUSTOM and GRAPHICS_API_OPENGL_SOFTWARE"
#endif

#define OVERRUN_YIELD_EVERY 8U
#define OVERRUN_YIELD_US 1000U

typedef struct {
    raylib_lite_rcore_config_t config;
    bool configured;
    bool port_ready;
    raylib_lite_result_t last_acquire;
    raylib_lite_result_t last_present;
    uint64_t base_us;
    uint32_t unpaced_frames;
    bool key_pressed_now[MAX_KEYBOARD_KEYS];
    bool key_release_pending[MAX_KEYBOARD_KEYS];
    bool mouse_pressed_now;
    bool mouse_release_pending;
} PlatformData;

static PlatformData platform = { 0 };

static const int default_button_keys[] = {
    KEY_LEFT, KEY_RIGHT, KEY_SPACE, KEY_P, KEY_ENTER,
};

//----------------------------------------------------------------------------------
// Raylib Lite API
//----------------------------------------------------------------------------------

raylib_lite_result_t raylib_lite_rcore_configure(const raylib_lite_rcore_config_t *config)
{
    if (!config || !config->clock.monotonic_us || !config->clock.sleep_for_us)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    if (config->button_keys && !config->button_key_count)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    if (platform.port_ready) return RAYLIB_LITE_INVALID_STATE;
    platform.config = *config;
    platform.configured = true;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_rcore_last_acquire_result(void)
{
    return platform.port_ready ? platform.last_acquire : RAYLIB_LITE_NOT_READY;
}

raylib_lite_result_t raylib_lite_rcore_last_present_result(void)
{
    return platform.port_ready ? platform.last_present : RAYLIB_LITE_NOT_READY;
}

raylib_lite_result_t raylib_lite_rcore_flush(uint32_t timeout_ms)
{
    if (!platform.port_ready) return RAYLIB_LITE_INVALID_STATE;
    return raylib_lite_raylib_port_flush(timeout_ms);
}

static uint64_t now_us(void)
{
    return platform.config.clock.monotonic_us(platform.config.clock.context);
}

static void sleep_us(uint64_t duration)
{
    platform.config.clock.sleep_for_us(platform.config.clock.context, duration);
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    return true;
}

void ToggleFullscreen(void) { TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on target platform"); }
void ToggleBorderlessWindowed(void) { TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on target platform"); }
void MaximizeWindow(void) { TRACELOG(LOG_WARNING, "MaximizeWindow() not available on target platform"); }
void MinimizeWindow(void) { TRACELOG(LOG_WARNING, "MinimizeWindow() not available on target platform"); }
void RestoreWindow(void) { TRACELOG(LOG_WARNING, "RestoreWindow() not available on target platform"); }
void SetWindowState(unsigned int flags) { (void)flags; TRACELOG(LOG_WARNING, "SetWindowState() not available on target platform"); }
void ClearWindowState(unsigned int flags) { (void)flags; TRACELOG(LOG_WARNING, "ClearWindowState() not available on target platform"); }
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count) { (void)images; (void)count; }
void SetWindowTitle(const char *title) { CORE.Window.title = title; }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowMonitor(int monitor) { (void)monitor; }

void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

void SetWindowSize(int width, int height)
{
    (void)width; (void)height;
    TRACELOG(LOG_WARNING, "SetWindowSize() not available on target platform");
}

void SetWindowOpacity(float opacity) { (void)opacity; }
void SetWindowFocused(void) { }
void *GetWindowHandle(void) { return NULL; }
int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { (void)monitor; return (Vector2){ 0, 0 }; }
int GetMonitorWidth(int monitor) { (void)monitor; return CORE.Window.display.width; }
int GetMonitorHeight(int monitor) { (void)monitor; return CORE.Window.display.height; }
int GetMonitorPhysicalWidth(int monitor) { (void)monitor; return 0; }
int GetMonitorPhysicalHeight(int monitor) { (void)monitor; return 0; }
int GetMonitorRefreshRate(int monitor) { (void)monitor; return 0; }
const char *GetMonitorName(int monitor) { (void)monitor; return "Raylib Lite"; }
Vector2 GetWindowPosition(void) { return (Vector2){ 0, 0 }; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){ 1.0f, 1.0f }; }
void SetClipboardText(const char *text) { (void)text; }
const char *GetClipboardText(void) { return NULL; }
Image GetClipboardImage(void) { return (Image){ 0 }; }
void ShowCursor(void) { CORE.Input.Mouse.cursorHidden = false; }
void HideCursor(void) { CORE.Input.Mouse.cursorHidden = true; }
void EnableCursor(void) { CORE.Input.Mouse.cursorHidden = false; }
void DisableCursor(void) { CORE.Input.Mouse.cursorHidden = true; }

/* rlsw stores rows bottom-up in the same RGB565 layout the video contract
 * uses, so presenting is one row-reversed copy into the acquired frame. */
static void copy_color_buffer(uint16_t *dst, size_t stride, uint16_t width, uint16_t height)
{
    int src_width = 0, src_height = 0;
    const uint16_t *src = swGetColorBuffer(&src_width, &src_height);
    if (!src) return;
    int rows = src_height < height ? src_height : height;
    int cols = src_width < width ? src_width : width;
    for (int y = 0; y < rows; y++)
    {
        memcpy(dst + (size_t)y*stride, src + (size_t)(src_height - 1 - y)*src_width,
               (size_t)cols*sizeof(uint16_t));
    }
}

/* WaitTime() only sleeps on desktop OSes, so pacing to SetTargetFPS() happens
 * here with the platform clock. Unpaced frames still yield periodically so
 * idle tasks and the task watchdog are serviced. */
static void pace_frame(void)
{
    double elapsed = CORE.Time.update + (GetTime() - CORE.Time.previous);
    if (CORE.Time.target > 0.0 && elapsed < CORE.Time.target)
    {
        sleep_us((uint64_t)((CORE.Time.target - elapsed)*1000000.0));
        platform.unpaced_frames = 0;
    }
    else if (++platform.unpaced_frames >= OVERRUN_YIELD_EVERY)
    {
        sleep_us(OVERRUN_YIELD_US);
        platform.unpaced_frames = 0;
    }
}

void SwapScreenBuffer(void)
{
    uint16_t *pixels = NULL;
    size_t stride = 0;
    platform.last_present = RAYLIB_LITE_NOT_READY;
    platform.last_acquire = raylib_lite_raylib_port_begin_frame(&pixels, &stride);
    if (platform.last_acquire == RAYLIB_LITE_OK)
    {
        uint16_t width = 0, height = 0;
        raylib_lite_raylib_port_get_dimensions(&width, &height);
        copy_color_buffer(pixels, stride, width, height);
        platform.last_present = raylib_lite_raylib_port_present_frame();
    }
    pace_frame();
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

double GetTime(void)
{
    if (!platform.configured) return 0.0;
    return (double)(now_us() - platform.base_us)*1e-6;
}

void OpenURL(const char *url) { (void)url; }

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

int SetGamepadMappings(const char *mappings) { (void)mappings; return 0; }

void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    (void)gamepad; (void)leftMotor; (void)rightMotor; (void)duration;
}

void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

void SetMouseCursor(int cursor) { CORE.Input.Mouse.cursor = cursor; }
const char *GetKeyName(int key) { (void)key; return ""; }

static int button_key(int32_t value)
{
    const int *keys = platform.config.button_keys ? platform.config.button_keys : default_button_keys;
    size_t count = platform.config.button_keys ? platform.config.button_key_count
        : sizeof(default_button_keys)/sizeof(default_button_keys[0]);
    if (value < 0 || (size_t)value >= count) return KEY_NULL;
    int key = keys[value];
    return (key > KEY_NULL && key < MAX_KEYBOARD_KEYS) ? key : KEY_NULL;
}

/* A press and release inside one poll would otherwise never be observed by
 * IsKeyPressed(); the release is applied at the next poll instead. */
static void apply_key(int key, bool pressed)
{
    if (key == KEY_NULL) return;
    if (pressed)
    {
        platform.key_release_pending[key] = false;
        if (!CORE.Input.Keyboard.currentKeyState[key] &&
            CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
        {
            CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount++] = key;
        }
        CORE.Input.Keyboard.currentKeyState[key] = 1;
        platform.key_pressed_now[key] = true;
    }
    else if (platform.key_pressed_now[key]) platform.key_release_pending[key] = true;
    else CORE.Input.Keyboard.currentKeyState[key] = 0;
}

static void apply_mouse(int32_t x, int32_t y, bool pressed)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    if (pressed)
    {
        platform.mouse_release_pending = false;
        CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 1;
        platform.mouse_pressed_now = true;
    }
    else if (platform.mouse_pressed_now) platform.mouse_release_pending = true;
    else CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 0;
}

static void apply_touch(int32_t id, int32_t x, int32_t y, bool pressed)
{
    int slot = -1;
    for (int i = 0; i < CORE.Input.Touch.pointCount; i++)
    {
        if (CORE.Input.Touch.pointId[i] == id) { slot = i; break; }
    }
    if (pressed)
    {
        if (slot < 0)
        {
            if (CORE.Input.Touch.pointCount >= MAX_TOUCH_POINTS) return;
            slot = CORE.Input.Touch.pointCount++;
            CORE.Input.Touch.pointId[slot] = id;
        }
        CORE.Input.Touch.position[slot] = (Vector2){ (float)x, (float)y };
        CORE.Input.Touch.currentTouchState[slot] = 1;
    }
    else if (slot >= 0)
    {
        int last = --CORE.Input.Touch.pointCount;
        CORE.Input.Touch.pointId[slot] = CORE.Input.Touch.pointId[last];
        CORE.Input.Touch.position[slot] = CORE.Input.Touch.position[last];
        CORE.Input.Touch.currentTouchState[last] = 0;
        CORE.Input.Touch.pointId[last] = -1;
    }
    /* Mouse queries read the first touch, as on raylib touch platforms. */
    if (CORE.Input.Touch.pointCount > 0)
        apply_mouse((int32_t)CORE.Input.Touch.position[0].x, (int32_t)CORE.Input.Touch.position[0].y, true);
    else apply_mouse(x, y, false);
}

void PollInputEvents(void)
{
#if SUPPORT_GESTURES_SYSTEM
    UpdateGestures();
#endif
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = (Vector2){ 0.0f, 0.0f };
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];
    for (int i = 0; i < MAX_TOUCH_POINTS; i++)
    {
        CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];
        CORE.Input.Touch.previousPosition[i] = CORE.Input.Touch.position[i];
    }
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
        if (platform.key_release_pending[i]) CORE.Input.Keyboard.currentKeyState[i] = 0;
        platform.key_release_pending[i] = false;
        platform.key_pressed_now[i] = false;
    }
    if (platform.mouse_release_pending) CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 0;
    platform.mouse_release_pending = false;
    platform.mouse_pressed_now = false;

    raylib_lite_input_event_t event;
    while (platform.config.input && raylib_lite_input_poll(platform.config.input, &event))
    {
        switch (event.type)
        {
            case RAYLIB_LITE_INPUT_BUTTON: apply_key(button_key(event.value), event.pressed); break;
            case RAYLIB_LITE_INPUT_POINTER: apply_mouse(event.x, event.y, event.pressed); break;
            case RAYLIB_LITE_INPUT_TOUCH: apply_touch(event.value, event.x, event.y, event.pressed); break;
            default: break;
        }
        if (platform.config.on_event) platform.config.on_event(platform.config.user, &event);
    }
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

int InitPlatform(void)
{
    if (!platform.configured)
    {
        TRACELOG(LOG_WARNING, "PLATFORM: call raylib_lite_rcore_configure() before InitWindow()");
        return -1;
    }
    if (rlGetVersion() != RL_OPENGL_SOFTWARE)
    {
        TRACELOG(LOG_WARNING, "PLATFORM: requires GRAPHICS_API_OPENGL_SOFTWARE");
        return -1;
    }

    raylib_lite_raylib_port_set_clock(&platform.config.clock);
    raylib_lite_result_t result = raylib_lite_raylib_port_init_backend(&platform.config.video);
    if (result != RAYLIB_LITE_OK)
    {
        raylib_lite_raylib_port_set_clock(NULL);
        TRACELOG(LOG_WARNING, "PLATFORM: video backend init failed (%d)", (int)result);
        return -1;
    }
    platform.port_ready = true;
    platform.last_acquire = RAYLIB_LITE_NOT_READY;
    platform.last_present = RAYLIB_LITE_NOT_READY;
    platform.unpaced_frames = 0;
    memset(platform.key_pressed_now, 0, sizeof(platform.key_pressed_now));
    memset(platform.key_release_pending, 0, sizeof(platform.key_release_pending));
    platform.mouse_pressed_now = platform.mouse_release_pending = false;
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.pointId[i] = -1;

    uint16_t width = 0, height = 0;
    raylib_lite_raylib_port_get_dimensions(&width, &height);
    if ((CORE.Window.screen.width && CORE.Window.screen.width != width) ||
        (CORE.Window.screen.height && CORE.Window.screen.height != height))
    {
        TRACELOG(LOG_WARNING, "PLATFORM: InitWindow(%i, %i) uses the %ux%u display size",
                 CORE.Window.screen.width, CORE.Window.screen.height, width, height);
    }
    CORE.Window.display.width = width;
    CORE.Window.display.height = height;
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
    CORE.Window.render.width = width;
    CORE.Window.render.height = height;
    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;
    CORE.Window.ready = true;

    platform.base_us = now_us();
    InitTimer();
    CORE.Storage.basePath = GetWorkingDirectory();

    TRACELOG(LOG_INFO, "PLATFORM: Raylib Lite initialized (%ux%u)", width, height);
    return 0;
}

void ClosePlatform(void)
{
    if (platform.port_ready)
    {
        raylib_lite_raylib_port_deinit();
        raylib_lite_raylib_port_set_clock(NULL);
    }
    platform.port_ready = false;
    platform.configured = false;
}
