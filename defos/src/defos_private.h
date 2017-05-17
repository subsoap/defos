#pragma once

#include <dmsdk/sdk.h>

extern void defos_disable_maximize_button();
extern void defos_disable_minimize_button();
extern void defos_disable_window_resize();
extern void defos_disable_mouse_cursor();
extern void defos_enable_mouse_cursor();
extern void defos_toggle_fullscreen();
extern void defos_toggle_maximize();

extern void defos_set_window_size(lua_State* L);
extern void defos_set_window_title(lua_State* L);

extern bool defos_is_fullscreen();
extern bool defos_is_maximized();

void init_window();
