#define EXTENSION_NAME defos
#define LIB_NAME "defos"
#define MODULE_NAME "defos"

#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>
#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_HTML5) || defined(DM_PLATFORM_LINUX)
#include "defos_private.h"

static bool checkboolean(lua_State *L, int index)
{
    luaL_checktype(L, index, LUA_TBOOLEAN);
    return lua_toboolean(L, index);
}

LuaCallbackInfo defos_event_handlers[DEFOS_EVENT_COUNT];

// Window management

static int disable_maximize_button(lua_State *L)
{
    defos_disable_maximize_button();
    return 0;
}

static int disable_minimize_button(lua_State *L)
{
    defos_disable_minimize_button();
    return 0;
}

static int disable_window_resize(lua_State *L)
{
    defos_disable_window_resize();
    return 0;
}

static int get_window_size(lua_State *L)
{
    WinRect rect;
    rect = defos_get_window_size();
    lua_pushnumber(L, rect.x);
    lua_pushnumber(L, rect.y);
    lua_pushnumber(L, rect.w);
    lua_pushnumber(L, rect.h);
    return 4;
}

static int get_view_size(lua_State *L)
{
    WinRect rect;
    rect = defos_get_view_size();
    lua_pushnumber(L, rect.x);
    lua_pushnumber(L, rect.y);
    lua_pushnumber(L, rect.w);
    lua_pushnumber(L, rect.h);
    return 4;
}

static int set_window_size(lua_State *L)
{
    float x = nanf("");
    if (!lua_isnil(L, 1))
    {
        x = luaL_checknumber(L, 1);
    }
    float y = nanf("");
    if (!lua_isnil(L, 2))
    {
        y = luaL_checknumber(L, 2);
    }
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);
    defos_set_window_size(x, y, w, h);
    return 0;
}

static int set_view_size(lua_State *L)
{
    float x = nanf("");
    if (!lua_isnil(L, 1))
    {
        x = luaL_checknumber(L, 1);
    }
    float y = nanf("");
    if (!lua_isnil(L, 2))
    {
        y = luaL_checknumber(L, 2);
    }
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);
    defos_set_view_size(x, y, w, h);
    return 0;
}

static int set_window_title(lua_State *L)
{
    const char *title_lua = luaL_checkstring(L, 1);
    defos_set_window_title(title_lua);
    return 0;
}

static int toggle_fullscreen(lua_State *L)
{
    defos_toggle_fullscreen();
    return 0;
}

static int set_fullscreen(lua_State *L)
{
    if (checkboolean(L, 1) != defos_is_fullscreen()) {
        defos_toggle_fullscreen();
    }
    return 0;
}

static int is_fullscreen(lua_State *L)
{
    lua_pushboolean(L, defos_is_fullscreen());
    return 1;
}

static int toggle_maximized(lua_State *L)
{
    defos_toggle_maximized();
    return 0;
}

static int set_maximized(lua_State *L)
{
    if (checkboolean(L, 1) != defos_is_maximized()) {
        defos_toggle_maximized();
    }
    return 0;
}

static int is_maximized(lua_State *L)
{
    lua_pushboolean(L, defos_is_maximized());
    return 1;
}

// Windows console

static int set_console_visible(lua_State *L)
{
    defos_set_console_visible(checkboolean(L, 1));
    return 0;
}

static int is_console_visible(lua_State *L)
{
    lua_pushboolean(L, defos_is_console_visible());
    return 1;
}

// Cursor and mouse

static int is_mouse_in_view(lua_State *L)
{
    lua_pushboolean(L, defos_is_mouse_in_view());
    return 1;
}

static int set_cursor_visible(lua_State *L)
{
    defos_set_cursor_visible(checkboolean(L, 1));
    return 0;
}

static int is_cursor_visible(lua_State *L)
{
    lua_pushboolean(L, defos_is_cursor_visible());
    return 1;
}

static int set_cursor_pos(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    defos_set_cursor_pos(x, y);
    return 0;
}
static int move_cursor_to(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    defos_move_cursor_to(x, y);
    return 0;
}

static int set_cursor_clipped(lua_State *L)
{
    defos_set_cursor_clipped(checkboolean(L, 1));
    return 0;
}

static int is_cursor_clipped(lua_State *L)
{
    lua_pushboolean(L, defos_is_cursor_clipped());
    return 1;
}

static int set_cursor_locked(lua_State *L)
{
    defos_set_cursor_locked(checkboolean(L, 1));
    return 0;
}

static int is_cursor_locked(lua_State *L)
{
    lua_pushboolean(L, defos_is_cursor_locked());
    return 1;
}

static int set_cursor(lua_State *L)
{
    if (lua_isnil(L, 1))
    {
        defos_reset_cursor();
        return 0;
    }

    if (lua_isnumber(L, 1))
    {
        DefosCursor cursor = (DefosCursor)luaL_checkint(L, 1);
        defos_set_cursor(cursor);
        return 0;
    }

    #ifdef DM_PLATFORM_WINDOWS
    const char *cursor_path_lua = luaL_checkstring(L, 1);
    defos_set_custom_cursor_win(cursor_path_lua);
    return 0;
    #endif

    #ifdef DM_PLATFORM_LINUX
    const char *cursor_path_lua = luaL_checkstring(L,1);
    defos_set_custom_cursor_linux(cursor_path_lua);
    return 0;

    // TODO: X11 support animated cursor by XRender
    #endif

    #ifdef DM_PLATFORM_OSX
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "hot_spot_x");
    float hotSpotX = 0.0f;
    if (!lua_isnil(L, -1))
    {
        hotSpotX = luaL_checknumber(L, -1);
    }

    lua_getfield(L, 1, "hot_spot_y");
    float hotSpotY = 0.0f;
    if (!lua_isnil(L, -1))
    {
        hotSpotY = luaL_checknumber(L, -1);
    }

    lua_getfield(L, 1, "image");
    dmBuffer::HBuffer image = dmScript::CheckBuffer(L, -1)->m_Buffer;

    defos_set_custom_cursor_mac(image, hotSpotX, hotSpotY);
    lua_pop(L, 3);
    return 0;
    #endif

    #ifdef DM_PLATFORM_HTML5
    const char * cursor_url = luaL_checkstring(L, 1);
    defos_set_custom_cursor_html5(cursor_url);
    return 0;
    #endif

    lua_pushstring(L, "Invalid argument");
    lua_error(L);
    return 0;
}

static int reset_cursor(lua_State *L)
{
    defos_reset_cursor();
    return 0;
}

// Events

static void set_event_handler(lua_State *L, int index, DefosEvent event)
{
    LuaCallbackInfo *cbk = &defos_event_handlers[event];
    if (cbk->m_Callback != LUA_NOREF)
    {
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
    }

    if (lua_isnil(L, index))
    {
        cbk->m_Callback = LUA_NOREF;
    }
    else
    {
        luaL_checktype(L, index, LUA_TFUNCTION);
        lua_pushvalue(L, index);
        int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

        cbk->m_L = dmScript::GetMainThread(L);
        cbk->m_Callback = cb;

        dmScript::GetInstance(L);

        cbk->m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    }

    defos_event_handler_was_set(event);
}

static int on_mouse_leave(lua_State *L)
{
    set_event_handler(L, 1, DEFOS_EVENT_MOUSE_LEAVE);
    return 0;
}

static int on_mouse_enter(lua_State *L)
{
    set_event_handler(L, 1, DEFOS_EVENT_MOUSE_ENTER);
    return 0;
}

static int on_click(lua_State *L)
{
    #ifndef DM_PLATFORM_HTML5
    dmLogInfo("Event 'on_click' exists only on HTML5");
    #endif
    set_event_handler(L, 1, DEFOS_EVENT_CLICK);
    return 0;
}

static int on_cursor_lock_disabled(lua_State *L)
{
    set_event_handler(L, 1, DEFOS_EVENT_CURSOR_LOCK_DISABLED);
    return 0;
}

void defos_emit_event(DefosEvent event)
{
    LuaCallbackInfo *mscb = &defos_event_handlers[event];

    if (mscb->m_Callback == LUA_NOREF)
    {
        return;
    }

    lua_State *L = mscb->m_L;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, mscb->m_Callback);

    // Setup self (the script instance)
    lua_rawgeti(L, LUA_REGISTRYINDEX, mscb->m_Self);
    dmScript::SetInstance(L);

    int ret = lua_pcall(L, 0, 0, 0);

    if (ret != 0)
    {
        dmLogError("Error running event handler: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    assert(top == lua_gettop(L));
}

// Lua module initialization

static const luaL_reg Module_methods[] =
    {
        {"disable_maximize_button", disable_maximize_button},
        {"disable_minimize_button", disable_minimize_button},
        {"disable_window_resize", disable_window_resize},
        {"set_window_title", set_window_title},
        {"get_window_size", get_window_size},
        {"set_window_size", set_window_size},
        {"toggle_fullscreen", toggle_fullscreen},
        {"set_fullscreen", set_fullscreen},
        {"is_fullscreen", is_fullscreen},
        {"toggle_maximize", toggle_maximized}, // For backwards compatibility
        {"toggle_maximized", toggle_maximized},
        {"set_maximized", set_maximized},
        {"is_maximized", is_maximized},
        {"set_console_visible", set_console_visible},
        {"is_console_visible", is_console_visible},
        {"set_cursor_visible", set_cursor_visible},
        {"is_cursor_visible", is_cursor_visible},
        {"on_mouse_enter", on_mouse_enter},
        {"on_mouse_leave", on_mouse_leave},
        {"on_click", on_click},
        {"is_mouse_in_view", is_mouse_in_view},
        {"set_cursor_pos", set_cursor_pos},
        {"move_cursor_to", move_cursor_to},
        {"set_cursor_clipped", set_cursor_clipped},
        {"is_cursor_clipped", is_cursor_clipped},
        {"set_cursor_locked", set_cursor_locked},
        {"is_cursor_locked", is_cursor_locked},
        {"on_cursor_lock_disabled", on_cursor_lock_disabled},
        {"set_view_size", set_view_size},
        {"get_view_size", get_view_size},
        {"set_cursor", set_cursor},
        {"reset_cursor", reset_cursor},
        {0, 0}};

static void LuaInit(lua_State *L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pushnumber(L, DEFOS_CURSOR_ARROW);
    lua_setfield(L, -2, "CURSOR_ARROW");
    lua_pushnumber(L, DEFOS_CURSOR_CROSSHAIR);
    lua_setfield(L, -2, "CURSOR_CROSSHAIR");
    lua_pushnumber(L, DEFOS_CURSOR_HAND);
    lua_setfield(L, -2, "CURSOR_HAND");
    lua_pushnumber(L, DEFOS_CURSOR_IBEAM);
    lua_setfield(L, -2, "CURSOR_IBEAM");
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result InitializeDefos(dmExtension::Params *params)
{
    // Clear any bound events
    for (int i = 0; i < DEFOS_EVENT_COUNT; i++)
    {
        defos_event_handlers[i].m_Callback = LUA_NOREF;
    }
    defos_init();
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDefos(dmExtension::Params *params)
{
    defos_final();
    return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateDefos(dmExtension::Params *params)
{
    defos_update();
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, 0, 0, InitializeDefos, UpdateDefos, 0, FinalizeDefos)
#else
dmExtension::Result InitializeDefos(dmExtension::Params *params)
{
   return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDefos(dmExtension::Params *params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, 0, 0, InitializeDefos, 0, 0, FinalizeDefos)
#endif