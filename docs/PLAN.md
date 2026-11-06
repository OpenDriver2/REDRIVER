# OpenDriver1 — plan

Agent instructions for the current work are in [CLAUDE.md](../CLAUDE.md). This file is the
long-term plan; update it when a stage is reached or the plan changes.

## Stage 1 — decompile everything (done)

Translate the whole game from the IDA database into `src_rebuild/game6.4g/`, keeping the original
platform code (Win32, DirectDraw/Direct3D 6 through the Glide emulation, DirectInput 5,
DirectSound/EAX, MCI CD audio, multimedia timers). The standalone exe links the untranslated rest
from generated stubs, so the game runs further with every translated function.

Done when `stubs.c` is empty (no unresolved symbols left) and every function has at least `[D]`.

Progress so far (2026-11-06): all 2315 functions are translated and `stubs.c` is empty. The Glide
polygon functions of `Engine/3dfx/polydraw.c` match their D3D pairs apart from the projection and the
draw calls, and only the D3D path has been run. The next stage is the verification.

## Stage 2 — verify (current)

Compare against the original `Game.exe` (`data\`; a different build than the IDA database) and
mark verified functions `[T]`, then `[F]`. Fix behaviour differences found on the way.

## Stage 3 — port to SDL and OpenGL

Only after stage 1 (and ideally most of stage 2): the decompiled code must stay a faithful
reference while it is being finished. The port is done as a platform layer under the game code,
like REDRIVER2's `PsyCross` / `platform`: game files keep their `[D]` code; replaced platform code
is marked `[A]` (see [translating.md](translating.md#function-status)).

| Original | Replacement |
|---|---|
| Win32 window, message pump (`shell.c`: `InitWindow`, `WndProc`, `HandleWinMessages`) | SDL window and events |
| Glide through the D3D6 emulation (`D3DGlide/glide2x.c`, `system.c`): the `grX` pointer table | an OpenGL backend that fills the same table (`gl_X` functions); the engine (`Engine/3dfx/*`) keeps calling `grX` |
| Linear frame buffer access (`grLfbLock` + `frontend/cpupoly.c`, `tsd.c`: the frontend's 8 bit `vscreen`) | convert `vscreen` through `pallookup` and upload it as a texture / draw it as a quad |
| DirectInput 5 (`input.c`): keyboard, mouse, joystick, force feedback | SDL keyboard (scancodes mapped to the `DIK_*` values in `KeyMap`), SDL game controller and haptics |
| DirectSound / EAX (`sound.c`, `eax/*`) | SDL audio or OpenAL (EAX effects approximated or dropped) |
| MCI CD audio (`eax/cdaudio.c`) | music from files (ripped tracks) |
| Multimedia timers (`time.c`: `timeSetEvent`), `timeGetTime` | SDL timers / `SDL_GetTicks` |
| FMV (`PlayRender`) | a video decoder for the original movies, or skipped at first |
| Inline x86 assembly (`asmmath.c`, `tsd.c`) | plain C (already the case for `tsd.c`) |

Further steps after the port works: 64-bit builds (pointers stored as `int`, e.g.
`gRamTextureRawDataDump`, and struct layouts read from files), other platforms, then optional
improvements (resolution, widescreen), each kept switchable from the original behaviour.
