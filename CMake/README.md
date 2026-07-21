# CMake support for AGDE (Android Game Development Extension)

This directory contains a CMake toolchain that makes the Visual Studio generator emit
AGDE-style project files — vcxproj files targeting the MSBuild platform
`Android-arm64-v8a` with the `Clang` toolset, the same form as the hand-written
projects in `build_android_vs/`.

| File | Purpose |
|------|---------|
| `AGDE.toolchain.cmake` | The toolchain file. Selects the AGDE platform/toolset, injects `AndroidMinSdkVersion` / `AndroidNdkVersion` / `CppLanguageStandard` into every generated project, and provides `agde_enable_packaging()` for application targets. |
| `Platform/AndroidAGDE.cmake` | Platform module for the custom `AndroidAGDE` system name (artefact naming, `ANDROID`/`UNIX` variables). Loaded automatically. |
| `AGDETest/` | Standalone smoke-test project (one static library, one application) for verifying generation and building without touching the main Teleport build. |

## Why a custom system name?

AGDE does not officially support CMake, and CMake's built-in Visual Studio support for
`CMAKE_SYSTEM_NAME=Android` targets the *legacy* "Visual Studio Tools for Android"
component instead: it emits `<ApplicationType>Android</ApplicationType>`, which reroutes
MSBuild to `$(VCTargetsPath)\Application Type\Android\...` — a directory that does not
contain AGDE's platforms. AGDE resolves its platforms purely from the `<Platform>`
element of an ordinary `Win32Proj`-style vcxproj, which is exactly what the Visual
Studio generator produces for any system name it does not special-case. The toolchain
therefore sets `CMAKE_SYSTEM_NAME=AndroidAGDE` and passes the platform and toolset
through the ordinary `-A` / `-T` mechanism.

## Prerequisites (Windows host)

- Visual Studio 2022 with the C++ workload.
- The AGDE extension — CI installs `AndroidGameDevelopmentExtension-2022-v26.1.102.vsix`
  (see `.github/workflows/build_android_client.yml`).
- Android NDK `27.2.12479018` (r27c — pinned in `release.properties`), with
  `ANDROID_NDK_HOME` or `ANDROID_NDK_ROOT` set so both AGDE and the toolchain can find it.
- CMake 3.24 or later (for `CMAKE_VS_NO_COMPILE_BATCHING`).

## Usage

Smoke test (recommended first):

```bat
cmake -B build_agde_test -S Teleport/CMake/AGDETest -G "Visual Studio 17 2022" ^
      -A Android-arm64-v8a -T Clang -DCMAKE_TOOLCHAIN_FILE=../AGDE.toolchain.cmake
cmake --build build_agde_test --config Release
```

Main project (once its CMakeLists gains Android target support):

```bat
cmake -B build_agde -S Teleport -G "Visual Studio 17 2022" ^
      -A Android-arm64-v8a -T Clang -DCMAKE_TOOLCHAIN_FILE=CMake/AGDE.toolchain.cmake
```

`-A Android-arm64-v8a -T Clang` are the defaults and may be omitted. Other ABIs
(`Android-x86_64`, `Android-armeabi-v7a`, `Android-x86`) can be selected with `-A`,
one ABI per build tree, matching AGDE's model.

Cache variables:

| Variable | Default | Purpose |
|----------|---------|---------|
| `TELEPORT_AGDE_ABI` | `arm64-v8a` | Android ABI (also derivable from `-A Android-<abi>`). |
| `TELEPORT_AGDE_API` | `29` | Android API level (`CMAKE_SYSTEM_VERSION`). |
| `TELEPORT_AGDE_MIN_SDK` | `TELEPORT_AGDE_API` | Value emitted as `AndroidMinSdkVersion`. |
| `TELEPORT_AGDE_NDK_VERSION` | from `release.properties` | Value emitted as `AndroidNdkVersion`. |
| `TELEPORT_ANDROID_NDK` | auto-detected | NDK root, used for `find_*` isolation. |

For application targets that should be packaged into an APK, mirror the hand-written
`TeleportVRQuestClientAGDE.vcxproj` with:

```cmake
agde_enable_packaging(MyApp GRADLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/AndroidPackaging")
```

## Verifying generated projects

Compare a generated vcxproj against, say,
`build_android_vs/TeleportCore/TeleportCore_Android.vcxproj`. It should contain:

- `<Platform>Android-arm64-v8a</Platform>` in the `ProjectConfigurations` item group;
- `<PlatformToolset>Clang</PlatformToolset>`;
- `<Keyword>Win32Proj</Keyword>` and **no** `<ApplicationType>` element;
- `AndroidMinSdkVersion`, `AndroidNdkVersion` and `CppLanguageStandard` in the
  `Globals` property group;
- only the three standard `$(VCTargetsPath)\Microsoft.Cpp.*` imports.

Then build with `cmake --build <dir> --config Release` (which drives MSBuild → AGDE →
NDK clang) and confirm `libSmokeLib.a` and the `SmokeApp` binary appear under the
build tree.

## First production use: ozz-animation for the Android build

The toolchain's first real use is generating the `ozz_animation` static-library
project for the hand-rolled AGDE solution (see `plans/ANDROID_OZZ_BUILD_PLAN.md` in
the workspace). The top-level `TELEPORT_ANDROID_MINIMAL_SETUP` option fetches
ozz-animation and generates `thirdparty/ozz_animation/ozz_animation.vcxproj` inside
`build_android_vs/`, where `TeleportVRQuestClient.sln` references it by a pinned GUID
(`ozz_animation_GUID_CMAKE`, set in the top-level CMakeLists):

```bat
cmake -B build_android_vs -S Teleport -G "Visual Studio 17 2022" ^
      -A Android-arm64-v8a -T Clang ^
      -DCMAKE_TOOLCHAIN_FILE=CMake/AGDE.toolchain.cmake ^
      -DTELEPORT_ANDROID_MINIMAL_SETUP=ON
```

CI runs this in `build_android_client.yml` before the MSBuild step. Generated and
hand-rolled projects coexist in `build_android_vs/`; the intent is to migrate more
projects to CMake generation as the approach is proven.

## Known caveats

- **In PowerShell, quote the toolchain argument.** PowerShell splits an unquoted
  token that starts with `-` at the first `.`, so
  `-DCMAKE_TOOLCHAIN_FILE=CMake/AGDE.toolchain.cmake` reaches cmake as two arguments
  (`…=CMake/AGDE` plus a stray `.toolchain.cmake` source path). Write
  `"-DCMAKE_TOOLCHAIN_FILE=CMake/AGDE.toolchain.cmake"` instead. `cmd.exe` does not
  have this problem.
- **Compiler detection runs through MSBuild.** At configure time CMake builds its
  compiler-identification project with the AGDE platform/toolset, so configuration
  itself requires AGDE and the NDK to be installed and working. The compiler *path*
  cannot be extracted from AGDE build output (CMake's probe looks for cl.exe on PATH),
  so the toolchain pre-sets `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` to the NDK's clang —
  which is why the NDK must be locatable (via `ANDROID_NDK_HOME`/`ANDROID_NDK_ROOT` or
  `-DTELEPORT_ANDROID_NDK`) at configure time.
- **MSVC-oriented properties appear in generated projects** (`CharacterSet`,
  `UseDebugLibraries`, and similar): CMake's generator writes them because the
  toolset is not one it recognises. AGDE's Clang toolset ignores properties it does
  not consume; they are noise, not errors.
- **Compile batching must stay off.** CMake normally writes a folder-valued
  `<ObjectFileName>$(IntDir)</ObjectFileName>`, which makes MSBuild batch all sources
  into one Clang invocation — AGDE's MultiToolTask rejects that ("MultiToolTask build
  is not compatible with batched Clang invocation"). The toolchain therefore sets
  `CMAKE_VS_NO_COMPILE_BATCHING ON`, giving per-file object names. If a future AGDE
  version still objects, the fallback is adding `"NativeBuildBackend=OriginalMSBuild"`
  (or `"UseMultiToolTask=false"`) to `CMAKE_VS_GLOBALS`.
- **Unmapped compile/link flags flow through `AdditionalOptions`.** CMake maps flags
  to MSBuild XML using its default (MSVC-oriented) flag table; GNU-style flags that do
  not match are appended verbatim to `<AdditionalOptions>`, which AGDE's clang tasks
  honour. If finer control is ever needed, a custom flag table can be supplied with
  `-T Clang,customFlagTableDir=<dir>`.
- **`CppLanguageStandard=cpp2a` and CMake's `-std=` flag can coexist.** CMake may also
  emit `-std=c++20` via `AdditionalOptions` for targets that set `CMAKE_CXX_STANDARD`;
  clang accepts the repetition (the last flag wins).
- This toolchain generates projects; it does **not** convert the existing
  `build_android_vs/` tree. The hand-written projects remain the build of record until
  the main CMakeLists gains Android target definitions.
