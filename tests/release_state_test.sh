#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/bin" "$work/repo"
cat > "$work/bin/gh" <<'MOCK'
#!/usr/bin/env bash
case "${RELEASE_FIXTURE:-}" in
    draft) echo true ;;
    published) echo false ;;
    error) exit 1 ;;
    *) echo unexpected ;;
esac
MOCK
chmod +x "$work/bin/gh"
export PATH="$work/bin:$PATH"
export GITHUB_REPOSITORY=fixture/RunnerScope
export GITHUB_OUTPUT="$work/output"
cd "$work/repo"
git init -q -b main
git config user.name Fixture
git config user.email fixture@example.invalid
echo 1.3.20 > VERSION
git add VERSION
git commit -qm 'Fixture source'
GITHUB_SHA=$(git rev-parse HEAD)
export GITHUB_SHA
bash "$root/support/release/state.sh"
grep -Fqx 'version=1.3.20' "$GITHUB_OUTPUT"
grep -Fqx 'publish=true' "$GITHUB_OUTPUT"
git tag v1.3.20
export RELEASE_FIXTURE=draft
: > "$GITHUB_OUTPUT"
bash "$root/support/release/state.sh"
grep -Fqx 'publish=true' "$GITHUB_OUTPUT"
export RELEASE_FIXTURE=published
: > "$GITHUB_OUTPUT"
bash "$root/support/release/state.sh"
grep -Fqx 'publish=false' "$GITHUB_OUTPUT"
export RELEASE_FIXTURE=error
if bash "$root/support/release/state.sh"; then exit 1; fi
export RELEASE_FIXTURE=invalid
if bash "$root/support/release/state.sh" 2>/dev/null; then exit 1; fi
echo changed > source
git add source
git commit -qm 'A later revision cannot reuse the published version'
GITHUB_SHA=$(git rev-parse HEAD)
export GITHUB_SHA
if bash "$root/support/release/state.sh" 2>/dev/null; then exit 1; fi
echo malformed > VERSION
if bash "$root/support/release/state.sh" 2>/dev/null; then exit 1; fi
echo 'Release recovery: new, draft, published, unavailable, malformed and immutable-tag states passed.'
