#!/bin/bash
set -euo pipefail

readonly LLVM_IR_VERSION=14
additional_dependencies=("$@")

(
    source /etc/os-release
    distro="$ID" # ubuntu / debian / alpine / centos / rocky
    distro_version="$VERSION_ID" # 22.04 / 12 / 3.21.2 / 8 / 9.3
    # can be used to adapt to different distros / version

    packages=("${additional_dependencies[@]}")
    
    packages+=(
        git ca-certificates build-essential cmake ninja-build # build
        binutils # LTO
        "clang-$LLVM_IR_VERSION" # compiler for IR
        "libclang-rt-$LLVM_IR_VERSION-dev" # ASAN
        libboost-graph-dev libsqlite3-dev libssl-dev zlib1g-dev "libclang-$LLVM_IR_VERSION-dev" "llvm-$LLVM_IR_VERSION-dev" "libclang-common-$LLVM_IR_VERSION-dev" # build deps
    )   

    pkg_mgr=()
    if which sudo >/dev/null 2>&1; then
        pkg_mgr+=("sudo")
    fi

    if which apt-get >/dev/null 2>&1; then
        pkg_mgr+=("apt-get")
    else
        echo "Couldn't determine package manager, sry."
        exit 1
    fi

    "${pkg_mgr[@]}" update
    DEBIAN_FRONTEND=noninteractive "${pkg_mgr[@]}" install --no-install-recommends -y "${packages[@]}"
)
