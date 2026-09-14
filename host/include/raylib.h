// SPDX-License-Identifier: Apache-2.0
#pragma once

/* The simulator compiles the embedded RGB565 backend directly, so it only
 * needs Raylib's public data types and constants. Device builds continue to
 * include the upstream Raylib component header. */
#include <stdbool.h>
#include <stdint.h>

typedef struct Vector2 { float x, y; } Vector2;
typedef struct Vector3 { float x, y, z; } Vector3;
typedef struct Rectangle { float x, y, width, height; } Rectangle;
typedef struct Color { uint8_t r, g, b, a; } Color;
typedef struct Texture { unsigned int id; int width, height, mipmaps, format; } Texture;
typedef Texture Texture2D;
typedef struct Camera2D { Vector2 offset, target; float rotation, zoom; } Camera2D;

#define KEY_SPACE 32
#define KEY_A 65
#define KEY_D 68
#define KEY_P 80
#define KEY_ENTER 257
#define KEY_RIGHT 262
#define KEY_LEFT 263
#define MOUSE_BUTTON_LEFT 0

#define PIXELFORMAT_UNCOMPRESSED_R5G6B5 4
#define LIGHTGRAY  (Color){ 200, 200, 200, 255 }
#define GRAY       (Color){ 130, 130, 130, 255 }
#define DARKGRAY   (Color){ 80, 80, 80, 255 }
#define YELLOW     (Color){ 253, 249, 0, 255 }
#define GOLD       (Color){ 255, 203, 0, 255 }
#define ORANGE     (Color){ 255, 161, 0, 255 }
#define PINK       (Color){ 255, 109, 194, 255 }
#define RED        (Color){ 230, 41, 55, 255 }
#define MAROON     (Color){ 190, 33, 55, 255 }
#define GREEN      (Color){ 0, 228, 48, 255 }
#define LIME       (Color){ 0, 158, 47, 255 }
#define DARKGREEN  (Color){ 0, 117, 44, 255 }
#define SKYBLUE    (Color){ 102, 191, 255, 255 }
#define BLUE       (Color){ 0, 121, 241, 255 }
#define DARKBLUE   (Color){ 0, 82, 172, 255 }
#define PURPLE     (Color){ 200, 122, 255, 255 }
#define VIOLET     (Color){ 135, 60, 190, 255 }
#define DARKPURPLE (Color){ 112, 31, 126, 255 }
#define BEIGE      (Color){ 211, 176, 131, 255 }
#define BROWN      (Color){ 127, 106, 79, 255 }
#define DARKBROWN  (Color){ 76, 63, 47, 255 }
#define WHITE      (Color){ 255, 255, 255, 255 }
#define BLACK      (Color){ 0, 0, 0, 255 }
#define BLANK      (Color){ 0, 0, 0, 0 }
#define MAGENTA    (Color){ 255, 0, 255, 255 }
#define RAYWHITE   (Color){ 245, 245, 245, 255 }
