#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$ROOT"

version=$(tr -d '[:space:]' < VERSION)
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    printf 'VERSION must contain three numeric components.\n' >&2
    exit 1
}
source_revision=$(git rev-parse --verify HEAD)
common_revision=$(git rev-parse --verify HEAD:infiltratr-common)
for revision in "$source_revision" "$common_revision"; do
    [[ "$revision" =~ ^[0-9a-f]{40}$ ]] || {
        printf 'Git returned an invalid release revision.\n' >&2
        exit 1
    }
done

output=${1:-"runnerscope-${version}-native-installer.run"}
mkdir -p -- "$(dirname "$output")"
{
    printf '%s\n' '#!/usr/bin/env bash' '# SPDX-License-Identifier: GPL-3.0-or-later'
    printf "version='%s'\nsource_revision='%s'\ncommon_revision='%s'\n" \
        "$version" "$source_revision" "$common_revision"
    cat <<'RUNNER_INSTALLER'
set -Eeuo pipefail
source_url="https://github.com/Infiltrator-Projects/RunnerScope/archive/${source_revision}.tar.gz"
common_url="https://github.com/Infiltrator-Projects/Libraries/archive/${common_revision}.tar.gz"
system_package_mode=0
for argument in "$@"; do
    case "$argument" in
        -h|--help)
            printf 'RunnerScope %s source-download installer\n\n' "$version"
            printf 'Downloads the exact release source, compiles it for this machine and installs it.\n'
            printf 'Source revision: %s\n\n' "$source_revision"
            printf 'Build policy: native CPU tuning + LTO + trained two-pass PGO\n'
            printf 'Options: --jobs NUMBER --skip-tests --no-strip --dry-run\n'
            printf '         --system-package-mode -h --help\n'
            exit 0 ;;
        --system-package-mode) system_package_mode=1 ;;
    esac
done
if ((EUID == 0 && system_package_mode == 0)); then
    printf 'Do not run this file with sudo or as root.\n' >&2
    exit 1
fi
if ((EUID != 0 && system_package_mode != 0)); then
    printf 'Error: --system-package-mode requires root.\n' >&2
    exit 1
fi

curl_path=$(command -v curl 2>/dev/null || true)
wget_path=$(command -v wget 2>/dev/null || true)
if [[ -z "$curl_path" && -z "$wget_path" ]]; then
    printf 'Install curl or wget, then run the installer again.\n' >&2
    exit 1
fi
command -v tar >/dev/null 2>&1 || {
    printf 'The installer requires tar.\n' >&2
    exit 1
}

download() {
    if [[ -n "$curl_path" ]]; then
        "$curl_path" --fail --show-error --location --proto '=https' --proto-redir '=https' \
            --connect-timeout 30 --max-time 300 --retry 2 --output "$2" "$1"
    else
        "$wget_path" --https-only --timeout=30 --tries=3 --output-document="$2" "$1"
    fi
}

work=$(mktemp -d "${TMPDIR:-/tmp}/runnerscope-native-XXXXXX")
cleanup() { rm -rf -- "$work"; }
trap cleanup EXIT
source_root="$work/source"
mkdir -p -- "$source_root"
printf 'Downloading RunnerScope %s source...\n' "$version"
download "$source_url" "$work/runnerscope.tar.gz" || {
    printf 'Unable to download the release source. Nothing was installed.\n' >&2
    exit 1
}
tar -xzf "$work/runnerscope.tar.gz" --strip-components=1 --no-same-owner -C "$source_root"
[[ -r "$source_root/VERSION" ]] || {
    printf 'Release source has no VERSION.\n' >&2
    exit 1
}
actual_version=$(tr -d '[:space:]' < "$source_root/VERSION")
[[ "$actual_version" == "$version" ]] || {
    printf 'Release source version does not match this installer.\n' >&2
    exit 1
}

printf 'Downloading matching Libraries source...\n'
download "$common_url" "$work/libraries.tar.gz" || {
    printf 'Unable to download the Libraries source. Nothing was installed.\n' >&2
    exit 1
}
common_root="$source_root/infiltratr-common"
mkdir -p -- "$common_root"
tar -xzf "$work/libraries.tar.gz" --strip-components=1 --no-same-owner -C "$common_root"
[[ -s "$common_root/VERSION" ]] || {
    printf 'Libraries source is incomplete.\n' >&2
    exit 1
}

bootstrap="$source_root/support/installer/bootstrap.sh"
[[ -x "$bootstrap" ]] || {
    printf 'Downloaded native build bootstrap was not found.\n' >&2
    exit 1
}
printf 'Compiling RunnerScope for this machine...\n'
"$bootstrap" "$@"
RUNNER_INSTALLER
} > "$output"
chmod 0755 "$output"
printf '%s\n' "$output"
