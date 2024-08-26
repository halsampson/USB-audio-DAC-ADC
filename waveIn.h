#pragma once

#include <windows.h>
#include <mmsystem.h>

#define WAV_IN_SAMPLE_HZ 48000  // requested standard nominal value
#define NUM_WAV_IN_BUFFERS 2    // TODO: single buffer will lose samples

const double SampleHz = WAV_IN_SAMPLE_HZ; // 191996.84 / 4;          // Audio sampling rate
const int BufferSamples = WAV_IN_SAMPLE_HZ / 32 / 3 * 4; // TODO: short buffer will lose samples at switch

  // Use 1PPScalib to calibrate
  // Use 'f' key to see estimated error

void setupAudioIn(const char* deviceName, void (*)(WAVEHDR* wh));
void startWaveIn();
void waveInReady();
void stopWaveIn();