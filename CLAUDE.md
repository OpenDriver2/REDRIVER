# OpenDriver1 / Driver — the rebuilt Driver PC source

Reply in the user's language. This file and `docs/` are kept in English.

The Driver (PC) code from the IDA database is translated into `.c` source files under
`src_rebuild/game6.4g/`, laid out like the original source. Every function is translated; the work
now is verifying it against the original and fixing differences.

## Before editing

1. Read this file completely, and the docs below that the task needs.
2. Run `git status --short`. The worktree is shared and often dirty; assume every existing change
   belongs to the user or another agent.
3. Read [docs/COMMIT_MESSAGES.md](docs/COMMIT_MESSAGES.md) before committing.
4. Check whether another agent is working on the same files. Partition the work (by source file)
   or coordinate before editing overlapping code. The IDA database is shared too (one `ida-mcp`
   session, never modified).
5. At the start of each new task in a long-running session, and always right before committing,
   check whether this file or the docs you rely on changed since you read them
   (`git status --short -- CLAUDE.md docs/`, `git log -1 --format=%h -- CLAUDE.md docs/`).
   Re-read any that changed and follow the current version; other agents may update the rules
   in parallel sessions.

## Docs

| File | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | How the game fits together: main flow, Glide pointer table, CPU-drawn frontend, input/timing, data blocks per file |
| [docs/building.md](docs/building.md) | premake build, running from `data\`, dgVoodoo, `/WINDOW`, DirectInput |
| [docs/COMMIT_MESSAGES.md](docs/COMMIT_MESSAGES.md) | commit message format and the review-before-commit procedure |

## Key rules

- Do **not save or modify** the IDA database (renames, types) unless the user asks.
- Changes are checked against IDA; references (REDRIVER2, Glide SDK) are for orientation only —
  never copy reference functions. Never modify the reference repositories.
- Every function carries its address and status (`// [0xADDR] [D]`, see "Function status");
  intentional behaviour changes are `[A]` with a `// NOTE:` saying what and why. The agent sets
  only `[D]` and `[A]`.
- Mark guesses with `// NOTE:` and mention them in the reply.
- Build after changing code and fix errors before reporting; when running the game, back up
  `data\debug.log` first.

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

## Commits

- Commit only when the user asks. Write commit messages following
  [docs/COMMIT_MESSAGES.md](docs/COMMIT_MESSAGES.md).
- Before running `git commit`, prepare the commit following "Review before committing" in
  `docs/COMMIT_MESSAGES.md` (check the index, stage only the task files or use
  `git commit --only` when others have staged files), show the user the file list and the full
  commit message, and commit only after the user confirms.

Example of a finished file: `src_rebuild/game6.4g/frontend/cars.c`; with data-ownership analysis:
`src_rebuild/game6.4g/shell.c`.
