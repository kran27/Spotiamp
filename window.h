#ifndef SPOTIFYAMP_WINDOW_H_
#define SPOTIFYAMP_WINDOW_H_

#include <string>

struct Rect { int left, top, right, bottom; };
struct Size { int w, h; };
struct Point { int x, y; };
struct Bitmap { int x; };

// Base class using CRTP.
template<typename T>
class PlatformWindowBase {
 public:
  PlatformWindowBase();
  ~PlatformWindowBase();

  void Create(T *owner_window);

  // Double all pixels
  void SetDoubleSize(bool v);
  bool double_size() const { return double_size_; }

  void SetAlwaysOnTop(bool v);
  bool always_on_top() const { return always_on_top_; }

  void SetActive(bool active);
  bool active() const { return active_window_; }
  
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Resize window. If you're in doublesize mode, these still return the normal sized size.
  void Resize(int w, int h);
  int width() const { return width_; }
  int height() const { return height_; }

  T *next() const { return next_; }
  const Rect *screen_rect() const { return &screen_rect_; }

  void SetWindowDragable(bool dragable);

  void InitWindowDragging();
  void InitWindowSizing();

 protected:
  void SizeChanged();
  T *self() { return static_cast<T*>(this); }   
  Point HandleMouseDragOrSize(int dx, int dy, bool snap);

  bool double_size_;
  bool always_on_top_;
  bool active_window_;
  bool visible_;
  bool need_move_;
  bool dragable_;
  int width_, height_;
  Rect screen_rect_;
  T *next_, *owner_;
};

#if defined(WITH_SDL)
#include "window_sdl.h"
#else
#include "window_win32.h"
#endif

struct Bitmap;
class PlatformWindow;

// Shared state
extern Rect g_monitor_rects[16];
extern int g_num_monitor_rects;
extern PlatformWindow *g_platform_windows,*g_main_window;
extern PlatformWindow *g_drag_windows[16];
extern int g_num_drag_windows;
extern bool g_drag_sizing;

int FindConnectedWindows(PlatformWindow *start, PlatformWindow **ws, int mask);
void SnapToScreenEdges(PlatformWindow **ws, int nw, int *dxp, int *dyp);

void HandleWindowResized(PlatformWindow *wnd, int w, int h);

void PrefInit();
int PrefReadInt(int def, const char *name, ...);
bool PrefReadBool(bool def, const char *name, ...);
const char *PrefReadStr(const char *def, const char *name, ...);
void PrefWriteInt(int v, const char *name, ...);
void PrefWriteStr(const char *v, const char *name, ...);

bool PlatformLoadBitmap(Bitmap **bitmap, const char *name);
bool PlatformLoadString(const char *name, std::string *result);
void PlatformSetSkin(const char *filename);
Bitmap *PlatformLoadBitmapFromBuf(const void *data, size_t data_size);
void PlatformDeleteBitmap(Bitmap *bitmap);
Size PlatformGetBitmapSize(Bitmap *bitmap);

char *PlatformReadClipboard();
PlatformWindow *InitSpotamp(int argc, char **argv);

#endif  // SPOTIFYAMP_WINDOW_H_