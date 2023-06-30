#include "audio_eq.h"
#include "build_config.h"
#include "fpu_util.h"
#include <math.h>
#include <string.h>

// powf requires linking in the runtime lib...
// TODO(ludde): Figure out a way to move this out of here.
#if defined(OS_WIN) && defined(ARCH_CPU_X86)
float __declspec(naked) _cdecl Pow(float base, float exp) {
  __asm {
    fld dword ptr [esp + 8] // exp
    fld dword ptr [esp + 4] // base
    fyl2x               //  exp * log2(base) 
    fld1 
    fld st(1) 	
    fprem               // remainder
    f2xm1               // (2 ^ remainder) - 1 
    faddp st(1),st(0)	// (exp * log2(base)) + ((2 ^ remainder) - 1) 
    fscale	        // (2 ^ int(st1)) * st0 
    fxch	        
    fstp st(0)
    ret
  }
}
#endif  // defined(OS_WIN) && defined(ARCH_CPU_X86)

#if TSP_WITH_EQUALIZER && !defined(MIPSEMULATOR)

static void EqualizerFloatCore(EqChannel *te, float *fdata, int num) {
  // For each active band, process all the samples
  EqBand *band = te->bands;
  int num_band = 10, i;
  do {
    float A = band->A;
    if (A != 0.0f) {
      float B = band->B, G = band->G;
      float y1 = band->y1, y2 = band->y2, x1 = band->x1, x2 = band->x2;
      for(i = 0; i < num; i++) {
        float x0 = fdata[i];
        // y[n] = 2 * (alpha*(x[n]-x[n-2]) + gamma*y[n-1] - beta*y[n-2])
        float y0 = (x0 - x2) * A + y1 * G + y2 * B;
        fdata[i] = x0 + y0;
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
      }
      band->x1 = x1;
      band->x2 = x2;
      band->y1 = y1;
      band->y2 = y2;
    }
  } while (band++, --num_band);

  // Run a peak limiter
  {
    float X = te->LX, Y = te->LY;
    for(i = 0; i < num; i++) {
      float v = fdata[i];
      float va = fabs(v);
      if (va > X) X = va;
      if (X >= 0.99f)
        fdata[i] = v * (0.99f / X);
      X *= Y;
    }
    te->LX = X;
  }
}

static void EqualizerIntCore(EqChannel *te, int16 *data, int num, int nch) {
  float fdata[4096], preamp;
  int i;
  // Convert from integers to floats, and pre-multiply.
  preamp = te->preamp;
  for(i = 0; i < num; i++)
    fdata[i] = data[i * nch] * preamp;
  EqualizerFloatCore(te, fdata, num);
  // Convert floats back to int
  for(i = 0; i < num; i++) {
    int j = (int)(fdata[i] * 32768.0f);
    if (j < -32768) j = -32768;
    if (j > 32767) j = 32767;
    data[i * nch] = j;
  }
}

static void InitCoeffs(float *coeffs, int sample_rate, int hz, float factor) {
  if (sample_rate >= hz * 2) {
    float u = (2 * 3.14159265358979323846f) * hz / sample_rate;
    float v = sin(u) / (factor * 2);
    float w = 1.0f / (v + 1.0f);
    coeffs[0] = v * w;
    coeffs[1] = (v - 1.0f) * w;
    coeffs[2] = cos(u) * (w * 2);
  } else {
    coeffs[0] = 0.0f;
  }
}

static void EqualizerSetBand(TspEqualizer *te, int band, float level_db) {
  int i;
  float level_lin = Pow(10.0f, level_db * (1.0f / 20)) - 1.0f;
  static const uint16 bandhz[] = {60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};
  te->coeffs[band] = level_db;
  for(i = 0; i < 2; i++) {
    EqBand *b = &te->channels[i].bands[band];
    InitCoeffs(&b->A, te->sample_rate, bandhz[band], 
               (level_db < 0) ? (1.41 * 0.5f) : (1.41 * 2.0f));
    b->A *= level_lin;
  }
}

static void EqualizerSetFormat(TspEqualizer *te, int num_channels, float sample_rate) {
  int i;
  te->sample_rate = sample_rate;
  memset(te->channels[0].bands, 0, sizeof(te->channels[0].bands));
  memset(te->channels[1].bands, 0, sizeof(te->channels[1].bands));
  for(i = 0; i < EQ_MAX_BANDS; i++)
    EqualizerSetBand(te, i, te->coeffs[i]);
  te->channels[1].LX = te->channels[0].LX = 0;
  te->channels[1].LY = te->channels[0].LY = Pow(0.001f, 1.0f / (sample_rate * 0.7f));
}

void TspEqualizerProcess(TspEqualizer *te, int16 *data, int lengthx, TspSampleFormat *format) {
  if (te->sample_rate != format->sample_rate)
    EqualizerSetFormat(te, 2, format->sample_rate);

  // Apply replay gain?
#if TSP_WITH_REPLAYGAIN
  if (te->enable & (kTspEqualizer_AutoGain_Track | kTspEqualizer_AutoGain_Album)) {
    float f = (te->enable & kTspEqualizer_AutoGain_Track) ? format->replaygain_track : format->replaygain_album;
    te->channels[1].preamp = te->channels[0].preamp = te->preamp * f;
  }
#endif

  do {
    int num = lengthx > 8192 ? 8192 : lengthx;
    // Process left and right
    EqualizerIntCore(te->channels + 0, data + 0, num >> 1, 2);
    EqualizerIntCore(te->channels + 1, data + 1, num >> 1, 2);
    data += num;
    lengthx -= num;
  } while (lengthx != 0);
}

void TspEqualizerSetValues(TspEqualizer *te, int enable, float pregain, const float bands[10]) {
  int i, j;
  float p = Pow(10.0f, pregain * (1.0f / 20)) * (1.0 / 32768);
  for(i = j = 0; i < EQ_MAX_BANDS; i++)
    j += (bands[i] != 0);
  if (pregain == 0.0f && j == 0 && !(enable & (kTspEqualizer_AutoGain_Track | kTspEqualizer_AutoGain_Album)))
    enable = 0;
  te->enable = enable;
  te->preamp = p;
  te->channels[0].preamp = p;
  te->channels[1].preamp = p;
  for(i = 0; i < EQ_MAX_BANDS; i++)
    EqualizerSetBand(te, i, bands[i]);
}

#endif  // TSP_WITH_EQUALIZER
