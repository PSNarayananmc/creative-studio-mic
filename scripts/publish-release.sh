#!/bin/bash
# Publishes only the explicitly requested new repository and a new release.
set -euo pipefail
cd "$(dirname "$0")/.."
repo=PSNarayananmc/creative-studio-mic
tag=v0.1.0
if [[ -n ${VC_GITHUB_TOKEN:-} ]]; then export GH_TOKEN="$VC_GITHUB_TOKEN"; fi
expected=$(gh api user --jq .login)
[[ "$expected" == PSNarayananmc ]] || { echo 'Expected the PSNarayananmc identity.' >&2; exit 1; }
test -f dist/voicecraft-studio_0.1.0_amd64.deb
test -f dist/SHA256SUMS
(cd dist && sha256sum --check SHA256SUMS)
if ! gh repo view "$repo" --json nameWithOwner --jq .nameWithOwner; then
  gh repo create "$repo" --public --description 'VoiceCraft Studio: native offline Linux microphone processing and OBS virtual microphone'
fi
url="https://github.com/$repo.git"
if git remote get-url origin >/dev/null 2>&1; then
  [[ $(git remote get-url origin) == "$url" ]] || { echo 'Unexpected origin; refusing to push.' >&2; exit 1; }
else git remote add origin "$url"; fi
# No token is persisted in repository configuration or URLs.
git -c credential.helper= -c 'credential.helper=!gh auth git-credential' push --set-upstream origin main
if [[ -n $(git ls-remote --tags origin "refs/tags/$tag") ]]; then
  echo 'Release tag already exists; refusing replacement.' >&2; exit 1
fi
gh release create "$tag" --repo "$repo" --target "$(git rev-parse HEAD)" \
  --title 'VoiceCraft Studio 0.1.0 — Ubuntu 20.04 amd64' \
  --notes-file docs/RELEASE_NOTES.md \
  dist/voicecraft-studio_0.1.0_amd64.deb dist/SHA256SUMS \
  dist/voicecraft-studio-0.1.0-source.tar.gz dist/voicecraft-studio-0.1.0-evidence.tar.gz
