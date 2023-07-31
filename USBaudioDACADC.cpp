// USBaudioDACADC.cpp 

// Experiments with cheap USB audio dongles as DACs & ADCs
// with input and output DC blocking capacitors shorted
//   and mic bias disconnected

// TODO: ?mixer code to set output Speaker level to 100
//    waveOutSetVolume() no effect?

#if 0
  #define AudDeviceName "C-Media"  // C-Media USB Headphone Set
  // C-Media CM109/119
  // ~100KΩ input impedance
  // 
  const float VoutMax = 3.5933;  
  const float VoutMin = 0.8833;  
  const bool OutInverted = false;
  const unsigned short MicLevel = 0;

  // 5.030 Vusb
  // Microphone Level 0   120 Hz (no effect)
  // VinMax =    3.819    
  // Vcenter =   2.25     (2.2285 open circuit  (reads -549))
  // VinMin =    0.682    0.68 

#elif 0
  #define AudDeviceName "USB Ear-Microphone"
  // Genesi GL632
  // mic input on both Tip and Ring
  const float VoutMax = 3.0397;  // 2.708 Vpp
  const float VoutMin = 0.3317;  
  const bool OutInverted = false;
  const unsigned short MicLevel = 2.25 * 65535 / 12; // 1.5 dB min; 1.5 dB steps

  // set Microphone level >= +1.5dB (anything less mutes) 
  // +1.5dB actually amplifies signal X 5 = +14 dB, around ~1.645V
    //   how disable mic boost?  libusb driver? -- may need spec (or try CM1x9 endpoints?)
    //   chip also has AINL/R (not connected or supported by dongle driver)
  // input has 4.7KΩ input resistor, impedance is very high, no bias?
  // could use with external resistor (divide / 5) to extend range
  // or  Headphone Left -> R1 -> R2 / MicIn -> Headphone Right (w/ 5X reduced Headphone signal)
  //   thermistor and 10K 1% fixed R to measure absolute temperature

  const float VinMax = 1.89435;   // 0.54175 Vpp   gain X 5
  const float VinCenter = 1.62415;
  const float VinMin = 1.3526;   

#elif 1  // blue
  #define AudDeviceName "USB Headphone" 
  const float VoutMax = 2.7230; 
  const float VoutMin = 0.6086;  
  const bool OutInverted = true;
  const unsigned short MicLevel = 0; 

  // has 4 Hz digital HPF on input

#elif 1  // bag 
  #define AudDeviceName "USB Audio Device"  //  VID_1B3F&PID_2008  Generalplus Technology Inc.
  // 4 Hz HPF on mic input 
  // modulate >> 4 Hz using:
  // Headphone Left -> R1 -> R2 / MicIn -> Headphone Right 
  // -> no need to remove DC blocking caps for temperature (difference) measurement

  // set Microphone level to 40 to match input level to output
  // sine chirp shows ~4 Hz high-pass pop filter -- on chip
  //   -> DC is digitally blocked
  // ?option to disable pop filter -- need spec!
  // 
  const float VoutMax = 2.6826;   // TODO: gain from Vcenter depends on SAMPLE_HZ (filter?) and history
  const float VoutMin = 0.6270;  
  const bool OutInverted = true;
#endif

  const float VoutRange = VoutMax - VoutMin; 

// Uses USB dongle with CM108/119
// turn off AGC = 10X boost
//   blocking caps removed

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <mmsystem.h>
#pragma comment(lib, "Winmm.lib")

const int LoopSecs = 1; // beware possible overflow if LoopSecs > 9 * 60

#define BITS_PER_SAMPLE 16

#define WAV_OUT_BUF_SECS 1
#define WAV_OUT_SAMPLE_HZ 48000  // Voltage range is reduced below 44100
#define WAV_OUT_CHANNELS 2

struct {
 short left;
 short right;
} wavOutBuf[2][WAV_OUT_BUF_SECS * WAV_OUT_SAMPLE_HZ];

void wavOutDC(float leftV, float rightV) { 
  short leftVal, rightVal;
  if (OutInverted) { 
    leftVal =  -(max(0, min(1, (leftV  - VoutMin) / VoutRange)) * 65534 - 32767); // reversed
    rightVal = -(max(0, min(1, (rightV - VoutMin) / VoutRange)) * 65534 - 32767);
  } else {
    leftVal =  max(0, min(1, (leftV  - VoutMin) / VoutRange)) * 65535 - 32768;
    rightVal = max(0, min(1, (rightV - VoutMin) / VoutRange)) * 65535 - 32768;
  }

  for (int b = 0; b < 2; ++b)
    for (int s = 0; s < WAV_OUT_SAMPLE_HZ * WAV_OUT_BUF_SECS; ++s) {
      wavOutBuf[b][s].left = leftVal;
      wavOutBuf[b][s].right = rightVal;
    }
}

void wavOutSquare(int Hz = 480, bool inPhase = false) {
  for (int b = 0; b < 2; ++b)
    for (int s = 0; s < WAV_OUT_SAMPLE_HZ * WAV_OUT_BUF_SECS; ++s) {
      short val = s * Hz / WAV_OUT_SAMPLE_HZ % 2 ? 32767 : - 32767;
      wavOutBuf[b][s].left = val;
      wavOutBuf[b][s].right = inPhase ? val : -val;
    }
}

HWAVEIN hwi;
HWAVEOUT hwo;
WAVEHDR woh[2];

void queueWaveOut() {
  for (int b = 0; b < 2; ++b) {
    if (woh[b].dwFlags & WHDR_DONE || !woh[b].dwFlags) {
      if (woh[b].dwFlags & WHDR_DONE)
        waveOutUnprepareHeader(hwo, &woh[b], sizeof(WAVEHDR));

      // TODO: can change next fill data
      woh[b].dwBufferLength = WAV_OUT_BUF_SECS * WAV_OUT_SAMPLE_HZ * WAV_OUT_CHANNELS * BITS_PER_SAMPLE / 8;
      woh[b].dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP; 
      woh[b].dwLoops = LoopSecs / WAV_OUT_BUF_SECS;
      woh[b].lpData = (LPSTR)&wavOutBuf[b];
      MMRESULT res =  waveOutPrepareHeader(hwo, &woh[b], sizeof(WAVEHDR));
      res = waveOutWrite(hwo, &woh[b], sizeof(WAVEHDR));
      res = waveInStart(hwi); // for consistent phase
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
                    WAV_OUT_CHANNELS * BITS_PER_SAMPLE / 8, BITS_PER_SAMPLE, 0};  
  MMRESULT res = waveOutOpen(&hwo, wavOutDevID, &wfx, NULL, 0, WAVE_FORMAT_DIRECT);
  res = waveOutSetVolume(hwo, 0xFFFFFFFF); // not supported?

  DWORD volume = 0;
  res = waveOutGetVolume(hwo, &volume);
  if (res || volume != 0xFFFFFFFF) {
    printf("Set Headphone Level to 100\a\n");
    setOutLevel(wavOutDevID, 0xFFFF);  // no help
  }
  queueWaveOut();
}


#define WAV_IN_BUF_SECS LoopSecs
#define WAV_IN_SAMPLE_HZ 48000 // for 60 Hz notch filtering
#define WAV_IN_CHANNELS 1

short wavInBuf[2][WAV_IN_BUF_SECS * WAV_IN_SAMPLE_HZ];

WAVEHDR wih[2]; 

const int WavOutHz = 40;  // ? attenuation at 10X HPF cutoff? -- minimal with good digital filter

float amplitude;

float queueWaveIn() {
  float avg = 0;
  
  for (int b = 0; b < 2; ++b) {
    if (wih[b].dwFlags & WHDR_DONE || !wih[b].dwFlags) {
      if (wih[b].dwFlags & WHDR_DONE) {
        // average the data 
        const int NumSamples = WAV_IN_BUF_SECS * WAV_IN_SAMPLE_HZ;
        long long sum = 0;  // beware overflow 
        
        long long amplSum = 0;
        int amplSamples = 0;

        int phase = -1;
        for (int p = 1200; p < 2400; ++p)
          if (wavInBuf[b][p] * wavInBuf[b][1200] < 0) {  // sign change
            phase = p - 1200;
            break;
          }
        static int prevPhase;
        if (phase != prevPhase)
          printf("%d\n", prevPhase = phase);

        const int RingingSamples = 32;  // generous - actually ~20?

        phase -= WAV_OUT_SAMPLE_HZ / WavOutHz + RingingSamples / 2;

        for (int s = 0; s < NumSamples; ++s) {
          sum += wavInBuf[b][s];

          // for modulated version, need phase (so dot product wwith wavOutBuf)
          // window the input to avoid ringing
          // TODO: try LPF the output waveform for less ringing?

          if ((s - phase) % (WAV_OUT_SAMPLE_HZ / WavOutHz) > RingingSamples) { // avoid ringing
            short outVal = (s - phase) * WavOutHz / WAV_OUT_SAMPLE_HZ % 2 ? -1 : 1;  // TODO - phase can be off 180 ************
            amplSum += wavInBuf[b][s] * outVal;
            ++amplSamples;
          }
        }
        avg = float(sum) / NumSamples;
        amplitude = float(amplSum) / amplSamples;

        waveInUnprepareHeader(hwi, &wih[b], sizeof(WAVEHDR));
      }
      
      wih[b].dwBufferLength = WAV_IN_BUF_SECS * WAV_IN_SAMPLE_HZ * WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8;
      wih[b].lpData = (LPSTR)&wavInBuf[b];
      MMRESULT res = waveInPrepareHeader(hwi, &wih[b], sizeof(WAVEHDR));
      res = waveInAddBuffer(hwi, &wih[b], sizeof(WAVEHDR));
    }
  }
  return avg;
}

void setMicLevel(int wavInDevID, unsigned short micLevel) {
  MMRESULT result;
  HMIXER hMixer;
  result = mixerOpen(&hMixer, (UINT)wavInDevID, NULL, 0, MIXER_OBJECTF_WAVEIN);

  MIXERLINE ml = {0};
  ml.cbStruct = sizeof(MIXERLINE);
  ml.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;
  result = mixerGetLineInfo((HMIXEROBJ)hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE);

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
  mcdu.dwValue = micLevel; // 0..65535
  mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
  mcd.hwndOwner = 0;
  mcd.dwControlID = mc.dwControlID;
  mcd.paDetails = &mcdu;
  mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  mcd.cChannels = 1;
  result = mixerSetControlDetails((HMIXEROBJ) hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
}

void startAudioIn(const char* deviceName) {
  int wavInDevID = -1;
  int numDevs = waveInGetNumDevs();
  for (int devID = 0; devID < numDevs; ++devID) {
    WAVEINCAPS wic;
    if (waveInGetDevCaps(devID, &wic, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR) {
      printf("DeviceID %d: %s\n", devID, wic.szPname);
      if (strstr(wic.szPname, deviceName)) {
        wavInDevID = devID;
        break;
      }
    }
  }
  if (wavInDevID == -1) {
    printf("Input %s not found\n", deviceName);
    return;
  }

  WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, WAV_IN_CHANNELS,
                      WAV_IN_SAMPLE_HZ, WAV_IN_SAMPLE_HZ * WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8,
                      WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8, BITS_PER_SAMPLE, 0};  
  MMRESULT res = waveInOpen(&hwi, wavInDevID, &wfx, NULL, 0, WAVE_FORMAT_DIRECT);
  
  setMicLevel(wavInDevID, MicLevel);

  queueWaveIn();
}


float temp(float ampl){
  const float attenuation = 29021.59 / 32767;  // calibrated with wavOutSqaure( inPhase = true)
  const int Beta = 3950;
  const float CtoK = 273.16;
  const int T0 = 25;

  // ampl = (R - R0) / (R0 + R) * attenuation
  // R - R0 = ampl / attenuation * (R0 + R)
  // R * (1 - ampl/ attenuation) = R0 * (1 + ampl/attenuation)
  // R / R0 = (1 + ampl / attenuation) / (1 - ampl / attenuation)
  // R / R0 = exp(Beta * (1 / (t + CtoK) - 1/(T0 + CtoK)))
  // log (R / R0) = Beta * (1 / (t + CtoK) - 1/ (T0 + CtoK))
  // t = 1 / (log(R / R0) / Beta + 1 / (T0 + CtoK)) - CtoK;
 
  ampl /= 32767;
  float temp = 1 / (log((1 + ampl / attenuation) / (1 - ampl / attenuation)) / Beta + 1 / (T0 + CtoK)) - CtoK;
  return temp;
}

int main() {
  
  // wavOutDC(0, 0);
#if 0  // calibrate attenuation - varies slightly -> could continuosly calibrate ??  TODO
  wavOutSquare(WavOutHz, true); // calibrate
#else
  wavOutSquare(WavOutHz);
#endif
  startAudioIn(AudDeviceName);
  startAudioOut(AudDeviceName);
  
  while (1) {
    float avg = queueWaveIn();
    if (avg != 0.0) {
      printf("%.4f %.2f\n", temp(amplitude), amplitude);
    }

  #if 0
    char ch;
    if(_kbhit()) switch(ch = _getch()) {
      case 'h' : wavOutDC(5, 5); break; // high
      case 'c' : wavOutDC((VoutMin + VoutMax)/2, (VoutMin + VoutMax)/2); break; // center
      case 'l' : wavOutDC(0, 0); break; // low

      default : wavOutDC( 1 + (ch - '0') / 10.,  1 + (ch - '0') / 10.); break;  // 1.0 to 1.9 + other chars
    }    
  #endif

    queueWaveOut();
    Sleep(LoopSecs * 1000 / 2);
  }

  return 0;
}















#if 0

void setVolts(float leftV, float rightV, int secs = 5) {
   #define SAMPLE_HZ 10  // Voltage is off below 40
   #define CHANNELS 2
   #define BITS_PER_SAMPLE 16

   const int dataSize = secs * SAMPLE_HZ * CHANNELS * BITS_PER_SAMPLE / 8;

   struct WAVEHEADER {  // 44 bytes
     char riffID[4];
     DWORD riffSize;  // file size - 8

     char riffFORMAT[4];
       char fmtID[4];
         DWORD fmtSize;
         PCMWAVEFORMAT pcm;

       char dataID[4];
         DWORD dataSize;
   } wavHdr = {{'R','I','F','F'}, 44 - 8 + dataSize, 
                 {'W','A','V','E'}, 
                    {'f','m','t',' '}, sizeof(PCMWAVEFORMAT),  
                     { WAVE_FORMAT_PCM, CHANNELS,
                       SAMPLE_HZ, SAMPLE_HZ * CHANNELS * BITS_PER_SAMPLE / 8,
                       CHANNELS * BITS_PER_SAMPLE / 8, BITS_PER_SAMPLE},
                    {'d','a','t','a'}, dataSize};                 

  FILE* fWav;
  if (fopen_s(&fWav, "setVolts.wav", "wb")) return; // playing

  fwrite(&wavHdr, sizeof(wavHdr), 1, fWav);

  // 2.237 Vcenter
  const float Vmax = 3.219;   // TODO: adjust
  const float Vmin = 1.266;   // drop with Si diode for RT9701 0.8V   ******
  const float Vrange = Vmax - Vmin;

  struct {
    short leftVal;
    short rightVal;
  } sample = {max(0, min(1, (leftV  - Vmin) / Vrange)) * 65535 - 32768, 
              max(0, min(1, (rightV - Vmin) / Vrange)) * 65535 - 32768};

  // better in memory -> one disk write!
  // once!
  for (int s = 0; s < SAMPLE_HZ * secs; ++s) 
     fwrite(&sample, sizeof(sample), 1, fWav);

  fclose(fWav);
}

#endif