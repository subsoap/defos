
#if defined(DM_PLATFORM_OSX) && !defined(DM_HEADLESS)

#include "defos_private.h"
#include <dmsdk/sdk.h>
#include <AppKit/AppKit.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CVDisplayLink.h>

static NSWindow* window = nil;
static NSCursor* current_cursor = nil;
static NSCursor* default_cursor = nil;

#define MAX_DISPLAYS 32

static bool is_maximized = false;
static bool is_mouse_in_view = false;
static bool is_cursor_visible = true;
static bool is_cursor_locked = false;
static bool is_cursor_clipped = false;
static bool is_fullscreen_transitioning = false;
static bool desired_borderless = false;
static bool has_pending_borderless_change = false;
static NSRect previous_state;
static NSUInteger windowed_style_mask = 0;

// Current display mode. On macOS fullscreen is visible in the window style mask, but a window
// covering the visible screen area looks exactly like a regular window, so the selected mode is
// stored here.
static DefosDisplayMode current_display_mode = DEFOS_DISPLAY_MODE_WINDOWED;
static bool is_display_mode_known = false;

// Whether the user may resize the window. On macOS the lock is done with minSize/maxSize and NOT
// by removing NSResizableWindowMask: without that mask toggleFullScreen: stops working, so the
// window could not enter fullscreen anymore.
static bool user_resizable = true;

// Leaving native fullscreen is animated and asynchronous: a frame set right after
// toggleFullScreen: is overwritten by the system. So the target geometry is stored and applied on
// the notification that fullscreen has actually been left.
static bool has_pending_frame = false;
static NSRect pending_frame;
static id fullscreen_exit_observer = nil;

// Share of the visible screen area used by the windowed mode by default.
static const float WINDOWED_VISIBLE_FRAME_PART = 0.8f;

static void enable_mouse_tracking();
static void disable_mouse_tracking();
static NSUInteger default_windowed_style_mask();
static void apply_borderless_state(bool borderless);
static void apply_pending_borderless_state();

@interface DefosWindowObserver : NSObject
@end

static DefosWindowObserver* window_observer = nil;

@implementation DefosWindowObserver

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
    (void)notification;
    is_fullscreen_transitioning = true;
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
    (void)notification;
    is_fullscreen_transitioning = false;
    if (has_pending_borderless_change && window != nil && defos_is_fullscreen()) {
        [window toggleFullScreen:window];
    }
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
    (void)notification;
    is_fullscreen_transitioning = true;
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
    (void)notification;
    is_fullscreen_transitioning = false;
    apply_pending_borderless_state();
}

@end

static NSUInteger default_windowed_style_mask() {
    return NSWindowStyleMaskTitled |
           NSWindowStyleMaskResizable |
           NSWindowStyleMaskClosable |
           NSWindowStyleMaskMiniaturizable;
}

static void apply_borderless_state(bool borderless) {
    if (!window || is_fullscreen_transitioning || defos_is_fullscreen()) {
        return;
    }

    if (borderless) {
        NSUInteger current_style_mask = [window styleMask] & ~NSWindowStyleMaskFullScreen;
        if (current_style_mask != NSWindowStyleMaskBorderless) {
            windowed_style_mask = current_style_mask;
        }
        [window setStyleMask:NSWindowStyleMaskBorderless];
    } else {
        NSUInteger target_style_mask = windowed_style_mask;
        if (target_style_mask == NSWindowStyleMaskBorderless) {
            target_style_mask = default_windowed_style_mask();
        }
        [window setStyleMask:target_style_mask];
    }

    [window makeKeyAndOrderFront:nil];
}

static void apply_pending_borderless_state() {
    if (!has_pending_borderless_change || is_fullscreen_transitioning || defos_is_fullscreen()) {
        return;
    }

    apply_borderless_state(desired_borderless);
    has_pending_borderless_change = false;
}
static void set_window_frame(NSRect frame);

/*
* Convert the mode string to the more convinient bits per pixel value
*/
static int getBPPFromModeString(CFStringRef mode)
{
    if ((CFStringCompare(mode, CFSTR(kIO30BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
        // This is a strange mode, where we using 10 bits per RGB component and pack it into 32 bits
        // Java is not ready to work with this mode but we have to specify it as supported
        return 30;
    }
    else if (CFStringCompare(mode, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        return 32;
    }
    else if (CFStringCompare(mode, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        return 16;
    }
    else if (CFStringCompare(mode, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        return 8;
    }

    return 0;
}

void defos_init() {
    window = dmGraphics::GetNativeOSXNSWindow();
    windowed_style_mask = default_windowed_style_mask();
    desired_borderless = false;
    has_pending_borderless_change = false;
    is_fullscreen_transitioning = false;
    if (window) {
        NSUInteger initial_style_mask = [window styleMask] & ~NSWindowStyleMaskFullScreen;
        desired_borderless = initial_style_mask == NSWindowStyleMaskBorderless;
        if (!desired_borderless && initial_style_mask != NSWindowStyleMaskBorderless) {
            windowed_style_mask = initial_style_mask;
        }

        window_observer = [[DefosWindowObserver alloc] init];
        NSNotificationCenter* notification_center = [NSNotificationCenter defaultCenter];
        [notification_center addObserver:window_observer selector:@selector(windowWillEnterFullScreen:) name:NSWindowWillEnterFullScreenNotification object:window];
        [notification_center addObserver:window_observer selector:@selector(windowDidEnterFullScreen:) name:NSWindowDidEnterFullScreenNotification object:window];
        [notification_center addObserver:window_observer selector:@selector(windowWillExitFullScreen:) name:NSWindowWillExitFullScreenNotification object:window];
        [notification_center addObserver:window_observer selector:@selector(windowDidExitFullScreen:) name:NSWindowDidExitFullScreenNotification object:window];
    }
    // [window disableCursorRects];
    // [window resetCursorRects];
    default_cursor = NSCursor.arrowCursor;
    [default_cursor retain];
    current_cursor = default_cursor;
    [current_cursor retain];
    enable_mouse_tracking();

    // Wait until native fullscreen has actually been left to apply the geometry of the selected
    // mode: the transition is animated, and a frame set before it ends is lost.
    fullscreen_exit_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSWindowDidExitFullScreenNotification
                    object:window
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *notification) {
                    if (has_pending_frame) {
                        has_pending_frame = false;
                        set_window_frame(pending_frame);
                    }
                }];
    [fullscreen_exit_observer retain];
}

void defos_final() {
    disable_mouse_tracking();
    if (window_observer != nil) {
        [[NSNotificationCenter defaultCenter] removeObserver:window_observer];
        [window_observer release];
        window_observer = nil;
    }
    if (fullscreen_exit_observer) {
        [[NSNotificationCenter defaultCenter] removeObserver:fullscreen_exit_observer];
        [fullscreen_exit_observer release];
        fullscreen_exit_observer = nil;
    }
    [default_cursor release];
    [current_cursor release];
    current_cursor = nil;
    default_cursor = nil;
}

void defos_update() {
    apply_pending_borderless_state();
}

void defos_event_handler_was_set(DefosEvent event) {
}

void defos_disable_maximize_button() {
    [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];
}

void defos_disable_minimize_button() {
    [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
}

// Screen the window is currently on. Using [NSScreen mainScreen] here would move the window to the
// main screen on multi-monitor setups.
static NSScreen* current_screen() {
    return window.screen ? window.screen : [NSScreen mainScreen];
}

// Converts an AppKit rectangle (origin at the bottom left) to defos coordinates, where y is counted
// from the top edge of the main screen, as in defos_get_window_size().
static WinRect to_win_rect(NSRect rect) {
    WinRect result;
    result.x = rect.origin.x;
    result.y = NSMaxY(NSScreen.screens[0].frame) - NSMaxY(rect);
    result.w = rect.size.width;
    result.h = rect.size.height;
    return result;
}

// Removes size limits: otherwise setFrame: clamps the new frame to the minSize/maxSize used to
// lock resizing.
static void release_resize_limits() {
    [window setMinSize:NSMakeSize(1, 1)];
    [window setMaxSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
}

static void apply_resize_limits(NSSize size) {
    // No limits in fullscreen: the system manages the window there, and minSize/maxSize pinned to the
    // screen size would break both the enter animation and the way back.
    if (user_resizable || defos_is_fullscreen()) {
        release_resize_limits();
    } else {
        [window setMinSize:size];
        [window setMaxSize:size];
    }
}

static void set_window_frame(NSRect frame) {
    release_resize_limits();
    [window setFrame:frame display:YES];
    apply_resize_limits(frame.size);
}

void defos_disable_window_resize() {
    // Keep NSResizableWindowMask: without it macOS does not let the window enter fullscreen.
    defos_set_window_resizable(false);
}

void defos_toggle_fullscreen() {
    // The old API keeps working: it switches between fullscreen and a regular window.
    defos_set_display_mode(
        defos_is_fullscreen() ? DEFOS_DISPLAY_MODE_WINDOWED : DEFOS_DISPLAY_MODE_FULLSCREEN,
        nanf(""), nanf(""));
}

void defos_toggle_borderless() {
    if (!window) return;

    desired_borderless = !desired_borderless;

    if (defos_is_fullscreen() || is_fullscreen_transitioning) {
        has_pending_borderless_change = true;
        if (defos_is_fullscreen() && !is_fullscreen_transitioning) {
            [window toggleFullScreen:window];
        }
        return;
    }

    apply_borderless_state(desired_borderless);
}

void defos_toggle_maximized() {
    if (defos_is_fullscreen()){
        // Leaving fullscreen on macOS is asynchronous, so the target geometry goes through the common
        // display mode code, which applies it once fullscreen has actually been left.
        defos_set_display_mode(
            is_maximized ? DEFOS_DISPLAY_MODE_WINDOWED : DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED,
            nanf(""), nanf(""));
        return;
    }
    if (is_maximized){
        is_maximized = false;
        set_window_frame(previous_state);
    }
    else
    {
        is_maximized = true;
        previous_state = [window frame];
        set_window_frame(current_screen().visibleFrame);
    }
}

void defos_set_display_mode(DefosDisplayMode mode, float width, float height) {
    NSRect visible = current_screen().visibleFrame;

    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN) {
        has_pending_frame = false;
        if (!defos_is_fullscreen()) {
            // Remove size limits before the transition: the system expands the window to the whole screen
            // itself and would hit maxSize.
            release_resize_limits();
            [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
            [window toggleFullScreen:window];
        }
        is_maximized = false;
        current_display_mode = mode;
        is_display_mode_known = true;
        return;
    }

    NSRect target;
    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED) {
        // On macOS this is a titled window covering the visible screen area: the menu bar and the Dock
        // stay available. A borderless window is avoided on purpose, it may stop receiving keyboard input
        // (canBecomeKeyWindow returns NO for a window without a title bar).
        target = visible;
    } else {
        float client_w = isnan(width) ? visible.size.width * WINDOWED_VISIBLE_FRAME_PART : width;
        float client_h = isnan(height) ? visible.size.height * WINDOWED_VISIBLE_FRAME_PART : height;

        // frameRectForContentRect adds the frame and the title bar to the client area, so the whole
        // window, title bar included, fits into the visible screen area.
        target = [window frameRectForContentRect:NSMakeRect(0, 0, client_w, client_h)];
        if (target.size.width > visible.size.width) { target.size.width = visible.size.width; }
        if (target.size.height > visible.size.height) { target.size.height = visible.size.height; }
        target.origin.x = visible.origin.x + (visible.size.width - target.size.width) * 0.5f;
        target.origin.y = visible.origin.y + (visible.size.height - target.size.height) * 0.5f;
    }

    bool was_fullscreen = defos_is_fullscreen();

    // previous_state is the regular window geometry restored by the old toggle_maximized API. Store it
    // before maximizing and update it when returning to the windowed mode.
    if (mode == DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED) {
        if (!was_fullscreen && !is_maximized) {
            previous_state = [window frame];
        }
    } else {
        previous_state = target;
    }

    if (was_fullscreen) {
        // Fullscreen is left asynchronously, the notification observer applies the frame.
        pending_frame = target;
        has_pending_frame = true;
        release_resize_limits();
        [window toggleFullScreen:window];
    } else {
        set_window_frame(target);
    }

    is_maximized = (mode == DEFOS_DISPLAY_MODE_FULLSCREEN_WINDOWED);
    current_display_mode = mode;
    is_display_mode_known = true;
}

DefosDisplayMode defos_get_display_mode() {
    // Always read the fullscreen state from the system: the user may have entered it with the green
    // window button or a keyboard shortcut.
    if (defos_is_fullscreen()) {
        return DEFOS_DISPLAY_MODE_FULLSCREEN;
    }
    if (!is_display_mode_known || current_display_mode == DEFOS_DISPLAY_MODE_FULLSCREEN) {
        return DEFOS_DISPLAY_MODE_WINDOWED;
    }
    return current_display_mode;
}

bool defos_is_display_mode_supported(DefosDisplayMode mode) {
    // All three modes are native on macOS.
    return true;
}

WinRect defos_get_work_area() {
    return to_win_rect(current_screen().visibleFrame);
}

void defos_set_window_resizable(bool resizable) {
    user_resizable = resizable;
    apply_resize_limits(window.frame.size);
}

bool defos_is_window_resizable() {
    return user_resizable;
}

void defos_toggle_always_on_top() {
    window.level = defos_is_always_on_top() ? NSNormalWindowLevel : NSFloatingWindowLevel;
}

bool defos_is_fullscreen() {
    if (!window) return false;
    BOOL fullscreen = (([window styleMask] & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen);
    return fullscreen == YES;
}

bool defos_is_borderless() {
    return desired_borderless;
}

bool defos_is_maximized() {
    return is_maximized;
}

bool defos_is_always_on_top() {
    return window.level == NSFloatingWindowLevel;
}

void defos_minimize() {
    [window performMiniaturize: nil];
}

void defos_activate() {
    [[NSApplication sharedApplication] activateIgnoringOtherApps: YES];
}

void defos_set_window_title(const char* title_lua) {
    NSString* title = [NSString stringWithUTF8String:title_lua];
    [window setTitle:title];
}

void defos_set_window_icon(const char *icon_path){
    @autoreleasepool {
        NSString *path = [NSString stringWithUTF8String:icon_path];
        NSImage* image = [[NSImage alloc] initWithContentsOfFile: path];
        [window setRepresentedURL:[NSURL URLWithString:path]];
        [[window standardWindowButton:NSWindowDocumentIconButton] setImage:image];
        [image release];
    }
}

char* defos_get_bundle_root() {
    const char *bundlePath = [[[NSBundle mainBundle] bundlePath] UTF8String];
    char *bundlePath_lua = (char*)malloc(strlen(bundlePath) + 1);
    strcpy(bundlePath_lua, bundlePath);
    return bundlePath_lua;
}

void defos_get_arguments(dmArray<char*> &arguments) {
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    int argCount = [args count];
    arguments.OffsetCapacity(argCount);
    for (int i = 0; i < argCount; i++){
        const char *param = [args[i] UTF8String];
        char* lua_param = (char*)malloc(strlen(param) + 1);
        strcpy(lua_param, param);
        arguments.Push(lua_param);
    }
}

void defos_set_window_size(float x, float y, float w, float h) {
    if (isnan(x)) {
        NSRect frame = window.screen.frame;
        x = floorf(frame.origin.x + (frame.size.width - w) * 0.5f);
    }
    float win_y;
    if (isnan(y)) {
        NSRect frame = window.screen.frame;
        win_y = floorf(frame.origin.y + (frame.size.height - h) * 0.5f);
    } else {
        // correction for result like on Windows PC
        win_y = NSMaxY(NSScreen.screens[0].frame) - h - y;
    }

    [window setFrame:NSMakeRect(x, win_y, w , h) display:YES];
}

void defos_set_view_size(float x, float y, float w, float h) {
    if (isnan(x)) {
        NSRect frame = window.screen.frame;
        x = floorf(frame.origin.x + (frame.size.width - w) * 0.5f);
    }
    float win_y;
    if (isnan(y)) {
        NSRect frame = window.screen.frame;
        win_y = floorf(frame.origin.y + (frame.size.height - h) * 0.5f);
    } else {
        win_y = NSMaxY(NSScreen.screens[0].frame) - h - y;
    }

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

WinRect defos_get_view_size() {
    WinRect rect;
    NSView* view = dmGraphics::GetNativeOSXNSView();
    NSRect viewFrame = [view convertRect: view.bounds toView: nil];
    NSRect windowFrame = [window frame];
    viewFrame.origin.x += windowFrame.origin.x;
    viewFrame.origin.y += windowFrame.origin.y;
    rect.x = viewFrame.origin.x;
    rect.y = NSMaxY(NSScreen.screens[0].frame) - NSMaxY(viewFrame);
    rect.w = viewFrame.size.width;
    rect.h = viewFrame.size.height;
    return rect;
}

void defos_set_console_visible(bool visible) {
    dmLogWarning("Method 'set_console_visible' is only supported on Windows");
}

bool defos_is_console_visible() {
    return false;
}

void defos_set_cursor_visible(bool visible) {
    if (is_cursor_visible == visible) { return; }
    is_cursor_visible = visible;
    if (visible) {
        [NSCursor unhide];
    } else {
        [NSCursor hide];
    }
}

bool defos_is_cursor_visible() {
    return is_cursor_visible;
}

bool defos_is_mouse_in_view() {
    return is_mouse_in_view;
}

WinPoint defos_get_cursor_pos() {
    NSPoint point = NSEvent.mouseLocation;
    WinPoint result;
    result.x = (float)point.x;
    result.y = (float)NSMaxY(NSScreen.screens[0].frame) - point.y;
    return result;
}

WinPoint defos_get_cursor_pos_view() {
    NSView* view = dmGraphics::GetNativeOSXNSView();
    NSPoint pointInScreen = NSEvent.mouseLocation;
    NSPoint windowOrigin = window.frame.origin;
    NSPoint pointInWindow = NSMakePoint(
      pointInScreen.x - windowOrigin.x,
      pointInScreen.y - windowOrigin.y
    );
    NSPoint point = [view convertPoint: pointInWindow fromView: nil];
    WinPoint result;
    result.x = (float)point.x;
    result.y = (float)(view.bounds.size.height - point.y);
    return result;
}

void defos_set_cursor_pos(float x, float y) {
    CGWarpMouseCursorPosition(CGPointMake(x, y));
    if (!is_cursor_locked) {
        CGAssociateMouseAndMouseCursorPosition(true); // Prevents a delay after the Wrap call
    }
}

void defos_set_cursor_pos_view(float x, float y) {
    NSView* view = dmGraphics::GetNativeOSXNSView();
    NSPoint pointInWindow = [view convertPoint: NSMakePoint(x, view.bounds.size.height - y) toView: nil];
    NSPoint windowOrigin = window.frame.origin;
    NSPoint point = NSMakePoint(pointInWindow.x + windowOrigin.x, pointInWindow.y + windowOrigin.y);
    point.y = NSMaxY(NSScreen.screens[0].frame) - point.y;
    defos_set_cursor_pos(point.x, point.y);
}

void defos_set_cursor_clipped(bool clipped) {
    is_cursor_clipped = clipped;
    if (clipped) { is_mouse_in_view = true; }
}

bool defos_is_cursor_clipped() {
    return is_cursor_clipped;
}

static void clip_cursor(NSEvent * event) {
    NSView* view = dmGraphics::GetNativeOSXNSView();
    NSPoint mousePos = [view convertPoint:[event locationInWindow] fromView: nil];
    NSRect bounds = view.bounds;

    bool willClip = false;
    if (mousePos.x < NSMinX(bounds)) {
        willClip = true;
        mousePos.x = NSMinX(bounds);
    }
    if (mousePos.y <= NSMinY(bounds)) {
        willClip = true;
        mousePos.y = NSMinY(bounds) + 1.0;
    }
    if (mousePos.x >= NSMaxX(bounds)) {
        willClip = true;
        mousePos.x = NSMaxX(bounds) - 1.0;
    }
    if (mousePos.y > NSMaxY(bounds)) {
        willClip = true;
        mousePos.y = NSMaxY(bounds);
    }

    if (willClip) {
        defos_set_cursor_pos_view(mousePos.x, bounds.size.height - mousePos.y);
    }
}

extern void defos_set_cursor_locked(bool locked) {
    is_cursor_locked = locked;
    CGAssociateMouseAndMouseCursorPosition(!locked);
}

bool defos_is_cursor_locked() {
    return is_cursor_locked;
}

void *defos_load_cursor_mac(dmBuffer::HBuffer buffer, float hotSpotX, float hotSpotY) {
    @autoreleasepool {
        uint8_t* bytes = NULL;
        uint32_t size = 0;
        if (dmBuffer::GetBytes(buffer, (void**)&bytes, &size) != dmBuffer::RESULT_OK) {
            dmLogError("defos_set_custom_cursor_mac: dmBuffer::GetBytes failed");
            return NULL;
        }

        uint8_t* copy = (uint8_t*)malloc(size);
        memcpy(copy, bytes, size);
        NSData * data = [[NSData alloc] initWithBytesNoCopy:copy length:size freeWhenDone:YES];
        NSImage * image = [[NSImage alloc] initWithData:data];
        NSCursor * cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(hotSpotX, hotSpotY)];

        [image release];
        [data release];
        return cursor;
    }
}

void defos_gc_custom_cursor(void * _cursor) {
    NSCursor * cursor = (NSCursor*)_cursor;
    [cursor release];
}

void defos_set_custom_cursor(void * _cursor) {
    NSCursor * cursor = (NSCursor*)_cursor;
    [cursor retain];
    [current_cursor release];
    current_cursor = cursor;
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
    [cursor retain];
    [current_cursor release];
    current_cursor = cursor;
    [current_cursor set];
}

void defos_reset_cursor() {
    [default_cursor set];
    [default_cursor retain];
    [current_cursor release];
    current_cursor = default_cursor;
}

static DisplayModeInfo parse_mode(CGDisplayModeRef mode, CVDisplayLinkRef displayLink, double rotation) {
    DisplayModeInfo mode_info;
    mode_info.width = CGDisplayModeGetPixelWidth(mode);
    mode_info.height = CGDisplayModeGetPixelHeight(mode);
    mode_info.scaling_factor = (double)mode_info.width / (double)CGDisplayModeGetWidth(mode);
    mode_info.refresh_rate = CGDisplayModeGetRefreshRate(mode);

    CFStringRef pixelEncoding = CGDisplayModeCopyPixelEncoding(mode);
    mode_info.bits_per_pixel = getBPPFromModeString(pixelEncoding);
    CFRelease(pixelEncoding);
    mode_info.orientation = (unsigned long)rotation;
    mode_info.reflect_x = false;
    mode_info.reflect_y = false;

    if (mode_info.refresh_rate == 0) {
        const CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(displayLink);
        if (!(time.flags & kCVTimeIsIndefinite)) {
            mode_info.refresh_rate = (double)time.timeScale / (double)time.timeValue;
        }
    }

    return mode_info;
}

static io_service_t IOServicePortFromCGDisplayID(CGDirectDisplayID displayID) {
    io_iterator_t iter;
    io_service_t serv, servicePort = 0;

    CFMutableDictionaryRef matching = IOServiceMatching("IODisplayConnect");

    // releases matching for us
    kern_return_t err = IOServiceGetMatchingServices(kIOMasterPortDefault, matching, &iter);
    if (err) { return 0; }

    while ((serv = IOIteratorNext(iter)) != 0) {
        CFDictionaryRef displayInfo;
        CFNumberRef vendorIDRef;
        CFNumberRef productIDRef;
        CFNumberRef serialNumberRef;

        displayInfo = IODisplayCreateInfoDictionary(serv, kIODisplayOnlyPreferredName);

        Boolean success;
        success =  CFDictionaryGetValueIfPresent(displayInfo, CFSTR(kDisplayVendorID),  (const void**)&vendorIDRef);
        success &= CFDictionaryGetValueIfPresent(displayInfo, CFSTR(kDisplayProductID), (const void**)&productIDRef);

        if (!success) {
            CFRelease(displayInfo);
            continue;
        }

        SInt32 vendorID;
        CFNumberGetValue(vendorIDRef, kCFNumberSInt32Type, &vendorID);
        SInt32 productID;
        CFNumberGetValue(productIDRef, kCFNumberSInt32Type, &productID);

        // If a serial number is found, use it.
        // Otherwise serial number will be nil (= 0) which will match with the output of 'CGDisplaySerialNumber'
        SInt32 serialNumber = 0;
        if (CFDictionaryGetValueIfPresent(displayInfo, CFSTR(kDisplaySerialNumber), (const void**)&serialNumberRef)) {
            CFNumberGetValue(serialNumberRef, kCFNumberSInt32Type, &serialNumber);
        }

        // If the vendor and product id along with the serial don't match
        // then we are not looking at the correct monitor.
        // NOTE: The serial number is important in cases where two monitors
        //       are the exact same.
        if (CGDisplayVendorNumber(displayID) != vendorID ||
            CGDisplayModelNumber(displayID)  != productID ||
            CGDisplaySerialNumber(displayID) != serialNumber ) {
            CFRelease(displayInfo);
            continue;
        }

        servicePort = serv;
        CFRelease(displayInfo);
        break;
    }

    IOObjectRelease(iter);
    return servicePort;
}

static char* get_display_name(CGDirectDisplayID displayID) {
    NSString *screenName = nil;

    NSDictionary *deviceInfo = (NSDictionary *)IODisplayCreateInfoDictionary(IOServicePortFromCGDisplayID(displayID), kIODisplayOnlyPreferredName);
    NSDictionary *localizedNames = [deviceInfo objectForKey:[NSString stringWithUTF8String:kDisplayProductName]];

    if ([localizedNames count] > 0) {
        NSString *screenName = [localizedNames objectForKey:[[localizedNames allKeys] objectAtIndex:0]];
        size_t nameLength = [screenName lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
        char *name = (char*)malloc(nameLength);
        [screenName getCString:name maxLength:nameLength encoding:NSUTF8StringEncoding];
        [deviceInfo release];
        return name;
    }

    [deviceInfo release];
    return NULL;
}

void defos_get_displays(dmArray<DisplayInfo> &displayList){
    uint32_t numDisplays;
    CGDirectDisplayID displays[MAX_DISPLAYS];
    CGGetActiveDisplayList(MAX_DISPLAYS, displays, &numDisplays);

    displayList.OffsetCapacity(numDisplays);
    for (int i = 0; i < numDisplays; i++) {
        CGDirectDisplayID displayID = displays[i];

        // We don't report mirrored displays
        if (CGDisplayIsInMirrorSet(displayID) && CGDisplayPrimaryDisplay(displayID) != displayID) {
            continue;
        }

        DisplayInfo display;
        display.id = (void*)(size_t)displayID;

        CGRect bounds = CGDisplayBounds(displayID);
        display.bounds.x = bounds.origin.x;
        display.bounds.y = bounds.origin.y;
        display.bounds.w = bounds.size.width;
        display.bounds.h = bounds.size.height;

        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
        CVDisplayLinkRef displayLink;
        CVDisplayLinkCreateWithCGDisplay(displayID, &displayLink);
        double rotation = CGDisplayRotation(displayID);
        display.mode = parse_mode(mode, displayLink, rotation);
        CVDisplayLinkRelease(displayLink);
        CGDisplayModeRelease(mode);

        display.name = get_display_name(displayID);

        displayList.Push(display);
    }
}

void defos_get_display_modes(DisplayID displayID_, dmArray<DisplayModeInfo> &modeList) {
    CGDirectDisplayID displayID = (CGDirectDisplayID)(size_t)displayID_;

    NSDictionary *optDict = [[NSDictionary alloc] initWithObjectsAndKeys:
        [NSNumber numberWithBool: YES], kCGDisplayShowDuplicateLowResolutionModes, nil
    ];
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(displayID, (__bridge CFDictionaryRef)optDict);
    [optDict release];

    // Prepend the current display mode
    int modeCount = CFArrayGetCount(modes) + 1;
    CGDisplayModeRef* allModes = new CGDisplayModeRef[modeCount];
    allModes[0] = CGDisplayCopyDisplayMode(displayID);
    for (int i = 1; i < modeCount; i++) {
        allModes[i] = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i - 1);
    }

    // Make a display link for refresh rate detection fallback
    CVDisplayLinkRef displayLink;
    CVDisplayLinkCreateWithCGDisplay(displayID, &displayLink);

    double rotation = CGDisplayRotation(displayID);

    modeList.OffsetCapacity(modeCount);
    for (int i = 0; i < modeCount; i++) {
        CGDisplayModeRef mode = allModes[i];
        DisplayModeInfo modeInfo = parse_mode(mode, displayLink, rotation);

        // Remove duplicates
        size_t width = CGDisplayModeGetWidth(mode);
        size_t height = CGDisplayModeGetHeight(mode);
        size_t pixelWidth = modeInfo.width;
        size_t pixelHeight = modeInfo.height;
        double refreshRate = CGDisplayModeGetRefreshRate(mode);
        CFStringRef pixelEncoding = CGDisplayModeCopyPixelEncoding(mode);

        bool shouldAdd = true;
        for (int j = 0; j < i; j++) {
            CGDisplayModeRef otherMode = allModes[j];
            if (CFEqual(mode, otherMode)) {
                shouldAdd = false;
                break;
            }

            size_t otherWidth = CGDisplayModeGetWidth(otherMode);
            size_t otherHeight = CGDisplayModeGetHeight(otherMode);
            size_t otherPixelWidth = CGDisplayModeGetPixelWidth(otherMode);
            size_t otherPixelHeight = CGDisplayModeGetPixelHeight(otherMode);
            double otherRefreshRate = CGDisplayModeGetRefreshRate(otherMode);
            CFStringRef otherPixelEncoding = CGDisplayModeCopyPixelEncoding(otherMode);

            bool samePixelEncoding = (CFStringCompare(pixelEncoding, otherPixelEncoding, 0) == kCFCompareEqualTo);
            CFRelease(pixelEncoding);
            CFRelease(otherPixelEncoding);

            if (samePixelEncoding
                && pixelWidth == otherPixelWidth
                && pixelHeight == otherPixelHeight
                && width == otherWidth
                && height == otherHeight
                && refreshRate == otherRefreshRate
            ) {
                shouldAdd = false;
                break;
            }
        }

        if (shouldAdd) { modeList.Push(modeInfo); }
    }

    CVDisplayLinkRelease(displayLink);
    CGDisplayModeRelease(allModes[0]);
    CFRelease(modes);
}

DisplayID defos_get_current_display() {
    return (void*)(size_t)[[[[window screen] deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
}

@interface DefOSMouseTracker : NSResponder
@end
@implementation DefOSMouseTracker
- (void)mouseEntered:(NSEvent *)event {
    is_mouse_in_view = true;
    if (!is_cursor_clipped) {
        defos_emit_event(DEFOS_EVENT_MOUSE_ENTER);
        if (!is_cursor_visible) {
            [NSCursor unhide]; // Work around a macOS bug?
            [NSCursor hide];
        }
    }
}
- (void)mouseExited:(NSEvent *)event {
    if (is_cursor_clipped) {
        clip_cursor(event);
    } else {
        is_mouse_in_view = false;
        defos_emit_event(DEFOS_EVENT_MOUSE_LEAVE);
    }
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
