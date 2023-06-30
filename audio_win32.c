#include "build_config.h"
#include "config.h"
#include "types.h"
#include <assert.h>
#include <stdarg.h>
#include "tiny_spotify.h"
#include "util.h"
#include "mp3.h"

#if defined(OS_WIN) && (TSP_WITH_DEFAULT_AUDIO_DRIVER || TSP_WITH_MP3_COMPRESSOR)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmreg.h>
#include <MMSystem.h>
#include <msacm.h>
#include <stdlib.h>

#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "msacm32.lib")

#endif // defined(OS_WIN) && (TSP_WITH_DEFAULT_AUDIO_DRIVER || TSP_WITH_MP3_COMPRESSOR)

#if defined(OS_WIN) && TSP_WITH_DEFAULT_AUDIO_DRIVER



#define NUMBUFS 3

// For wave out
static HWAVEOUT hWaveOut;
static WAVEHDR wavehdr[NUMBUFS];
static HANDLE waveevent;
// Each buffer is 0.20 seconds of data
#define WAVBUFSIZE (44100 * 4 / 5)
static char wavebuf[NUMBUFS][WAVBUFSIZE];
static WAVEHDR *filling_header;
static HANDLE audio_thread_handle;
static int filling_pos;
struct TspSampleFormat last_sample_format;
static HANDLE wait_handle;
static CRITICAL_SECTION lock;
static int pending_bytes, pending_bufs;
static bool sect_inited;
static DWORD pending_bytes_ref_bytes, pending_bytes_ref_time;
static bool audio_paused;

static int devindex;
static volatile bool force_reload;
static void CALLBACK BufferCallback(PVOID lpParameter, BOOLEAN timer_fired);

TSP_PUBLIC int WavPush(void *context, int flags, const TspSampleType *data, int size, 
                       const TspSampleFormat *format, int *samples_buffered);
TSP_PUBLIC void WavSetVolume(int vol);
TSP_PUBLIC bool WavInit(Tsp *tsp);
TSP_PUBLIC void WavFree();

TSP_PUBLIC void WavSetDeviceId(int id) {
  devindex = id;
  force_reload = true;
}

TSP_PUBLIC int WavGetDeviceId() {
  return devindex;
}

TSP_PUBLIC bool WavReinit(const TspSampleFormat *sample_format) {
  WAVEFORMATEX wfex;
  int i;

  filling_pos = 0;
  filling_header = NULL;
  pending_bufs = 0;
  pending_bytes = 0;
  pending_bytes_ref_bytes = 0;
 
  if (hWaveOut) {
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);
  }
  wfex.wFormatTag=WAVE_FORMAT_PCM;                                 // simple, uncompressed format
  wfex.nChannels=sample_format->channels;                          //  1=mono, 2=stereo
  wfex.nSamplesPerSec=sample_format->sample_rate;                  // 44100
  wfex.nAvgBytesPerSec=wfex.nSamplesPerSec * wfex.nChannels * 2;   // = nSamplesPerSec * n.Channels * wBitsPerSample/8
  wfex.nBlockAlign= wfex.nChannels * 2;
  wfex.wBitsPerSample=16;                                          //  16 for high quality, 8 for telephone-grade
  wfex.cbSize=0;

  for (;;) {
    if (waveOutOpen(&hWaveOut, devindex, &wfex, (DWORD)waveevent, 0L, WAVE_FORMAT_DIRECT | CALLBACK_EVENT) == MMSYSERR_NOERROR)
      break;
    if (devindex == -1) {
      hWaveOut = NULL;
      LeaveCriticalSection(&lock);
      return false;
    }
    devindex = -1;
  }

  for(i = 0; i != NUMBUFS; i++) {
    WAVEHDR *hdr = wavehdr + i;
    hdr->lpData = wavebuf[i];
    hdr->dwBufferLength = WAVBUFSIZE;
    hdr->dwBytesRecorded=0;
    hdr->dwUser = 0L;
    hdr->dwFlags = 0;
    hdr->dwLoops = 0L;
    if (waveOutPrepareHeader(hWaveOut, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
      hWaveOut = NULL;
      LeaveCriticalSection(&lock);
      return false;
    }
  }  
  audio_paused = 0;
  return true;
}


static bool WriteFilledHeader() {
  // Always maintain two outstanding buffers while we fill the third
  if (pending_bufs < (NUMBUFS-1) && filling_header && filling_pos == filling_header->dwBufferLength) {
    MMRESULT mmr;
    filling_header->dwUser = 1;
    mmr = waveOutWrite(hWaveOut, filling_header, sizeof(WAVEHDR));
    if (mmr != MMSYSERR_NOERROR) {
      assert(0);
    }
    pending_bufs++;
    pending_bytes += filling_header->dwBufferLength;
    filling_header = NULL;
    filling_pos = 0;
    return true;
  }
  return false;
}

static void UpdateFreeBuffers() {
  int i;
  bool did_free = false;
  // Check which of the headers are now done.
  for(i = 0; i != NUMBUFS; i++) {
    WAVEHDR *hdr = &wavehdr[i];
    if (hdr->dwUser == 1 && (hdr->dwFlags & WHDR_DONE)) {
      hdr->dwUser = 0;
      pending_bytes -= hdr->dwBufferLength;
      pending_bufs--;

      did_free = true;
    }
  }

  // We know now that there is exactly 'pending_bytes + filling_pos' bytes left to play.
  if (did_free) {
    pending_bytes_ref_bytes = pending_bytes;
    pending_bytes_ref_time = GetTickCount();
  }

  WriteFilledHeader();
}


TSP_PUBLIC void WavSetVolume(int vol) {
  if (hWaveOut != NULL) {
    if (vol < 0) vol = 0;
    if (vol > 0xffff) vol = 0xffff;
    waveOutSetVolume(hWaveOut, vol * 0x10001);
  }
}

TSP_PUBLIC int WavGetVolume() {
  if (hWaveOut != NULL) {
    DWORD vol = 0xFFFFFFFF;
    waveOutGetVolume(hWaveOut, &vol);
    return ((vol & 0xFFFF) + (vol >> 16)) >> 1;
  } else {
    return 65535;
  }
}

// Push audio to sound card!
TSP_PUBLIC int WavPush(void *context, int flags, const TspSampleType *data, int size, 
                       const TspSampleFormat *sample_format, int *samples_buffered) {
  DWORD bytes_left_in_driver;
  int p = 0, n, i;
  size *= sizeof(TspSampleType);

  if (audio_thread_handle == NULL)
    UpdateFreeBuffers();

  // This has the unfortunate effect of skipping some samples
  // at the end - but it's OK for this demo driver.
  if (sample_format && (sample_format->channels != last_sample_format.channels ||
                        sample_format->sample_rate != last_sample_format.sample_rate || force_reload)) {
    last_sample_format = *sample_format;
    force_reload = false;
    WavReinit(sample_format);
  }

  if (hWaveOut) {
    // flags&1 is set when we're clearing the play state.
    if (flags & kTspAudioFlag_FlushBuffer) {
      waveOutReset(hWaveOut);
      filling_header = NULL;
      filling_pos = 0;
      pending_bytes = 0;
      pending_bufs = 0;
      for(i=0; i != NUMBUFS; i++) {
        wavehdr[i].dwUser = 0;
        wavehdr[i].dwFlags = WHDR_PREPARED;
      }
    }

    if (!!(flags & kTspAudioFlag_Pause) != audio_paused) {
      audio_paused ^= 1;
      if (audio_paused)
        waveOutPause(hWaveOut);
      else
        waveOutRestart(hWaveOut);
    }

    for(;;) {
      if (!filling_header) {
        for(i=0; i != NUMBUFS; i++) {
          if (wavehdr[i].dwUser == 0) {
            filling_header = &wavehdr[i];
            break;
          }
        }
        if (!filling_header)
          break;
      }
      n = IntMin(filling_header->dwBufferLength - filling_pos, size - p);
      memcpy(filling_header->lpData + filling_pos, (char*)data + p, n);
      p += n;
      filling_pos += n;
      if (!WriteFilledHeader())
        break;
    }
  }
  if (samples_buffered) {
    bytes_left_in_driver = 0;
    if (pending_bytes_ref_bytes != 0) {
      DWORD ms_since_update = GetTickCount() - pending_bytes_ref_time;
      if (ms_since_update < 10000) {
        DWORD consumed = ms_since_update * (44100 * 4) / 1000;
        if (consumed < pending_bytes_ref_bytes)
          bytes_left_in_driver = pending_bytes_ref_bytes - consumed;
      }
    }
    bytes_left_in_driver += filling_pos + pending_bytes - pending_bytes_ref_bytes;
    *samples_buffered = bytes_left_in_driver/ sizeof(TspSampleType);
    if (last_sample_format.channels == 2)
      *samples_buffered >>= 1;

  }

  return (unsigned)p / sizeof(TspSampleType);
}


static CRITICAL_SECTION audio_critical_section;
static HANDLE audio_event;
static Tsp *audio_tsp;
static void LockAudioThread(TspMutex *atd) { EnterCriticalSection(&audio_critical_section); }
static void UnlockAudioThread(TspMutex *atd) { LeaveCriticalSection(&audio_critical_section); }
static void WakeAudioThread(TspMutex *atd) { SetEvent(audio_event); }
static const TspMutex audio_thread_mutex = { &LockAudioThread, &UnlockAudioThread, &WakeAudioThread };
static DWORD WINAPI AudioThreadRunner(PVOID arg) {
  TspBool audio_active;
  HANDLE handles[2];
  DWORD rv = WAIT_OBJECT_0 - 1;
  int curr_pending_bufs;
  int sleep_time;

  for(;;) {
    EnterCriticalSection(&audio_critical_section);
    if (audio_tsp == NULL)
      break;
    // Once a buffer becomes free, fill it.
    if (rv == WAIT_OBJECT_0)
      UpdateFreeBuffers();

    // Invoke audio code in TSP, this will cause more buffers to be pushed.
    audio_active = TspAudioThreadingDoWork(audio_tsp);
    curr_pending_bufs = pending_bufs;
    LeaveCriticalSection(&audio_critical_section);

    handles[0] = waveevent;
    handles[1] = audio_event;
    sleep_time = audio_active && (audio_paused || curr_pending_bufs < (NUMBUFS-1)) ? 10 : INFINITE;
    rv = WaitForMultipleObjects(2, handles, FALSE, sleep_time);
  }
  return 0;
}

TSP_PUBLIC bool WavInit(Tsp *tsp) {
  DWORD thread_id;
  TspSampleFormat sample_format;

  audio_tsp = tsp;
  InitializeCriticalSection(&audio_critical_section);
  InitializeCriticalSection(&lock);

  audio_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  waveevent = CreateEvent(NULL, FALSE, FALSE, NULL);

  sample_format.channels = 2;
  sample_format.sample_rate = 44100;

  WavReinit(&sample_format);

  if (TspEnableAudioThreading(tsp, (TspMutex*)&audio_thread_mutex))
    audio_thread_handle = CreateThread(NULL, 0, &AudioThreadRunner, NULL, 0, &thread_id);
  return true;
}

TSP_PUBLIC void WavFree() {
  EnterCriticalSection(&audio_critical_section);
  audio_tsp = NULL;
  SetEvent(audio_event);
  LeaveCriticalSection(&audio_critical_section);
  if (audio_thread_handle) {
    WaitForSingleObject(audio_thread_handle, INFINITE);
    CloseHandle(audio_thread_handle);
  }
  CloseHandle(audio_event);
  CloseHandle(waveevent);
  DeleteCriticalSection(&audio_critical_section);
  DeleteCriticalSection(&lock);
  if (hWaveOut) {
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);
  }
  hWaveOut = NULL;
}



#endif  // defined(OS_WIN) && TSP_WITH_DEFAULT_AUDIO_DRIVER


#if defined(OS_WIN) && TSP_WITH_MP3_COMPRESSOR

#define MP3_WAVBUFSIZE 16384

typedef struct Mp3CompressorWin32 {
  Mp3Compressor base;

  HACMSTREAM mp3stream;
  ACMSTREAMHEADER mp3streamHead;
  byte wavbuf[MP3_WAVBUFSIZE];
} Mp3CompressorWin32;

static bool g_acm_error;

TSP_PUBLIC bool Mp3CompressorHadError() {
  return g_acm_error;
}

Mp3Compressor *Mp3CompressorCreate(const TspSampleFormat *format, int bitrate) {
  MMRESULT mmr;
  unsigned long rawbufsize = 0;
  Mp3CompressorWin32 *mp3;
  WAVEFORMATEX waveFormat[1];
  MPEGLAYER3WAVEFORMAT mp3format[1];

  mp3 = (Mp3CompressorWin32 *)HeapAlloc(GetProcessHeap(), 0, sizeof(Mp3CompressorWin32));
  memset(mp3, 0, sizeof(Mp3CompressorWin32));
  
  // define desired input format
  waveFormat->wFormatTag = WAVE_FORMAT_PCM;
  waveFormat->nChannels = format->channels; // stereo
  waveFormat->nSamplesPerSec = format->sample_rate; // 44.1kHz
  waveFormat->wBitsPerSample = 16; // 16 bits
  waveFormat->nBlockAlign = waveFormat->nChannels * 2; // 4 bytes of data at a time are useful (1 sample)
  waveFormat->nAvgBytesPerSec = waveFormat->nBlockAlign * waveFormat->nSamplesPerSec; // byte-rate
  waveFormat->cbSize = 0; // no more data to follow

  // define MP3 output format
  mp3format->wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
  mp3format->wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
  mp3format->wfx.nChannels = 2;
  mp3format->wfx.nAvgBytesPerSec = bitrate * (1000 / 8);
  mp3format->wfx.wBitsPerSample = 0;
  mp3format->wfx.nBlockAlign = 1;
  mp3format->wfx.nSamplesPerSec = format->sample_rate;
  mp3format->wID = MPEGLAYER3_ID_MPEG;
  mp3format->fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
  mp3format->nBlockSize = (144 * bitrate * 1000 / format->sample_rate);
  mp3format->nFramesPerBlock = 1;
  mp3format->nCodecDelay = 1393;
  
  mp3->mp3stream = NULL;
  mmr = acmStreamOpen(&mp3->mp3stream,               // open an ACM conversion stream
                     NULL,                       // querying all ACM drivers
                     waveFormat,                 // from WAV
                     (LPWAVEFORMATEX) mp3format, // to  MP3
                     NULL, 0,0,0);
  if (mmr != MMSYSERR_NOERROR) {
    g_acm_error = true;
    return 0;
  }
  
  // find out how big the decompressed buffer will be
  mmr = acmStreamSize(mp3->mp3stream, sizeof(mp3->wavbuf), &rawbufsize, ACM_STREAMSIZEF_SOURCE);
  if (mmr != MMSYSERR_NOERROR) {
    Mp3CompressorDestroy(&mp3->base);
    return NULL;
  }

  // allocate our I/O buffers
  mp3->base.mp3buf = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, rawbufsize);
  mp3->base.wavbuf = mp3->wavbuf;
  mp3->base.wavbuf_capacity = MP3_WAVBUFSIZE;
  
  // prepare the decoder
  ZeroMemory(&mp3->mp3streamHead, sizeof(ACMSTREAMHEADER ) );
  mp3->mp3streamHead.cbStruct = sizeof(ACMSTREAMHEADER );
  mp3->mp3streamHead.pbSrc = mp3->wavbuf;
  mp3->mp3streamHead.cbSrcLength = sizeof(mp3->wavbuf);
  mp3->mp3streamHead.pbDst = (byte*)mp3->base.mp3buf;
  mp3->mp3streamHead.cbDstLength = rawbufsize;
  mmr = acmStreamPrepareHeader( mp3->mp3stream, &mp3->mp3streamHead, 0 );
  if (mmr != MMSYSERR_NOERROR) {
    Mp3CompressorDestroy(&mp3->base);
    return NULL;
  }
  return &mp3->base;
}

void Mp3CompressorDestroy(Mp3Compressor *mp3i) {
  Mp3CompressorWin32 *mp3 = (Mp3CompressorWin32 *)mp3i;
  HeapFree(GetProcessHeap(), 0, mp3->base.mp3buf);
  if (mp3->mp3stream) acmStreamClose(mp3->mp3stream, 0);
  HeapFree(GetProcessHeap(), 0, mp3);
}

bool Mp3CompressorCompress(Mp3Compressor *mp3i, int flags) {
  Mp3CompressorWin32 *mp3 = (Mp3CompressorWin32 *)mp3i;
  MMRESULT mmr;
  mp3->mp3streamHead.cbSrcLengthUsed = mp3->base.wavbuf_size;
  mp3->mp3streamHead.cbDstLengthUsed = 0;
  mmr = acmStreamConvert(mp3->mp3stream, &mp3->mp3streamHead, ACM_STREAMCONVERTF_BLOCKALIGN);
  mp3->base.mp3buf_size = mp3->mp3streamHead.cbDstLengthUsed;
  return (mmr == MMSYSERR_NOERROR);
}

#endif  // defined(OS_WIN) && TSP_WITH_MP3_COMPRESSOR

