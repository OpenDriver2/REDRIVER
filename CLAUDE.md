# OpenDriver1 / Driver — rebuilding the source from IDA

Reply in the user's language. This file is kept in English.

Goal: translate the decompiled Driver (PC) code from the IDA database into `.c` source files under
`src_rebuild/game6.4g/`. That tree already holds empty placeholder files laid out like the original
source; the job is to fill them in. The long-term plan (finish the decompilation, then port to SDL
and OpenGL) is in [PLAN.md](PLAN.md).

## IDA access

- Database: `Driver_sym2_94.i64` (repo root). Usually open in IDA 9.4 GUI
  (`D:\Projects\LEARN\ida94\ida.exe`) with the Hex-Rays `ida-mcp` plugin.
- If the session has no IDA MCP tools, use the client in `tools/ida/`
  (it starts `ida-mcp stdio` itself and attaches to the open GUI database):

  ```bash
  python tools/ida/idacall.py list_databases                     # what is open
  python tools/ida/idacall.py py tools/ida/dump_folder.py        # list function folders
  python tools/ida/idacall.py py tools/ida/dump_folder.py FOLDER="frontend\cars.c" OUT=<scratchpad>/cars.txt
  python tools/ida/idacall.py py <your_script.py> NAME=VALUE     # any IDAPython; NAME=VALUE becomes a variable
  python tools/ida/idacall.py py tools/ida/call_tree.py ROOTS="InitFrontend,HandleMenu" DEPTH=5 OUT=<scratchpad>/tree.txt
  python tools/ida/idacall.py py tools/ida/heads.py RANGES="0x54cac0-0x54cb00,0x5fa0a0-0x5fa0f0"
  ```

- `call_tree.py` prints the call tree below functions (thunks followed): address, size, IDA
  folder, and `+` / `-` for translated / untranslated (an `// [0xADDR]` marker exists in
  `src_rebuild/game6.4g`). Use it to plan what a feature needs before dumping.
- `heads.py` lists the data items in address ranges with type, size, the user's marks and xrefs —
  for deciding data ownership (see "How to translate", step 2).

- `dump_folder.py` writes a report: decompilation of every function in the folder (disassembly when
  Hex-Rays fails), the user's IDA comments, the data the functions reference (decoded field by
  field, `<uninitialized (bss)>` for bss), called externals with prototypes and owning folder, and
  the struct definitions used. Put reports in the scratchpad, not in the repo.
- The first call can take about a minute (uv sets up its environment). Use a 300 s timeout.
- Do **not save or modify** the IDA database (renames, types) unless the user asks.
- IDA 9 Python: `ida_nalt.get_tinfo(tif, ea)` (not `ida_typeinf.get_tinfo`);
  `tinfo._print(name, flags)` with a name returns `None` — pass `None`.
- When passing Python source through a bash heredoc into another Python string, `\n`, `\3`, `\f`
  (as in `@frontend\frontend.c`), `\d` get mangled; edit files with the Edit tool, or write
  snippets with the Write tool and insert them with `insert_functions.py`.

## IDA folders → files

Function folders in IDA (Functions → folders) are the source files. Folder names contain a
backslash within one segment: `/frontend\cars.c` → `src_rebuild/game6.4g/frontend/cars.c`.

Some folders are named `NameInIDA=OriginalName`. The target file is whichever of the two exists in
`src_rebuild`:

| IDA folder | File |
|---|---|
| `frontend\highscores.c=frontend\highscore.c` | `frontend/highscores.c` |
| `frontend\selectrace.c=frontend\selrace.c` | `frontend/selrace.c` |
| `motel\*.c=flat\*.c` | `flat/*.c` |
| `motel\gameplayoptions.c=flat\options.c` and `motel\options.c=flat\options.c` | both into `flat/options.c` |
| `motel\path.c=flat\prescam.c` | `flat/prescam.c` |
| `pedest.c=pedestrian.c` | `pedest.c` |
| `asmmath.asm` | `asmmath.c` |

The `[THUNKS]` folder holds the `j__*` thunks and is not a file.

## IDA names

- `_Name` — global C symbol `Name`.
- `file._name` (`file__name` in the decompilation) — `static name` inside that file
  (e.g. `cars._menufunc` → `static FE_BUTTON menufunc[]` in `cars.c`).
- `j__Name` — thunk; call `Name` directly. Take the prototype from the real function, not the
  thunk (IDA often invents `__fastcall(_DWORD)` and extra arguments for thunks).
- `byte_XXXX`, `dword_XXXX`, `aXxx` — unnamed data; give it a meaningful name
  (`aPcarD` → string literal `"PCAR%d"`, `byte_57D7B8` → `static char carname[8]`).

## User comments in IDA

`dump_folder.py` lists them before each function (`// IDA user comments`) and on data items
(`// IDA comments:`). IDA's automatic comments (WinAPI parameter names, `switch jump`) are excluded.

- **Hex-Rays pseudocode comments** (~700 in ~200 functions). Empty ones (`"\n"`) are just paragraph
  breaks in the code: reproduce them as blank lines. Meaningful ones:
  - explanations (`NB: …`, labels like `Shadows`, `Fog` in `SetupDetailOptions`) — **copy into the
    code verbatim** as `// …` at the same spot;
  - code written as a comment (`if ( InputDevice == 2 )`, `memcpy(JoySet, DefaultJoySet,
    sizeof(JoySet))`, `switch ( exit_flag )`) — a hint of what the original source looked like:
    write the code that way, don't copy the comment;
  - notes about IDA mistakes (`IDA BUG: …`, `not unsigned :/`) — act on them (fix what they point
    at), don't copy them into the code.
- **Data ownership marks** — an anterior line above a data item:
  - `@file` — the item is defined in that file (`@frontend\cars.c`, `@Director.c`);
  - `@file;static` — `static` in that file;
  - `@file;extern`, `@file;unused` — rare (4 and 1); meaning not settled yet: don't interpret,
    keep the mark as a `// NOTE: IDA mark @file;extern` next to the declaration and move on;
  - a line with only a file name (`camera.c`, `frontend\cars.c`) — start of that file's data block.

  The user's marks override conclusions drawn from the layout (see "How to translate", step 2):
  if they disagree, follow the mark and say so in the reply.

## How to translate

Work goes either by file (a whole IDA folder) or down the call tree from `WinMain` (the way the
user rebuilt Driver 2): take the callees one level down, wherever they live, and add each to its
own file. Files are therefore often partial — untranslated functions simply have no entry yet;
declare what a function needs and leave the rest of the file for later. Dump chosen functions with
`dump_folder.py FUNCS="MainInit,DoFrontend,..."` (works across folders).

1. Dump the folder with `dump_folder.py` and read the whole report. Run `match_refs.py` to see
   whether the folder's functions exist in the reference sources (below).
2. Declare the file's data (statics and globals) in address order — that is how the compiler laid
   it out. Take values from the report; `<uninitialized (bss)>` → no initializer.
   Struct arrays shown untyped in the report (`raw dwords`) are decoded by hand from the struct
   definition (`FE_BUTTON` = 3 pointers + 4 bytes `l r u d`).
   **Who owns the data**, in order of trust:
   1. the user's `@file` marks (see above);
   2. `.data` layout: initialised data (and its string literals) of each object file is one
      contiguous chunk in link order, so a chunk recognised by its strings/statics owns every
      global inside it, even if xrefs make it look foreign (e.g. `shell.c` owns `gTrafficDensity`,
      `gDetailLevel` … at 0x578248). The same holds for the per-object zero/bss blocks around
      0x57d7xx and 0x5fa0xx/0x600bxx, which carry `@file` marks;
   3. the big bss region 0xb9xxxx–0xc2xxxx is the linker's **common area** and mixes globals from
      many files (general.c's `mem_allocated` sits among sound and debug globals), so position
      there proves nothing. Without a mark, define such a global provisionally in the file of its
      main user with a `// NOTE:`, or leave it `extern`.
   Globals that belong elsewhere become `extern` with a `// NOTE:`; while their owner is not
   translated, `gen_stubs.py` defines them with the IDA initial bytes, so leaving an unowned global
   `extern` is safe. Inspect the neighbourhood with `tools/ida/heads.py`. Keep unnamed unused
   slots inside an owned chunk as `static int unknown_XXXX;` so the layout stays intact; at an
   unclear chunk boundary leave them out and say so. Strings with no xrefs in the middle of a
   chunk are left over from a function the linker dropped; don't rebuild it, mention it in the
   file header. `FE_POLY` lists end with an entry whose text points at a small static bss buffer
   (`poly_end_text`), not at `""`.
   If Hex-Rays fails (usually a switch with a two-level jump table), `dump_folder.py` prints the
   disassembly — translate from that, with a `// NOTE:`.
3. Functions go in address order, each preceded by its address and status:
   `// [0xADDR] [D]` (see "Function status"). To add functions to an existing file, write them
   (with their markers) into a scratch file and run
   `python tools/insert_functions.py <target.c> <snippets.c>` — it places each by address.
4. Clean up decompiler artifacts while keeping behaviour identical:
   `v1/v2` → real names; `for (i = Open(...); ; i = Draw())` → a normal loop; drop bogus arguments
   and return values of `void` functions; simplify `!= 0` and nested `if`s; drop dead computations
   (a value computed and never used).
5. Keep magic numbers as they are unless the database names them; use the Windows/Glide constant
   when the API is known (`0x7F00` → `IDC_ARROW`); if the meaning is obvious from the code,
   add a `#define` in the file (e.g. `NUM_CHEAT_CARS 18`).
6. Mark guesses (anything not directly backed by the database) with `// NOTE:` and mention them in
   the reply.
7. Until shared headers exist, the structs and `extern` declarations a file needs go at the top of
   each `.c` (see `frontend/cars.c`). Style: tabs, braces on their own line, a `/* */` file header
   naming the IDA folder and address range.
8. `char` is unsigned: the game appears to be built with MSVC `/J` (the
   decompilation zero-extends `char` fields like `(unsigned __int8)config_data.JoyIsAnalogue`).
   Write plain assignments; don't reproduce those casts.
9. Old platform helpers (inline FPU sequences like `float_to_int`, `_ftol`) need not be bit-exact:
   a plain C cast is fine. `float_to_int` already exists in `asmmath.c` and stays.
10. Not yet translated `static` functions referenced by translated code (menu callbacks …) are
    declared without `static` with a `// NOTE:` so the stub can define them; make them `static`
    again when translated.
11. After translating, build (see "Build") and fix errors before reporting.

## Build

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

## Function status

Marks as in REDRIVER2 — cumulative, space separated, after the address:
`// [0x404610] [D]` → `// [0x404610] [D] [T]` → `// [0x404610] [D] [T] [F]`.

| Mark | Meaning |
|---|---|
| `[D]` | Decompiled: translated from IDA and cleaned up; behaviour matches the original by reading, not verified by running. |
| `[T]` | Tested: verified to behave 1:1 with the original (compared against the game/binary). |
| `[F]` | Final: after `[T]`, names, types, constants and comments are finished; don't touch without a reason. |
| `[A]` | Altered/Added. With `[D]` (`[D] [A]`) — behaviour **intentionally** differs from the original (bug fix, platform code replacement); a `// NOTE:` saying what and why is required. A lone `// [A]` without an address — a function or data that does not exist in the original (added by us). |

- The agent only sets `[D]` and `[A]` itself. `[T]` and `[F]` only when the user confirmed the check
  or explicitly asked.
- A behaviour-changing edit to a `[T]`/`[F]` function resets its marks to `[D]`.
- A function without a mark is not translated yet (or the translation is incomplete).

## Reference sources

Three sources, most reliable first. Run the matcher before translating a folder:

```bash
python tools/ida/idacall.py py tools/ida/list_functions.py OUT=<scratchpad>/d1funcs.tsv
python tools/ida/match_refs.py <scratchpad>/d1funcs.tsv                 # per-folder summary
python tools/ida/match_refs.py <scratchpad>/d1funcs.tsv "handling.c"    # function → file:line in REDRIVER2
```

Matching is by name only: search manually for renamed or rewritten functions (grep for constants,
strings, characteristic calls). Mark what came from a reference with a short `// NOTE:`:
`// NOTE: known constant` — the name is known from a reference (the value is the game's own);
`// NOTE: known value` — a reference confirms the value/meaning of a number.

### 1. Original Reflections source

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

### 2. `src_rebuild/dependency/glideSDK` — 3dfx Glide 2.x SDK

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

The Glide types are **already loaded into the database** (2026-10-27): structs (`GrVertex`,
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

### 3. REDRIVER2 — reconstruction of Driver 2 (PlayStation)

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

### No counterparts

`frontend\*`, `motel\*` (PC menus), `eax\*`, `input.c`, `pause.c`, `MScript.c`, `Director.c`,
`DirectorStorage.c`, `interview.c`, `roadbits.c`, `pathfind.c` — IDA only.

Example of a finished file: `src_rebuild/game6.4g/frontend/cars.c`; with data-ownership analysis:
`src_rebuild/game6.4g/shell.c`.
