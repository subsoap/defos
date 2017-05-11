#include <dmsdk/sdk.h>
#include "defos_private.h"

#if defined(DM_PLATFORM_OSX)
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

NSWindow* window;

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
	init_window();
	[window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    [window toggleFullScreen:window];
}

bool defos_isFullScreen() {
	init_window();
    BOOL fullscreen = (([window styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask);
    return fullscreen == YES;
}

void defos_set_window_size(lua_State* L) {
    init_window();
    int x = luaL_checkint(L, 1);
    int y = luaL_checkint(L, 2);
    int w = luaL_checkint(L, 3);
    int h = luaL_checkint(L, 4);
    [window setFrame:NSMakeRect(x, y, w , h) display:YES];
}

void defos_set_window_title(lua_State* L) {
    init_window();
    const char* title_lua = luaL_checkstring(L, 1);
	NSString* title = [NSString stringWithUTF8String:title_lua];
    [window setTitle:title];
}

#endif
