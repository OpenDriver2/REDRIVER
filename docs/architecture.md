# Architecture

How the game is put together, as far as it is translated. Read it before working on a subsystem
instead of rediscovering it from IDA; add to it when a translation reveals how something works.
Translation progress per file: `python tools/progress.py` (see [ida.md](ida.md)).

## Start-up and main flow

```
WinMain (shell.c)
  InitMemDebug, InitScreen, Init_FileSystem, InitWindow     [/WINDOW: framed window, [A]]
  LoadSavedOptions (frontend/config.c)                      config.dat; failure -> "Please run driver configuration"
  InitDisplay -> grSetDevice, Detect3dfx, InitDrawingSystem (Engine/3dfx/gfxinit.c)
  InitTimers (time.c)
  MainInit (glaunch.c): CoInitialize, InitDirectInput, InitStringMng, InitSoundDevices,
                        InitSoundSystem, InitCDAudio
  DoFrontend (frontend/main.c)        normal start (gDoFrontend)
    FadeInScreen x3                   splash BMPs (frontend/frontend.c)
    PlayRender                        FMV intro (not translated)
    InitFrontend, menu loop: HandleMenu + DrawFrontend, choosefuncs open sub-menus
    StartGame (frontend/frontend.c) -> GameInit, GameLoop, GameExit (glaunch.c)
  or DoFlat (flat/flat.c)             the "motel" front end (gDoMotelRoom)
  or StartGame(0)                     straight into the game
  StopTimers, MainExit, ShutdownDrawingSystem, grGlideReallyShutdown, CloseMemDebug
```

`HandleWinMessages` pumps Windows messages; the game calls it from its loops. Waits like
`FadeInScreen` don't pump at all (see `/WINDOW` in [building.md](building.md)).

## Graphics: Glide through a pointer table

The engine (`Engine/3dfx/*`) is written for 3dfx Glide 2.x and calls it only through function
pointers `grX` (`D3DGlide/glide_ptrs.h`, defined in `D3DGlide/system.c`):

- `grSetDevice` picks the device; `UseD3DDevice` (`D3DGlide/glide2x.c`) fills the table with the
  game's own Glide-on-Direct3D 6 implementation `d3dgl_X` / `d3dgu_X`; `UseGlideDevice`
  (`gfxinit.c`) with the real `glide2x.dll`. `device_type` / `using_glide` tell which.
- A few `grX` names without `_` (`grGlideInit`, `grSstWinOpen`, `grSstControl`, `grLfbLock`,
  `grTexDownloadMipMap2`, `grTexDownloadTable2`, …) are engine wrappers in `system.c` that call
  the `gr_X` pointers or `d3d_X` extras of the D3D path.
- The D3D path renders to `lpRenderSurface`; full screen flips, windowed blits into the clipped
  primary surface (`d3dgl_BufferSwap`). Texture memory is emulated: 64 KB slots
  (`Engine/3dfx/tset.c`: `TextureSlots[256]` texture sets, `gTSlot[64]` slots), downloads go
  through `grTexDownloadMipMap2` with the texture set number.
- `GrVertex.oow` is 1 for Glide and 1/64 for D3D (`using_glide`); the D3D code takes texture
  coordinates and depth from it.
- Render state caching in the engine: `last_tset`, `last_state` (`TEXT3` …), `last_trans_on`.

## Frontend drawing (8 bit, CPU)

The PC menus are not drawn with Glide polygons:

- `InitFrontend` loads `DATA\FRONTEND.TSD` (`LoadFrontendTextures`): palettes into `gPalette`,
  256x256 8 bit texture pages copied to a 64 KB aligned buffer (`gRamTextureRawDataDump`), one per
  `TextureSlots[n].data`, and the texture names/rectangles into `tinfo_block`
  (`GetTextureDetails(name)` in `map.c`).
- Everything is drawn into `vscreen` (640x480, 8 bit, `frontend/cpupoly.c`) by the `tsd.c`
  blitters (chroma key index 113, 50% blend table `pallookup_hh`).
- `CPUEndOfFrame` locks the back buffer (`grLfbLock`, pixel format from `d3d_LfbLock`), converts
  `vscreen` through `pallookup` (`COPY8TO16` / `COPY8TO32`), then draws the "hires" polys
  (double size, straight into the frame buffer), and unlocks.
- A screen is a list of `FE_POLY` (sprites / filled rectangles, ends with flag 4) and `FE_TEXT`
  (ends with `text == NULL`), shown with `OpenFrontendScreen`. Menus are `FE_BUTTON` arrays
  (`selfunc`, `unselfunc`, `choosefunc`, neighbours `l r u d`) run by `HandleMenu`; `selfunc(1)` /
  `unselfunc(1)` make the selection flash.
- Fonts (`frontend/font.c`): glyphs are textures `F1_<code>` … `F4_<code>`; in the frontend
  `DrawFontString` blits into `vscreen`, in game it draws Glide quads. `^^n` in a string switches
  to font n.
- Splash screens (`FadeInScreen`) are different: 24 bit BMPs cut into six 16 bit Glide textures.

## Input, timing, sound

- `input.c`: DirectInput 5. `ReadInput` copies `KeyMap` to `PrevKeyMap` and reads the keyboard
  state (`DIK_*` indices, 0x80 = down); `CheckKey` = pressed this frame. Joystick buttons and stick
  are mapped onto `KeyMap` (in menus onto Return / Escape / arrows).
- `time.c`: up to 4 multimedia timers (`CreateTimer`); `frontend/demo.c` counts seconds with one
  (`demo_timer_count`, the rolling demo starts after 45 s in the main menu).
- `general.c`: `WaitForSync` paces frames with `timeGetTime` and returns the frames elapsed;
  `DebugMalloc` / `DebugFree` wrap every allocation.
- `sound.c`, `eax/*`: DirectSound with EAX, MCI CD audio — mostly not translated yet.

## Stubs

Anything not translated links from the generated `src_rebuild/stubs/stubs.c` (see
[building.md](building.md)). A stub function returns 0 and logs its first call to `stubs.log`;
stub data has the IDA initial bytes. So the first stub in `stubs.log` on a path is the next thing
to translate, and a stub returning 0 can change the flow (e.g. a `d3dgl_SstControl` stub made the
window close itself on `WM_MOVE`).

## Data layout

Each object file's initialised data is one chunk in `.data`; its zero-initialised statics form
per-object blocks too. The big region from about 0xaac000 up (seen up to 0x12fxxxx) is the
linker's common area: uninitialised globals of many files mixed, position proves nothing.
Ownership rules: [translating.md](translating.md), step 2.

Blocks established so far (the `.c` files note them at their data declarations):

| File | Initialised data | Zero / bss blocks |
|---|---|---|
| `frontend/config.c` | 0x54bcc0 | 0x57d8f8 |
| `frontend/demo.c` | 0x54c1f4 | 0x5d79f0 |
| `frontend/font.c` | 0x54cad4 | 0x5d7a18 |
| `frontend/frontend.c` | 0x54caf0 | 0x5da0b0 (`tile_buffer`), 0x5fa0b0 |
| `frontend/gvars.c` | 0x54ced8 (key name tables) | |
| `frontend/main.c` | 0x54ecd8 | 0x600bd0 |
| `flat/flat.c` | 0x550610 | 0x600ca8 |
| `D3DGlide/glide2x.c` | 0x551068 | 0x602a30 |
| `eax/cdaudio.c` | 0x55335c | 0xb9bb40 (IDA mark) |
| `debug.c` | 0x555e8c | |
| `input.c` | 0x564cb0 | |
| `pres.c` | 0x577d1c | |
| `shell.c` | 0x578240 | 0x57d8c8 (`gScreen` …) |
| `sound.c` | 0x5787dc | 0x5c89e0 |
| `asmmath.c` | 0x57c330 | |
| `tsd.c` | 0x57c410 | |
| `frontend/cpupoly.c` | | 0x57d9b0 – 0x5c89dd (`vscreen`) |
| `Engine/3dfx/gfxinit.c` | | 0xb9b314 (IDA mark) |
| `Engine/3dfx/tset.c` | | 0xb9b61c (IDA mark) |
