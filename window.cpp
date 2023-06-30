// This file contains common windowing functions
#include "stdafx.h"
#include "window.h"
#include <stdio.h>
#include <Shlobj.h>

Rect g_monitor_rects[16];
int g_num_monitor_rects;
PlatformWindow *g_platform_windows;
PlatformWindow *g_main_window;
PlatformWindow *g_drag_windows[16];
int g_num_drag_windows;
bool g_drag_sizing;
Point g_drag_start;

// Returns if b is a neighbor to a, returns the side 0=left, 1=top, 2=right, 3=bottom, -1 = no
static int IsNeighbour(const Rect *a, const Rect *b) {
  int res = 0;
  if (a->right > b->left && a->left < b->right) {
    if (a->bottom == b->top) res |= 1<<3;
    if (a->top == b->bottom) res |= 1<<1;
  }
  if (a->bottom > b->top && a->top < b->bottom) {
    if (a->right == b->left) res |= 1<<2;
    if (a->left == b->right) res |= 1<<0;
  }
  return res;
}

// Returns if b is a neighbor to a, returns the side 0=left, 1=top, 2=right, 3=bottom, -1 = no
static int IsNeighbourInner(const Rect *a, const Rect *b) {
  int res = 0;
  if (a->right > b->left && a->left < b->right) {
    if (a->bottom == b->bottom) res |= 1<<3;
    if (a->top == b->top) res |= 1<<1;
  }
  if (a->bottom > b->top && a->top < b->bottom) {
    if (a->right == b->right) res |= 1<<2;
    if (a->left == b->left) res |= 1<<0;
  }
  return res;
}

// Returns a bitmask of the screen edges touched by the windows.
static int AnyTouchesScreenEdge(PlatformWindow **ws, int nw) {
  int result = 0;
  for(int i = 0; i < nw; i++) {
    const Rect *a = ws[i]->screen_rect();
    for(int j = 0; j < g_num_monitor_rects; j++) {
      const Rect *b = &g_monitor_rects[j];
      result |= IsNeighbourInner(a, b);
    }
  }
  return result;
}

struct SnapData {
  Rect r;
  int bestx, besty;
};

static void SnapInner(SnapData *sd, const Rect *r) {
  if (sd->r.top <= r->bottom && sd->r.bottom >= r->top) {
    if (abs(sd->r.left - r->left) < abs(sd->bestx)) 
      sd->bestx = sd->r.left - r->left;
    if (abs(sd->r.right - r->right) < abs(sd->bestx))
      sd->bestx = sd->r.right - r->right;
  }
  if (sd->r.left <= r->right && sd->r.right >= r->left) {
    if (abs(sd->r.top - r->top) < abs(sd->besty))
      sd->besty = sd->r.top - r->top;
    if (abs(sd->r.bottom - r->bottom) < abs(sd->besty))
      sd->besty = sd->r.bottom - r->bottom;
  }
}

static void SnapOuter(SnapData *sd, const Rect *r) {
  if (sd->r.top <= r->bottom + 10 && sd->r.bottom >= r->top - 10) {
    if (abs(sd->r.left - r->right) < abs(sd->bestx)) sd->bestx = sd->r.left - r->right;
    if (abs(sd->r.right - r->left) < abs(sd->bestx)) sd->bestx = sd->r.right - r->left;
  }
  if (abs(sd->r.top - r->bottom) < 10 || abs(sd->r.bottom - r->top) < 10) {
    if (abs(sd->r.left - r->left) < abs(sd->bestx)) sd->bestx = sd->r.left - r->left;
    if (abs(sd->r.right - r->right) < abs(sd->bestx)) sd->bestx = sd->r.right - r->right;
  }
  if (sd->r.left <= r->right + 10 && sd->r.right >= r->left - 10) {
    if (abs(sd->r.top - r->bottom) < abs(sd->besty)) sd->besty = sd->r.top - r->bottom;
    if (abs(sd->r.bottom - r->top) < abs(sd->besty)) sd->besty = sd->r.bottom - r->top;
  }
  if (abs(sd->r.left - r->right) < 10 || abs(sd->r.right - r->left) < 10) {
    if (abs(sd->r.top - r->top) < abs(sd->besty))       sd->besty = sd->r.top - r->top;
    if (abs(sd->r.bottom - r->bottom) < abs(sd->besty)) sd->besty = sd->r.bottom - r->bottom;
  }
}

static inline bool HasWindow(PlatformWindow **ws, int nw, PlatformWindow *w) {
  for(int i = 0; i < nw; i++)
    if (ws[i] == w) return true;
  return false;
}

void SnapToScreenEdges(PlatformWindow **ws, int nw, int *dxp, int *dyp) {
  SnapData data;
  data.bestx = data.besty = 10;
  int dx = *dxp, dy = *dyp;
  for(int i = 0; i < nw; i++) {
    data.r = *ws[i]->screen_rect();
    data.r.left += dx;
    data.r.right += dx;
    data.r.top += dy;
    data.r.bottom += dy;

    for(int i = 0; i < g_num_monitor_rects; i++)
      SnapInner(&data, &g_monitor_rects[i]);

    // Also go through all windows
    for(PlatformWindow *w = g_platform_windows; w; w = w->next()) {
      if (!HasWindow(ws, nw, w))
        SnapOuter(&data, w->screen_rect());
    }
  }
  if (data.bestx != 10) *dxp = dx - data.bestx; 
  if (data.besty != 10) *dyp = dy - data.besty; 
}

int FindConnectedWindows(PlatformWindow *start, PlatformWindow **ws, int mask) {
  int n;
  ws[0] = start;
  n = 1;
  for(int i = 0; i < n; i++) {
    // Find all neighbors to w[i] that are not already in the set
    for(PlatformWindow *w = g_platform_windows; w; w = w->next()) {
      if (!HasWindow(ws, n, w)) {
        if (IsNeighbour(ws[i]->screen_rect(), w->screen_rect()) & mask)
          ws[n++] = w; 
      }
    }
  }
  return n;
}


// Find all connected windows BELOW this window, and see if
// any of them touches the screen edge.
// If touching screen edge, move things UP instead.
// else, move all found windows down.
template<>
PlatformWindowBase<PlatformWindow>::PlatformWindowBase() {
  double_size_ = false;
  always_on_top_ = false;
  active_window_ = false;
  visible_ = false;
  need_move_ = false;
  dragable_ = false;
  width_ = height_ = 0;
  owner_ = NULL;
  next_ = g_platform_windows;
  g_platform_windows = self();
  memset(&screen_rect_, 0, sizeof(screen_rect_));
}

template<>
PlatformWindowBase<PlatformWindow>::~PlatformWindowBase() {
  PlatformWindow **p = &g_platform_windows;
  while (*p != this) p = &(*p)->next_;
  *p = (*p)->next_;
}

template<>
void PlatformWindowBase<PlatformWindow>::Create(PlatformWindow *owner_window) {
   owner_ = owner_window;
  if (owner_ == NULL)
    g_main_window = self();
}


template<>
void PlatformWindowBase<PlatformWindow>::SetDoubleSize(bool v) {
  if (v != double_size_) {
    double_size_ = v;
    SizeChanged();
  }
}

template<>
void PlatformWindowBase<PlatformWindow>::SetAlwaysOnTop(bool v) {
  if (v != always_on_top_) {
    always_on_top_ = v;
    static_cast<PlatformWindow*>(this)->AlwaysOnTopChanged();
  }
}

template<>
void PlatformWindowBase<PlatformWindow>::SetActive(bool active) {
  if (active !=  active_window_) {
    active_window_ = active;
    static_cast<PlatformWindow*>(this)->Repaint();
  }
}

template<>
void PlatformWindowBase<PlatformWindow>::SetVisible(bool visible) {
  if (visible != visible_) {
    visible_ = visible;
    static_cast<PlatformWindow*>(this)->VisibleChanged();
  }
}


template<>
void PlatformWindowBase<PlatformWindow>::SetWindowDragable(bool dragable) {
  if (dragable != dragable_) {
    dragable_ = dragable;
    static_cast<PlatformWindow*>(this)->WindowDragableChanged();
  }
}


template<>
void PlatformWindowBase<PlatformWindow>::Resize(int w, int h) {
  if (w != width_ || h != height_) {
    width_ = w;
    height_ = h;
    SizeChanged();
  }
}

template<>
void PlatformWindowBase<PlatformWindow>::SizeChanged() {
  int h = height_ << (int)double_size_;
  int w = width_ << (int)double_size_;
  PlatformWindow *wsx[16], *wsy[16], *wnd = static_cast<PlatformWindow*>(this);
  int deltax, deltay, nwx, nwy, first_to_move_x, first_to_move_y;

  deltay = (h - (wnd->screen_rect_.bottom - wnd->screen_rect_.top));
  deltax = (w - (wnd->screen_rect_.right - wnd->screen_rect_.left));

  nwy = FindConnectedWindows(wnd, wsy, 8);
  first_to_move_y = 1;
  if (AnyTouchesScreenEdge(wsy, nwy) & 8) {
    nwy = FindConnectedWindows(wnd, wsy, 2);
    deltay = -deltay;
    first_to_move_y = 0;
  }

  nwx = FindConnectedWindows(wnd, wsx, 4);
  first_to_move_x = 1;
  if (AnyTouchesScreenEdge(wsx, nwx) & 4) {
    nwx = FindConnectedWindows(wnd, wsx, 1);
    deltax = -deltax;
    first_to_move_x = 0;
  }

  for(int i = first_to_move_y; i < nwy; i++) {
    PlatformWindow *ww = wsy[i];
    ww->screen_rect_.top += deltay;
    ww->screen_rect_.bottom += deltay;
    ww->need_move_ = true;
  }
  for(int i = first_to_move_x; i < nwx; i++) {
    PlatformWindow *ww = wsx[i];
    ww->screen_rect_.left += deltax;
    ww->screen_rect_.right += deltax;
    ww->need_move_ = true;
  }

  // Update the size of myself.
  screen_rect_.bottom = screen_rect_.top + h;
  screen_rect_.right = screen_rect_.left + w;
  need_move_ = true;
  PlatformWindow::MoveAllWindows();
}

template<>
void PlatformWindowBase<PlatformWindow>::InitWindowDragging() {
  // If dragging the main window, drag all windows that are connected as one unit.
  if (owner_ == NULL) {
    g_num_drag_windows = FindConnectedWindows(self(), g_drag_windows, 0xF);
  } else {
    g_drag_windows[0] = self();
    g_num_drag_windows = 1;
  }
  g_drag_sizing = false;
  PlatformWindow::FindMonitors();
  PlatformWindow::InitDraggingOrSizing();
}

template<>
void PlatformWindowBase<PlatformWindow>::InitWindowSizing() {
  g_drag_windows[0] = self();
  g_num_drag_windows = 1;
  g_drag_sizing = true;
  PlatformWindow::InitDraggingOrSizing();
}

template<>
Point PlatformWindowBase<PlatformWindow>::HandleMouseDragOrSize(int dx, int dy, bool snap) {
  if (dx | dy) {
    if (!g_drag_sizing) {
      if (snap)
        SnapToScreenEdges(g_drag_windows, g_num_drag_windows, &dx, &dy);
      for(int i = 0; i < g_num_drag_windows; i++) {
        PlatformWindow *w = g_drag_windows[i];
        w->screen_rect_.left += dx;
        w->screen_rect_.right += dx;
        w->screen_rect_.top += dy;
        w->screen_rect_.bottom += dy;
        w->need_move_ = true;
      }
      PlatformWindow::MoveAllWindows();
    } else {
      int w = width_ + dx, h = height_ + dy;
      if (self()->SizeChanging(&w, &h)) {
        dx = w - width_;
        dy = h - height_;
        Resize(w, h);
      }
    }
  }
  Point rv = {dx, dy};
  return rv;
}


#ifdef _MSC_VER

static char inifile[MAX_PATH + 100];
extern char exepath[MAX_PATH];

const char *GetIniFilePath(){return inifile;}

void PrefInit() {
  sprintf(inifile, "%sSpotiamb.ini", exepath);
  // If Spotiamp exists, use it.
  if (GetFileAttributesA(inifile) == INVALID_FILE_ATTRIBUTES) {
    // Try to make it.
    HANDLE h = CreateFileA(inifile, GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
      // Use profile directory instead.
      strcpy(inifile, "C:");
      SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, inifile);
      strcat(inifile, "\\Spotiamb");
      CreateDirectoryA(inifile, NULL);
      strcat(inifile, "\\Spotiamb.ini");
    } else {
      CloseHandle(h);
    }
  }

}

int PrefReadInt(int def, const char *name, ...) {
  char buf[256];
  va_list va;
  va_start(va, name);
  vsprintf(buf, name, va);
  va_end(va);
  return GetPrivateProfileIntA("Main", buf, def, inifile);
}

bool PrefReadBool(bool def, const char *name, ...) {
  char buf[256];
  va_list va;
  va_start(va, name);
  vsprintf(buf, name, va);
  va_end(va);
  return GetPrivateProfileIntA("Main", buf, def, inifile) != 0;
}


const char *PrefReadStr(const char *def, const char *name, ...) {
  char buf[256];
  static char databuf[4096];
  va_list va;
  va_start(va, name);
  vsprintf(buf, name, va);
  va_end(va);

  if (!GetPrivateProfileStringA("Main", buf, def, databuf, sizeof(databuf), inifile))
    return def;
  return databuf;
}

void PrefWriteInt(int v, const char *name, ...) {
  char buf[256];
  char buf2[32];
  va_list va;
  va_start(va, name);
  vsprintf(buf, name, va);
  va_end(va);
  sprintf(buf2, "%d", v);
  WritePrivateProfileStringA("Main", buf, buf2, inifile);
}

void PrefWriteStr(const char *v, const char *name, ...) {
  char buf[256];
  va_list va;
  va_start(va, name);
  vsprintf(buf, name, va);
  va_end(va);
  WritePrivateProfileStringA("Main", buf, v, inifile);
}

#else  // defined(_MSC_VER)

void PrefInit() {}
int PrefReadInt(int def, const char *name, ...) { return def; }
bool PrefReadBool(bool def, const char *name, ...) { return def; }
const char *PrefReadStr(const char *def, const char *name, ...) { return def; }
void PrefWriteInt(int v, const char *name, ...) {}
void PrefWriteStr(const char *v, const char *name, ...) {}

#endif  // defined(_MSC_VER)

