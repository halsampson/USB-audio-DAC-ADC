#include "waveOut.h"
#include <stdio.h>
// #include "msclr\marshal.h"

// TODO: set output Speaker level to 100
//    waveOutSetVolume() no effect?
//    mixer Master Volume ont headphone out gain

#define WAV_OUT_CHANNELS 2

WAV_SAMPLE wavOutBuf[NUM_WAV_OUT_BUFFERS][WAV_OUT_BUFFER_SAMPLES];

HWAVEOUT hwo;
WAVEHDR woh[NUM_WAV_OUT_BUFFERS];

void queueWaveOut() {
  for (int b = 0; b < NUM_WAV_OUT_BUFFERS; ++b) {
    if (woh[b].dwFlags & WHDR_DONE || !woh[b].dwFlags) {
      if (woh[b].dwFlags & WHDR_DONE)
        waveOutUnprepareHeader(hwo, &woh[b], sizeof(WAVEHDR));

      // TODO: can change next fill data
      woh[b].dwBufferLength = sizeof(wavOutBuf[b]);
      woh[b].lpData = (LPSTR)wavOutBuf[b];
      woh[b].dwLoops = NUM_WAV_OUT_BUFFERS > 1 ? 1 : -1;   // TODO
      MMRESULT res = waveOutPrepareHeader(hwo, &woh[b], sizeof(WAVEHDR));
      if (res) 
        printf("woph %d ", res);

      woh[b].dwFlags |= WHDR_BEGINLOOP | WHDR_ENDLOOP; 
      res = waveOutWrite(hwo, &woh[b], sizeof(WAVEHDR));
      if (res) 
        printf("wow %d ", res);
    }
  }
}

void setOutLevel(int wavOutDevID, unsigned short outLevel) {
  MMRESULT result;
  HMIXER hMixer;
  result = mixerOpen(&hMixer, (UINT)wavOutDevID, NULL, 0, MIXER_OBJECTF_WAVEOUT);

  MIXERLINE ml = {0};
  ml.cbStruct = sizeof(MIXERLINE);
  ml.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
  result = mixerGetLineInfo((HMIXEROBJ)hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE); 
  if (result) return; // 0x400 = MIXERR_INVALLINE

  MIXERLINECONTROLS mlineControls;            // contains information about the controls of an audio line
  MIXERCONTROL controlArray[8];
  mlineControls.dwLineID  = ml.dwLineID;      // unique audio line identifier
  mlineControls.cControls = ml.cControls;     // number of controls associated with the line
  mlineControls.pamxctrl  = controlArray;     // points to the first MIXERCONTROL structure to be filled
  mlineControls.cbStruct  = sizeof(MIXERLINECONTROLS);
  mlineControls.cbmxctrl  = sizeof(MIXERCONTROL);
  // Get information on ALL controls associated with the specified audio line
  result = mixerGetLineControls((HMIXEROBJ) hMixer, &mlineControls, MIXER_OBJECTF_MIXER | MIXER_GETLINECONTROLSF_ALL);
  // 0: Mute  1: Volume

  MIXERLINECONTROLS mlc = {0};
  MIXERCONTROL mc = {0};
  mlc.cbStruct = sizeof(MIXERLINECONTROLS);
  mlc.dwLineID = ml.dwLineID;
  mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
  mlc.cControls = 1;
  mlc.pamxctrl = &mc;
  mlc.cbmxctrl = sizeof(MIXERCONTROL);
  result = mixerGetLineControls((HMIXEROBJ) hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);

  MIXERCONTROLDETAILS mcd = {0};
  MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};
  mcdu.dwValue = outLevel; // 0..65535
  mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
  mcd.dwControlID = mc.dwControlID;
  mcd.paDetails = &mcdu;
  mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  mcd.cChannels = 1;  // set all channels
  result = mixerSetControlDetails((HMIXEROBJ) hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
}

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
  if (uMsg == WOM_DONE) { // only when dwLoops exhausted !!!
    MMRESULT res = waveOutUnprepareHeader(hwo, &woh[0], sizeof(WAVEHDR));
    if (res !=	WAVERR_STILLPLAYING)
      printf("!");
  }
}


void startAudioOut(const char* deviceName) {
  int wavOutDevID = -1;
  int numDevs = waveOutGetNumDevs();
  for (int devID = 0; devID < numDevs; ++devID) {
    WAVEOUTCAPS woc;
    if (waveOutGetDevCaps(devID, &woc, sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR) {
      // printf("DeviceID %d: %s\n", devID, woc.szPname);
      if (strstr(woc.szPname, deviceName)) {
        wavOutDevID = devID;
        break;
      }
    }
  }
  if (wavOutDevID == -1) {
    printf("Output %s not found\n", deviceName);
    return;
  }

  WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, WAV_OUT_CHANNELS,
                    WAV_OUT_SAMPLE_HZ, WAV_OUT_SAMPLE_HZ * WAV_OUT_CHANNELS * BITS_PER_SAMPLE / 8,
                    WAV_OUT_CHANNELS * BITS_PER_SAMPLE / 8, BITS_PER_SAMPLE, sizeof(wfx)};  
  MMRESULT res = waveOutOpen(&hwo, wavOutDevID, &wfx, (DWORD_PTR)(VOID*)waveOutProc, 0, WAVE_FORMAT_DIRECT | CALLBACK_NULL); //  CALLBACK_FUNCTION);
  if (res)
    printf("woo %d ", res);

  res = waveOutSetVolume(hwo, 0xFFFFFFFF); // not supported?

  setOutLevel(wavOutDevID, 0xFFFF);  // no help

  DWORD volume = 0;
  res = waveOutGetVolume(hwo, &volume);
  if (res || volume != 0xFFFFFFFF) {
    printf("Set Headphone Level to 100\a\n");
  }
  queueWaveOut();
}
