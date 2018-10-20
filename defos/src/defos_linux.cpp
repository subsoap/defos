#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_LINUX)

/*
    some resources to manage window for x11.

    1. https://tronche.com/gui/x/xlib/
    2. https://github.com/glfw/glfw/blob/master/src/x11_window.c
    3. https://github.com/yetanothergeek/xctrl/blob/master/src/xctrl.c
*/

#include "defos_private.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
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
static Atom NET_FRAME_EXTENTS;

static Cursor custom_cursor; // image cursor
static bool has_custom_cursor = false;

static bool is_window_visible(Window window);
static void send_message(Window &window, Atom type, long a, long b, long c, long d, long e);

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
    NET_FRAME_EXTENTS = XATOM("_NET_FRAME_EXTENTS");
}

void defos_final()
{
    if (has_custom_cursor)
    {
        XFreeCursor(disp, custom_cursor);
        has_custom_cursor = false;
    }
}

void defos_event_handler_was_set(DefosEvent event)
{
}

static bool hint_state_contains_atom(Atom atom)
{
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    Atom* data = NULL;
    XEvent event;
    while (XGetWindowProperty(disp, win, NET_WM_STATE,
        0, (~0L), False, AnyPropertyType,
        &actualType, &actualFormat,
        &nItems, &bytesAfter, (unsigned char**)&data) == Success && bytesAfter != 0
    ) {
        XNextEvent(disp, &event);
    }

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

bool defos_is_mouse_in_view()
{
    Window d1, d2;
    int x, y, d3, d4;
    unsigned int d5;
    if (!XQueryPointer(disp, win, &d1, &d2, &d3, &d4, &x, &y, &d5)) { return false; }

    if (x < 0 || y < 0) { return false; }

    unsigned int w, h, d6;
    XGetGeometry(disp, win, &d1, &d3, &d4, &w, &h, &d5, &d6);

    if (x >= w || y >= h) { return false; }
    return true;
}

void defos_disable_maximize_button()
{
    dmLogInfo("Method 'defos_disable_maximize_button' is not supported in Linux");
}

void defos_disable_minimize_button()
{
    dmLogInfo("Method 'defos_disable_minimize_button' is not supported in Linux");
}

void defos_disable_window_resize()
{
    dmLogInfo("Method 'defos_disable_window_resize' is not supported in Linux");
}

void defos_set_cursor_visible(bool visible)
{
    dmLogInfo("Method 'defos_set_cursor_visible' is not supported in Linux");
}

bool defos_is_cursor_visible()
{
    return false;
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

void defos_set_console_visible(bool visible)
{
    dmLogInfo("Method 'defos_set_console_visible' is not supported in Linux");
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
            x = screenBounds.x + ((float)screenBounds.w - w) / 2;
            y = screenBounds.y + ((float)screenBounds.h - h) / 2;
        }

        WindowExtents extents = get_window_extents();
        w -= extents.left + extents.right;
        h -= extents.top + extents.bottom;
        x += extents.left;
        y += extents.top;

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
            x = screenBounds.x + ((float)screenBounds.w - w) / 2;
            y = screenBounds.y + ((float)screenBounds.h - h) / 2;
        }

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

void defos_set_cursor_pos(float x, float y)
{
    XWarpPointer(disp, None, root, 0, 0, 0, 0, (int)x, (int)y);
    XFlush(disp);
}

void defos_move_cursor_to(float x, float y)
{
    XWarpPointer(disp, None, win, 0, 0, 0, 0, (int)x, (int)y);
    XFlush(disp);
}

void defos_set_cursor_clipped(bool clipped)
{
    dmLogInfo("Method 'defos_set_cursor_clipped' is not supported in Linux");
}

bool defos_is_cursor_clipped()
{
    return false;
}

void defos_set_cursor_locked(bool locked)
{
    dmLogInfo("Method 'defos_set_cursor_locked' is not supported in Linux");
}

bool defos_is_cursor_locked()
{
    return false;
}

void defos_update()
{
}

void defos_set_custom_cursor_linux(const char *filename)
{
    Cursor cursor = XcursorFilenameLoadCursor(disp, filename);
    XDefineCursor(disp, win, cursor);
    if (has_custom_cursor) { XFreeCursor(disp, custom_cursor); }
    custom_cursor = cursor;
    has_custom_cursor = true;
}

static unsigned int get_cursor(DefosCursor cursor);

void defos_set_cursor(DefosCursor cursor_type)
{
    Cursor cursor = XCreateFontCursor(disp, get_cursor(cursor_type));
    XDefineCursor(disp, win, cursor);
    if (has_custom_cursor) { XFreeCursor(disp, custom_cursor); }
    custom_cursor = cursor;
    has_custom_cursor = true;
}

void defos_reset_cursor()
{
    if (has_custom_cursor)
    {
        XUndefineCursor(disp, win);
        XFreeCursor(disp, custom_cursor);
        has_custom_cursor = false;
    }
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
        display.mode.width = modeInfo->width;
        display.mode.height = modeInfo->height;
        display.mode.refresh_rate = compute_refresh_rate(modeInfo);
        display.mode.bits_per_pixel = bpp;
        display.mode.scaling_factor = (double)display.mode.width / (double)(
            axis_flipped(crtcInfo->rotation) ? crtcInfo->height : crtcInfo->width
        );
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
        mode.width = modeInfo->width;
        mode.height = modeInfo->height;
        mode.refresh_rate = compute_refresh_rate(modeInfo);
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
    dmLogInfo("Method 'defos_set_window_icon' is not supported on Linux");
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

void defos_get_parameters(dmArray<char*> &parameters)
{
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
