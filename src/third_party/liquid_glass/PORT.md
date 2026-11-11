# Liquid Glass DX9 port

Source: the user-supplied `liquidDX11-main/examples/example_win32_directx11`.
The original MIT license and third-party notices accompany this directory.

`glass.cpp`, all eight widget includes, materials, spring animations, icon
mapping and `shaders.h` originate from that template. The SDK keeps ImGui 1.91.8
and its existing public C/Lua/C# ABI. No `SurfaceDock`, desktop duplication,
recorder, global hotkeys, example application, or alternate ImGui backend is
integrated. The SDK uses the DX9 adapter, not the template's DX11 backend.

`renderer_dx9.inc` adapts the unchanged optical shader expressions to SM3
samplers/registers, CPU-supplied half-pixel-correct quads and DX9 Dual Kawase
render targets. Glass primitives use ordered ImGui callbacks so child panels
cannot cover earlier text and popup/modal ordering remains correct. Render
targets, depth, MRTs and viewport are saved explicitly alongside D3DSBT_ALL.
Default-pool resources are invalidated before Reset and rebuilt afterwards.
Unsupported SM3 or scene capture uses the SDK's readable fallback panels.

The DX9 adapter explicitly restores stream 0 and FVF after DrawPrimitiveUP,
whose implicit stream reset is not safely covered by all state-block paths.
Backdrop captures use a bounded atlas and a 10-Hz cache; control geometry and
spring motion still update at the current render cadence. One pane covers each
manager; child regions do not each recapture/blur the scene.

Launcher targets are A8R8G8B8 with a transparent clear and launcher-specific
output alpha. The actual launcher does not capture the desktop or draw the
preview's procedural ribbon backdrop. Native DWM transparency and final optical
fidelity still need rendered/manual acceptance; CPU layout tests cannot prove
either transparency or low in-game overhead.
