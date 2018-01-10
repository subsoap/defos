#define EXTENSION_NAME defos
#define LIB_NAME "defos"
#define MODULE_NAME "defos"

#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_HTML5)

#include "defos_private.h"

LuaCallbackInfo defos_event_handlers[DEFOS_EVENT_COUNT];

static int disable_maximize_button(lua_State* L) {
    defos_disable_maximize_button();
    return 0;
}

static int disable_minimize_button(lua_State* L) {
    defos_disable_minimize_button();
    return 0;
}

static int disable_window_resize(lua_State* L) {
    defos_disable_window_resize();
    return 0;
}

static int disable_mouse_cursor(lua_State* L) {
    defos_disable_mouse_cursor();
    return 0;
}

static int enable_mouse_cursor(lua_State* L) {
    defos_enable_mouse_cursor();
    return 0;
}

static int set_window_size(lua_State* L) {
    int x = luaL_checkint(L, 1);
    int y = luaL_checkint(L, 2);
    int w = luaL_checkint(L, 3);
    int h = luaL_checkint(L, 4);
    defos_set_window_size(x, y, w, h);
    return 0;
}

static int set_window_title(lua_State* L) {
    const char* title_lua = luaL_checkstring(L, 1);
    defos_set_window_title(title_lua);
    return 0;
}

static int toggle_fullscreen(lua_State* L) {
    defos_toggle_fullscreen();
    return 0;
}

static int is_fullscreen(lua_State* L) {
    bool isFullScreen = defos_is_fullscreen();
    lua_pushboolean(L, isFullScreen);
    return 1;
}

static int toggle_maximize(lua_State* L) {
    defos_toggle_maximize();
    return 0;
}

static int is_maximized(lua_State* L) {
    bool isMaximized = defos_is_maximized();
    lua_pushboolean(L, isMaximized);
    return 1;
}

static int show_console(lua_State* L) {
	defos_show_console();
	return 0;
}

static int hide_console(lua_State* L) {
	defos_hide_console();
	return 0;
}

static int is_console_visible(lua_State* L) {
	bool isConsoleVisible = defos_is_console_visible();
	lua_pushboolean(L, isConsoleVisible);
	return 1;
}

static int is_mouse_inside_window(lua_State* L) {
    bool isWithin = defos_is_mouse_inside_window();
    lua_pushboolean(L, isWithin);
    return 1;
}

static void set_event_handler(lua_State* L, int index, DefosEvent event) {
    LuaCallbackInfo* cbk = &defos_event_handlers[event];
    if (cbk->m_Callback != LUA_NOREF) {
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
    }

    if (lua_isnil(L, index)) {
        cbk->m_Callback = LUA_NOREF;
    } else {
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

static int on_mouse_leave(lua_State* L) {
    set_event_handler(L, 1, DEFOS_EVENT_MOUSE_LEAVE);
    return 0;
}

static int on_mouse_enter(lua_State* L) {
    set_event_handler(L, 1, DEFOS_EVENT_MOUSE_ENTER);
    return 0;
}

static int get_window_size(lua_State* L) {
    WinRect rect;
    rect = defos_get_window_size();
    lua_pushnumber(L, rect.x);
    lua_pushnumber(L, rect.y);
    lua_pushnumber(L, rect.w);
    lua_pushnumber(L, rect.h);
    return 4;
}

static int set_cursor_pos(lua_State* L){
    int x = luaL_checkint(L, 1);
    int y = luaL_checkint(L, 2);

    defos_set_cursor_pos(x, y);

    return 0;
}

static int clip_cursor(lua_State* L){
    defos_clip_cursor();

    return 0;
}

static int restore_cursor_clip(lua_State* L){
    defos_restore_cursor_clip();

    return 0;
}

void defos_emit_event(DefosEvent event) {
    LuaCallbackInfo *mscb = &defos_event_handlers[event];

    if (mscb->m_Callback == LUA_NOREF) { return; }

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

static const luaL_reg Module_methods[] =
{
    {"disable_maximize_button", disable_maximize_button},
    {"disable_minimize_button", disable_minimize_button},
    {"disable_window_resize", disable_window_resize},
    {"disable_mouse_cursor", disable_mouse_cursor},
    {"enable_mouse_cursor", enable_mouse_cursor},
    {"set_window_size", set_window_size},
    {"set_window_title", set_window_title},
    {"toggle_fullscreen", toggle_fullscreen},
    {"is_fullscreen", is_fullscreen},
	{"toggle_maximize", toggle_maximize},	
	{"is_maximized", is_maximized},
	{"show_console", show_console},	
	{"hide_console", hide_console},
	{"is_console_visible", is_console_visible},
    {"is_mouse_inside_window", is_mouse_inside_window},
    {"on_mouse_leave", on_mouse_leave},
    {"on_mouse_enter", on_mouse_enter},
    {"get_window_size", get_window_size},
    {"set_cursor_pos", set_cursor_pos},
    {"clip_cursor", clip_cursor},
    {"restore_cursor_clip", restore_cursor_clip},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeDefos(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDefos(dmExtension::Params* params)
{
    // Clear any bound events
    for (int i = 0; i < DEFOS_EVENT_COUNT; i++) {
        defos_event_handlers[i].m_Callback = LUA_NOREF;
    }
    defos_init();
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDefos(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDefos(dmExtension::Params* params)
{
    defos_final();
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeDefos, AppFinalizeDefos, InitializeDefos, 0, 0, FinalizeDefos)

#else

dmExtension::Result AppInitializeDefos(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDefos(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDefos(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDefos(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeDefos, AppFinalizeDefos, InitializeDefos, 0, 0, FinalizeDefos)
#endif
