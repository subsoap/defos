#pragma once

#include <dmsdk/sdk.h>
#include <math.h>

struct WinRect {
    float x,y,w,h;
};

struct LuaCallbackInfo {
    LuaCallbackInfo() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
    lua_State *m_L;
    int m_Callback;
    int m_Self;
};

typedef enum {
    DEFOS_EVENT_MOUSE_LEAVE,
    DEFOS_EVENT_MOUSE_ENTER,
    DEFOS_EVENT_COUNT
} DefosEvent;

typedef enum {
    DEFOS_CURSOR_ARROW,
    DEFOS_CURSOR_CROSSHAIR,
    DEFOS_CURSOR_HAND,
    DEFOS_CURSOR_IBEAM,
} DefosCursor;

extern LuaCallbackInfo defos_event_handlers[];
extern void defos_emit_event(DefosEvent event);
extern void defos_event_handler_was_set(DefosEvent event);
inline bool defos_event_is_bound(DefosEvent event) {
    return defos_event_handlers[event].m_Callback != LUA_NOREF;
}

extern void defos_init();
extern void defos_final();

extern void defos_disable_maximize_button();
extern void defos_disable_minimize_button();
extern void defos_disable_window_resize();

extern void defos_toggle_fullscreen();
extern void defos_toggle_maximize();
extern bool defos_is_fullscreen();
extern bool defos_is_maximized();

extern void defos_set_window_title(const char* title_lua);

extern void defos_set_window_size(float x, float y, float w, float h);
extern WinRect defos_get_window_size();
extern void defos_set_view_size(float x, float y, float w, float h);
extern WinRect defos_get_view_size();

extern bool defos_is_console_visible();
extern void defos_show_console();
extern void defos_hide_console();

extern void defos_disable_mouse_cursor();
extern void defos_enable_mouse_cursor();

extern bool defos_is_mouse_in_view();
extern void defos_set_cursor_pos(float x, float y);
extern void defos_move_cursor_to(float x, float y);

extern void defos_set_cursor_clipped(bool clipped);
extern bool defos_is_cursor_clipped();
extern void defos_set_cursor_locked(bool locked);
extern bool defos_is_cursor_locked();

extern void defos_set_custom_cursor_html5(const char *url);
extern void defos_set_custom_cursor_win(const char *filename);
extern void defos_set_custom_cursor_mac(dmBuffer::HBuffer buffer, float hotSpotX, float hotSpotY);
extern void defos_set_cursor(DefosCursor cursor);
extern void defos_reset_cursor();
