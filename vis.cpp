#include "stdafx.h"
#include "tiny_spotify.h"
#include "spotifyamp.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <Windows.h>

#pragma warning (disable: 4244)
#pragma warning (disable: 4305)

WindowHandle g_visualizer_wnd;

struct Filter {
  float a0, a1, a2, a3, a4;
  float x1, x2, y1, y2;

  float tx1, tx2, ty1, ty2;

  float val;
};
#ifndef M_LN2
#define M_LN2	   0.69314718055994530942
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

static int UpdateFilter(int sample, struct Filter *b) {
  float input = sample;
  float output = b->a0 * input + b->a1 * b->x1 + b->a2 * b->x2 -  b->a3 * b->y1 - b->a4 * b->y2;
  b->x2 = b->x1;
  b->x1 = input;
  b->y2 = b->y1;
  b->y1 = output;

  input = output;
  output = b->a0 * input + b->a1 * b->tx1 + b->a2 * b->tx2 -  b->a3 * b->ty1 - b->a4 * b->ty2;
  b->tx2 = b->tx1;
  b->tx1 = input;
  b->ty2 = b->ty1;
  b->ty1 = output;

  b->val = (b->val * 0.99f) + fabs(output) * 0.01f;
  return 0;
}

/* filter types */
enum {
    LPF, /* low pass filter */
    HPF, /* High pass filter */
    BPF, /* band pass filter */
    NOTCH, /* Notch Filter */
    PEQ, /* Peaking band EQ filter */
    LSH, /* Low shelf filter */
    HSH /* High shelf filter */
};

static void BiQuad_new(struct Filter *b, int type, float dbGain, float freq, float srate, float bandwidth) {
    float A, omega, sn, cs, alpha, beta;
    float a0, a1, a2, b0, b1, b2;

    /* setup variables */
    A = pow(10, dbGain /40);
    omega = 2 * M_PI * freq /srate;
    sn = sin(omega);
    cs = cos(omega);
    alpha = sn * sinh(M_LN2 /2 * bandwidth * omega /sn);
    beta = sqrt(A + A);

    switch (type) {
    case LPF:
        b0 = (1 - cs) /2;
        b1 = 1 - cs;
        b2 = (1 - cs) /2;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case HPF:
        b0 = (1 + cs) /2;
        b1 = -(1 + cs);
        b2 = (1 + cs) /2;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case BPF:
        b0 = alpha;
        b1 = 0;
        b2 = -alpha;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case NOTCH:
        b0 = 1;
        b1 = -2 * cs;
        b2 = 1;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case PEQ:
        b0 = 1 + (alpha * A);
        b1 = -2 * cs;
        b2 = 1 - (alpha * A);
        a0 = 1 + (alpha /A);
        a1 = -2 * cs;
        a2 = 1 - (alpha /A);
        break;
    case LSH:
        b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
        b1 = 2 * A * ((A - 1) - (A + 1) * cs);
        b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
        a0 = (A + 1) + (A - 1) * cs + beta * sn;
        a1 = -2 * ((A - 1) + (A + 1) * cs);
        a2 = (A + 1) + (A - 1) * cs - beta * sn;
        break;
    case HSH:
        b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
        b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
        a0 = (A + 1) - (A - 1) * cs + beta * sn;
        a1 = 2 * ((A - 1) - (A + 1) * cs);
        a2 = (A + 1) - (A - 1) * cs - beta * sn;
        break;
    }
    /* precompute the coefficients */
    b->a0 = b0 /a0;
    b->a1 = b1 /a0;
    b->a2 = b2 /a0;
    b->a3 = a1 /a0;
    b->a4 = a2 /a0;
    /* zero initial samples */
    b->x1 = b->x2 = 0;
    b->y1 = b->y2 = 0;
}

static struct Filter f[19];

struct State {
  uint8 value[19];
  uint8 maximums[19];
};

static HBITMAP bmp_vis;
static unsigned int *pixels_vis;
int vis_pause;

#define MK(a,b,c) ((a<<16)|(b<<8)|(c))


unsigned int viscolors[24];

extern const unsigned int default_viscolors[24];
const unsigned int default_viscolors[24] = {
  MK(0,0,0), // color 0 = black
  MK(24,33,41), // color 1 = grey for dots
  MK(239,49,16), // color 2 = top of spec
  MK(206,41,16), // 3
  MK(214,90,0), // 4
  MK(214,102,0), // 5
  MK(214,115,0), // 6
  MK(198,123,8), // 7
  MK(222,165,24), // 8
  MK(214,181,33), // 9
  MK(189,222,41), // 10
  MK(148,222,33), // 11
  MK(41,206,16), // 12
  MK(50,190,16), // 13
  MK(57,181,16), // 14
  MK(49,156,8),  // 15
  MK(41,148,0),  // 16
  MK(24,132,8),   // 17 = bottom of spec
  MK(255,255,255), // 18 = osc 1
  MK(214,214,222), // 19 = osc 2 (slightly dimmer)
  MK(181,189,189), // 20 = osc 3
  MK(160,170,175),  // 21 = osc 4
  MK(148,156,165),  // 22 = osc 4
  MK(150, 150, 150), // 23 = analyzer peak dots
};


static HBITMAP CreateDibSect( int cxImage, int cyImage, int nBitsPerPixel, void **ppImageBits ) {
  BITMAPINFO BitmapInfo;
  memset( &BitmapInfo, 0, sizeof(BitmapInfo) );
  BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  BitmapInfo.bmiHeader.biWidth = cxImage;
  BitmapInfo.bmiHeader.biHeight = -cyImage;
  BitmapInfo.bmiHeader.biPlanes = 1;
  BitmapInfo.bmiHeader.biBitCount = nBitsPerPixel;

  HDC hDC = GetDC( NULL );
  HBITMAP hBitmap = CreateDIBSection( hDC, &BitmapInfo, DIB_RGB_COLORS, ppImageBits, NULL, 0 );
  ReleaseDC( NULL, hDC );

  return hBitmap;
}

// This stores the state of the visualizer
struct VisState {
  uint32 timestamp;
  float values[19];
  int8 lastsamp[76];
};

struct VisData {
  unsigned char spectrumData[2][576];
  unsigned char waveformData[2][576];
};

VisData *FindVisData(unsigned int timestamp);

static float peaks[19];
static float peaksvel[19];
static int total;
#define VISCOUNT 64
static VisState vis_state[64];
static int vis_read, vis_write;
static int base_timestamp;
int vis_time_adjustment;
int vismode;
#define FRAMERATE 60

static CRITICAL_SECTION vis_cs;
static DWORD WINAPI ThreadRunner(PVOID arg) {
  for(;;) {
    VisState vs_curr, *vs_found = NULL;
    int best_diff = 200;
    uint32 now = PlatformGetTicks() + vis_time_adjustment;
    static uint32 last;

    int i = 0;
   
    EnterCriticalSection(&vis_cs);

    do {
      VisState *vs = &vis_state[(i + vis_read) & (VISCOUNT - 1)];
      int diff = (int)(vs->timestamp - now);
      if (abs(diff) < best_diff) {
        best_diff = abs(diff);
        vs_found = vs;
        vis_read = (i + vis_read) & (VISCOUNT - 1);
      } else if (vs_found != NULL) {
        break;
      }
    } while (++i != VISCOUNT);
    if (vs_found) {
//       printf("Now=%5d (%+2d) %5d %5d\n", now, now-last, vis_read, vis_write&63);
       vs_curr = *vs_found;
    } else {
//       printf("XX Now=%d (+%d)\n", now, now-last);
      memset(&vs_curr, 0, sizeof(vs_curr));
    }
        last=now;
    LeaveCriticalSection(&vis_cs);

    uint32 bkcolor = viscolors[0];
    for(int i = 0; i < 76*16; i++)
      pixels_vis[i] = bkcolor;

    if (vismode == 0) {
      for(int i = 0; i < 19; i++) {
        float u = 20.0*log10(vs_curr.values[i] / 32768.0 + 1e-16); // -90 to 0
        u = u * 16 / 32.0 + 26.0f;
        int v = 15 - Clamp((int)u, -1, 15);

        for(int j = 15; j >= v; j--) {
          unsigned int color = viscolors[2+j];
          pixels_vis[j * 76 + i * 4 + 0] = color;
          pixels_vis[j * 76 + i * 4 + 1] = color;
          pixels_vis[j * 76 + i * 4 + 2] = color;
        }
        if (peaksvel[i] < 0)
          peaks[i] += peaksvel[i];
        peaksvel[i] -= 0.05;
        if (u >= peaks[i]) {
          peaks[i] = u;
          peaksvel[i] = FRAMERATE*0.05;
        }
        if (peaks[i] > 0) {
          v = Clamp((int)peaks[i], 0, 15);
          unsigned int color = viscolors[23];
          pixels_vis[(15-v) * 76 + i * 4 + 0] = color;
          pixels_vis[(15-v) * 76 + i * 4 + 1] = color;
          pixels_vis[(15-v) * 76 + i * 4 + 2] = color;
        }
      }
    } else {
      for(int i = 0;  i < 76; i++) {
        int y = Clamp(vs_curr.lastsamp[i] + 8, 0, 15);
        pixels_vis[y * 76 + i] = 0xffffff;
      }
    }
    if (g_visualizer_wnd) {
      HDC dc = GetDC(g_visualizer_wnd);
      HDC bmpdc = CreateCompatibleDC(dc);
      RECT rect;
      GetClientRect(g_visualizer_wnd, &rect);
      SelectObject(bmpdc, bmp_vis);
      StretchBlt(dc, 0, 0, rect.right, rect.bottom, bmpdc, 0, 0, 76, 16, SRCCOPY);
      DeleteDC(bmpdc);
      ReleaseDC(g_visualizer_wnd, dc);
    }

    do {
      Sleep(1000/FRAMERATE);
    } while(vis_pause);
  }
  return 0;
}

void InitVisualizer() {
  bmp_vis = CreateDibSect(76, 16, 32, (void**)&pixels_vis);
  memset(pixels_vis, 0, 76*16*4);
  DWORD thread_id;
  InitializeCriticalSection(&vis_cs);
  CloseHandle(CreateThread(NULL, 0, &ThreadRunner, NULL, 0, &thread_id));
}

void InsertVisDataSamples(unsigned int timestamp, const int16 *samp, int nch);

static TspSampleType ebuff[576*2];

void UpdateEqualizer(const TspSampleType *ptr, int count, int buff) {
  int i,j;
  static bool inited;
  
#define COPY_INTERVAL (44100 / FRAMERATE)

  if (!inited) {
    inited = true;
    float freq = 1.0;
    for(j = 0; j < 19; j++) {
      BiQuad_new(&f[j], BPF, 0, (int)(50 * freq + 0.5f), 44100, 0.1);
      freq *= 1.3868094922572831;
    }
  }
  int p = 0;
  
  for(;;) {
    int n = IntMin(COPY_INTERVAL - (total), (count - p) >> 1) * 2;
    for(i = 0; i < n; i += 2) {
      int v = (ptr[i + 0] + ptr[i + 1]) >> 1;
//      static int q;
//      v = 16384 * sin(2 * 3.1415 * 1000 * q++ / 44100);
      for(j = 0; j < 19; j++)
        UpdateFilter(v, &f[j]);
    }
    ptr += n;
    p += n;
    total += (n >> 1);
    
    if (total == COPY_INTERVAL) {
      total = 0;

      // If not enough linear samples available. Combine the new samples with the previously
      // buffered samples.
      const TspSampleType *linear_ptr = ptr;
      if (p < 576 * 2) {
        memcpy(ebuff + 576 * 2 - p, ptr - p, p * sizeof(TspSampleType));
        linear_ptr = ebuff + 576 * 2;
      }

      uint32 current_time = p * 1000 / (44100 * 2);
      EnterCriticalSection(&vis_cs);
      current_time += base_timestamp;
      if (count >= 76 * 2) {
        VisState *vs = &vis_state[vis_write++ & (VISCOUNT - 1)];
        vs->timestamp = current_time;
        for(i = 0; i < 19; i++)
          vs->values[i] = f[i].val;
     
        for(i = 0; i < 76; i++)
          vs->lastsamp[i] = (linear_ptr[i - 76 * 2 + 0] + linear_ptr[i - 76 * 2 + 1]) >> 13;
      }
      LeaveCriticalSection(&vis_cs);

      InsertVisDataSamples(current_time, linear_ptr - 576 * 2, 2);
      memset(ebuff, 0x55, sizeof(ebuff));
    }
    if (p == count)
      break;
  }

  vis_time_adjustment = base_timestamp + (count - buff) * 1000 / (44100 * 2) - PlatformGetTicks();
  base_timestamp += count * 1000 / (44100 * 2);

  // If less than 576*2 samples left until next frame grab, it means
  // we'll need bytes from this buffer on the next call, so buffer samples.
  int n;
  if ((n = (COPY_INTERVAL - total) * 2) < 576 * 2) {
    int offs = 576 * 2 - n;
    int to_copy = count > offs ? offs : count;
    memcpy(ebuff + offs - to_copy, ptr - to_copy, to_copy * sizeof(TspSampleType));
  }
}

