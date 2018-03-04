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

// original mouse clip rect
static RECT originalRect;
static bool is_cursor_clipped = false;
static POINT lock_point;
static bool is_cursor_locked = false;

static bool is_cursor_visible = true;
static bool is_custom_cursor_loaded;
static HCURSOR custom_cursor;
static HCURSOR original_cursor; // used to restore

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
    is_cursor_clipped = false;
    GetClipCursor(&originalRect);  // keep the original clip for restore
    original_cursor = GetCursor(); // keep the original cursor
    GetWindowPlacement(dmGraphics::GetNativeWindowsHWND(), &placement);
    subclass_window();
}

void defos_final()
{
    defos_set_cursor_clipped(false);
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

bool defos_is_mouse_in_view()
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

void defos_set_cursor_visible(bool visible)
{
    if (visible != is_cursor_visible)
    {
        is_cursor_visible = visible;
        ShowCursor(visible ? TRUE : FALSE);
    }
}

bool defos_is_cursor_visible()
{
    return is_cursor_visible;
}

// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/
void defos_toggle_fullscreen()
{
    if (defos_is_maximized())
    {
        defos_toggle_maximized();
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

void defos_toggle_maximized()
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

void defos_set_console_visible(bool visible)
{
    ::ShowWindow(::GetConsoleWindow(), visible ? SW_SHOW : SW_HIDE);
}

bool defos_is_console_visible()
{
    return (::IsWindowVisible(::GetConsoleWindow()) != FALSE);
}

void defos_set_window_size(float x, float y, float w, float h)
{
    if (isnan(x))
    {
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    }
    if (isnan(y))
    {
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }

    HWND window = dmGraphics::GetNativeWindowsHWND();
    SetWindowPos(window, window, (int)x, (int)y, (int)w, (int)h, SWP_NOZORDER);
}

void defos_set_view_size(float x, float y, float w, float h)
{
    if (isnan(x))
    {
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    }
    if (isnan(y))
    {
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }

    RECT rect = {0, 0, (int)w, (int)h};

    DWORD style = (DWORD)get_window_style();

    // TODO: we are assuming the window have no menu, maybe it is better to expose it as parameter later
    AdjustWindowRect(&rect, style, false);

    HWND window = dmGraphics::GetNativeWindowsHWND();

    SetWindowPos(window, window, (int)x, (int)y, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
}

void defos_set_window_title(const char *title_lua)
{
    SetWindowTextW(dmGraphics::GetNativeWindowsHWND(), CA2W(title_lua));
}

void defos_set_window_icon(const char *icon_path)
{
    HANDLE icon = LoadImage(NULL, icon_path, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (icon)
    {
        HWND window = dmGraphics::GetNativeWindowsHWND();
        SendMessage(window, (UINT)WM_SETICON, ICON_BIG, (LPARAM)icon);
    }
}

char const* defos_get_bundle_root() {
    char bundlePath[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, bundlePath);
    return bundlePath;
}

WinRect defos_get_window_size()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();
    WINDOWPLACEMENT frame = {sizeof(placement)};
    GetWindowPlacement(window, &frame);
    WinRect rect;
    rect.x = (float)frame.rcNormalPosition.left;
    rect.y = (float)frame.rcNormalPosition.top;
    rect.w = (float)(frame.rcNormalPosition.right - frame.rcNormalPosition.left);
    rect.h = (float)(frame.rcNormalPosition.bottom - frame.rcNormalPosition.top);
    return rect;
}

WinRect defos_get_view_size()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();

    RECT wrect;
    GetClientRect(window, &wrect);

    POINT pos = {wrect.left, wrect.top};
    ClientToScreen(window, &pos);

    WINDOWPLACEMENT frame = {sizeof(placement)};
    GetWindowPlacement(window, &frame);
    WinRect rect;
    rect.x = (float)pos.x;
    rect.y = (float)pos.y;
    rect.w = (float)(wrect.right - wrect.left);
    rect.h = (float)(wrect.bottom - wrect.top);
    return rect;
}

void defos_set_cursor_pos(float x, float y)
{
    SetCursorPos((int)x, (int)y);
}

// move cursor to pos relative to current window
// top-left is (0, 0)
void defos_move_cursor_to(float x, float y)
{
    HWND window = dmGraphics::GetNativeWindowsHWND();

    RECT wrect;
    GetClientRect(window, &wrect);

    int tox = wrect.left + (int)x;
    int toy = wrect.top + (int)y;
    POINT pos = {tox, toy};

    ClientToScreen(window, &pos);
    SetCursorPos(pos.x, pos.y);
}

void defos_set_cursor_clipped(bool clipped)
{
    is_cursor_clipped = clipped;
    if (clipped)
    {
        HWND window = dmGraphics::GetNativeWindowsHWND();

        RECT wrect;
        GetClientRect(window, &wrect);

        POINT left_top = {wrect.left, wrect.top};
        POINT right_bottom = {wrect.right, wrect.bottom};

        ClientToScreen(window, &left_top);
        ClientToScreen(window, &right_bottom);

        wrect.left = left_top.x;
        wrect.top = left_top.y;
        wrect.right = right_bottom.x;
        wrect.bottom = right_bottom.y;

        ClipCursor(&wrect);
    }
    else
    {
        ClipCursor(&originalRect);
    }
}

bool defos_is_cursor_clipped()
{
    return is_cursor_clipped;
}

void defos_set_cursor_locked(bool locked)
{
    if (!is_cursor_locked && locked)
    {
        GetCursorPos(&lock_point);
    }
    is_cursor_locked = locked;
}

bool defos_is_cursor_locked()
{
    return is_cursor_locked;
}

void defos_update() {
    if (is_cursor_locked) {
        SetCursorPos(lock_point.x, lock_point.y);
    }
}

// path of the cursor file,
// for defold it may be a good idea to save the cursor file to the save folder,
// then pass the path to this function to load
void defos_set_custom_cursor_win(const char *filename)
{
    custom_cursor = LoadCursorFromFile(_T(filename));
    SetCursor(custom_cursor);
    is_custom_cursor_loaded = true;
}

static LPCTSTR get_cursor(DefosCursor cursor);

void defos_set_cursor(DefosCursor cursor)
{
    custom_cursor = LoadCursor(NULL, get_cursor(cursor));
    SetCursor(custom_cursor);
    is_custom_cursor_loaded = true;
}

void defos_reset_cursor()
{
    // here we do not need to set cursor again, as we already ignore that in winproc
    is_custom_cursor_loaded = false;
}

/********************
 * internal functions
 ********************/

static LPCTSTR get_cursor(DefosCursor cursor) {
    switch (cursor) {
        case DEFOS_CURSOR_ARROW:
            return IDC_ARROW;
        case DEFOS_CURSOR_HAND:
            return IDC_HAND;
        case DEFOS_CURSOR_CROSSHAIR:
            return IDC_CROSS;
        case DEFOS_CURSOR_IBEAM:
            return IDC_IBEAM;
        default:
            return IDC_ARROW;
    }
}

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
    if (originalProc)
    {
        return;
    }

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

            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;

    case WM_MOUSELEAVE:
        is_mouse_inside = false;
        defos_emit_event(DEFOS_EVENT_MOUSE_LEAVE);
        break;

    case WM_SETCURSOR:
        if (is_custom_cursor_loaded)
        {
            SetCursor(custom_cursor);
            return TRUE;
        }
        break;

    case WM_SIZE:
        if (is_cursor_locked)
        {
            defos_set_cursor_locked(true);
        }
        break;
    }

    if (originalProc != NULL)
        return CallWindowProc(originalProc, hwnd, umsg, wp, lp);
    else
        return 0;
}

#endif
