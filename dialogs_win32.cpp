#include "stdafx.h"
#include "dialogs.h"
#include "spotifyamp.h"
#include <Windows.h>
#include "resource.h"

static std::string ToUtf8(const wchar_t *buf) {
  size_t len = wcslen(buf);
  int bytes_needed = WideCharToMultiByte(CP_UTF8, 0, buf, len, NULL, 0, NULL, NULL);
  std::string rv;
  rv.resize(bytes_needed);
  if (!WideCharToMultiByte(CP_UTF8, 0, buf, len, &rv[0], bytes_needed, NULL, NULL))
    rv.clear();
  return rv;
}

static std::string GetWindowString(HWND wnd, int child) {
  wchar_t buf[1024];
  GetDlgItemTextW(wnd, child, buf, ARRAYSIZE(buf));
  return ToUtf8(buf);
}

static void SetWindowString(HWND wnd, int child, const char *msg) {
  wchar_t buf[1024];
  if (!msg) msg = "";
  buf[0] = 0;
  if (MultiByteToWideChar(CP_UTF8, 0, msg, -1, buf, ARRAYSIZE(buf)) == 0) buf[0] = 0;
  SetDlgItemTextW(wnd, child, buf);
}


static WNDPROC old_combo_proc;
static int suggent, sugglen;
static HWND searchhwnd;
static std::string *g_username, *g_password, *g_search_q;
static Tsp *g_tsp;
//IDC_SEARCH

void AutoCompleteCopy() {
  if (!g_search_q) return;
  const char *recent = TspAutoCompleteGetResult(g_tsp, suggent);
  if (recent) {
    SetWindowString(searchhwnd, IDC_SEARCH, recent);
    SendDlgItemMessage(searchhwnd, IDC_SEARCH, CB_SETEDITSEL, 0, MAKELPARAM(sugglen, -1));
  }
}

static void AutoCompleteInit() {
  std::string searchs = GetWindowString(searchhwnd, IDC_SEARCH);
  const char *recent = TspAutoCompleteGetResult(g_tsp, suggent);
  if (recent && recent == searchs) {
    suggent++;
    if (!TspAutoCompleteGetResult(g_tsp, suggent))
      suggent = 0;
    AutoCompleteCopy();
  } else {
    suggent = 0;
    sugglen = searchs.size();
    TspAutoComplete(g_tsp, searchs.c_str());
  }
}

static LRESULT WINAPI ComboSubclass(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch(msg) {
  case WM_KEYDOWN:
    if (wparam == VK_TAB) {
      AutoCompleteInit();
      return 0;
    }
    break;
  case WM_CHAR:
    if (wparam == VK_TAB)
      return 0;
    break;
  }
  LRESULT r = CallWindowProc(old_combo_proc, wnd, msg, wparam, lparam);
  switch(msg) {
    case WM_GETDLGCODE:
      r |= DLGC_WANTTAB;
      break;
  }
  return r;
};

static BOOL WINAPI SearchDialogFunc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch(msg) {
  case WM_INITDIALOG: {
    searchhwnd = wnd;
    HWND combo = GetDlgItem(wnd, IDC_SEARCH);
    HWND edit = FindWindowEx(combo, 0,NULL,NULL);
    old_combo_proc = (WNDPROC)SetWindowLong(edit, GWL_WNDPROC, (LONG)&ComboSubclass);
    return TRUE;
  }

  case WM_COMMAND:
    if (wparam == IDCANCEL)
      EndDialog(wnd, 0);
    else if (wparam == IDOK) {
      *g_search_q = GetWindowString(searchhwnd, IDC_SEARCH);
      EndDialog(wnd, 1);
    }
    break;
  case WM_GETDLGCODE:
    return (BOOL)DLGC_WANTTAB;
  case WM_CLOSE:
    EndDialog(wnd, 0);
    return TRUE;
  }
  return FALSE;
}

static BOOL WINAPI LoginDialogFunc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch(msg) {
  case WM_INITDIALOG:
    SetWindowString(wnd, IDC_USERNAME, g_username->c_str());
    return TRUE;
  case WM_CLOSE:
    EndDialog(wnd, 0);
    return TRUE;
  case WM_COMMAND:
    if (wparam == IDCANCEL)
      EndDialog(wnd, 0);
    else if (wparam == IDOK) {
      *g_username = GetWindowString(wnd, IDC_USERNAME);
      *g_password = GetWindowString(wnd, IDC_PASSWORD);
      EndDialog(wnd, 1);
    }
    break;
  }
  return FALSE;
}

bool ShowLoginDialog(PlatformWindow *parent, std::string *username, std::string *password) {
  g_username = username;
  g_password = password;
  return DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_LOGIN), parent->handle(), LoginDialogFunc) == 1;
}

std::string ShowSearchDialog(PlatformWindow *parent, Tsp *tsp) {
  std::string searchq;
  g_search_q = &searchq;
  g_tsp = tsp;
  DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SEARCH), parent->handle(), SearchDialogFunc);
  g_search_q = NULL;
  return searchq;
}
