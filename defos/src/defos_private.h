#pragma once

#include <dmsdk/sdk.h>
#include <math.h>

struct WinRect {
    float x, y, w, h;
};

struct WinPoint {
    float x, y;
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
    DEFOS_EVENT_CLICK,
    DEFOS_EVENT_CURSOR_LOCK_DISABLED,
    DEFOS_EVENT_COUNT
} DefosEvent;

typedef enum {
    DEFOS_CURSOR_ARROW = 0,
    DEFOS_CURSOR_CROSSHAIR,
    DEFOS_CURSOR_HAND,
    DEFOS_CURSOR_IBEAM,
    DEFOS_CURSOR_INTMAX,
} DefosCursor;

#ifdef DM_PLATFORM_WINDOWS
typedef const char* DisplayID;
#else
typedef void* DisplayID;
#endif

struct DisplayModeInfo {
    unsigned long width;
    unsigned long height;
    unsigned long bits_per_pixel;
    double refresh_rate;
    double scaling_factor;
    unsigned long orientation;
    bool reflect_x;
    bool reflect_y;
};

struct DisplayInfo {
    DisplayID id;
    struct WinRect bounds;
    struct DisplayModeInfo mode;
    char * name;
};

extern LuaCallbackInfo defos_event_handlers[];
extern void defos_emit_event(DefosEvent event);
extern void defos_event_handler_was_set(DefosEvent event);
inline bool defos_event_is_bound(DefosEvent event) {
    return defos_event_handlers[event].m_Callback != LUA_NOREF;
}

extern void defos_init();
extern void defos_final();
extern void defos_update();

extern void defos_disable_maximize_button();
extern void defos_disable_minimize_button();
extern void defos_disable_window_resize();

extern void defos_toggle_fullscreen();
extern void defos_toggle_maximized();
extern void defos_toggle_always_on_top();
extern bool defos_is_fullscreen();
extern bool defos_is_maximized();
extern bool defos_is_always_on_top();
extern void defos_minimize();
extern void defos_activate();

extern void defos_set_window_title(const char* title_lua);
extern void  defos_set_window_icon(const char *icon_path);
extern char* defos_get_bundle_root();
extern void defos_get_arguments(dmArray<char*> &arguments);

extern void defos_set_window_size(float x, float y, float w, float h);
extern WinRect defos_get_window_size();
extern void defos_set_view_size(float x, float y, float w, float h);
extern WinRect defos_get_view_size();

extern bool defos_is_console_visible();
extern void defos_set_console_visible(bool visible);

extern void defos_set_cursor_visible(bool visible);
extern bool defos_is_cursor_visible();

extern bool defos_is_mouse_in_view();
extern void defos_set_cursor_pos(float x, float y);
extern void defos_set_cursor_pos_view(float x, float y);
extern WinPoint defos_get_cursor_pos();
extern WinPoint defos_get_cursor_pos_view();

extern void defos_set_cursor_clipped(bool clipped);
extern bool defos_is_cursor_clipped();
extern void defos_set_cursor_locked(bool locked);
extern bool defos_is_cursor_locked();

extern void *defos_load_cursor_html5(const char *url);
extern void *defos_load_cursor_win(const char *filename);
extern void *defos_load_cursor_mac(dmBuffer::HBuffer buffer, float hotSpotX, float hotSpotY);
extern void *defos_load_cursor_linux(const char *filename);
extern void defos_gc_custom_cursor(void *cursor);
extern void defos_set_custom_cursor(void *cursor);
extern void defos_set_cursor(DefosCursor cursor);
extern void defos_reset_cursor();

extern void defos_get_displays(dmArray<DisplayInfo> &displayList);
extern void defos_get_display_modes(DisplayID displayID, dmArray<DisplayModeInfo> &modeList);
extern DisplayID defos_get_current_display();
