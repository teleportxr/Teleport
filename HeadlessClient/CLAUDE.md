# TeleportHeadlessClient

Terminal-controlled Teleport client for Linux, Windows and macOS, with no GUI, GPU, or OpenXR dependency.

On macOS this is the *only* client that is built: `TELEPORT_GUI_CLIENT` is forced OFF there,
since `pc_client`/`ClientRender` need Vulkan, GLFW and an OpenXR runtime.

## Purpose

Two primary use cases:

1. **Minimal Bot Mode** (default, M1 complete) — pure network diagnostic tool for testing server connectivity, bandwidth, and stream health without graphics rendering.
2. **Simulated User Mode** (M3 in progress) — full-protocol test client exercising geometry streaming, request/receive/ack flow, and node visibility routing.

## Architecture

### Core Components

- **HeadlessClient** — orchestrates SessionClient lifecycle, connection management, and the main tick loop (~20 Hz)
- **HeadlessSessionCommandInterface** — implements `teleport::client::SessionCommandInterface`, wiring non-GPU pipeline components based on mode
- **HeadlessGeometryCacheBackend** — `teleport::client::GeometryCacheBackendInterface`. Records streamed geometry and supplies the acknowledgement lists `SessionClient` drains each frame (`ReceivedResourcesMessage`, `NodeStatusMessage`). Mutex-guarded: written from the pipeline thread, read from the tick thread
- **HeadlessGeometryTarget** — `avs::GeometryTargetBackendInterface`. Records structure only; creates no GPU resources
- **HeadlessGeometryDecoder** — `avs::GeometryDecoderBackendInterface`. Parses Node/RemoveNodes/Skeleton in full, reads and records pointer URLs without fetching, acknowledges all other payload types by uid without parsing their bodies
- **HeadlessInputState** — thread-safe pose and input state shared between REPL and tick threads
- **Repl** — blocking REPL loop reading commands from stdin
- **ReplCommandParser** — dependency-free command tokenizer (used for Catch2 unit tests in future)

### Thread Model

- **Main thread** — blocking `std::getline()` REPL, processes user commands
- **Tick thread** — fixed 20 Hz loop calling `HeadlessClient::TickOnce()`, which calls `SessionClient::HandleConnections()` and `Frame()`

## Build

```bash
cmake -B build_pc_client -S . -DTELEPORT_HEADLESS_CLIENT=ON
cmake --build build_pc_client --target teleport_terminal
```

`TELEPORT_HEADLESS_CLIENT` already defaults to the value of `TELEPORT_CLIENT`, so it only
needs stating explicitly when the GUI client has been switched off.

On macOS (Apple Silicon; needs the Xcode command line tools):

```bash
brew install ninja pkg-config openssl@3
cmake -S . -B build_macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTELEPORT_GUI_CLIENT=OFF -DTELEPORT_CLIENT_USE_VULKAN=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build build_macos
```

Build the default target rather than just `teleport_terminal`: the Platform submodule has
`install()` rules for libraries that `cpack` expects to exist, and a target-specific build
leaves them unbuilt.

Packaging (macOS only): the `install()` rules in `CMakeLists.txt` bundle Homebrew's
`libssl.3.dylib`/`libcrypto.3.dylib` into `<prefix>/lib` and rewrite `teleport_terminal`'s
load commands to `@loader_path`-relative paths via `install_name_tool`. Without this, the
installed binary references the absolute Homebrew path baked in at link time
(e.g. `/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib`) and fails to launch on any Mac
without that exact keg installed ("Library not loaded"). Verify with
`otool -L build_macos/bin/teleport_terminal | grep -i ssl` after `cpack`.

Verify the build does NOT pull in unwanted dependencies (openxr_loader check):

```bash
# Windows
dumpbin /IMPORTS build_pc_client/bin/Release/teleport_terminal.exe | findstr /i "openxr"
# Linux
ldd build_pc_client/bin/teleport_terminal | grep -iE 'vulkan|openxr|glfw|pulse'
# macOS
otool -L build_macos/bin/teleport_terminal | grep -iE 'vulkan|moltenvk|openxr|glfw|pulse'
```

## Implementation Status

### ✅ M1: Minimal Mode Complete

- ClientBootstrap extraction (shared across pc_client + HeadlessClient)
- Minimal mode REPL with connect/disconnect/status/move/turn commands
- Non-GPU pipeline wiring (video receive but no decode, stats accumulation)
- Input state management (pose synchronisation between REPL/tick threads)
- Mode selection via CLI or REPL command

### ✅ M2: Input Event Sending Complete

- Input event REPL commands: binary/analogue/motion
- HeadlessInputState methods to add events to the input stream
- Enhanced status output with connection state, server info, latency, input definitions count
- Mode switching persists across status checks

### ✅ M3: Simulated Mode Foundation Complete

- HeadlessGeometryTarget class created (stub for geometry logging)
- Mode-dependent HeadlessSessionCommandInterface initialization
- GeometryTarget instantiation in simulated mode
- Mode switching updates the session command interface

### ⚠️ Known Limitations

**M1-M2:**
- Ctrl-C does not immediately exit; use `quit` + Enter instead (non-blocking stdin is M4)
- Video decode completely skipped (stats-only) — network health only, no validation of frame content
- No geometry streaming in M1 (M3 foundation laid, full integration deferred)

**M3:**
- Mesh, Material, Texture and Animation payload *bodies* are acknowledged but not parsed — decoding
  them would pull in draco, ktx and image codecs. Node/RemoveNodes/Skeleton are parsed in full
- Assets behind pointer URLs are deliberately never downloaded; the uid and URL are recorded
- `MaterialPointer` is parsed on the assumption it shares the `uint16` length + URL format of
  `MeshPointer`/`TexturePointer`. `clientrender::GeometryDecoder` does not implement it, so this
  path is untested against a live server
- No node visibility routing yet
- Wire formats in HeadlessGeometryDecoder mirror `clientrender::GeometryDecoder`; the two must be
  kept in step by hand until the parsers are extracted into a shared GPU-free unit

## M2-M4 Roadmap

- **M2** ✅ — input event sending (binary/analogue/motion REPL commands), extended status with latency/node counts
- **M3** (partial) — `simulated` mode foundation; full geometry decoding/node visibility/MeshParser extraction deferred to prevent scope creep
- **M4** — non-blocking stdin (select/console APIs), ReplCommandParser Catch2 tests, Heroku/CI-ready piping support, full M3 geometry integration

## Key Files

### New Files
- `Teleport/HeadlessClient/*` — all headless client implementation
- `Teleport/TeleportClient/ClientBootstrap.h/.cpp` — shared bootstrap logic

### Modified Files
- `Teleport/pc_client/Main.cpp` — refactored to use ClientBootstrap
- `Teleport/CMakeLists.txt` — added TELEPORT_HEADLESS_CLIENT option and subdirectory
- `Teleport/TeleportClient/CMakeLists.txt` — added ClientBootstrap.cpp

## REPL Commands (M1-M3)

```
# Connection
connect <ip[:port]>      - Connect to server
disconnect              - Disconnect from server

# Status & Info
status                  - Show connection status (state, server, latency, inputs)
help                    - Show help

# Movement/Orientation
move <x> <y> <z>        - Set avatar position
turn <qx> <qy> <qz> <qw> - Set avatar orientation (quaternion)

# Input Events (M2)
input list              - List available inputs
input binary <id> <0|1> - Send binary input event
input analogue <id> <value> - Send analogue input event
input motion <id> <x> <y> - Send motion input event

# Mode Control
mode <minimal|simulated> - Switch client mode (M3)

# Geometry
geometry                - Summary: nodes, resources, pointers, pending acks
geometry nodes          - Tracked nodes with data/parent/skeleton uids
geometry resources      - Pointer resource URLs, and uids referenced but never sent

# Exit
quit / exit             - Exit the client
```

## Code Style

- C++17 minimum
- 4-space indentation (matching pc_client)
- Naming: PascalCase classes (HeadlessClient), camelCase members (inputState)
- TELEPORT_LOG/TELEPORT_WARN for diagnostics
- British English spelling in comments

## Testing Checklist

- [x] Smoke test: `help` → `quit` works
- [x] Move/turn commands update state
- [x] Input commands parse and report connection status
- [x] Mode switching (minimal ↔ simulated)
- [x] Status shows connection info and input count
- [ ] Against real server: connection state progression, bandwidth stats
- [ ] No GPU required: run on headless system without NVIDIA driver
- [ ] Geometry streaming (M3 full integration)
- [ ] Node visibility routing (M3 full integration)
