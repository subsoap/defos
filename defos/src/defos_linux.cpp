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
static GC gc;



void defos_init()
{
    disp = XOpenDisplay(NULL);
    screen = DefaultScreen(disp);
    win = dmGraphics::GetNativeX11Window();
}

void defos_final()
{

}

void defos_event_handler_was_set(DefosEvent event)
{

}

bool defos_is_fullscreen()
{
    return false;
}

bool defos_is_maximized()
{
    return false;
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

}

void defos_toggle_maximized()
{

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
}

void defos_set_view_size(float x, float y, float w, float h)
{
}

void defos_set_window_title(const char *title_lua)
{
    Atom utf8atom = XInternAtom(disp, "UTF8_STRING", False);

    int ret = XChangeProperty(disp, win, XInternAtom(disp,"_NET_WM_NAME",False), utf8atom, 8, PropModeReplace, (unsigned char*)title_lua, strlen(title_lua));
    
    XFlush(disp); // IMPORTANT: we have to flush, or nothing will be changed
}

WinRect defos_get_window_size()
{
    XWindowAttributes attributes;
    XGetWindowAttributes(disp, win, &attributes);

    Window dummy;
    Window root = XDefaultRootWindow(disp);
    int x, y;
    XTranslateCoordinates(disp, win,root, 0, 0, &x, &y, &dummy);

    WinRect r = {
        x,
        y,
        attributes.width,
        attributes.height
    };
    return r;
}

WinRect defos_get_view_size()
{
    WinRect r;
    return r;
}

void defos_set_cursor_pos(float x, float y)
{
}

void defos_move_cursor_to(float x, float y)
{
  
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