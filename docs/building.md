# Building and running

Standalone exe, premake like REDRIVER2 (`src_rebuild/premake5.lua`, `premake5.exe`,
`gen_vc2022.bat` for the IDE). x86 only, C, `/J`, static CRT, MBCS; Glide SDK include path;
links ddraw, dxguid, dinput8, dsound, winmm, ole32. Toolchain: Visual Studio 2022 (MSVC x86) and
the Windows 10 SDK, which still ships `d3d.h`/`ddraw.h`/`dinput.h`/`dsound.h`.

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
- Windowed mode ([A]): command line `/WINDOW` (case-insensitive, like the original's dropped
  `/RES640`, `/NOPEDS` switches; set as the debugger argument in premake) sets `gWindowed` in
  `shell.c`: a framed window whose client area is `display_width x display_height`, never
  maximised, with Windows ghosting disabled (the game doesn't pump messages while it waits, so
  the framed window would turn into "Not responding"). The renderer still takes the fullscreen DirectDraw path, so dgVoodoo's
  `FullScreenMode = false` is what keeps it in the window. Without `/WINDOW` the original
  maximised `WS_POPUP` window covers the screen even with dgVoodoo windowed.
- Glide ([A]): command line `/GLIDE` uses `glide2x.dll` (dgVoodoo's Glide wrapper) instead of the
  device in `CONFIG.DAT`, which sets `using_glide`. The post-build step copies `data\Glide2x.dll`
  into `bin\<config>`. Without the DLL the game stays on the configured device.
- Motel room ([A]): command line `/MOTEL` skips the frontend and starts in `DoFlat` (the
  original's own `gDoMotelRoom` branch of `WinMain`), for testing without playing the mission
  ladder up to it.
- Interview / missions ([A]): `/INTERVIEW` starts straight in the interview (IVIEW.LEV, no mission
  script), `/MISSION<n>` in `Scripts\Missions\Mission<n>.dms` (`/MISSION201`: the interview
  training); both set what the mission ladder's INTERVIEW / MISSION commands set and take
  `WinMain`'s own `StartGame(0)` branch.
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
- DirectInput: the game uses DirectInput 5 (`DIRECTINPUT_VERSION 0x0500`). Current SDKs have no
  `dinput.lib`, so `input.c` defines `DirectInputCreateA` itself ([A]) and loads it from
  `dinput.dll`; the data formats (`c_dfDIKeyboard` …) come from `dinput8.lib`. Input uses
  `DISCL_FOREGROUND`: when testing by script, keys typed into other windows while the game has
  focus reach the game.
- Run from the game folder `D:\Projects\OpenDriver1\Driver\data` (the debugger working dir). That
  `Game.exe` is a different build than the IDA database (same MSVC 6, data addresses differ), so
  use it only for its data files. Running overwrites `data\debug.log` (back it up first) and shows
  a message box on errors.
