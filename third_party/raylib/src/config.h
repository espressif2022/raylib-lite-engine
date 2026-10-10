// SPDX-License-Identifier: Apache-2.0
/* raylib configuration for the Raylib Lite rcore platform. Module and format
 * selection follows georgik/raylib 6.0.0~2 so device firmware keeps the same
 * footprint; MAX_KEYBOARD_KEYS stays at the upstream default because
 * KEY_RIGHT/LEFT/ENTER are above 256.
 * rlgl.h always routes SW_MALLOC to RL_MALLOC (plain malloc), so allocator
 * placement is governed by the SPIRAM malloc policy in sdkconfig. */
#ifndef CONFIG_H
#define CONFIG_H

#define SW_FRAMEBUFFER_COLOR_TYPE R5G6B5
#define SW_FRAMEBUFFER_OUTPUT_BGRA false

#define SUPPORT_MODULE_RSHAPES          1
#define SUPPORT_MODULE_RTEXTURES        1
#define SUPPORT_MODULE_RTEXT            1
#define SUPPORT_MODULE_RMODELS          0
#define SUPPORT_MODULE_RAUDIO           0

#define SUPPORT_CAMERA_SYSTEM           0
#define SUPPORT_GESTURES_SYSTEM         1
#define SUPPORT_RPRAND_GENERATOR        1
#define SUPPORT_MOUSE_GESTURES          0
#define SUPPORT_SSH_KEYBOARD_RPI        0
#define SUPPORT_WINMM_HIGHRES_TIMER     0
#define SUPPORT_PARTIALBUSY_WAIT_LOOP   0
#define SUPPORT_SCREEN_CAPTURE          0
#define SUPPORT_GIF_RECORDING           0
#define SUPPORT_COMPRESSION_API         0
#define SUPPORT_AUTOMATION_EVENTS       0
#define SUPPORT_CLIPBOARD_IMAGE         0

#define MAX_FILEPATH_CAPACITY         512
#define MAX_FILEPATH_LENGTH           256

#define MAX_KEYBOARD_KEYS             512
#define MAX_MOUSE_BUTTONS               4
#define MAX_GAMEPADS                    1
#define MAX_GAMEPAD_AXES                4
#define MAX_GAMEPAD_BUTTONS            16
#define MAX_GAMEPAD_VIBRATION_TIME   2.0f
#define MAX_TOUCH_POINTS                4
#define MAX_KEY_PRESSED_QUEUE           8
#define MAX_CHAR_PRESSED_QUEUE          8

#define MAX_DECOMPRESSION_SIZE         16

#define RL_SUPPORT_MESH_GPU_SKINNING    0

#undef RL_DEFAULT_BATCH_BUFFERS
#define RL_DEFAULT_BATCH_BUFFERS        1
#undef RL_DEFAULT_BATCH_DRAWCALLS
#define RL_DEFAULT_BATCH_DRAWCALLS    128
#undef RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS
#define RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS 2
#undef RL_MAX_MATRIX_STACK_SIZE
#define RL_MAX_MATRIX_STACK_SIZE       16
#undef RL_MAX_SHADER_LOCATIONS
#define RL_MAX_SHADER_LOCATIONS        16
#undef RL_CULL_DISTANCE_NEAR
#define RL_CULL_DISTANCE_NEAR        0.05
#undef RL_CULL_DISTANCE_FAR
#define RL_CULL_DISTANCE_FAR       1000.0

#define SUPPORT_QUADS_DRAW_MODE         1
#define SPLINE_SEGMENT_DIVISIONS       16

#define SUPPORT_FILEFORMAT_PNG          1
#define SUPPORT_FILEFORMAT_BMP          0
#define SUPPORT_FILEFORMAT_TGA          0
#define SUPPORT_FILEFORMAT_JPG          0
#define SUPPORT_FILEFORMAT_GIF          0
#define SUPPORT_FILEFORMAT_QOI          1
#define SUPPORT_FILEFORMAT_PSD          0
#define SUPPORT_FILEFORMAT_DDS          0
#define SUPPORT_FILEFORMAT_HDR          0
#define SUPPORT_FILEFORMAT_PIC          0
#define SUPPORT_FILEFORMAT_KTX          0
#define SUPPORT_FILEFORMAT_ASTC         0
#define MAX_IMAGE_FLIP_QUEUE            2

#define SUPPORT_FILEFORMAT_FNT          1
#define SUPPORT_FILEFORMAT_TTF          0
#define SUPPORT_FILEFORMAT_BMF          0
#define MAX_TEXT_BUFFER_LENGTH        512
#define MAX_TEXTSPLIT_COUNT            64

#endif
