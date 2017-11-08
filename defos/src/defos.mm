#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_OSX)
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

NSWindow* window;

bool is_maximized = false;
bool is_mouse_cursor_within_window = false;
NSRect previous_state;

void init_window(){
    if (window == NULL) {
        window = dmGraphics::GetNativeOSXNSWindow();
    }
}

void defos_disable_maximize_button() {
    init_window();
    [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];
}

void defos_disable_minimize_button() {
    init_window();
    [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
}

void defos_disable_window_resize() {
    init_window();
    [window setStyleMask:[window styleMask] & ~NSResizableWindowMask];
}

void defos_disable_mouse_cursor() {
    [NSCursor hide];
}

void defos_enable_mouse_cursor() {
    [NSCursor unhide];
}

void defos_toggle_fullscreen() {
    if (is_maximized){
        defos_toggle_maximize();
    }
	init_window();
	[window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    [window toggleFullScreen:window];
}

void defos_toggle_maximize() {
    if (defos_is_fullscreen()){
        defos_toggle_fullscreen();
    }
    if (is_maximized){
        is_maximized = false;
        [window setFrame:previous_state display:YES];
    }
    else
    {
        is_maximized = true;
        previous_state = [window frame];
        [window setFrame:[[NSScreen mainScreen] visibleFrame] display:YES];
    }
}

bool defos_is_fullscreen() {
	init_window();
    BOOL fullscreen = (([window styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask);
    return fullscreen == YES;
}

bool defos_is_maximized() {
    return is_maximized;
}

bool defos_is_mouse_cursor_within_window() {
	return is_mouse_cursor_within_window;
}

void defos_set_window_size(int x, int y, int w, int h) {
    init_window();
	//correction for result like on Windows PC
	int win_y = [[window screen] frame].size.height - h - y;
    [window setFrame:NSMakeRect(x, win_y, w , h) display:YES];
}

void defos_set_window_title(const char* title_lua) {
    init_window();
	NSString* title = [NSString stringWithUTF8String:title_lua];
    [window setTitle:title];
}

#endif
