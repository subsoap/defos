#include <dmsdk/sdk.h>
//#define DM_PLATFORM_LINUX 0
#if defined(DM_PLATFORM_LINUX)

#include "defos_private.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>


void defos_init()
{

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

// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/
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
}

WinRect defos_get_window_size()
{
    WinRect r;
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

// move cursor to pos relative to current window
// top-left is (0, 0)
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

// path of the cursor file,
// for defold it may be a good idea to save the cursor file to the save folder,
// then pass the path to this function to load
void defos_set_custom_cursor_win(const char *filename)
{

}

void defos_set_cursor(DefosCursor cursor)
{

}

void defos_reset_cursor()
{

}

#endif