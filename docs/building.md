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
  `#define GLIDE_EXTERN`). Include it instead of declaring Glide pointers by hand.
- Headers (`<file>.h` next to the `.c`, `dr_globals.h`, `dr_types.h`) are maintained by hand:
  declare a global or function other files use in its owner's header.
- Debug builds define `OPENDRIVER_DEBUG`: our own diagnostics ([A]). Never change behaviour under it.
- No code in `game6.4g` includes `windows.h`, the few types it took from there are in `dr_types.h`;
  `DisableProcessWindowsGhosting` has its prototype in `shell.c`. The exe needs `SDL2.dll` and
  `OpenAL32.dll`, which the post-build step copies next to it.
- Windowed mode ([A]) is the default, command line `/FULLSCREEN` (case-insensitive, like the
  original's dropped `/RES640`, `/NOPEDS` switches) starts in full screen; see `gWindowed` in
  `shell.c`. The window comes from SDL (`InitWindow`), with Windows ghosting disabled (the game
  doesn't pump messages while it waits, so the framed window would turn into "Not responding"). It
  can be moved and resized freely (320x240 at least), the game stands still while the frame is
  dragged. Alt-Enter switches between the window and full screen, which covers the desktop without
  changing the display mode.
- `config.dat` ([A]) keeps the window after the original's data, where Config.exe doesn't look
  (`WINDOW_CONFIG` in `frontend/config.c`): the size of the framed window (not of full screen or
  a maximised window) and whether the game was in full screen. Written with the options and on the
  way out (`SaveWindowOptions`). Without it the window opens at the display mode from the config.
  Without a valid `config.dat` the game no longer asks to run Driver Configuration: it starts with
  the default options and writes them.
- Mouse ([A]): the game doesn't read the mouse, so it is left free: there is no mouse device,
  and the cursor is hidden only with `/FULLSCREEN`.
- The 3D device is OpenGL 3.1, the sound OpenAL Soft, FMV pl_mpeg ([A]). `/GLIDE`, `/DSOUND` and
  `/GL` are gone with the old back ends, which stay on `master`. A `/GL` on the command line does
  nothing.
- Music ([A]): `Music\Track02.ogg` - `Track10.ogg` or `.wav` (PCM, 8 or 16 bit, mono or stereo) in
  the game folder, numbered as on the CD. Ogg goes first if both exist. No files means no music.
- Motel room ([A]): command line `/MOTEL` skips the frontend and starts in `DoFlat` (the
  original's own `gDoMotelRoom` branch of `main`), for testing without playing the mission
  ladder up to it.
- Interview / missions ([A]): `/INTERVIEW` starts straight in the interview (IVIEW.LEV, no mission
  script), `/MISSION<n>` in `Scripts\Missions\Mission<n>.dms` (`/MISSION201`: the interview
  training); both set what the mission ladder's INTERVIEW / MISSION commands set and take
  `main`'s own `StartGame(0)` branch.
- Game display ([A]): the size of the window's picture, so the resolution and the shape follow the
  window. While the game runs it follows the window too (`FollowWindowSize` in
  `frontend/frontend.c`, `ResizeDisplay` in `gfxinit.c`, `gl_ResizeDevice`): after the frame was
  dragged, a maximise or Alt-Enter the OpenGL device makes its buffers again at the new size and
  keeps the textures. Not during the fly-by, whose letterbox changes `display_height`, and not in
  the motel room, which has its own loop; until then the picture is scaled. Wider than 4:3 the view
  gets wider at the same height (`wide_aspect`), the HUD keeps its proportions and its side items
  go to the sides of the screen. The frontend stays 640x480, 4:3 in the middle of the window.
- `/RES=1280x720` ([A]): the size of the window to start with, over the one in `config.dat` and
  saved there. Not needed any more, the window can simply be resized.
  `view_arc` still comes from the level data at its 4:3 width, cells at the edges can be missing.
- `/NOFADE` ([A]): polygons are cut at `far_clip` as in the original, instead of fading out through a
  4x4 pattern on the last 15% of the way. The fade is for world polygons only: effects, the sun and
  searchlights are drawn past `far_clip` on purpose.
- `/AUSSIE` ([A]): the polygons and the HUD upside down, for Australia. The switch is a string left
  in the binary with no code using it, `gDoAussie` has no writer there.
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
  new ones need the same. Defining `OPENDRIVER_NO_FPU_CHOP` (`dr_globals.h`) leaves the FPU alone ([A]).
- Input ([A]): SDL, see `input.c`. The keys only reach the game while its window has the keyboard
  focus. The `DIK_*` key codes are our own (`dr_keys.h`), with the DirectInput values, so
  `CONFIG.DAT` stays valid.
- Run from the game folder `D:\Projects\OpenDriver1\Driver\data` (the debugger working dir). That
  `Game.exe` is a different build than the IDA database (same MSVC 6, data addresses differ), so
  use it only for its data files. Running overwrites `data\debug.log` (back it up first) and shows
  a message box on errors.

### CI

AppVeyor (`appveyor.yml`, Windows only) builds on the `Visual Studio 2022` image: `.appveyor/Build.bat`
runs `premake5 vs2022` and MSBuild for Debug and Release (x86), `.appveyor/AfterBuild.bat` packs
each output folder into `src_rebuild\bin\<config>\OpenDriver1_<config>.zip` (exe, pdb and the
DLLs next to it). The game data is not needed for the build and is not in the archives.
