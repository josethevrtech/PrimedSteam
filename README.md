# PrimedSteam / PrimedGun

![PrimedGun gameplay](assets/readme/primedgun-hero.jfif)

PrimedSteam is a SteamOS-focused build of PrimedGun, a Dolphin ReduX-based build focused on improving Metroid Prime's VR experience.

The current primary target is SteamOS on x86_64 hardware.

## Status

Verified on SteamOS after switching back to the main update channel:

* Builds with GCC 15
* Builds with CMake and Ninja
* Launches successfully from the build output
* Runtime library check passes with `ldd`
* OpenXR loader builds correctly with exceptions enabled
* Qt private QPA headers are resolved without hardcoding a Qt version

## Quick start for SteamOS

SteamOS uses a read-only root filesystem by default. Temporarily disable it before installing build dependencies:

```bash
sudo steamos-readonly disable
```

Install the required packages:

```bash
sudo pacman -S git base-devel cmake ninja pkgconf clang libglvnd \
  qt6-base qt6-svg qt6-tools qt6-wayland \
  mesa vulkan-headers vulkan-radeon \
  libx11 libxi libxrandr libxext libxrender libxfixes \
  libevdev systemd-libs alsa-lib libpulse bluez-libs \
  openal ffmpeg openxr sdl3 libxcb xcb-proto xorgproto \
  glibc linux-api-headers
```

Initialize submodules:

```bash
git submodule update --init --recursive
```

Build:

```bash
./scripts/build-linux.sh
```

The binary will be generated at:

```text
build-linux/Binaries/PrimedGun
```

## Build from a custom folder

You can pass a build directory to the script:

```bash
./scripts/build-linux.sh build-steamos
```

or:

```bash
./scripts/build-linux.sh build-portable-test
```

## Verify the binary

```bash
ldd build-linux/Binaries/PrimedGun | grep "not found" || echo "runtime libs OK"
```

Launch:

```bash
./build-linux/Binaries/PrimedGun
```

## SteamOS update-channel notes

SteamOS update-channel changes can remove or invalidate build tools, package database state, and development headers.

If `cmake` disappears:

```bash
sudo pacman -S cmake ninja base-devel
```

If `EGL/egl.h` is missing:

```bash
sudo pacman -S libglvnd
```

If Qt private QPA headers are missing, reinstall Qt packages:

```bash
sudo pacman -S qt6-base qt6-svg qt6-tools qt6-wayland
```

If the package keyring breaks after an update-channel change:

```bash
sudo install -d -m 755 /etc/pacman.d/gnupg
sudo chown -R root:root /etc/pacman.d/gnupg
sudo pacman-key --init
sudo pacman-key --populate
sudo pacman -Syy
```

## Persistent storage recommendation

Keep the repo and build outputs on persistent storage, not inside the SteamOS root filesystem.

Example:

```text
/var/mnt/Storage/Dev/PrimedSteam
```

This prevents source and build work from being lost or disrupted by SteamOS system updates.

## More build details

See:

```text
BUILDING-LINUX.md
```

## Legal notes

This repository should contain source code, build scripts, and documentation only.

Do not commit or distribute:

* Game ISOs, ROMs, WADs, or disc images
* Nintendo BIOS, keys, firmware, or system files
* Copyrighted texture packs or assets you do not own
* Steam or Valve runtime files copied from a local installation
* Local build folders such as `build-steamos/`, `build-linux/`, or `build-portable-test/`

Preserve upstream license files, SPDX notices, and third-party license documentation.
