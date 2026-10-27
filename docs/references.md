# Reference sources

Three sources, most reliable first. Run the matcher before translating a folder:

```bash
python tools/ida/match_refs.py tools/ida/functions.tsv                 # per-folder summary
python tools/ida/match_refs.py tools/ida/functions.tsv "handling.c"    # function → file:line in REDRIVER2
```

Matching is by name only: search manually for renamed or rewritten functions (grep for constants,
strings, characteristic calls). Mark what came from a reference with a short `// NOTE:`:
`// NOTE: known constant` — the name is known from a reference (the value is the game's own);
`// NOTE: known value` — a reference confirms the value/meaning of a number.

## 1. Original Reflections source

This is **real code from the same studio**, and many files are shared with the game. Use it **for
orientation only**: the translation is always written from IDA, never by copying the reference
functions or their `/*****` header blocks. What may be taken from it is the meaning of constants
— `#define` names and values the code uses (e.g. `MAX_MEM_PACKETS`), marked `// NOTE: known
constant` / `// NOTE: known value` — and a
hint of what a tangled decompilation is doing. The reference and the game were built from different
versions, so values differ (`MAX_MEM_PACKETS` is 2048 in the reference, 4096 in the game).

| IDA folder | Matched | Where in the reference source |
|---|---|---|
| `D3DGlide\glide2x.c` | 150/186 | `D3DGlide/glide2x.c` |
| `D3DGlide\system.c` | 8/19 | `D3DGlide/glide2x.c` |
| `Engine\3dfx\polydraw.c` | 38/107 | `Engine/3dfx/polydraw.c` |
| `Engine\3dfx\object.c` | 12/28 | `Engine/3dfx/OBJECT.C` |
| `Engine\3dfx\gfxinit.c`, `tset.c` | 5/17, 5/10 | `Engine/3dfx/gfxinit.c`, `mapdraw.c` |
| `testmath.c` | 36/46 | `testmath.c` |
| `debug.c` | 10/12 | `DebugTxt.C` |
| `map.c` | 10/52 | `model.c`, `mapdraw.c`, `cell.c` (plus REDRIVER2 `map.c`) |
| `motion_c.c`, `file.c`, `lamps.c`, `event.c`, `general.c` | a few each | same-named files, `main.c`, `events.c` |

Also useful there (constant names and meanings): `asmmath.asm`/`asmmath.h`,
`Engine/include/TYPES.H`, `mdl.h`, `SINTAB.C`, `TSD.h`, `cell.h`, `chairs.c`, `lamps.h`.

**Glide in the game.** The engine (`Engine\3dfx\*`) calls Glide through a table of function
pointers `grX` (`grDrawTriangle`, `grBufferClear`, …). `UseD3DDevice` fills it with the emulation
functions `d3dgl_X` / `d3dgu_X` from `D3DGlide\glide2x.c`; for a real 3dfx card, with addresses
from `glide2x.dll` (strings `_grX@N`). In the reference source the same emulation is simply named `grX`/`guX`
(linked statically) — when translating `D3DGlide\glide2x.c`, name the functions `d3dgl_X` and write
them from IDA; its `grX` only helps to read them. `match_refs.py` accounts for this renaming.

## 2. `src_rebuild/dependency/glideSDK` — 3dfx Glide 2.x SDK

`glide2x/include/glide.h` (+ `3DFX.H`, `glideutl.h`, `sst1vid.h`) — exact prototypes of `grX`/`guX`
(`FX_CALL` = `__stdcall`), types (`GrVertex`, `GrColor_t`, `GrState`, …) and all `GR_*` constants.
For `D3DGlide\*` and `Engine\3dfx\*` code:
- take prototypes and `grX` pointer types from `glide.h`, not from IDA;
- replace magic numbers in Glide call arguments with `GR_*` constants
  (`grDepthBufferMode(1)` → `grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER)`), checking the value in `glide.h`;
- include it as `#define FX_GLIDE_NO_FUNC_PROTO` + `#include <glide.h>` (types and `GR_*` only,
  no function prototypes — the game calls Glide through pointers), then declare the pointers it
  uses, e.g. `extern void (FX_CALL *grClipWindow)(FxU32, FxU32, FxU32, FxU32);`. Don't write
  private copies of the Glide types.

The Glide types are **already loaded into the database** (`Driver_sym2.i64`, reloaded 2026-10-27
after the switch to it): structs (`GrVertex`,
`GrLfbInfo_t`, `GrHwConfiguration`, …), 38 enums built from the `GR_*` groups
(`GrDepthBufferMode_t`, `GrCmpFnc_t`, `GrChipID_t`, …) and `glide.h` prototypes on the
`d3dgl_X`/`d3dgu_X` functions and on the `grX`/`gr_X`/`guX` pointers. So the decompilation already
shows `grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER)`.
`grGlideInit`, `grSstWinOpen`, `grSstControl`, `grLfbLock` and a few other `grX` without `_` are
**engine wrappers** (plain `__cdecl` functions calling the `gr_X` pointers), not the Glide API; they
did not get glide.h prototypes. With `FX_GLIDE_NO_FUNC_PROTO` their names don't clash with
`glide.h`. To reload (if the database is rolled back):

```bash
python tools/ida/glide_decls.py <scratchpad>/glide                       # glide.h → flat declarations
python tools/ida/idacall.py py tools/ida/load_glide.py DIR=<scratchpad>/glide DRY=1   # check
python tools/ida/idacall.py py tools/ida/load_glide.py DIR=<scratchpad>/glide         # load
```

IDA's clang parser (`ida_srclang`) hangs on these headers — use the built-in parser as above.

## 3. REDRIVER2 — reconstruction of Driver 2 (PlayStation)

`D:\Projects\OpenDriver2\ReDriver2\code\src_rebuild\Game` — the same engine, next game.
Many Driver 1 gameplay functions are there almost verbatim, with field and macro names worked out.
The repository has **bug fixes and improvements** on top of the decompilation, so behaviour may
differ from the original. Rely mainly on functions marked `[D] [T]` without `[A]`; code marked `[A]`
and the platform layers (`PsyCross`, `platform`) are not original.

- Most overlap (matched / total): `handling.c` 16/26 (`handling.c`, `wheelforces.c`),
  `debris.c` 31/89 (`debris.c`, `cosmetic.c`), `pedest.c` 14/48, `map.c` 11/52,
  `overlay.c` 10/47 (`overlay.c`, `overmap.c`), `bcollide.c` 6/7, `bcoll3d.c` 3/3, `shadow.c` 6/12,
  `effects.c` 6/28 and `smashable.c` 4/6 (both → `debris.c`), `denting.c`, `cops.c` (→ `cop_ai.c`),
  `camera.c`, `felony.c`, `gsound.c` (→ `gamesnd.c`).
- What to take: variable and field names, the meaning of magic numbers (`GRAVITY`, `FIXEDH`,
  `RSIN`/`RCOS` from `dr2math.h`, `dr2limits.h`, `dr2types.h`), code structure when the
  decompilation is tangled.
- How it differs (e.g. `nose_down`, `InitCarPhysics`): in Driver 1 `_CAR_DATA` has no `hd`
  sub-struct (`cp->where`, not `cp->hd.where`), fewer parameters (`InitCarPhysics(cp, startpos)`
  without `direction`), different logic.

## No counterparts

`frontend\*`, `motel\*` (PC menus), `eax\*`, `input.c`, `pause.c`, `MScript.c`, `Director.c`,
`DirectorStorage.c`, `interview.c`, `roadbits.c`, `pathfind.c` — IDA only.

Example of a finished file: `src_rebuild/game6.4g/frontend/cars.c`; with data-ownership analysis:
`src_rebuild/game6.4g/shell.c`.
