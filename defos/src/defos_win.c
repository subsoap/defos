#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_WINDOWS)

#include <atlbase.h>
#include <atlconv.h>

// keep track of window placement when going to/from fullscreen or maximized
WINDOWPLACEMENT placement = { sizeof(placement) };

void defos_init() {}
void defos_final() {}

bool set_window_style(LONG_PTR style) {
    return SetWindowLongPtrA(dmGraphics::GetNativeWindowsHWND(), GWL_STYLE, style) != 0;
}

LONG_PTR get_window_style() {
    return GetWindowLongPtrA(dmGraphics::GetNativeWindowsHWND(), GWL_STYLE);
}

bool defos_is_fullscreen() {
    return !(get_window_style() & WS_OVERLAPPEDWINDOW);
}

bool defos_is_maximized() {
    return IsZoomed(dmGraphics::GetNativeWindowsHWND());
}

void defos_disable_maximize_button() {
    set_window_style(get_window_style() & ~WS_MAXIMIZEBOX);
}

void defos_disable_minimize_button() {
    set_window_style(get_window_style() & ~WS_MINIMIZEBOX);
}

void defos_disable_window_resize() {
    set_window_style(get_window_style() & ~WS_SIZEBOX);
}

void defos_disable_mouse_cursor() {
    ShowCursor(0);
}

void defos_enable_mouse_cursor() {
    ShowCursor(1);
}

// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/
void defos_toggle_fullscreen() {
    if(defos_is_maximized()) {
        defos_toggle_maximize();
    }

    HWND window = dmGraphics::GetNativeWindowsHWND();
    if(!defos_is_fullscreen()) {
        MONITORINFO mi = { sizeof(mi) };
        if(GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi)) {
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
    else {
        set_window_style(get_window_style() | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &placement);
        SetWindowPos(window, NULL,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

}

void defos_toggle_maximize() {
    if(defos_is_fullscreen()) {
        defos_toggle_fullscreen();
    }

    HWND window = dmGraphics::GetNativeWindowsHWND();
    if(defos_is_maximized()) {
        SetWindowPlacement(window, &placement);
    }
    else {
        GetWindowPlacement(window, &placement);
        ShowWindow(window, SW_MAXIMIZE);
    }
}

void defos_set_window_size(int x, int y, int w, int h) {
    if(x == -1) {
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }
    HWND window = dmGraphics::GetNativeWindowsHWND();
    SetWindowPos(window, window, x, y, w, h, SWP_NOZORDER);
}

void defos_set_window_title(const char* title_lua) {
    SetWindowTextW(dmGraphics::GetNativeWindowsHWND(), CA2W(title_lua));
}

WinRect defos_get_window_size(){
    HWND window = dmGraphics::GetNativeWindowsHWND();
    WINDOWPLACEMENT frame = { sizeof(placement) };
    GetWindowPlacement(window, &frame);
    WinRect rect;
    rect.x = frame.rcNormalPosition.left;
    rect.y = frame.rcNormalPosition.top;
    rect.w = frame.rcNormalPosition.right - frame.rcNormalPosition.left;
    rect.h = frame.rcNormalPosition.bottom - frame.rcNormalPosition.top;
    return rect;
}

#endif
