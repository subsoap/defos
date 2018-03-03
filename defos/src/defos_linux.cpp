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

static Display *disp;
static int screen;
static Window win;
static Window root;
    
//static GC gc;
#define _NET_WM_STATE_REMOVE        0
#define _NET_WM_STATE_ADD           1
#define _NET_WM_STATE_TOGGLE 2
#define XATOM(name) XInternAtom(disp, name, False)

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


bool is_window_visible(Window window)
{
    XWindowAttributes attributes;
    XGetWindowAttributes(disp, window, &attributes);
    return attributes.map_state == IsViewable;
}

//from glfw/x11_window.c
void send_message(Window& window, Atom type, long a, long b, long c, long d,long e)
{
    XEvent event;
    memset(&event, 0, sizeof(event));

    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.format = 32;
    event.xclient.message_type = type;
    event.xclient.data.l[0]=a;
    event.xclient.data.l[1]=b;
    event.xclient.data.l[2]=c;
    event.xclient.data.l[3]=d;
    event.xclient.data.l[4]=e;
    
    XSendEvent(disp, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
}

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
}

void defos_disable_minimize_button()
{
}

void defos_disable_window_resize()
{
}

void defos_set_cursor_visible(bool visible)
{

}

bool defos_is_cursor_visible()
{
    return false;
}

void defos_toggle_fullscreen()
{
    if(!is_fullscreen)
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
    if(!is_maximized)
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
}

bool defos_is_console_visible()
{
    return false;
}

void defos_set_window_size(float x, float y, float w, float h)
{
    // change size only if it is visible
    if(is_window_visible(win))
    {
        if(isnan(x) || isnan(y)){
            XWindowAttributes attributes;
            XGetWindowAttributes(disp, root, &attributes);

            x = ((float)attributes.width - w)/2;
            y = ((float)attributes.height - h)/2;    
        }
        
        XMoveWindow(disp, win, x, y);
        XResizeWindow(disp, win, (unsigned int)w, (unsigned int)h);
        XFlush(disp);
    }
}

void defos_set_view_size(float x, float y, float w, float h)
{
}

void defos_set_window_title(const char *title_lua)
{
    XChangeProperty(disp, win, NET_WM_NAME, UTF8_STRING, 8, PropModeReplace, (unsigned char*)title_lua, strlen(title_lua));
    XFlush(disp); // IMPORTANT: we have to flush, or nothing will be changed
}

WinRect defos_get_window_size()
{
    XWindowAttributes attributes;
    XGetWindowAttributes(disp, win, &attributes);

    Window dummy;
    int x, y;
    XTranslateCoordinates(disp, win,root, 0, 0, &x, &y, &dummy);

    WinRect r = {(float)x, (float)y, (float)attributes.width, (float)attributes.height};
    return r;
}

WinRect defos_get_view_size()
{
    WinRect r;
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
   if(ix > rect.w) ix = rect.w;
   if(ix < 0) ix = 0;
   if(iy > rect.h) iy=rect.h;
   if(iy < 0) iy = 0;


   XWarpPointer(disp, None, win, 0, 0, 0, 0, ix, iy);
   XFlush(disp);
}

void defos_set_cursor_clipped(bool clipped)
{
 
}

bool defos_is_cursor_clipped()
{
    return false;
}

void defos_set_cursor_locked(bool locked)
{
 
}

bool defos_is_cursor_locked()
{
    return false;
}

void defos_update() {

}

void defos_set_custom_cursor(const char *filename)
{
    // TODO: x11 support .xbm image for cursor
}

void defos_set_cursor(DefosCursor cursor)
{

}

void defos_reset_cursor()
{

}

#endif