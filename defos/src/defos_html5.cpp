#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_HTML5)

#include <emscripten.h>
#include "defos_private.h"

static bool is_maximized = false;
static WinRect previous_state;

void js_warn(const char* msg) {
    EM_ASM_({console.warn(UTF8ToString($0))}, msg);
}

void defos_init() {
}

void defos_final() {
}

void defos_event_handler_was_set(DefosEvent event) {
    js_warn("Callbacks 'on_mouse_leave' and 'on_mouse_enter' don't supported in html5");
}

void defos_disable_maximize_button() {
    js_warn("Method 'disable_maximize_button' don't supported in html5");
}

void defos_disable_minimize_button() {
    js_warn("Method 'disable_minimize_button' don't supported in html5");
}

void defos_disable_window_resize() {
    js_warn("Method 'disable_window_resize' don't supported in html5");
}

void defos_disable_mouse_cursor() {
     EM_ASM(Module.canvas.style.cursor = 'none';);
}

void defos_enable_mouse_cursor() {
    EM_ASM(Module.canvas.style.cursor = 'default';);
}

void defos_toggle_fullscreen() {
    js_warn("Method 'toggle_fullscreen' don't supported in html5, you can use Module.toggleFullscreen() method in JS");
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
    return false;
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
