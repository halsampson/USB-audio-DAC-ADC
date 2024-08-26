// USBaudioDACADC.cpp 

// Experiments with cheap USB audio dongles as DACs & ADCs
// with input and output DC blocking capacitors shorted
//   and mic bias disconnected

// TODO: faster sampling to WAV file -- detect breathing

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "wavein.h"
#include "waveOut.h"


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

#elif 0  // blue
  #define AudDeviceName "USB Headphone" 
  const float VoutMax = 2.7230; 
  const float VoutMin = 0.6086;  
  const bool OutInverted = true;
  const unsigned short MicLevel = 0; 

  // has 4 Hz digital HPF on input

#elif 1  // bag - most
  #define AudDeviceName "USB Audio Device"  //  VID_1B3F&PID_2008  Generalplus Technology Inc.
  // 4 Hz HPF on mic input 
  // modulate >> 4 Hz using:
  // Headphone Left -> R1 -> R2 / MicIn -> Headphone Right 
  // -> no need to remove DC blocking caps for temperature (difference) measurement

  // ?set Microphone level to 40 to match input level to output??
  // sine chirp shows ~4 Hz high-pass pop filter -- on chip
  //   -> DC is digitally blocked
  // ?option to disable pop filter -- need spec!
  // 
  const float VoutMax = 2.6826;   // TODO: gain from Vcenter depends on SAMPLE_HZ (filter?) and history
  const float VoutMin = 0.6270;  
  const bool OutInverted = true;
  const unsigned short MicLevel = 65535- 20000; // max   "AGC" = Mic Boost? on also  TODO
#elif 0
   #define AudDeviceName "USB PnP Sound Device"
#endif

const float VoutRange = VoutMax - VoutMin; 

// Uses USB dongle with CM108/119
// turn off AGC = 10X boost
//   blocking caps removed

void wavOutDC(float leftV, float rightV) { 
  short left, right;
  if (OutInverted) { 
    left =  -(max(0, min(1, (leftV  - VoutMin) / VoutRange)) * 65534 - 32767); // reversed
    right = -(max(0, min(1, (rightV - VoutMin) / VoutRange)) * 65534 - 32767);
  } else {
    left =  max(0, min(1, (leftV  - VoutMin) / VoutRange)) * 65535 - 32768;
    right = max(0, min(1, (rightV - VoutMin) / VoutRange)) * 65535 - 32768;
  }

  for (int b = 0; b < NUM_WAV_OUT_BUFFERS; ++b)
    for (int s = 0; s < WAV_OUT_BUFFER_SAMPLES; ++s) {
      wavOutBuf[b][s].left = left;
      wavOutBuf[b][s].right = right;
    }
}

void wavOutSquare(int Hz = 480, bool inPhase = false) {
  for (int b = 0; b < NUM_WAV_OUT_BUFFERS; ++b)
    for (int s = 0; s < WAV_OUT_BUFFER_SAMPLES; ++s) {
      short val = s * Hz / WAV_OUT_SAMPLE_HZ % 2 ? 32767 : - 32767;
      wavOutBuf[b][s].left = val;
      wavOutBuf[b][s].right = inPhase ? val : -val;
    }
}

void wavOutFilteredSquare(int Hz = 480, bool inPhase = false) {
  for (int b = 0; b < NUM_WAV_OUT_BUFFERS; ++b) {
    short val = 32767; // match last sample
    for (int s = 0; s < WAV_OUT_BUFFER_SAMPLES; ++s) {
      int targetVal = s * Hz / WAV_OUT_SAMPLE_HZ % 2 ? 32767 : -32767;
      int sign = targetVal > 0 ? 1 : -1;
      const int steps = 8;
      val += sign * min(abs(targetVal - val), 32768 / steps);
      wavOutBuf[b][s].left = val;
      wavOutBuf[b][s].right = inPhase ? val : -val;
    }
  }
}

float amplitude, avg;


const int WavOutHz = 480; // well above 4Hz HPF
const int CycleLen = WAV_OUT_SAMPLE_HZ / WavOutHz;

short* waveIn;

void squareAmpl() {
  long long amplSum = 0;
  int amplSamples = 0;
  const int RingingSamples = 24;  // depends on WAV_OUT_SAMPLE_HZ TODO
  int phase, refPhase = 0;

  // TODO: better find at least two zero crossings / direction
    // better average several 0-crossings 
  // best if works even with small amplitude: autocorrelate
        
  for (int p = 1200; p < 2400; ++p)
    if (waveIn[p] * waveIn[1200] < 0) {  // sign change
      phase = p - 1200;
      break;
    }
  
  int offset = refPhase - CycleLen + RingingSamples / 2;

  long long sum = 0;  // beware overflow 
  for (int s = 0; s < WAV_OUT_BUFFER_SAMPLES; ++s) {
    sum += waveIn[s];

    // for modulated version, need phase (so dot product wwith wavOutBuf)
    // window the input to avoid ringing

    if ((s - phase) % (CycleLen) > RingingSamples) { // avoid ringing
      short outVal = (s - phase) * CycleLen % 2 ? -1 : 1;  // TODO - phase can be off 180 ************
      amplSum += waveIn[s] * outVal;
      ++amplSamples;
    }
  }
  amplitude = float(amplSum) / amplSamples;
}

int phase;

void simpleAmplitude() {
  // average the data
  long long sum = 0, sal1 = 0;  // beware overflow 
  for (int s = phase; s < phase + BufferSamples - CycleLen; ++s) {
    sum  += waveIn[s];
    sal1 += waveIn[s] * ((s - phase) % CycleLen < CycleLen / 2 ? 1 : -1);  // * Walsh SAL(1)
  }
        
  amplitude = float(sal1 - sum) / (BufferSamples - CycleLen);  // DC removed
}

const int MaxVal = 16384;   // allows 2:1 ratio (down to 5K ohm = 10C)
volatile float r0Peak = MaxVal, thermPeak;

void wavOutTriangle() {
  for (int b = 0; b < NUM_WAV_OUT_BUFFERS; ++b) {
    for (int s = 0; s < WAV_OUT_BUFFER_SAMPLES; ++s) {
      float triangle = (1 - float(abs(s % CycleLen - CycleLen / 2)) / (CycleLen / 4));
      float dither = (float)rand() / RAND_MAX - 0.5f;
      wavOutBuf[b][s].left = triangle * r0Peak + dither; 
      wavOutBuf[b][s].right = triangle * thermPeak - dither;
    }
  }
}

float loopGain;

void waveInCallback(WAVEHDR* wh) {
  waveIn = (short*)wh->lpData;
  simpleAmplitude();

  // TODO: servo should track / predict (constant) temperature slew ***********
  // TODO: PID D vs. overshoot?
  static float lastAmplitude;
  thermPeak += loopGain * amplitude;  // simple P servo
  lastAmplitude = amplitude;
  wavOutTriangle();
}


// TODO: servo L/R out of phase output amplitudes for minimum mic input signal
//   --> R / R0 = L amplitude / R amplitude

// Ring = Red = Right = Thermistor \______ Mic In (mono)
// Tip = White = Left = 10K ohm    /

float temp(){
  const int Beta = 3950;
  const float CtoK = 273.16;
  const int T0 = 25;
  
  // Rm = 3K ohm electret bias resistor on mic input (most)
  // R / R0 = exp(Beta * (1 / (t + CtoK) - 1/(T0 + CtoK)))
  // log (R / R0) = Beta * (1 / (t + CtoK) - 1/ (T0 + CtoK))
  // 1/T = 1/T0 + (1/β) * ln(R/R0)
  // t = 1 / (log(R / R0) / Beta + 1 / (T0 + CtoK)) - CtoK;

  float temp = 1 / (log(-thermPeak / r0Peak) / Beta + 1 / (T0 + CtoK)) - CtoK; // check log
  return temp;
}

short peak;

int outInPhase() { 
  int phase = 0;
  peak = -32768;
  for (int s = 0; s < CycleLen; ++s)
    if (waveIn[s] > peak) {
      peak = waveIn[s];
      phase = s;
    }
  return (phase + CycleLen * 3 / 4) % CycleLen;
  // check also zero-crossings --> float phase?
}

const int SamplingMs = 1000 * NUM_WAV_IN_BUFFERS * BufferSamples / WAV_IN_SAMPLE_HZ;

void setPhase() {
  loopGain = 0;
  thermPeak = MaxVal; // in phase
  Sleep(200 + SamplingMs);  // let caps settle
  
  printf("   %d %d %.0f\n", phase = outInPhase(), peak, amplitude); // Determine out:in phase
  // peak should be well below 32767 else adjust gain (Out level 28)  TODO
}



int main() {
  setupAudioIn(AudDeviceName, &waveInCallback);

  startAudioOut(AudDeviceName);
  startWaveIn();
  
  // TODO: reduce playback level vs. clipping (42)
  setPhase();
  setPhase(); // TODO: check amplitude positive
  setPhase();

  thermPeak = -MaxVal;  // 25C
  loopGain = -0.3f; // adjust for fast settling with small overhsoot

  while (1) {
    static time_t lastTm;
    time_t tm = time(NULL);
    if (tm != lastTm) {
      lastTm = tm;
      static float last_t;
      float t = temp(); 
      printf("%.4f %+.4f %.1f %+.1f\n", t, t - last_t, -thermPeak, loopGain * amplitude);
      last_t = t;

      char ch;
      if (_kbhit()) switch(ch = _getch()) {
        case 'p' : 
          float savedThermPeak = thermPeak;
          float savedGain = loopGain;
          setPhase(); 
          loopGain = savedGain;
          thermPeak = savedThermPeak;
          Sleep(SamplingMs);
          thermPeak = savedThermPeak;
          break;
      }
    }
  }
  
  return 0;
}



# if 0
    char ch;
    if(_kbhit()) switch(ch = _getch()) { // DC calibration -- with output caps removed
      case 'h' : wavOutDC(5, 5); break; // high
      case 'c' : wavOutDC((VoutMin + VoutMax)/2, (VoutMin + VoutMax)/2); break; // center
      case 'l' : wavOutDC(0, 0); break; // low

      default : wavOutDC( 1 + (ch - '0') / 10.,  1 + (ch - '0') / 10.); break;  // 1.0 to 1.9 + other chars
    }    
#endif



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
    short left;
    short right;
  } sample = {max(0, min(1, (leftV  - Vmin) / Vrange)) * 65535 - 32768, 
              max(0, min(1, (rightV - Vmin) / Vrange)) * 65535 - 32768};

  // better in memory -> one disk write!
  // once!
  for (int s = 0; s < SAMPLE_HZ * secs; ++s) 
     fwrite(&sample, sizeof(sample), 1, fWav);

  fclose(fWav);
}

#endif