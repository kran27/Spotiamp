#include "stdafx.h"
#include "spotifyamp.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "winamp_sdk/vis.h"
#include "winamp_sdk/wa_ipc.h"
#include "winamp_sdk/api_language.h"
#include "winamp_sdk/api_syscb.h"
#include "winamp_sdk/api_application.h"
#include "winamp_sdk/api_service.h"
#include "winamp_sdk/api_memmgr.h"
#include "winamp_sdk/waservicefactory.h"
#include "window.h"

class VisWindow : public GenWindow {
public:
  virtual BOOL WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *retval);
  virtual void OnClose() { SendMessageA(handle(), WM_CLOSE, 0, 0); }
};

static void StopThread();

BOOL VisWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *retval) {
  if (message >= WM_KEYFIRST && message <= WM_KEYUP) {
    HWND child = GetWindow(handle(), GW_CHILD);
    if (child) {
      PostMessage(child, message, wParam, lParam);
      return TRUE;
    }
  }
  BOOL handled = PlatformWindow::WndProc(hWnd, message, wParam, lParam, retval);
  switch(message) {
    case WM_WINDOWPOSCHANGED: {
      HWND child = GetWindow(handle(), GW_CHILD);
      if (child != NULL) {
        SetWindowPos(child, NULL, 11, 20, screen_rect_.right - screen_rect_.left - 19,
                                          screen_rect_.bottom - screen_rect_.top - 34, SWP_NOZORDER | SWP_NOACTIVATE);
      }
      if (screen_rect_.right - screen_rect_.left != width_ ||
          screen_rect_.bottom - screen_rect_.top != height_) {
        width_ = screen_rect_.right - screen_rect_.left;
        height_ = screen_rect_.bottom - screen_rect_.top;
        Repaint();
      }
      break;
    }
    case WM_CLOSE: {
      StopThread();
      break;
    }
  }

  return handled;
}

static HWND EmbedWindow(embedWindowState *ews) {
  SendMessageA(g_main_window->handle(), WM_USER, (WPARAM)ews, 9191919);
  return ews->me;
}

const char *GetIniFilePath();

static VisWindow *g_vis_window;

#define CBCLASS MyLanguageCrap
class MyLanguageCrap : public api_language {
public:
  char *GetString(HINSTANCE hinst, HINSTANCE owner, UINT uID, char *str=NULL, size_t maxlen=0) {
    if(str) {
      LoadStringA(owner, uID, str, maxlen);
      return str;
    }
    return NULL;
  }
  wchar_t *GetStringW(HINSTANCE hinst, HINSTANCE owner, UINT uID, wchar_t *str=NULL, size_t maxlen=0) {
    static wchar_t kossa[512];
    if (!str) {
      str=kossa;
      maxlen=512;
    }
    if(str) {
      LoadStringW(owner, uID, str, maxlen);
      return str;
    }
    return L"assdad";
  }
  HMENU LoadLMenu(HINSTANCE localised, HINSTANCE original, UINT id) {
    return ::LoadMenuA(original, (LPCSTR)id);
  }
  START_DISPATCH
    CB(API_LANGUAGE_GETSTRING, GetString) 
    CB(API_LANGUAGE_GETSTRINGW, GetStringW)
    CB(API_LANGUAGE_LOADLMENU, LoadLMenu)
  END_DISPATCH
};
static MyLanguageCrap m_languageCrap;
#undef CBCLASS

#define CBCLASS MyApplicationCrap
class MyApplicationCrap : public api_application {
public:
  START_DISPATCH
  END_DISPATCH
};
static MyApplicationCrap m_applicationCrap;
#undef CBCLASS

#define CBCLASS MySyscbCrap
class MySyscbCrap : public api_syscb {
public:
  START_DISPATCH
  END_DISPATCH
};
static MySyscbCrap m_syscbCrap;
#undef CBCLASS

#define CBCLASS MyMemmgrCrap
class MyMemmgrCrap : public api_memmgr {
public:
  START_DISPATCH
  END_DISPATCH
};
static MyMemmgrCrap m_memmgrCrap;
#undef CBCLASS

#define CBCLASS MyFallbackCrap
class MyFallbackCrap : public Dispatchable {
public:
  START_DISPATCH
  END_DISPATCH
};
static MyFallbackCrap m_fallbackCrap;
#undef CBCLASS


#define CBCLASS ServiceFactory
class ServiceFactory : public waServiceFactory {
public:
  ServiceFactory(void *x) : x(x) {}
  void *getInterface(int global_lock) { return x; }
  START_DISPATCH
    CB(WASERVICEFACTORY_GETINTERFACE, getInterface)
  END_DISPATCH
  void *x;
};
static ServiceFactory m_languageServiceFactory(&m_languageCrap);
static ServiceFactory m_applicationServiceFactory(&m_applicationCrap);
static ServiceFactory m_syscbServiceFactory(&m_syscbCrap);
static ServiceFactory m_memmgrServiceFactory(&m_memmgrCrap);
static ServiceFactory m_fallbackServiceFactory(&m_fallbackCrap);


#undef CBCLASS

#define CBCLASS MyApiService
class MyApiService : public api_service {
public:
  waServiceFactory *service_getServiceByGuid(GUID guid) {
    if(guid == languageApiGUID) return &m_languageServiceFactory;
    if(guid == applicationApiServiceGuid) return &m_applicationServiceFactory;
    if(guid == syscbApiServiceGuid) return &m_syscbServiceFactory;
    if(guid == memMgrApiServiceGuid) return &m_memmgrServiceFactory;
    return &m_fallbackServiceFactory;
  }
  START_DISPATCH
    CB(API_SERVICE_SERVICE_GETSERVICEBYGUID, service_getServiceByGuid)
  END_DISPATCH
};
static MyApiService m_apiService;

LRESULT HandlePluginApi(WPARAM wParam, LPARAM lParam) {
  
  switch(lParam) {
  case IPC_GETVERSION: return 0x2900;
  case IPC_GET_EMBEDIF: return (LRESULT)&EmbedWindow;
  case IPC_GETINIFILE: return (LRESULT)GetIniFilePath();
  case IPC_SETVISWND: return 0;
  case IPC_ISPLAYING: return 1;
  case IPC_GET_API_SERVICE:
    return (LPARAM)&m_apiService;
  case IPC_GETLISTPOS:
    return 0;
  case IPC_GETPLAYLISTTITLE:
    return (LRESULT)"";
  case IPC_GETPLAYLISTTITLEW:
    return (LRESULT)L"";
  case 9191919: {
    embedWindowState *ews = (embedWindowState *)wParam;
    VisWindow *v = g_vis_window;
    if (!v) {
      v = new VisWindow;
      v->Create(g_main_window);
      v->SetVisible(true);
      g_vis_window = v;
    }
    GetWindowRect(v->handle(), &ews->r);
    ews->me = v->handle();
    return 0;
  }
  default:
    printf("Plugin %d %d\n", wParam, lParam);
  }
  return 0;
}

struct Plugin {
  char *file;
  char *desc;
};

static Plugin vis_plugins[32];
static int num_vis_plugins;
static bool plugins_indexed;
static winampVisModule *g_module;
static int g_plugin_index = -1;
static HINSTANCE g_plugin_hinst;
static HANDLE g_render_thread;

struct VisData {
  unsigned char spectrumData[2][576];
  unsigned char waveformData[2][576];
};

struct TimestampedVisData {
  unsigned int timestamp;
  VisData data;
};

#define VISDATA_COUNT 64
static TimestampedVisData visdata[VISDATA_COUNT];
static int visdata_write_pos, visdata_read_pos;
static CRITICAL_SECTION visdata_lock;
static void InsertVisData(unsigned int timestamp, VisData *data) {
  EnterCriticalSection(&visdata_lock);
  TimestampedVisData *tv = &visdata[visdata_write_pos++ & (VISDATA_COUNT - 1)];
  tv->timestamp = timestamp;
  tv->data = *data;
  LeaveCriticalSection(&visdata_lock);
}

volatile bool g_want_thread_running;
static void tiny_fft(float *buf);
static int tiny_fft_rtab[512];

// Input is exactly 576 samples.
void InsertVisDataSamples(unsigned int timestamp, const int16 *samp, int nch) {
  float fft[1024];
  int i, ch;
  VisData tmp;
  byte *p, *p_end, *s;
  if (nch != 2) return;
  if (!g_render_thread) return;

  for(ch = 0; ch < 2; ch++) {
    // Copy 512 samples across, zero out the imaginary component
    for(i = 0; i < 512; i++) {
      fft[i*2] = samp[i*2] * (1/(512.0f*6.0f));
      // //
      fft[i*2+1] = 0;
    }
    tiny_fft(fft);
    float offs = fabs(fft[255*4]);
    int last = 0;
    p = tmp.spectrumData[ch];
    p_end = p + 576;
    for(i = 0; i < 254; i++) {
      int v = (int) (fabs(fft[tiny_fft_rtab[i+1]*2]) - offs);
      if (v < 0) v = 0;
      else if (v > 255) 
        v = 255;
      *p++ = (byte) ((v + last)/2);
      *p++ = (byte) v;
      if ((i&3) == 3) *p++ = (byte)v;
      last = v;
    }
    while (p < p_end) {
      last = last/2 + last/4;
      *p++ = last;
    }
    // Setup also the visdata
    p = tmp.waveformData[ch];
    s = (byte*)samp + 1;
    for(i = 0; i < 576; i++)
      p[i] = s[i*2*sizeof(TspSampleType)];
    samp++;
  }
/*  {
    static FILE *x = fopen("d:\\foo.raw", "wb");
    fwrite(tmp.waveformData, 1, sizeof(tmp.waveformData), x);
    fflush(x);
  }*/
  InsertVisData(timestamp, &tmp);
}

VisData *FindVisData(unsigned int timestamp) {
  EnterCriticalSection(&visdata_lock);
  int best_v = 1000;
  int best_p = -1;
  int p = visdata_read_pos;
  static VisData tmp;
  for(int i = 0; i < VISDATA_COUNT; i++, p = (p + 1) & (VISDATA_COUNT - 1)) {
    TimestampedVisData *tv = &visdata[p];
    int v = timestamp - tv->timestamp;
    if (v < 0) v = -v;
    if (v < best_v) {
      best_v = v;
      best_p = p;
    } else if (best_v < 200) {
      break;
    }
  }
  if (best_p < 0) {
    LeaveCriticalSection(&visdata_lock);
    return NULL;
  }
  visdata_read_pos = best_p;
  tmp = visdata[best_p].data;
  LeaveCriticalSection(&visdata_lock);
  return &tmp;
}

extern int vis_time_adjustment;

static DWORD WINAPI ThreadRunner(PVOID arg) {
  INT_PTR dialogWndProc;
  {
    WNDCLASSA wc;
    GetClassInfoA(g_plugin_hinst,"#32770",&wc);
    dialogWndProc=(INT_PTR)wc.lpfnWndProc;   
  }
  g_module->Init(g_module);
  while (g_want_thread_running) {
    MSG msg;
    if(PeekMessageA(&msg, NULL,0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) break;
      HWND hwndCurrent = GetForegroundWindow();
      if (GetClassLongPtrA(hwndCurrent,GCLP_WNDPROC) == dialogWndProc && IsDialogMessage(hwndCurrent,&msg))
        continue;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    } else {
      uint32 now = PlatformGetTicks() + vis_time_adjustment;
      VisData *vd = FindVisData(now + g_module->latencyMs);
      if (vd) {
        memcpy(g_module->spectrumData, vd->spectrumData, sizeof(g_module->spectrumData));
        memcpy(g_module->waveformData, vd->waveformData, sizeof(g_module->waveformData));
      } else {
        memset(g_module->spectrumData, 0, sizeof(g_module->spectrumData));
        memset(g_module->waveformData, 0, sizeof(g_module->waveformData));
      }
      if (g_module->Render(g_module))
        break;
      Sleep(g_module->delayMs);
    }
  }
  g_module->Quit(g_module);
  return 0;
}

extern char exepath[MAX_PATH];

void IndexPlugins() {
  char file[MAX_PATH + 100];
  char tmp[MAX_PATH + 100 + MAX_PATH];
  InitializeCriticalSection(&visdata_lock);
  sprintf(file, "%sPlugins\\vis_*.dll", exepath);

  WIN32_FIND_DATAA wfd;
  HANDLE h = FindFirstFileA(file, &wfd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      sprintf(file, "%sPlugins\\%s", exepath, wfd.cFileName);
      HINSTANCE hInstance = LoadLibraryA(file);
      winampVisGetHeaderType func = (winampVisGetHeaderType)GetProcAddress(hInstance, "winampVisGetHeader");
      if (func) {
        winampVisHeader *hdr = NULL;
//        __try {
          hdr = func(g_main_window->handle());
//        } __except(EXCEPTION_CONTINUE_EXECUTION) {
//          hdr = NULL;
//        }
        if (hdr && (hdr->version == 0x101 || hdr->version == 0x102) ) {
          Plugin *p = &vis_plugins[num_vis_plugins++];
          p->file = strdup(file);
          sprintf(tmp, "%s   [%s]", hdr->description, wfd.cFileName);
          p->desc = strdup(tmp);
        }
      }
      FreeLibrary(hInstance);
    } while (num_vis_plugins < 32 && FindNextFileA(h, &wfd));
    FindClose(h);
  }
  plugins_indexed = true;
}

const char *VisPlugin_Iterate(int i) {
  if (!plugins_indexed) IndexPlugins();
  if ((unsigned)i >= (unsigned)num_vis_plugins) return 0;
  return vis_plugins[i].desc;
}

static void StopThread() {
  g_want_thread_running = false;
  if (g_render_thread != NULL) {
    //run msg pump for a bit so the plugin can destroy its window, etc...
    int x = 200;
    while (WaitForSingleObject(g_render_thread,10) == WAIT_TIMEOUT && x-- > 0) {
      MSG msg;
      if (PeekMessage(&msg,NULL,0,0,PM_REMOVE))
        DispatchMessage(&msg);
    }
    WaitForSingleObject(g_render_thread, 10000);
    CloseHandle(g_render_thread);
    g_render_thread = NULL;
  }
  if (g_plugin_hinst != NULL) {
    FreeLibrary(g_plugin_hinst);
    g_plugin_hinst = NULL;
  }
  g_module = NULL;
  g_plugin_index = -1;
  delete g_vis_window;
  g_vis_window = NULL;
}

static void StartThread() {
  g_want_thread_running = true;
  if (g_render_thread == NULL) {
    DWORD thread_id;
    g_render_thread = CreateThread(NULL, 0, &ThreadRunner, NULL, 0, &thread_id);
  }
}

void VisPlugin_Load(int i) {
  if (i == g_plugin_index)
    return;
  StopThread();
  if (i >= 0) {
    HINSTANCE hInstance = LoadLibraryA(vis_plugins[i].file);
    winampVisGetHeaderType func = (winampVisGetHeaderType)GetProcAddress(hInstance, "winampVisGetHeader");
    if (!func) return;
    winampVisHeader *hdr = func(g_main_window->handle());
    if (!hdr || (hdr->version != 0x101 && hdr->version != 0x102) || !hdr->getModule) return;
    winampVisModule *module = hdr->getModule(0);
    module->hwndParent = g_main_window->handle();
    module->hDllInstance = hInstance;
    module->sRate = 44100;
    module->nCh = 2;
    g_module = module;
    g_plugin_hinst = hInstance;
  }
  g_plugin_index = i;
}

void VisPlugin_StartStop(int i) {
  VisPlugin_Load(i);
  if (g_render_thread)
    StopThread();
  else if (g_module != NULL)
    StartThread();
}

void VisPlugin_Stop() {
  StopThread();
}

void VisPlugin_Config(int i) {
  VisPlugin_Load(i);
  if (g_module)
    g_module->Config(g_module);


}

void LoadVisPlugin() {
  

}

#define PI       3.14159265358979323846

static void tiny_fft(float *buf) {
  static int has_init;
  static float sintab[512];

  if (!has_init) {
    int i;
    has_init=1;
    for (i = 0; i < 512; i ++) {
      int cnt=9;
      int src=i;
      int v = 0;
      while (--cnt) {
        v<<=1;
        v |= (src & 1);
        src>>=1;
      }
      tiny_fft_rtab[i] = v;
      if (!(i&1)) {
        sintab[i]=(float)cos(v * PI / 256.0);
        sintab[i+1]=-(float)sin(v * PI / 256.0);
      }
    }
  }

  int cursize=1;
  int curdiv=256;
  while (curdiv>0) {
    float *bufp=buf;
    float *sinp=sintab;
    int ofs=curdiv+curdiv;
    int loop1=cursize;
    while (loop1--) {
      float z1 = sinp[0];
      float z2 = sinp[1];
      int i=curdiv;
      do {
        float c1 = bufp[ofs];
        float c2 = bufp[ofs+1];
        float b1 = z1 * c1 - z2 * c2;
        float b2 = z2 * c1 + z1 * c2;
        float a1 = bufp[0];
        float a2 = bufp[1];
        bufp[0] = a1 + b1;
        bufp[1] = a2 + b2;
        bufp[ofs] = a1 - b1;
        bufp[ofs+1] = a2 - b2;
        bufp+=2;
      } while (--i);
      bufp+=ofs;
      sinp+=2;
    }
    curdiv/=2;
    cursize*=2;
  }
}
