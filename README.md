# REDRIVERPC (Reverse-Engineered Driver PC)

### Information
- This is the original **Driver** (Reflections Interactive, 1999), PC version — an
  **AI-assisted decompilation**: the executable is translated back to C function by function, with
  an AI agent working from the IDA database under the author's review.
- **Work in progress.** Every function is translated; the code is being verified against the
  original.
- The code keeps the original platform layer for now (Win32, DirectDraw / Direct3D 6 under the
  game's own Glide-to-Direct3D layer, DirectInput 5, DirectSound / EAX). A port to SDL and OpenGL comes
  after the decompilation is complete.
- For compatibility it currently uses the 3dfx Glide 2.x SDK headers (Glide types and constants)
  and runs through [dgVoodoo 2](http://dege.freeweb.hu/) (DirectDraw / Direct3D wrapper).

### How is it done?
- An IDA database of the PC executable with recovered names, types and data layout; the source
  tree is laid out like the original (`game6.4g`), and IDA function folders map to its files.
- The [REDRIVER2](https://github.com/OpenDriver2/REDRIVER2) reconstruction of Driver 2 (same
  engine) is used for orientation — names and meanings of constants, structure of tangled code.
  The translation itself is always written from the binary.
- The 3dfx Glide 2.x SDK for the Glide types and constants used by the engine.
- Every function carries its address and status (`[D]` decompiled, `[T]` tested, `[F]` final,
  `[A]` altered on purpose), as in REDRIVER2. Details: [CLAUDE.md](CLAUDE.md).

Progress:
- WinMain, window and Direct3D initialisation through the Glide-to-Direct3D layer
- splash screens (copyright, GT Interactive, Reflections)
- frontend main menu: texture loading, CPU-drawn 8 bit screen, fonts, DirectInput, menu
  navigation and the quit question
- next: frontend sub-menus, then the game itself

### How to build
Windows, Visual Studio 2022 (MSVC x86) and the Windows 10 SDK.

```
cd src_rebuild
gen_vc2022.bat          # generates build/OpenDriver1.sln with premake
build_vc2022.bat Debug  # or open the solution in Visual Studio
```

The game data is not included: point `DRIVER_DATA_DIR` (default `../data`) at an installed copy
of Driver; it is used as the working directory. A DirectDraw wrapper (dgVoodoo) from
`data\dgvoodoo` is copied next to the exe, since modern systems offer no hardware Direct3D 6
device. Run with `/WINDOW` for a window instead of full screen. More in
[docs/building.md](docs/building.md).

### How can I contribute?
Start with [CLAUDE.md](CLAUDE.md) and the [docs](docs/) — they describe the workflow (function
status marks, build, commit messages) for people and agents alike.

### What's next
- Verify the decompilation against the original, then port to SDL and OpenGL.

### Credits
- **Reflections Interactive** — the original game
