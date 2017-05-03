#pragma once

#include <dmsdk/sdk.h>

extern void defos_disable_maximize_button();
extern void defos_disable_minimize_button();
extern void defos_disable_window_resize();
extern void defos_disable_mouse_cursor();
extern void defos_enable_mouse_cursor();
extern void defos_set_window_size(lua_State* L);

void init_window();
