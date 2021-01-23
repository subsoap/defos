
#if defined(DM_PLATFORM_HTML5) && !defined(DM_HEADLESS)

#include "defos_private.h"
#include <dmsdk/sdk.h>
#include <emscripten.h>
#include <stdlib.h>

struct CustomCursor {
    int ref_count;
    char url[];
};

static const char * current_cursor = "default";
static CustomCursor * current_custom_cursor = NULL;
static bool is_maximized = false;
static bool is_mouse_inside = false;
static bool is_cursor_visible = true;
static bool is_cursor_locked = false;
static WinRect previous_state;

void js_warn(const char* msg) {
    EM_ASM({console.warn(UTF8ToString($0))}, msg);
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
    current_custom_cursor = NULL;
    EM_ASM({
        Module.__defosjs_mouseenter_listener = function () {
            _defos_emit_event_from_js($0);
        };
        Module.__defosjs_mouseleave_listener = function () {
            _defos_emit_event_from_js($1);
        };
        Module.__defosjs_click_listener = function () {
            _defos_emit_event_from_js($2);
        };
        Module.__defosjs_interaction_listener = function () {
            _defos_emit_event_from_js($3);
        };
        Module.__defosjs_mousemove_listener = function (evt) {
            var rect = Module.canvas.getBoundingClientRect();
            Module.__defosjs_mouse_x = evt.clientX - rect.left;
            Module.__defosjs_mouse_y = evt.clientY - rect.top;
        };
        Module.__defosjs_mouse_x = -1;
        Module.__defosjs_mouse_y = -1;
        Module.canvas.addEventListener('mouseenter', Module.__defosjs_mouseenter_listener);
        Module.canvas.addEventListener('mouseleave', Module.__defosjs_mouseleave_listener);
        Module.canvas.addEventListener('click', Module.__defosjs_click_listener);
        Module.canvas.addEventListener('click', Module.__defosjs_interaction_listener);
        Module.canvas.addEventListener('keyup', Module.__defosjs_interaction_listener);
        Module.canvas.addEventListener('touchend', Module.__defosjs_interaction_listener);
        document.addEventListener('mousemove', Module.__defosjs_mousemove_listener);
    }, DEFOS_EVENT_MOUSE_ENTER, DEFOS_EVENT_MOUSE_LEAVE, DEFOS_EVENT_CLICK, DEFOS_EVENT_INTERACTION);

    EM_ASM({
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
    if (current_custom_cursor) {
        defos_gc_custom_cursor(current_custom_cursor);
        current_custom_cursor = NULL;
    }
    EM_ASM({
        Module.canvas.removeEventListener('mouseenter', Module.__defosjs_mouseenter_listener);
        Module.canvas.removeEventListener('mouseleave', Module.__defosjs_mouseleave_listener);
        Module.canvas.removeEventListener('click', Module.__defosjs_click_listener);
        Module.canvas.removeEventListener('click', Module.__defosjs_interaction_listener);
        Module.canvas.removeEventListener('keyup', Module.__defosjs_interaction_listener);
        Module.canvas.removeEventListener('touchend', Module.__defosjs_interaction_listener);
        document.removeEventListener('mousemove', Module.__defosjs_mousemove_listener);
        document.removeEventListener('pointerlockchange', Module.__defosjs_pointerlockchange_listener);
        document.removeEventListener('mozpointerlockchange', Module.__defosjs_pointerlockchange_listener);
        document.removeEventListener('webkitpointerlockchange', Module.__defosjs_pointerlockchange_listener);
        document.removeEventListener('mspointerlockchange', Module.__defosjs_pointerlockchange_listener);
    });
}

void defos_update() {
}

void defos_event_handler_was_set(DefosEvent event) {
}

void defos_disable_maximize_button() {
    dmLogWarning("Method 'disable_maximize_button' is not supported in HTML5");
}

void defos_disable_minimize_button() {
    dmLogWarning("Method 'disable_minimize_button' is not supported in HTML5");
}

void defos_disable_window_resize() {
    dmLogWarning("Method 'disable_window_resize' is not supported in HTML5");
}

void defos_minimize() {
    dmLogWarning("Method 'minimize' is not supported in HTML5");
}

void defos_activate() {
    dmLogWarning("Method 'activate' is not supported in HTML5");
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
    });
    return is_fullscreen != 0;
}

bool defos_is_maximized() {
    return is_maximized;
}

void defos_toggle_always_on_top() {
    dmLogWarning("Method 'toggle_always_on_top' is not supported in HTML5");
}

bool defos_is_always_on_top() {
    return false;
}

void defos_set_window_title(const char* title_lua) {
    EM_ASM({document.title = UTF8ToString($0)}, title_lua);
}

void defos_set_window_icon(const char *icon_path)
{
    EM_ASM({
        function changeFavicon(src) {
            var oldLink = document.querySelector("link[rel*='icon']");
            if (oldLink) { document.head.removeChild(oldLink); }
            var link = document.createElement('link');
            link.rel = 'shortcut icon';
            link.href = src;
            document.head.appendChild(link);
        }
        changeFavicon(UTF8ToString($0));
    }, icon_path);
}

char* defos_get_bundle_root() {
    char *bundlePath = (char*)EM_ASM_INT({
        var jsString = location.href.substring(0, location.href.lastIndexOf("/"));
        var lengthBytes = lengthBytesUTF8(jsString)+1; // 'jsString.length' would return the length of the string as UTF-16 units, but Emscripten C strings operate as UTF-8.
        var stringOnWasmHeap = _malloc(lengthBytes);
        stringToUTF8(jsString, stringOnWasmHeap, lengthBytes+1);
        return stringOnWasmHeap;
    });
    return bundlePath;
}

void defos_get_arguments(dmArray<char*> &arguments) {
    char *param = (char*)EM_ASM_INT({
        var jsString = window.location.search;
        var lengthBytes = lengthBytesUTF8(jsString) + 1;
        var stringOnWasmHeap = _malloc(lengthBytes);
        stringToUTF8(jsString, stringOnWasmHeap, lengthBytes+1);
        return stringOnWasmHeap;
    });
    arguments.OffsetCapacity(1);
    arguments.Push(param);
}

void defos_set_window_size(float x, float y, float w, float h) {
    defos_set_view_size(x, y, w, h);
}

void defos_set_view_size(float x, float y, float w, float h) {
    EM_ASM({
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
    });
    rect.h = EM_ASM_INT({
        return Module.canvas.height;
    });
    return rect;
}

bool defos_is_console_visible() {
    return false;
}

void defos_set_console_visible(bool visible) {
    dmLogWarning("Method 'set_console_visible' is only supported on Windows");
}

void defos_set_cursor_visible(bool visible) {
    if (is_cursor_visible == visible) { return; }
    is_cursor_visible = visible;
    if (visible) {
        EM_ASM({Module.canvas.style.cursor = UTF8ToString($0);}, current_cursor);
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

WinPoint defos_get_cursor_pos() {
    return defos_get_cursor_pos_view();
}

WinPoint defos_get_cursor_pos_view() {
    WinPoint point;
    point.x = (float)EM_ASM_DOUBLE({ return Module.__defosjs_mouse_x }, 0.0);
    point.y = (float)EM_ASM_DOUBLE({ return Module.__defosjs_mouse_y }, 0.0);
    return point;
}

void defos_set_cursor_pos(float x, float y) {
    dmLogWarning("Method 'defos_set_cursor_pos' is not supported in HTML5");
}

void defos_set_cursor_pos_view(float x, float y) {
    dmLogWarning("Method 'defos_set_cursor_pos_view' is not supported in HTML5");
}

void defos_set_cursor_clipped(bool clipped) {
    dmLogWarning("Method 'defos_set_cursor_clipped' is not supported in HTML5");
}

bool defos_is_cursor_clipped() {
    return false;
}

EM_JS(void, defos_set_cursor_locked_, (bool locked), {
    if (locked) {
        (Module.canvas.requestPointerLock ||
        Module.canvas.mozRequestPointerLock ||
        Module.canvas.webkitRequestPointerLock ||
        Module.canvas.msRequestPointerLock ||
        function () {}).call(Module.canvas);
    } else {
        (document.exitPointerLock ||
        document.mozExitPointerLock ||
        document.webkitExitPointerLock ||
        document.msExitPointerLock ||
        function () {}).call(document);
    }
});

void defos_set_cursor_locked(bool locked) {
    is_cursor_locked = locked;
    defos_set_cursor_locked_(locked);
}

bool defos_is_cursor_locked() {
    return is_cursor_locked;
}

void *defos_load_cursor_html5(const char *url) {
    size_t len = strlen(url);
    CustomCursor * cursor = (CustomCursor*)malloc(sizeof(CustomCursor) + len + 12);
    cursor->ref_count = 1;
    strcpy(cursor->url, "url(");
    strcpy(cursor->url + 4, url);
    strcpy(cursor->url + 4 + len, "), auto");
    return cursor;
}

void defos_gc_custom_cursor(void *_cursor) {
    CustomCursor * cursor = (CustomCursor*)_cursor;
    if (!cursor) { return; }
    cursor->ref_count -= 1;
    if (!cursor->ref_count) {
        free(cursor);
    }
}

static void update_cursor() {
    if (is_cursor_visible) {
        EM_ASM({Module.canvas.style.cursor = UTF8ToString($0);}, current_cursor);
    }
}

void defos_set_custom_cursor(void *_cursor) {
    CustomCursor * cursor = (CustomCursor*)_cursor;
    cursor->ref_count += 1;
    defos_gc_custom_cursor(current_custom_cursor);

    current_cursor = cursor->url;
    current_custom_cursor = cursor;

    update_cursor();
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
    defos_gc_custom_cursor(current_custom_cursor);
    current_cursor = get_cursor(cursor);
    current_custom_cursor = NULL;
    update_cursor();
}

void defos_reset_cursor() {
    defos_gc_custom_cursor(current_custom_cursor);
    current_cursor = "default";
    current_custom_cursor = NULL;
    update_cursor();
}

void defos_get_displays(dmArray<DisplayInfo> &displayList) {
}

void defos_get_display_modes(DisplayID displayID, dmArray<DisplayModeInfo> &modeList) {
}

DisplayID defos_get_current_display() {
    return NULL;
}

#endif
