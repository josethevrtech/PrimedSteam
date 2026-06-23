#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build-linux}"

cmake -S "$repo_root" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DLINUX_LOCAL_DEV=ON \
  -DUSE_SYSTEM_FMT=OFF \
  -DENABLE_VR=ON \
  -DENABLE_AUTOUPDATE=OFF \
  -DENABLE_ANALYTICS=OFF \
  -DDISTRIBUTOR="PrimedSteam"

cmake --build "$build_dir" --parallel

echo
echo "Built:"
echo "$build_dir/Binaries/PrimedGun"
