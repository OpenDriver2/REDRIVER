# Building and running

Standalone exe, premake like REDRIVER2 (`src_rebuild/premake5.lua`, `premake5.exe`,
`gen_vc2022.bat` for the IDE). x86 only, C, `/J`, static CRT, MBCS; Glide SDK include path;
links SDL2, OpenAL32, ddraw, dxguid, dsound, winmm, ole32; SDL2.dll and OpenAL32.dll
are copied next to the exe. The entry point is `main` (`mainCRTStartup`, no SDL2main, `SDL_MAIN_HANDLED`)
in a windowed subsystem. Toolchain: Visual Studio 2022 (MSVC x86) and
the Windows 10 SDK, which still ships `d3d.h`/`ddraw.h`/`dsound.h`.

```bash
cmd //c 'D:\Projects\OpenDriver1\Driver\src_rebuild\build_vc2022.bat' > <scratchpad>/build.txt 2>&1   # [Debug|Release]
```

- The Glide pointer table is in `D3DGlide/glide_ptrs.h` ([A], defined in `system.c` via
  `#define GLIDE_EXTERN`), the emulation's `d3dgl_*` prototypes in `D3DGlide/d3dglide.h` ([A]).
  Include those instead of declaring Glide pointers by hand.
- Headers (`<file>.h` next to the `.c`, `dr_globals.h`, `dr_types.h`) are maintained by hand:
  declare a global or function other files use in its owner's header.
- Debug builds define `OPENDRIVER_DEBUG`: our own diagnostics ([A], e.g. `OutputDebugMessage` in
  `glide2x.c` writes to `debug.log`). Never change behaviour under it.
- A DirectDraw wrapper must sit next to the exe: the system `ddraw.dll` gives only software D3D
  devices in an RDP session (and on many modern setups), which `CanUse3DDevice` rightly rejects
  ("Please run driver configuration"). The post-build step copies dgVoodoo (`DDraw.dll`,
  `D3DImm.dll`, `dgVoodoo.conf`) from `data\dgvoodoo` into `bin\<config>`.
- Windowed mode ([A]) is the default, command line `/FULLSCREEN` (case-insensitive, like the
  original's dropped `/RES640`, `/NOPEDS` switches) turns it off; see `gWindowed` in `shell.c`. A
  framed window whose client area is `display_width x display_height`, never maximised, with
  Windows ghosting disabled (the game doesn't pump messages while it waits, so the framed window
  would turn into "Not responding"). The window comes from SDL (`InitWindow`) and its HWND goes to
  DirectDraw and DirectSound through `gScreen`. The renderer still takes the
  fullscreen DirectDraw path, so dgVoodoo's `FullScreenMode = false` is what keeps it in the
  window. With `/FULLSCREEN` the borderless SDL window sits at the corner of the screen and is
  maximised by `d3dgl_SstWinOpen`, as the original's `WS_POPUP` window.
- Mouse ([A]): the game doesn't read the mouse, so it is left free: there is no mouse device,
  the cursor is hidden only with `/FULLSCREEN`, and `CaptureMouse` is off in
  `data\dgvoodoo\dgVoodoo.conf`.
- Glide ([A]): command line `/GLIDE` uses `glide2x.dll` (dgVoodoo's Glide wrapper) instead of the
  device in `CONFIG.DAT`, which sets `using_glide`. The post-build step copies `data\Glide2x.dll`
  into `bin\<config>`. Without the DLL the game stays on the configured device.
- Sound ([A]): OpenAL Soft by default; command line `/DSOUND` uses the original DirectSound back
  end, for comparing by ear until it is removed.
- Motel room ([A]): command line `/MOTEL` skips the frontend and starts in `DoFlat` (the
  original's own `gDoMotelRoom` branch of `main`), for testing without playing the mission
  ladder up to it.
- Interview / missions ([A]): `/INTERVIEW` starts straight in the interview (IVIEW.LEV, no mission
  script), `/MISSION<n>` in `Scripts\Missions\Mission<n>.dms` (`/MISSION201`: the interview
  training); both set what the mission ladder's INTERVIEW / MISSION commands set and take
  `main`'s own `StartGame(0)` branch.
- Credits ([A]): command line `/CREDITS` skips the frontend and plays the end credits (the
  `CREDITS` replay with `gDoCredits`, as `credits_choosefunc` does from the cheats menu), then
  quits at the end of the replay or on Escape.
- FPU mode: the game runs the x87 FPU in single precision with chop rounding (`FPUInit`,
  asmmath.c; `FPUMaintain` restores it every frame). premake builds the C float code for the x87
  (`vectorextensions "IA32"`, `/arch:IA32`) so every float operation rounds as in the original:
  with SSE2 code (round to nearest) the road heights and normals (`MapHeight`, `FindSurface`)
  differ by a unit now and then and replays drift. The current CRT parses floats only in
  round-to-nearest mode (debug builds assert "unexpected rounding mode"): scanf calls with `%f`
  switch to it for the call ([A] `fscanf_nearest` in camera.c, `sscanf_nearest` in compiler.c);
  new ones need the same. Defining `OPENDRIVER_NO_FPU_CHOP` leaves the FPU alone ([A]).
- Input ([A]): SDL, see `input.c`. The keys only reach the game while its window has the keyboard
  focus. The `DIK_*` key codes are our own (`dr_keys.h`), with the DirectInput values, so
  `CONFIG.DAT` stays valid.
- Run from the game folder `D:\Projects\OpenDriver1\Driver\data` (the debugger working dir). That
  `Game.exe` is a different build than the IDA database (same MSVC 6, data addresses differ), so
  use it only for its data files. Running overwrites `data\debug.log` (back it up first) and shows
  a message box on errors.
