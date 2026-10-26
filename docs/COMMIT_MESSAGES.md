# Commit messages

Rules for every commit in this repository (adopted from another project of the user's).

## Format

Every line of the message is one change, written as a list item:

```
- <scope>: <change>
```

A commit with several changes has one line per change, without a separate subject line or blank
lines:

```
- frontend: implement InitFrontend, DrawFrontend, HandleMenu
- frontend: add cpupoly.c CPU drawing into the 8 bit vscreen
- input: implement DirectInput keyboard, mouse and joystick
```

## Rules

- Start every line with `- ` (dash and space).
- `<scope>` — the area changed, lowercase:
  - game source: the file or folder name under `src_rebuild/game6.4g` without `.c` — `shell`,
    `frontend`, `flat`, `input`, `sound`, `eax`, `general`, `pres`, `map`, `tset`, `gfxinit`,
    `polydraw`, `d3dglide`, … A folder scope covers its files (`frontend` for `frontend/main.c`);
    name the file when it matters (`frontend: font - ...`);
  - `stubs` (`src_rebuild/stubs`), `premake` (build files), `deps` (`src_rebuild/dependency`),
    `tools` (`tools/`, a tool may follow: `tools: call_tree - ...`), `docs` (`CLAUDE.md`,
    `docs/*.md`).
- `<change>` — short description starting with a lowercase verb: `implement ...` (translated from
  IDA), `fix ...`, `add ...`, `rename A -> B`, `remove ...`, `refactor ...`. Code identifiers keep
  their original case (`implement LoadFrontendTextures, AddTSet`).
- Mark intentional changes of original behaviour (`[A]`, see [translating.md](translating.md#function-status)) in the
  line: `- shell: [A] add /WINDOW windowed mode`.
- One line per logical change; keep lines short (about 100 characters at most).
- No trailing period.
- Omit the scope only for changes that do not belong to one area (`- fix compile error`).
- `Co-Authored-By` trailers are allowed (e.g. for agent-assisted commits). Put them after the
  change lines, separated by one blank line:

  ```
  - input: implement ReadInput, ReadKeyboardInput

  Co-Authored-By: Name <email>
  ```

  Other trailers (`Signed-off-by`, ...) are not used.
- Merge commits keep the default git message (`Merge branch 'develop' into ...`).

## Review before committing

Agents never run `git commit` right away:

1. Check the index first: `git diff --cached --name-only`. Never unstage, reset, stash or modify
   files staged by others — they may be a deliberate partial selection.
   - **Index is empty** — stage the files of the task with `git add <files>` (never
     `git add -A` / `git add .`). Review list: `git diff --cached --stat`. Commit: `git commit`.
   - **Index has files of others that do not overlap with the task** — do not stage the task
     files. Review list: `git diff HEAD --stat -- <files>`, plus the pre-staged files listed as
     "not included". Commit: `git commit --only -- <files>`, which commits the listed paths as
     they are in the working tree and leaves the rest of the index untouched.
   - **Index overlaps with the task files** — stop and ask the user.
2. Present for review:
   - the file list from step 1, so the user can also inspect the diff;
   - the complete commit message, exactly as it will be committed, including trailers.
3. Commit only after the user approves. If the user requests changes, update the message or the
   file list (only the task's own staging) and present it again.

`src_rebuild/stubs/stubs.c` is generated; commit it together with the translation that changed it.

### Reporting to the user

Follow the procedure above without narrating it; the user expects every step to be done.

- Review request: the line "Ready for review" (in the user's language),
  then the file list and the full commit message. Nothing else.
- After committing: a single line `Committed <hash> -> <branch>`.
- Mention a check only when it found something: the rules or docs changed, the index has files of
  others, files changed after approval.
- Do not mention unchanged rules, that nothing is committed yet, line lengths, the git command
  that will be used, or untracked files left out — the file list already shows what is included.

## Examples

```
- frontend: implement LoadFrontendTextures, AllocateTSet, AddTSet
- tsd: implement the 8 bit blitters in C (assembly in the original)
- d3dglide: implement d3dgl_LfbLock, d3dgl_LfbUnlock
- shell: [A] add /WINDOW windowed mode
- tools: add call_tree.py and heads.py
- docs: add PLAN.md (port to SDL and OpenGL)
```
