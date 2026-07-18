# CMake/ — AGDE toolchain support

CMake toolchain for generating AGDE-style (Android Game Development Extension) Visual
Studio projects targeting the `Android-arm64-v8a` MSBuild platform, matching the form
of the hand-written projects in `build_android_vs/`.

- `AGDE.toolchain.cmake` — the toolchain; uses a custom `CMAKE_SYSTEM_NAME` (`AndroidAGDE`)
  to bypass CMake's legacy VS-Android emission, and injects AGDE MSBuild properties via
  `CMAKE_VS_GLOBALS`. Provides `agde_enable_packaging()` for APK-producing targets.
- `Platform/AndroidAGDE.cmake` — platform module for the custom system name.
- `AGDETest/` — standalone smoke-test project (also builds on a host compiler).

See `README.md` here for usage, Windows verification steps, and caveats. Generation and
building require a Windows host with VS2022 + AGDE + NDK; only parse checks and a host
build of the smoke test are possible on Linux.
