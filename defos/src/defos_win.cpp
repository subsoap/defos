#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_WINDOWS)

#include <atlbase.h>
#include <atlconv.h>
#include <WinUser.h>
#include <Windows.h>

// keep track of window placement when going to/from fullscreen or maximized
static WINDOWPLACEMENT placement = {sizeof(placement)};

// used to check if WM_MOUSELEAVE detected the mouse leaving the window
static bool is_mouse_inside = false;

// original wndproc pointer
static WNDPROC originalProc = NULL;

// forward declarations
bool set_window_style(LONG_PTR style);
LONG_PTR get_window_style();
LRESULT __stdcall custom_wndproc(HWND hwnd, UINT umsg, WPARAM wp, LPARAM lp);
void restore_window_class();
void subclass_window();

/******************
 * exposed functions
 ******************/

void defos_init()
{
    is_mouse_inside = false;
    subclass_window();
}

void defos_final()
{
    restore_window_class();
}

void defos_event_handler_was_set(DefosEvent event)
{
}

bool defos_is_fullscreen()
{
    return !(get_window_style() & WS_OVERLAPPEDWINDOW);
}

bool defos_is_maximized()
{
    return !!IsZoomed(dmGraphics::GetNativeWindowsHWND());
}

bool defos_is_mouse_inside_window()
{
    return is_mouse_inside;
}

void defos_disable_maximize_button()
{
    set_window_style(get_window_style() & ~WS_MAXIMIZEBOX);
}

void defos_disable_minimize_button()
{
    set_window_style(get_window_style() & ~WS_MINIMIZEBOX);
}

void defos_disable_window_resize()
{
    set_window_style(get_window_style() & ~WS_SIZEBOX);
}

void defos_disable_mouse_cursor()
{
    ShowCursor(0);
}

void defos_enable_mouse_cursor()
{
    ShowCursor(1);
}

// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/
void defos_toggle_fullscreen()
{
    if (defos_is_maximized())
    {
        defos_toggle_maximize();
    }

    HWND window = dmGraphics::GetNativeWindowsHWND();
    if (!defos_is_fullscreen())
    {
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            set_window_style(get_window_style() & ~WS_OVERLAPPEDWINDOW);
            GetWindowPlacement(window, &placement);
            SetWindowPos(window, HWND_TOP,
                         mi.rcMonitor.left,
                         mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        set_window_style(get_window_style() | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &placement);
    }
}

void defos_toggle_maximize()
{
    if (defos_is_fullscreen())
    {
        defos_toggle_fullscreen();
    }

    HWND window = dmGraphics::GetNativeWindowsHWND();
    if (defos_is_maximized())
    {
        SetWindowPlacement(window, &placement);
    }
    else
    {
        GetWindowPlacement(window, &placement);
        ShowWindow(window, SW_MAXIMIZE);
    }
}

void defos_show_console() {
	::ShowWindow(::GetConsoleWindow(), SW_SHOW);
}

void defos_hide_console() {
	::ShowWindow(::GetConsoleWindow(), SW_HIDE);
}

bool defos_is_console_visible() {
	return (::IsWindowVisible(::GetConsoleWindow()) != FALSE);
}

void defos_set_window_size(int x, int y, int w, int h)
{
    if (x == -1)
    {
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }
    HWND window = dmGraphics::GetNativeWindowsHWND();
    SetWindowPos(window, window, x, y, w, h, SWP_NOZORDER);
}

void defos_set_window_title(const char *title_lua)
{
    SetWindowTextW(dmGraphics::GetNativeWindowsHWND(), CA2W(title_lua));
}

WinRect defos_get_window_size(){
    HWND window = dmGraphics::GetNativeWindowsHWND();
    WINDOWPLACEMENT frame = { sizeof(placement) };
    GetWindowPlacement(window, &frame);
    WinRect rect;
    rect.x = (float)frame.rcNormalPosition.left;
    rect.y = (float)frame.rcNormalPosition.top;
    rect.w = (float)(frame.rcNormalPosition.right - frame.rcNormalPosition.left);
    rect.h = (float)(frame.rcNormalPosition.bottom - frame.rcNormalPosition.top);
    return rect;
}

/********************
 * internal functions
 ********************/

static bool set_window_style(LONG_PTR style)
{
    return SetWindowLongPtrA(dmGraphics::GetNativeWindowsHWND(), GWL_STYLE, style) != 0;
}

static LONG_PTR get_window_style()
{
    return GetWindowLongPtrA(dmGraphics::GetNativeWindowsHWND(), GWL_STYLE);
}

static void subclass_window()
{
    // check if we already subclass the window
    if (originalProc) { return; }

    HWND window = dmGraphics::GetNativeWindowsHWND();

    originalProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)&custom_wndproc); // keep original proc

    if (originalProc == NULL)
    {
        DWORD error = GetLastError();
        dmLogError("Error while subclassing current window: %d\n", error);
    }
}

static void restore_window_class()
{
    if (originalProc != NULL)
    {
        HWND window = dmGraphics::GetNativeWindowsHWND();
        SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)originalProc);
        originalProc = NULL;
    }
}

// replaced wndproc to cutomize message processing
static LRESULT __stdcall custom_wndproc(HWND hwnd, UINT umsg, WPARAM wp, LPARAM lp)
{
    // NOTE: we do not handle any event here, so they will be processed by the default wndproc callback

    switch (umsg)
    {
    case WM_MOUSEMOVE:
        if (!is_mouse_inside)
        {
            is_mouse_inside = true;
            defos_emit_event(DEFOS_EVENT_MOUSE_ENTER);

            TRACKMOUSEEVENT tme = { sizeof(tme) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;

    case WM_MOUSELEAVE:
        is_mouse_inside = false;
        defos_emit_event(DEFOS_EVENT_MOUSE_LEAVE);
        break;
    }

    if (originalProc != NULL)
        return CallWindowProc(originalProc, hwnd, umsg, wp, lp);
    else
        return 0;
}

#endif
