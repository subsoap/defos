#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_HTML5)

#include <emscripten.h>
#include "defos_private.h"

static bool is_maximized = false;
static bool is_mouse_inside = false;
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
    }
    defos_emit_event(event);
}

void defos_init() {
    EM_ASM_({
        Module.__defosjs_mouseenter_listener = function () {
            _defos_emit_event_from_js($0);
        };
        Module.__defosjs_mouseleave_listener = function () {
            _defos_emit_event_from_js($1);
        };
        Module.canvas.addEventListener('mouseenter', Module.__defosjs_mouseenter_listener);
        Module.canvas.addEventListener('mouseleave', Module.__defosjs_mouseleave_listener);
    }, DEFOS_EVENT_MOUSE_ENTER, DEFOS_EVENT_MOUSE_LEAVE);
}

void defos_final() {
    EM_ASM(
        Module.canvas.removeEventListener('mouseenter', Module.__defosjs_mouseenter_listener);
        Module.canvas.removeEventListener('mouseleave', Module.__defosjs_mouseleave_listener);
    );
}

void defos_event_handler_was_set(DefosEvent event) {
}

void defos_disable_maximize_button() {
    js_warn("DefOS: Method 'disable_maximize_button' is not supported in html5");
}

void defos_disable_minimize_button() {
	js_warn("DefOS: Method 'disable_minimize_button' is not supported in html5");
}

void defos_disable_window_resize() {
	js_warn("DefOS: Method 'disable_window_resize' is not supported in html5");
}

void defos_disable_mouse_cursor() {
     EM_ASM(Module.canvas.style.cursor = 'none';);
}

void defos_enable_mouse_cursor() {
    EM_ASM(Module.canvas.style.cursor = 'default';);
}

void defos_toggle_fullscreen() {
	js_warn("DefOS: Method 'toggle_fullscreen' is not supported in html5, you can use Module.toggleFullscreen() method in JS");
}

void defos_show_console() {
	js_warn("DefOS: Method 'defos_show_console' is not supported in html5, it is meant for Windows builds only");
}

void defos_hide_console() {
	js_warn("DefOS: Method 'defos_hide_console' is not supported in html5, it is meant for Windows builds only");
}

bool defos_is_console_visible() {
	js_warn("DefOS: Method 'defos_is_console_visible' is not supported in html5, it is meant for Windows builds only");
}

void defos_toggle_maximize() {
    if (is_maximized) {
        is_maximized = false;
        defos_set_window_size(0,0, previous_state.w, previous_state.h);
    }
    else{
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

bool defos_is_mouse_inside_window() {
    return is_mouse_inside;
}

void defos_set_window_size(int x, int y, int w, int h) {
    EM_ASM_({
        Module.canvas.width = $0;
        Module.canvas.height = $1;
    }, w, h);
}

void defos_set_window_title(const char* title_lua) {
    EM_ASM_({document.title = UTF8ToString($0)}, title_lua);
}

WinRect defos_get_window_size(){
	WinRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = EM_ASM_INT({
        return Module.canvas.width;
    },0);
    rect.h = EM_ASM_INT({
        return Module.canvas.height;
    },0);
	return rect;
}

#endif
