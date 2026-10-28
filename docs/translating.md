# Translating

How to turn IDA output into source files, and the function status marks.

## How to translate

Work goes either by file (a whole IDA folder) or down the call tree from `WinMain` (the way the
user rebuilt Driver 2): take the callees one level down, wherever they live, and add each to its
own file. Files are therefore often partial — untranslated functions simply have no entry yet;
declare what a function needs and leave the rest of the file for later. Dump chosen functions with
`dump_folder.py FUNCS="MainInit,DoFrontend,..."` (works across folders).

1. Dump the folder with `dump_folder.py` and read the whole report. Run `match_refs.py` to see
   whether the folder's functions exist in the reference sources ([references.md](references.md)).
2. Declare the file's data (statics and globals) in address order — that is how the compiler laid
   it out. Take values from the report; `<uninitialized (bss)>` → no initializer.
   Struct arrays shown untyped in the report (`raw dwords`) are decoded by hand from the struct
   definition (`FE_BUTTON` = 3 pointers + 4 bytes `l r u d`).
   **Who owns the data**, in order of trust:
   1. the user's `@file` marks ([ida.md](ida.md#user-comments-in-ida));
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
7. Types: a type several files need lives in `src_rebuild/game6.4g/dr_types.h` ([A], grouped by
   the IDA types folders; it also brings `<windows.h>`, `<stdint.h>`, `<glide.h>`, `byte`,
   `u_char`); a type only one file needs is defined at the top of that `.c`. Check with
   `python tools/progress.py --types NAME`; never copy a type into a second file — move it to
   `dr_types.h` (layout from IDA) and `#include "dr_types.h"` instead.
   Globals and functions other files use are declared in their owner's header (`<file>.h` next to
   the `.c`, e.g. `frontend/config.h`; `dr_time.h` for `time.c`, `dr_scores.h` for the top-level `scores.c`, which
   `frontend/scores.h` would hide from files in `frontend/`), and a global with several users
   and no known owner (no IDA `@file` mark) in `dr_globals.h`. These headers are maintained by
   **`python tools/headers.py`** (`--dry` to preview): run it after translating, then build. It
   rewrites the block between the GENERATED markers of each header — prototypes of the non-static
   functions other files call (from their definitions), the globals (owner and type from
   `tools/ida/globals.tsv`), and the not yet translated functions translated code calls (owner from
   `tools/ida/functions.tsv`) — and in the `.c` files replaces every hand-written `extern` with
   `#include` of the right header. So while translating, call or use anything freely and declare
   it with a plain `extern` at the top of the `.c` if the compiler needs it; the tool moves it.
   Each `.c` includes its own header, so the compiler checks declarations against definitions.
   Hand-written declarations in a header go outside the markers (e.g. `gWindowed` in `shell.h`).
   An `extern` that must stay in its `.c` goes under a comment containing
   `tools/headers.py: keep` (glide2x.c cannot include `Engine/3dfx/gfxinit.h` because of the two
   static `glide_active`). A prototype using a type defined only in its `.c` is not exported (the
   tool reports it): move the type to `dr_types.h` if other files need the function.
   Style: tabs, braces on their own line, a `/* */` file header naming the IDA folder and address
   range.
8. `char` is unsigned: the game appears to be built with MSVC `/J` (the
   decompilation zero-extends `char` fields like `(unsigned __int8)config_data.JoyIsAnalogue`).
   Write plain assignments; don't reproduce those casts.
9. Old platform helpers (inline FPU sequences like `float_to_int`, `_ftol`) need not be bit-exact:
   a plain C cast is fine. `float_to_int` already exists in `asmmath.c` and stays.
10. Not yet translated `static` functions referenced by translated code (menu callbacks …) are
    declared without `static` with a `// NOTE:` so the stub can define them; make them `static`
    again when translated.
11. After translating, run `python tools/headers.py`, then build (see [building.md](building.md))
    and fix errors before reporting.

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
