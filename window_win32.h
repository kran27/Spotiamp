#ifndef SPOTIFYAMP_WINDOW_WIN32_H_
#define SPOTIFYAMP_WINDOW_WIN32_H_

#include "stdafx.h"

typedef HWND WindowHandle;


class PlatformWindow : public PlatformWindowBase<PlatformWindow> {
  friend class PlatformWindowBase<PlatformWindow>;
public:
  PlatformWindow();
  virtual ~PlatformWindow();

  // Functions implemented by subclass
  virtual void LeftButtonDown(int x, int y) {}
  virtual void LeftButtonDouble(int x, int y) {}
  virtual void LeftButtonUp(int x, int y) {}
  virtual void RightButtonDown(int x, int y) {}
  virtual void RightButtonUp(int x, int y) {}
  virtual void MouseMove(int x, int y) {}
  virtual void MouseWheel(int x, int y, int dx, int dy) {}
  virtual void Paint() = 0;
  virtual void KeyDown(int key, int shift) {}
  virtual bool SizeChanging(int *w, int *h) { return false; }
  virtual void MainLoop() {}
  virtual void ScrollText() {}
  virtual bool HandleAppCommandHotKeys(int cmd, int keys) { return false; }
  virtual void Destroy() {}
  virtual void GlobalHotkey(int id) {}
  virtual void PumpIOEvents() {}

  void Create(PlatformWindow *owner_window);
  void Quit() { PostQuitMessage(0); }

  void MakeActive() { SetActiveWindow(handle()); }

  void SetFont(const char *fontname, int size);
  int GetFontHeight();

  // Draw a bitmap to a portion of the window
  void Blit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy);

  // Draw a bitmap to a portion of the window with alpha
  void AlphaBlit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy);

  // Draw a bitmap to a portion of the window
  void StretchBlit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy, int sw, int sh);

  // Fill a rectangle with a color
  void Fill(int x, int y, int w, int h, unsigned int color);

  // Draw a text inside a rectangle
  void DrawText(int x, int y, int w, int h, const char *text, int flags, unsigned int color);

  // Draw a line
  void Line(int x, int y, int x2, int y2, unsigned int color);

  // Trigger repaint at earliest convenience
  void Repaint();

  // Repaint only a range of the window
  void RepaintRange(int x, int y, int w, int h);

  void SetPixel(int x, int y, unsigned int color);

  // Return a pixel
  unsigned int GetBitmapPixel(Bitmap *src, int x, int y);
  
  void Move(int l, int t);
  void Minimize();
  void BringWindowToTop();

  HWND handle() const { return hwnd_; }
  void RegisterForGlobalHotkeys();

  // Internal functions
  virtual BOOL WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *retval);
  friend LRESULT WINAPI WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  // Set the visualizer rectangle
  WindowHandle SetVisualizer(int x, int y, int w, int h);

  void GetWindowText(char *s, int size) { ::GetWindowTextA(handle(), s, size); }

  void SetWindowText(const char *s) { ::SetWindowTextA(handle(), "Album Art"); }

  void NotifyIOEvents();


  static bool g_easy_move;

  // Private but needed by system code
public:
  void SizeChanged();
  void AlwaysOnTopChanged();
  void VisibleChanged();
  void WindowDragableChanged() {}

  static void FindMonitors();
  static void InitDraggingOrSizing();
  static void MoveAllWindows();

private:
  void PlatformPaint(HDC dc);
  
  HBITMAP selected_bmp_;
  HWND hwnd_;
  HWND vis_hwnd_;
  HBITMAP bmp_screen_;
  HDC bmpdc_;
  HDC srcdc_;
  UINT shellhook_message_;
  Point last_window_rgn_size_;
};

class FileEnumerator {
public:
  explicit FileEnumerator(const char *directory);
  ~FileEnumerator();

  bool Next();

  bool is_directory() const { return !!(wfd_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); }
  char *filename() const { return (char*)wfd_.cFileName; }

private:
  WIN32_FIND_DATAA wfd_;
  HANDLE h_;
  bool first_;
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
  int pos_;
  HMENU curr_;
  HMENU stack_[64];
};


char *PlatformReadClipboard();
bool PlatformWriteClipboard(const char *text);

static unsigned int PlatformGetTicks() { return GetTickCount(); }

PlatformWindow *InitSpotamp(int argc, char **argv);



// Preferences
void PrefInit();
int PrefReadInt(int def, const char *name, ...);
bool PrefReadBool(bool def, const char *name, ...);
const char *PrefReadStr(const char *def, const char *name, ...);
void PrefWriteInt(int v, const char *name, ...);
void PrefWriteStr(const char *v, const char *name, ...);

void OpenUrl(const char *url);

const char *PlatformEnumAudioDevices(int i);

int MsgBox(const char *message, const char *title, int flags);



#endif  // SPOTIFYAMP_WINDOW_WIN32_H_
