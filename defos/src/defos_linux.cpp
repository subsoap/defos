#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_LINUX)

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
    int x, y;
    unsigned int w, h, bw, depth;

    Window dummy;
    XGetGeometry(disp, win, &dummy, &x, &y, &w, &h, &bw, &depth);

    lock_resize(w, h);
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
