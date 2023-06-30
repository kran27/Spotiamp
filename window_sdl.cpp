#include "stdafx.h"
#if defined(WITH_SDL)
#include "window.h"
#include <stdio.h>
#include "Resource.h"
#include "SDL.h"
#include "SDL_surface.h"
#include "SDL_clipboard.h"
#include "SDL_video.h"

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "sdl2.lib")

static bool g_quit;
static Point g_drag_start;

PlatformWindow::PlatformWindow() {
  active_window_ = true;
  need_repaint_ = true;
}

PlatformWindow::~PlatformWindow() {
}

void PlatformWindow::Create(PlatformWindow *owner_window) {
  PlatformWindowBase::Create(owner_window);
  window_ = SDL_CreateWindow("SpotifyAmp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             width_, height_, SDL_WINDOW_BORDERLESS | (visible_ ? 0 : SDL_WINDOW_HIDDEN));
  window_id_ = SDL_GetWindowID(window_);
  surface_ = SDL_GetWindowSurface(window_);
}

void PlatformWindow::Blit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy) {
  SDL_Surface *srcb = (SDL_Surface*)src;
  SDL_Rect srcrect = {sx,sy,w,h};
  SDL_Rect dstrect = {dx,dy,w,h};
  SDL_BlitSurface(srcb, &srcrect, surface_, &dstrect);
}

void PlatformWindow::StretchBlit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy, int sw, int sh) {
  SDL_Surface *srcb = (SDL_Surface*)src;
  SDL_Rect srcrect = {sx,sy,sw,sh};
  SDL_Rect dstrect = {dx,dy,w,h};
  SDL_BlitScaled(srcb, &srcrect, surface_, &dstrect);
}

void PlatformWindow::Repaint() {
  need_repaint_ = true;
}

void PlatformWindow::Minimize() {
  SDL_MinimizeWindow(window_);
}

void PlatformWindow::InitDraggingOrSizing() {
  SDL_GetMouseState(&g_drag_start.x, &g_drag_start.y);
}

void PlatformWindow::PlatformPaint() {
  if (need_repaint_ && visible_) {
    need_repaint_ = false;
    Paint();
    SDL_UpdateWindowSurface(window_);
  }
}

void PlatformWindow::Quit() {
  g_quit = true;
}

PlatformWindow *PlatformWindow::FromID(Uint32 window_id) {
  for(PlatformWindow *w = g_platform_windows; w; w = w->next())
    if (w->window_id_ == window_id)
      return w;
  return NULL;
}


void PlatformWindow::HandleEvent(SDL_Event *event) {
  switch(event->type) {
  case SDL_WINDOWEVENT: {
    if (event->window.type == SDL_WINDOWEVENT_EXPOSED) {
      PlatformWindow *w = FromID(event->window.windowID);
      if (w) w->Repaint();
    }
    break;
  }
  case SDL_KEYDOWN:
    break;
  case SDL_MOUSEMOTION: {
    PlatformWindow *w = FromID(event->motion.windowID);
    if (w) w->MouseMove(event->motion.x, event->motion.y);
 /*   if (dragging_) {
      int x,y;
      int mx,my;
      //int mx = event->motion.x, my = event->motion.y;
      SDL_GetWindowPosition(window_, &x, &y);
      SDL_GetMouseState(&mx, &my);

      x += mx - drag_start_x_;
      y += my - drag_start_y_;
//      drag_start_x_ = mx;
//      drag_start_y_ = my;

      SDL_SetWindowPosition(window_, x, y);
    }*/
    break;
  }
  case SDL_MOUSEBUTTONDOWN:
    if (event->button.button == SDL_BUTTON_LEFT) {
      PlatformWindow *w = FromID(event->button.windowID);
      if (w) {
        SDL_SetWindowGrab(w->window_, SDL_TRUE);
        w->LeftButtonDown(event->button.x, event->button.y);
      }
    }
    break;
  case SDL_MOUSEBUTTONUP:
    if (event->button.button == SDL_BUTTON_LEFT) {
      PlatformWindow *w = FromID(event->button.windowID);
      if (w) {
        SDL_SetWindowGrab(w->window_, SDL_FALSE);
        g_num_drag_windows = 0;
        w->LeftButtonUp(event->button.x, event->button.y);
      }
    }
    break;
  case SDL_QUIT:
    g_quit = true;
    break;
  }
}

void PlatformWindow::MoveAllWindows() {
  for(PlatformWindow *w = g_platform_windows; w; w = w->next()) {
    if (w->need_move_) {
      const Rect *r = w->screen_rect();
      SDL_SetWindowPosition(w->window_, r->left, r->top);
      SDL_SetWindowSize(w->window_, r->right - r->left, r->bottom - r->top);
      w->need_move_ = false;
    }
  }
}

void PlatformWindow::FindMonitors() {

}

void PlatformWindow::VisibleChanged() {
  if (!window_) return;
  if (visible_) {
    SDL_ShowWindow(window_);
    need_repaint_ = true;
  } else {
    SDL_HideWindow(window_);
  }
}

void PlatformWindow::AlwaysOnTopChanged() {

}

void PlatformWindow::Line(int x, int y, int x2, int y2, unsigned int color) {

}

void PlatformWindow::SetPixel(int x, int y, unsigned int color) {

}

unsigned int PlatformWindow::GetBitmapPixel(Bitmap *src, int x, int y) {
  return 0;
}

void PlatformWindow::Move(int l, int t) {
  if (window_)
    SDL_SetWindowPosition(window_, l, t);
}

void PlatformWindow::BringWindowToTop() {
  if (window_)
    SDL_RaiseWindow(window_);
}

extern "C" int _stdcall WinMain(int a, int b, int c, int d) {
  SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);
#if 0
  SDL_Window *win = SDL_CreateWindow("test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             275, 348, SDL_WINDOW_BORDERLESS);
  SDL_Surface *surf = SDL_GetWindowSurface(win);
  SDL_Surface *bmp = SDL_LoadBMP("testfile.bmp");
  SDL_Event ev;
  while (true) {
    SDL_WaitEventTimeout(&ev, 10);
    {
      SDL_Rect srcrect = {0, 0, 25, 20};
      SDL_Rect dstrect = {0, 0, 25, 20};
      SDL_BlitSurface(bmp, &srcrect, surf, &dstrect);
    }
    {
      SDL_Rect srcrect = {0, 42, 25, 29};
      SDL_Rect dstrect = {0, 20, 25, 290};
      SDL_BlitScaled(bmp, &srcrect, surf, &dstrect);
    }
    SDL_UpdateWindowSurface(win);
  }
#endif

  InitSpotamp(__argc, __argv);
  
  SDL_Event event;
  while (!g_quit) {
    if (SDL_WaitEventTimeout(&event, 10))
      PlatformWindow::HandleEvent(&event);
    g_main_window->MainLoop();
    for(PlatformWindow *w = g_platform_windows; w; w = w->next())
      w->PlatformPaint();
  }

  return 0;
}

Bitmap *PlatformLoadBitmap(const char *name) {
  char buf[256];
  sprintf(buf, "skin/%s", name);
  Bitmap *r = (Bitmap*)SDL_LoadBMP(buf);
  const char*er = SDL_GetError();
  return r;
}

char *PlatformReadClipboard() {
  static char *data;
  if (data) { SDL_free(data); data = NULL; }
  data = SDL_GetClipboardText();
  return data;
}

#endif  // defined(WITH_SDL)