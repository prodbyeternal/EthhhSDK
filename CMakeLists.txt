cmake_minimum_required(VERSION 3.20)
project(EthhhSDK CXX C)

# ---------------------------------------------------------------------------
# EthhhSDK - I-Ninja modding API/SDK
#   EthhhSDK.dll  : core SDK (hooks, ImGui, Lua, C# host, mod manager)
#   dinput8.dll   : app-local proxy that loads EthhhSDK.dll before the game's
#                   entry point runs (I-Ninja statically imports DINPUT8.dll).
# Target: Windows x86 (I-Ninja.exe is a 32-bit process, image base 0x400000).
# ---------------------------------------------------------------------------

if(NOT WIN32)
  message(FATAL_ERROR "EthhhSDK targets Windows only.")
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "I-Ninja is 32-bit: configure with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

set(ETH_ROOT       ${CMAKE_CURRENT_SOURCE_DIR})
set(ETH_TP         ${ETH_ROOT}/src/third_party)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

string(TIMESTAMP ETH_BUILD_DATE "%Y-%m-%d" UTC)

add_compile_definitions(
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _CRT_SECURE_NO_WARNINGS
  _CRT_NONSTDC_NO_DEPRECATE
  ETH_BUILD_DATE="${ETH_BUILD_DATE}"
)

if(MSVC)
  add_compile_options(/MP /W3 /wd4244 /wd4267 /wd4305 /wd4996)
  # The core uses structured exception handling (__try/__except) in functions
  # that also own C++ objects; /EHa allows that combination.
  string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------- imageui ---
add_library(imgui STATIC
  ${ETH_TP}/imgui/imgui.cpp
  ${ETH_TP}/imgui/imgui_draw.cpp
  ${ETH_TP}/imgui/imgui_tables.cpp
  ${ETH_TP}/imgui/imgui_widgets.cpp
  ${ETH_TP}/imgui/backends/imgui_impl_win32.cpp
  ${ETH_TP}/imgui/backends/imgui_impl_dx9.cpp
)
target_include_directories(imgui PUBLIC ${ETH_TP}/imgui ${ETH_TP}/imgui/backends)
target_link_libraries(imgui PUBLIC d3d9.lib)

# ------------------------------------------------------------------- lua ----
file(GLOB LUA_SRC ${ETH_TP}/lua/[l-z]*.c)
# lua.c / luac.c are standalone interpreters with their own main(); never link them.
list(FILTER LUA_SRC EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA_SRC EXCLUDE REGEX "/luac\\.c$")
list(FILTER LUA_SRC EXCLUDE REGEX "/onelua\\.c$")
add_library(lua54 STATIC ${LUA_SRC})
target_include_directories(lua54 PUBLIC ${ETH_TP}/lua)
if(MSVC)
  target_compile_options(lua54 PRIVATE /wd4244 /wd4996 /wd4310)
endif()

# --------------------------------------------------------------- minhook ----
add_library(minhook STATIC
  ${ETH_TP}/minhook/src/buffer.c
  ${ETH_TP}/minhook/src/hook.c
  ${ETH_TP}/minhook/src/trampoline.c
  ${ETH_TP}/minhook/src/hde/hde32.c
  ${ETH_TP}/minhook/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${ETH_TP}/minhook/include ${ETH_TP}/minhook/src/hde)

# ------------------------------------------------------------- nanovg? no ----------------------------------------------------------
# ----------------------------------------------------------------- core -----
file(GLOB ETH_CORE_SRC ${ETH_ROOT}/src/core/*.cpp)
add_library(EthhhSDK SHARED ${ETH_CORE_SRC})
target_include_directories(EthhhSDK PRIVATE ${ETH_ROOT}/src/core ${ETH_TP}/lua)
# NOTE: no mscoree.lib -- not installed with this SDK. clr_host.cpp binds
# CorBindToRuntimeEx from mscoree.dll dynamically.
target_link_libraries(EthhhSDK PRIVATE imgui lua54 minhook d3d9.lib dxguid.lib ole32.lib oleaut32.lib user32.lib gdi32.lib shell32.lib advapi32.lib winmm.lib bcrypt.lib shlwapi.lib)
set_target_properties(EthhhSDK PROPERTIES PREFIX "" OUTPUT_NAME "EthhhSDK")

# Export .def generated from the sources so the C ABI stays undecorated.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_custom_command(TARGET EthhhSDK PRE_LINK
    COMMAND ${Python3_EXECUTABLE} ${ETH_ROOT}/tools/gen_def.py
            ${ETH_ROOT}/src/core/c_api.cpp ${ETH_ROOT}/src/core/imgui_c.cpp
            ${ETH_ROOT}/src/core/sdk.cpp
            -o ${CMAKE_CURRENT_BINARY_DIR}/ethhhsdk_exports.def
    COMMENT "Generating ethhhsdk_exports.def"
    VERBATIM)
  target_link_options(EthhhSDK PRIVATE /DEF:${CMAKE_CURRENT_BINARY_DIR}/ethhhsdk_exports.def)
else()
  target_link_options(EthhhSDK PRIVATE /DEF:${ETH_ROOT}/tools/ethhhsdk_exports.def)
endif()

# ---------------------------------------------------------------- proxy -----
add_library(dinput8 SHARED ${ETH_ROOT}/src/proxy/dinput8_proxy.cpp)
target_link_libraries(dinput8 PRIVATE user32.lib)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME "dinput8")
target_link_options(dinput8 PRIVATE /DEF:${ETH_ROOT}/src/proxy/dinput8.def)

# ----------------------------------------------------------------- output ---
set_target_properties(EthhhSDK dinput8 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR}/dist
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/dist
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/dist
)

option(ETH_BUILD_TESTS "Build isolated SDK regression/preview tools" OFF)
if(ETH_BUILD_TESTS)
  if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Python is required for regression tools.")
  endif()
  add_subdirectory(tests)
endif()
