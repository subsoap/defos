# DefOS
Extra native OS functions for games written using the Defold game engine

Currently uses FFI for Windows and Native Extension for macOS, but when Native Extensions are released for all platforms will switch to that for all targets. Contribute!

## Installation
You can use DefOS in your own project by adding this project as a [Defold library dependency](http://www.defold.com/manuals/libraries/). Open your game.project file and in the dependencies field under project add:

	https://github.com/subsoap/defos/archive/master.zip
### Windows
Once added, you must require the main Lua module via
```
local defos = require("defos.defos")
```
This method is used only on Windows. macOS uses Native Extensions which will allow you to use defos methods globally.

By using the dependency version, you can more easily update the module, but you may wish to keep it static. You can also download the defos.lua module file and add it to a utils folder. Then you would require via

```
local defos = require("utils.defos")
```

### macOS
Follow the steps in Installation to add this project as a library dependency. Then you can use the "defos" module globablly. You should not require the Lua module like is done in Windows steps.

## Methods

Use DefOS module methods via

```
function init(self)
	defos.disable_maximize_button()
	defos.disable_minimize_button()
	defos.disable_window_resize()
	defos.disable_mouse_cursor()
	defos.enable_mouse_cursor()
	defos.set_window_size(-1,-1,800,600)
	defos.set_window_title("I set this title using Defos")
	defos.toggle_fullscreen()
	defos.isFullScreen()
end
```
Use issues for feature requests.
