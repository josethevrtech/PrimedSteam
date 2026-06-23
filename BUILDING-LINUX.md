# Building PrimedSteam / PrimedGun on Linux

## Arch / CachyOS / SteamOS dependencies

```bash
sudo pacman -S git base-devel cmake ninja pkgconf clang libglvnd \
  qt6-base qt6-svg qt6-tools qt6-wayland \
  mesa vulkan-headers vulkan-radeon \
  libx11 libxi libxrandr libxext libxrender libxfixes \
  libevdev systemd-libs alsa-lib libpulse bluez-libs \
  openal ffmpeg openxr sdl3 libxcb xcb-proto xorgproto \
  glibc linux-api-headers
```

## Build

```bash
git submodule update --init --recursive
./scripts/build-linux.sh
```

The binary will be generated at:

```text
build-linux/Binaries/PrimedGun
```

## SteamOS notes

SteamOS may require temporarily disabling the read-only rootfs before installing build dependencies:

```bash
sudo steamos-readonly disable
```

SteamOS update-channel changes can remove or invalidate build tools and development headers. Keep the source tree and build outputs on persistent storage, then reinstall dependencies if CMake or headers disappear.

If `EGL/egl.h` is missing, reinstall `libglvnd`.

Do not commit build folders such as `build-steamos/`, `build-linux/`, or `build-portable-test/`.
