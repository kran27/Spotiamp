#ifndef SPOTIFYAMP_WINDOW_SDL_H_
#define SPOTIFYAMP_WINDOW_SDL_H_

#include "stdafx.h"
#include "SDL.h"
#include "window.h"

typedef SDL_Window *WindowHandle;

class PlatformWindow : public PlatformWindowBase<PlatformWindow> {
  friend class PlatformWindowBase<PlatformWindow>;
public:
  PlatformWindow();
  virtual ~PlatformWindow();

  virtual void LeftButtonDown(int x, int y) = 0;
  virtual void LeftButtonDouble(int x, int y) = 0;
  virtual void LeftButtonUp(int x, int y) = 0;
  virtual void MouseMove(int x, int y) = 0;
  virtual void Paint() = 0;
  virtual void KeyDown(int key, int shift) = 0;
  virtual bool SizeChanging(int *w, int *h) { return false; }
  virtual void MainLoop() {}
  virtual void ScrollText() {}
  virtual bool HandleAppCommandHotKeys(int cmd, int keys) { return false; }
  virtual void Destroy() {}

  void RegisterForGlobalHotkeys() {}
  void MakeActive();

  void Create(PlatformWindow *owner_window);

  // Draw a bitmap to a portion of the window
  void Blit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy);

  // Draw a bitmap to a portion of the window
  void StretchBlit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy, int sw, int sh);

  // Fill a rectangle with a color
  void Fill(int x, int y, int w, int h, unsigned int color) {}

  // Draw a text inside a rectangle
  void DrawText(int x, int y, int w, int h, const char *text, int flags, unsigned int color) {}
  
  // Draw a line
  void Line(int x, int y, int x2, int y2, unsigned int color);

  // Trigger repaint at earliest convenience
  void Repaint();

  // Repaint only a range of the window
  void RepaintRange(int x, int y, int w, int h);

  void SetPixel(int x, int y, unsigned int color);

  void SetFont(const char *fontname, int size);
  int GetFontHeight();

  void GetWindowText(char *s, int size);
  void SetWindowText(const char *s);

  // Return a pixel
  unsigned int GetBitmapPixel(Bitmap *src, int x, int y);

  void Move(int l, int t);

  void Minimize();
  void BringWindowToTop();
  void Quit();

  void PlatformPaint();
  static void HandleEvent(SDL_Event *event);
  static PlatformWindow *FromID(Uint32 window_id);

  WindowHandle SetVisualizer(int x, int y, int w, int h);

  static bool g_easy_move;

private:  
  void AlwaysOnTopChanged();
  void VisibleChanged();

  static void InitDraggingOrSizing();
  static void MoveAllWindows();
  static void FindMonitors();
  
  bool need_repaint_;
  SDL_Window *window_;
  SDL_Surface *surface_;
  Uint32 window_id_;
  int drag_start_x_, drag_start_y_;
};

class FileEnumerator {
public:
  explicit FileEnumerator(const char *directory);
  ~FileEnumerator();

  bool Next();
  bool is_directory() const;
  char *filename() const;

private:
};

class MenuBuilder {
public:
  MenuBuilder();
  ~MenuBuilder();

  enum {
    kChecked = 1,
    kRadio = 2,
    kGrayed = 4,
  };

  void BeginSubMenu();
  void EndSubMenu(const char *title, int flags = 0);

  void AddSeparator();
  void AddItem(int id, const char *title, int flags = 0);

  void AddCheckItem(int id, const char *title, bool checked) { AddItem(id, title, checked * kChecked); }
  void AddRadioItem(int id, const char *title, bool radio) { AddItem(id, title, radio * kRadio); }

  int Popup(PlatformWindow *window);
  int PopupAt(PlatformWindow *window, int x, int y);
private:
};




enum {
  APPCOMMAND_MEDIA_PLAY_PAUSE,
  APPCOMMAND_MEDIA_PLAY,
  APPCOMMAND_MEDIA_NEXTTRACK,
  APPCOMMAND_MEDIA_PREVIOUSTRACK,
  APPCOMMAND_MEDIA_STOP,
  APPCOMMAND_LAUNCH_MEDIA_SELECT,
};

// windows alikes
enum {
  MOD_CONTROL = 1,
  MOD_ALT = 2,
  MOD_SHIFT = 4,

  VK_LEFT = 1,
  VK_RIGHT = 2,
  VK_UP = 3,
  VK_DOWN = 4,
  VK_INSERT = 5,
  VK_HOME = 6,
  VK_END = 7,
  VK_PRIOR = 8,
  VK_NEXT = 9,
  VK_RETURN = 10,
};

enum {
  MB_ICONINFORMATION = 1,
  MB_ICONEXCLAMATION = 2,
  MB_YESNOCANCEL = 4,
  MB_OKCANCEL = 8,

  IDOK = 0,
  IDYES = 1,
  IDCANCEL = 2,
};
int MsgBox(const char *message, const char *title, int flags);

void OpenUrl(const char *url);
const char *PlatformEnumAudioDevices(int i);
unsigned int PlatformGetTicks();
char *PlatformReadClipboard();
bool PlatformWriteClipboard(const char *text);


#endif  // SPOTIFYAMP_WINDOW_SDL_H_
