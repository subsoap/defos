-- use the native extension version if it exists
if _G.defos then
	return _G.defos
end

local M = {}

local ffi = package.preload.ffi()

--[[
http://luajit.org/ext_ffi_api.html

according to the document, we do not need to load user32.dll manually:
"On Windows systems, .... C runtime library LuaJIT was linked with (msvcrt*.dll), kernel32.dll, user32.dll and gdi32.dll.""
]]--
local C = ffi.C

if ffi.os == "Windows" then
	local GWL_STYLE = -16
	local WS_POPUP = 0x80000000
	local WS_CAPTION = 0x00C00000
	local WS_SYSMENU = 0x00080000
	local WS_SIZEBOX = 0x00040000
	local WS_MAXIMIZEBOX = 0x00010000
	local WS_MINIMIZEBOX = 0x00020000
	local SWP_NOZORDER = 0x0004
	local SW_MAXIMIZE = 3

	-- add definitions here,
	-- to make it clear and avoid re-define exception (for struct) when calling a method more than 1 time
	ffi.cdef([[

		typedef long LONG;
		typedef int BOOL;
		typedef unsigned long DWORD;
		typedef long long LONG_PTR;
		typedef void *PVOID;
		typedef unsigned int UINT;
		typedef wchar_t WCHAR;
		typedef unsigned char BYTE;
		typedef const char* LPCSTR;
		typedef wchar_t* LPWSTR;
		typedef unsigned short      WORD;
		typedef const WCHAR *LPCWSTR, *PCWSTR;
		typedef PVOID HANDLE;
		typedef HANDLE HICON;
		typedef HANDLE HWND;
		typedef HICON HCURSOR;
		typedef BYTE BCHAR;

		static const int	CCHDEVICENAME = 32;
		static const int 	CCHFORMNAME = 32;

		typedef struct tagRECT
		{
			LONG    left;
			LONG    top;
			LONG    right;
			LONG    bottom;
		} RECT, *PRECT, *NPRECT, *LPRECT;

		typedef struct tagPOINT
		{
			LONG  x;
			LONG  y;
		} POINT, *PPOINT, *NPPOINT, *LPPOINT;

		typedef struct _POINTL
		{
			LONG  x;
			LONG  y;
		} POINTL, *PPOINTL;

		typedef struct HMONITOR__
		{
			int unused;
		} HMONITOR, *HMONITOR;

		typedef struct tagMONITORINFO
		{
			DWORD   cbSize;
			RECT    rcMonitor;
			RECT    rcWork;
			DWORD   dwFlags;
		} MONITORINFO, *LPMONITORINFO;

		typedef struct {
			DWORD   cbSize;
			DWORD   flags;
			HCURSOR hCursor;
			POINT   ptScreenPos;
		} CURSORINFO, *PCURSORINFO, *LPCURSORINFO;

		typedef struct _devicemode {
			BCHAR  dmDeviceName[CCHDEVICENAME];
			WORD   dmSpecVersion;
			WORD   dmDriverVersion;
			WORD   dmSize;
			WORD   dmDriverExtra;
			DWORD  dmFields;
			union {
				struct {
					short dmOrientation;
					short dmPaperSize;
					short dmPaperLength;
					short dmPaperWidth;
					short dmScale;
					short dmCopies;
					short dmDefaultSource;
					short dmPrintQuality;
				};
				POINTL dmPosition;
				DWORD  dmDisplayOrientation;
				DWORD  dmDisplayFixedOutput;
			};

			short  dmColor;
			short  dmDuplex;
			short  dmYResolution;
			short  dmTTOption;
			short  dmCollate;
			BYTE  dmFormName[CCHFORMNAME];
			WORD  dmLogPixels;
			DWORD  dmBitsPerPel;
			DWORD  dmPelsWidth;
			DWORD  dmPelsHeight;
			union {
				DWORD  dmDisplayFlags;
				DWORD  dmNup;
			};
			DWORD  dmDisplayFrequency;
			DWORD  dmICMMethod;
			DWORD  dmICMIntent;
			DWORD  dmMediaType;
			DWORD  dmDitherType;
			DWORD  dmReserved1;
			DWORD  dmReserved2;
			DWORD  dmPanningWidth;
			DWORD  dmPanningHeight;
		} DEVMODE, *PDEVMODE;

		typedef struct tagWINDOWPLACEMENT {
			UINT  length;
			UINT  flags;
			UINT  showCmd;
			POINT ptMinPosition;
			POINT ptMaxPosition;
			RECT  rcNormalPosition;
		} WINDOWPLACEMENT, *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;



		int  GetSystemMetrics(int nIndex);

		HWND GetActiveWindow();

		BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);

		BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);

		int ShowCursor(BOOL bShow);

		BOOL GetCursorInfo(PCURSORINFO pci);

		HWND GetActiveWindow();

		LONG GetWindowLongPtrA(HWND hWnd, int nIndex);

		LONG SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);

		BOOL SetWindowTextW(HWND hWnd,LPCWSTR lpString);

		HMONITOR MonitorFromWindow(HWND  hwnd,DWORD dwFlags);

		BOOL GetMonitorInfoW(HMONITOR hMonitor, LPMONITORINFO lpmi);

		BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);

		HWND GetDesktopWindow(void);

		BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT *lpwndpl);

		BOOL SetWindowPlacement(HWND hWnd,const WINDOWPLACEMENT *lpwndpl);

		BOOL ShowWindow(HWND hWnd,int  nCmdShow);
	]])


	-- get wchar_t string on Windows to support unicode
	-- @string  s lua string
	-- @return first true or false
	-- @return second nil if failed, of wchar_t string
	-- @usage local ws=get_wstring("a test string")
	-- https://github.com/Youka/Yutils/blob/master/lua/Yutils/string.lua
	local function get_wstring(s)
		local kernel32 = ffi.load("kernel32")

		local CP_UTF8 = 65001	-- No static values in C definitions to avoid ffi override errors

		ffi.cdef([[
			int MultiByteToWideChar(UINT, DWORD, LPCSTR, int, LPWSTR, int);
		]])

		local wlen = kernel32.MultiByteToWideChar(CP_UTF8, 0x0, s, -1, nil, 0)

		if wlen > 0 then
				local ws = ffi.new("wchar_t[?]", wlen)

				if kernel32.MultiByteToWideChar(CP_UTF8, 0x0, s, -1, ws, wlen) > 0 then
					return ws
				end
		end

		return nil
	end

	-- set window style (not EXT_StYLE) using SetWindowLongPtrA on Windows
	-- @number window style from https://msdn.microsoft.com/en-us/library/windows/desktop/ms632600(v=vs.85).aspx
	-- @usage set_window_style(bit.band(bit.bnot(WS_MAXIMIZEBOX), bit.bnot(WS_MINIMIZEBOX))) or set_window_style(bit.bnot(WS_SIZEBOX))
	-- @return true if success to change style, or false
	local function set_window_style(style)
		local ptr = C.GetActiveWindow()
		local value = C.GetWindowLongPtrA(ptr, GWL_STYLE)

		return C.SetWindowLongPtrA(ptr, GWL_STYLE, bit.band(value, style)) ~= 0
	end

	-- get the nearest (always the monitor window in) monitor info
	-- @return nil if fail, or MONITORINFO struct
	local function get_monitor_info()
		--https://blogs.msdn.microsoft.com/oldnewthing/20050505-04/?p=35703/

		--local MONITOR_DEFAULTTONULL    =   0x00000000 -- Returns NULL.
		--local MONITOR_DEFAULTTOPRIMARY =   0x00000001 -- Returns a handle to the primary display monitor.
		local MONITOR_DEFAULTTONEAREST =   0x00000002 -- Returns a handle to the display monitor that is nearest to the window.

		local hwnd = C.GetActiveWindow()
		local monitor = C.MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
		local mi = ffi.new("MONITORINFO")

		mi.cbSize = ffi.sizeof("MONITORINFO")

		local success = C.GetMonitorInfoW(monitor, mi)

		return (success and mi) or nil
	end

	-- Retrieves the show state and the restored, minimized, and maximized positions of window.
	-- @return WINDOWPLACEMENT if success, or nil
	local function get_window_placement(hwnd)
		local placement = ffi.new("WINDOWPLACEMENT")
		placement.length = ffi.sizeof("WINDOWPLACEMENT")

		local successed = C.GetWindowPlacement(hwnd, placement)

		return (successed > 0 and placement) or nil
	end

	-- set the window placement, always used to restore window state
	local function set_window_placement(hwnd, placement)
		if hwnd and placement then
			C.SetWindowPlacement(hwnd, placement)
		end
	end

	local is_maximize = false
	local is_fullscreen = false;
	local previous_state = {style = nil, placement=nil}

	function M.is_maximize()
		return is_maximize
	end

	function M.toggle_maximize()
		if is_fullscreen then
			M.toggle_fullscreen()
		end

		local hwnd = C.GetActiveWindow()

		if is_maximize then
			set_window_placement(hwnd, previous_state.placement)
			is_maximize = false
		else
			previous_state.placement = get_window_placement(hwnd)
			C.ShowWindow(hwnd, SW_MAXIMIZE)
			is_maximize = true
		end
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

		-- NOTE: try to get position with win32 api here, but not sure about the performance to get it per update
		local pci = ffi.new("CURSORINFO")
		-- we have to set the size or we canont get the result
		pci.cbSize = ffi.sizeof("CURSORINFO")

		local result = C.GetCursorInfo(pci)

		return {x = pci.ptScreenPos.x, y = pci.ptScreenPos.y}
 end

	function M.is_fullscreen()
		return is_fullscreen
	end

	function M.toggle_fullscreen()

		if is_maximize then
			-- if itis maximized, then we need to restore it then toggle fullscreen
			M.toggle_maximize()
		end

		local hwnd = C.GetActiveWindow()

		if not is_fullscreen then
			-- TODO: exception handle?
			previous_state.style = C.GetWindowLongPtrA(hwnd, GWL_STYLE)
			previous_state.placement = get_window_placement(hwnd)

			-- get desktop rect, or we can use get_monitor_info?
			local rc = ffi.new("RECT")
			local dhwnd = C.GetDesktopWindow()
			C.GetWindowRect(dhwnd, rc)

			-- for fullscreen, we remove these styles
			local windowed_fullscreen_style = bit.band(
				bit.bnot(WS_CAPTION),
				bit.bnot(WS_SYSMENU),
				bit.bnot(WS_MINIMIZEBOX),
				bit.bnot(WS_MAXIMIZEBOX),
				bit.bnot(WS_SIZEBOX))

			if set_window_style(windowed_fullscreen_style) then
				if C.SetWindowPos(hwnd, nil, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER) then
					-- we are in fullscreen mode
					is_fullscreen = true
				end
			end
		else
			-- TODO: exception handle?
			-- restore the style and position/size
			C.SetWindowLongPtrA(hwnd, GWL_STYLE, previous_state.style)
			set_window_placement(hwnd, previous_state.placement)

			is_fullscreen = false
		end
	end

	function M.set_window_size(pos_x, pos_y, width, height)
		--local HWND_TOP = 0
		--local SWP_NOMOVE = 0x0002
		--local SWP_NOZORDER = 0x0004
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


	function M.disable_mouse_cursor()
		C.ShowCursor(false)
	end


	function M.enable_mouse_cursor()
		C.ShowCursor(true)
	end


	function M.disable_window_resize()
		set_window_style(bit.bnot(WS_SIZEBOX))
	end


	function M.disable_maximize_button()
		set_window_style(bit.bnot(WS_MAXIMIZEBOX))
	end


	function M.disable_minimize_button()
		set_window_style(bit.bnot(WS_MINIMIZEBOX))
	end


	function M.set_window_title(title)
		local wtitle = get_wstring(title)

		if wtitle then
			local hwnd = C.GetActiveWindow()
			C.SetWindowTextW(hwnd, wtitle)
		end
	end
end

return M
