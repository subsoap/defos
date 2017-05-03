local M = {}

local ffi = package.preload.ffi()


-- https://github.com/ffi/ffi/wiki/Types
-- https://github.com/luapower/winapi/blob/master/winapi/window.lua
--http://www.glfw.org/GLFWReference27.pdf


function M.get_mouse_pos()
	-- definitions
    ffi.cdef[[
    void glfwEnable(int token);
    ]]
    --ffi.C.glfwEnable(0x00030001) --GLFW_MOUSE_CURSOR
end


function M.set_window_size(pos_x, pos_y, width, height)
	if(ffi.os == "Windows") then

	  -- definitions
	  ffi.cdef([[
			typedef unsigned int LONG;
			typedef long long LONG_PTR;
			typedef void* PVOID;
			typedef PVOID HANDLE;
			typedef HANDLE HWND;
			typedef unsigned int UINT;
			typedef bool BOOL;

			int  GetSystemMetrics(int nIndex);
			HWND GetActiveWindow();
			BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
			
		]])

		-- load User32.dll
		local user32 = ffi.load("User32")
		--local glfw2 = ffi.load("glfw2")
		
		local HWND_TOP = 0
		local SWP_NOMOVE = 0x0002
		local SWP_NOZORDER = 0x0004
		local SM_CXSCREEN = 0
		local SM_CYSCREEN = 1
		
		
		local ptr = user32.GetActiveWindow()
		
		if pos_x == -1 then 
			local x_pos = (user32.GetSystemMetrics(SM_CXSCREEN) - width) / 2
			local y_pos = (user32.GetSystemMetrics(SM_CYSCREEN) - height) / 2
			user32.SetWindowPos(ptr, ptr, x_pos, y_pos, width, height, SWP_NOZORDER)
		else
			user32.SetWindowPos(ptr, ptr, pos_x, pos_y, width, height, SWP_NOZORDER)
		end
	end	
end	

function M.disable_mouse_cursor()
	if(ffi.os == "Windows") then

	  -- definitions
	  ffi.cdef([[
			typedef unsigned int LONG;
			typedef long long LONG_PTR;
			typedef void* PVOID;
			typedef PVOID HANDLE;
			typedef HANDLE HWND;
			typedef bool BOOL;

			int ShowCursor(BOOL bShow);
		]])

		-- load User32.dll
		local user32 = ffi.load("User32")

		user32.ShowCursor(false)
	end	
end

function M.enable_mouse_cursor()
	if(ffi.os == "Windows") then

	  -- definitions
	  ffi.cdef([[
			typedef unsigned int LONG;
			typedef long long LONG_PTR;
			typedef void* PVOID;
			typedef PVOID HANDLE;
			typedef HANDLE HWND;
			typedef bool BOOL;

			int ShowCursor(BOOL bShow);
		]])

		-- load User32.dll
		local user32 = ffi.load("User32")

		user32.ShowCursor(true)
	end	
end	

function M.disable_window_resize()
	if(ffi.os == "Windows") then

	  -- definitions
	  ffi.cdef([[
			typedef unsigned int LONG;
			typedef long long LONG_PTR;
			typedef void* PVOID;
			typedef PVOID HANDLE;
			typedef HANDLE HWND;

			HWND GetActiveWindow();
			LONG GetWindowLongPtrA(HWND hWnd, int nIndex);
			LONG SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
		]])

		local GWL_STYLE = -16

		-- styles from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
		local WS_SIZEBOX = 0x00040000

		-- load User32.dll
		local user32 = ffi.load("User32")

		local ptr = user32.GetActiveWindow()
		local value = user32.GetWindowLongPtrA(ptr, GWL_STYLE)
		user32.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, bit.bnot(WS_SIZEBOX)))
	end	
end	

function M.disable_maximize_button()
	if(ffi.os == "Windows") then

	  -- definitions
	  ffi.cdef([[
			typedef unsigned int LONG;
			typedef long long LONG_PTR;
			typedef void* PVOID;
			typedef PVOID HANDLE;
			typedef HANDLE HWND;

			HWND GetActiveWindow();
			LONG GetWindowLongPtrA(HWND hWnd, int nIndex);
			LONG SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
		]])

		local GWL_STYLE = -16

		-- styles from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
		local WS_MAXIMIZEBOX = 0x00010000

		-- load User32.dll
		local user32 = ffi.load("User32")

		local ptr = user32.GetActiveWindow()
		local value = user32.GetWindowLongPtrA(ptr, GWL_STYLE)
		user32.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, bit.bnot(WS_MAXIMIZEBOX)))
	end
end

function M.disable_minimize_button()
	if(ffi.os == "Windows") then

	  -- definitions
	  ffi.cdef([[
			typedef unsigned int LONG;
			typedef long long LONG_PTR;
			typedef void* PVOID;
			typedef PVOID HANDLE;
			typedef HANDLE HWND;

			HWND GetActiveWindow();
			LONG GetWindowLongPtrA(HWND hWnd, int nIndex);
			LONG SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
		]])

		local GWL_STYLE = -16

		-- styles from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
		local WS_MINIMIZEBOX = 0x00020000

		-- load User32.dll
		local user32 = ffi.load("User32")

		local ptr = user32.GetActiveWindow()
		local value = user32.GetWindowLongPtrA(ptr, GWL_STYLE)
		user32.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, bit.bnot(WS_MINIMIZEBOX)))
	end
end	

return M
