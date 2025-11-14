--[[
DefOS API documentation
Extra native OS functions for games written using the Defold game engine
--]]

---@meta
---@diagnostic disable: lowercase-global
---@diagnostic disable: missing-return
---@diagnostic disable: duplicate-doc-param
---@diagnostic disable: duplicate-set-field
---@diagnostic disable: args-after-dots

---@class defold_api.defos
defos = {}

---@alias defos.CURSOR
---|`defos.CURSOR_ARROW`
---|`defos.CURSOR_HAND`
---|`defos.CURSOR_CROSSHAIR`
---|`defos.CURSOR_IBEAM`

---Default arrow cursor
---@type integer
defos.CURSOR_ARROW = nil

---Hand cursor
---@type integer
defos.CURSOR_HAND = nil

---Crosshair cursor
---@type integer
defos.CURSOR_CROSSHAIR = nil

---I-beam cursor
---@type integer
defos.CURSOR_IBEAM = nil

---The system path separator ("\\" on Windows, "/" everywhere else)
defos.PATH_SEP = ""

---@class defos.display_info
---@field id userdata
---@field bounds { x: integer, y: integer, width: integer, height: integer } This is the position and size in screen coordinates of the display (relative to the top-left corner of the main display)
---@field mode defos.display_mode The current resolution mode of the display
---@field name string

---@class defos.display_mode
---@field width integer Full width in pixels (not points)
---@field height integer Full height in pixels (not points)
---@field scaling_factor number Hi-DPI scaling factor
---@field refresh_rate integer Refresh rate, for example 60
---@field bits_per_pixel integer Bits per pixel, for example 32
---@field orientation integer One of 0, 90, 180, 270 (degrees measured clockwise)
---@field reflect_x boolean Linux supports reflecting either of the axes, effectively flipping the image like a mirror
---@field reflect_y boolean Linux supports reflecting either of the axes, effectively flipping the image like a mirror

---@class defos.macos_cursor
---@field image userdata Buffer loaded by resource.load("cursor.tiff")
---@field hot_spot_x integer An anchor point X within the image that will overlap with the functional position of the mouse pointer (eg. the tip of the arrow)
---@field hot_spot_y integer An anchor point Y within the image that will overlap with the functional position of the mouse pointer (eg. the tip of the arrow)

---Disables the maximize button in the window title bar. Platforms - Linux, Windows, OSX.
function defos.disable_maximize_button() end

---Disables the minimize button in the window title bar. Platforms - Linux, Windows, OSX.
function defos.disable_minimize_button() end

---Disables window resizing. Platforms - Linux, Windows, OSX.
function defos.disable_window_resize() end

---Sets the window title. Platforms - Linux, Windows, OSX, HTML5.
---@param title string The new window title
function defos.set_window_title(title) end

---Sets the window maximized state. Platforms - Linux, Windows, OSX, HTML5.
---@param maximized boolean Whether to maximize the window
function defos.set_maximized(maximized) end

---Toggles the window maximized state. Platforms - Linux, Windows, OSX, HTML5.
function defos.toggle_maximized() end

---Checks if the window is maximized. Platforms - Linux, Windows, OSX, HTML5.
---@return boolean maximized
function defos.is_maximized() end

---Sets the window fullscreen state. On HTML5, this only works from defos.on_click() or defos.on_interaction(). Platforms - Linux, Windows, OSX, HTML5.
---@param fullscreen boolean Whether to set fullscreen mode
function defos.set_fullscreen(fullscreen) end

---Toggles the window fullscreen state. Platforms - Linux, Windows, OSX, HTML5.
function defos.toggle_fullscreen() end

---Checks if the window is in fullscreen mode. Platforms - Linux, Windows, OSX, HTML5.
---@return boolean fullscreen
function defos.is_fullscreen() end

---Sets the window borderless state. Platforms - Windows.
---@param borderless boolean Whether to set borderless mode
function defos.set_borderless(borderless) end

---Toggles the window borderless state. Platforms - Windows.
function defos.toggle_borderless() end

---Checks if the window is borderless. Platforms - Windows.
---@return boolean borderless
function defos.is_borderless() end

---Sets whether the window should always stay on top. Platforms - Linux, Windows, OSX.
---@param always_on_top boolean Whether to keep window on top
function defos.set_always_on_top(always_on_top) end

---Toggles whether the window should always stay on top. Platforms - Linux, Windows, OSX.
function defos.toggle_always_on_top() end

---Checks if the window is set to always stay on top. Platforms - Linux, Windows, OSX.
---@return boolean always_on_top
function defos.is_always_on_top() end

---Minimizes the window. Platforms - Linux, Windows, OSX.
function defos.minimize() end

---Activates (focuses) the window. Platforms - Linux, Windows, OSX.
function defos.activate() end

---Gets the window's size and position in screen coordinates. Platforms - Linux, Windows, OSX, HTML5.
---@return number x X position
---@return number y Y position
---@return number width Window width
---@return number height Window height
function defos.get_window_size() end

---Sets the window's size and position in screen coordinates. Passing nil for x and y will center the window. Platforms - Linux, Windows, OSX, HTML5.
---@param x? number X position
---@param y? number Y position
---@param width number Window width
---@param height number Window height
function defos.set_window_size(x, y, width, height) end

---Gets the game view size and position in screen coordinates. Platforms - Linux, Windows, OSX, HTML5.
---@return number x X position
---@return number y Y position
---@return number width View width
---@return number height View height
function defos.get_view_size() end

---Sets the game view size and position in screen coordinates. Passing nil for x and y will center the window. Platforms - Linux, Windows, OSX, HTML5.
---@param x? number X position
---@param y? number Y position
---@param width number View width
---@param height number View height
function defos.set_view_size(x, y, width, height) end

---Returns information about all displays. Platforms - Linux, Windows, OSX.
---@return table<userdata|integer, defos.display_info> displays Table of display information, indexed by number or display id
function defos.get_displays() end

---Gets the ID of the game's current display. Platforms - Linux, Windows, OSX.
---@return userdata display_id The current display ID
function defos.get_current_display_id() end

---Gets all supported resolution modes for a display. Platforms - Linux, Windows, OSX.
---@param display_id userdata The display ID to get modes for
---@return defos.display_mode[] modes Table of supported resolution modes
function defos.get_display_modes(display_id) end

---Sets the mouse cursor visibility. Platforms - Linux, Windows, OSX, HTML5.
---@param visible boolean Whether to show the cursor
function defos.set_cursor_visible(visible) end

---Checks if the mouse cursor is visible. Platforms - Linux, Windows, OSX, HTML5.
---@return boolean visible
function defos.is_cursor_visible() end

---Checks if the mouse is within the game view area. Platforms - Linux, Windows, OSX, HTML5.
---@return boolean in_view
function defos.is_mouse_in_view() end

---Sets a callback for when the mouse enters the game view area. Platforms - Linux, Windows, OSX, HTML5.
---@param callback? fun() Function to be called when mouse enters
function defos.on_mouse_enter(callback) end

---Sets a callback for when the mouse leaves the game view area. Platforms - Linux, Windows, OSX, HTML5.
---@param callback? fun() Function to be called when mouse leaves
function defos.on_mouse_leave(callback) end

---Gets the cursor position in screen coordinates. Platforms - Linux, Windows, OSX, HTML5.
---@return number x X position
---@return number y Y position
function defos.get_cursor_pos() end

---Gets the cursor position in game view coordinates. Platforms - Linux, Windows, OSX, HTML5.
---@return number x X position
---@return number y Y position
function defos.get_cursor_pos_view() end

---Sets the cursor position in screen coordinates. Platforms - Linux, Windows, OSX.
---@param x number X position
---@param y number Y position
function defos.set_cursor_pos(x, y) end

---Sets the cursor position in game view coordinates. Platforms - Linux, Windows, OSX.
---@param x number X position
---@param y number Y position
function defos.set_cursor_pos_view(x, y) end

---Clips cursor to current game view area. Platforms - Windows, OSX.
---@param clipped boolean Whether to clip the cursor
function defos.set_cursor_clipped(clipped) end

---Checks if the cursor is clipped to the game view area. Platforms - Linux, Windows, OSX.
---@return boolean clipped
function defos.is_cursor_clipped() end

---Locks cursor movement. On HTML5 this only works from defos.on_click() or defos.on_interaction(). Platforms - Windows, OSX, HTML5.
---@param locked boolean Whether to lock the cursor
function defos.set_cursor_locked(locked) end

---Checks if the cursor is locked. Platforms - Windows, OSX, HTML5.
---@return boolean locked
function defos.is_cursor_locked() end

---Sets a callback for when cursor lock is disabled (e.g. when user presses ESC on HTML5). Platforms - Windows, OSX, HTML5.
---@param callback? fun() Function to be called when cursor lock is disabled
function defos.on_cursor_lock_disabled(callback) end

---Loads a custom hardware cursor. See README for supported formats per platform. Platforms - Linux, Windows, OSX, HTML5.
---@param cursor_data string|defos.macos_cursor Cursor data in platform-specific format
---@return userdata cursor The loaded cursor
function defos.load_cursor(cursor_data) end

---Sets the current cursor. See README for supported values. Platforms - Linux, Windows, OSX, HTML5.
---@param cursor? defos.CURSOR|userdata|string|defos.macos_cursor The cursor to set
function defos.set_cursor(cursor) end

---Resets the cursor to default. Platforms - Linux, Windows, OSX, HTML5.
function defos.reset_cursor() end

---Shows/hides the console window. Platforms - Windows.
---@param visible boolean Whether to show the console
function defos.set_console_visible(visible) end

---Checks if the console window is visible. Platforms - Windows.
---@return boolean visible
function defos.is_console_visible() end

---Sets a callback for user interaction with the canvas. Platforms - HTML5.
---@param callback? fun() Function to be called on interaction
function defos.on_interaction(callback) end

---Sets a callback for user click on the canvas. Platforms - HTML5.
---@param callback? fun() Function to be called on click
function defos.on_click(callback) end

---Gets the absolute path to the game's containing directory. Platforms - Linux, Windows, OSX, HTML5.
---@return string path The bundle root path
function defos.get_bundle_root() end

---Changes the game window's icon at runtime. Platforms - Windows, OSX, HTML5.
---@param path string Path to the icon file
function defos.set_window_icon(path) end

---Returns command line arguments used to run the app. Platforms - Linux, Windows, OSX, HTML5.
---@return table arguments Table of command line arguments
function defos.get_arguments() end