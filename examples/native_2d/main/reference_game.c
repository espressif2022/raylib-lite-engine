// SPDX-License-Identifier: Apache-2.0
#include "reference_game.h"
#include "raylib.h"

enum { WIDTH = 160, HEIGHT = 120 };

static float clamp_paddle(float value)
{
    if (value < 10) return 10;
    if (value > WIDTH - 10) return WIDTH - 10;
    return value;
}

int native_2d_run(const char *screenshot)
{
    InitWindow(0, 0, "Sprite Catch");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(30);
    SetExitKey(KEY_ESCAPE);
    SetRandomSeed(42);

    if (!FileExists("assets/sprite.png") || !FileExists("assets/glyphs.fnt")) return 2;
    Texture2D sprite = LoadTexture("assets/sprite.png");
    Font font = LoadFont("assets/glyphs.fnt");
    RenderTexture2D scene = LoadRenderTexture(WIDTH, HEIGHT);

    InitAudioDevice();
    Sound catch_sound = LoadSound("assets/catch.sound");
    Music music = LoadMusicStream("assets/music.sound");
    SetMusicVolume(music, 0.5f);
    PlayMusicStream(music);

    int result = 0;
    if (!IsTextureValid(sprite) || !IsFontValid(font) || GetGlyphInfo(font, 0x4e2d).value != 0x4e2d ||
        !IsRenderTextureValid(scene)) result = 3;
    if (!IsAudioDeviceReady() || !IsSoundValid(catch_sound) || !IsMusicValid(music)) result = 4;

    float paddle = WIDTH/2.0f;
    float star_x = paddle;
    float star_y = 24;
    int score = 0;
    bool paused = false;
    bool quit = false;
    float scale_x = (float)GetScreenWidth()/WIDTH;
    float scale_y = (float)GetScreenHeight()/HEIGHT;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    const Rectangle viewport = {
        (GetScreenWidth() - WIDTH*scale)/2, (GetScreenHeight() - HEIGHT*scale)/2,
        WIDTH*scale, HEIGHT*scale,
    };
    /* The native screen buffer retains the letterbox between frames. */
    bool clear_letterbox = scale_x != scale_y;
    while (!result && !quit && !WindowShouldClose())
    {
        Vector2 pointer = GetMousePosition();
        bool in_viewport = CheckCollisionPointRec(pointer, viewport);
        Vector2 touch = {(pointer.x - viewport.x)/scale, (pointer.y - viewport.y)/scale};
        bool tapped = in_viewport && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        quit = tapped && touch.y < 20 && touch.x >= WIDTH - 39;
        if (IsKeyPressed(KEY_P) || (tapped && touch.y < 20 && touch.x < 41))
        {
            paused = !paused;
            if (paused)
            {
                PauseMusicStream(music);
                PauseSound(catch_sound);
            }
            else
            {
                ResumeMusicStream(music);
                ResumeSound(catch_sound);
            }
        }
        if (!paused)
        {
            float dt = GetFrameTime();
            if (dt > 0.1f) dt = 0.1f;
            if (in_viewport && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && touch.y >= 20) paddle = touch.x;
            if (IsKeyDown(KEY_LEFT)) paddle -= dt*84.0f;
            if (IsKeyDown(KEY_RIGHT)) paddle += dt*84.0f;
            paddle = clamp_paddle(paddle);
            star_y += dt*78.0f;
            if (star_y >= HEIGHT - 19)
            {
                if (star_x >= paddle - 13 && star_x <= paddle + 13)
                {
                    score++;
                    PlaySound(catch_sound);
                }
                star_x = (float)GetRandomValue(12, WIDTH - 12);
                star_y = 24;
            }
        }
        UpdateMusicStream(music);

        BeginTextureMode(scene);
        ClearBackground((Color){13, 20, 35, 255});
        DrawRectangle(0, 0, WIDTH, 20, (Color){26, 39, 59, 255});
        DrawText(paused ? "Resume" : "Pause", 5, 5, 10, RAYWHITE);
        DrawText("Quit", WIDTH - 30, 5, 10, RAYWHITE);
        DrawText(TextFormat("%03d", score), 69, 5, 10, GOLD);
        DrawTextEx(font, "\xe4\xb8\xad", (Vector2){91, 5}, 9, 0, RAYWHITE);
        BeginScissorMode(0, 20, WIDTH, HEIGHT - 20);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircle((int)star_x, (int)star_y, 8, Fade(ORANGE, 0.3f));
        EndBlendMode();
        DrawTexture(sprite, (int)star_x - sprite.width/2, (int)star_y - sprite.height/2, WHITE);
        DrawRectangleRounded((Rectangle){paddle - 10, HEIGHT - 13, 20, 4}, 1, 4, SKYBLUE);
        EndScissorMode();
        EndTextureMode();

        BeginDrawing();
        if (clear_letterbox)
        {
            ClearBackground(BLACK);
            clear_letterbox = false;
        }
        DrawTexturePro(scene.texture, (Rectangle){0, 0, WIDTH, -HEIGHT},
                       viewport, (Vector2){0, 0}, 0, WHITE);
        EndDrawing();
    }
    if (!result && screenshot) TakeScreenshot(screenshot);

    StopMusicStream(music);
    StopSound(catch_sound);
    UnloadMusicStream(music);
    UnloadSound(catch_sound);
    CloseAudioDevice();
    UnloadRenderTexture(scene);
    if (font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
    UnloadTexture(sprite);
    return result;
}
