# Building and running

Standalone exe, premake like REDRIVER2 (`src_rebuild/premake5.lua`, `premake5.exe`,
`gen_vc2022.bat` for the IDE). x86 only, C, `/J`, static CRT, MBCS; Glide SDK include path;
links ddraw, dxguid, dinput8, dsound, winmm, ole32. Toolchain: Visual Studio 2022 (MSVC x86) and
the Windows 10 SDK, which still ships `d3d.h`/`ddraw.h`/`dinput.h`/`dsound.h`.

```bash
bash tools/build.sh <scratchpad>/b [Debug|Release]   # full build: empty stubs -> build -> gen_stubs -> build
# the steps it runs:
cmd //c 'D:\Projects\OpenDriver1\Driver\src_rebuild\build_vc2022.bat' > <scratchpad>/build.txt 2>&1   # [Debug|Release]
python tools/build_errors.py <scratchpad>/build.txt <scratchpad>/unresolved.txt   # compile errors + unresolved symbols
python tools/ida/idacall.py py tools/ida/gen_stubs.py SYMS=<scratchpad>\unresolved.txt OUT=D:\Projects\OpenDriver1\Driver\src_rebuild\stubs\stubs.c
```

- The Glide pointer table is in `D3DGlide/glide_ptrs.h` ([A], defined in `system.c` via
  `#define GLIDE_EXTERN`), the emulation's `d3dgl_*` prototypes in `D3DGlide/d3dglide.h` ([A]).
  Include those instead of declaring Glide pointers by hand. `__stdcall` stubs (`_name@N`) are
  generated with N/4 int arguments.
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
- Motel room ([A]): command line `/MOTEL` skips the frontend and starts in `DoFlat` (the
  original's own `gDoMotelRoom` branch of `WinMain`), for testing while the mission ladder that
  leads there is not translated.
- Interview / missions ([A]): `/INTERVIEW` starts straight in the interview (IVIEW.LEV, no mission
  script), `/MISSION<n>` in `Scripts\Missions\Mission<n>.dms` (`/MISSION201`: the interview
  training); both set what the mission ladder's INTERVIEW / MISSION commands set and take
  `WinMain`'s own `StartGame(0)` branch. There is no way out of the game yet (Escape is
  `CheckMiscKeys` / the pause menu, not translated): close the window.
- FPU mode ([A]): the game runs the x87 FPU in single precision with chop rounding (`FPUInit`,
  asmmath.c). The current CRT parses and prints floats only in round-to-nearest mode (debug builds
  assert "unexpected rounding mode"), so premake defines `OPENDRIVER_NO_FPU_CHOP` for all builds:
  `FPUInit` leaves the FPU alone and `float_to_int` truncates (what `fistp` gives under chop). C
  float code is SSE2 and does not depend on the x87 mode. Remove the define to run with the
  original mode once all the code is rebuilt (`camera.c` already reads its floats safely).
- DirectInput: the game uses DirectInput 5 (`DIRECTINPUT_VERSION 0x0500`). Current SDKs have no
  `dinput.lib`, so `input.c` defines `DirectInputCreateA` itself ([A]) and loads it from
  `dinput.dll`; the data formats (`c_dfDIKeyboard` …) come from `dinput8.lib`. Input uses
  `DISCL_FOREGROUND`: when testing by script, keys typed into other windows while the game has
  focus reach the game.

- Everything not translated yet is linked from `src_rebuild/stubs/stubs.c`, **generated** by
  `gen_stubs.py` from the linker's unresolved symbols: functions log their first call to
  `stubs.log` (in the working dir) and return 0 (`void`/`int`/`float` by the IDA prototype);
  data gets the IDA size and initial bytes (string pointers kept, other pointers 0). Never edit
  it by hand. When a function gets translated, its stub becomes a duplicate symbol: delete
  `stubs.c`, build, regenerate from the new unresolved list, build again. Symbols that are not
  IDA items go to the `MANUAL` set in `gen_stubs.py` and are written by hand.
- Run from the game folder `D:\Projects\OpenDriver1\Driver\data` (the debugger working dir). That
  `Game.exe` is a different build than the IDA database (same MSVC 6, data addresses differ), so
  use it only for its data files. Running overwrites `data\debug.log` (back it up first) and shows
  a message box on errors; `stubs.log` shows how far the run got — the first stub on the path is
  what to translate next.
