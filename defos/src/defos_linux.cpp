#if defined(DM_PLATFORM_LINUX) && !defined(DM_HEADLESS)

/*
    some resources to manage window for x11.

    1. https://tronche.com/gui/x/xlib/
    2. https://github.com/glfw/glfw/blob/master/src/x11_window.c
    3. https://github.com/yetanothergeek/xctrl/blob/master/src/xctrl.c
*/

/* TODO:
 ON_MOUSE_ENTER / ON_MOUSE_LEAVE
 cursor locking
 cursor clipping
 setting the window icon
*/

#include <dmsdk/sdk.h>

#include "defos_private.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <Xcursor.h>
#include <Xrandr.h>

#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/auxv.h>
#include <limits.h>

//static GC gc;
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2
#define XATOM(name) XInternAtom(disp, name, False)

static Display *disp;
static int screen;
static Window win;
static Window root;

// TODO: add support checking
static Atom UTF8_STRING;
static Atom NET_WM_NAME;
static Atom NET_WM_STATE;
static Atom NET_WM_STATE_FULLSCREEN;
static Atom NET_WM_STATE_MAXIMIZED_VERT;
static Atom NET_WM_STATE_MAXIMIZED_HORZ;
static Atom NET_WM_STATE_ABOVE;
static Atom NET_WM_ALLOWED_ACTIONS;
static Atom NET_WM_ACTION_MAXIMIZE_HORZ;
static Atom NET_WM_ACTION_MAXIMIZE_VERT;
static Atom NET_WM_ACTION_MINIMIZE;
static Atom NET_FRAME_EXTENTS;
static Atom NET_ACTIVE_WINDOW;
// Used by display modes: the list of states supported by the window manager and the desktop work
// area (screen minus panels).
static Atom NET_SUPPORTED;
static Atom NET_WORKAREA;
static Atom NET_CURRENT_DESKTOP;

struct CustomCursor {
    Cursor cursor;
    int ref_count;
};

static CustomCursor * current_cursor;
static CustomCursor * default_cursors[DEFOS_CURSOR_INTMAX];

static bool is_cursor_visible = true;
static bool is_cursor_actually_visible = true;
static bool window_has_focus = true;
static bool resize_locked = false;

// Whether the user may resize the window. A locked size is enforced with WM size hints
// (lock_resize).
static bool user_resizable = true;

// Last requested display mode. It has to be the requested one, not the actual one: the window
// manager applies states asynchronously and right after a request still reports the old state.
static DefosDisplayMode intended_display_mode = DEFOS_DISPLAY_MODE_WINDOWED;

// Share of the work area used by the windowed mode when no size is given.
static const float WINDOWED_WORK_AREA_PART = 0.8f;

static bool is_window_visible(Window window);
static void send_message(Window &window, Atom type, long a, long b, long c, long d, long e);

static bool is_cursor_in_view = true;

void defos_init()
{
    disp = XOpenDisplay(NULL);
    screen = DefaultScreen(disp);
    win = dmGraphics::GetNativeX11Window();
    root = XDefaultRootWindow(disp);

    // from https://specifications.freedesktop.org/wm-spec/1.3/ar01s05.html
    UTF8_STRING = XATOM("UTF8_STRING");
    NET_WM_NAME = XATOM("_NET_WM_NAME");
    NET_WM_STATE = XATOM("_NET_WM_STATE");
    NET_WM_STATE_FULLSCREEN = XATOM("_NET_WM_STATE_FULLSCREEN");
    NET_WM_STATE_MAXIMIZED_VERT = XATOM("_NET_WM_STATE_MAXIMIZED_VERT");
    NET_WM_STATE_MAXIMIZED_HORZ = XATOM("_NET_WM_STATE_MAXIMIZED_HORZ");
    NET_WM_STATE_ABOVE = XATOM("_NET_WM_STATE_ABOVE");
    NET_WM_ALLOWED_ACTIONS = XATOM("_NET_WM_ALLOWED_ACTIONS");
    NET_WM_ACTION_MINIMIZE = XATOM("_NET_WM_ACTION_MINIMIZE");
    NET_WM_ACTION_MAXIMIZE_HORZ = XATOM("_NET_WM_ACTION_MAXIMIZE_HORZ");
    NET_WM_ACTION_MAXIMIZE_VERT = XATOM("_NET_WM_ACTION_MAXIMIZE_VERT");
    NET_FRAME_EXTENTS = XATOM("_NET_FRAME_EXTENTS");
    NET_ACTIVE_WINDOW = XATOM("_NET_ACTIVE_WINDOW");
    NET_SUPPORTED = XATOM("_NET_SUPPORTED");
    NET_WORKAREA = XATOM("_NET_WORKAREA");
    NET_CURRENT_DESKTOP = XATOM("_NET_CURRENT_DESKTOP");

    resize_locked = false;
    is_cursor_visible = true;
    is_cursor_actually_visible = true;
    window_has_focus = true;

    current_cursor = NULL;
    memset(default_cursors, 0, DEFOS_CURSOR_INTMAX * sizeof(CustomCursor*));
}

void defos_final()
{
    defos_gc_custom_cursor(current_cursor);
    for (int i = 0; i < DEFOS_CURSOR_INTMAX; i++) {
        defos_gc_custom_cursor(default_cursors[i]);
    }
}

void defos_event_handler_was_set(DefosEvent event)
{
}

static Atom* get_atom_list(Atom property, unsigned long &nItems)
{
    Atom actualType;
    int actualFormat;
    unsigned long bytesAfter;
    Atom* data = NULL;
    XEvent event;
    while (XGetWindowProperty(disp, win, property,
        0, (~0L), False, AnyPropertyType,
        &actualType, &actualFormat,
        &nItems, &bytesAfter, (unsigned char**)&data) == Success && bytesAfter != 0
    ) {
        XNextEvent(disp, &event);
    }

    return data;
}

static bool hint_state_contains_atom(Atom atom)
{
    unsigned long nItems;
    Atom* data = get_atom_list(NET_WM_STATE, nItems);

    if (data) {
        for (unsigned int i = 0; i < nItems; i++) {
            if (data[i] == atom) {
                XFree(data);
                return true;
            }
        }
        XFree(data);
    }

    return false;
}

bool defos_is_fullscreen()
{
    return hint_state_contains_atom(NET_WM_STATE_FULLSCREEN);
}

bool defos_is_borderless() {
    // TODO: add defos_is_borderless for Linux
    return true;
}

bool defos_is_maximized()
{
    return hint_state_contains_atom(NET_WM_STATE_MAXIMIZED_VERT);
}

bool defos_is_always_on_top()
{
    return hint_state_contains_atom(NET_WM_STATE_ABOVE);
}

bool defos_is_mouse_in_view()
{
    Window d1, d2;
    int x, y, d3, d4;
    unsigned int d5;
    if (!XQueryPointer(disp, win, &d1, &d2, &d3, &d4, &x, &y, &d5)) { return false; }

    if (x < 0 || y < 0) { return false; }

    unsigned int w, h, d6;
    XGetGeometry(disp, win, &d1, &d3, &d4, &w, &h, &d5, &d6);

    if ((unsigned)x >= w || (unsigned)y >= h) { return false; }
    return true;
}

void defos_disable_maximize_button()
{
    unsigned long nItems;
    Atom* data = get_atom_list(NET_WM_ALLOWED_ACTIONS, nItems);

    if (!data) { return; }

    // Filter the allowed actions list
    Atom* newList = (Atom*)malloc(sizeof(Atom) * nItems);
    unsigned long newNItems = 0;
    for (unsigned long i = 0; i < nItems; i++) {
        if (data[i] == NET_WM_ACTION_MAXIMIZE_HORZ || data[i] == NET_WM_ACTION_MAXIMIZE_VERT) { continue; }
        newList[newNItems++] = data[i];
    }
    XFree(data);

    XChangeProperty(disp, win, NET_WM_ALLOWED_ACTIONS, XA_ATOM, 32, PropModeReplace, (unsigned char*)newList, newNItems);
    XFlush(disp);
    free(newList);
}

void defos_disable_minimize_button()
{
    unsigned long nItems;
    Atom* data = get_atom_list(NET_WM_ALLOWED_ACTIONS, nItems);

    if (!data) { return; }

    // Filter the allowed actions list
    Atom* newList = (Atom*)malloc(sizeof(Atom) * nItems);
    unsigned long newNItems = 0;
    for (unsigned long i = 0; i < nItems; i++) {
        if (data[i] == NET_WM_ACTION_MINIMIZE) { return; }
        newList[newNItems++] = data[i];
    }
    XFree(data);

    XChangeProperty(disp, win, NET_WM_ALLOWED_ACTIONS, XA_ATOM, 32, PropModeReplace, (unsigned char*)newList, newNItems);
    XFlush(disp);
    free(newList);
}

static void lock_resize(int width, int height)
{
    XSizeHints *sizeHints = XAllocSizeHints();
    sizeHints->flags = PMinSize | PMaxSize;
    sizeHints->min_width = width;
    sizeHints->min_height = height;
    sizeHints->max_width = width;
    sizeHints->max_height = height;

    XSetWMNormalHints(disp, win, sizeHints);
    XFlush(disp);
    XFree(sizeHints);

    resize_locked = true;
}

void defos_disable_window_resize()
{
    // Go through the common resize control so that the lock can also be lifted later.
    defos_set_window_resizable(false);
}

static void apply_cursor() {
    Cursor cursor = current_cursor ? current_cursor->cursor : None;
    XDefineCursor(disp, win, cursor);
}

static void apply_cursor_visible() {
    bool visible = is_cursor_visible || !window_has_focus;
    if (visible == is_cursor_actually_visible) { return; }
    is_cursor_actually_visible = visible;

    if (visible) {
		    XFixesShowCursor(disp, win);
    } else {
		    XFixesHideCursor(disp, win);
    }
    XFlush(disp);
}

void defos_set_cursor_visible(bool visible)
{
    if (visible == is_cursor_visible) { return; }
    is_cursor_visible = visible;
    apply_cursor_visible();
}

bool defos_is_cursor_visible()
{
    return is_cursor_visible;
}

void defos_toggle_fullscreen()
{
    send_message(win,
        NET_WM_STATE,
        _NET_WM_STATE_TOGGLE,
        NET_WM_STATE_FULLSCREEN,
        0,
        1,
        0
    );
    XFlush(disp);
}

void defos_toggle_borderless() {
    // TODO: add defos_toggle_borderless for Linux
}

void defos_toggle_maximized()
{
    send_message(win,
        NET_WM_STATE,
        _NET_WM_STATE_TOGGLE,
        NET_WM_STATE_MAXIMIZED_VERT,
        NET_WM_STATE_MAXIMIZED_HORZ,
        1,
        0
    );
    XFlush(disp);
}

void defos_toggle_always_on_top()
{
    send_message(win,
        NET_WM_STATE,
        defos_is_always_on_top() ? _NET_WM_STATE_REMOVE : _NET_WM_STATE_ADD,
        NET_WM_STATE_ABOVE,
        0,
        1,
        0
    );
    XFlush(disp);
}

void defos_minimize()
{
    XIconifyWindow(disp, win, screen);
}

void defos_activate()
{
    send_message(win,
        NET_ACTIVE_WINDOW,
        1,
        1,
        0,
        0,
        0
    );
    XFlush(disp);
}

void defos_set_console_visible(bool visible)
{
    dmLogWarning("Method 'set_console_visible' is only supported on Windows");
}

bool defos_is_console_visible()
{
    return false;
}

typedef struct {
    long left, right, top, bottom;
} WindowExtents;

static WindowExtents get_window_extents()
{
    Atom actualType;
    int actualFormat;
    unsigned long nitems, bytesAfter;
    long* extents = NULL;
    XEvent event;
    while (XGetWindowProperty(disp, win, NET_FRAME_EXTENTS,
        0, 4, False, AnyPropertyType,
        &actualType, &actualFormat,
        &nitems, &bytesAfter, (unsigned char**)&extents) == Success && bytesAfter != 0
    ) {
        XNextEvent(disp, &event);
    }

    if (!extents || nitems != 4) {
        WindowExtents result = { 0, 0, 0, 0 };
        if (extents) { XFree(extents); }
        return result;
    }

    WindowExtents result = { extents[0], extents[1], extents[2], extents[3] };
    if (extents) { XFree(extents); }
    return result;
}

static RRCrtc get_current_crtc(WinRect &bounds);

void defos_set_window_size(float x, float y, float w, float h)
{
    // change size only if it is visible
    if (is_window_visible(win))
    {
        if (isnan(x) || isnan(y))
        {
            WinRect screenBounds;
            get_current_crtc(screenBounds);
            if (isnan(x)) { x = screenBounds.x + ((float)screenBounds.w - w) / 2; }
            if (isnan(y)) { y = screenBounds.y + ((float)screenBounds.h - h) / 2; }
        }

        WindowExtents extents = get_window_extents();
        w -= extents.left + extents.right;
        h -= extents.top + extents.bottom;
        x += extents.left;
        y += extents.top;

        if (resize_locked) { lock_resize(w, h); }
        XMoveResizeWindow(disp, win, (int)x, (int)y, (unsigned int)w, (unsigned int)h);
        XFlush(disp);
    }
}

void defos_set_view_size(float x, float y, float w, float h)
{
    // change size only if it is visible
    if (is_window_visible(win))
    {
        if (isnan(x) || isnan(y))
        {
            WinRect screenBounds;
            get_current_crtc(screenBounds);
            if (isnan(x)) { x = screenBounds.x + ((float)screenBounds.w - w) / 2; }
            if (isnan(y)) { y = screenBounds.y + ((float)screenBounds.h - h) / 2; }
        }

        if (resize_locked) { lock_resize(w, h); }
        XMoveResizeWindow(disp, win, (int)x, (int)y, (unsigned int)w, (unsigned int)h);
        XFlush(disp);
    }
}

void defos_set_window_title(const char *title_lua)
{
    XChangeProperty(disp, win, NET_WM_NAME, UTF8_STRING, 8, PropModeReplace, (unsigned char *)title_lua, strlen(title_lua));
    XFlush(disp); // IMPORTANT: we have to flush, or nothing will be changed
}

WinRect defos_get_window_size()
{
    WindowExtents extents = get_window_extents();
    WinRect size = defos_get_view_size();

    size.w += extents.left + extents.right;
    size.h += extents.top + extents.bottom;
    size.x -= extents.left;
    size.y -= extents.top;

    return size;
}

WinRect defos_get_view_size()
{
    int x, y;
    unsigned int w, h, bw, depth;

    Window dummy;
    XGetGeometry(disp, win, &dummy, &x, &y, &w, &h, &bw, &depth);
    XTranslateCoordinates(disp, win, root, 0, 0, &x, &y, &dummy);

    WinRect r = {(float)x, (float)y, (float)w, (float)h};
    return r;
}

// ============================ Display modes =============================
// On Linux window states belong to the window manager: both fullscreen and maximizing to the work
// area are EWMH states, and the WM accounts for desktop panels itself. So there is no frame to
// compute and no mode to store here: the state can always be read back from the WM. The only thing
// to check is whether the WM supports these states.

static bool is_wm_state_supported(Atom state)
{
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    Atom *supported = NULL;
    bool result = false;

    if (XGetWindowProperty(disp, root, NET_SUPPORTED,
            0, (~0L), False, XA_ATOM,
            &actualType, &actualFormat,
            &nItems, &bytesAfter, (unsigned char **)&supported) == Success
        && supported)
    {
        for (unsigned long i = 0; i < nItems; i++)
        {
            if (supported[i] == state) { result = true; break; }
        }
        XFree(supported);
    }

    return result;
}

static void set_wm_state(bool add, Atom first, Atom second)
{
    send_message(win,
        NET_WM_STATE,
        add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
        first,
        second,
        1,
        0
    );
    XFlush(disp);
}

// Removes hard size limits. Otherwise the WM rejects our own XMoveResizeWindow when resizing was
// locked before.
static void unlock_resize()
{
    XSizeHints *sizeHints = XAllocSizeHints();
    sizeHints->flags = PMinSize;
    sizeHints->min_width = 1;
    sizeHints->min_height = 1;

    XSetWMNormalHints(disp, win, sizeHints);
    XFlush(disp);
    XFree(sizeHints);

    resize_locked = false;
}

static unsigned long get_current_desktop()
{
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *data = NULL;
    unsigned long desktop = 0;

    if (XGetWindowProperty(disp, root, NET_CURRENT_DESKTOP,
            0, 1, False, XA_CARDINAL,
            &actualType, &actualFormat,
            &nItems, &bytesAfter, &data) == Success
        && data)
    {
        if (nItems >= 1) { desktop = (unsigned long)(*(long *)data); }
        XFree(data);
    }

    return desktop;
}

// Work area of the monitor the window is on. _NET_WORKAREA describes the work area of the whole
// virtual screen (panels already excluded), so it is intersected with the monitor rectangle.
// If the WM does not provide the property, the whole monitor is returned.
static void get_work_area_rect(WinRect &out)
{
    WinRect monitor = { 0, 0, 0, 0 };
    get_current_crtc(monitor);
    out = monitor;

    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *data = NULL;

    if (XGetWindowProperty(disp, root, NET_WORKAREA,
            0, (~0L), False, XA_CARDINAL,
            &actualType, &actualFormat,
            &nItems, &bytesAfter, &data) != Success
        || !data)
    {
        return;
    }

    if (nItems < 4)
    {
        XFree(data);
        return;
    }

    // Each desktop has its own entry of four numbers (x, y, width, height).
    long *area = (long *)data;
    unsigned long offset = get_current_desktop() * 4;
    if (offset + 4 > nItems) { offset = 0; }

    float left = (float)area[offset + 0];
    float top = (float)area[offset + 1];
    float right = left + (float)area[offset + 2];
    float bottom = top + (float)area[offset + 3];
    XFree(data);

    float clipped_left = monitor.x > left ? monitor.x : left;
    float clipped_top = monitor.y > top ? monitor.y : top;
    float monitor_right = monitor.x + monitor.w;
    float monitor_bottom = monitor.y + monitor.h;
    float clipped_right = monitor_right < right ? monitor_right : right;
    float clipped_bottom = monitor_bottom < bottom ? monitor_bottom : bottom;

    if (clipped_right <= clipped_left || clipped_bottom <= clipped_top) { return; }

    out.x = clipped_left;
    out.y = clipped_top;
    out.w = clipped_right - clipped_left;
    out.h = clipped_bottom - clipped_top;
}

// Sets a regular window: client area plus frame must fit into the work area, and the window is
// centered on it.
static void apply_windowed_geometry(float width, float height)
{
    WinRect work;
    get_work_area_rect(work);

    float client_w = isnan(width) ? work.w * WINDOWED_WORK_AREA_PART : width;
    float client_h = isnan(height) ? work.h * WINDOWED_WORK_AREA_PART : height;

    WindowExtents extents = get_window_extents();
    float frame_w = client_w + extents.left + extents.right;
    float frame_h = client_h + extents.top + extents.bottom;
    if (frame_w > work.w)
    {
        client_w -= frame_w - work.w;
        frame_w = work.w;
    }
    if (frame_h > work.h)
    {
        client_h -= frame_h - work.h;
        frame_h = work.h;
    }
    if (client_w < 1) { client_w = 1; }
    if (client_h < 1) { client_h = 1; }

    // XMoveResizeWindow works with the client area, so the frame thickness is added to the top-left
    // corner of the window.
    float x = work.x + (work.w - frame_w) / 2 + extents.left;
    float y = work.y + (work.h - frame_h) / 2 + extents.top;

    unlock_resize();
    XMoveResizeWindow(disp, win, (int)x, (int)y, (unsigned int)client_w, (unsigned int)client_h);
    XFlush(disp);

    if (!user_resizable) { lock_resize((int)client_w, (int)client_h); }
}

void defos_set_display_mode(DefosDisplayMode mode, float width, float height)
{
    intended_display_mode = mode;

    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN)
    {
        // Drop the hard size hints: from here on the WM manages the window.
        unlock_resize();
        set_wm_state(false, NET_WM_STATE_MAXIMIZED_VERT, NET_WM_STATE_MAXIMIZED_HORZ);
        set_wm_state(true, NET_WM_STATE_FULLSCREEN, 0);
        return;
    }

    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED)
    {
        // The WM fits a maximized window into the work area itself, without covering panels.
        unlock_resize();
        set_wm_state(false, NET_WM_STATE_FULLSCREEN, 0);
        set_wm_state(true, NET_WM_STATE_MAXIMIZED_VERT, NET_WM_STATE_MAXIMIZED_HORZ);
        return;
    }

    set_wm_state(false, NET_WM_STATE_FULLSCREEN, 0);
    set_wm_state(false, NET_WM_STATE_MAXIMIZED_VERT, NET_WM_STATE_MAXIMIZED_HORZ);

    // Let the WM process the removed states first: otherwise it restores the previous geometry after
    // our XMoveResizeWindow and the requested size is lost.
    XSync(disp, False);

    if (is_window_visible(win))
    {
        apply_windowed_geometry(width, height);
    }
}

DefosDisplayMode defos_get_display_mode()
{
    // Always read the mode from the window manager: the user may have maximized the window or entered
    // fullscreen with a system shortcut.
    if (defos_is_fullscreen()) { return DEFOS_DISPLAY_MODE_FULLSCREEN; }
    if (defos_is_maximized()) { return DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED; }
    return DEFOS_DISPLAY_MODE_WINDOWED;
}

bool defos_is_display_mode_supported(DefosDisplayMode mode)
{
    // Fullscreen and maximizing are done by the WM, so ask it for the supported states: a mode whose
    // state is missing would do nothing.
    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN)
    {
        return is_wm_state_supported(NET_WM_STATE_FULLSCREEN);
    }
    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED)
    {
        return is_wm_state_supported(NET_WM_STATE_MAXIMIZED_VERT)
            && is_wm_state_supported(NET_WM_STATE_MAXIMIZED_HORZ);
    }
    return true;
}

WinRect defos_get_work_area()
{
    WinRect work;
    get_work_area_rect(work);
    return work;
}

void defos_set_window_resizable(bool resizable)
{
    user_resizable = resizable;

    // Locking the size only makes sense for a regular window: in fullscreen and maximized states the
    // WM manages the size, and hard size hints may get in its way. Use the requested mode rather than
    // the current window state: states are applied asynchronously and right after a request the old
    // mode is still reported.
    if (resizable || intended_display_mode != DEFOS_DISPLAY_MODE_WINDOWED)
    {
        unlock_resize();
        return;
    }

    WinRect view = defos_get_view_size();
    lock_resize((int)view.w, (int)view.h);
}

bool defos_is_window_resizable()
{
    return user_resizable;
}

WinPoint defos_get_cursor_pos()
{
    WinPoint point = { .x = -INFINITY, .y = -INFINITY };
    Window d1, d2;
    int x, y, d3, d4;
    unsigned int d5;
    if (XQueryPointer(disp, root, &d1, &d2, &d3, &d4, &x, &y, &d5)) {
        point.x = x;
        point.y = y;
    }
    return point;
}

WinPoint defos_get_cursor_pos_view()
{
    WinPoint point = { .x = -INFINITY, .y = -INFINITY };
    Window d1, d2;
    int x, y, d3, d4;
    unsigned int d5;
    if (XQueryPointer(disp, win, &d1, &d2, &d3, &d4, &x, &y, &d5)) {
        point.x = x;
        point.y = y;
    }
    return point;
}

void defos_set_cursor_pos(float x, float y)
{
    XWarpPointer(disp, None, root, 0, 0, 0, 0, (int)x, (int)y);
    XFlush(disp);
}

void defos_set_cursor_pos_view(float x, float y)
{
    XWarpPointer(disp, None, win, 0, 0, 0, 0, (int)x, (int)y);
    XFlush(disp);
}

void defos_set_cursor_clipped(bool clipped)
{
    dmLogWarning("Method 'set_cursor_clipped' is not supported on Linux");
}

bool defos_is_cursor_clipped()
{
    return false;
}

void defos_set_cursor_locked(bool locked)
{
    dmLogWarning("Method 'set_cursor_locked' is not supported on Linux");
}

bool defos_is_cursor_locked()
{
    return false;
}

void defos_update()
{
	// Show/hide cursor when window is focused/unfocused
    Window focused_window;
    int revert_to;
    if (!XGetInputFocus(disp, &focused_window, &revert_to)) {
      focused_window = None;
    }

    window_has_focus = focused_window == win;
    apply_cursor_visible();

    // Cursor enter/exit events
    bool visible = defos_is_mouse_in_view() && window_has_focus;
    if (visible != is_cursor_in_view)
    {
        is_cursor_in_view = visible;
        defos_emit_event(is_cursor_in_view ? DEFOS_EVENT_MOUSE_ENTER : DEFOS_EVENT_MOUSE_LEAVE);
    }
}

void * defos_load_cursor_linux(const char *filename)
{
    CustomCursor * cursor = new CustomCursor();
    int cursorSize = XcursorGetDefaultSize(disp);
    XcursorImage * image = XcursorFilenameLoadImage(filename, cursorSize);
    if (image) {
      cursor->cursor = XcursorImageLoadCursor(disp, image);
      XcursorImageDestroy(image);
    } else {
      cursor->cursor = XCreateFontCursor(disp, XC_left_ptr);
    }
    cursor->ref_count = 1;
    return cursor;
}

void defos_gc_custom_cursor(void * _cursor)
{
    CustomCursor * cursor = (CustomCursor*)_cursor;
    if (!cursor) { return; }
    cursor->ref_count -= 1;
    if (!cursor->ref_count) {
        XFreeCursor(disp, cursor->cursor);
        delete cursor;
    }
}

void defos_set_custom_cursor(void * _cursor)
{
    CustomCursor * old_cursor = current_cursor;

    CustomCursor * cursor = (CustomCursor*)_cursor;
    cursor->ref_count += 1;
    current_cursor = cursor;

    apply_cursor();

    defos_gc_custom_cursor(old_cursor);
}

static unsigned int get_cursor(DefosCursor cursor);

void defos_set_cursor(DefosCursor cursor_type)
{
    CustomCursor * cursor = default_cursors[cursor_type];
    if (!cursor) {
        cursor = new CustomCursor();
        cursor->cursor = XCreateFontCursor(disp, get_cursor(cursor_type));
        cursor->ref_count = 1;
        default_cursors[cursor_type] = cursor;
    }
    defos_set_custom_cursor(cursor);
}

void defos_reset_cursor()
{
    XUndefineCursor(disp, win);
    defos_gc_custom_cursor(current_cursor);
    current_cursor = NULL;
}

static unsigned int get_cursor(DefosCursor cursor)
{
    switch (cursor)
    {
    case DEFOS_CURSOR_ARROW:
        return XC_left_ptr;
    case DEFOS_CURSOR_CROSSHAIR:
        return XC_tcross;
    case DEFOS_CURSOR_HAND:
        return XC_hand2;
    case DEFOS_CURSOR_IBEAM:
        return XC_xterm;
    default:
        return XC_left_ptr;
    }
}

static bool is_window_visible(Window window)
{
    XWindowAttributes attributes;
    XGetWindowAttributes(disp, window, &attributes);
    return attributes.map_state == IsViewable;
}

static const XRRModeInfo* get_mode_info(const XRRScreenResources* screenResources, RRMode id){
    for (int i = 0; i < screenResources->nmode; i++)
    {
        if (screenResources->modes[i].id == id)
        {
            return screenResources->modes + i;
        }
    }
    return NULL;
}

static double compute_refresh_rate(const XRRModeInfo* modeInfo)
{
    if (!modeInfo->hTotal || !modeInfo->vTotal)
    {
        return 0;
    }
    return ((double)modeInfo->dotClock / ((double)modeInfo->hTotal * (double)modeInfo->vTotal));
}

static bool axis_flipped(Rotation rotation)
{
    return !!(rotation & (RR_Rotate_90 | RR_Rotate_270));
}

static unsigned long orientation_from_rotation(Rotation rotation)
{
    if (rotation & RR_Rotate_0) { return 0; }
    if (rotation & RR_Rotate_90) { return 90; }
    if (rotation & RR_Rotate_180) { return 180; }
    if (rotation & RR_Rotate_270) { return 270; }
    return 0;
}

static void parse_display_mode(const XRRModeInfo* modeInfo, DisplayModeInfo &mode, Rotation rotation)
{
    if (axis_flipped(rotation))
    {
        mode.width = modeInfo->height;
        mode.height = modeInfo->width;
    } else {
        mode.width = modeInfo->width;
        mode.height = modeInfo->height;
    }
    mode.refresh_rate = compute_refresh_rate(modeInfo);
    mode.bits_per_pixel = 32;
    mode.scaling_factor = 1.0;
    mode.orientation = orientation_from_rotation(rotation);
    mode.reflect_x = !!(rotation & RR_Reflect_X);
    mode.reflect_y = !!(rotation & RR_Reflect_Y);
}

void defos_get_displays(dmArray<DisplayInfo> &displayList)
{
    XRRScreenResources *screenResources = XRRGetScreenResourcesCurrent(disp, win);
    unsigned long bpp = (long)DefaultDepth(disp, screen);

    displayList.OffsetCapacity(screenResources->ncrtc);
    for (int i = 0; i < screenResources->ncrtc; i++)
    {
        RRCrtc crtc = screenResources->crtcs[i];
        DisplayInfo display;

        XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, screenResources, crtc);
        const XRRModeInfo * modeInfo = get_mode_info(screenResources, crtcInfo->mode);

        if (!modeInfo)
        {
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        bool isMirror = false;
        for (unsigned int j = 0; j < displayList.Size(); j++)
        {
            DisplayInfo &otherDisplay = displayList[j];
            if (otherDisplay.bounds.x == crtcInfo->x
                && otherDisplay.bounds.y == crtcInfo->y
                && otherDisplay.bounds.w == crtcInfo->width
                && otherDisplay.bounds.h == crtcInfo->height)
            {
                isMirror = true;
                break;
            }
        }

        if (isMirror)
        {
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        display.id = (DisplayID)crtc;
        display.bounds.x = crtcInfo->x;
        display.bounds.y = crtcInfo->y;
        display.bounds.w = crtcInfo->width;
        display.bounds.h = crtcInfo->height;
        parse_display_mode(modeInfo, display.mode, crtcInfo->rotation);
        display.mode.bits_per_pixel = bpp;
        display.mode.scaling_factor = (double)display.mode.width / (double)crtcInfo->width;
        display.name = NULL;

        if (crtcInfo->noutput > 0)
        {
            XRROutputInfo *outputInfo = XRRGetOutputInfo(disp, screenResources, crtcInfo->outputs[0]);
            if (outputInfo->name)
            {
                display.name = (char*)malloc(outputInfo->nameLen + 1);
                strcpy(display.name, outputInfo->name);
            }
            XRRFreeOutputInfo(outputInfo);
        }

        displayList.Push(display);

        XRRFreeCrtcInfo(crtcInfo);
    }

    XRRFreeScreenResources(screenResources);
}

void defos_get_display_modes(DisplayID displayID, dmArray<DisplayModeInfo> &modeList)
{
    RRCrtc crtc = (RRCrtc)displayID;

    XRRScreenResources *screenResources = XRRGetScreenResourcesCurrent(disp, win);
    XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, screenResources, crtc);

    if (crtcInfo->noutput <= 0)
    {
        XRRFreeCrtcInfo(crtcInfo);
        XRRFreeScreenResources(screenResources);
        return;
    }

    RROutput output = crtcInfo->outputs[0];
    XRROutputInfo *outputInfo = XRRGetOutputInfo(disp, screenResources, output);

    unsigned long bpp = (long)DefaultDepth(disp, screen);

    const XRRModeInfo *currentModeInfo = get_mode_info(screenResources, crtcInfo->mode);
    double scaling_factor = (double)currentModeInfo->width / (double)(
        axis_flipped(crtcInfo->rotation) ? crtcInfo->height : crtcInfo->width
    );

    modeList.OffsetCapacity(outputInfo->nmode);
    for (int i = 0; i < outputInfo->nmode; i++)
    {
        const XRRModeInfo *modeInfo = get_mode_info(screenResources, outputInfo->modes[i]);

        DisplayModeInfo mode;
        parse_display_mode(modeInfo, mode, crtcInfo->rotation);
        mode.bits_per_pixel = bpp;
        mode.scaling_factor = scaling_factor;

        modeList.Push(mode);
    }

    XRRFreeOutputInfo(outputInfo);
    XRRFreeScreenResources(screenResources);
}

static RRCrtc get_current_crtc(WinRect &bounds)
{
    WinRect viewBounds = defos_get_view_size();
    WinRect bestBounds = { 0, 0, 0, 0 };
    RRCrtc bestCrtc = 0;
    float bestArea = -1.0f;

    XRRScreenResources *screenResources = XRRGetScreenResourcesCurrent(disp, win);
    for (int i = 0; i < screenResources->ncrtc; i++)
    {
        RRCrtc crtc = screenResources->crtcs[i];
        XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, screenResources, crtc);
        WinRect crtcBounds = { (float)crtcInfo->x, (float)crtcInfo->y, (float)crtcInfo->width, (float)crtcInfo->height };
        XRRFreeCrtcInfo(crtcInfo);

        if (!crtcBounds.w || !crtcBounds.h) { continue; }

        WinRect clip = viewBounds;
        if (crtcBounds.x > clip.x)
        {
            clip.w -= crtcBounds.x - clip.x;
            clip.x = crtcBounds.x;
        }
        if (crtcBounds.y > clip.y)
        {
            clip.h -= crtcBounds.y - clip.y;
            clip.y = crtcBounds.y;
        }
        if (crtcBounds.x + crtcBounds.w < clip.x + clip.w)
        {
            clip.w = crtcBounds.x + crtcBounds.w - clip.x;
        }
        if (crtcBounds.y + crtcBounds.h < clip.y + clip.h)
        {
            clip.h = crtcBounds.y + crtcBounds.h - clip.y;
        }

        float area = (clip.w > 0 && clip.h > 0) ? clip.w * clip.h : 0.0f;
        if (area > bestArea)
        {
            bestCrtc = crtc;
            bestBounds = crtcBounds;
            bestArea = area;
        }
    }
    XRRFreeScreenResources(screenResources);

    bounds = bestBounds;
    return bestCrtc;
}

DisplayID defos_get_current_display()
{
    WinRect bounds;
    return (DisplayID)get_current_crtc(bounds);
}

void defos_set_window_icon(const char *icon_path)
{
    dmLogWarning("Method 'set_window_icon' is not supported on Linux");
}

static char* copy_string(const char * s)
{
    char *newString = (char*)malloc(strlen(s) + 1);
    strcpy(newString, s);
    return newString;
}

char* defos_get_bundle_root()
{
    char* result;
    char* path = (char*)malloc(PATH_MAX + 2);
    ssize_t ret = readlink("/proc/self/exe", path, PATH_MAX + 2);
    if (ret >= 0 && ret <= PATH_MAX + 1) {
        path[ret] = '\0';
        result = copy_string(dirname(path));
    } else {
        const char* path2 = (const char*)getauxval(AT_EXECFN);
        if (!path2) {
            result = copy_string(".");
        } else if (!realpath(path2, path)) {
            result = copy_string(".");
        } else {
            result = copy_string(dirname(path));
        }
    }
    free(path);
    return result;
}

static int shared_argc = 0;
static char** shared_argv = NULL;
static void arguments_main_hook(int argc, char* argv[], char* envp[])
{
    shared_argc = argc;
    shared_argv = argv;
}

__attribute__((section(".init_array"), used))
void (* defos_arguments_main_hook)(int,char*[],char*[]) = &arguments_main_hook;


void defos_get_arguments(dmArray<char*> &arguments)
{
    arguments.OffsetCapacity(shared_argc);
    for (int i = 0; i < shared_argc; i++)
    {
        arguments.Push(copy_string(shared_argv[i]));
    }
}

//from glfw/x11_window.c
static void send_message(Window &window, Atom type, long a, long b, long c, long d, long e)
{
    XEvent event;
    memset(&event, 0, sizeof(event));

    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.format = 32;
    event.xclient.message_type = type;
    event.xclient.data.l[0] = a;
    event.xclient.data.l[1] = b;
    event.xclient.data.l[2] = c;
    event.xclient.data.l[3] = d;
    event.xclient.data.l[4] = e;

    XSendEvent(disp, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &event);
}

#endif
