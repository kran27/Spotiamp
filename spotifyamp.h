#ifndef SPOTIFYAMP_SPOTIFYAMP_H_
#define SPOTIFYAMP_SPOTIFYAMP_H_

extern "C" {
#include "tiny_spotify.h"
#include "types.h"
};

#include "window.h"

#include <string>
#include <vector>

static inline bool IsInside(int x, int pos, int w) {
  return (unsigned)(x - pos) < (unsigned)w;
}

static inline int Clamp(int v, int mi, int ma) {
  if (v < mi) v = mi;
  if (v > ma) v = ma;
  return v;
}

static inline int IntMin(int a, int b) {
  return (a < b) ? a : b;
}

static inline int IntMax(int a, int b) {
  return (a > b) ? a : b;
}


struct ButtonRect {
  int x, y, w, h;
};
#define WND_MAIN_W 275
#define WND_MAIN_H 116

class PlaylistWindow;
class EqWindow;

class MainWindow : public PlatformWindow {
public:
  MainWindow();
  virtual ~MainWindow();

  bool Load();

  // -- from PlatformWindow
  virtual void LeftButtonDown(int x, int y);
  virtual void LeftButtonDouble(int x, int y);
  virtual void LeftButtonUp(int x, int y);
  virtual void RightButtonDown(int x, int y);
  virtual void MouseMove(int x, int y);
  virtual void MouseWheel(int x, int y, int dx, int dy);
  virtual void Paint();
  virtual void KeyDown(int key, int shift);
  virtual void MainLoop();
  virtual void ScrollText();
  virtual bool HandleAppCommandHotKeys(int cmd, int keys);
  virtual void GlobalHotkey(int id);
  virtual void PumpIOEvents();
  
  void Perform(int cmd);

  void OpenUri(const char *query, int track_to_play);
  void PaintBigNumber(int num, int x, int y);
  void PaintSmallNumber(int num, int x, int y);
  int FindButton(int x, int y);
  void ButtonClicked(int b);
  void SetCompact(bool v);
  void SetPositionIndicator(int pos);
  void SetPlaylistVisible(bool v);
  void SetCoverartVisible(bool v);
  bool playlist_visible() const { return playlist_; }
  bool coverart_visible() const { return coverart_visible_; }

  void SetEqVisible(bool v);
  void SetDoubleSize(bool v);
  void SetAlwaysOnTop(bool v);
  bool eq_visible() const { return equalizer_; }

  void ResetScroll() { text_scroll_ = 0; text_scroll_delay_ = 5; }

  void Login(const char *username, const char *password);
  void SaveNowPlaying();

  void ShowEjectMenu();
  void ShowSearchDialog();
  void ShowLoginDialog();
  void ShowOptionsMenu();
  void ShowVisualizeMenu();

  void Quit();

  void AppendPlaylistPopupMenu(MenuBuilder &menu, int base_id, 
                               const char *cur_uri, bool only_writable);

  Tsp *tsp() const { return tsp_; }
  TspItemList *item_list() const { return item_list_;  }
  TspItemList *player_list() const { return player_list_; }

public:
  void PaintCompact();
  void PaintFull();
  void InitThreading();
  void SetShoutcast(bool shoutcast);
  void EnableGlobalHotkeys(bool enable);

  Tsp *tsp_;
  TspItemList *item_list_, *player_list_;

  bool lbutton_down_;
  bool equalizer_, playlist_;
  bool mono_, stereo_;
  bool compact_;
  bool dragging_text_;
  bool time_mode_;
  bool connect_error_;

  bool in_dialog_;
  bool global_hotkeys_;
  bool itemlist_in_sync_with_player_;

  bool dragging_vol_;
  int8 dragging_vol_delta_;
  int volume_pixel_;

  bool shoutcast_;
  byte curr_vis_plugin_;

  bool coverart_visible_;

  bool dragging_seek_;
  int8 dragging_seek_delta_;
  int seek_pixel_;
  int seek_target_;

  int position_indicator_;
  int text_scroll_;
  unsigned char text_scroll_delay_;

  int mouse_last_x_, mouse_last_y_;

  const ButtonRect *buttons_;
  int buttons_count_;

  int hover_button_;

  int delayed_command_;

  PlaylistWindow *playlist_window_;
  EqWindow *eq_window_;
  TspBitrate bitrate_;

  std::string username_;
  std::string curr_skin_;
  std::vector<std::string> pluris_;
  std::vector<std::string> remote_device_ids_;
};

class PlaylistWindow : public PlatformWindow {
 public:
  explicit PlaylistWindow(MainWindow *main_window);
  virtual ~PlaylistWindow();

  void Load();

  // -- from PlatformWindow
  virtual void LeftButtonDown(int x, int y);
  virtual void LeftButtonDouble(int x, int y);
  virtual void LeftButtonUp(int x, int y);
  virtual void MouseMove(int x, int y);
  virtual void MouseWheel(int x, int y, int dx, int dy);
  virtual void Paint();
  virtual void KeyDown(int key, int shift);
  virtual bool SizeChanging(int *w, int *h);
  virtual void RightButtonDown(int x, int y);

  void SetCompact(bool compact);
  void SelectItem(int item);
  void SetScroll(int s);
  void ScrollInView();
  void ShowRightClickMenu();
  void Perform(int cmd);

  int CoordToItem(int x, int y);

  void FixupScroll() { SetScroll(scroll_); }

 private:
  void PaintCompact();
  void PaintFull();
  int VisibleItemCount();
  int FindButton(int x, int y);
  int ItemCount();
  void OpenItem(int itemindex);

  bool compact_;
  bool left_button_down_;
  bool dragging_list_;
  int dragging_list_delta_;
  int dragging_list_pixel_;

  int normal_height_;
  MainWindow *main_window_;
  int selected_item_;
  int scroll_;
  int hover_button_;

  int font_size_;
  int row_height_;

  std::string last_added_playlist_uri_;
};

class EqWindow : public PlatformWindow {
 public:
  explicit EqWindow(MainWindow *main_window);
  virtual ~EqWindow();

  // -- from PlatformWindow
  virtual void LeftButtonDown(int x, int y);
  virtual void LeftButtonDouble(int x, int y);
  virtual void LeftButtonUp(int x, int y);
  virtual void RightButtonUp(int x, int y);
  virtual void MouseMove(int x, int y);
  virtual void Paint();
  virtual void KeyDown(int key, int shift);

  void SetCompact(bool compact);
  void Load();

  bool SetEqVal(int i, int v);

  void ShowAutoPopup();
  void OnSkinLoaded() { got_colors_ = false; }
 private:
  void PaintCompact();
  void PaintFull();
  int FindButton(int x, int y);
  int FindEq(int x, int y);
  void RepaintEq(int e);
  int EqPixel(int j);

  void DrawGraph();

  void CopyToTsp();
  void ShowPresets();

  byte gain_mode_;
  bool auto_enabled_;
  bool compact_;
  bool left_button_down_;
  bool dragging_eq_;
  bool got_colors_;
  bool eq_enabled_;
  int hover_eq_;
  int dragging_eq_delta_;
  int hover_button_;
  int normal_height_;
  MainWindow *main_window_;

  int8 eqvals_[11]; // -127..127 

  uint32 colors_[28];
};

class GenWindow : public PlatformWindow {
public:
  GenWindow();
  virtual ~GenWindow();

  // -- from PlatformWindow
  virtual void LeftButtonDown(int x, int y);
  virtual void LeftButtonDouble(int x, int y) {}
  virtual void LeftButtonUp(int x, int y);
  virtual void MouseMove(int x, int y);
  virtual void Paint();
  virtual void KeyDown(int key, int shift) {}
  virtual void SizeChanged() {}
  virtual bool SizeChanging(int *w, int *h);
  virtual void OnClose() = 0;

  int GetStringWidth(const char *s);
  void DrawLetters(int x, int y, const char *s);
private:
  int FindButton(int x, int y);
  int hover_button_;
  bool left_button_down_;
};


class CoverArtWindow : public GenWindow {
 public:
  explicit CoverArtWindow(MainWindow *main_window);
  virtual ~CoverArtWindow();

  virtual void Paint();
  virtual void OnClose();

  void SetImage(const char *image);

  void GotImagePart(TspImageDownloadResult *part);

 private:
  void Load();

  MainWindow *main_window_;
  std::string image_;
  std::string accumulating_bytes_;
  bool image_needs_load_;
  Bitmap *bitmap_;
};

struct Resources {
  Bitmap *main;
  Bitmap *cbuttons;
  Bitmap *shufrep;
  Bitmap *monoster;
  Bitmap *titlebar;
  Bitmap *text;
  Bitmap *numbers;
  Bitmap *volume;
  Bitmap *posbar;
  Bitmap *playpaus;
  Bitmap *pledit;
  Bitmap *eqmain;
  Bitmap *gen;
};

extern Resources res;

#endif  // SPOTIFYAMP_SPOTIFYAMP_H_
