# IDA database

How to read the database and what its names, folders and the user's comments mean.

## IDA access

- Database: `Driver_sym2.i64` (repo root; restored from a backup on 2026-10-27 after
  `Driver_sym2_94.i64` stopped opening). Usually open in IDA 9.4 GUI
  (`D:\Projects\LEARN\ida94\ida.exe`) with the Hex-Rays `ida-mcp` plugin. `idacall.py py` uses
  whichever database the GUI has open.
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
  for deciding data ownership (see [translating.md](translating.md), step 2).
- `tools/ida/functions.tsv` — the database's function list (folder, address, name), exported with
  `list_functions.py OUT=<repo>/tools/ida/functions.tsv`. Commit it; re-export when the user
  changes function folders or names in IDA. Tools that work without IDA read it:
  - `python tools/progress.py` — translation progress per file (`[D]` / `[T]` / `[A]` counts from
    the `// [0xADDR]` markers), `python tools/progress.py "frontend\main.c"` — one folder function
    by function, `--todo` — what is left in partly translated files;
  - `match_refs.py` (see [references.md](references.md)).
- `tools/ida/types.tsv` — the local types (types folder, kind, size, origin, name), exported with
  `list_types.py OUT=<repo>/tools/ida/types.tsv`. Origin `system` (Windows/DirectX/CRT: in a
  standard type library, or a DirectX/SEH name in the root folder), `glide` (named in the Glide SDK
  headers) or `game` (everything else; the user groups them in folders named after the original
  headers, `cars.h`, `frontend\frontend.h`, …).
- `tools/ida/type_usage.tsv` — for each type, the function folders using it (prototype, Hex-Rays
  locals, globals and members accessed; about a minute, it decompiles every function) and the
  named types its definition needs; exported with `type_usage.py OUT=<repo>/tools/ida/type_usage.tsv`.
  Re-export both when the user changes types or folders. `python tools/progress.py --types` turns
  them into a table: for every game type, where it is defined in the source now, which files need it
  (users plus files that need a type built on it) and where it belongs (one file → that `.c`,
  several → `src_rebuild/game6.4g/dr_types.h`); `--types NAME` shows one type in full.
- `tools/ida/globals.tsv` — the global data items (address, name, size, IDA declaration, the user's
  `@file` mark, referencing function folders with reference counts), exported with
  `list_globals.py OUT=<repo>/tools/ida/globals.tsv`. `python tools/progress.py --globals` gives
  each global an owner — the file defining it in the source, else the `@file` mark, else its only
  user, else (a guess) the file referencing it most — and per owner file counts the globals other
  files use, i.e. what its header (`<file>.h`) would declare `extern`;
  `--globals <file.c>` prints that header's draft (IDA types turned into C, guesses and
  not-yet-defined globals commented; globals already declared in a header are named, not
  repeated); `--globals --externs` lists the hand-written `extern`s in the sources with the header
  that would replace each.

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

  The user's marks override conclusions drawn from the layout (see [translating.md](translating.md), step 2):
  if they disagree, follow the mark and say so in the reply.
