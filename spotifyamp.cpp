// spotifyamp.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "spotifyamp.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
extern "C" {
#include "shoutcast.h"
};
#include "commands.h"
#include "dialogs.h"
#include "appkey.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif  // MAX_PATH

#define kEllipsis 1
#define arraysize(x) (sizeof(x)/sizeof(x[0]))

static void MyCallback(void *context, TspCallbackEvent event, void *source, void *param);
extern "C" bool WavInit(Tsp *tsp);
extern "C" void WavSetVolume(int vol);
extern "C" int WavGetVolume();
extern "C" int WavPush(void *context, int flags,
                       const TspSampleType *datai, int sizei,
                       const TspSampleFormat *sample_format, int *samples_buffered);
extern "C" void WavSetDeviceId(int id);
extern "C" int WavGetDeviceId();

void CheckUpdate();
void DoAutoUpdateStuff(PlatformWindow *w);
extern "C" bool Mp3CompressorHadError();
const char *VisPlugin_Iterate(int i);
void VisPlugin_StartStop(int i);
void VisPlugin_Config(int i);
void VisPlugin_Load(int i);
void InitVisualizer();

extern char machine_id[128];
extern char exepath[MAX_PATH];

const char *url_to_open;
extern WindowHandle g_visualizer_wnd;
static byte glyphs[26], glyphx[26];

Resources res;
static MainWindow main_window;
static PlaylistWindow playlist_window(&main_window);
static EqWindow eq_window(&main_window);
static CoverArtWindow coverart_window(&main_window);

static int tsp_iteration;
static Shoutcast *g_sc;

void UpdateEqualizer(const TspSampleType *ptr, int count, int buff);

extern int vis_pause;

static const char * const toplistnames[][2] = {
  {"Everywhere", "spotify:toplist:track:everywhere"},
  {"For Me",     "spotify:toplist:track:forme"},
  {NULL, NULL},
  {"in Belgium", "spotify:toplist:track:BE"},
  {"in Denmark", "spotify:toplist:track:DK"},
  {"in Finland", "spotify:toplist:track:FI"},
  {"in France", "spotify:toplist:track:FR"},
  {"in Germany", "spotify:toplist:track:DE"},
  {"in Ireland", "spotify:toplist:track:IE"},
  {"in Italy", "spotify:toplist:track:IT"},
  {"in the Netherlands", "spotify:toplist:track:NL"},
  {"in Norway", "spotify:toplist:track:NO"},
  {"in Spain", "spotify:toplist:track:ES"},
  {"in Sweden", "spotify:toplist:track:SE"},
  {"in the United Kingdom", "spotify:toplist:track:GB"},
  {"in the United States", "spotify:toplist:track:US"},
};

int MyWavPush(void *context, int flags,
              const TspSampleType *datai, int sizei,
              const TspSampleFormat *sample_format, int *samples_buffered) {
  int r;
  if (g_sc && ShoutcastNumClients(g_sc)) {
    r = ShoutcastWavPush(g_sc, flags, datai, sizei, sample_format, samples_buffered);
  } else {
    r = WavPush(context, flags, datai, sizei, sample_format, samples_buffered);
    if (r > 0)
      UpdateEqualizer(datai, r, *samples_buffered);
  }
  vis_pause = (flags & kTspAudioFlag_Pause);
  return r;
}

struct SkinInfo {
  const char *name;
  const char *filename;
};

static SkinInfo skininfo[100];

static void Skin_Index() {
  static bool skins_indexed;
  if (skins_indexed) return;
  skins_indexed = true;
  char file[MAX_PATH + 100];
  int num_skins = 0;
  sprintf(file, "%sSkins", exepath);
  FileEnumerator enumerator(file);
  while (num_skins < 100 && enumerator.Next()) {
    skininfo[num_skins].filename = strdup(enumerator.filename());
    // Strip the extension component if this entry is a filename.
    if (!enumerator.is_directory()) {
      char *t = enumerator.filename() + strlen(enumerator.filename());
      while (t > enumerator.filename() && t[-1] != '\\') {
        if (t[-1] == '.') { t[-1] = 0; break; }
        t--;
      }
    }
    skininfo[num_skins].name = strdup(enumerator.filename());
    num_skins++;
  }
}

static const char *Skin_Enumerate(int i) {
  Skin_Index();
  return (i < 0 || i >= 100) ? NULL : skininfo[i].name;
}

static const char *Skin_GetFileName(int i) {
  return (i < 0 || i >= 100) ? NULL : skininfo[i].filename;
}

const ButtonRect main_buttons[] = {
  {16, 88, 23, 18}, // 0 Prev
  {39, 88, 23, 18}, // 1 Play
  {62, 88, 23, 18}, // 2 Pause
  {85, 88, 23, 18}, // 3 Stop
  {108, 88, 22, 18}, // 4 Next
  {136, 89, 22, 16}, // 5 Eject

  {164, 89, 46, 15}, // 6 Shuffle
  {210, 89, 28, 15}, // 7 Repeat

  {219, 58, 23, 12}, // 8 Equalizer
  {242, 58, 23, 12}, // 9 Playlist

  {6, 3, 9, 9}, // 10 Main Menu
  {244, 3, 9, 9}, // 11 Minimize Button
  {254, 3, 9, 9}, // 12 Make Compact
  {264, 3, 9, 9}, // 13 Close

  {12, 23, 9, 9}, // 14 O
  {12, 32, 9, 8}, // 15 A
  {12, 40, 9, 8}, // 16 I
  {12, 48, 9, 8}, // 17 D
  {12, 56, 9, 8}, // 18 V

  {37, 25, 63, 15}, // 19 Toggle time
};

const ButtonRect compact_buttons[] = {
  {168, 2, 8, 11}, // 0 Prev
  {176, 2, 10,11}, // 1 Play
  {186, 2, 10, 11}, // 2 Pause
  {196, 2, 9, 11}, // 3 Stop
  {205, 2, 9, 11}, // 4 Next
  {215, 2, 9, 11}, // 5 Eject
  {0,0,0,0},
  {0,0,0,0},
  {0,0,0,0},
  {0,0,0,0},
  {6, 3, 9, 9},   // 10 Main Menu
  {244, 3, 9, 9}, // 11 Expand Button
  {254, 3, 9, 9}, // 12 Make Compact
  {264, 3, 9, 9}, // 13 Close
  {0,0,0,0},  // 14
  {0,0,0,0},  // 15
  {0,0,0,0},  // 16
  {0,0,0,0},  // 17
  {0,0,0,0},  // 18
  {127, 4, 30, 7}, // 19 Toggle time
};

MainWindow::MainWindow() {
  lbutton_down_ = false;
  hover_button_ = -1;
  compact_ = true;
  stereo_ = true;
  in_dialog_ = false;
  connect_error_ = false;
  global_hotkeys_ = false;
  coverart_visible_ = false;
  itemlist_in_sync_with_player_ = true;
  playlist_window_ = &playlist_window;
  eq_window_ = &eq_window;
  tsp_ = NULL;
  bitrate_ = kTspBitrate_Normal;
  delayed_command_ = 0;
}

MainWindow::~MainWindow() {

}

void MainWindow::InitThreading() {}

bool GetMachineId(char buf[64]);

extern unsigned int viscolors[24];
extern const unsigned int default_viscolors[24];
static uint32 g_color_normal, g_color_current;
static uint32 g_color_bg_normal, g_color_bg_selected;
static std::string g_playlist_font;

char *for_each_line(char **ptr);

char *strbegins(const char *big, const char *little) {
  int l = strlen(little);
  if (strnicmp(big, little, l) == 0)
    return (char*)big + l;
  return NULL;
}

static uint32 ParseColor(const char *s) {
  if (*s == '#') s++;
  uint32 rv = strtoul(s, NULL, 16);

  return (rv & 0xff0000)>>16 | (rv & 0xff00) | (rv & 0xff) << 16;
}

static void LoadSkin(const char *skinfile) {
  char file[MAX_PATH+100];
  if (skinfile && skinfile[0]) {
    sprintf(file, "%sSkins\\%s", exepath, skinfile);
    PlatformSetSkin(file);
  } else {
    PlatformSetSkin(NULL);
  }
  PlatformLoadBitmap(&res.main, "main.bmp");
  PlatformLoadBitmap(&res.cbuttons, "cbuttons.bmp");
  PlatformLoadBitmap(&res.shufrep, "shufrep.bmp");
  PlatformLoadBitmap(&res.monoster, "monoster.bmp");
  PlatformLoadBitmap(&res.titlebar, "titlebar.bmp");
  PlatformLoadBitmap(&res.text, "text.bmp");
  if (!PlatformLoadBitmap(&res.numbers, "nums_ex.bmp"))
    PlatformLoadBitmap(&res.numbers, "numbers.bmp");
  PlatformLoadBitmap(&res.volume, "volume.bmp");
  PlatformLoadBitmap(&res.posbar, "posbar.bmp");
  PlatformLoadBitmap(&res.playpaus, "playpaus.bmp");
  PlatformLoadBitmap(&res.pledit, "pledit.bmp");
  PlatformLoadBitmap(&res.eqmain, "eqmain.bmp");
  PlatformLoadBitmap(&res.gen, "gen.bmp");

  memcpy(viscolors,default_viscolors,sizeof(viscolors));
  std::string vis_str;
  if (PlatformLoadString("viscolor.txt", &vis_str)) {
    char *lines = &vis_str[0];
    int i = 0;
    while (char *s = for_each_line(&lines)) {
      int r = 0, g = 0, b = 0;
      sscanf(s, "%d,%d,%d", &r, &g, &b);
      viscolors[i] = (r<<16)|(g<<8)|b;
      if (++i==24) break;
    }
  }

  g_color_normal = 0x00ff00;
  g_color_current = 0xffffff;
  g_color_bg_normal = 0x000000;
  g_color_bg_selected = 0xff0000;
  g_playlist_font = "Arial";

  if (PlatformLoadString("pledit.txt", &vis_str)) {
    char *lines = &vis_str[0];
    while (char *s = for_each_line(&lines)) {
      if (char *t = strbegins(s, "Normal=")) {
        g_color_normal = ParseColor(t);
      } else if (char *t = strbegins(s, "Current=")) {
        g_color_current = ParseColor(t);
      } else if (char *t = strbegins(s, "NormalBG=")) {
        g_color_bg_normal = ParseColor(t);
      } else if (char *t = strbegins(s, "SelectedBG=")) {
        g_color_bg_selected = ParseColor(t);
      } else if (char *t = strbegins(s, "Font=")) {
        g_playlist_font = t;
      }
    }
  }

}

#ifdef _DEBUG
static int start_ticks;
void TspDprintf(const char *s, ...) {
  va_list va;
  int millis = GetTickCount() - start_ticks;
  va_start(va, s);
  fprintf(stderr, "[%5d] [%d.%.3d] ", tsp_iteration, millis / 1000, millis % 1000);
  vfprintf(stderr, s, va);
  fputc('\n', stderr);
  va_end(va);
}
#endif

// This is called on the disk thread, to notify the main thread that there are pending
// disk events.
static void FileIOThreadNotify(void *user_data) {
  MainWindow *mw = (MainWindow *)user_data;
  mw->NotifyIOEvents();
}

void MainWindow::PumpIOEvents() {
//  TspOs *os = TspGetOs(tsp_);
//  os->PumpIOEvents(os);
}

bool MainWindow::Load() {
#ifndef _DEBUG
   CheckUpdate();
#else
  start_ticks = GetTickCount();
  TspSetDebugLogger(&TspDprintf);
#endif

  TspMemoryLimits limits = {0};
  limits.track_player_size = 200;
  limits.compressed_buffer_size = 1024 * 2048;

  tsp_ = TspCreate(machine_id,
                   MyCallback, this,
                   g_appkey, sizeof(g_appkey),
                   "Spotiamb " VERSION_STR,
                   kTspCreate_DownloadRootlist, &limits, NULL);

  if (!tsp_) {
    return false;
  }

//  TspOs *os = TspGetOs(tsp_);
//  os->EnableFileIOMultithreading(os, &FileIOThreadNotify, this);
  
  item_list_ = TspItemListCreate(tsp_, limits.track_player_size);
  player_list_ = TspPlayerGetItemList(tsp_);
  
  char *username = strdup(PrefReadStr("", "username"));
  if (*username) {
    const char *password = PrefReadStr("", "password");
    if (*password &&
        TspLogin(tsp_, username, password, kTspCredentialType_Token) >= 0)
      username_ = username;
  }
  free(username);

  TspSetDisplayName(tsp_, "Spotiamb " VERSION_STR);
  TspSetAudioCallback(tsp_, &MyWavPush, NULL);
  TspPlayerSetDevice(tsp_, kTspDeviceIdAuto, 0);

  WavInit(tsp_);
  TspPlayerSetVolume(tsp_, WavGetVolume());
  InitThreading();
  RegisterForGlobalHotkeys();

  bool compact = PrefReadBool(false, "compact");
  compact_ = !compact;
  SetCompact(compact);
  equalizer_ = PrefReadBool(false, "equalizer");
  g_easy_move = PrefReadBool(true, "easy_move");
  time_mode_ = PrefReadBool(false, "time_mode");
  playlist_ = PrefReadBool(false, "playlist");
  bitrate_ = (TspBitrate)PrefReadInt(bitrate_, "bitrate");
  shoutcast_ = PrefReadBool(false, "shoutcast");
  curr_vis_plugin_ = PrefReadInt(0, "vis_plugin_index");
  coverart_visible_ = PrefReadBool(false, "albumart");
  PlatformWindow::SetAlwaysOnTop(PrefReadBool(false, "ontop"));
  SetDoubleSize(PrefReadBool(false, "double_size"));
  TspPlayerSetShuffle(tsp_, PrefReadBool(false, "shuffle"));
  TspPlayerSetRepeat(tsp_, PrefReadBool(false, "repeat"));
  WavSetDeviceId(PrefReadInt(-1, "wave_device"));
  TspSetTargetBitrate(tsp_, bitrate_);
  curr_skin_ = PrefReadStr("", "skin");
  LoadSkin(curr_skin_.c_str());

  EnableGlobalHotkeys(PrefReadBool(false, "global_hotkeys"));

  if (url_to_open == NULL) {
    const char *uri = PrefReadStr("", "now_playing.uri");
    if (*uri) {
      int now_playing = PrefReadInt(-1, "now_playing.index");
      if (now_playing < 0) now_playing = 0;
      TspItemListLoad(player_list_, uri, IntMax(now_playing-50, 0));
      TspItemListLoad(item_list_, uri, IntMax(now_playing - 50, 0));
      TspPlayerSetNowPlayingIndex(tsp_, now_playing);
    }
  } else {
    OpenUri(url_to_open, -2);
  }
  return true;
}

void MainWindow::SaveNowPlaying() {
  PrefWriteStr(TspItemListGetUri(player_list_), "now_playing.uri");
  PrefWriteInt(TspItemListGetNowPlayingIndex(player_list_), "now_playing.index");
}

void MainWindow::Quit() {
  PrefWriteInt(g_easy_move, "easy_move");
  PrefWriteInt(time_mode_, "time_mode");
  PlatformWindow::Quit();
}

void MainWindow::LeftButtonDown(int x, int y) {
  lbutton_down_ = true;
  MouseMove(x, y);

  if (hover_button_ == -2) {
    dragging_text_ = true;
  } else if (hover_button_ == -3) {
    dragging_vol_delta_ = (unsigned)(x - volume_pixel_) <= 14 ? (x - volume_pixel_) : 7;
    dragging_vol_ = true;
    MouseMove(x, y);
  } else if (hover_button_ == -4) {
    dragging_seek_delta_ = (unsigned)(x - seek_pixel_) <= 29 ? (x - seek_pixel_) : (29 / 2);
    dragging_seek_ = true;
    seek_target_ = -1;
    MouseMove(x, y);
  } else if (hover_button_ < 0) {
    lbutton_down_ = false;
  }
}

void MainWindow::LeftButtonDouble(int x, int y) {
  if (y < 14) {
    int b = FindButton(x,y);
    if (b < 0) {
      SetCompact(!compact_);
      return;
    }
    if (b == 10) {
      ButtonClicked(13);
      return;
    }
  }
  LeftButtonDown(x, y);
}

void MainWindow::LeftButtonUp(int x, int y) {
  int b = hover_button_ >= 0 ? FindButton(x, y) : -1;

  if (dragging_seek_) {
    dragging_seek_ = false;
    if (seek_target_ >= 0)
      TspPlayerSeek(tsp_, seek_target_ * 1000);
  }

  lbutton_down_ = false;
  dragging_text_ =  false;
  dragging_seek_ = false;
  dragging_vol_ = false;
  MouseMove(x, y);
  Repaint();
  if (b >= 0)
    ButtonClicked(b);
}

void MainWindow::RightButtonDown(int x, int y) {
  ShowOptionsMenu();
}

void MainWindow::MouseMove(int x, int y) {
  int over = FindButton(x, y);

  int b = lbutton_down_ ? over : -1;
  if (b != hover_button_) {
    hover_button_ = b;
    Repaint();
  }

  SetWindowDragable(over == -1 && (g_easy_move || y < 14));

  {
    int dx = x - mouse_last_x_;
    int dy = y - mouse_last_y_;
    mouse_last_x_ = x;
    mouse_last_y_ = y;

    if (dragging_text_ && dx) {
      text_scroll_ -= dx;
      text_scroll_delay_ = 10;
      Repaint();
    }

    if (dragging_vol_) {
      int p = x - dragging_vol_delta_ - 107;
      if (p <= 0) p = 0;
      if (p >= 51) p = 51;
      // 51 * 1285 is exactly 65535.
      TspPlayerSetVolume(tsp_, p * 1285);
      Repaint();
    }

    if (dragging_seek_) {
      int p = x - dragging_seek_delta_ - 16;
      if (p < 0) p = 0;
      if (p > 218) p = 218;
      int dur = TspPlayerGetDuration(tsp_);
      if (dur > 0) {
        int t = p * dur / 218;
        if (t != seek_target_) {
          seek_target_ = t;
          Repaint();
        }
      }
    }
  }
}

void MainWindow::MouseWheel(int x, int y, int dx, int dy) {
  if (dy < 0) {
    Perform(CMD_VOLUME_DOWN);
    Perform(CMD_VOLUME_DOWN);
  } else if (dy > 0) {
    Perform(CMD_VOLUME_UP);
    Perform(CMD_VOLUME_UP);
  }
}

int MainWindow::FindButton(int x, int y) {
  int i;
  for(i = 0; i < buttons_count_; i++) {
    if ( (unsigned)(x - buttons_[i].x) < (unsigned)buttons_[i].w &&
         (unsigned)(y - buttons_[i].y) < (unsigned)buttons_[i].h)
      return i;
  }
  if (x >= 128 && y >= 23 && x <= 268 && y <= 37)
    return -2;

  if (IsInside(x, 107, 68+1) && IsInside(y, 57, 14+1))
    return -3;

  if ((unsigned)(x - 16) <= 249 && (unsigned)(y - 72) <= 10)
    return -4;

  return -1;
}

static const unsigned char charmap[128] = {
  30,30,30,30, 30,30,30,30, 30,30,30,30, 30,30,30,30, // 0x00
  30,30,30,30, 30,30,30,30, 30,30,30,30, 30,30,30,30, // 0x10
  30,48,26,61, 60,57,56,47, 44,45,30,50, 58,46,42,52, // 0x20
  31,32,33,34, 35,36,37,38, 39,40,43,30, 30,59,30,30, // 0x30
  30, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14, // 0x40
  15,16,17,18, 19,20,21,22, 23,24,25,53, 51,54,55,49, // 0x50
  47, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14, // 0x60
  15,16,17,18, 19,20,21,22, 23,24,25,30, 30,30,66,30, // 0x70
};

static int PaintText(PlatformWindow *w, const char *text, int scroll, int flags,
                     int left, int top, int width, int height) {
  unsigned char b;
  int glyph;
  int x = left - scroll;
  int count = 0;
  const char *text_org = text;
  while (x < left + width) {
    b = *text++;
    if (!b) {
      if (scroll == 0)
        break;
      text = text_org;
      b = *text++;
      if (!b)
        break;

      count = 0;
    }
    if (x >= left) {
      glyph = (b <= 127) ? charmap[b] : 30;

      // Ellipsis?
      if ((flags & kEllipsis) && x + 5 >= left + width)
        glyph = 41;

      int ww = IntMin(5, left + width - x);
      w->Blit(x, top, ww, 6, res.text, (glyph % 31) * 5, (glyph / 31) * 6);
    }
    x += 5;
    count++;
  }
  return left;
}

void MainWindow::PaintBigNumber(int num, int x, int y) {
  int d0 = num / 10, d1 = num % 10;
  Blit(x, y, 9, 13, res.numbers, d0 * 9, 0);
  x += 12;
  Blit(x, y, 9, 13, res.numbers, d1 * 9, 0);
}

void MainWindow::PaintSmallNumber(int num, int x, int y) {
  char buf[32];
  sprintf(buf, "%.2d", num);
  PaintText(this, buf, 0, 0, x, y, 20, 6);
}

void MainWindow::PaintCompact() {
  const ButtonRect *buttons = buttons_;
  Bitmap *curbmp;

  // Title bar
  curbmp = res.titlebar;
  Blit(0, 0, 275, 14,     curbmp, 27, active() ? 29 : 42);

  buttons += 10;
  // Buttons in title bar
  if (hover_button_ == 10)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, curbmp, 0, 9);
  buttons++;
  if (hover_button_ == 11)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, curbmp, 9, 9);
  buttons++;
  if (hover_button_ == 12)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, curbmp, 9, 18);
  buttons++;
  if (hover_button_ == 13)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, curbmp, 18, 9);
  buttons++;

  // Time
  TspPlayState state = TspPlayerGetState(tsp_);

  if (state != kTspPlayState_Stopped && position_indicator_ != INT_MIN) {
    int v = abs(position_indicator_);
    if (position_indicator_ < 0)
      Blit(130, 7, 3, 1, res.text, 54, 11);
    PaintSmallNumber((v / 60) % 100, 134, 4);
    PaintSmallNumber(v % 60, 147, 4);
  }
}

static const char *ResolveError(TspError error) {
  switch(error) {
  case kTspErrorPremiumRequired: return "PREMIUM REQUIRED";
  case kTspErrorBadCredentials: return "BAD USERNAME/PASSWORD";
  }
  return NULL;
}

void MainWindow::PaintFull() {
  char string[1024];
  Bitmap *srcbmp;

  Blit(0, 0, width(), height(), res.main, 0, 0);

  const ButtonRect *buttons = buttons_;
  int srcx = 0;

  // Main buttons
  for(int i = 0; i < 6; i++) {
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, 
            res.cbuttons, srcx, (hover_button_ == i) * buttons->h);
    srcx += buttons->w;
    buttons++;
  }

  bool shuffle = TspPlayerGetShuffle(tsp_) != 0;
  bool repeat = TspPlayerGetRepeat(tsp_) != 0;
  // Shuffle
  srcbmp = res.shufrep;
  Blit(buttons->x, buttons->y, buttons->w, buttons->h, 
          srcbmp, 28, ((hover_button_ == 6) + shuffle * 2) * buttons->h);
  buttons++;

  // Repeat
  Blit(buttons->x, buttons->y, buttons->w, buttons->h, 
          srcbmp, 0, ((hover_button_ == 7) + repeat * 2) * buttons->h);
  buttons++;

  // Equalizer
  Blit(buttons->x, buttons->y, buttons->w, buttons->h, 
          srcbmp, ((hover_button_ == 8) * 2) * buttons->w, 61 + equalizer_ * buttons->h);
  buttons++;

  // Playlist
  Blit(buttons->x, buttons->y, buttons->w, buttons->h, 
          srcbmp, ((hover_button_ == 9) * 2 + 1) * buttons->w, 61 + playlist_ * buttons->h);
  buttons++;

  // Mono / Stereo Indicator
  srcbmp = res.monoster;
  Blit(212, 41, 28, 12,  srcbmp, 29, mono_ ? 0 : 12);
  Blit(239, 41, 29, 12,  srcbmp, 0, stereo_ ? 0 : 12);

  // Title bar
  srcbmp = res.titlebar;
  Blit(0, 0, 275, 14,     srcbmp, 27, active() ? 0 : 15);

  // Buttons in title bar
  if (hover_button_ == 10)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, srcbmp, 0, 9);
  buttons++;
  if (hover_button_ == 11)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, srcbmp, 9, 9);
  buttons++;
  if (hover_button_ == 12)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, srcbmp, 9, 18);
  buttons++;
  if (hover_button_ == 13)
    Blit(buttons->x, buttons->y, buttons->w, buttons->h, srcbmp, 18, 9);
  buttons++;

  // OAIDU
  if (hover_button_ >= 14 && hover_button_ <= 18) {
    Blit(10, 22, 8, 43, srcbmp, 304 + (hover_button_ - 14) * 8, 44);
  } else {
    Blit(10, 22, 8, 43, srcbmp, 304, 0);
  }
  if (double_size())    Blit(10, 22 + 26, 8, 7, srcbmp, 328, 70);
  if (always_on_top()) Blit(10, 22 + 10, 8, 7, srcbmp, 312, 54);

  {
    int br = TspPlayerGetBitrate(tsp_);
    if (br)
      PaintSmallNumber(br, 111, 43);
  }
  PaintSmallNumber(44, 156, 43);

  TspItem *item = TspPlayerGetNowPlaying(tsp_);
  int duration = TspPlayerGetDuration(tsp_);
  int position = TspPlayerGetPosition(tsp_);
  TspPlayState state = TspPlayerGetState(tsp_);

  if (state != kTspPlayState_Stopped && position_indicator_ != INT_MIN) {
    int v = abs(position_indicator_);
    if (position_indicator_ < 0)
      Blit(38, 32, 5, 1, res.numbers, 83, 6);
    PaintBigNumber((v / 60) % 100, 48, 26);
    PaintBigNumber(v % 60, 78, 26);
  }

  const char *text = NULL;

  if (hover_button_ == 14) text = "Options Menu";
  else if (hover_button_ == 15) text = always_on_top() ? "Disable Always-On-Top" : "Enable Always-On-Top";
  else if (hover_button_ == 16) text = "File Info Box";
  else if (hover_button_ == 17) text = double_size() ? "Disable Doublesize Mode" : "Enable Doublesize Mode";
  else if (hover_button_ == 18) text = "Visualization Menu";

  if (username_.empty()) {
    text = "Please Login First   ~~~  ";
  } else {
    if (!TspIsConnected(tsp_)) {
      TspError error = TspGetConnectionError(tsp_);
      if (error != 0 && error != kTspErrorForcedDisconnect) {
        text = string;
        const char *err = ResolveError(error);
        if (err)
          sprintf(string, "Login Error %d: %s   ~~~  ", error, err);
        else
          sprintf(string, "Login Error %d   ~~~  ", error);
      } else {
        text = "Logging In...   ~~~  ";
      }
    }
  }

  if (dragging_seek_ && duration > 0) {
    text = string;
    sprintf(string, "SEEK TO: %.2d:%.2d/%.2d:%.2d (%d%%)", 
            seek_target_ / 60 % 100, seek_target_ % 60,
            duration / 60 % 100, duration % 60,
            (seek_target_ * 100 + (duration>>1)) / duration);
  } else if (dragging_vol_) {
    text = string;
    int v = TspPlayerGetVolume(tsp_);
    sprintf(string, "VOLUME: %d%%", (v * 100 + (65535/2)) / 65535);
  }

  if (!text) {
    int idx = TspItemListGetNowPlayingIndex(player_list_);
    if (item) {
      int d = (duration < 0) ? 0 : duration;
      sprintf(string, "%d. %s - %s  (%d:%.2d)  ~~~  ", idx + 1, TspItemGetName(item), TspItemGetArtistName(item), (d / 60) % 99, d % 60);
      text = string;
    } else {
      text = "CLICK OPEN TO SEE PLAYLISTS";
    }
  }

  if (text) {
    static char lastmsg[16];
    if (strncmp(lastmsg, text, sizeof(lastmsg))) {
      strncpy(lastmsg, text, 16);
      ResetScroll();
    }

    int l = strlen(text) * 5;
    if (l <= 155) {
      text_scroll_ = 0;
    } else {
      while (text_scroll_ < 0)
        text_scroll_ += l * 100;
      text_scroll_ %= l;
    }
    PaintText(this, text, text_scroll_, 0, 111, 27, 154, 6);
  }

  // Volume slider
  int volume = TspPlayerGetVolume(tsp_);
  int volbmp = volume / 2341;
  Blit(107, 57, 68, 13, res.volume, 0, volbmp * 15);
  // 107 - 158
  int volidx = 107 + ((volume + (1285/2)) / 1285);
  Blit(volidx, 58, 14, 11, res.volume, dragging_vol_ ? 0 : 15, 422);
  volume_pixel_ = volidx;

  // Main seek bar
  // Range = 219
  if (state != kTspPlayState_Stopped) {
    int seekpos = dragging_seek_ ? seek_target_ : position;
    int seekpix = 16 + (duration ? (seekpos * 219 + (duration / 2)) / duration : 0);
    Blit(seekpix, 72, 29, 10, res.posbar, dragging_seek_ ? 278 : 248, 0);
    seek_pixel_ = seekpix;
  }

  // Playpaus
  Blit(26, 28, 9, 9, res.playpaus, state == kTspPlayState_Stopped ? 18 : state == kTspPlayState_Paused ? 9 : 0, 0);
}

void MainWindow::Paint() {
  if (compact_) {
    PaintCompact();
  } else {
    PaintFull();
  }
}

void MainWindow::SetDoubleSize(bool v) {
  PlatformWindow::SetDoubleSize(v);
  eq_window_->SetDoubleSize(v);
  PrefWriteInt(v, "double_size");
  SetVisualizer(24, 43, 76, 16);
}

void MainWindow::SetAlwaysOnTop(bool v) {
  PlatformWindow::SetAlwaysOnTop(v);
  PrefWriteInt(v, "ontop");
}

static const char all_genres[] = 
  "Alternative\0"
  "Black Metal\0"
  "Blues\0"
  "Classical\0"
  "Christmas\0"
  "Country\0"
  "Dance\0"
  "Death Metal\0"
  "Electronic\0"
  "Emo\0"
  "Folk\0"
  "Hardcore\0"
  "Heavy Metal\0"
  "Hip-Hop\0"
  "Indie\0"
  "Jazz\0"
  "Latin\0"
  "Pop\0"
  "Punk\0"
  "R&&B\0"
  "Reggae\0"
  "Rock\0"
  "Singer-Songwriter\0"
  "Soul\0"
  "Trance\0"
  "60s\0"
  "70s\0"
  "80s\0"
  "90s\0"
  "00s\0";

static const char *GetRadioGenreId(int i) {
  const char *p = all_genres;
  while (i) {
    do p++; while(p[-1]);
    if (!*p) return NULL;
    i--;
  }
  return p;
}

const char *GetRadioUri(int i) {
  static char buf[256];
  char *d;
  const char *s;
  strcpy(buf, "spotify:genre:");
  d = buf + sizeof("spotify:genre:") - 1;
  s = GetRadioGenreId(i);
  for(;;) {
    char c = *s++;
    if (!c) break;
    if (c >= 'A' && c <= 'Z') c += 32;
    if (c == '&') {
      *d++ = '_';
      *d++ = 'n';
      *d++ = '_';
      s++;
      continue;
    } else if(c == ' ' || c == '/' || c == '-') {
      *d++ = '_';
      continue;
    }
    *d++ = c;
  }
  *d = 0;
  return buf;
}

void MainWindow::KeyDown(int key, int shift) {
  if (key == 'V' && shift == MOD_CONTROL)           Perform(CMD_PLAY_CLIPBOARD);
  else if (key == 'D' && shift == MOD_CONTROL)    Perform(CMD_DOUBLE_SIZE);
  else if (key == 'A' && shift == MOD_CONTROL)    Perform(CMD_ALWAYS_ON_TOP);
  else if (key == 'W' && shift == MOD_CONTROL)    Perform(CMD_COMPACT);
  else if (key == 'L' && shift == MOD_CONTROL)    Perform(CMD_LOGIN);
  else if (key == 'T' && shift == MOD_CONTROL)    Perform(CMD_TIME_TOGGLE);
  else if (key == 'E' && shift == MOD_CONTROL)    Perform(CMD_EASYMOVE);
  else if (key == 'S' && shift == MOD_CONTROL)    Perform(CMD_SEARCH);
  else if (key == 'G' && shift == MOD_ALT)        Perform(CMD_EQ_TOGGLE);
  else if (key == 'P' && shift == MOD_ALT)        Perform(CMD_PLAYLIST_TOGGLE);
  else if (key == 'A' && shift == MOD_ALT)        Perform(CMD_TOGGLE_ALBUMART);
  else if (shift == 0 && key == 'S')    Perform(CMD_SHUFFLE);
  else if (shift == 0 && key == 'R')    Perform(CMD_REPEAT);
  else if (shift == 0 && key == ' ')   Perform(CMD_PLAYPAUSE);
  else if (shift == 0 && key == 'Z')   Perform(CMD_PREV);
  else if (shift == 0 && key == 'X')   Perform(CMD_PLAY);
  else if (shift == 0 && key == 'C')   Perform(CMD_PAUSE);
  else if (shift == 0 && key == 'V')   Perform(CMD_STOP);
  else if (shift == 0 && key == 'B')   Perform(CMD_NEXT);
  else if (shift == 0 && key == 'L')   delayed_command_ = CMD_OPENFILE;
  else if (shift == 0 && key == VK_LEFT) Perform(CMD_REWIND);
  else if (shift == 0 && key == VK_RIGHT) Perform(CMD_FORWARD);
  else if (shift == 0 && key == VK_UP) Perform(CMD_VOLUME_UP);
  else if (shift == 0 && key == VK_DOWN) Perform(CMD_VOLUME_DOWN);
  else if (key == 'K' && shift == (MOD_SHIFT|MOD_CONTROL)) Perform(CMD_VISUALIZER_STARTSTOP);
}

void MainWindow::ButtonClicked(int b) {
  if (b == 0) Perform(CMD_PREV);  
  else if (b == 1) Perform(CMD_PLAY);
  else if (b == 2) Perform(CMD_PAUSE);
  else if (b == 3) Perform(CMD_STOP);
  else if (b == 4) Perform(CMD_NEXT);
  else if (b == 5) Perform(CMD_OPENFILE);
  else if (b == 6) Perform(CMD_SHUFFLE);
  else if (b == 7) Perform(CMD_REPEAT);
  else if (b == 8) Perform(CMD_EQ_TOGGLE);
  else if (b == 9) Perform(CMD_PLAYLIST_TOGGLE);
  else if (b == 10) Perform(CMD_OPTIONS);
  else if (b == 11) Perform(CMD_MINIMIZE);
  else if (b == 12) Perform(CMD_COMPACT);
  else if (b == 13) Perform(CMD_EXIT);
  else if (b == 14) Perform(CMD_OPTIONS);
  else if (b == 15) Perform(CMD_ALWAYS_ON_TOP);
  else if (b == 17) Perform(CMD_DOUBLE_SIZE);
  else if (b == 18) Perform(CMD_VISUALIZER_MENU);
  else if (b == 19) Perform(CMD_TIME_TOGGLE);
}

void MainWindow::Perform(int cmd) {
  switch(cmd) {
  case CMD_PLAY_CLIPBOARD:
    if (const char *s = PlatformReadClipboard())
      TspPlayerPlayContext(tsp_, s, NULL, 0);
    break;
  case CMD_DOUBLE_SIZE: SetDoubleSize(!double_size()); break;
  case CMD_ALWAYS_ON_TOP: SetAlwaysOnTop(!always_on_top()); break;
  case CMD_COMPACT: SetCompact(!compact_); break;
  case CMD_LOGIN: ShowLoginDialog(); break;
  case CMD_TIME_TOGGLE: time_mode_ = !time_mode_; break;
  case CMD_TIME_ELAPSED:time_mode_ = false; break;
  case CMD_TIME_REMAINS:time_mode_ = true; break;
  case CMD_EASYMOVE: g_easy_move ^= true; break;
  case CMD_TOGGLE_ALBUMART: SetCoverartVisible(!coverart_visible_); break;
  case CMD_SEARCH: ShowSearchDialog(); break;
  case CMD_EQ_TOGGLE: SetEqVisible(!equalizer_); break;
  case CMD_PLAYLIST_TOGGLE: SetPlaylistVisible(!playlist_); break;
  case CMD_SHUFFLE: TspPlayerSetShuffle(tsp_, !TspPlayerGetShuffle(tsp_)); break;
  case CMD_REPEAT: TspPlayerSetRepeat(tsp_, !TspPlayerGetRepeat(tsp_)); break;
  case CMD_PREV: TspPlayerPrevTrack(tsp_); break;
  case CMD_PLAY: TspPlayerPlay(tsp_); break;
  case CMD_PLAYPAUSE:
  case CMD_PAUSE: {
    TspPlayState state = TspPlayerGetState(tsp_);
    if (state == kTspPlayState_Playing)
      TspPlayerPause(tsp_);
    else if (state == kTspPlayState_Paused || cmd == CMD_PLAYPAUSE)
      TspPlayerPlay(tsp_);
    } break;
  case CMD_STOP: TspPlayerStop(tsp_); break;
  case CMD_NEXT: TspPlayerNextTrack(tsp_); break;
  case CMD_REWIND: TspPlayerSeek(tsp_, TspPlayerGetPositionMs(tsp_) - 5000); break;
  case CMD_FORWARD: TspPlayerSeek(tsp_, TspPlayerGetPositionMs(tsp_) + 5000); break;
  case CMD_VOLUME_UP: TspPlayerSetVolume(tsp_, TspPlayerGetVolume(tsp_) + 1025); break;
  case CMD_VOLUME_DOWN: TspPlayerSetVolume(tsp_, TspPlayerGetVolume(tsp_) - 1025); break;
  
  case CMD_OPENFILE: ShowEjectMenu(); break;
  case CMD_MINIMIZE: Minimize(); break;
  case CMD_EXIT: Quit(); break;
  case CMD_OPTIONS: ShowOptionsMenu(); break;
  case CMD_VISUALIZER_MENU: ShowVisualizeMenu(); break;
  case CMD_VISUALIZER_STARTSTOP: VisPlugin_StartStop(curr_vis_plugin_); break;
  case CMD_VISUALIZER_CONFIG: VisPlugin_Config(curr_vis_plugin_); break;
  case CMD_BRINGTOTOP: BringWindowToTop(); break;
  case CMD_STARRED:
    SetPlaylistVisible(true);
    playlist_window_->MakeActive();
    OpenUri(TspGetStarredUri(tsp_, NULL), -2);
    break;
  case CMD_BITRATE_LO:
  case CMD_BITRATE_MED:
  case CMD_BITRATE_HI:
    bitrate_ = (TspBitrate)(cmd - CMD_BITRATE_LO);
    TspSetTargetBitrate(tsp_, bitrate_);
    PrefWriteInt(bitrate_, "bitrate");
    break;
  case CMD_SHOUTCAST:
    shoutcast_ = !shoutcast_;
    PrefWriteInt(shoutcast_, "shoutcast");
    break;
  case CMD_ABOUT: {
    char mesg[4096];
    const char *ver = TspGetVersionString() + sizeof("Spotiamb-") - 1;
    sprintf(mesg, "Spotiamb  (version %s)\r\nCopyright \xa9 2013, Spotify\r\n\r\nThis program is a tribute to Winamp :)\r\n\r\n"
      "Thanks to the fantastic team at the Spotify Gothenburg office!\r\nThis program was made by Ludde.\r\n\r\nFor assistance, visit #Spotiamb on EFnet.", ver);
    MsgBox(mesg, "About Spotiamb", MB_ICONINFORMATION);
    break;
  }
  case CMD_LEGAL0:
    MsgBox("Spotiamb uses third party source code based on the BSD license:\r\nLibtremor - Copyright (c) 2002, Xiph.org Foundation", "Spotiamb Open Source Software", MB_ICONINFORMATION);
    break;
  case CMD_LEGAL1:
    OpenUrl("https://www.spotify.com/legal/end-user-agreement/");
    break;
  case CMD_LEGAL2:
    OpenUrl("https://www.spotify.com/legal/privacy-policy/");
    break;
  case CMD_GLOBAL_HOTKEYS:
    EnableGlobalHotkeys(!global_hotkeys_);
    PrefWriteInt(global_hotkeys_, "global_hotkeys");
    break;
  default:
    if (cmd >= CMD_RADIO && cmd <= CMD_RADIO + 4095) {
      OpenUri(GetRadioUri(cmd - CMD_RADIO), -1);
    } else if (cmd >= CMD_PLAYLIST && cmd <= CMD_PLAYLIST + 4095) {
      SetPlaylistVisible(true);
      playlist_window_->MakeActive();
      OpenUri(pluris_[cmd - CMD_PLAYLIST].c_str(), -2);
    } else if (cmd >= CMD_SKIN && cmd <= CMD_SKIN + 0xfff) {
      const char *s = Skin_GetFileName(cmd - CMD_SKIN - 1 );
      curr_skin_ = s ? s : "";
      PrefWriteStr(curr_skin_.c_str(), "skin");
      LoadSkin(curr_skin_.c_str());
      glyphs[0] = 0;
      eq_window.OnSkinLoaded();
      for(PlatformWindow *w = g_platform_windows; w; w = w->next())
        w->Repaint();
    } else if (cmd >= CMD_VISUALIZER && cmd < CMD_VISUALIZER + 256) {
      curr_vis_plugin_ = cmd - CMD_VISUALIZER;
      VisPlugin_Load(curr_vis_plugin_);
      PrefWriteInt(curr_vis_plugin_, "vis_plugin_index");
    } else if ((cmd>>12) == (CMD_OUTPUT_DEVICE>>12)) {
      WavSetDeviceId(cmd - CMD_OUTPUT_DEVICE - 1);
      PrefWriteInt(cmd - CMD_OUTPUT_DEVICE - 1, "wave_device");
    } else if ((cmd>>12) == (CMD_TOPLIST>>12)) {
      SetPlaylistVisible(true);
      playlist_window_->MakeActive();
      OpenUri(toplistnames[cmd-CMD_TOPLIST][1], -2);
    } else if ((cmd >> 12) == (CMD_REMOTE_DEVICE >> 12)) {
      const char *which = (cmd == CMD_REMOTE_DEVICE) ? NULL : remote_device_ids_[cmd - CMD_REMOTE_DEVICE - 1].c_str();
      TspPlayerSetDevice(tsp_, which, kTspSetDeviceFlag_MovePlayback);
    }
  }
}

void MainWindow::SetCompact(bool v) {
  if (v != compact_) {
    compact_ = v;
    if (!v) {
      buttons_ = main_buttons;
      buttons_count_ = sizeof(main_buttons) / sizeof(main_buttons[0]);
    } else {
      buttons_ = compact_buttons;
      buttons_count_ = sizeof(compact_buttons) / sizeof(compact_buttons[0]);
    }
    PrefWriteInt(compact_, "compact");
    Resize(WND_MAIN_W, v ? 14 : WND_MAIN_H);
  }
}

void MainWindow::SetPositionIndicator(int pos) {
  if (pos != position_indicator_) {
    position_indicator_ = pos;
    if (compact_) {
      RepaintRange(127, 4, 30, 7);
    } else {
      RepaintRange(37, 25, 64, 15);
      RepaintRange(16, 72, 249, 10);
    }
  }
}

void MainWindow::SetPlaylistVisible(bool v) {
  if (v != playlist_) {
    playlist_ = v;
    playlist_window_->SetVisible(playlist_);
    PrefWriteInt(playlist_, "playlist");
    Repaint();
  }
}

void MainWindow::SetCoverartVisible(bool v) {
  if (v != coverart_visible_) {
    coverart_visible_ = v;
    PrefWriteInt(coverart_visible_, "albumart");
    coverart_window.SetVisible(v);
  }
}


void MainWindow::SetEqVisible(bool v) {
  if (v != equalizer_) {
    equalizer_ = v;
    eq_window_->SetVisible(equalizer_);
    PrefWriteInt(equalizer_, "equalizer");
    Repaint();
  }
}

void MainWindow::MainLoop() {
  tsp_iteration++;
  TspDoWork(tsp_);
  int indicator;
  if (time_mode_) {
    int dur = TspPlayerGetDurationMs(tsp_);
    if (dur < 0) {
      indicator = INT_MIN;
    } else {
      indicator = -(dur - TspPlayerGetPositionMs(tsp_)) / 1000;
    }
  } else {
    indicator = TspPlayerGetPosition(tsp_);
  }

  SetPositionIndicator(indicator);
  TspWaitForNetworkEvents(tsp_, 10);

  if (shoutcast_ != (g_sc != NULL)) {
    if (shoutcast_) {
      g_sc = ShoutcastCreate(tsp_);
      ShoutcastListen(g_sc, 5010);
    } else {
      ShoutcastDestroy(g_sc);
      g_sc = NULL;
    }
  }

  if (!in_dialog_) {
    if (delayed_command_) {
      int x = delayed_command_;
      delayed_command_ = 0;
      Perform(x);
    }

    in_dialog_ = true;

    static bool g_login_shown;
    if (!g_login_shown) {
      g_login_shown = true;

      if (username_.empty())
        ShowLoginDialog();
    }

    if (connect_error_) {
      connect_error_ = false;
      TspError error = TspGetConnectionError(tsp_);
      if (error == kTspErrorBadCredentials) {
        MsgBox("Bad username / password.", "Spotiamb", MB_ICONEXCLAMATION);
        ShowLoginDialog();
      } else if (error == kTspErrorPremiumRequired) {
        int r = MsgBox("You need Spotify Premium to use Spotiamb.\r\nDo you want to sign up now?", "Spotiamb", MB_ICONEXCLAMATION | MB_YESNOCANCEL);
        if (r == IDYES)
          OpenUrl("http://spotiamb.com/premium/");
        if (r != IDCANCEL)
          ShowLoginDialog();
      }
    }

    static bool g_showed_mp3_error;
    if (Mp3CompressorHadError() && !g_showed_mp3_error) {
      g_showed_mp3_error = true;
      if (MsgBox("There was a problem starting the MP3 codec. Do you want to read a guide?", "Spotiamb", MB_ICONEXCLAMATION | MB_OKCANCEL) == IDOK) {
        OpenUrl("http://spotiamb.com/mp3guide.html");
      }
    }

    DoAutoUpdateStuff(this);
    in_dialog_ = false;
  } else {
    delayed_command_ = 0;
  }
}

void MainWindow::ScrollText() {
  if (!dragging_text_ && !compact_) {
    if (text_scroll_delay_) {
      text_scroll_delay_--;
    } else {
      text_scroll_ += 5;
      RepaintRange(111, 27, 154, 6);
    }
  }
}

bool MainWindow::HandleAppCommandHotKeys(int cmd, int keys) {
  static uint32 last_hot_key_press = 0;
  uint32 now = PlatformGetTicks();
  uint32 diff = last_hot_key_press - now;
  if (diff < 30)
    return true;
  last_hot_key_press = now;
  switch (cmd) {
  case APPCOMMAND_MEDIA_PLAY_PAUSE:
    Perform(CMD_PLAYPAUSE);
    return true;
  case APPCOMMAND_MEDIA_PLAY:
    Perform(CMD_PLAY);
    return TspPlayerGetState(tsp_) == kTspPlayState_Playing || TspPlayerIsLoading(tsp_);
  case APPCOMMAND_MEDIA_NEXTTRACK:
    Perform(CMD_NEXT);
    return true;
  case APPCOMMAND_MEDIA_PREVIOUSTRACK:
    Perform(CMD_PREV);
    return true;
  case APPCOMMAND_MEDIA_STOP:
    if (TspPlayerGetState(tsp_) != kTspPlayState_Stopped) {
      Perform(CMD_STOP);
      return true;
    }
    return false;
  case APPCOMMAND_LAUNCH_MEDIA_SELECT:
    Perform(CMD_BRINGTOTOP);
    return true;
  default:
    return false;
  }
}

void MainWindow::OpenUri(const char *query, int track_to_play) {
  if (memcmp(query, "spotify:", 8) != 0 && query[0] != '<')
    query = TspGetSearchUri(tsp_, query);
  if (track_to_play == -2) {
    itemlist_in_sync_with_player_ = false;
    TspItemListLoad(item_list_, query, 0);
  } else {
    itemlist_in_sync_with_player_ = true;
    TspPlayerPlayContext(tsp_, query, NULL, track_to_play);
  }
}

void MainWindow::AppendPlaylistPopupMenu(MenuBuilder &menu, int base_id,
                                         const char *cur_uri, bool only_writable) {
  std::vector<int> grp;
  int more_count = 0, toplevel_count = 0;
  pluris_.clear();
  int rootlist_count = TspGetRootlistCount(tsp_);
  for(int i = 0; i < rootlist_count; i++) {
    TspItemType type = TspGetRootlistType(tsp_, i);
    int indent = TspGetRootlistIndent(tsp_, i);
    while ((int)grp.size() > indent) {
      menu.EndSubMenu(TspGetRootlistName(tsp_, grp.back()));
      grp.pop_back();
    }

    bool is_folder = (type == kTspItemType_ClosedFolder || type == kTspItemType_OpenFolder);

    if (only_writable && !is_folder && !(TspGetRootlistUriInfo(tsp_, i) & kTspUriInfo_Writable))
      continue;

    // Menu gets too large?
    if (indent == 0 && ++toplevel_count >= 30) {
      toplevel_count = 0;
      more_count++;
      menu.AddSeparator();
      menu.BeginSubMenu();
    }

    if (is_folder) {
      grp.push_back(i);
      menu.BeginSubMenu();
    } else {
      const char *rootlist_name = TspGetRootlistName(tsp_, i);
      const char *uri = TspGetRootlistUri(tsp_, i);
      bool checked = cur_uri && *cur_uri && !strcmp(uri, cur_uri);
      pluris_.emplace_back(uri);
      menu.AddRadioItem(base_id + pluris_.size() - 1, rootlist_name, checked);
    }
  }
  while (!grp.empty()) {
    menu.EndSubMenu(TspGetRootlistName(tsp_, grp.back()));
    grp.pop_back();
  }
  while (more_count--)
    menu.EndSubMenu("More");
}

void MainWindow::ShowEjectMenu() {
  MenuBuilder menu;
  std::string cur_uri = TspItemListGetUri(item_list_);
  if (cur_uri.empty())
    cur_uri = "?";
  menu.AddRadioItem(CMD_STARRED, "Starred", cur_uri == TspGetStarredUri(tsp_, NULL));
  // Menu of all radio channels
  menu.BeginSubMenu();
  const char *s;
  for(int i = 0; (s = GetRadioGenreId(i)) != NULL; i++) {
    menu.AddRadioItem(CMD_RADIO + i, s, stricmp(GetRadioUri(i), cur_uri.c_str())==0);
  }
  menu.EndSubMenu("Radio");
  menu.BeginSubMenu();
  for(int i = 0; i < arraysize(toplistnames); i++) {
    if (toplistnames[i][0]==0)
      menu.AddSeparator();
    else
      menu.AddRadioItem(CMD_TOPLIST + i, toplistnames[i][0], 
          toplistnames[i][1] && toplistnames[i][1] == cur_uri);
  }
  menu.EndSubMenu("Top Lists");
  menu.AddSeparator();
  menu.AddItem(CMD_SEARCH, "Search...\tCtrl+S");
  menu.AddSeparator();
  menu.AddItem(0, "[My Playlists]", MenuBuilder::kGrayed);
  AppendPlaylistPopupMenu(menu, CMD_PLAYLIST, cur_uri.c_str(), false);
  Perform(menu.Popup(this));
}

void MainWindow::ShowOptionsMenu() {
  MenuBuilder menu;

  bool repeat = TspPlayerGetRepeat(tsp_) != 0;
  bool shuffle = TspPlayerGetShuffle(tsp_) != 0;

  menu.AddItem(CMD_LOGIN, username_.empty() ? "Login...\tCtrl+L" : ("Log Out " + username_ + "...\tCtrl+L").c_str());
  menu.AddSeparator();

  menu.BeginSubMenu();
    menu.AddRadioItem(CMD_BITRATE_LO, "Low bitrate (96 kbit)", bitrate_ == kTspBitrate_Low);
    menu.AddRadioItem(CMD_BITRATE_MED, "Normal bitrate (160 kbit)", bitrate_ == kTspBitrate_Normal);
    menu.AddRadioItem(CMD_BITRATE_HI, "High bitrate (320 kbit)", bitrate_ == kTspBitrate_High);
    menu.AddSeparator();
    menu.AddCheckItem(CMD_SHOUTCAST, "Enable Shoutcast Server (on port 5010)", shoutcast_);
    menu.AddCheckItem(CMD_GLOBAL_HOTKEYS, "Enable Global Hotkeys", global_hotkeys_);
    menu.AddSeparator();
    menu.BeginSubMenu();
    int active_device = WavGetDeviceId();
    menu.AddRadioItem(CMD_OUTPUT_DEVICE, "(Default Device)", (active_device == -1));
    {
      const char *s;
      for(int i = 0; (s = PlatformEnumAudioDevices(i)) != NULL; i++) {
        if (*s)
          menu.AddRadioItem(CMD_OUTPUT_DEVICE + i + 1, s, (active_device == i));
      }
    }
    menu.EndSubMenu("Output Device");
  menu.EndSubMenu("Settings");

  menu.BeginSubMenu();
  int i = 0;
  menu.AddRadioItem(CMD_SKIN + 0, "Base Skin", curr_skin_.empty());
  while (const char *s = Skin_Enumerate(i)) {
    bool active = curr_skin_ == Skin_GetFileName(i++);
    menu.AddRadioItem(CMD_SKIN + i, s, active);
  }
  menu.EndSubMenu("Skins");

  if (TspGetRemoteDeviceName(tsp_, 0) != NULL) {
    menu.BeginSubMenu();
    int active_dev = TspGetActiveRemoteDevice(tsp_);
    const char *name;
    remote_device_ids_.clear();
    menu.AddRadioItem(CMD_REMOTE_DEVICE, "Local Playback", active_dev == -1);
    menu.AddSeparator();
    for (int i = 0; (name = TspGetRemoteDeviceName(tsp_, i)) != NULL; i++) {
      menu.AddRadioItem(CMD_REMOTE_DEVICE + i + 1, name, i == active_dev);
      remote_device_ids_.emplace_back(TspGetRemoteDeviceId(tsp_, i));
    }
    menu.EndSubMenu("Playing Device");
  }

  menu.AddSeparator();
  menu.AddRadioItem(CMD_TIME_ELAPSED, "Time elapsed\tCtrl+T toggles", !time_mode_);
  menu.AddRadioItem(CMD_TIME_REMAINS, "Time remaining\tCtrl+T toggles", time_mode_);
  menu.AddSeparator();
  menu.AddCheckItem(CMD_ALWAYS_ON_TOP, "Always On Top\tCtrl+A", always_on_top());
  menu.AddCheckItem(CMD_DOUBLE_SIZE, "Double Size\tCtrl+D", double_size());
  menu.AddCheckItem(CMD_EASYMOVE, "EasyMove\tCtrl+E", g_easy_move);
  menu.AddSeparator();
  menu.AddCheckItem(CMD_TOGGLE_ALBUMART, "Album Art\tAlt+A", coverart_visible_);
  menu.AddSeparator();
  menu.AddCheckItem(CMD_REPEAT, "Repeat\tR", repeat);
  menu.AddCheckItem(CMD_SHUFFLE, "Shuffle\tS", shuffle);
  menu.AddSeparator();
  menu.AddItem(CMD_ABOUT, "About...");

  menu.BeginSubMenu();
    menu.AddItem(CMD_LEGAL0, "Open Source Software");
    menu.AddItem(CMD_LEGAL1, "End User Agreement");
    menu.AddItem(CMD_LEGAL2, "Privacy Policy");
  menu.EndSubMenu("Legal Stuff");

  menu.AddSeparator();
  menu.AddItem(CMD_EXIT, "Exit");
//  CheckMenuRadioItem(menu, CMD_TIME_ELAPSED, CMD_TIME_REMAINS, time_mode_?CMD_TIME_REMAINS:CMD_TIME_ELAPSED, MF_BYCOMMAND);
  Perform(menu.Popup(this));
}

void MainWindow::ShowVisualizeMenu() {
  MenuBuilder menu;
  menu.AddItem(CMD_VISUALIZER_STARTSTOP, "Start / Stop plug-in\tCtrl+Shift+K");
  menu.AddItem(CMD_VISUALIZER_CONFIG, "Configure plug-in");
  menu.AddSeparator();
  int i = 0;
  const char *s;
  for(; (s = VisPlugin_Iterate(i)) != NULL; i++)
    menu.AddRadioItem(CMD_VISUALIZER + i, s, i == curr_vis_plugin_);
  if (i == 0)
    menu.AddItem(0, "(no plugins found)", MenuBuilder::kGrayed);
  Perform(menu.Popup(this));
}

void MainWindow::ShowSearchDialog() {
  std::string query = ::ShowSearchDialog(this, tsp_);
  if (query.size()) {
    SetPlaylistVisible(true);
    playlist_window_->MakeActive();
    OpenUri(TspGetSearchUri(tsp_, query.c_str()), -2);
  }
}

void MainWindow::ShowLoginDialog() {
  std::string username = username_;
  std::string password;
  if (::ShowLoginDialog(this, &username, &password)) {
    Login(username.c_str(), password.c_str());
  }
}

void MainWindow::Login(const char *username, const char *password) {
  username_.clear();
  if (TspLogin(tsp_, username, password, kTspCredentialType_Password) >= 0) {
    PrefWriteStr("", "username");
    if (*username)
      username_ = username;
  }
}

struct HotkeyInfo {
  uint16 key;
  uint16 modifier;
  uint16 cmd;
};

static const HotkeyInfo hotkeys[] = {
  {VK_INSERT, MOD_CONTROL|MOD_ALT, CMD_PLAY},
  {VK_HOME, MOD_CONTROL|MOD_ALT, CMD_PAUSE},
  {VK_END, MOD_CONTROL|MOD_ALT, CMD_STOP},
  {VK_PRIOR, MOD_CONTROL|MOD_ALT, CMD_PREV},
  {VK_NEXT, MOD_CONTROL|MOD_ALT, CMD_NEXT},
  {VK_UP, MOD_CONTROL|MOD_ALT, CMD_VOLUME_UP},
  {VK_DOWN, MOD_CONTROL|MOD_ALT, CMD_VOLUME_DOWN},
  {VK_RIGHT, MOD_CONTROL|MOD_ALT, CMD_FORWARD},
  {VK_LEFT, MOD_CONTROL|MOD_ALT, CMD_REWIND},
  {'L', MOD_CONTROL|MOD_ALT, CMD_OPENFILE},
  {'S', MOD_CONTROL|MOD_ALT, CMD_SEARCH},
};

void MainWindow::EnableGlobalHotkeys(bool enable) {
#if defined(OS_WIN) && !defined(WITH_SDL)
  if (global_hotkeys_ == enable) return;
  global_hotkeys_ = enable;
  for(int i = 0; i < arraysize(hotkeys); i++) {
    const HotkeyInfo *hi = &hotkeys[i];
    if (enable)
      RegisterHotKey(handle(), hi->cmd, hi->modifier, hi->key);
    else
      UnregisterHotKey(handle(), hi->cmd);
  }
#endif
}

void MainWindow::GlobalHotkey(int id) {
  Perform(id);
}

///////////////////////////////////////////////////////////
// PLAYLIST WINDOW
///////////////////////////////////////////////////////////
PlaylistWindow::PlaylistWindow(MainWindow *main_window) {
  main_window_ = main_window;
  hover_button_ = -1;
  Resize(WND_MAIN_W, WND_MAIN_H * 3);
}

PlaylistWindow::~PlaylistWindow() {

}

void PlaylistWindow::Load() {
  SetCompact(PrefReadBool(false, "pl.compact"));
  font_size_ = PrefReadInt(10, "pl.font_size");
  row_height_ = 13;
}

static ButtonRect playlist_window_buttons[] = {
  {-21, 2, 9, 11},  // 0 = expand / contract
  {-12, 2, 11, 11}, // 1 = close
  {-20, -20, 20, 20}, // 2 = size
};

static ButtonRect playlist_window_buttons_compact[] = {
  {-21, 2, 9, 11},  // 0 = expand / contract
  {-12, 2, 11, 11}, // 1 = close
  {-29, 2, 8, 11},  // 2 = size
};


int PlaylistWindow::FindButton(int x, int y) {
  int i;
  ButtonRect *buttons = !compact_ ? playlist_window_buttons : playlist_window_buttons_compact;
  int num_buttons = !compact_ ? arraysize(playlist_window_buttons) :
                                arraysize(playlist_window_buttons_compact);
  for(i = 0; i < num_buttons; i++) {
    int bx = buttons[i].x;
    int by = buttons[i].y;
    if (bx < 0) bx += width();
    if (by < 0) by += height();
    if ( (unsigned)(x - bx) < (unsigned)buttons[i].w &&
         (unsigned)(y - by) < (unsigned)buttons[i].h)
      return i;
  }

  if (!compact_) {
    // Scrollbar
    if (IsInside(x, width()-15, 8) && IsInside(y, 20, height()-58))
      return -2;

    // Item list
    if (IsInside(x, 11, width() - 11 - 19) && IsInside(y, 20, height() - 38 - 20))
      return -3;
  }
  return -1;
}

int PlaylistWindow::CoordToItem(int x, int y) {
  return scroll_ + (y - 20) / row_height_;
}

void PlaylistWindow::LeftButtonDown(int x, int y) {
  left_button_down_ = true;
  MouseMove(x, y);
  
  if (!compact_) {
    if (hover_button_ == -3) {
      SelectItem(CoordToItem(x, y));
    } else if (hover_button_ == -2) {
      dragging_list_delta_ = (unsigned)(y - dragging_list_pixel_) <= 18 ? (y - dragging_list_pixel_) : 9;
      dragging_list_ = true;
      Repaint();
      MouseMove(x, y);
    }
  }
  if (hover_button_ == 2)
    InitWindowSizing();
}

void PlaylistWindow::OpenItem(int itemindex) {
  TspItem *item = TspItemListGetItem(main_window_->item_list(), itemindex);
  if (item) {
    TspItemType item_type = TspItemGetType(item);
    switch(item_type) {
    case kTspItemType_Track:
      TspPlayerPlayItemList(main_window_->tsp(), main_window_->item_list(), selected_item_);
      break;
    case kTspItemType_Playlist:
      main_window_->OpenUri(TspItemGetUri(item, main_window_->tsp()), -2);
      break;
    case kTspItemType_OpenFolder:
    case kTspItemType_ClosedFolder:
      TspItemListSetFolderOpen(main_window_->item_list(), selected_item_, -1);
      break;
    }
  }
}

void PlaylistWindow::LeftButtonDouble(int x, int y) {
  if (FindButton(x, y) == -3 && selected_item_ >= 0) {
    OpenItem(selected_item_);
  } else if (y < 14) {
    SetCompact(!compact_);
  }
}

void PlaylistWindow::LeftButtonUp(int x, int y) {
  int b = (hover_button_ >= 0) ? FindButton(x, y) : -1;
  if (b == 0)
    SetCompact(!compact_);
  else if (b == 1)
    main_window.SetPlaylistVisible(!main_window.playlist_visible());
  if (dragging_list_) {
    dragging_list_ = false;
    Repaint();
  }
  left_button_down_ = false;
  MouseMove(x, y);
}

void PlaylistWindow::MouseMove(int x, int y) {
  int over = FindButton(x, y);
  int b = left_button_down_ && !dragging_list_ ? over : -1;
  if (b != hover_button_) {
    hover_button_ = b;
    Repaint();
  }

  SetWindowDragable(over == -1 && (g_easy_move || y < 14));

  if (dragging_list_) {
    int p = y - dragging_list_ - 20;
    int h = height() - 58 - 18;
    int n = IntMax(0, ItemCount() - VisibleItemCount());
    int s = (n * p + h/2) / h;
    SetScroll(s);
  }
}

void PlaylistWindow::MouseWheel(int x, int y, int dx, int dy) {
  SetScroll(scroll_ - dy * 8 / 120);
}

void PlaylistWindow::Paint() {
  if (!compact_)
    PaintFull();
  else
    PaintCompact();
}

void PlaylistWindow::PaintCompact() {
  char buffer[1024];

  Blit(0, 0, 25, 14, res.pledit, 72, 42);
  StretchBlit(25, 0, width() - 75, 14, res.pledit, 72, 57, 25, 14);
  Blit(width() - 50, 0, 50, 14, res.pledit, 99, active() ? 42 : 57);

  // expand contract
  if (hover_button_ == 0) Blit(width() - 21, 3, 9, 9, res.pledit, 150, 42);
  // close
  if (hover_button_ == 1) Blit(width() - 11, 3, 9, 9, res.pledit, 52, 42);

  Tsp *tsp = main_window_->tsp();
  TspItem *item = TspPlayerGetNowPlaying(tsp);
  if (item) {
    int idx = TspItemListGetNowPlayingIndex(main_window_->player_list());
    // Draw song artist + title
    sprintf(buffer, "%d. %s - %s", idx + 1, TspItemGetArtistName(item), TspItemGetName(item));
    PaintText(this, buffer, 0, kEllipsis, 5, 4, width() - 65, 6);

    int dur = TspItemGetLength(item);
    sprintf(buffer, "%2d:%.2d", dur / 60, dur % 60);
    PaintText(this, buffer, 0, 0, width() - 54, 4, 25, 6);
  }
}

void PlaylistWindow::PaintFull() {
  // Top part
  int y = active() ? 0 : 21;
  Blit(0, 0, 25, 20, res.pledit, 0, y);
  int m = (width() - 100) >> 1;
  Blit(m, 0, 100, 20, res.pledit, 26, y);
  StretchBlit(25, 0, m - 25, 20, res.pledit, 127, y, 25, 20);
  StretchBlit(m + 100, 0, width() - 125 - m, 20, res.pledit, 127, y, 25, 20);
  Blit(width() - 25, 0, 25, 20, res.pledit, 153, y);

  // expand contract
  if (hover_button_ == 0) Blit(width() - 21, 3, 9, 9, res.pledit, 62, 42);
  // close
  if (hover_button_ == 1) Blit(width() - 11, 3, 9, 9, res.pledit, 52, 42);


  // Bottom part
  Blit(0, height() - 38, 125, 38, res.pledit, 0, 72);
  StretchBlit(125, height() - 38, width() - 150 - 125, 38, res.pledit, 179, 0, 25, 38);
  Blit(width() - 150, height() - 38, 150, 38, res.pledit, 126, 72);

  // Sides
  StretchBlit(0, 20, 12, height() - 20 - 38, res.pledit, 0, 42, 12, 29);
  StretchBlit(width() - 20, 20, 20, height() - 20 - 38, res.pledit, 31, 42, 20, 29);

  // Fill center part with black
  Fill(12, 20, width() - 12 - 20, height() - 20 - 38, g_color_bg_normal);

  SetFont(g_playlist_font.c_str(), font_size_);

  row_height_ = GetFontHeight();
  if (row_height_ < 2) row_height_ = 2;

  // Lines
  int num_lines = VisibleItemCount();
  char buffer[1024];
  Tsp *tsp = main_window_->tsp();
  TspItemList *item_list = main_window_->item_list();
  int playing_item = TspItemListGetNowPlayingIndex(item_list);
  for(int i = 0; i < num_lines; i++) {
    int j = i + scroll_;
    TspItem *item = TspItemListGetItem(item_list, j);
    if (!item) continue;

    int y = 22 + i * row_height_;

    // If line is selected, fill with a blue color
    if (selected_item_ == j)
      Fill(12, y, width() - 12 - 19, row_height_, g_color_bg_selected);
    
    unsigned int text_color = (j == playing_item) ? g_color_current : g_color_normal;
    TspItemType item_type = TspItemGetType(item);
    switch(item_type) {
    case kTspItemType_Track: {
      // Draw song artist + title
      sprintf(buffer, "%d. %s - %s", j + 1, TspItemGetArtistName(item), TspItemGetName(item));
      DrawText(16, y, width() - 12 - 19 - 35 - 4, row_height_, buffer, 0, text_color);
    
      // Draw song time
      int dur = TspItemGetLength(item);
      sprintf(buffer, TspItemIsPlayable(item) ? "%d:%.2d" : "--:--", dur / 60, dur % 60);
      DrawText(width() - 19 - 35 - 2, y, 32, row_height_, buffer, 1, text_color);
      } break;
    case kTspItemType_ClosedFolder:
    case kTspItemType_OpenFolder: {
      int indent = TspItemListGetIndent(item_list, j) * 8;
      sprintf(buffer, "%s [%s]", item_type == kTspItemType_OpenFolder ? "\xE2\x96\xBC" : "\xE2\x96\xBA", TspItemGetName(item));
      DrawText(16 + indent, y, width() - 12 - 19 - 35 - 4 - indent, row_height_, buffer, 0, 0x00ff00);
      } break;

    case kTspItemType_Playlist: {
      int indent = TspItemListGetIndent(item_list, j) * 8;
      DrawText(16 + indent, y, width() - 12 - 19 - 70 - 4 - indent, row_height_, TspItemGetName(item), 0, 0x00ff00);
      DrawText(width() - 19 - 70 - 2, y, 67, row_height_, TspItemGetArtistName(item), 1, 0x00ff00);
      } break;
    }
  }

  int n = IntMax(0, ItemCount() - VisibleItemCount());
  int s = scroll_ < 0 ? 0 : scroll_ > n ? n : scroll_;
  int h = (height() - 58 - 18);
  int p = 19 + (n ? s * h / n : 0);
  Blit(width()-15, p, 8, 18, res.pledit, dragging_list_?61:52, 53);
}

int PlaylistWindow::VisibleItemCount() {
  return (height() - 22 - 42) / row_height_;
}

int PlaylistWindow::ItemCount() {
  TspItemList *item_list = main_window_->item_list();
  int length = TspItemListGetTotalLength(item_list);
  if (length < 0) length = TspItemListGetLastVisibleItem(item_list) + 1;
  return length;
}

void PlaylistWindow::KeyDown(int key, int shift) {
  if (key == 'W' && shift == MOD_CONTROL) {
    SetCompact(!compact_);
  } else if (shift == 0 && key == 'P') {
    main_window_->OpenUri(kTspUri_Root, -2);
  } else if (shift == 0 && key == 'S') {
    const char *starred_uri = TspGetStarredUri(main_window_->tsp(), NULL);
    main_window_->OpenUri(starred_uri, -2);
  } else if (shift == 0 && key == VK_UP) {
    if (selected_item_ > 0)
      SelectItem(selected_item_ - 1);
  } else if (shift == 0 && key == VK_DOWN) {
    SelectItem(selected_item_ + 1);
  } else if (shift == 0 && key == VK_PRIOR) {
    SelectItem(IntMax(selected_item_ - 4, 0));
  } else if (shift == 0 && key == VK_NEXT) {
    SelectItem(IntMin(selected_item_ + 4, ItemCount() - 1));
  } else if (shift == 0 && key == VK_RETURN) {
    Perform(CMD_PLAYLIST_PLAY);
  } else if (shift == MOD_CONTROL && key == 'C') {
    Perform(CMD_COPY_TRACK);
  } else {
    main_window_->KeyDown(key, shift);
  }
}

void PlaylistWindow::SetScroll(int s) {
  int vis_count = VisibleItemCount();
  s = IntMin(s, ItemCount() - vis_count);
  if (s < 0) s = 0;
  if (s != scroll_) {
    scroll_ = s;
    TspItemListLoadRange(main_window_->item_list(), s, vis_count);
    Repaint();
  }
}

void PlaylistWindow::SelectItem(int item) {
  if (item != selected_item_) {
    if (item < 0 || item >= ItemCount())
      return;
    selected_item_ = item;
    Repaint();

    if (item >= 0) {
      int n;
      if (item < scroll_) {
        SetScroll(item);
      } else if (item >= scroll_ + (n=VisibleItemCount())) {
        SetScroll(item - n + 1);
      }
    }
  }
}

// Scroll so that the playing item is in view.
void PlaylistWindow::ScrollInView() {
  int item = TspItemListGetNowPlayingIndex(main_window_->item_list());
  if (item < 0) return;
  int visible_count = VisibleItemCount();
  if ((unsigned)(item - scroll_) >= (unsigned)visible_count)
    SetScroll(item - (visible_count >> 1));
}

bool PlaylistWindow::SizeChanging(int *w, int *h) {
  *w = (Clamp(*w, WND_MAIN_W, WND_MAIN_W * 20) + 12) / 25 * 25;
  *h = compact_ ? 14 : (Clamp(*h, WND_MAIN_H, WND_MAIN_H * 20) + 14) / 29 * 29;
  return true;
}

void PlaylistWindow::SetCompact(bool v) {
  if (v != compact_) {
    compact_ = v;
    if (v) normal_height_ = height();
    Resize(width(), v ? 14 : normal_height_);
    PrefWriteInt(compact_, "pl.compact");
  }
}

void PlaylistWindow::RightButtonDown(int x, int y) {
  int item = CoordToItem(x, y);
  if (item >= 0) {
    int t = item;
    item = selected_item_;
    SelectItem(t);
  }
  ShowRightClickMenu();
  if (item >= 0)
    SelectItem(item);
}

static const char *PlaylistNameFromUri(Tsp *tsp, const char *uri) {
  int rootlist_count = TspGetRootlistCount(tsp);
  for(int i = 0; i < rootlist_count; i++) {
    TspItemType type = TspGetRootlistType(tsp, i);
    if (type == kTspItemType_Playlist) {
      const char *cur_uri = TspGetRootlistUri(tsp, i);
      if (strcmp(uri, cur_uri) == 0)
        return TspGetRootlistName(tsp, i);
    }
  }
  return NULL;
}

void PlaylistWindow::ShowRightClickMenu() {
  MenuBuilder menu;
  menu.AddItem(CMD_PLAYLIST_PLAY, "Play");
  menu.AddItem(CMD_GOTO_ALBUM, "Go to Album");
  menu.AddSeparator();
  menu.BeginSubMenu();
  main_window_->AppendPlaylistPopupMenu(menu, CMD_PLAYLIST_ADD_TO, NULL, true);
  menu.EndSubMenu("Add To");
  if (last_added_playlist_uri_.size()) {
    const char *name = PlaylistNameFromUri(main_window_->tsp(), last_added_playlist_uri_.c_str());
    if (name)
      menu.AddItem(CMD_PLAYLIST_ADD_TO_LAST, (std::string("Add to: ") + name).c_str());
  }
  menu.AddSeparator();
  menu.AddItem(CMD_COPY_TRACK, "Copy Track URI\tCtrl+C");
  menu.AddItem(CMD_COPY_ALBUM, "Copy Album URI");
  menu.AddItem(CMD_COPY_ARTIST, "Copy Artist URI");
  menu.AddItem(CMD_COPY_PLAYLIST, "Copy Playlist URI");
  menu.AddSeparator();
  menu.AddItem(CMD_TRACK_RADIO, "Start Track Radio");
  menu.AddItem(CMD_ALBUM_RADIO, "Start Album Radio");
  menu.AddItem(CMD_ARTIST_RADIO, "Start Artist Radio");
  menu.AddItem(CMD_PLAYLIST_RADIO, "Start Playlist Radio");
  menu.AddSeparator();
  menu.AddItem(CMD_OPEN_IN_BROWSER, "Open in Browser");
  Perform(menu.Popup(this));
}

static std::string ConvertToPlayUri(const char *s) {
  char buf[128], *d, c;
  buf[0] = 0;
  if (memcmp(s, "spotify:", 8) == 0) {
    s += 8;
    strcpy(buf, "https://play.spotify.com/");
    d = buf + sizeof("https://play.spotify.com/") - 1;
    for(;;) {
      c = *s++;
      if (c == ':') c = '/';
      *d++ = c;
      if (c == 0) break;
    }
  }
  return buf;
}

void PlaylistWindow::Perform(int cmd) {
  Tsp *tsp = main_window_->tsp();
  TspItem *item = TspItemListGetItem(main_window_->item_list(), selected_item_);

  switch(cmd) {
  case CMD_PLAYLIST_PLAY:
    OpenItem(selected_item_);
    break;
  case CMD_GOTO_ALBUM:
    if (item) main_window_->OpenUri(TspItemGetAlbumUri(item, tsp), -2);
    break;
  case CMD_COPY_TRACK:
    if (item) PlatformWriteClipboard(TspItemGetUri(item, tsp));
    break;
  case CMD_COPY_ALBUM:
    if (item) PlatformWriteClipboard(TspItemGetAlbumUri(item, tsp));
    break;
  case CMD_COPY_ARTIST:
    if (item) PlatformWriteClipboard(TspItemGetArtistUri(item, tsp));
    break;
  case CMD_COPY_PLAYLIST:
    PlatformWriteClipboard(TspItemListGetUri(main_window_->item_list()));
    break;
  case CMD_TRACK_RADIO:
    if (item) main_window_->OpenUri(TspGetRadioUri(tsp, TspItemGetUri(item, tsp)), -1);
    break;
  case CMD_ALBUM_RADIO:
    if (item) main_window_->OpenUri(TspGetRadioUri(tsp, TspItemGetAlbumUri(item, tsp)), -1);
    break;
  case CMD_ARTIST_RADIO:
    if (item) main_window_->OpenUri(TspGetRadioUri(tsp, TspItemGetArtistUri(item, tsp)), -1);
    break;
  case CMD_PLAYLIST_RADIO:
    TspItemListSetRadioTracks(main_window_->item_list());
    main_window_->OpenUri(kTspUri_TrackRadio, -1);
    break;
  case CMD_OPEN_IN_BROWSER:
    if (item)
      OpenUrl(ConvertToPlayUri(TspItemGetUri(item, tsp)).c_str());
    break;
  case CMD_PLAYLIST_ADD_TO_LAST:
    if (item && TspItemGetType(item) == kTspItemType_Track) {
      const char *uri = TspItemGetUri(item, tsp);
      TspPlaylistSimpleModify(tsp, last_added_playlist_uri_.c_str(), &uri, 1, kTspPlaylistSimpleModify_AddLast);
    }
    break;
  default:
    switch(cmd >> 12) {
    case CMD_PLAYLIST_ADD_TO >> 12:
      if (item && TspItemGetType(item) == kTspItemType_Track) {
        last_added_playlist_uri_ = main_window_->pluris_[cmd - CMD_PLAYLIST_ADD_TO];
        const char *uri = TspItemGetUri(item, tsp);
        TspPlaylistSimpleModify(tsp, last_added_playlist_uri_.c_str(), &uri, 1, kTspPlaylistSimpleModify_AddLast);
      }
      break;
    }

  }
}

///////////////////////////////////////////////////////////
// EQUALIZER WINDOW
///////////////////////////////////////////////////////////
EqWindow::EqWindow(MainWindow *main_window) {
  main_window_ = main_window;
  hover_button_ = -1;
  hover_eq_ = -1;
  Resize(WND_MAIN_W, WND_MAIN_H);
}

EqWindow::~EqWindow() {
}

void EqWindow::Load() {
  for(int i = 0; i != arraysize(eqvals_); i++)
    eqvals_[i] = PrefReadInt(0, "eq.band%d", i);
  eq_enabled_ = PrefReadInt(0, "eq.enable") != 0;
  auto_enabled_ = PrefReadInt(0, "eq.auto") != 0;
  gain_mode_ = PrefReadInt(0, "eq.gain_mode");
  CopyToTsp();

  SetCompact(PrefReadBool(false, "eq.compact"));
}

static const unsigned char eqx[] = {21, 78, 96, 114, 132, 150, 168, 186, 204, 222, 240};

const ButtonRect eq_buttons[] = {
  {254, 3, 9, 9}, // 0 Make Compact
  {264, 3, 9, 9}, // 1 Close
  {14, 18, 26, 12}, // 2 On
  {217, 18, 44, 12}, // 3 Presets
  {40, 18, 32, 12}, // 4 Auto
};

int EqWindow::FindButton(int x, int y) {
  int i;
  const ButtonRect *buttons = eq_buttons;
  int num_buttons = arraysize(eq_buttons);
  for(i = 0; i < num_buttons; i++) {
    if ( (unsigned)(x - buttons[i].x) < (unsigned)buttons[i].w &&
         (unsigned)(y - buttons[i].y) < (unsigned)buttons[i].h)
      return i;
  }
  return -1;
}

int EqWindow::FindEq(int x, int y) {
  if (!IsInside(y, 38,  63)) return -1;
  for(int i = 0; i < arraysize(eqx); i++) {
    if (IsInside(x, eqx[i], 14)) return i;
  }
  return -1;
}

void EqWindow::RepaintEq(int v) {
  if ((unsigned)v > arraysize(eqx)) return;
  //RepaintRange(eqx[v], 38, 14, 63);
  Repaint();
}

int EqWindow::EqPixel(int j) {
  // 127 => 0,  -127 => 50
  int i = eqvals_[j];
  i = (((-i) + 127) * 50 + 127) / 254;
  if (i >= 25) i++;
  return i;
}

void EqWindow::LeftButtonDown(int x, int y) {
  int j = FindEq(x, y);
  dragging_eq_ = (j >= 0);
  hover_eq_ = -1;
  if (j >= 0) {
    unsigned p = y - EqPixel(j) - 38;
    dragging_eq_delta_ = p < 11 ? p : 5;
  }
  left_button_down_ = !dragging_eq_;
  MouseMove(x, y);
}

void EqWindow::LeftButtonDouble(int x, int y) {
  if (y < 14) {
    SetCompact(!compact_);
    PrefWriteInt(compact_, "eq.compact");
  }
}

void EqWindow::LeftButtonUp(int x, int y) {
  int b = (hover_button_ >= 0) ? FindButton(x, y) : -1;

  left_button_down_ = false;
  dragging_eq_ = false;
  MouseMove(x, y);

  if (b == 0) {
    SetCompact(!compact_);
  } else if (b == 1) {
    main_window.SetEqVisible(false);
  } else if (b == 2) {
    eq_enabled_ = !eq_enabled_;
    PrefWriteInt(eq_enabled_, "eq.enable");
    Repaint();
    CopyToTsp();
  } else if (b == 3) {
    ShowPresets();
  } else if (b == 4) {
    auto_enabled_ = !auto_enabled_;
    PrefWriteInt(auto_enabled_, "eq.auto");
    Repaint();
    CopyToTsp();
  }
}

void EqWindow::RightButtonUp(int x, int y) {
  if (FindButton(x,y) == 4)
    ShowAutoPopup();
}

void EqWindow::MouseMove(int x, int y) {
  int over = FindButton(x, y);
  int b = left_button_down_ ? over : -1;
  if (b != hover_button_) {
    hover_button_ = b;
    Repaint();
  }
  int j = -1;

  SetWindowDragable(over == -1 && FindEq(x, y) == -1 && (y < 14 || g_easy_move));
  
  if (dragging_eq_) {
    if (hover_eq_ == 0) {
      j = 0;
    } else {
      j = FindEq(x, y);
      if (hover_eq_ > 0 && j <= 0) j = hover_eq_;
    }
    
    if (j >= 0) {
      int v = Clamp(y - 38 - dragging_eq_delta_, 0, 51);
      if (v > 25) v--;
      // Convert 0..50 to 127..-127
      v = (-v) * 254 / 50 + 127;
      if (SetEqVal(j, v)) CopyToTsp();
    }
  }
  if (j != hover_eq_) {
    RepaintEq(hover_eq_);
    hover_eq_ = j;
    RepaintEq(hover_eq_);
  }
}

bool EqWindow::SetEqVal(int j, int v) {
  if (eqvals_[j] != v) {
    PrefWriteInt(v, "eq.band%d", j);
    eqvals_[j] = v;
    RepaintEq(j);
    return true;
  }
  return false;
}

void EqWindow::Paint() {
  if (!compact_)
    PaintFull();
  else
    PaintCompact();
}

void EqWindow::PaintCompact() {
  Blit(0, 14, 275, 116-14, res.eqmain, 0, 14);
  Blit(0, 0, 275, 14, res.eqmain, 0, active() ? 134 : 149);

  // expand contract
  if (hover_button_ == 0) Blit(275 - 21, 3, 9, 9, res.eqmain, 0, 125);
  // close
  if (hover_button_ == 1) Blit(275 - 11, 3, 9, 9, res.eqmain, 0, 116);
}

void EqWindow::PaintFull() {
  Blit(0, 14, 275, 116-14, res.eqmain, 0, 14);
  Blit(0, 0, 275, 14, res.eqmain, 0, active() ? 134 : 149);

  // expand contract
  if (hover_button_ == 0) Blit(275 - 21, 3, 9, 9, res.eqmain, 0, 125);
  // close
  if (hover_button_ == 1) Blit(275 - 11, 3, 9, 9, res.eqmain, 0, 116);

  // On button
  Blit(14, 18, 26, 12, res.eqmain, 10 + 118 * (hover_button_ == 2) + (eq_enabled_ * 59), 119);

  // Auto button
  Blit(40, 18, 32, 12, res.eqmain, 36 + 118 * (hover_button_ == 4) + (auto_enabled_ * 59), 119);

  // Preset button
  Blit(217, 18, 44, 12, res.eqmain, 224, (hover_button_ == 3) ? 176 : 164);
    
  for(int i = 0; i < arraysize(eqx); i++) {
    int ev = EqPixel(i); // 0..51
    int bv = (51 - ev) * 27 / 50;          // 0..27
    Blit(eqx[i], 38, 14, 63, res.eqmain, 13 + (bv % 14) * 15, 164 + (bv / 14) * 65);
    Blit(eqx[i] + 1, ev + 38, 11, 11, res.eqmain, 0, (dragging_eq_ && hover_eq_ == i) ? 176 : 164);
  }

  DrawGraph();
}

void EqWindow::KeyDown(int key, int shift) {
  if (key == 'W' && shift == MOD_CONTROL) {
    SetCompact(!compact_);
  }
}

void EqWindow::SetCompact(bool v) {
  if (v != compact_) {
    compact_ = v;
    if (v) normal_height_ = height();
    Resize(width(), v ? 14 : normal_height_);
  }
}

void EqWindow::ShowAutoPopup() {
  MenuBuilder menu;
  menu.AddRadioItem(1, "Track Gain", gain_mode_ == 0);
  menu.AddRadioItem(2, "Album Gain", gain_mode_ == 1);
  int rv = menu.Popup(this);
  if (rv == 1 || rv == 2) {
    gain_mode_ = rv - 1;
    PrefWriteInt(gain_mode_, "eq.gain_mode");
    CopyToTsp();
  }
}

typedef struct pl_Spline {
  float *keys;              /* Key data, keyWidth*numKeys */
  int keyWidth;             /* Number of floats per key */
  int numKeys;              /* Number of keys */
  float cont;               /* Continuity. Should be -1.0 -> 1.0 */
  float bias;               /* Bias. -1.0 -> 1.0 */
  float tens;               /* Tension. -1.0 -> 1.0 */
} pl_Spline;

void plSplineGetPoint(pl_Spline *s, float frame, float *out) {
  int i, i_1, i0, i1, i2;
  float time1,time2,time3;
  float t1,t2,t3,t4,u1,u2,u3,u4,v1,v2,v3;
  float a,b,c,d;
  float *keys = s->keys;
  float framez = floor(frame);

  a = (1-s->tens)*(1+s->cont)*(1+s->bias);
  b = (1-s->tens)*(1-s->cont)*(1-s->bias);
  c = (1-s->tens)*(1-s->cont)*(1+s->bias);
  d = (1-s->tens)*(1+s->cont)*(1-s->bias);
  v1 = t1 = -a * 0.5f; u1 = a; 
  u2 = (-6-2*a+2*b+c) * 0.5f; v2 = (a-b) * 0.5f; t2 = (4+a-b-c) * 0.5f; 
  t3 = (-4+b+c-d)  * 0.5f; 
  u3 = (6-2*b-c+d) * 0.5f; 
  v3 = b * 0.5f; 
  t4 = d * 0.5f; u4 = -t4;
  
  i0 = (unsigned int)framez;
  i_1 = i0 - 1;
  while (i_1 < 0) i_1 += s->numKeys;
  i1 = i0 + 1;
  while (i1 >= s->numKeys) i1 -= s->numKeys;
  i2 = i0 + 2;
  while (i2 >= s->numKeys) i2 -= s->numKeys;
  time1 = frame - framez;
  time2 = time1*time1;
  time3 = time2*time1;
  i0 *= s->keyWidth;
  i1 *= s->keyWidth;
  i2 *= s->keyWidth;
  i_1 *= s->keyWidth;
  for (i = 0; i < s->keyWidth; i ++) {
    a = t1*keys[i+i_1]+t2*keys[i+i0]+t3*keys[i+i1]+t4*keys[i+i2];
    b = u1*keys[i+i_1]+u2*keys[i+i0]+u3*keys[i+i1]+u4*keys[i+i2];
    c = v1*keys[i+i_1]+v2*keys[i+i0]+v3*keys[i+i1];
    *out++ = a*time3 + b*time2 + c*time1 + keys[i+i0];
  }
}

void EqWindow::DrawGraph() {
  pl_Spline s;
  float keys[13], out;

  s.cont = 0;
  s.bias = 0;
  s.tens = 0.1f;
  s.keys = keys;
  s.keyWidth = 1;
  s.numKeys = 12;

  for(int i = 0; i < 10; i++)
    keys[i+1] = (127 - eqvals_[i + 1]) * (19.0f / 254.0f);

  keys[0] = keys[1];
  keys[11] = keys[10];

  if (!got_colors_) {
    got_colors_ = true;
    for(int bv = 0; bv < 28; bv++)
      colors_[bv] = GetBitmapPixel(res.eqmain, 19 + (bv % 14) * 15, 195 + (bv / 14) * 65);
  }

  int ly = 17 + ((eqvals_[0] + 127) * 18 + 127) / 254;
  Line(86, ly, 86+113, ly, 0xDDCBBA);

  // 27, 12 - 109, 19
  int oldpix = -1;
  for(int i = 0; i < 109; i++) {
    plSplineGetPoint(&s, i * (1.0f/12.0f) + 1.0f, &out);
    int pixval = Clamp((int)floor(out), 0, 18);
    SetPixel(88 + i, 17 + pixval, colors_[27-pixval * 27 / 18]);
    while (oldpix >= 0 && oldpix != pixval) {
      oldpix += (oldpix < pixval) ? 1 : -1;
      SetPixel(88 + i - 1, 17 + oldpix, colors_[27-oldpix * 27 / 18]);
    }
    oldpix = pixval;
  }
}

struct EqPreset {
  const char *name;
  int8 vals[11];
};

static const EqPreset eq_presets[] = {
  {"Default", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
  {"Classical", {0, 0, 0, 0, 0, 0, 0, -51, -51, -51, -66}},
  {"Club", {0, 0, 0, 20, 37, 37, 37, 20, 0, 0, 0}},
  {"Dance", {0, 61, 45, 12, -3, -3, -43, -51, -51, -3, -3}},
  {"Laptop speakers/headphones", {0, 29, 70, 33, -27, -19, 8, 29, 61, 82, 94}},
  {"Large hall", {0, 66, 66, 37, 37, 0, -35, -35, -35, 0, 0}},
  {"Party", {0, 45, 45, 0, 0, 0, 0, 0, 0, 45, 45}},
  {"Pop", {0, -15, 29, 45, 49, 33, -11, -19, -19, -15, -15}},
  {"Reggae", {0, 0, 0, -7, -43, 0, 41, 41, 0, 0, 0}},
  {"Rock", {0, 49, 29, -39, -55, -27, 25, 57, 70, 70, 70}},
  {"Soft", {0, 29, 8, -11, -19, -11, 25, 53, 61, 70, 78}},
  {"Ska", {0, -19, -35, -31, -7, 25, 37, 57, 61, 70, 61}},
  {"Full Bass", {0, 61, 61, 61, 37, 8, -31, -59, -70, -74, -74}},
  {"Soft Rock", {0, 25, 25, 12, -7, -31, -39, -27, -7, 16, 57}},
  {"Full Treble", {0, -66, -66, -66, -31, 16, 70, 102, 102, 102, 111}},
  {"Full Bass & Treble", {0, 45, 37, 0, -51, -35, 8, 53, 70, 78, 78}},
  {"Live", {0, -35, 0, 25, 33, 37, 37, 25, 16, 16, 12}},
  {"Techno", {0, 49, 37, 0, -39, -35, 0, 49, 61, 61, 57}},
};


void EqWindow::CopyToTsp() {
  float allvals[11];
  for(int i = 0; i < 11; i++) {
    allvals[i] = eqvals_[i] * (12.0f / 127.0f);
  }
  int flags = (eq_enabled_ * kTspEqualizer_Enable);
  if (auto_enabled_)
    flags |= gain_mode_ ? kTspEqualizer_AutoGain_Album : kTspEqualizer_AutoGain_Track;
  TspPlayerSetEqualizer(main_window_->tsp(), flags, allvals[0], allvals + 1);
}

void EqWindow::ShowPresets() {
  MenuBuilder menu;
  for(int i = 0; i < arraysize(eq_presets); i++) {
    bool checked = (memcmp(eq_presets[i].vals, eqvals_, sizeof(eqvals_)) == 0);
    menu.AddRadioItem(1 + i, eq_presets[i].name, checked);
  }
  int rv = menu.PopupAt(this, 219, 20);
  if (rv) {
    for(int i = 0; i < 11; i++)
      SetEqVal(i, eq_presets[rv - 1].vals[i]);
    CopyToTsp();
    Repaint();
  }
}

///////////////////////////////////////////////////////////
// GenWindow
///////////////////////////////////////////////////////////


GenWindow::GenWindow() {
  hover_button_ = -1;
  left_button_down_ = false;
  Resize(275, 116*2+29);
}

GenWindow::~GenWindow() {

}

int GenWindow::FindButton(int x, int y) {
  if (x >= width() - 10 && y <= 10) return 0;
  if (y < 14) return 1;
  if (x >= width() - 12 && y >= height() - 12) return 2;
  return -1;
}

void GenWindow::LeftButtonDown(int x, int y) {
  left_button_down_ = true;
  MouseMove(x, y);
  if (hover_button_ == 2)
    InitWindowSizing();
  Repaint();
}

void GenWindow::MouseMove(int x, int y) {
  int over = FindButton(x, y);
  int b = left_button_down_ ? over : -1;
  if (b != hover_button_) {
    hover_button_ = b;
    Repaint();
  }
  SetWindowDragable(over == 1);
}

void GenWindow::LeftButtonUp(int x, int y) {
  int b = (hover_button_ >= 0) ? FindButton(x, y) : -1;
  left_button_down_ = false;
  if (b == 0) OnClose();
}

bool GenWindow::SizeChanging(int *w, int *h) {
  *w = (Clamp(*w, 100, WND_MAIN_W * 20) + 12) / 25 * 25;
  *h = (Clamp(*h, WND_MAIN_H, WND_MAIN_H * 20) + 14) / 29 * 29;
  return true;
}

int GenWindow::GetStringWidth(const char *s) {
  if (!glyphs[0]) {
    int x = 0;
    uint bgcolor = GetBitmapPixel(res.gen, x, 90);
    for(int i = 0; i < 26; i++) {
      int sx = ++x;
      while (GetBitmapPixel(res.gen, x, 90) != bgcolor && (x - sx) < 30)
        x++;
      glyphs[i] = x - sx;
      glyphx[i] = sx;
    }
  }
  int w = 0;
  for(;;) {
    char c = *s++;
    if (c == 0) break;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') 
      w += glyphs[c - 'A'];
    else
      w += 5;
  }
  return w;
}

void GenWindow::DrawLetters(int x, int y, const char *s) {
  int yy = active() ? 88 : 96;
  for(;;) {
    char c = *s++;
    if (c == 0) break;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') 
      Blit(x, y, glyphs[c - 'A'], 7, res.gen, glyphx[c - 'A'], yy), x += glyphs[c - 'A'];
    else
      x += 5;
  }

}

void GenWindow::Paint() {
  Bitmap *curbmp;
  int y = active() ? 0 : 21, x = 0;
  int z;
  char buf[64];

  GetWindowText(buf, sizeof(buf));

  int textw = GetStringWidth(buf);
  int textw_rounded = textw + 24 - (textw + 24) % 25;
  if (textw_rounded > width() - 100)
    textw_rounded = width() - 100;
  
  curbmp = res.gen;
  z = (width() - textw_rounded - 100) / 25;

  // Draw left side before the text
  Blit(x, 0, 25, 20, curbmp, 0, y), x += 25;
  if (z & 1)
    Blit(x, 0, 12, 20, curbmp, 104, y), x += 12;
  for(int zz=z; zz>=2; zz-=2)
    Blit(x, 0, 25, 20, curbmp, 104, y), x += 25;
  Blit(x, 0, 25, 20, curbmp, 26, y), x += 25;

  if (textw_rounded) {
    int xorg = x;
    for(int zz=textw_rounded; zz>=25; zz-=25)
      Blit(x, 0, 25, 20, curbmp, 52, y), x += 25;
    DrawLetters(xorg + (textw_rounded - textw) / 2, 4, buf);
  }
  
  // Draw the right side after the text
  Blit(x, 0, 25, 20, curbmp, 78, y), x += 25;
  if (z & 1)
    Blit(x, 0, 13, 20, curbmp, 104, y), x += 13;
  for(int zz=z; zz>=2; zz-=2)
    Blit(x, 0, 25, 20, curbmp, 104, y), x += 25;
  Blit(x, 0, 25, 20, curbmp, 130, y), x += 25;

  // Draw left and right sides
  y = 20;
  for(int zz=height() - 14 - 20 - 24; zz >= 29; zz -= 29) {
    Blit(0, y, 11, 29, curbmp, 127, 42);
    Blit(width() - 8, y, 8, 29, curbmp, 139, 42);
    y += 29;
  }
  Blit(0, y, 11, 24, curbmp, 158, 42);
  Blit(width() - 8, y, 8, 24, curbmp, 170, 42);
  y += 24;

  int botw = IntMin(width() >> 1, 125);
  int botw2 = IntMin((width() + 1) >> 1, 125);

  // Draw bottom part
  x = 0;
  Blit(x, y, botw, 14, curbmp, 0, 42), x += botw;
  for(int zz=width() - 250; zz >= 25; zz -= 25)
    Blit(x, y, 25, 14, curbmp, 127, 72), x += 25;
  Blit(x, y, botw2, 14, curbmp, 125-botw2, 57);

  if (hover_button_ == 0) Blit(width()-11, 3, 9, 9, curbmp, 148, 42);
}

///////////////////////////////////////////////////////////
// CoverArtWindow
///////////////////////////////////////////////////////////

CoverArtWindow::CoverArtWindow(MainWindow *main_window) {
  main_window_ = main_window;
  image_needs_load_ = false;
  bitmap_ = NULL;
}

CoverArtWindow::~CoverArtWindow() {
  PlatformDeleteBitmap(bitmap_);
}

void CoverArtWindow::Paint() {
  Load();

  GenWindow::Paint();
  if (bitmap_) {
    Size size = PlatformGetBitmapSize(bitmap_);
    StretchBlit(11, 20, width() - 19, height() - 34, bitmap_, 0, 0, size.w, size.h);
  } else {
    Fill(11, 20, width() - 19, height() - 34, 0);  
  }
}

void CoverArtWindow::OnClose() {
  main_window.SetCoverartVisible(false);
}

void CoverArtWindow::SetImage(const char *image) {
  if (image == image_) return;
  image_ = image;
  image_needs_load_ = true;
  if (!visible()) {
    PlatformDeleteBitmap(bitmap_);
    bitmap_ = NULL;
  }
  Load();
}

void CoverArtWindow::Load() {
  if (!image_needs_load_ || !visible())
    return;
  image_needs_load_ = false;
  accumulating_bytes_.clear();
  TspDownloadImageCancel(main_window.tsp_, this);
  TspDownloadImage(main_window.tsp_, image_.c_str(), this, kTspImageQuality_300);
}

void CoverArtWindow::GotImagePart(TspImageDownloadResult *part) {
  accumulating_bytes_.append((char*)part->data, part->data_size);
  if (part->error == kTspErrorOk) {
    PlatformDeleteBitmap(bitmap_);
    bitmap_ = PlatformLoadBitmapFromBuf(accumulating_bytes_.data(),accumulating_bytes_.size());
    Repaint();
  } else if (part->error != kTspErrorInProgress) {
    PlatformDeleteBitmap(bitmap_);
    bitmap_ = NULL;
    Repaint();
  }
}

static void MyCallback(void *context, TspCallbackEvent event, void *source, void *param) {
  MainWindow *w = (MainWindow*)context;
  if (event == kTspCallbackEvent_Connected) {
    w->connect_error_ = false;

    char token[1024];
    if (!w->username_.empty() && TspGetCredentialToken(w->tsp_, token, sizeof(token)) == 0) {
      PrefWriteStr(w->username_.c_str(), "username");
      PrefWriteStr(token, "password");
    }

  } else if (event == kTspCallbackEvent_AutoComplete) {
    AutoCompleteCopy();
  } else if (event == kTspCallbackEvent_NowPlayingChanged || event == kTspCallbackEvent_RemoteUpdate) {
    w->ResetScroll();
    w->Repaint();
    w->SaveNowPlaying();
    playlist_window.ScrollInView();
    playlist_window.Repaint();
    
    TspItem *playing_item = TspPlayerGetNowPlaying(w->tsp_);
    if (g_sc)
      ShoutcastSetNowPlaying(g_sc, playing_item);

    coverart_window.SetImage(playing_item ? TspItemGetImageUri(playing_item, w->tsp_) : "");
  } else if (event == kTspCallbackEvent_PlayControlsChanged) {
    PrefWriteInt(TspPlayerGetShuffle(w->tsp()), "shuffle");
    PrefWriteInt(TspPlayerGetRepeat(w->tsp()), "repeat");
    w->Repaint();
  } else if (event == kTspCallbackEvent_VolumeChanged) {
    WavSetVolume(TspPlayerGetVolume(w->tsp()));
    w->Repaint();
  } else if (event == kTspCallbackEvent_Pause || event == kTspCallbackEvent_Resume) {
    w->Repaint();
  } else if (event == kTspCallbackEvent_ItemListChanged) {
    if (source == w->item_list()) {
      playlist_window.FixupScroll();
      playlist_window.Repaint();
    } else if (source == w->player_list() && w->itemlist_in_sync_with_player_) {
      TspItemListCopyFrom(w->item_list(), w->player_list());
      playlist_window.FixupScroll();
      playlist_window.Repaint();
      w->itemlist_in_sync_with_player_ = false;
    }
  } else if (event == kTspCallbackEvent_ConnectError) {
    w->connect_error_ = true;
  } else if (event == kTspCallbackEvent_ImageDownload) {
    coverart_window.GotImagePart((TspImageDownloadResult*)param);
  }
}

PlatformWindow *InitSpotamp(int argc, char **argv) {
  if (argc >= 2)
    url_to_open = argv[1];

  main_window.Create(NULL);
  playlist_window.Create(&main_window);
  eq_window.Create(&main_window);
  coverart_window.Create(&main_window);

  g_visualizer_wnd = main_window.SetVisualizer(24, 43, 76, 16);

  if (!main_window.Load())
    return NULL;

  playlist_window.Move(main_window.screen_rect()->left, main_window.screen_rect()->bottom);
  eq_window.Move(main_window.screen_rect()->right, main_window.screen_rect()->top);
  coverart_window.Move(main_window.screen_rect()->right, main_window.screen_rect()->bottom);

  eq_window.Load();
  playlist_window.Load();

  InitVisualizer();

  coverart_window.SetWindowText("Album Art");

  main_window.SetVisible(true);
  if (main_window.eq_visible()) eq_window.SetVisible(true);
  if (main_window.playlist_visible()) playlist_window.SetVisible(true);
  if (main_window.coverart_visible()) coverart_window.SetVisible(true);
 
  return &main_window;
}
