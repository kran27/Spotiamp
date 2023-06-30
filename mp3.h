#ifndef TINY_SPOTIFY_MP3_H_
#define TINY_SPOTIFY_MP3_H_

#include "types.h"
#include "tiny_spotify.h"

typedef struct Mp3Compressor {
  // Points at the wave buffer. Put the wave data here, set wavbuf_size to the size,
  // up to wavbuf_capacity, and call the dowork method.
  void *wavbuf;
  int wavbuf_size, wavbuf_capacity;
  
  // After compression finishes, this is set to the size of the mp3 data.
  byte *mp3buf;
  int mp3buf_size;
} Mp3Compressor;

Mp3Compressor *Mp3CompressorCreate(const TspSampleFormat *format, int bitrate);
void Mp3CompressorDestroy(Mp3Compressor *mp3);
bool Mp3CompressorCompress(Mp3Compressor *mp3, int flags);



#endif  // TINY_SPOTIFY_MP3_H_