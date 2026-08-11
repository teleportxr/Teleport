# pc_client — TeleportPCClient

The GUI/OpenXR desktop client executable. Windows and Linux build it with GLFW + Vulkan or
D3D11/D3D12; macOS (Apple Silicon) builds it via MoltenVK (Vulkan on Metal) — see the
`TELEPORT_GUI_CLIENT` comment in the root `CMakeLists.txt`. There is still no OpenXR *runtime*
for macOS, so `ENABLE_VR` defaults off there (`TeleportClient/Config.h`) and `UseOpenXR` never
calls `xrCreateInstance`.

## Files

- `Main.cpp` — entry point: window/device creation, the render loop, and (macOS only)
  `SetupMoltenVkIcd()`, which points the Vulkan loader at the bundled MoltenVK ICD before the
  first Vulkan call.
- `UseOpenXR.h/.cpp` — OpenXR session/swapchain wrapper shared by the render loop.
- `ProcessHandler.h/.cpp`, `MemoryUtil.h/.cpp` — single-instance enforcement and process/memory
  utilities.
- `UnixDebugOutput.h` — POSIX (Linux/macOS) debug-output sink, paired with
  `VisualStudioDebugOutput` on Windows.
- `Icon.rc`, `Resource.h`, `targetver.h` — Windows resource/version-info plumbing.
- `TeleportXR.desktop` — Linux freedesktop menu entry, installed to `/usr/share/applications`.
- `Info.plist.in` — macOS `.app` bundle Info.plist template (`MACOSX_BUNDLE_INFO_PLIST`);
  CMake substitutes `${TELEPORT_VERSION}`, `${CMAKE_OSX_DEPLOYMENT_TARGET}` and the
  `MACOSX_BUNDLE_*` target properties set in `CMakeLists.txt` into it.
- `CPackOptions.cmake` — per-platform `CPACK_PACKAGE_FILE_NAME`; the rest of the CPack
  configuration lives in `../CMake/TeleportPackaging.cmake`.
- `textures/` — icons/textures loaded relative to the client data directory at runtime.

## macOS packaging

`CMakeLists.txt`'s `TELEPORT_MACOS` install() branch turns the build into a self-contained,
relocatable `TeleportPCClient.app`:

- Built as a real bundle (`add_static_executable(... MACOSX_BUNDLE ...)`, a keyword added to
  `firstparty/Platform/CMake/Include.cmake` for this), so the icon and Info.plist are already
  present in the *build tree* output (`build_macos/bin/TeleportPCClient.app`), not just after
  `cpack` — the icon works whether you run it straight from the build tree or after installing.
- `install(CODE ...)` runs CMake's `BundleUtilities` `fixup_bundle()` on the staged bundle,
  copying every non-system dylib (Homebrew's Vulkan loader and OpenSSL, plus this project's own
  FetchContent-built libktx/libdraco/libozz_*/libopus/libglfw/libopenxr_loader/libcurl) into
  `Contents/Frameworks` and rewriting `TeleportPCClient`'s load commands to match.
- MoltenVK is handled separately, by hand: it's never linked (the Vulkan loader `dlopen`s it via
  an ICD manifest, not a normal link dependency), so `fixup_bundle` never finds it. It's copied
  together with `../Installers/MoltenVK_icd.json` into
  `Contents/Resources/vulkan/icd.d/`, and `Main.cpp`'s `SetupMoltenVkIcd()` points
  `VK_ICD_FILENAMES` at it (relative to `CFBundleGetMainBundle()`) before the first Vulkan call —
  necessary because a GUI app launched from Finder inherits no shell environment.
- The runtime data directory goes to `Contents/share/teleportxr` (not `Contents/Resources`):
  `teleport::client::ResolveClientDataDirectory` (`TeleportClient/ClientBootstrap.cpp`) walks up
  from the executable looking for a `share/teleportxr` or `client` subdirectory, and
  `Contents/MacOS/../share/teleportxr` satisfies that unmodified, the same way the Linux
  `bin/../share/teleportxr` layout does.
- Every embedded dylib is re-signed after `fixup_bundle` (which invalidates any earlier
  signature), then the bundle as a whole, when `TELEPORT_MACOS_APP_IDENTITY` is set — same
  pattern as `HeadlessClient/CMakeLists.txt`'s OpenSSL bundling.
- Packaged as a drag-to-Applications `.dmg` (CPack DragNDrop generator, `pcclient` component),
  separate from `teleport_terminal`'s `.pkg` — see `../CMake/TeleportPackaging.cmake` and the
  root `README.md`'s "Building the macOS clients" section for the exact `cpack` invocations.

## Build

See the root `README.md`'s "Building the macOS clients" / "Building the PC Client" sections, or
`HeadlessClient/CLAUDE.md` for the headless-only (no Vulkan SDK) macOS configure.
