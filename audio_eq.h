#ifndef TINY_SPOTIFY_AUDIO_EQ_H_
#define TINY_SPOTIFY_AUDIO_EQ_H_

#include "config.h"
#include "types.h"
#include "tiny_spotify.h"

#if TSP_WITH_EQUALIZER
enum {
  EQ_MAX_BANDS = 10,
  EQ_CHANNELS = 2
};

typedef struct EqBand {
  float A,B,G;
  float y1,y2,x1,x2;
} EqBand;

typedef struct EqChannel {
  float preamp;
  float LX,LY;
  EqBand bands[10];
} EqChannel;

typedef struct TspEqualizer {
  int enable;
  float preamp;
  float coeffs[10];
  int sample_rate;
  EqChannel channels[2];
} TspEqualizer;

void TspEqualizerSetValues(TspEqualizer *te, int enable, float pregain, const float bands[10]);
void TspEqualizerProcess(TspEqualizer *te, int16 *data, int lengthx, TspSampleFormat *format);

#endif  // TSP_WITH_EQUALIZER

#endif  // TINY_SPOTIFY_AUDIO_EQ_H_
