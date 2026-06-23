# PrimedSteam AKA PrimedGun for SteamOS

![PrimedGun gameplay](assets/readme/primedgun-hero.jfif)

PrimedSteam is a SteamOS-focused build of PrimedGun, a Dolphin ReduX-based build focused on improving Metroid Prime's VR experience by Nobbie248.

The current target is SteamOS on x86_64 hardware.

## Verified system

Tested on:

```text
SteamOS 3.9 Build: 20260617.1001
Kernel: 6.18.33-valve2-1-neptune-618-g29c9aecca098
CPU: AMD Ryzen 9 PRO 8945HS w/ Radeon 780M Graphics
RAM: 32 GiB
GPU: AMD Radeon RX 7900 GRE
```

## SteamOS quick start

Run these commands from the PrimedSteam repo folder.

### 1. Allow package installs

SteamOS uses a read-only root filesystem by default. Temporarily disable it:

```bash
sudo steamos-readonly disable
```

### 2. Prepare the pacman keyring

This is included for new SteamOS users and for systems that recently changed update channels.

```bash
sudo install -d -m 755 /etc/pacman.d/gnupg
sudo chown -R root:root /etc/pacman.d/gnupg
sudo pacman-key --init

for keyring in /usr/share/pacman/keyrings/*.gpg; do
  sudo pacman-key --populate "$(basename "$keyring" .gpg)"
done

sudo pacman -Syy
```

### 3. Install build dependencies

```bash
sudo pacman -S git base-devel cmake ninja pkgconf clang libglvnd \
  qt6-base qt6-svg qt6-tools qt6-wayland \
  mesa vulkan-headers vulkan-radeon \
  libx11 libxi libxrandr libxext libxrender libxfixes \
  libevdev systemd-libs alsa-lib libpulse bluez-libs \
  openal ffmpeg openxr sdl3 libxcb xcb-proto xorgproto \
  glibc linux-api-headers
```

### 4. Prepare the source tree

```bash
git submodule update --init --recursive
```

### 5. Build

```bash
./scripts/build-linux.sh
```

The binary will be created here:

```text
build-linux/Binaries/PrimedGun
```

## Custom build folder

You can choose a different build folder:

```bash
./scripts/build-linux.sh build-steamos
```

or:

```bash
./scripts/build-linux.sh build-portable-test
```

## Verify and launch

Check for missing runtime libraries:

```bash
ldd build-linux/Binaries/PrimedGun | grep "not found" || echo "runtime libs OK"
```

Launch:

```bash
./build-linux/Binaries/PrimedGun
```

## SteamOS troubleshooting

SteamOS updates or update-channel changes can remove build tools, reset headers, or break package metadata.

If `cmake` is missing:

```bash
sudo pacman -S cmake ninja base-devel
```

If `EGL/egl.h` is missing:

```bash
sudo pacman -S libglvnd
```

If Qt private QPA headers are missing:

```bash
sudo pacman -S qt6-base qt6-svg qt6-tools qt6-wayland
```

If pacman shows keyring errors, rerun the pacman keyring step above.

## Recommended location

Keep the repo and build output on persistent storage, not inside the SteamOS root filesystem.

Example:

```text
/var/mnt/Storage/Dev/PrimedSteam
```

This helps prevent SteamOS updates from disrupting your source tree or build output.

## More build details

See:

```text
BUILDING-LINUX.md
```

## Legal notes

This repository contains source code, build scripts, and documentation only.

This repository does **not** distribute:

* Game ISOs, ROMs, WADs, or disc images
* Nintendo BIOS, keys, firmware, or system files
* Copyrighted texture packs or assets you do not own
* Steam or Valve runtime files copied from a local installation
* Local build folders such as `build-steamos/`, `build-linux/`, or `build-portable-test/`

Preserve upstream license files, SPDX notices, and third-party license documentation.

