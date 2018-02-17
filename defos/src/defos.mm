#define DLIB_LOG_DOMAIN "defos"
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_OSX)

#include "defos_private.h"
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

static NSWindow* window = nil;
static NSCursor* current_cursor = nil;
static NSCursor* default_cursor = nil;

static bool is_maximized = false;
static bool is_mouse_inside_window = false;
static NSRect previous_state;

static void enable_mouse_tracking();
static void disable_mouse_tracking();

void defos_init() {
    window = dmGraphics::GetNativeOSXNSWindow();
    // [window disableCursorRects];
    // [window resetCursorRects];
    default_cursor = NSCursor.arrowCursor;
    current_cursor = default_cursor;
    [current_cursor retain];
    enable_mouse_tracking();
}

void defos_final() {
    disable_mouse_tracking();
    [current_cursor release];
    current_cursor = nil;
}

void defos_event_handler_was_set(DefosEvent event) {
}

void defos_disable_maximize_button() {
    [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];
}

void defos_disable_minimize_button() {
    [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
}

void defos_disable_window_resize() {
    [window setStyleMask:[window styleMask] & ~NSResizableWindowMask];
}

void defos_toggle_fullscreen() {
    if (is_maximized){
        defos_toggle_maximize();
    }
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
    BOOL fullscreen = (([window styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask);
    return fullscreen == YES;
}

bool defos_is_maximized() {
    return is_maximized;
}

void defos_set_window_title(const char* title_lua) {
    NSString* title = [NSString stringWithUTF8String:title_lua];
    [window setTitle:title];
}

void defos_set_window_size(int x, int y, int w, int h) {
    // correction for result like on Windows PC
    int win_y = NSMaxY(NSScreen.screens[0].frame) - h - y;
    [window setFrame:NSMakeRect(x, win_y, w , h) display:YES];
}

void defos_set_view_size(int x, int y, int w, int h) {
    int win_y = NSMaxY(NSScreen.screens[0].frame) - h - y;
    NSView* view = dmGraphics::GetNativeOSXNSView();
    NSRect viewFrame = [view convertRect: view.bounds toView: nil];
    NSRect windowFrame = window.frame;
    NSRect rect = NSMakeRect(
        x - viewFrame.origin.x,
        win_y - viewFrame.origin.x,
        w + viewFrame.origin.x + windowFrame.size.width - viewFrame.size.width,
        h + viewFrame.origin.y + windowFrame.size.height - viewFrame.size.height
    );
    [window setFrame: rect display:YES];
}

WinRect defos_get_window_size() {
    WinRect rect;
    NSRect frame = [window frame];
    rect.x = frame.origin.x;
    rect.y = NSMaxY(NSScreen.screens[0].frame) - NSMaxY(frame);
    rect.w = frame.size.width;
    rect.h = frame.size.height;
    return rect;
}

void defos_show_console() {
    dmLogInfo("Method 'defos_show_console' is not supported in macOS");
}

void defos_hide_console() {
    dmLogInfo("Method 'defos_hide_console' is not supported in macOS");
}

bool defos_is_console_visible() {
    dmLogInfo("Method 'defos_is_console_visible' is not supported in macOS");
    return false;
}

void defos_disable_mouse_cursor() {
    [NSCursor hide];
}

void defos_enable_mouse_cursor() {
    [NSCursor unhide];
}

bool defos_is_mouse_inside_window() {
    return is_mouse_inside_window;
}

void defos_set_cursor_pos(int x, int y) {
    CGWarpMouseCursorPosition(CGPointMake(x, y));
}

void defos_move_cursor_to(int x, int y) {
    NSView* view = dmGraphics::GetNativeOSXNSView();
    NSPoint pointInWindow = [view convertPoint: NSMakePoint(x, view.bounds.size.height - y) toView: nil];
    NSPoint windowOrigin = window.frame.origin;
    NSPoint point = NSMakePoint(pointInWindow.x + windowOrigin.x, pointInWindow.y + windowOrigin.y);
    point.y = NSMaxY(NSScreen.screens[0].frame) - point.y;
    defos_set_cursor_pos(point.x, point.y);
}

void defos_clip_cursor() {
    dmLogInfo("Method 'defos_clip_cursor' is not supported in macOS");
}

void defos_restore_cursor_clip() {
    dmLogInfo("Method 'defos_restore_cursor_clip' is not supported in macOS");
}

void defos_set_custom_cursor_mac(dmBuffer::HBuffer buffer, float hotSpotX, float hotSpotY) {
    uint8_t* bytes = NULL;
    uint32_t size = 0;
    if (dmBuffer::GetBytes(buffer, (void**)&bytes, &size) != dmBuffer::RESULT_OK) {
      dmLogError("defos_set_custom_cursor_mac: dmBuffer::GetBytes failed");
      return;
    }

    uint8_t* copy = (uint8_t*)malloc(size);
    memcpy(copy, bytes, size);
    NSData * data = [[NSData alloc] initWithBytesNoCopy:copy length:size freeWhenDone:YES];

    NSImage * image = [[NSImage alloc] initWithData:data];
    NSCursor * cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(hotSpotX, hotSpotY)];
    [current_cursor release];
    current_cursor = cursor;
    [image release];
    [data release];
    [current_cursor set];
}

static NSCursor * get_cursor(DefosCursor cursor) {
  switch (cursor) {
    case DEFOS_CURSOR_ARROW:
        return NSCursor.arrowCursor;
    case DEFOS_CURSOR_HAND:
        return NSCursor.pointingHandCursor;
    case DEFOS_CURSOR_CROSSHAIR:
        return NSCursor.crosshairCursor;
    case DEFOS_CURSOR_IBEAM:
        return NSCursor.IBeamCursor;
    default:
        return NSCursor.arrowCursor;
  }
}

void defos_set_cursor(DefosCursor cur) {
    NSCursor * cursor = get_cursor(cur);
    [cursor set];
    [cursor retain];
    [current_cursor release];
    current_cursor = cursor;
}

void defos_reset_cursor() {
    [default_cursor set];
    [default_cursor retain];
    [current_cursor release];
    current_cursor = default_cursor;
}

@interface DefOSMouseTracker : NSResponder
@end
@implementation DefOSMouseTracker
- (void)mouseEntered:(NSEvent *)event {
    is_mouse_inside_window = true;
    defos_emit_event(DEFOS_EVENT_MOUSE_ENTER);
}
- (void)mouseExited:(NSEvent *)event {
    is_mouse_inside_window = false;
    defos_emit_event(DEFOS_EVENT_MOUSE_LEAVE);
}
// For some reason this doesn't get called and the homonymous method
// gets called on the view instead
- (void)cursorUpdate:(NSEvent *)event {
    [current_cursor set];
}
@end

// This is unstable in case Defold renames this class
// Maybe use the objc runtime to hook this method if there's no better solution?
@interface GLFWContentView
@end
@interface GLFWContentView(CursorUpdate)
- (void)cursorUpdate:(NSEvent *)event;
@end
@implementation GLFWContentView(CursorUpdate)
- (void)cursorUpdate:(NSEvent *)event {
    [current_cursor set];
}
@end

static DefOSMouseTracker* mouse_tracker = nil;
static NSTrackingArea* tracking_area = nil;

static void enable_mouse_tracking() {
    if (tracking_area) { return; }
    NSView * view = dmGraphics::GetNativeOSXNSView();
    mouse_tracker = [[DefOSMouseTracker alloc] init];
    tracking_area = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
        options: NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingActiveAlways | NSTrackingCursorUpdate
        owner: mouse_tracker
        userInfo: nil
    ];
    [view addTrackingArea:tracking_area];
    [tracking_area release];
}

static void disable_mouse_tracking() {
    if (!tracking_area) { return; }

    [dmGraphics::GetNativeOSXNSView() removeTrackingArea:tracking_area];
    tracking_area = nil;

    [mouse_tracker release];
    mouse_tracker = nil;
}

#endif
