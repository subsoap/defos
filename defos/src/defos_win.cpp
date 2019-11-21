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

static bool is_window_on_top = false;
static bool is_window_active = true;
static bool is_cursor_visible = true;
static bool is_custom_cursor_loaded;

struct CustomCursor {
    HCURSOR cursor;
    int ref_count;
};

static CustomCursor * current_cursor;
static CustomCursor * default_cursors[DEFOS_CURSOR_INTMAX];

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
    is_window_active = true;
    is_window_on_top = false;
    is_mouse_inside = false;
    is_cursor_clipped = false;
    current_cursor = NULL;
    memset(default_cursors, 0, DEFOS_CURSOR_INTMAX * sizeof(CustomCursor*));
    GetClipCursor(&originalRect);  // keep the original clip for restore
    GetWindowPlacement(dmGraphics::GetNativeWindowsHWND(), &placement);
    subclass_window();
}

void defos_final()
{
    defos_set_cursor_clipped(false);
    restore_window_class();
    defos_gc_custom_cursor(current_cursor);
    for (int i = 0; i < DEFOS_CURSOR_INTMAX; i++) {
        defos_gc_custom_cursor(default_cursors[i]);
    }
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

void defos_toggle_always_on_top()
{
    is_window_on_top = !is_window_on_top;
    HWND window = dmGraphics::GetNativeWindowsHWND();
    SetWindowPos(window,
        is_window_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE
    );
}

bool defos_is_always_on_top() {
    return is_window_on_top;
}

void defos_minimize()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();
    ShowWindow(window, SW_MINIMIZE);
}

void defos_activate()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();
    SetForegroundWindow(window);
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
    HWND window = dmGraphics::GetNativeWindowsHWND();

    if (isnan(x) || isnan(y))
    {
        HMONITOR hMonitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(hMonitor, &monitorInfo);
        if (isnan(x)) { x = (monitorInfo.rcMonitor.left + monitorInfo.rcMonitor.right - w) / 2; }
        if (isnan(y)) { y = (monitorInfo.rcMonitor.top + monitorInfo.rcMonitor.bottom - h) / 2; }
    }

    SetWindowPos(window, window, (int)x, (int)y, (int)w, (int)h, SWP_NOZORDER);
}

void defos_set_view_size(float x, float y, float w, float h)
{
    HWND window = dmGraphics::GetNativeWindowsHWND();

    if (isnan(x) || isnan(y))
    {
        HMONITOR hMonitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(hMonitor, &monitorInfo);
        if (isnan(x)) { x = (monitorInfo.rcMonitor.left + monitorInfo.rcMonitor.right - w) / 2; }
        if (isnan(y)) { y = (monitorInfo.rcMonitor.top + monitorInfo.rcMonitor.bottom - h) / 2; }
    }

    RECT rect = {0, 0, (int)w, (int)h};

    DWORD style = (DWORD)get_window_style();

    // TODO: we are assuming the window have no menu, maybe it is better to expose it as parameter later
    AdjustWindowRect(&rect, style, false);

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

char* defos_get_bundle_root() {
    char *bundlePath = (char*)malloc(MAX_PATH);
    size_t ret = GetModuleFileNameA(GetModuleHandle(NULL), bundlePath, MAX_PATH);
    if (ret > 0 && ret < MAX_PATH) {
        // Remove the last path component
        size_t i = strlen(bundlePath);
        do {
            i -= 1;
            if (bundlePath[i] == '\\') {
                bundlePath[i] = 0;
                break;
            }
        } while (i);
    } else {
        bundlePath[0] = 0;
    }
    return bundlePath;
}

void defos_get_arguments(dmArray<char*> &arguments) {
    LPWSTR *szArglist;
    int nArgs;
    int i;
    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if( NULL != szArglist ){
        arguments.OffsetCapacity(nArgs);
        for( i=0; i<nArgs; i++) {
            const wchar_t *param = szArglist[i];
            int len = wcslen(param) + 1;
            char* lua_param = (char*)malloc(len);
            wcstombs(lua_param, param, len);
            arguments.Push(lua_param);
        }
    }
    LocalFree(szArglist);
}

WinRect defos_get_window_size()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();
    RECT rect;
    GetWindowRect(window, &rect);
    WinRect result = {
        .x = (float)rect.left,
        .y = (float)rect.top,
        .w = (float)(rect.right - rect.left),
        .h = (float)(rect.bottom - rect.top),
    };
    return result;
}

WinRect defos_get_view_size()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();

    RECT wrect;
    GetClientRect(window, &wrect);

    POINT pos = {wrect.left, wrect.top};
    ClientToScreen(window, &pos);

    WinRect rect = {
        .x = (float)pos.x,
        .y = (float)pos.y,
        .w = (float)(wrect.right - wrect.left),
        .h = (float)(wrect.bottom - wrect.top),
    };
    return rect;
}

WinPoint defos_get_cursor_pos()
{
    POINT point;
    GetCursorPos(&point);

    WinPoint result = { .x = (float)point.x, .y = (float)point.y };
    return result;
}

WinPoint defos_get_cursor_pos_view()
{
    POINT point;
    GetCursorPos(&point);

    HWND window = dmGraphics::GetNativeWindowsHWND();
    ScreenToClient(window, &point);

    WinPoint result = { .x = (float)point.x, .y = (float)point.y };
    return result;
}

void defos_set_cursor_pos(float x, float y)
{
    SetCursorPos((int)x, (int)y);
}

// move cursor to pos relative to current window
// top-left is (0, 0)
void defos_set_cursor_pos_view(float x, float y)
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
    if (is_cursor_locked && is_window_active) {
        SetCursorPos(lock_point.x, lock_point.y);
    }
}

void * defos_load_cursor_win(const char *filename)
{
    CustomCursor * cursor = new CustomCursor();
    cursor->cursor = LoadCursorFromFile(_T(filename));
    cursor->ref_count = 1;
    return cursor;
}

void defos_gc_custom_cursor(void * _cursor)
{
    CustomCursor * cursor = (CustomCursor*)_cursor;
    if (!cursor) { return; }
    cursor->ref_count -= 1;
    if (!cursor->ref_count) {
        delete cursor;
    }
}

void defos_set_custom_cursor(void * _cursor)
{
    CustomCursor * cursor = (CustomCursor*)_cursor;
    cursor->ref_count += 1;
    defos_gc_custom_cursor(current_cursor);
    current_cursor = cursor;
    SetCursor(current_cursor->cursor);
}

static LPCTSTR get_cursor(DefosCursor cursor);

void defos_set_cursor(DefosCursor cursor_type)
{
    CustomCursor * cursor = default_cursors[cursor_type];
    if (!cursor) {
        cursor = new CustomCursor();
        cursor->cursor = LoadCursor(NULL, get_cursor(cursor_type));
        cursor->ref_count = 1;
        default_cursors[cursor_type] = cursor;
    }
    defos_set_custom_cursor(cursor);
}

void defos_reset_cursor()
{
    defos_gc_custom_cursor(current_cursor);
    current_cursor = NULL;
}

static char* copy_string(const char *s)
{
    char *buffer = (char*)malloc(strlen(s) + 1);
    strcpy(buffer, s);
    return buffer;
}

static unsigned long translate_orientation(DWORD orientation)
{
    switch (orientation)
    {
        case DMDO_DEFAULT: return 0;
        case DMDO_90: return 90;
        case DMDO_180: return 180;
        case DMDO_270: return 270;
        default: return 0;
    }
}

static void parse_display_mode(const DEVMODE &devMode, DisplayModeInfo &mode)
{
    mode.width = devMode.dmPelsWidth;
    mode.height = devMode.dmPelsHeight;
    mode.bits_per_pixel = devMode.dmBitsPerPel;
    mode.refresh_rate = devMode.dmDisplayFrequency;
    mode.scaling_factor = 1.0;
    mode.orientation = (devMode.dmFields & DM_DISPLAYORIENTATION)
        ? translate_orientation(devMode.dmDisplayOrientation)
        : 0;
    mode.reflect_x = false;
    mode.reflect_y = false;
}

static BOOL CALLBACK monitor_enum_callback(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(hMonitor, &monitorInfo)) { return TRUE; }

    DisplayInfo display;
    display.id = copy_string(monitorInfo.szDevice);
    display.bounds.x = monitorInfo.rcMonitor.left;
    display.bounds.y = monitorInfo.rcMonitor.top;
    display.bounds.w = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    display.bounds.h = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

    DEVMODE devMode;
    devMode.dmSize = sizeof(devMode);
    EnumDisplaySettingsEx(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode, 0);
    parse_display_mode(devMode, display.mode);
    display.mode.scaling_factor = (double)devMode.dmPelsWidth / (double)(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);

    DISPLAY_DEVICE displayDevice;
    displayDevice.cb = sizeof(displayDevice);
    EnumDisplayDevices(monitorInfo.szDevice, 0, &displayDevice, 0);
    display.name = copy_string(displayDevice.DeviceString);

    dmArray<DisplayInfo> *displayList = reinterpret_cast<dmArray<DisplayInfo>*>(dwData);
    displayList->OffsetCapacity(1);
    displayList->Push(display);

    return TRUE;
}

void defos_get_displays(dmArray<DisplayInfo> &displayList)
{
    EnumDisplayMonitors(NULL, NULL, monitor_enum_callback, reinterpret_cast<LPARAM>(&displayList));
}

struct MonitorScaleData {
    const char *display_device_name;
    double scaling_factor;
};

static BOOL CALLBACK monitor_scale_enum_callback(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
    MonitorScaleData *data = reinterpret_cast<MonitorScaleData*>(dwData);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(hMonitor, &monitorInfo)) { return TRUE; }

    if (strcmp(monitorInfo.szDevice, data->display_device_name) != 0) { return TRUE; }

    DEVMODE devMode;
    devMode.dmSize = sizeof(devMode);
    EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode);
    data->scaling_factor = (double)devMode.dmPelsWidth / (double)(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);

    return FALSE;
}

static double scale_of_monitor(DisplayID displayID)
{
    MonitorScaleData data = {
        .display_device_name = displayID,
        .scaling_factor = 1.0,
    };
    EnumDisplayMonitors(NULL, NULL, monitor_scale_enum_callback, reinterpret_cast<LPARAM>(&data));
    return data.scaling_factor;
}

void defos_get_display_modes(DisplayID displayID, dmArray<DisplayModeInfo> &modeList)
{
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(devMode);

    double scaling_factor = scale_of_monitor(displayID);

    for (int i = 0; EnumDisplaySettingsEx(displayID, i, &devMode, 0) != 0; i++)
    {
        DisplayModeInfo mode;
        parse_display_mode(devMode, mode);
        mode.scaling_factor = scaling_factor;

        bool isDuplicate = false;
        for (int j = (int)modeList.Size() - 1; j >= 0; j--)
        {
            DisplayModeInfo &otherMode = modeList[j];
            if (mode.width == otherMode.width
                && mode.height == otherMode.height
                && mode.bits_per_pixel == otherMode.bits_per_pixel
                && mode.refresh_rate == otherMode.refresh_rate
            ) {
                isDuplicate = true;
                break;
            }
        }

        if (isDuplicate) { continue; }
        modeList.OffsetCapacity(1);
        modeList.Push(mode);
    }
}

DisplayID defos_get_current_display()
{
    HWND window = dmGraphics::GetNativeWindowsHWND();
    HMONITOR hMonitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(hMonitor, &monitorInfo);
    return copy_string(monitorInfo.szDevice);
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
        if (current_cursor)
        {
            SetCursor(current_cursor->cursor);
            return TRUE;
        }
        break;

    case WM_WINDOWPOSCHANGED:
        if (is_cursor_clipped) { defos_set_cursor_clipped(true); }
        break;

    case WM_ACTIVATE:
        if (wp != WA_INACTIVE)
        {
            is_window_active = true;
            if (is_cursor_clipped) { defos_set_cursor_clipped(true); }
        } else {
            is_window_active = false;
        }
    }

    if (originalProc != NULL)
        return CallWindowProc(originalProc, hwnd, umsg, wp, lp);
    else
        return 0;
}

#endif
