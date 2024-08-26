#pragma once

#include <windows.h>
#include <mmsystem.h>

#define NUM_WAV_OUT_BUFFERS 1
#define WAV_OUT_SAMPLE_HZ 48000  // Voltage range is reduced below 44100
#define WAV_OUT_BUFFER_SAMPLES (WAV_OUT_SAMPLE_HZ / 480 * 16)  // CycleLen * 16 dithers
#define BITS_PER_SAMPLE 16


typedef struct {
 short left;
 short right;
} WAV_SAMPLE;

extern WAV_SAMPLE wavOutBuf[NUM_WAV_OUT_BUFFERS][WAV_OUT_BUFFER_SAMPLES];


void startAudioOut(const char* deviceName);
