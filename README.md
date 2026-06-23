# PrimedSteam AKA PrimedGun for SteamOS

![PrimedGun gameplay](assets/readme/primedgun-hero.jfif)

PrimedSteam is a SteamOS-focused build of PrimedGun, a Dolphin ReduX-based build focused on improving Metroid Prime's VR experience by Nobbie248.

## About this fork

PrimedSteam is not intended to replace or take credit for the original PrimedGun project.

This fork exists to make PrimedGun easier to build and run on SteamOS, especially for users who are new to Linux or using SteamOS as their main desktop/VR system.

Original PrimedGun credit goes to Nobbie248. This repository adds SteamOS-focused build fixes, documentation, and helper scripts.

These instructions are for this PrimedSteam fork. They are not guaranteed to work with the original upstream PrimedGun repository unless the same SteamOS build patches and scripts are applied there.

## What this fork changes

This fork keeps the project focused on SteamOS compatibility and build reliability.

Current SteamOS-focused changes include:

* Added a simple Linux/SteamOS build script: `scripts/build-linux.sh`
* Added SteamOS build documentation: `BUILDING-LINUX.md`
* Updated the README for new SteamOS users
* Added pacman keyring setup instructions for fresh SteamOS installs
* Added SteamOS dependency instructions
* Added guidance for SteamOS update-channel issues
* Fixed OpenXR loader builds when exception handling is disabled globally
* Fixed Qt private QPA header detection without hardcoding a Qt version
* Cleaned up small compiler warnings that affected the SteamOS build

This fork does not distribute games, ROMs, BIOS files, Nintendo system files, copyrighted texture packs, or Steam/Valve runtime files.


## Verified system

Tested on:

```text
SteamOS 3.9 Build: 20260617.1001
Kernel: 6.18.33-valve2-1-neptune-618-g29c9aecca098
CPU: AMD Ryzen 9 PRO 8945HS w/ Radeon 780M Graphics
RAM: 32 GiB
GPU: AMD Radeon RX 7900 GRE
```

## What this guide does

This guide will help you:

1. Prepare SteamOS for building software
2. Install the required build tools
3. Download PrimedSteam
4. Build PrimedGun
5. Launch it

## Start here

Go to **Desktop Mode**, open **Konsole**, then run the commands below.

## 1. Allow package installs

SteamOS normally protects the system files. Temporarily allow package installs:

```bash
sudo steamos-readonly disable
```

## 2. Prepare the SteamOS package keyring

Run this before installing packages. This helps avoid keyring errors on new SteamOS installs or after update-channel changes.

```bash
sudo install -d -m 755 /etc/pacman.d/gnupg
sudo chown -R root:root /etc/pacman.d/gnupg
sudo pacman-key --init

for keyring in /usr/share/pacman/keyrings/*.gpg; do
  sudo pacman-key --populate "$(basename "$keyring" .gpg)"
done

sudo pacman -Syy
```

## 3. Install the required tools

```bash
sudo pacman -S git base-devel cmake ninja pkgconf clang libglvnd \
  qt6-base qt6-svg qt6-tools qt6-wayland \
  mesa vulkan-headers vulkan-radeon \
  libx11 libxi libxrandr libxext libxrender libxfixes \
  libevdev systemd-libs alsa-lib libpulse bluez-libs \
  openal ffmpeg openxr sdl3 libxcb xcb-proto xorgproto \
  glibc linux-api-headers
```

## 4. Download PrimedSteam

Choose a place to keep the source code.

This example uses:

```text
~/Dev
```

Run:

```bash
mkdir -p ~/Dev
cd ~/Dev

git clone --recursive https://github.com/josethevrtech/PrimedSteam.git
cd PrimedSteam
```

## 5. Build PrimedGun

```bash
./scripts/build-linux.sh
```

The finished app will be created here:

```text
build-linux/Binaries/PrimedGun
```

## 6. Check that it built correctly

```bash
ldd build-linux/Binaries/PrimedGun | grep "not found" || echo "runtime libs OK"
```

If you see:

```text
runtime libs OK
```

you are good.

## 7. Launch PrimedGun

```bash
./build-linux/Binaries/PrimedGun
```

## Updating later

To update the source code and rebuild:

```bash
cd ~/Dev/PrimedSteam
git pull
git submodule update --init --recursive
./scripts/build-linux.sh
```

## Build in a custom folder

You can choose a different build folder:

```bash
./scripts/build-linux.sh build-steamos
```

or:

```bash
./scripts/build-linux.sh build-portable-test
```

## Troubleshooting

### `cmake: command not found`

Install CMake and Ninja again:

```bash
sudo pacman -S cmake ninja base-devel
```

### `EGL/egl.h: No such file or directory`

Reinstall `libglvnd`:

```bash
sudo pacman -S libglvnd
```

### Qt private QPA header errors

Reinstall the Qt packages:

```bash
sudo pacman -S qt6-base qt6-svg qt6-tools qt6-wayland
```

### Pacman keyring errors

Rerun the package keyring step near the top of this guide.

## Recommended location

For a normal SteamOS desktop, `~/Dev/PrimedSteam` is simple and easy.

For a separate storage drive, a good location is:

```text
/var/mnt/Storage/Dev/PrimedSteam
```

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
