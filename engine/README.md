# Fame Boy engine source

This folder holds the source of Fame Boy's native engine library (`libfameboy.so`, inside the APK under `lib/`), so it can be rebuilt and relinked:

- `native/bridge.c` — the bridge between the app and the emulator engine (MPL-2.0, see `LICENSE-MPL-2.0.txt`).
- `native/Native.java` — the app side of that bridge: the calls the app makes into `libfameboy.so`.
- `native/engine_check.c` — a small desktop check that runs the engine without the app.
- `CMakeLists.txt` and `tools/build_native.py` — the build used for Fame Boy 1.0.2.
- `blip_buf/` — the blip_buf sound library (`blip_buf.c`, and `blip_buf.h` from `include/mgba/core`) exactly as shipped inside mGBA 0.10.5 (GNU LGPL 2.1 or later, see `blip_buf/license.txt`).
- `UPSTREAM.json` — the mGBA release used, with its archive checksum. mGBA is used unchanged.

## Rebuild

1. Make a folder with `CMakeLists.txt`, `native/` and `tools/` from here.
2. Unpack mGBA 0.10.5 (https://github.com/mgba-emu/mgba/archive/refs/tags/0.10.5.tar.gz) into `vendor/mgba-0.10.5` in that folder. Its `src/third-party/blip_buf` is the same as `blip_buf/` here; to use a changed blip_buf, replace those files.
3. Run `python3 tools/build_native.py --ndk /path/to/android-ndk` (tested with NDK r30, CMake and Ninja). The libraries land in `prebuilt/<abi>/libfameboy.so`.
4. To use your build, replace `lib/<abi>/libfameboy.so` in a copy of `FameBoy.apk`, zip-align it and sign it with your own key. Android treats a copy signed with a different key as a separate install, so uninstall the original first (export a backup in Fame Boy before you do).

Fame Boy is not made, endorsed or sponsored by Nintendo. No games or console software are included.
