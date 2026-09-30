#pragma once
#include <stdbool.h>
typedef struct { unsigned frameCount; } Sound;
void InitAudioDevice(void);
bool IsAudioDeviceReady(void);
Sound LoadSound(const char *path);
void SetSoundVolume(Sound sound,float volume);
void PlaySound(Sound sound);
void StopSound(Sound sound);
void UnloadSound(Sound sound);
void CloseAudioDevice(void);
