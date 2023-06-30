#include "stdafx.h"
#if !defined(WITH_SDL)
#include "window.h"
#include <stdio.h>
#include "Resource.h"
#include "types.h"
#include <Sddl.h>
#include <wininet.h>
#include <string>
#include <Shellapi.h>
#include "zipfile.h"
#include <D2D1.h>
#include <Wincodec.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <mmsystem.h>

extern "C" {
#include "tiny_spotify.h"
};

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "wininet.lib")
#pragma comment (lib, "Msimg32.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "Windowscodecs.lib")

#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

void VisPlugin_Stop();


static std::string g_auto_update_msg, g_auto_update_caption, g_auto_update_url;
static int g_auto_update_flags;
bool g_auto_update_available;

static HINSTANCE hInst;								// current instance
static Point g_drag_start;

bool PlatformWindow::g_easy_move;

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
  MONITORINFO info;
  info.cbSize = sizeof(info);
  if (g_num_monitor_rects < 16 && GetMonitorInfo(hMonitor, &info))
    memcpy(&g_monitor_rects[g_num_monitor_rects++], &info.rcWork, sizeof(Rect));
  return TRUE;
}

void PlatformWindow::FindMonitors() {
  g_num_monitor_rects = 0;
  EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,NULL);
}


// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
static LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK	WndProcVis(HWND, UINT, WPARAM, LPARAM);

PlatformWindow::PlatformWindow() {
  active_window_ = true;
  shellhook_message_ = 0;
  selected_bmp_ = NULL;
  hwnd_ = NULL;
  vis_hwnd_ = NULL;
  bmp_screen_ = NULL;
  bmpdc_ = NULL;
  srcdc_ = NULL;
  last_window_rgn_size_.x = last_window_rgn_size_.y = NULL;

}

PlatformWindow::~PlatformWindow() {
  if (hwnd_) DestroyWindow(hwnd_);
  if (vis_hwnd_) DestroyWindow(vis_hwnd_);
}

void PlatformWindow::Blit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy) {
  HBITMAP srcb = (HBITMAP)src;
  if (srcb != selected_bmp_) {
    selected_bmp_ = srcb;
    SelectObject(srcdc_, srcb);
  }
  BitBlt(bmpdc_, dx, dy, w, h, srcdc_, sx, sy, SRCCOPY);
}

void PlatformWindow::AlphaBlit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy) {
  HBITMAP srcb = (HBITMAP)src;
  if (srcb != selected_bmp_) {
    selected_bmp_ = srcb;
    SelectObject(srcdc_, srcb);
  }
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  AlphaBlend(bmpdc_, dx, dy, w, h, srcdc_, sx, sy, w, h, blend);
}

void PlatformWindow::StretchBlit(int dx, int dy, int w, int h, Bitmap *src, int sx, int sy, int sw, int sh) {
  HBITMAP srcb = (HBITMAP)src;
  if (srcb != selected_bmp_) {
    selected_bmp_ = srcb;
    SelectObject(srcdc_, srcb);
  }
  StretchBlt(bmpdc_, dx, dy, w, h, srcdc_, sx, sy, sw, sh, SRCCOPY);
}

void PlatformWindow::Fill(int x, int y, int w, int h, unsigned int color) {
  RECT rect = {x, y, x+w, y+h};
  COLORREF old = ::SetBkColor(bmpdc_, color & 0xffffff);
  ::ExtTextOut(bmpdc_, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
  ::SetBkColor(bmpdc_, old);  
}

void PlatformWindow::DrawText(int x, int y, int w, int h,
                              const char *text, int flags, unsigned int color) {
  wchar_t buffer[1024];
  RECT rect = {x, y, x + w, y + h};
  int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, 1024);
  if (len > 0) {
    ::SetTextColor(bmpdc_, color);
    ::DrawTextW(bmpdc_, buffer, len - 1, &rect, (flags&1) ? 
                DT_END_ELLIPSIS | DT_RIGHT | DT_VCENTER | DT_NOPREFIX : 
                DT_END_ELLIPSIS | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
  }
}

void PlatformWindow::Line(int x, int y, int x2, int y2, unsigned int color) {
  HPEN pen = ::CreatePen(PS_SOLID, 1, color);
  HPEN old_pen = (HPEN)::SelectObject(bmpdc_, pen);
  ::MoveToEx(bmpdc_, x, y, NULL);
  ::LineTo(bmpdc_, x2, y2);
  ::SelectObject(bmpdc_, old_pen);
  ::DeleteObject(pen);
}

static std::string last_font;
static int last_font_size;
static HFONT last_font_handle;

void PlatformWindow::SetFont(const char *fontname, int size) {
  if (fontname != last_font || last_font_size != size) {
    last_font = fontname;
    last_font_size = size;
    if (last_font_handle)
      DeleteObject(last_font_handle);
    int size = -MulDiv(last_font_size, GetDeviceCaps(bmpdc_, LOGPIXELSY), 96);
    last_font_handle = CreateFontA(size, 0, 0, 0, FALSE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY  , DEFAULT_PITCH, last_font.c_str());
  }
  SelectObject(bmpdc_, last_font_handle);
}

int PlatformWindow::GetFontHeight() {
  TEXTMETRIC tm;;
  if (!GetTextMetrics(bmpdc_, &tm))
    return 10;
  return tm.tmHeight;
}

void PlatformWindow::PlatformPaint(HDC dc) {
  bmpdc_ = CreateCompatibleDC(dc);
  srcdc_ = CreateCompatibleDC(dc);

  ::SetBkMode(bmpdc_, TRANSPARENT);
  ::SetStretchBltMode(bmpdc_, COLORONCOLOR);

  if (bmp_screen_) {
    BITMAP b;
    if (!GetObject(bmp_screen_, sizeof(b), &b) || b.bmWidth != width_ || b.bmHeight != height_) {
      DeleteBitmap(bmp_screen_);
      bmp_screen_ = NULL;
    }
  }

  if (!bmp_screen_)
    bmp_screen_ = CreateCompatibleBitmap(dc, width_, height_);
  SelectObject(bmpdc_, bmp_screen_);
  selected_bmp_ = NULL;

  // Copy the dirty region to the bitmap
  if (!double_size_) {
    HRGN rgn = CreateRectRgn(0, 0, 0, 0);
    GetRandomRgn(dc, rgn, SYSRGN);
    POINT pt = {0, 0};
    MapWindowPoints(NULL, hwnd_, &pt, 1);
    OffsetRgn(rgn, pt.x, pt.y);
    SelectClipRgn(bmpdc_, rgn);
    DeleteObject(rgn);
  } else {
    RECT rect;
    GetClipBox(dc, &rect);
    rect.left >>= 1;
    rect.top >>= 1;
    rect.right = (rect.right + 1) >> 1;
    rect.bottom = (rect.bottom + 1) >> 1;
    HRGN rgn = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
    SelectClipRgn(bmpdc_, rgn);
    DeleteObject(rgn);
  }

  // Invoke subclass to do the actual drawing
  Paint();

  StretchBlt(dc, 0, 0, width_<<(int)double_size_, height_<<(int)double_size_, bmpdc_, 0, 0, width_, height_, SRCCOPY);
  DeleteDC(bmpdc_);
  DeleteDC(srcdc_);
  bmpdc_ = srcdc_ = 0;
}

void PlatformWindow::Repaint() {
  if (hwnd_)
    InvalidateRect(hwnd_, NULL, FALSE);
}

void PlatformWindow::RepaintRange(int x, int y, int w, int h) {
  if (hwnd_) {
    RECT r = {x << (int)double_size_, y << (int)double_size_,
             (x + w) << (int)double_size_, (y + h) << (int)double_size_};
    InvalidateRect(hwnd_, &r, FALSE);
  }
}

void PlatformWindow::SetPixel(int x, int y, unsigned int color) {
  ::SetPixelV(bmpdc_, x, y, (COLORREF)color);
}

unsigned int PlatformWindow::GetBitmapPixel(Bitmap *src, int x, int y) {
  HBITMAP srcb = (HBITMAP)src;
  if (srcb != selected_bmp_) {
    selected_bmp_ = srcb;
    SelectObject(srcdc_, srcb);
  }
  return GetPixel(srcdc_, x, y);
}

// Move all windows so they correspond to the screen rects.
void PlatformWindow::MoveAllWindows() {
  HDWP dp = BeginDeferWindowPos(16);
  for(PlatformWindow *w = g_platform_windows; w; w = w->next()) {
    if (w->need_move_) {
      const Rect *r = w->screen_rect();
      dp = DeferWindowPos(dp, w->handle(), NULL,
                      r->left, r->top,
                      r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
      w->need_move_ = false;
    }
  }
  EndDeferWindowPos(dp);
}

void PlatformWindow::Move(int l, int t) {
  if (hwnd_) {
    SetWindowPos(hwnd_, 0, l, t, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREPOSITION);
  }
}

void PlatformWindow::AlwaysOnTopChanged() {
  if (hwnd_) {
    SetWindowPos(hwnd_, always_on_top_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 
                 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOCOPYBITS | SWP_NOACTIVATE);
  }
}
  
void PlatformWindow::Minimize() {
  ShowWindow(hwnd_, SW_MINIMIZE);
}

void PlatformWindow::BringWindowToTop() {
  if (hwnd_)
    ::BringWindowToTop(hwnd_);
}

void PlatformWindow::VisibleChanged() {
  if (hwnd_)
    ShowWindow(hwnd_, visible_ ? SW_SHOWNA : SW_HIDE);
}

void PlatformWindow::RegisterForGlobalHotkeys() {
  if (!shellhook_message_)
    shellhook_message_ = RegisterWindowMessage(TEXT("SHELLHOOK"));
  if (hwnd_) {
    typedef BOOL WINAPI RegisterShellHookWindowProc(HWND hWnd);
    RegisterShellHookWindowProc *proc = (RegisterShellHookWindowProc*)GetProcAddress(GetModuleHandleA("user32.dll"), "RegisterShellHookWindow");
    if (proc)
      proc(hwnd_);
  }
}

void PlatformWindow::InitDraggingOrSizing() {
  ::GetCursorPos((POINT*)&g_drag_start);
}

void PlatformWindow::Create(PlatformWindow *owner_window) {
  PlatformWindowBase::Create(owner_window);
                                                                               
  hwnd_ = ::CreateWindowEx(owner_window ? 0 : WS_EX_WINDOWEDGE, owner_window ? L"SpotiambWnd" : L"SpotiambMain", 
    owner_window ? L"" : L"Spotiamb", 
    owner_window ? WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE : 
                   WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE | WS_MINIMIZEBOX | WS_SYSMENU,
    CW_USEDEFAULT, 200, width_ << (int)double_size_, height_ << (int)double_size_,
    owner_window ? owner_window->handle() : NULL, NULL, hInst, this);
  if (!hwnd_)
    return;

  if (shellhook_message_)
    RegisterForGlobalHotkeys();

  if (owner_window == NULL) {
    SetTimer(hwnd_, 13, 10, NULL);
    SetTimer(hwnd_, 14, 200, NULL);
  }

  if (visible_) {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
  }
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wcex.lpfnWndProc	= WndProc;
  wcex.cbClsExtra		= 0;
  wcex.cbWndExtra		= 0;
  wcex.hInstance		= hInstance;
  wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SPOTIFYAMP));
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground	= NULL;
  wcex.lpszMenuName	= NULL;
  wcex.lpszClassName	= L"SpotiambWnd";
  wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SPOTIFYAMP));
  RegisterClassEx(&wcex);

  wcex.lpszClassName	= L"SpotiambMain";
  RegisterClassEx(&wcex);
     
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style		= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wcex.lpfnWndProc	= WndProcVis;
  wcex.cbClsExtra	= 0;
  wcex.cbWndExtra	= 0;
  wcex.hInstance	= hInstance;
  wcex.hIcon		= 0;
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground	= GetStockBrush(BLACK_BRUSH);
  wcex.lpszMenuName	= NULL;
  wcex.lpszClassName	= L"SpotiambVis";
  wcex.hIconSm		= NULL;
  RegisterClassEx(&wcex);
  return true;
}

void PlatformWindow::NotifyIOEvents() {
  ::PostMessage(hwnd_, WM_USER + 100, 0, 0);
}

LRESULT HandlePluginApi(WPARAM wParam, LPARAM lParam);

BOOL PlatformWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *retval) {
  PAINTSTRUCT ps;
  HDC hdc;
  switch (message) {
  case WM_USER:
  case WM_USER + 1:
  case WM_USER + 2:
  case WM_USER + 3:
    if (lParam == 201) return 0;
    *retval = HandlePluginApi(wParam, lParam);
    break;
  case WM_USER + 100:
    PumpIOEvents();
    return 0;
  case WM_CLOSE:
    if (this == g_main_window)
      PostQuitMessage(0);
    break;
  case WM_PAINT:
    hdc = BeginPaint(hWnd, &ps);
    PlatformPaint(hdc);
    EndPaint(hWnd, &ps);
    break;
  case WM_TIMER:
    if (wParam == 13) {
      MainLoop();
    } else if (wParam == 14) {
      ScrollText();
    }
    break;
  case WM_ERASEBKGND:
    *retval = TRUE;
    break;
  case WM_KEYDOWN: {
    int ss = (GetAsyncKeyState(VK_CONTROL) < 0) * MOD_CONTROL + 
             (GetAsyncKeyState(VK_SHIFT) < 0) * MOD_SHIFT + 
             (GetAsyncKeyState(VK_MENU) < 0) * MOD_ALT;
    KeyDown(wParam, ss);
    } break;
  case WM_SYSKEYDOWN: {
    if (wParam < 'A' || wParam > 'Z')
      return FALSE;
    KeyDown(wParam, MOD_ALT);
    } break;
  case WM_SYSCHAR: {
    if ( (wParam | 32) >= 'a' && (wParam | 32) <= 'z')
      return TRUE;
    return FALSE;
  }
  case WM_LBUTTONDOWN: {
    SetCapture(hWnd);
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    LeftButtonDown(x,y);
    if (dragable_) {
      InitWindowDragging();
    }
    break;
  }
  case WM_RBUTTONDOWN: {
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    RightButtonDown(x,y);
    break;
  }
  case WM_RBUTTONUP: {
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    RightButtonUp(x,y);
    break;
  }
  case WM_LBUTTONDBLCLK: {
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    LeftButtonDouble(x,y);
    break;
  }
  case WM_LBUTTONUP: {
    g_num_drag_windows = 0;
    ReleaseCapture();
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    LeftButtonUp(x,y);
    break;
  }
  case WM_WINDOWPOSCHANGED: {
    GetWindowRect(hWnd, (RECT*)&screen_rect_);
    if (last_window_rgn_size_.x != screen_rect_.right - screen_rect_.left ||
        last_window_rgn_size_.y != screen_rect_.bottom - screen_rect_.top) {
      last_window_rgn_size_.x = screen_rect_.right - screen_rect_.left;
      last_window_rgn_size_.y = screen_rect_.bottom - screen_rect_.top;
      SetWindowRgn(hWnd, CreateRectRgn(0, 0, screen_rect_.right - screen_rect_.left, screen_rect_.bottom - screen_rect_.top), FALSE); 
    }
    need_move_ = false;
    break;
  }
  case WM_MOUSEWHEEL: {
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    MouseWheel(x,y,0,GET_WHEEL_DELTA_WPARAM(wParam));
    break;
  }

  case WM_MOUSEMOVE: {
    int d = double_size();
    int x = GET_X_LPARAM(lParam) >> d, y = GET_Y_LPARAM(lParam) >> d;
    MouseMove(x,y);
    if (g_num_drag_windows != 0) {
      Point pt;
      GetCursorPos((POINT*)&pt);    
      pt = HandleMouseDragOrSize(pt.x - g_drag_start.x, pt.y - g_drag_start.y, GetAsyncKeyState(VK_SHIFT) >= 0);
      g_drag_start.x += pt.x;
      g_drag_start.y += pt.y;
    }
    break;
  }
  case WM_HOTKEY:
    GlobalHotkey(wParam);
    break;
  case WM_ACTIVATE:
    SetActive(wParam != WA_INACTIVE);
    break;
  case WM_NCCALCSIZE:
    *retval = 0xF0u ;
    break;
  case WM_NCHITTEST:
    *retval = HTCLIENT;
    break;
  case WM_NCACTIVATE:
    *retval = TRUE;
    break;
  case WM_NCPAINT:
    break;
  case WM_GETMINMAXINFO: {
    ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 0;
    ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 0;
    break;
  }
  case WM_WINDOWPOSCHANGING:
    break;
  case WM_APPCOMMAND: {
      if (HandleAppCommandHotKeys(GET_APPCOMMAND_LPARAM(lParam),
                                  GET_KEYSTATE_LPARAM(lParam))) {
        *retval = 1;
      } else {
        return FALSE;
      }
    }
  default:
    if (shellhook_message_ && message == shellhook_message_) {
      if (wParam == HSHELL_APPCOMMAND) {
        HandleAppCommandHotKeys(GET_APPCOMMAND_LPARAM(lParam),
                                GET_KEYSTATE_LPARAM(lParam));
      }
      *retval = 1;
      return TRUE;
    }
    return FALSE;
  }
  return TRUE;
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    ((PlatformWindow*)((CREATESTRUCT*)lParam)->lpCreateParams)->hwnd_ = hWnd;
  }
  for(PlatformWindow *w = g_platform_windows; w; w = w->next_) {
    if (w->handle() == hWnd) {
      LRESULT retval = 0;
      if (w->WndProc(hWnd, message, wParam, lParam, &retval))
        return retval;
      break;
    }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

extern int vismode;

static LRESULT CALLBACK	WndProcVis(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_LBUTTONDOWN)
    vismode ^= 1;

  return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND PlatformWindow::SetVisualizer(int x, int y, int w, int h) {
  if (double_size()) {
    x *= 2;
    y *= 2;
    w *= 2;
    h *= 2;
  }
  if (vis_hwnd_ == 0) {
    vis_hwnd_ = ::CreateWindowEx(0, L"SpotiambVis", NULL, WS_CHILD | WS_VISIBLE, 
      x, y, w, h, hwnd_, NULL, GetModuleHandle(NULL), NULL);
  } else {
    SetWindowPos(vis_hwnd_, NULL, x, y, w, h, SWP_NOZORDER| SWP_NOACTIVATE| SWP_NOREPOSITION);
  }
  return vis_hwnd_;
}

char machine_id[128];

static bool GetMachineId(char buf[128]) {
  // Calculate the Windows SID.
  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = ARRAYSIZE(computer_name);
  buf[0] = 'a';
  buf[1] = '-';
  buf[2] = 0;
  BOOL success = ::GetComputerNameW(computer_name, &size);
  if (success) {
    char sid_buffer[SECURITY_MAX_SID_SIZE];
    SID* sid = reinterpret_cast<SID*>(sid_buffer);
    DWORD sid_dword_size = sizeof(sid_buffer), domain_size = 256;
    wchar_t domain[256];
    SID_NAME_USE sid_name_use;
    success = ::LookupAccountNameW(NULL, computer_name, sid, &sid_dword_size, domain, &domain_size, &sid_name_use);
    if (success) {
      wchar_t* sid_buffer = NULL;
      if (::ConvertSidToStringSidW(sid, &sid_buffer)) {
        WideCharToMultiByte(CP_UTF8, 0, sid_buffer, -1, buf + 2, 128, NULL, NULL);
        LocalFree(sid_buffer);
        return true;
      }
    }
  }
  return false;
}

char exepath[MAX_PATH];

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPSTR    lpCmdLine,
                       _In_ int       nCmdShow) {
  MSG msg;

  hInst = hInstance;
  CoInitialize(NULL);

  GetMachineId(machine_id);

  GetModuleFileNameA(NULL, exepath, MAX_PATH);
  int l = strlen(exepath);
  while (l > 0 && exepath[l-1] != '\\') l--;
  exepath[l] = 0;
  
  MyRegisterClass(hInstance);
  PrefInit();
  
  InitSpotamp(__argc, __argv);

  // Required for always-on-top to load
  if (g_main_window) g_main_window->AlwaysOnTopChanged();

  // Main message loop:
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  for(PlatformWindow *w = g_platform_windows; w; w = w->next())
    w->Destroy();

  VisPlugin_Stop();

  return (int) msg.wParam;
}

int main() {
  WinMain(GetModuleHandle(NULL), NULL, NULL, 0);
}

static ZipFile *current_skin_zip;
static std::string current_skin_basedir;

class SimpleFileIO : public FileIO {
public:
  SimpleFileIO(const char *filename);
  virtual ~SimpleFileIO();
  virtual size_t Read(int64 read_offs, void *buf, size_t buf_size);

private:
  FILE *f_;
};

SimpleFileIO::SimpleFileIO(const char *filename) {
  f_ = fopen(filename, "rb");
}

SimpleFileIO::~SimpleFileIO() {
  if (f_) fclose(f_);
}

size_t SimpleFileIO::Read(int64 read_offs, void *buf, size_t buf_size) {
  if (!f_) return 0;
  fseek(f_, (long)read_offs, SEEK_SET);
  return fread(buf, 1, buf_size, f_);
}

// Creates a 32-bit DIB from the specified WIC bitmap.
HBITMAP CreateHBITMAP(IWICBitmapSource * ipBitmap) {
  // initialize return value
  HBITMAP hbmp = NULL;
  // get image attributes and check for valid image
  UINT width = 0;
  UINT height = 0;
  if (FAILED(ipBitmap->GetSize(&width, &height)) || width == 0 || height == 0)
      goto Return;
  // prepare structure giving bitmap information (negative height indicates a top-down DIB)
  BITMAPINFO bminfo;
  ZeroMemory(&bminfo, sizeof(bminfo));
  bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bminfo.bmiHeader.biWidth = width;
  bminfo.bmiHeader.biHeight = -((LONG) height);
  bminfo.bmiHeader.biPlanes = 1;
  bminfo.bmiHeader.biBitCount = 32;
  bminfo.bmiHeader.biCompression = BI_RGB;
  // create a DIB section that can hold the image
  void * pvImageBits = NULL;
  HDC hdcScreen = GetDC(NULL);
  hbmp = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
  ReleaseDC(NULL, hdcScreen);
  if (hbmp == NULL)
      goto Return;
  // extract the image into the HBITMAP
  const UINT cbStride = width * 4;
  const UINT cbImage = cbStride * height;
  if (FAILED(ipBitmap->CopyPixels(NULL, cbStride, cbImage, static_cast<BYTE *>(pvImageBits)))) {
      DeleteObject(hbmp);
      hbmp = NULL;
  }
Return:
  return hbmp;
}

enum BitmapFormat {
  kFormatBmp,
  kFormatJpeg,
  kFormatPng
};

// Loads a PNG image from the specified stream (using Windows Imaging Component).
HBITMAP LoadBitmapFromStream(IStream *ipImageStream, BitmapFormat format) {
  // initialize return value
  IWICBitmapSource * ipBitmap = NULL;
  HBITMAP hBitmap = NULL;
  // load WIC's PNG decoder
  IWICBitmapDecoder * ipDecoder = NULL;
  if (FAILED(CoCreateInstance((format == kFormatBmp) ? CLSID_WICBmpDecoder : 
                              (format == kFormatPng) ? CLSID_WICPngDecoder :
                              CLSID_WICJpegDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(ipDecoder), reinterpret_cast<void**>(&ipDecoder))))
      goto Return;
  // load the PNG
  if (FAILED(ipDecoder->Initialize(ipImageStream, WICDecodeMetadataCacheOnLoad)))
      goto ReleaseDecoder;
  // check for the presence of the first frame in the bitmap
  UINT nFrameCount = 0;
  if (FAILED(ipDecoder->GetFrameCount(&nFrameCount)) || nFrameCount != 1)
      goto ReleaseDecoder;
  // load the first frame (i.e., the image)
  IWICBitmapFrameDecode * ipFrame = NULL;
  if (FAILED(ipDecoder->GetFrame(0, &ipFrame)))
      goto ReleaseDecoder;
  // convert the image to 32bpp BGRA format with pre-multiplied alpha
  //   (it may not be stored in that format natively in the PNG resource,
  //   but we need this format to create the DIB to use on-screen)
  WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, ipFrame, &ipBitmap);
  if (ipBitmap != NULL) {
    hBitmap = CreateHBITMAP(ipBitmap);
    ipBitmap->Release();
  }
  ipFrame->Release();
ReleaseDecoder:
  ipDecoder->Release();
Return:
  return hBitmap;
}

bool PlatformLoadString(const char *name, std::string *result) {
  // First try and load from zip
  if (current_skin_zip && ZipFileReader::ReadFileToString(current_skin_zip, name, result))
    return true;
  // Load file from resource

  return false;
}


void PlatformSetSkin(const char *filename) {
  if (current_skin_zip) {
    ZipFileReader::Free(current_skin_zip);
    current_skin_zip = NULL;
  }
  current_skin_basedir.clear();
  
  DWORD attr = GetFileAttributesA(filename);
  if (attr == INVALID_FILE_ATTRIBUTES)
    return;
  if (attr & FILE_ATTRIBUTE_DIRECTORY) {
    current_skin_basedir = filename;
    current_skin_basedir += '\\';
  } else {
    FileIO *file_io = new SimpleFileIO(filename);
    current_skin_zip = ZipFileReader::Open(file_io, true);
  }
}

void PlatformDeleteBitmap(Bitmap *bitmap) {
  DeleteObject((HBITMAP)bitmap);
}

Size PlatformGetBitmapSize(Bitmap *bitmap) {
  BITMAP bmp;
  Size rv = {0,0};
  if (GetObject((HBITMAP)bitmap, sizeof(BITMAP), &bmp)) {
    rv.w = bmp.bmWidth;
    rv.h = bmp.bmHeight;
  }
  return rv;
}

Bitmap *PlatformLoadBitmapFromBuf(const void *data, size_t data_size) {
  BitmapFormat format = ((unsigned char*)data)[0] == 'B' ? kFormatBmp : 
                        ((unsigned char*)data)[0] == 0x89 ? kFormatPng : 
                                                            kFormatJpeg;
  IStream *stream = SHCreateMemStream((BYTE*)data, data_size);
  if (!stream) return NULL;
  Bitmap *bmp = (Bitmap*)LoadBitmapFromStream(stream, format);
  stream->Release();
  return bmp;
}

bool PlatformLoadBitmap(Bitmap **bitmap, const char *name) {
  if (*bitmap) {
    DeleteObject((HBITMAP)*bitmap);
    *bitmap = NULL;
  }
  // First try and load from zip
  std::string zipdata;
  if (current_skin_zip && ZipFileReader::ReadFileToString(current_skin_zip, name, &zipdata)) {
    *bitmap = PlatformLoadBitmapFromBuf(zipdata.data(), zipdata.size());
    if (*bitmap)
      return true;
  }
  // Then from disk
  if (current_skin_basedir.size())
    *bitmap = (Bitmap*)LoadImageA(NULL, (current_skin_basedir + name).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
  if (!*bitmap) {
    // Last from resource
    *bitmap = (Bitmap*)LoadBitmapA(hInst, name);
  }
  return *bitmap != NULL;
}

char *PlatformReadClipboard() {
  static char *data;
  if (data) { free(data); data = NULL; }
  if (OpenClipboard(NULL)) {
    HANDLE hClipboardData = GetClipboardData(CF_TEXT);
    char *pchData = (char*)GlobalLock(hClipboardData);
    data = strdup(pchData);
    GlobalUnlock(hClipboardData);
    CloseClipboard();
  }
  return data;
}

static std::wstring DecodeUtf8(const char *text) {
  wchar_t buf[1024];
  std::wstring result;
  if (MultiByteToWideChar(CP_UTF8, 0, text, -1, buf, ARRAYSIZE(buf)) == 0) buf[0] = 0;
  return buf;
}

bool PlatformWriteClipboard(const char *text) {
  std::wstring data = DecodeUtf8(text);
  bool ok = false;
  if (OpenClipboard(NULL)) {
    HGLOBAL hglb;
    hglb = GlobalAlloc(GMEM_SHARE|GMEM_MOVEABLE, (data.size() + 1) * sizeof(wchar_t));
    LPWSTR lptstr = (LPWSTR)GlobalLock(hglb);
    memcpy(lptstr, data.c_str(), (data.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(hglb);
    EmptyClipboard();
    ok = SetClipboardData(CF_UNICODETEXT, hglb) != 0;
    CloseClipboard();
  }
  return ok;
}

// This is required for files built with MinGW to link
extern "C" uint64 mips_umoddi3(unsigned a, unsigned b, unsigned c, unsigned d) {
  uint64 x = ((uint64)b << 32) | a;
  uint64 y = ((uint64)d << 32) | c;
  return x % y;
}
extern "C" uint64 mips_udivdi3(unsigned a, unsigned b, unsigned c, unsigned d) {
  uint64 x = ((uint64)b << 32) | a;
  uint64 y = ((uint64)d << 32) | c;
  return x / y;
}
 
void DoAutoUpdateStuff(PlatformWindow *w) {
  HWND hWnd = w->handle();
  if (!g_auto_update_available) return;
  g_auto_update_available = false;

  if (g_auto_update_msg.size() && g_auto_update_url.size()) {
    const char *caption = g_auto_update_caption.size() ? g_auto_update_caption.c_str() : "Spotiamb";
    if ((g_auto_update_flags & 1) == 0) {
      if (MessageBoxA(hWnd, g_auto_update_msg.c_str(), caption, MB_ICONINFORMATION | MB_YESNO) == IDYES) {
        if (memcmp(g_auto_update_url.c_str(), "http://", 7) == 0)
          ShellExecuteA(NULL, "open", g_auto_update_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
      }
    } else if (g_auto_update_flags & 2) {
      MessageBoxA(hWnd, g_auto_update_msg.c_str(), caption, MB_ICONSTOP);
    }
  }
  if (g_auto_update_flags & 4)
    ExitProcess(0);
}

char *for_each_line(char **ptr) {
  char *s = *ptr;
  if (s == NULL) return NULL;
  char *x = strchr(s, '\n');
  if (x) {
    x[x > s && x[-1] == '\r' ? -1 : 0] = '\0';
    *ptr = x + 1;
  } else {
    *ptr = NULL;
  }
  return s;
}

static void ProcessUpdateMessage(char auto_update_message[8192]) {
  char *lines = auto_update_message;
  while (char *s = for_each_line(&lines)) {
    if (memcmp(s, "URL:", 4) == 0) {
      g_auto_update_url = s + 4;
    } else if (memcmp(s, "M:", 2) == 0) {
      g_auto_update_msg.append(s + 2);
      g_auto_update_msg.append("\r\n");
    } else if (memcmp(s, "T:", 2) == 0) {
      g_auto_update_caption = s + 2;
    } else if (memcmp(s, "F:", 2) == 0) {
      g_auto_update_flags = strtoul(s + 2, NULL, 0);
    }
  }
  g_auto_update_available = true;
}

static DWORD WINAPI UpdateCheckerThread(PVOID arg) {
  char auto_update_message[8192];
  char query[512];
  OSVERSIONINFOEXA ver = {0};
  ver.dwOSVersionInfoSize = sizeof(ver);
  GetVersionExA((OSVERSIONINFOA*)&ver);
  const char *version = TspGetVersionString();
 
  sprintf(query, "http://spotiamb.com/upd.php?v=%s&m=%s&w=%d.%d.%d.%d", version, machine_id, ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber, ver.wServicePackMajor);
  HINTERNET hINet = InternetOpenA("Spotiamb/" VERSION_STR, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
  if (hINet) {
    HINTERNET hFile = InternetOpenUrlA(hINet, query, NULL, 0, INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_UI, 0);
    if (hFile) {
      DWORD statusCode = 0;
      DWORD length;
      length = sizeof(statusCode);
      HttpQueryInfoA(hFile, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL);
      if (statusCode == 200) {
        DWORD dwRead = 0;
        if (InternetReadFile(hFile, auto_update_message, sizeof(auto_update_message) - 1, &dwRead)) {
          auto_update_message[dwRead] = 0;
          ProcessUpdateMessage(auto_update_message);
        }
      }
      InternetCloseHandle(hFile);
    }
    InternetCloseHandle(hINet);
  }
  return 0;
}

void CheckUpdate() {
  DWORD thread_id;
  CloseHandle(CreateThread(NULL, 0, &UpdateCheckerThread, NULL, 0, &thread_id));
}

void OpenUrl(const char *url) {
  ShellExecuteA(NULL, NULL, url, NULL, NULL, NULL);
}

FileEnumerator::FileEnumerator(const char *directory) {
  h_ = FindFirstFileA((std::string(directory) + "\\*.*").c_str(), &wfd_);
  first_ = true;
}

FileEnumerator::~FileEnumerator() {
  if (h_ != INVALID_HANDLE_VALUE)
    FindClose(h_);
}

bool FileEnumerator::Next() {
  bool b;
  do {
    if (first_) {
      first_ = false;
      b = (h_ != INVALID_HANDLE_VALUE);
    } else {
      b = FindNextFileA(h_, &wfd_) != FALSE;
    }
  } while (b && wfd_.cFileName[0] == '.');
  return b;
}

MenuBuilder::MenuBuilder() {
  pos_ = 0;
  curr_ = CreatePopupMenu();
}

MenuBuilder::~MenuBuilder() {
  for(int i = 0; i < pos_; i++)
    DestroyMenu(stack_[i]);
  DestroyMenu(curr_);
}

void MenuBuilder::BeginSubMenu() {
  int j = pos_++;
  if (j < ARRAYSIZE(stack_)) {
    stack_[j] = curr_;
    curr_ = CreatePopupMenu();
  }
}

void MenuBuilder::EndSubMenu(const char *title, int flags) {
  assert(pos_ > 0);
  if (--pos_ < ARRAYSIZE(stack_)) {
    HMENU old = curr_;
    curr_ = stack_[pos_];
    AppendMenu(curr_, MF_POPUP, (UINT_PTR)old, DecodeUtf8(title).c_str());
  }
}

void MenuBuilder::AddSeparator() {
  AppendMenu(curr_, MF_SEPARATOR, 0, 0);
}

void MenuBuilder::AddItem(int id, const char *title, int flags) {
  int f = (flags & 1) * MF_CHECKED + ((flags >> 2) & 1) * MF_GRAYED;
  AppendMenu(curr_, f, id, DecodeUtf8(title).c_str());
  if (flags & kRadio) {
    int i = GetMenuItemCount(curr_) - 1;
    CheckMenuRadioItem(curr_, i, i, i, MF_BYPOSITION);
  }
}

int MenuBuilder::Popup(PlatformWindow *window) {
  assert(pos_ == 0);
  POINT pt;
  GetCursorPos(&pt);
  return TrackPopupMenu(curr_, TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, window->handle(), NULL);
}

int MenuBuilder::PopupAt(PlatformWindow *window, int x, int y) {
  assert(pos_ == 0);
  POINT pt = {x, y};
  ClientToScreen(window->handle(), &pt);
  return TrackPopupMenu(curr_, TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, window->handle(), NULL);
}


static int g_num_audio_devs;
static WAVEOUTCAPSA g_audio_caps;

const char *PlatformEnumAudioDevices(int i) {
  if (i == 0) g_num_audio_devs = waveOutGetNumDevs();
  if (i >= g_num_audio_devs) return NULL;
  if (waveOutGetDevCapsA(i, &g_audio_caps, sizeof(g_audio_caps)) == MMSYSERR_NOERROR) {
    return g_audio_caps.szPname;
  }
  return "";
}

int MsgBox(const char *message, const char *title, int flags) {
  return MessageBoxA(g_main_window->handle(), message, title, flags);
}
#if 1

#include <Windows.h>
#include <WinCrypt.h>

extern "C" {
uint64 __umoddi3(unsigned a, unsigned b, unsigned c, unsigned d) {
  uint64 x = ((uint64)b << 32) | a;
  uint64 y = ((uint64)d << 32) | c;
  return x % y;
}

uint64 __udivdi3(unsigned a, unsigned b, unsigned c, unsigned d) {
  uint64 x = ((uint64)b << 32) | a;
  uint64 y = ((uint64)d << 32) | c;
  return x / y;
}

void __declspec(naked) __chkstk_ms() {
  _asm {
    push ecx
      push eax
      cmp eax, 0x1000
      lea ecx, [esp + 12]
      jb getout
again :
    sub ecx, 0x1000
      or dword ptr[ecx], 0
      sub eax, 0x1000
      cmp eax, 0x1000
      ja again
getout :
    sub ecx, eax
      or dword ptr[ecx], 0
      pop eax
      pop ecx
      ret
  }
}

TspBool MipsCryptGenRandom(int xx, size_t num_bytes, void *data) {
  HCRYPTPROV hProvider = 0;
  TspBool retval = false;
  if (CryptAcquireContextW(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
    retval = (TspBool)CryptGenRandom(hProvider, num_bytes, (BYTE*)data);
    CryptReleaseContext(hProvider, 0);
  }
  return retval;
}
}
#endif

#endif  // defined(WITH_SDL)