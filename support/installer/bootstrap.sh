#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)

system_package_mode=0
dry_run=0
skip_tests=0
no_strip=0
jobs=""

usage() {
    cat <<'TEXT'
RunnerScope hardware-native installer

Build policy: native CPU tuning + LTO + trained two-pass PGO
Options:
  --jobs NUMBER          Parallel build jobs (default: detected CPU count)
  --skip-tests           Skip post-PGO qualification tests; PGO training still runs
  --no-strip             Keep debug symbols in the generated Debian package
  --dry-run              Check requirements without building or installing
  --system-package-mode  Run as root without sudo (for CI/package automation)
  -h, --help             Show this help
TEXT
}

while (($#)); do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --system-package-mode) system_package_mode=1 ;;
        --dry-run) dry_run=1 ;;
        --skip-tests) skip_tests=1 ;;
        --no-strip) no_strip=1 ;;
        --jobs)
            shift
            [[ $# -gt 0 && "$1" =~ ^[1-9][0-9]*$ ]] || {
                printf 'Error: --jobs requires a positive integer.\n' >&2
                exit 2
            }
            jobs="$1"
            ;;
        *) printf 'Error: unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if ((EUID == 0 && system_package_mode == 0)); then
    printf 'Do not run this installer with sudo or as root.\n' >&2
    printf 'Run it as your normal user; it will request sudo only for packages/install.\n' >&2
    exit 1
fi
if ((EUID != 0 && system_package_mode != 0)); then
    printf 'Error: --system-package-mode requires root.\n' >&2
    exit 1
fi

if [[ -z "$jobs" ]]; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || printf '2')
    [[ "$jobs" =~ ^[1-9][0-9]*$ ]] || jobs=2
fi

missing=()
packages=()
need_package() {
    local label=$1 package=$2 command_name=$3
    if ! command -v "$command_name" >/dev/null 2>&1; then
        missing+=("$label")
        packages+=("$package")
    fi
}
need_package "GCC" build-essential gcc
need_package "CMake" cmake cmake
need_package "Debian package builder" dpkg-dev dpkg-buildpackage
need_package "debhelper" debhelper dh
need_package "pkg-config" pkg-config pkg-config
need_package "Xvfb" xvfb xvfb-run
need_package "xauth" xauth xauth

if ! pkg-config --exists gtk+-3.0 >/dev/null 2>&1; then
    missing+=("GTK 3 development files")
    packages+=(libgtk-3-dev)
fi
if ! pkg-config --exists libcurl >/dev/null 2>&1; then
    missing+=("libcurl development files")
    packages+=(libcurl4-openssl-dev)
fi
if ! dpkg-query -W -f='${Status}' ca-certificates 2>/dev/null | grep -Fqx 'install ok installed'; then
    packages+=(ca-certificates)
fi

if ((${#missing[@]})); then
    printf 'Missing build requirements: %s\n' "${missing[*]}" >&2
fi
if ((dry_run)); then
    if ((${#missing[@]})); then
        printf 'Dry run: install requirements first: %s\n' "${packages[*]}" >&2
        exit 1
    fi
    printf 'Dry run: RunnerScope native build requirements are satisfied.\n'
    printf 'Build policy would be: -O3 -march=native -mtune=native -flto + two-pass PGO.\n'
    exit 0
fi

if ((${#packages[@]})); then
    unique_packages=()
    declare -A seen=()
    for package in "${packages[@]}"; do
        [[ -n "${seen[$package]:-}" ]] && continue
        seen[$package]=1
        unique_packages+=("$package")
    done
    apt_get=/usr/bin/apt-get
    sudo_path=/usr/bin/sudo
    [[ -x "$apt_get" ]] || { printf 'apt-get is required to install build dependencies.\n' >&2; exit 1; }
    if ((system_package_mode)); then
        "$apt_get" -o DPkg::Lock::Timeout=300 update
        "$apt_get" -o DPkg::Lock::Timeout=300 install -y "${unique_packages[@]}"
    else
        [[ -x "$sudo_path" ]] || { printf 'sudo is required to install build dependencies.\n' >&2; exit 1; }
        "$sudo_path" -- "$apt_get" -o DPkg::Lock::Timeout=300 update
        "$sudo_path" -- "$apt_get" -o DPkg::Lock::Timeout=300 install -y "${unique_packages[@]}"
    fi
fi

gcc_path=$(command -v gcc)
export CC="$gcc_path"

version=$(tr -d '[:space:]' < "$ROOT/VERSION")
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    printf 'Invalid RunnerScope VERSION in downloaded source.\n' >&2
    exit 1
}
[[ -s "$ROOT/infiltratr-common/VERSION" ]] || {
    printf 'The matching Libraries source is missing.\n' >&2
    exit 1
}

build="$ROOT/build-debian"
profile="$ROOT/.runnerscope-pgo"
rm -rf -- "$build" "$profile"
mkdir -p -- "$profile"

generate_flags="-O3 -march=native -mtune=native -flto -fprofile-generate=$profile"
generate_link="-flto -fprofile-generate=$profile"
use_flags="-O3 -march=native -mtune=native -flto -fprofile-use=$profile -fprofile-correction -Wno-missing-profile -Wno-error=missing-profile -Wno-error=coverage-mismatch"
use_link="-flto -fprofile-use=$profile -fprofile-correction"

printf 'Building RunnerScope %s PGO training image (%s jobs)...\n' "$version" "$jobs"
cmake -S "$ROOT" -B "$build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$gcc_path" \
    -DCMAKE_C_FLAGS="$generate_flags" \
    -DCMAKE_EXE_LINKER_FLAGS="$generate_link"
cmake --build "$build" --parallel "$jobs"

printf 'Training the native profile...\n'
"$build/runnerscope" --self-test
XDG_CONFIG_HOME="$ROOT/.pgo-config" \
XDG_STATE_HOME="$ROOT/.pgo-state" \
    xvfb-run -a "$build/runnerscope" --ui-self-test
ctest --test-dir "$build" --output-on-failure
if ! find "$profile" -type f -name '*.gcda' -print -quit | grep -q .; then
    printf 'PGO training produced no profile data; refusing a falsely-labelled aggressive build.\n' >&2
    exit 1
fi

printf 'Rebuilding RunnerScope with the trained native profile...\n'
cmake -S "$ROOT" -B "$build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$gcc_path" \
    -DCMAKE_C_FLAGS="$use_flags" \
    -DCMAKE_EXE_LINKER_FLAGS="$use_link"
cmake --build "$build" --clean-first --parallel "$jobs"

if ((skip_tests == 0)); then
    ctest --test-dir "$build" --output-on-failure
    XDG_CONFIG_HOME="$ROOT/.qualified-config" \
    XDG_STATE_HOME="$ROOT/.qualified-state" \
        xvfb-run -a "$build/runnerscope" --ui-self-test
fi

export DEB_CFLAGS_MAINT_APPEND="$use_flags"
export DEB_LDFLAGS_MAINT_APPEND="$use_link"
build_options="${DEB_BUILD_OPTIONS:-}"
((skip_tests)) && build_options="${build_options} nocheck"
((no_strip)) && build_options="${build_options} nostrip"
export DEB_BUILD_OPTIONS="$build_options"

printf 'Packaging the trained build...\n'
(
    cd "$ROOT"
    dpkg-buildpackage --no-pre-clean -us -uc -b -j"$jobs"
)

deb=$(find "$ROOT/.." -maxdepth 1 -type f -name "runnerscope_${version}_*.deb" -print -quit)
[[ -n "$deb" && -f "$deb" ]] || {
    printf 'Native Debian package was not produced.\n' >&2
    exit 1
}

dpkg-deb --info "$deb" >/dev/null
[[ "$(dpkg-deb -f "$deb" Package)" == runnerscope ]] || {
    printf 'Native package identity is invalid.\n' >&2
    exit 1
}
[[ "$(dpkg-deb -f "$deb" Version)" == "$version" ]] || {
    printf 'Native package version is invalid.\n' >&2
    exit 1
}

printf 'Installing the trained RunnerScope package...\n'
if ((system_package_mode)); then
    /usr/bin/apt-get -o DPkg::Lock::Timeout=300 install -y --reinstall "$deb"
else
    /usr/bin/sudo -- /usr/bin/apt-get -o DPkg::Lock::Timeout=300 install -y --reinstall "$deb"
fi

installed=$(dpkg-query -W -f='${Version}' runnerscope 2>/dev/null || true)
[[ "$installed" == "$version" ]] || {
    printf 'RunnerScope installation verification failed.\n' >&2
    exit 1
}
printf 'RunnerScope %s installed with native CPU tuning, LTO and trained PGO.\n' "$version"
