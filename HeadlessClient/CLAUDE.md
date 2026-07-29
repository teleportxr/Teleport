# TeleportHeadlessClient

Terminal-controlled Teleport client for Linux and Windows, with no GUI, GPU, or OpenXR dependency.

## Purpose

Two primary use cases:

1. **Minimal Bot Mode** (default, M1 complete) — pure network diagnostic tool for testing server connectivity, bandwidth, and stream health without graphics rendering.
2. **Simulated User Mode** (M3 in progress) — full-protocol test client exercising geometry streaming, request/receive/ack flow, and node visibility routing.

## Architecture

### Core Components

- **HeadlessClient** — orchestrates SessionClient lifecycle, connection management, and the main tick loop (~20 Hz)
- **HeadlessSessionCommandInterface** — implements `teleport::client::SessionCommandInterface`, wiring non-GPU pipeline components based on mode
- **HeadlessGeometryCacheBackend** — `teleport::client::GeometryCacheBackendInterface` stub for minimal mode (returns empty)
- **HeadlessGeometryTarget** — geometry reception/tracking for simulated mode (stub implementation)
- **HeadlessInputState** — thread-safe pose and input state shared between REPL and tick threads
- **Repl** — blocking REPL loop reading commands from stdin
- **ReplCommandParser** — dependency-free command tokenizer (used for Catch2 unit tests in future)

### Thread Model

- **Main thread** — blocking `std::getline()` REPL, processes user commands
- **Tick thread** — fixed 20 Hz loop calling `HeadlessClient::TickOnce()`, which calls `SessionClient::HandleConnections()` and `Frame()`

## Build

```bash
cmake -B build_pc_client -S . -DTELEPORT_HEADLESS_CLIENT=ON
cmake --build build_pc_client --target TeleportHeadlessClient
```

On Windows, verify the build does NOT pull in unwanted dependencies (openxr_loader check):
```bash
dumpbin /IMPORTS build_pc_client/bin/Release/TeleportHeadlessClient.exe | findstr /i "openxr"
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
- HeadlessGeometryTarget is a stub; full avs::GeometryTargetBackendInterface integration deferred
- No actual geometry decoding or node visibility routing yet
- MeshParser extraction from ResourceCreator deferred (full M3 feature set pending)

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
