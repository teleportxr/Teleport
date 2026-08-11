# TeleportHeadlessClient

Terminal- and machine-controlled Teleport client for Linux, Windows and macOS, with no GUI, GPU,
or OpenXR dependency.

On macOS this used to be the only client that would build; `pc_client`/`ClientRender` now build
there too via MoltenVK (Vulkan on Metal) - see `pc_client/CLAUDE.md`. `TELEPORT_GUI_CLIENT` still
defaults ON everywhere `TELEPORT_CLIENT` is, macOS included, so both clients build together
unless it's explicitly turned off (`-DTELEPORT_GUI_CLIENT=OFF -DTELEPORT_CLIENT_USE_VULKAN=OFF`)
for a headless-only configure.

## Two processes, one library

The client is split into a service and a front end, and this is the central fact about the
directory. A streaming connection outlives the thing that asked for it: an agent issues a command,
reports to the user, waits for feedback and acts again, three requests that may be minutes apart.
A session that died with its terminal could not support that at all.

| Target | Source | Role |
|---|---|---|
| `teleportd` | `Main.cpp` | The service. Owns live connections, ticks them at 20 Hz, listens on loopback TCP for one-line commands. |
| `teleport_cli` | `cli_main.cpp` | POSIX front end. Sends command lines to `teleportd`, prints responses, exits. Links only `SocketUtil.cpp` — no libavstream, WebRTC, OpenSSL or JSON. |
| `teleport_headless_core` | everything else | Static library shared by both, and by the tests. |

`quit` detaches a control client; `shutdown` stops the service and every stream with it.

## Components

### Control plane

- **ControlProtocol.h** — wire constants and dot-stuffed line framing. Header-only, so
  `teleport_cli` can use it without linking the core.
- **ControlServer** — loopback listener, accept thread, one detached thread per attached control
  client. Holds a `ControlSessionState` per client and renders each `CommandResult` in whichever
  format that client asked for.
- **CommandProcessor** — the dispatcher. String in, `CommandResult` out; no I/O, no framing, which
  is what makes it unit-testable (`test/test_control_processor.cpp`).
- **CommandResult** — `{ok, text, error, data}`. Every verb fills *both* `text` and `data`; a verb
  that fills only one is a bug. This is what stops the prose and JSON renderings drifting apart.
- **ConnectionReport.h/.cpp** — the plain-data structs (`ConnectionStatus`, `GeometryReport`) that
  status and geometry prose is rendered *from*, plus those renderers. Nothing formats this prose
  anywhere else.
- **ReplCommandParser** — dependency-free whitespace tokeniser: verb plus args, no quoting.
- **SocketUtil** — BSD sockets / winsock2 shim.

The protocol is specified in `docs/protocol/local_control.rst`. Read that before changing any
response: the `data` schemas are a published contract that `teleport-mcp/` codes against, and the
text bodies are what existing scripts scrape.

### Streaming plane

- **ConnectionManager** — owns every `HeadlessConnection`, keyed by a small integer id from 1,
  never reused. Its mutex is what serialises command threads against the tick thread;
  `SessionClient` is not internally thread-safe, so holding it for a whole tick or command is
  required, not incidental.
- **HeadlessConnection** — one streaming session (`TabContext` + `SessionClient`). Formerly the
  monolithic `HeadlessClient`.
- **HeadlessSessionCommandInterface** — `teleport::client::SessionCommandInterface`, wiring non-GPU
  pipeline components according to the mode.
- **HeadlessGeometryCacheBackend** — records streamed geometry and supplies the acknowledgement
  lists `SessionClient` drains each frame. Mutex-guarded: written from the pipeline thread, read
  from the tick thread.
- **HeadlessGeometryTarget** — `avs::GeometryTargetBackendInterface`. Records structure only,
  creates no GPU resources.
- **HeadlessGeometryDecoder** — parses Node/RemoveNodes/Skeleton in full, records pointer URLs
  without fetching them, acknowledges every other payload type by uid without parsing the body.
- **HeadlessInputState** — thread-safe pose and input state shared between command and tick threads.

### Modes

1. **Minimal** (default) — network diagnostic: connectivity, bandwidth and stream health, no
   geometry decoding.
2. **Simulated** — full-protocol test client exercising geometry streaming and the
   request/receive/ack flow.

## Build

```bash
cmake -B build_pc_client -S . -DTELEPORT_HEADLESS_CLIENT=ON
cmake --build build_pc_client
```

`TELEPORT_HEADLESS_CLIENT` defaults to the value of `TELEPORT_CLIENT`, so it only needs stating
explicitly when the GUI client has been switched off.

On macOS (Apple Silicon; needs the Xcode command line tools), headless-only, skipping the Vulkan
SDK and the bison/flex that `pc_client` needs:

```bash
brew install ninja pkg-config openssl@3
cmake -S . -B build_macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTELEPORT_GUI_CLIENT=OFF -DTELEPORT_CLIENT_USE_VULKAN=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build build_macos
```

To build both clients together, see `pc_client/CLAUDE.md` or the root `README.md`.

Build the default target rather than just `teleportd`: the Platform submodule has `install()` rules
for libraries that `cpack` expects to exist, and a target-specific build leaves them unbuilt.

Packaging (macOS only): the `install()` rules bundle Homebrew's `libssl.3.dylib`/`libcrypto.3.dylib`
into `<prefix>/lib` and rewrite the binaries' load commands to `@loader_path`-relative paths via
`install_name_tool`. Without this, the installed binary references the absolute Homebrew path baked
in at link time and fails to launch on any Mac without that exact keg ("Library not loaded").
Verify with `otool -L build_macos/bin/teleportd | grep -i ssl` after `cpack`.

Verify the build has NOT pulled in graphics dependencies:

```bash
# Windows
dumpbin /IMPORTS build_pc_client/bin/Release/teleportd.exe | findstr /i "openxr"
# Linux
ldd build_pc_client/bin/teleportd | grep -iE 'vulkan|openxr|glfw|pulse'
# macOS
otool -L build_macos/bin/teleportd | grep -iE 'vulkan|moltenvk|openxr|glfw|pulse'
```

## Usage

```bash
teleportd [-p <port>]                        # service; default port 10510
teleport_cli                                 # interactive (line editing, history, completion)
teleport_cli -e "connect 127.0.0.1:8080; status"
teleport_cli connect 127.0.0.1:8080          # operands join into one command, ssh-style
teleport_cli -j -e connections               # JSON responses
printf 'ping\n' | teleport_cli               # piped stdin is batch input
```

Exit codes: 0 all OK, 1 some command answered ERROR, 2 usage error or service unreachable.

The selected connection is **per control socket**, so a fresh `teleport_cli` starts with nothing
selected. Batch scripts must either put `connect`/`use` in the same `-e` string or prefix each
command with `use <id>`.

`connect` returns as soon as the attempt is initiated; poll `status` until `state` is `CONNECTED`.

## Tests

| Test | Covers |
|---|---|
| `test/test_repl_command_parser.cpp` | tokeniser |
| `test/test_control_protocol.cpp` | framing and dot-stuffing |
| `test/test_control_processor.cpp` | dispatcher behaviour and the JSON schemas |
| `test/control_integration.sh` | end-to-end: launches `teleportd`, drives it with `teleport_cli` |

Run them with `ctest` from `build_pc_client/test`.

## Known limitations

- Video decode is skipped entirely (stats only) — network health, no validation of frame content.
- Mesh, Material, Texture and Animation payload *bodies* are acknowledged but not parsed; decoding
  them would pull in draco, ktx and image codecs. Node/RemoveNodes/Skeleton are parsed in full.
- Assets behind pointer URLs are deliberately never downloaded; the uid and URL are recorded.
- `MaterialPointer` is parsed on the assumption that it shares the `uint16` length + URL format of
  `MeshPointer`/`TexturePointer`. `clientrender::GeometryDecoder` does not implement it, so this
  path is untested against a live server.
- No node visibility routing yet.
- Wire formats in `HeadlessGeometryDecoder` mirror `clientrender::GeometryDecoder`; the two must be
  kept in step by hand until the parsers are extracted into a shared GPU-free unit.
- All connections in one service share `Config::GetInstance()`: one identity, one avatar URL, one
  storage folder. Simulating distinct users means one `teleportd` per identity.

## Code style

- C++17 minimum, tabs, Allman braces, 160-column limit (matches the rest of the repo).
- PascalCase classes and methods, camelCase members.
- `TELEPORT_LOG`/`TELEPORT_WARN` for diagnostics — never `std::cout` from the service, which may
  have no console attached.
- British English spelling in comments.
