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
    DEFOS_EVENT_INTERACTION,
    DEFOS_EVENT_COUNT
} DefosEvent;

typedef enum {
    DEFOS_CURSOR_ARROW = 0,
    DEFOS_CURSOR_CROSSHAIR,
    DEFOS_CURSOR_HAND,
    DEFOS_CURSOR_IBEAM,
    DEFOS_CURSOR_INTMAX,
} DefosCursor;

// Display modes of the game window. toggle_fullscreen only knows two states: "borderless over the
// whole monitor" and "restore the geometry from startup". On Windows that startup geometry is often
// broken (the engine creates the window as display.width * display scale, which may not fit the
// screen), and there was no "maximized to the work area, taskbar visible" state at all. Hence an
// explicit mode instead of a toggle.
typedef enum {
    DEFOS_DISPLAY_MODE_WINDOWED = 0,        // regular window with frame and title bar
    DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED, // covers the work area (taskbar / Dock stay visible)
    DEFOS_DISPLAY_MODE_FULLSCREEN,          // covers the whole monitor, above system panels
} DefosDisplayMode;

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
extern void defos_toggle_borderless();
extern void defos_toggle_maximized();
extern void defos_toggle_always_on_top();
extern bool defos_is_fullscreen();
extern bool defos_is_borderless();

// width/height is the requested CLIENT area size for the windowed mode in platform units
// (physical pixels on Windows, points on macOS); NAN means "pick a size" (80% of the work area).
// Other modes ignore the size.
extern void defos_set_display_mode(DefosDisplayMode mode, float width, float height);
extern DefosDisplayMode defos_get_display_mode();
// Not every mode is available everywhere: on Linux window states are performed by the window
// manager, and a mode whose EWMH state it does not support should be hidden, not offered broken.
extern bool defos_is_display_mode_supported(DefosDisplayMode mode);
// Work area of the monitor the window is on: monitor minus taskbar (Windows), minus menu bar and
// Dock (macOS), minus desktop panels (Linux).
extern WinRect defos_get_work_area();
// Whether the user may resize the window by dragging its edge.
extern void defos_set_window_resizable(bool resizable);
extern bool defos_is_window_resizable();
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
