#!/usr/bin/env bash
# Build every desktop package and create one draft GitHub Release:
#   macOS    universal arm64 + x86_64 DMG (standalone app with AUv3, VST3)
#   Windows  x64 + ARM64 installers (standalone app, VST3), through Parallels
#
# The release is always created as a draft so it can be inspected before
# publishing.

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
MACOS_SCRIPT="${SCRIPT_DIR}/macos/package-release.sh"
WINDOWS_SCRIPT="${SCRIPT_DIR}/windows/package-release.sh"
CONFIG_FILE="${SCRIPT_DIR}/macos/config.env"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi

: "${RELEASE_BRANCH:=main}"
: "${RELEASE_REMOTE:=}"
: "${GH_REPO:=}"
: "${PARALLELS_VM_NAME:=Windows 11}"
: "${PARALLELS_WINDOWS_REPO_ROOT:=}"

TAG_OVERRIDE=""
VERSION_OVERRIDE=""
MACOS_IDENTITY_OVERRIDE=""
TEAM_ID_OVERRIDE=""
NOTARY_PROFILE_OVERRIDE=""
FORCE=0

PRODUCT="TX2000W"
source "${SCRIPT_DIR}/common.sh"

usage() {
  cat <<'EOF'
Usage: ./scripts/package-release.sh [options]

Builds every desktop package and creates one draft GitHub Release:
  macOS    universal arm64 + x86_64 DMG (standalone app with AUv3, VST3)
  Windows  x64 + ARM64 installers (standalone app, VST3) through Parallels Desktop

The script refuses to build if the working tree is dirty or if HEAD is not
exactly equal to the configured release branch on the configured remote.

Options:
  --version VERSION        Release version (default: juce/S-MU2000.jucer)
  --tag vX.Y.Z             Release tag (default: v<version>)
  --remote NAME            Git remote (default: scripts/macos/config.env or auto)
  --repo OWNER/REPO        GitHub repository (default: config or remote)
  --identity NAME          macOS Developer ID Application identity
  --team-id ID             Apple Developer Team ID
  --notary-profile NAME    macOS notarytool Keychain profile
  --force                  Remove stale outputs for this version and rebuild them
  --vm NAME                Parallels VM name or UUID (default: Windows 11)
  --windows-repo PATH      Windows-side repository path (default: this checkout under C:\Mac\Home)
  -h, --help               Show this help

Machine-specific values can be configured in scripts/macos/config.env.
The GitHub Release is always created as a draft.
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --version) [[ $# -ge 2 ]] || die "--version requires an argument"; VERSION_OVERRIDE="$2"; shift 2 ;;
      --tag) [[ $# -ge 2 ]] || die "--tag requires an argument"; TAG_OVERRIDE="$2"; shift 2 ;;
      --remote) [[ $# -ge 2 ]] || die "--remote requires an argument"; RELEASE_REMOTE="$2"; shift 2 ;;
      --repo) [[ $# -ge 2 ]] || die "--repo requires an argument"; GH_REPO="$2"; shift 2 ;;
      --identity) [[ $# -ge 2 ]] || die "--identity requires an argument"; MACOS_IDENTITY_OVERRIDE="$2"; shift 2 ;;
      --team-id) [[ $# -ge 2 ]] || die "--team-id requires an argument"; TEAM_ID_OVERRIDE="$2"; shift 2 ;;
      --notary-profile) [[ $# -ge 2 ]] || die "--notary-profile requires an argument"; NOTARY_PROFILE_OVERRIDE="$2"; shift 2 ;;
      --force) FORCE=1; shift ;;
      --vm) [[ $# -ge 2 ]] || die "--vm requires an argument"; PARALLELS_VM_NAME="$2"; shift 2 ;;
      --windows-repo) [[ $# -ge 2 ]] || die "--windows-repo requires an argument"; PARALLELS_WINDOWS_REPO_ROOT="$2"; shift 2 ;;
      -h|--help) usage; exit 0 ;;
      *) die "unknown argument: $1 (try --help)" ;;
    esac
  done
}

check_parallels_vm() {
  local status_output vm_status

  if ! status_output="$(prlctl status "$PARALLELS_VM_NAME" 2>&1)"; then
    die "could not query Parallels VM '${PARALLELS_VM_NAME}': ${status_output}"
  fi
  vm_status="${status_output##* }"
  [[ "$vm_status" == "running" ]] \
    || die "Parallels VM '${PARALLELS_VM_NAME}' is not running (status: ${vm_status}); start it before releasing"
}

run_macos_package() {
  local -a args

  args=(
    --package-only
    --version "$VERSION"
    --tag "$RELEASE_TAG"
    --remote "$RELEASE_REMOTE"
  )
  [[ -n "$MACOS_IDENTITY_OVERRIDE" ]] && args+=(--identity "$MACOS_IDENTITY_OVERRIDE")
  [[ -n "$TEAM_ID_OVERRIDE" ]] && args+=(--team-id "$TEAM_ID_OVERRIDE")
  [[ -n "$NOTARY_PROFILE_OVERRIDE" ]] && args+=(--notary-profile "$NOTARY_PROFILE_OVERRIDE")
  [[ "$FORCE" -eq 1 ]] && args+=(--force)

  "$MACOS_SCRIPT" "${args[@]}"
}

run_windows_package() {
  local -a args

  # The macOS step has just regenerated the projects from this commit.
  args=(
    --vm "$PARALLELS_VM_NAME"
    --architecture all
    --configuration Release
    --version "$VERSION"
    --skip-generate
  )
  [[ -n "$PARALLELS_WINDOWS_REPO_ROOT" ]] && args+=(--windows-repo "$PARALLELS_WINDOWS_REPO_ROOT")
  [[ "$FORCE" -eq 1 ]] && args+=(--clean)
  "$WINDOWS_SCRIPT" "${args[@]}"
}

check_artifacts() {
  local artifact

  ARTIFACTS=(
    "${DIST_DIR}/${PRODUCT}-${VERSION}-macOS.dmg"
    "${DIST_DIR}/${PRODUCT} Windows x64 ${VERSION} Setup.exe"
    "${DIST_DIR}/${PRODUCT} Windows ARM64 ${VERSION} Setup.exe"
  )
  for artifact in "${ARTIFACTS[@]}"; do
    [[ -f "$artifact" ]] || die "release artifact was not created: $artifact"
  done
}

create_draft_release() {
  check_git_state
  assert_source_unchanged

  log "Creating and pushing tag: $RELEASE_TAG"
  git -C "$REPO_ROOT" tag -a "$RELEASE_TAG" "$RELEASE_COMMIT" -m "${PRODUCT} $RELEASE_TAG"
  git -C "$REPO_ROOT" push "$RELEASE_REMOTE" "$RELEASE_TAG"

  log "Creating draft GitHub Release: $GH_REPO/$RELEASE_TAG"
  gh release create "$RELEASE_TAG" "${ARTIFACTS[@]}" \
    --repo "$GH_REPO" \
    --verify-tag \
    --title "${PRODUCT} $RELEASE_TAG" \
    --generate-notes \
    --draft
}

main() {
  parse_args "$@"

  for cmd in git gh prlctl xcodebuild codesign hdiutil ditto security xcrun plutil lipo; do
    require_cmd "$cmd"
  done
  [[ -x "$MACOS_SCRIPT" ]] || die "missing executable macOS package script: $MACOS_SCRIPT"
  [[ -x "$WINDOWS_SCRIPT" ]] || die "missing executable Windows package script: $WINDOWS_SCRIPT"

  resolve_release_remote
  resolve_version_and_tag
  check_git_state
  gh auth status >/dev/null 2>&1 || die "gh is not authenticated; run gh auth login first"
  resolve_github_repo
  check_release_target
  check_parallels_vm

  log "Building macOS package"
  run_macos_package
  assert_source_unchanged

  log "Building Windows packages"
  run_windows_package
  assert_source_unchanged

  check_artifacts
  create_draft_release

  cat <<EOF

${PRODUCT} draft release ready
  tag     : $RELEASE_TAG
  commit  : $RELEASE_COMMIT
  repo    : $GH_REPO
  remote  : $RELEASE_REMOTE
  assets  : ${#ARTIFACTS[@]}
EOF
  for artifact in "${ARTIFACTS[@]}"; do
    printf '  - %s\n' "$artifact"
  done
}

main "$@"
