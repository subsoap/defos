#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_WINDOWS)

HWND* window;

void defos_disable_maximize_button() {

}

void defos_disable_minimize_button() {

}

void defos_disable_window_resize() {

}

void defos_disable_mouse_cursor() {
	ShowCursor(0);
}

void defos_enable_mouse_cursor() {
	ShowCursor(1);
}

void defos_toggle_fullscreen() {

}

void defos_toggle_maximize() {

}

void defos_set_window_size(lua_State* L) {
	int x = luaL_checkint(L, 1);
	int y = luaL_checkint(L, 2);
	int w = luaL_checkint(L, 3);
	int h = luaL_checkint(L, 4);
}

void defos_set_window_title(lua_State* L) {
	const char* title_lua = luaL_checkstring(L, 1);
}

bool defos_is_fullscreen() {

}

bool defos_is_maximized() {

}

#endif
