#include <dmsdk/sdk.h>
#include "defos_private.h"
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_WINDOWS)

#include <atlbase.h>
#include <atlconv.h>
#include <WinUser.h>

// keep track of window placement when going to/from fullscreen or maximized
WINDOWPLACEMENT placement = {sizeof(placement)};

// used to check if mouse tracking enabled
bool is_mouse_tracking = false;

// original wndproc pointer
WNDPROC originalProc;

// callback struct
struct LuaCallbackInfo
{
	LuaCallbackInfo() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
	lua_State *m_L;
	int m_Callback;
	int m_Self;
};

// tracking if mouse event already hooked
bool mouse_event_hooked = false;
// callback of mouse state
LuaCallbackInfo mouseStateCb;

// forward declarations
bool set_window_style(LONG_PTR style);
LONG_PTR get_window_style();
bool enable_mouse_tracking();
int __stdcall custom_wndproc(HWND hwnd, UINT umsg, WPARAM wp, LPARAM lp);
void register_callback(lua_State *L, int index, LuaCallbackInfo *cbk);
void unregister_callback(LuaCallbackInfo *cbk);
void invoke_mouse_state(int state);

/******************
 * exposed functions
 ******************/

void defos_init() {}
void defos_final() {}

// subclass the window
bool defos_enable_subclass_window()
{
	// check if we already subclass the window
	if (originalProc != NULL)
	{
		return true; // or false?
	}

	HWND window = dmGraphics::GetNativeWindowsHWND();

	originalProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)&custom_wndproc); // keep original proc

	if (originalProc == NULL)
	{
		DWORD error = GetLastError();

		// error handling? how?
		dmLogError("error while subclass current window: %d\n", error);

		return false;
	}

	return true;
}

// disable subclass windows
void defos_disable_subclass_window()
{
	if (originalProc != NULL)
	{
		HWND window = dmGraphics::GetNativeWindowsHWND();
		SetWindowLongPtr(window, -4, (LONG)originalProc);
	}
}

// watch the mouse state changing
void defos_watch_mouse_state(lua_State *L)
{
	if (!mouse_event_hooked)
	{
		register_callback(L, 1, &mouseStateCb);

		mouse_event_hooked = true;
	}
}

bool defos_is_fullscreen()
{
	return !(get_window_style() & WS_OVERLAPPEDWINDOW);
}

bool defos_is_maximized()
{
	return IsZoomed(dmGraphics::GetNativeWindowsHWND());
}

bool defos_is_mouse_cursor_within_window()
{
	HWND window = dmGraphics::GetNativeWindowsHWND();

	POINT ptr;
	ClientToScreen(window, &ptr);
	int client_x = ptr.x;
	int client_y = ptr.y;

	POINT pos;
	GetCursorPos(&pos);
	int cursor_x = pos.x;
	int cursor_y = pos.y;

	RECT client_rect;
	GetClientRect(window, &client_rect);
	int client_width = client_rect.right;
	int client_height = client_rect.bottom;

	cursor_x = cursor_x - client_x;
	cursor_y = cursor_y - client_y;

	if ((cursor_x >= 0) && (cursor_y >= 0) && (cursor_x <= client_width) && (cursor_y <= client_height))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void defos_disable_maximize_button()
{
	set_window_style(get_window_style() & ~WS_MAXIMIZEBOX);
}

void defos_disable_minimize_button()
{
	set_window_style(get_window_style() & ~WS_MINIMIZEBOX);
}

void defos_disable_window_resize()
{
	set_window_style(get_window_style() & ~WS_SIZEBOX);
}

void defos_disable_mouse_cursor()
{
	ShowCursor(0);
}

void defos_enable_mouse_cursor()
{
	ShowCursor(1);
}

// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/
void defos_toggle_fullscreen()
{
	if (defos_is_maximized())
	{
		defos_toggle_maximize();
	}

	HWND window = dmGraphics::GetNativeWindowsHWND();
	if (!defos_is_fullscreen())
	{
		MONITORINFO mi = {sizeof(mi)};
		if (GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi))
		{
			set_window_style(get_window_style() & ~WS_OVERLAPPEDWINDOW);
			GetWindowPlacement(window, &placement);
			SetWindowPos(window, HWND_TOP,
						 mi.rcMonitor.left,
						 mi.rcMonitor.top,
						 mi.rcMonitor.right - mi.rcMonitor.left,
						 mi.rcMonitor.bottom - mi.rcMonitor.top,
						 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		set_window_style(get_window_style() | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(window, &placement);
		SetWindowPos(window, NULL,
					 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

void defos_toggle_maximize()
{
	if (defos_is_fullscreen())
	{
		defos_toggle_fullscreen();
	}

	HWND window = dmGraphics::GetNativeWindowsHWND();
	if (defos_is_maximized())
	{
		SetWindowPlacement(window, &placement);
	}
	else
	{
		GetWindowPlacement(window, &placement);
		ShowWindow(window, SW_MAXIMIZE);
	}
}

void defos_set_window_size(int x, int y, int w, int h)
{
	if (x == -1)
	{
		x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
		y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	}
	HWND window = dmGraphics::GetNativeWindowsHWND();
	SetWindowPos(window, window, x, y, w, h, SWP_NOZORDER);
}

void defos_set_window_title(const char *title_lua)
{
	SetWindowTextW(dmGraphics::GetNativeWindowsHWND(), CA2W(title_lua));
}

/********************
 * internal functions
 ********************/

bool set_window_style(LONG_PTR style)
{
	return SetWindowLongPtrA(dmGraphics::GetNativeWindowsHWND(), GWL_STYLE, style) != 0;
}

LONG_PTR get_window_style()
{
	return GetWindowLongPtrA(dmGraphics::GetNativeWindowsHWND(), GWL_STYLE);
}

// enable the mouse tracking event, so that wndproc will recieve the mouse leave/hover event
bool enable_mouse_tracking()
{
	HWND window = dmGraphics::GetNativeWindowsHWND();

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_HOVER | TME_LEAVE;
	tme.hwndTrack = window;
	tme.dwHoverTime = 1;

	return TrackMouseEvent(&tme);
}

// replaced wndproc to cutomize message processing
int __stdcall custom_wndproc(HWND hwnd, UINT umsg, WPARAM wp, LPARAM lp)
{
	// NOTE: we do not handle any event here, so they will be processed by the default wndproc callback

	switch (umsg)
	{
	case WM_MOUSEMOVE:
		if (!is_mouse_tracking)
		{
			is_mouse_tracking = enable_mouse_tracking();
		}
		break;
	case WM_MOUSELEAVE:
		is_mouse_tracking = false; // each time leave and hover reached, the mouse tracking is disabled, we need to track it again

		invoke_mouse_state(0);
		break;
	case WM_MOUSEHOVER:
		is_mouse_tracking = false;
		invoke_mouse_state(1);
		break;
	}

	if (originalProc != NULL)
		return CallWindowProc(originalProc, hwnd, umsg, wp, lp);
	else
		return 0;
}

// TODO: make a meaningful name
// register can be a general one
void register_callback(lua_State *L, int index, LuaCallbackInfo *cbk)
{
	luaL_checktype(L, index, LUA_TFUNCTION);
	lua_pushvalue(L, index);
	int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

	if (cbk->m_Callback != LUA_NOREF)
	{
		dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
		dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
	}

	cbk->m_L = dmScript::GetMainThread(L);
	cbk->m_Callback = cb;

	dmScript::GetInstance(L);

	cbk->m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
}

void unregister_callback(LuaCallbackInfo *cbk)
{
	if (cbk->m_Callback != LUA_NOREF)
	{
		dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
		dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
		cbk->m_Callback = LUA_NOREF;
	}
}

// invoke the mouse state change callback, 0-leave, 1-hover
void invoke_mouse_state(int state)
{
	LuaCallbackInfo *mscb = &mouseStateCb;

	if (mscb->m_Callback == LUA_NOREF)
	{
		return;
	}

	lua_State *L = mscb->m_L;
	int top = lua_gettop(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, mscb->m_Callback);

	// Setup self (the script instance)
	lua_rawgeti(L, LUA_REGISTRYINDEX, mscb->m_Self);
	lua_pushvalue(L, -1);

	dmScript::SetInstance(L);

	lua_pushnumber(L, state);

	int ret = lua_pcall(L, 2, 0, 0);

	if (ret != 0)
	{
		dmLogError("Error running callback: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
	assert(top == lua_gettop(L));
}

#endif
