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
static Atom NET_WM_ACTION_MINIMIZE;

// TODO: should query state from system
static bool is_maximized = false;
static bool is_fullscreen = false;

static Cursor custom_cursor; // image cursor

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
}

void defos_final()
{
    if (custom_cursor == NULL)
    {
        XFreeCursor(disp, custom_cursor);
    }
}

void defos_event_handler_was_set(DefosEvent event)
{
}

bool defos_is_fullscreen()
{
    return is_fullscreen;
}

bool defos_is_maximized()
{
    return is_maximized;
}

bool defos_is_mouse_in_view()
{
    return false;
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
    if (!is_fullscreen)
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_ADD,
                     NET_WM_STATE_FULLSCREEN,
                     0,
                     1,
                     0);
        ;
    }
    else
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_REMOVE,
                     NET_WM_STATE_FULLSCREEN,
                     0,
                     1,
                     0);
    }

    is_fullscreen = !is_fullscreen;
    XFlush(disp);
}

void defos_toggle_maximized()
{
    if (!is_maximized)
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_ADD,
                     NET_WM_STATE_MAXIMIZED_VERT,
                     NET_WM_STATE_MAXIMIZED_HORZ,
                     1,
                     0);
    }
    else
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_REMOVE,
                     NET_WM_STATE_MAXIMIZED_VERT,
                     NET_WM_STATE_MAXIMIZED_HORZ,
                     1,
                     0);
    }

    is_maximized = !is_maximized;
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

void defos_set_window_size(float x, float y, float w, float h)
{
    // change size only if it is visible
    if (is_window_visible(win))
    {
        if (isnan(x) || isnan(y))
        {
            XWindowAttributes attributes;
            XGetWindowAttributes(disp, root, &attributes);

            x = ((float)attributes.width - w) / 2;
            y = ((float)attributes.height - h) / 2;
        }

        XMoveResizeWindow(disp, win, (int)x, (int)y, (unsigned int)w, (unsigned int)h);
        XFlush(disp);
    }
}

void defos_set_view_size(float x, float y, float w, float h)
{
    XWindowChanges changes;
    changes.x = (int)x;
    changes.y = (int)y;
    changes.width = (int)w;
    changes.height = (int)h;

    XConfigureWindow(disp, win, CWX | CWY | CWWidth | CWHeight, &changes);
    XFlush(disp);
}

void defos_set_window_title(const char *title_lua)
{
    XChangeProperty(disp, win, NET_WM_NAME, UTF8_STRING, 8, PropModeReplace, (unsigned char *)title_lua, strlen(title_lua));
    XFlush(disp); // IMPORTANT: we have to flush, or nothing will be changed
}

WinRect defos_get_window_size()
{
    WinRect r = {0.0f, 0.0f, 0.0f, 0.0f};
    return r;
}

WinRect defos_get_view_size()
{
    int x, y;
    unsigned int w, h, bw, depth;

    Window dummy;
    XGetGeometry(disp, win, &dummy, &x, &y, &w, &h, &bw, &depth);
    XTranslateCoordinates(disp, win, root, x, y, &x, &y, &dummy);

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
    WinRect rect = defos_get_window_size();

    int ix = (int)x;
    int iy = (int)y;

    // TODO: need this?
    if (ix > rect.w)
        ix = rect.w;
    if (ix < 0)
        ix = 0;
    if (iy > rect.h)
        iy = rect.h;
    if (iy < 0)
        iy = 0;

    XWarpPointer(disp, None, win, 0, 0, 0, 0, ix, iy);
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
    custom_cursor = XcursorFilenameLoadCursor(disp, filename);
    XDefineCursor(disp, win, custom_cursor);
}

static unsigned int get_cursor(DefosCursor cursor);

void defos_set_cursor(DefosCursor cursor)
{
    // TODO: X11 support change the cursor color, add it later
    defos_reset_cursor();
    custom_cursor = XCreateFontCursor(disp, get_cursor(cursor));
    XDefineCursor(disp, win, custom_cursor);
}

void defos_reset_cursor()
{
    XUndefineCursor(disp, win);

    if (custom_cursor != NULL)
    {
        XFreeCursor(disp, custom_cursor);
        custom_cursor = NULL;
    }
}

static const XRRModeInfo* getModeInfo(const XRRScreenResources* sr, RRMode id){
    for(int i=0;i<sr->nmode;i++){
        if(sr->modes[i].id == id){
            return sr->modes+i;
        }
    }

    return NULL;
}

static long calculateRefreshRate(const XRRModeInfo* mi)
{
    if (!mi->hTotal || !mi->vTotal)
        return 0;

    return (long) ((double) mi->dotClock / ((double) mi->hTotal * (double) mi->vTotal));
}

// NOTE: seems like this function only can query those that with 60 fraquency
void defos_get_displays(dmArray<DisplayInfo> *displist)
{
    RROutput output = XRRGetOutputPrimary(disp, win);

    XRRScreenResources *res = XRRGetScreenResourcesCurrent(disp, win);
    XRROutputInfo *oi= XRRGetOutputInfo(disp, res, output);
    long bpp = (long)DefaultDepth(disp, screen);

    for(int i=0;i<oi->nmode;i++){
        const XRRModeInfo *mi = getModeInfo(res, oi->modes[i]);

        DisplayInfo info;
        // TODO: add rotation detect
        info.w = (long)mi->width;
        info.h= (long)mi->height;
        info.frequency = calculateRefreshRate(mi);
        info.bitsPerPixel = bpp;

        displist->OffsetCapacity(1);
        displist->Push(info);
    }

    XRRFreeOutputInfo(oi);
    XRRFreeScreenResources(res);
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