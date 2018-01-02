#pragma once

#include <dmsdk/sdk.h>

struct WinRect {
    float x,y,w,h;
};

extern void defos_init();
extern void defos_final();

extern void defos_disable_maximize_button();
extern void defos_disable_minimize_button();
extern void defos_disable_window_resize();
extern void defos_disable_mouse_cursor();
extern void defos_enable_mouse_cursor();
extern void defos_toggle_fullscreen();
extern void defos_toggle_maximize();

extern bool defos_is_mouse_cursor_within_window();
extern bool defos_enable_subclass_window();
extern void defos_disable_subclass_window();
extern void defos_watch_mouse_state(lua_State* L);

extern void defos_set_window_size(int x, int y, int w, int h);
extern void defos_set_window_title(const char* title_lua);

extern bool defos_is_fullscreen();
extern bool defos_is_maximized();

extern WinRect defos_get_window_size();