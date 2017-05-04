local M = {}

local ffi = package.preload.ffi()

--[[
http://luajit.org/ext_ffi_api.html

according to the document, we do not need to load user32.dll manually:
"On Windows systems, .... C runtime library LuaJIT was linked with (msvcrt*.dll), kernel32.dll, user32.dll and gdi32.dll.""
]]--
local C = ffi.C

if ffi.os == "Windows" then
	-- add definitions here,
	-- to make it clear and avoid re-define exception (for struct) when calling a method more than 1 time
	ffi.cdef([[

			typedef long LONG;
    	typedef int BOOL;
    	typedef unsigned long DWORD;
    	typedef long long LONG_PTR;
    	typedef void *PVOID;
    	typedef unsigned int UINT;


    	typedef PVOID HANDLE;
    	typedef HANDLE HICON;
    	typedef HANDLE HWND;
    	typedef HICON HCURSOR;

    	typedef struct tagPOINT
    	{
    		LONG  x;
    		LONG  y;
    	} POINT, *PPOINT, *NPPOINT, *LPPOINT;


    	typedef struct {
    		DWORD   cbSize;
    		DWORD   flags;
    		HCURSOR hCursor;
    		POINT   ptScreenPos;
    	} CURSORINFO, *PCURSORINFO, *LPCURSORINFO;

			typedef struct tagRECT
			{
			    LONG    left;
			    LONG    top;
			    LONG    right;
			    LONG    bottom;
			} RECT, *PRECT, *NPRECT, *LPRECT;


			BOOL GetCursorInfo(PCURSORINFO pci);

			int  GetSystemMetrics(int nIndex);

			HWND GetActiveWindow();

			BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);

			BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);

			int ShowCursor(BOOL bShow);

			HWND GetActiveWindow();

			LONG GetWindowLongPtrA(HWND hWnd, int nIndex);

			LONG SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
	]])
end

-- TODO: a rough way to map cursor position to window coordinate
local function range_to_window(x, y)
	local rect = ffi.new("RECT")
	local hwnd = C.GetActiveWindow()
	local result = C.GetWindowRect(hwnd, rect);

	-- TODO: fail?
	local x_in_window = x - rect.left
	if x <= rect.left then
		x_in_window = 0
	elseif x >= rect.right then
		x_in_window = rect.right - rect.left
	end

	local y_in_window = y - rect.top
	if y <= rect.top then
		y_in_window = 0
	elseif y >= rect.bottom then
		y_in_window = rect.bottom - rect.top
	end

	-- now we have the offset in window, but we have to match to the defold coordinate... as the left-bottom is the (0, 0)
	-- NOTE: now there is a precision issue, since the GetWindowRect returned width in pixel, any better way?
	return vmath.vector3(x_in_window, rect.bottom - rect.top - y_in_window, 1)
end


-- https://github.com/glfw/glfw-legacy/tree/master/lib
-- https://github.com/luapower/winapi/blob/master/winapi/window.lua
--http://www.glfw.org/GLFWReference27.pdf
function M.get_mouse_pos()
	-- definitions

    --ffi.cdef[[
    --void glfwEnable(int token);
    --]]
    --ffi.C.glfwEnable(0x00030001) --GLFW_MOUSE_CURSOR


		-- NOTE: not sure about the performance to get it per update

		local pci = ffi.new("CURSORINFO")

		-- we have to set the size or we canont get the result
		pci.cbSize = ffi.sizeof("CURSORINFO")

		local result = C.GetCursorInfo(pci)

		-- TODO: what if the result is not 0? return nil?
		-- we make the z as 1 so we always on top of others
		return range_to_window(pci.ptScreenPos.x, pci.ptScreenPos.y)
end



function M.set_window_size(pos_x, pos_y, width, height)
	if ffi.os == "Windows" then

		local HWND_TOP = 0
		local SWP_NOMOVE = 0x0002
		local SWP_NOZORDER = 0x0004
		local SM_CXSCREEN = 0
		local SM_CYSCREEN = 1


		local ptr = C.GetActiveWindow()

		if pos_x == -1 then
			local x_pos = (C.GetSystemMetrics(SM_CXSCREEN) - width) / 2
			local y_pos = (C.GetSystemMetrics(SM_CYSCREEN) - height) / 2
			C.SetWindowPos(ptr, ptr, x_pos, y_pos, width, height, SWP_NOZORDER)
		else
			C.SetWindowPos(ptr, ptr, pos_x, pos_y, width, height, SWP_NOZORDER)
		end
	end
end

function M.disable_mouse_cursor()
	if ffi.os == "Windows" then
		C.ShowCursor(false)
	end
end

function M.enable_mouse_cursor()
	if ffi.os == "Windows" then
		C.ShowCursor(true)
	end
end

function M.disable_window_resize()
	if ffi.os == "Windows" then

		local GWL_STYLE = -16

		-- styles from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
		local WS_SIZEBOX = 0x00040000


		local ptr = C.GetActiveWindow()
		local value = C.GetWindowLongPtrA(ptr, GWL_STYLE)

		C.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, bit.bnot(WS_SIZEBOX)))
	end
end

function M.disable_maximize_button()
	if ffi.os == "Windows" then

		local GWL_STYLE = -16

		-- styles from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
		local WS_MAXIMIZEBOX = 0x00010000

		local ptr = C.GetActiveWindow()
		local value = C.GetWindowLongPtrA(ptr, GWL_STYLE)

		C.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, bit.bnot(WS_MAXIMIZEBOX)))
	end
end

function M.disable_minimize_button()
	if(ffi.os == "Windows") then

		local GWL_STYLE = -16

		-- styles from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
		local WS_MINIMIZEBOX = 0x00020000

		local ptr = C.GetActiveWindow()
		local value = C.GetWindowLongPtrA(ptr, GWL_STYLE)

		C.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, bit.bnot(WS_MINIMIZEBOX)))
	end
end

return M
