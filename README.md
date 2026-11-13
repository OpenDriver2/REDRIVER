# REDRIVER (Reverse-Engineered Driver)

### Information
- This is the original **Driver** (Reflections Interactive, 1999), PC version — an
  **AI-assisted decompilation**: the executable is translated back to C function by function, with
  an AI agent working from the IDA database under the author's review.

- The code keeps the original platform layer (Win32, DirectDraw / Direct3D 6 under the
  game's own Glide-to-Direct3D layer, DirectInput 5, DirectSound / EAX) in the *master* branch.
- This specific branch is a *platform-update* - a port to SDL and OpenGL, OpenAL that removes all of the legacy code + game fixes applied on top of it

### How is it done?
- An IDA database of the PC executable (mapped from iOS version symbols) with recovered names, types and data layout; the source
  tree is laid out like the original (`game6.4g`), and IDA function folders map to its files.
- The [REDRIVER2](https://github.com/OpenDriver2/REDRIVER2) reconstruction of Driver 2 (same
  engine), as well as [OpenDriverTools](https://github.com/OpenDriver2/OpenDriverTools) is used for orientation — names and meanings of constants, structure of tangled code.


### How to use
See [Wiki/Installation Instructions](https://github.com/OpenDriver2/REDRIVER2/wiki/Installation-instructions)

### How can I contribute?
See [Contributing to project](https://github.com/OpenDriver2/REDRIVER2/wiki/Contributing-to-project) page

### What's next
- Verify the decompilation against the original, then port to SDL and OpenGL.

### Credits
- **Reflections Interactive** — the original game
