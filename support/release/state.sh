#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
: "${GITHUB_SHA:?}" "${GITHUB_REPOSITORY:?}" "${GITHUB_OUTPUT:?}"
version=$(tr -d '[:space:]' < VERSION)
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo 'VERSION must contain three numeric components.' >&2
    exit 1
}
printf 'version=%s\n' "$version" >> "$GITHUB_OUTPUT"
tag="v$version"
if ! git rev-parse -q --verify "refs/tags/$tag^{commit}" >/dev/null; then
    echo 'publish=true' >> "$GITHUB_OUTPUT"
    exit 0
fi
tagged=$(git rev-parse "refs/tags/$tag^{commit}")
if [[ "$tagged" != "$GITHUB_SHA" ]]; then
    printf 'VERSION %s is already tagged at %s; bump VERSION for %s.\n' "$version" "$tagged" "$GITHUB_SHA" >&2
    exit 1
fi
# A tag with a draft release is an interrupted publication, not a completed
# release. Resume qualification/publication of the same immutable identity.
draft=$(gh release view "$tag" --repo "$GITHUB_REPOSITORY" --json isDraft --jq .isDraft)
case "$draft" in
    true) echo 'publish=true' >> "$GITHUB_OUTPUT" ;;
    false) echo 'publish=false' >> "$GITHUB_OUTPUT" ;;
    *) echo 'Unable to determine whether the release is published.' >&2; exit 1 ;;
esac
