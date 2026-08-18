## Installation

### Windows — `TeleportClientInstaller-*.exe`

Run the installer and follow the prompts. It installs the Teleport PC client
and a Start Menu shortcut. Administrator rights are required.

### macOS

Two packages are provided:

- **`teleportxr-*-arm64.pkg`** — the headless `teleportd` client, installed as
  a system package. Requires macOS on Apple Silicon.
- **`TeleportMacClientInstaller-*-arm64.dmg`** — the GUI client. Open the
  image and drag `TeleportPCClient.app` to Applications. Requires MoltenVK
  (bundled) and macOS on Apple Silicon.

### Linux — `teleportxr-*-x64.deb`

```bash
sudo apt install ./teleportxr-*-x64.deb
```

This installs the client to `/opt/teleportxr` with a `teleportxr` command on
the PATH and a desktop entry.

### Android / Meta Quest — `teleportxr-*.apk`

Sideload with adb:

```bash
adb install teleportxr-*.apk
```

The headset must have developer mode enabled.
