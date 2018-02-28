#define DLIB_LOG_DOMAIN "defos"
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_HTML5)

#include "defos_private.h"
#include <emscripten.h>
#include <stdlib.h>

static const char * current_cursor = "default";
static bool current_cursor_needs_free = false;
static bool is_maximized = false;
static bool is_mouse_inside = false;
static bool is_cursor_visible = true;
static bool is_cursor_locked = false;
static WinRect previous_state;

void js_warn(const char* msg) {
    EM_ASM_({console.warn(UTF8ToString($0))}, msg);
}

extern "C" void EMSCRIPTEN_KEEPALIVE defos_emit_event_from_js(DefosEvent event) {
    switch (event) {
        case DEFOS_EVENT_MOUSE_ENTER:
            is_mouse_inside = true;
            break;
        case DEFOS_EVENT_MOUSE_LEAVE:
            is_mouse_inside = false;
            break;
        case DEFOS_EVENT_CURSOR_LOCK_DISABLED:
            if (!is_cursor_locked) { return; }
            is_cursor_locked = false;
            break;
        default: {}
    }
    defos_emit_event(event);
}

void defos_init() {
    current_cursor = "default";
    current_cursor_needs_free = false;
    EM_ASM_({
        Module.__defosjs_mouseenter_listener = function () {
            _defos_emit_event_from_js($0);
        };
        Module.__defosjs_mouseleave_listener = function () {
            _defos_emit_event_from_js($1);
        };
        Module.__defosjs_click_listener = function () {
            _defos_emit_event_from_js($2);
        };
        Module.canvas.addEventListener('mouseenter', Module.__defosjs_mouseenter_listener);
        Module.canvas.addEventListener('mouseleave', Module.__defosjs_mouseleave_listener);
        Module.canvas.addEventListener('click', Module.__defosjs_click_listener);
    }, DEFOS_EVENT_MOUSE_ENTER, DEFOS_EVENT_MOUSE_LEAVE, DEFOS_EVENT_CLICK);

    EM_ASM_({
        Module.__defosjs_pointerlockchange_listener = function () {
            if ((
                document.pointerLockElement ||
                document.mozPointerLockElement ||
                document.webkitPointerLockElement ||
                document.msPointerLockElement
            ) !== Module.canvas) {
                _defos_emit_event_from_js($0);
            }
        };
        if ('onpointerlockchange' in document) {
            document.addEventListener('pointerlockchange', Module.__defosjs_pointerlockchange_listener, false);
        } else if ('onmozpointerlockchange' in document) {
            document.addEventListener('mozpointerlockchange', Module.__defosjs_pointerlockchange_listener, false);
        } else if ('onwebkitpointerlockchange' in document) {
            document.addEventListener('webkitpointerlockchange', Module.__defosjs_pointerlockchange_listener, false);
        } else if ('onmspointerlockchange' in document) {
            document.addEventListener('mspointerlockchange', Module.__defosjs_pointerlockchange_listener, false);
        }
    }, DEFOS_EVENT_CURSOR_LOCK_DISABLED);
}

void defos_final() {
    if (current_cursor_needs_free) {
        current_cursor_needs_free = false;
        free((void*)current_cursor);
    }
    EM_ASM(
        Module.canvas.removeEventListener('mouseenter', Module.__defosjs_mouseenter_listener);
        Module.canvas.removeEventListener('mouseleave', Module.__defosjs_mouseleave_listener);
        Module.canvas.removeEventListener('click', Module.__defosjs_click_listener);
        document.removeEventListener('pointerlockchange', Module.__defosjs_pointerlockchange_listener);
        document.removeEventListener('mozpointerlockchange', Module.__defosjs_pointerlockchange_listener);
        document.removeEventListener('webkitpointerlockchange', Module.__defosjs_pointerlockchange_listener);
        document.removeEventListener('mspointerlockchange', Module.__defosjs_pointerlockchange_listener);
    );
}

void defos_update() {
}

void defos_event_handler_was_set(DefosEvent event) {
}

void defos_disable_maximize_button() {
    dmLogInfo("Method 'disable_maximize_button' is not supported in html5");
}

void defos_disable_minimize_button() {
    dmLogInfo("Method 'disable_minimize_button' is not supported in html5");
}

void defos_disable_window_resize() {
    dmLogInfo("Method 'disable_window_resize' is not supported in html5");
}

void defos_toggle_fullscreen() {
    EM_ASM(Module.toggleFullscreen(););
}

void defos_toggle_maximized() {
    if (is_maximized) {
        is_maximized = false;
        defos_set_window_size(0,0, previous_state.w, previous_state.h);
    } else {
        previous_state = defos_get_window_size();
        is_maximized = true;
        EM_ASM({
            Module.canvas.width = window.innerWidth;
            Module.canvas.height = window.innerHeight;
        });
    }
}

bool defos_is_fullscreen() {
    int is_fullscreen = EM_ASM_INT({
        return GLFW.isFullscreen;
    },0);
    return is_fullscreen != 0;
}

bool defos_is_maximized() {
    return is_maximized;
}

void defos_set_window_title(const char* title_lua) {
    EM_ASM_({document.title = UTF8ToString($0)}, title_lua);
}

void defos_set_window_icon(const char *icon_path)
{
    EM_ASM_({
        document.head || (document.head = document.getElementsByTagName('head')[0]);
        function changeFavicon(src) {
            var link = document.createElement('link');
            var oldLink = document.getElementById('dynamic-favicon');
            link.id = 'dynamic-favicon';
            link.rel = 'shortcut icon';
            link.href = src;
            if (oldLink) {
                document.head.removeChild(oldLink);
            }
            document.head.appendChild(link);
        }
        changeFavicon(UTF8ToString($0));
    }, icon_path);
}

void defos_set_window_size(float x, float y, float w, float h) {
    defos_set_view_size(x, y, w, h);
}

void defos_set_view_size(float x, float y, float w, float h) {
    EM_ASM_({
        Module.canvas.width = $0;
        Module.canvas.height = $1;
    }, w, h);
}

extern WinRect defos_get_window_size() {
    return defos_get_view_size();
}

WinRect defos_get_view_size() {
    WinRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = EM_ASM_INT({
        return Module.canvas.width;
    }, 0);
    rect.h = EM_ASM_INT({
        return Module.canvas.height;
    }, 0);
    return rect;
}

bool defos_is_console_visible() {
    return false;
}

void defos_set_console_visible(bool visible) {
    dmLogInfo("Method 'defos_set_console_visible' is not supported in html5, it is meant for Windows builds only");
}

void defos_set_cursor_visible(bool visible) {
    if (is_cursor_visible == visible) { return; }
    is_cursor_visible = visible;
    if (visible) {
        EM_ASM_({Module.canvas.style.cursor = UTF8ToString($0);}, current_cursor);
    } else {
        EM_ASM(Module.canvas.style.cursor = 'none';);
    }
}

bool defos_is_cursor_visible() {
    return is_cursor_visible;
}

bool defos_is_mouse_in_view() {
    return is_mouse_inside;
}

void defos_set_cursor_pos(float x, float y) {
    dmLogInfo("Method 'defos_set_cursor_pos' is not supported in html5");
}

void defos_move_cursor_to(float x, float y) {
    dmLogInfo("Method 'defos_move_cursor_to' is not supported in html5");
}

void defos_set_cursor_clipped(bool clipped) {
    dmLogInfo("Method 'defos_set_cursor_clipped' is not supported in html5");
}

bool defos_is_cursor_clipped() {
    return false;
}

extern void defos_set_cursor_locked(bool locked) {
    is_cursor_locked = locked;
    if (locked) {
        EM_ASM(
            (Module.canvas.requestPointerLock ||
            Module.canvas.mozRequestPointerLock ||
            Module.canvas.webkitRequestPointerLock ||
            Module.canvas.msRequestPointerLock ||
            function () {}).call(Module.canvas);
        );
    } else {
        EM_ASM(
            (document.exitPointerLock ||
            document.mozExitPointerLock ||
            document.webkitExitPointerLock ||
            document.msExitPointerLock ||
            function () {}).call(document);
        );
    }
}

bool defos_is_cursor_locked() {
    return is_cursor_locked;
}

static const char * get_cursor(DefosCursor cursor) {
    switch (cursor) {
        case DEFOS_CURSOR_ARROW:
            return "default";
        case DEFOS_CURSOR_HAND:
            return "pointer";
        case DEFOS_CURSOR_CROSSHAIR:
            return "crosshair";
        case DEFOS_CURSOR_IBEAM:
            return "text";
        default:
            return "default";
    }
}

void defos_set_cursor(DefosCursor cursor) {
    if (current_cursor_needs_free) {
        free((void*)current_cursor);
    }
    current_cursor = get_cursor(cursor);
    current_cursor_needs_free = false;

    if (is_cursor_visible) {
        EM_ASM_({Module.canvas.style.cursor = UTF8ToString($0);}, current_cursor);
    }
}

extern void defos_set_custom_cursor_html5(const char *url) {
    size_t len = strlen(url);
    char * buffer = (char*)malloc(len + 12);
    strcpy(buffer, "url(");
    strcpy(buffer + 4, url);
    strcpy(buffer + 4 + len, "), auto");

    if (current_cursor_needs_free) {
        free((void*)current_cursor);
    }
    current_cursor = buffer;
    current_cursor_needs_free = true;

    if (is_cursor_visible) {
        EM_ASM_({Module.canvas.style.cursor = UTF8ToString($0);}, current_cursor);
    }
}

void defos_reset_cursor() {
    if (current_cursor_needs_free) {
        free((void*)current_cursor);
    }
    current_cursor = "default";
    current_cursor_needs_free = false;

    if (is_cursor_visible) {
        EM_ASM(Module.canvas.style.cursor = 'default';);
    }
}

#endif
