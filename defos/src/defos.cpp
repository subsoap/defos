#define EXTENSION_NAME defos
#define LIB_NAME "defos"
#define MODULE_NAME "defos"

#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_WINDOWS)

#include "defos_private.h"

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

static int get_window_size(lua_State* L) {
    WinRect rect;
    rect = defos_get_window_size();
    lua_pushnumber(L, rect.x);
    lua_pushnumber(L, rect.y);
    lua_pushnumber(L, rect.w);
    lua_pushnumber(L, rect.h);
    return 4;
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
    {"get_window_size", get_window_size},
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
