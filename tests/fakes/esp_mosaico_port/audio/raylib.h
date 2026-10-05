// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>

typedef struct rAudioBuffer rAudioBuffer;
typedef struct rAudioProcessor rAudioProcessor;

typedef struct AudioStream {
    rAudioBuffer *buffer;
    rAudioProcessor *processor;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
} AudioStream;

typedef struct Sound {
    AudioStream stream;
    unsigned int frameCount;
} Sound;

typedef struct Music {
    AudioStream stream;
    unsigned int frameCount;
    bool looping;
    int ctxType;
    void *ctxData;
} Music;
