#!/usr/bin/env bash
set -euo pipefail

ORG="Infiltrator-Projects"
BRANCH="main"

command -v gh >/dev/null 2>&1 || { echo 'gh is required.' >&2; exit 1; }
command -v jq >/dev/null 2>&1 || { echo 'jq is required.' >&2; exit 1; }
command -v base64 >/dev/null 2>&1 || { echo 'base64 is required.' >&2; exit 1; }
command -v sed >/dev/null 2>&1 || { echo 'sed is required.' >&2; exit 1; }

gh auth status >/dev/null 2>&1 || {
  echo 'GitHub CLI is not authenticated.' >&2
  exit 1
}

if ! runners_json="$(gh api --paginate "/orgs/${ORG}/actions/runners?per_page=100" 2>/tmp/runnerscope-routing-api.err)"; then
  cat /tmp/runnerscope-routing-api.err >&2 || true
  echo >&2
  echo 'Runner administration is not authorised for the current gh login.' >&2
  echo 'Grant it once with: gh auth refresh -h github.com -s admin:org' >&2
  exit 1
fi

mapfile -t bigbedroom_ids < <(
  jq -r '.runners[] | select(.name | startswith("BigBedroom-Linux-")) | .id' <<<"$runners_json"
)
mapfile -t latitude_ids < <(
  jq -r '.runners[] | select(.name | startswith("Latitude-5550-Linux")) | .id' <<<"$runners_json"
)

if [[ ${#bigbedroom_ids[@]} -ne 5 ]]; then
  printf 'Expected 5 BigBedroom runners, found %d.\n' "${#bigbedroom_ids[@]}" >&2
  exit 1
fi
if [[ ${#latitude_ids[@]} -ne 5 ]]; then
  printf 'Expected 5 Latitude runners, found %d.\n' "${#latitude_ids[@]}" >&2
  exit 1
fi

echo 'Applying dedicated routing labels...'
for id in "${bigbedroom_ids[@]}"; do
  gh api --method POST -H 'Accept: application/vnd.github+json' \
    "/orgs/${ORG}/actions/runners/${id}/labels" -f 'labels[]=bigbedroom' >/dev/null
  gh api --method DELETE -H 'Accept: application/vnd.github+json' \
    "/orgs/${ORG}/actions/runners/${id}/labels/latitude" >/dev/null 2>&1 || true
done
for id in "${latitude_ids[@]}"; do
  gh api --method POST -H 'Accept: application/vnd.github+json' \
    "/orgs/${ORG}/actions/runners/${id}/labels" -f 'labels[]=latitude' >/dev/null
  gh api --method DELETE -H 'Accept: application/vnd.github+json' \
    "/orgs/${ORG}/actions/runners/${id}/labels/bigbedroom" >/dev/null 2>&1 || true
done

update_workflow() {
  local repo="$1" path="$2"
  local meta sha old new encoded payload
  meta="$(gh api "/repos/${ORG}/${repo}/contents/${path}?ref=${BRANCH}")"
  sha="$(jq -r '.sha' <<<"$meta")"
  old="$(mktemp)"
  new="$(mktemp)"
  jq -r '.content' <<<"$meta" | tr -d '\n' | base64 -d >"$old"
  sed -E \
    -e "/runs-on:/ s/'linux-native'/'bigbedroom'/g" \
    -e '/runs-on:/ s/runs-on:[[:space:]]+linux-native/runs-on: [self-hosted, Linux, X64, linux-native, bigbedroom]/' \
    -e '/runs-on:/ s/\[self-hosted, Linux, X64, linux-native\]/[self-hosted, Linux, X64, linux-native, bigbedroom]/' \
    "$old" >"$new"
  if cmp -s "$old" "$new"; then
    rm -f "$old" "$new"
    printf '%s: %s already routed.\n' "$repo" "$path"
    return 0
  fi
  encoded="$(base64 -w0 "$new")"
  payload="$(jq -n --arg message 'Prefer BigBedroom self-hosted runners' --arg content "$encoded" --arg sha "$sha" --arg branch "$BRANCH" '{message:$message,content:$content,sha:$sha,branch:$branch}')"
  gh api --method PUT "/repos/${ORG}/${repo}/contents/${path}" --input - <<<"$payload" >/dev/null
  rm -f "$old" "$new"
  printf '%s: updated %s\n' "$repo" "$path"
}

update_defragmenter_contract() {
  local repo="Defragmenter" path="defragger/tests/test_release_gate.py"
  local meta sha old new encoded payload
  meta="$(gh api "/repos/${ORG}/${repo}/contents/${path}?ref=${BRANCH}")"
  sha="$(jq -r '.sha' <<<"$meta")"
  old="$(mktemp)"
  new="$(mktemp)"
  jq -r '.content' <<<"$meta" | tr -d '\n' | base64 -d >"$old"
  sed 's/runs-on: \[self-hosted, Linux, X64, linux-native\]/runs-on: [self-hosted, Linux, X64, linux-native, bigbedroom]/g' "$old" >"$new"
  if cmp -s "$old" "$new"; then
    rm -f "$old" "$new"
    echo 'Defragmenter: release-gate contract already routed.'
    return 0
  fi
  encoded="$(base64 -w0 "$new")"
  payload="$(jq -n --arg message 'Align runner routing contract with BigBedroom preference' --arg content "$encoded" --arg sha "$sha" --arg branch "$BRANCH" '{message:$message,content:$content,sha:$sha,branch:$branch}')"
  gh api --method PUT "/repos/${ORG}/${repo}/contents/${path}" --input - <<<"$payload" >/dev/null
  rm -f "$old" "$new"
  echo 'Defragmenter: updated release-gate contract.'
}

targets=(
  'RunnerScope|.github/workflows/ci.yml'
  'System-Monitor|.github/workflows/ci.yml'
  'Infiltrator-Libraries|.github/workflows/ci.yml'
  'backyard-racer|.github/workflows/ci.yml'
  'Jaglink|.github/workflows/ci.yml'
  'InfiltratorFS|.github/workflows/ci.yml'
  'InfiltratorFS|.github/workflows/kernel-module.yml'
  'Defragmenter|.github/workflows/local-quality.yml'
  'FORDLINK|.github/workflows/ci.yml'
  'BMWLink|.github/workflows/ci.yml'
  'AUDILINK|.github/workflows/ci.yml'
  'LINK|.github/workflows/ci.yml'
)

echo 'Rewriting development CI selectors to BigBedroom...'
for target in "${targets[@]}"; do
  repo="${target%%|*}"
  path="${target#*|}"
  update_workflow "$repo" "$path"
done
update_defragmenter_contract

echo 'Verifying routing labels...'
verify_json="$(gh api --paginate "/orgs/${ORG}/actions/runners?per_page=100")"
for prefix in BigBedroom-Linux-1 BigBedroom-Linux-2 BigBedroom-Linux-3 BigBedroom-Linux-4 BigBedroom-Linux-5; do
  jq -e --arg name "$prefix" '.runners[] | select(.name == $name) | any(.labels[]; .name == "bigbedroom")' <<<"$verify_json" >/dev/null
done
for name in Latitude-5550-Linux Latitude-5550-Linux-2 Latitude-5550-Linux-3 Latitude-5550-Linux-4 Latitude-5550-Linux-5; do
  jq -e --arg name "$name" '.runners[] | select(.name == $name) | any(.labels[]; .name == "latitude")' <<<"$verify_json" >/dev/null
done

echo
echo 'BigBedroom routing is active.'
echo 'Normal development jobs now target the BigBedroom pool; release commits remain on GitHub-hosted runners.'
