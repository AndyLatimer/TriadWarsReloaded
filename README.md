# Triad Wars Reloaded

Revival and local server emulation project for **Triad Wars** (PC x64 build 1.6.0.0, Changelist 541920), originally developed by United Front Games and Square Enix.

This repository provides the local server infrastructure, patches, input translation wrappers, modern ultrawide display configuration, and unified launcher required to run the game offline in single-player mode.

---

## Features

- **Local Emulation Server (`server/`):** Full HTTPS/WSS local backend running on `127.0.0.1:443`, implementing:
  - Session authentication & keep-alive handshakes
  - Player profile, currency (clean & dirty money), and turf management
  - 87 playable errands (*Errands*) across 4 dynamic slots
  - 7 Triad operations (*Operations*: Catch, Defeat, Photo, Deliver, Collect, Destroy, Hijack)
  - Open world dynamic encounters (*World Encounters*: Police Pursuits, Ambushes, Armoured Cars)
  - 7 Open world crime scenes (*Crime Scenes*) with 3D coordinate markers
  - Full vehicle dealership store (`UFG::StoreVehicle`) with 247 drivable cars, bikes, and vans
- **DualSense PS5 Controller Native Wrapper (`tools/xinput_dualsense/`):**
  - C++ x64 `XINPUT9_1_0.dll` drop-in hook reading DirectInput/HID reports directly with deadzone calibration.
- **Engine Stability Patch (`server/LocalServer/LocalServer/tools/patch_exe.py`):**
  - Resolves spot-shadow pass null-pointer race condition crash (`TriadWars.exe+0x548a`).
- **Official Spanish Localization:**
  - Automated configuration for official Spanish language pack (`SP_Global.bin`, 2,592 translated strings).
- **Ultrawide Display Preset:**
  - 2560x1080 @ 75Hz profile with high texture and ultra world density settings.
- **Integrated C# Launcher (`launcher/`):**
  - Graphical Windows Forms management application to start/stop the server, launch the game, toggle patches, switch languages, and monitor live status.

---

## Repository Structure

```
TriadWarsReloaded/
├── docs/                     # Technical documentation & reverse engineering reports
├── launcher/                 # Launcher C# source code and build files
│   └── src/Launcher.cs
├── server/                   # Local server emulator backend
│   └── LocalServer/LocalServer/
│       ├── data/             # Item catalogs, missions database & tuning tables
│       ├── tools/            # Patchers, unpackers, and verification smoke tests
│       ├── handlers.py       # REST API endpoints implementation
│       ├── server.py         # HTTP & WebSocket core server
│       ├── state.py          # State representation & data bags
│       └── useroptions.py    # XOR-obfuscated UserOptions.dat parser
├── tools/                    # Tool source code
│   └── xinput_dualsense/     # Native DualSense XInput hook source (C++/DEF)
├── Play-TriadWars.bat        # Quick launcher script
└── README.md
```

> **Note on Commercial Game Assets:** In accordance with copyright policies, original proprietary game binaries (`TriadWars.exe`), sound packs (`.pck`), and asset archives (`.big` / `.bix`) are not hosted in this repository. Place your original beta client data into a `beta/` folder to play.

---

## Quick Start

1. Ensure **Python 3.9+** and **.NET Framework 4.5+** are installed.
2. Place your original Triad Wars 1.6.0.0 files in `beta/`.
3. Run `Play-TriadWars.bat` or compile and launch `TriadWarsLauncher.exe`.
4. Click **JUGAR (Servidor + Juego)**.

---

## License & Credits

- Original game assets and engine by United Front Games & Square Enix.
- Reverse engineering and revival toolchain by SDmodding, sneakyevil, TheSubsonicTouch, and the Triad Wars Revival contributors.
