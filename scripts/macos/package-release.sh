#!/usr/bin/env bash
# Build, sign, notarize and staple the TX2000W macOS package, and optionally
# publish it as a GitHub Release.
#
# The generated DMG contains:
#   TX2000W.app            standalone app, with the AUv3 extension inside
#   Applications        -> /Applications
#   TX2000W.vst3           VST3 plug-in
#   VST3 Plug-Ins       -> /Library/Audio/Plug-Ins/VST3
#
# The script refuses to run with a dirty worktree. A release must be created
# from the exact commit that was built and notarized.

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
CONFIG_FILE="${SCRIPT_DIR}/config.env"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi

: "${APP_IDENTITY:=Developer ID Application: Masanao Hayashi (P5G28RMWUN)}"
: "${TEAM_ID:=P5G28RMWUN}"
: "${NOTARY_PROFILE:=Notalization2021}"
: "${BUILD_CONFIGURATION:=Release}"
: "${BUILD_ARCHS:=arm64 x86_64}"
: "${RELEASE_BRANCH:=main}"
: "${RELEASE_REMOTE:=}"
: "${GH_REPO:=}"

TAG_OVERRIDE=""
VERSION_OVERRIDE=""
DRAFT_RELEASE=0
PACKAGE_ONLY=0
FORCE=0
ALLOW_DIRTY=0

PRODUCT="TX2000W"
source "${SCRIPT_DIR}/../common.sh"
XCODE_PROJECT="${MACOS_PROJECT_DIR}/${PRODUCT}.xcodeproj"
APP_ENTITLEMENTS="${MACOS_PROJECT_DIR}/Standalone_Plugin.entitlements"
APPEX_ENTITLEMENTS="${MACOS_PROJECT_DIR}/AUv3_AppExtension.entitlements"

usage() {
  cat <<'EOF'
Usage: ./scripts/macos/package-release.sh [options]

Builds a signed Universal macOS standalone app (with the AUv3 extension) and
VST3 plug-in, creates a DMG with drag targets for both, notarizes and staples
the DMG, then publishes a GitHub Release with the DMG attached unless
--package-only is specified.

Options:
  --tag vX.Y.Z           Override the Git tag (default: v<version>)
  --version VERSION      Override the version read from juce/S-MU2000.jucer
  --identity NAME        Developer ID Application identity
  --team-id ID           Apple Developer Team ID
  --notary-profile NAME  notarytool Keychain profile
  --remote NAME          Git remote used for fetch/tag checks and tag push
  --repo OWNER/REPO      GitHub repository (default: from the remote)
  --architectures LIST   Space-separated macOS architectures (default: arm64 x86_64)
  --package-only         Build and notarize the DMG without tagging or publishing
  --draft                Leave the GitHub Release as a draft
  --force                Remove stale outputs for this version and rebuild them
  --allow-dirty          With --package-only: build uncommitted changes for testing
  -h, --help             Show this help

The following environment variables can also be set in scripts/macos/config.env:
  APP_IDENTITY, TEAM_ID, NOTARY_PROFILE, BUILD_CONFIGURATION,
  BUILD_ARCHS, RELEASE_BRANCH, RELEASE_REMOTE, GH_REPO, PROJUCER
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --tag) [[ $# -ge 2 ]] || die "--tag requires an argument"; TAG_OVERRIDE="$2"; shift 2 ;;
      --version) [[ $# -ge 2 ]] || die "--version requires an argument"; VERSION_OVERRIDE="$2"; shift 2 ;;
      --identity) [[ $# -ge 2 ]] || die "--identity requires an argument"; APP_IDENTITY="$2"; shift 2 ;;
      --team-id) [[ $# -ge 2 ]] || die "--team-id requires an argument"; TEAM_ID="$2"; shift 2 ;;
      --notary-profile) [[ $# -ge 2 ]] || die "--notary-profile requires an argument"; NOTARY_PROFILE="$2"; shift 2 ;;
      --remote) [[ $# -ge 2 ]] || die "--remote requires an argument"; RELEASE_REMOTE="$2"; shift 2 ;;
      --repo) [[ $# -ge 2 ]] || die "--repo requires an argument"; GH_REPO="$2"; shift 2 ;;
      --architectures) [[ $# -ge 2 ]] || die "--architectures requires an argument"; BUILD_ARCHS="$2"; shift 2 ;;
      --package-only) PACKAGE_ONLY=1; shift ;;
      --draft) DRAFT_RELEASE=1; shift ;;
      --force) FORCE=1; shift ;;
      --allow-dirty) ALLOW_DIRTY=1; shift ;;
      -h|--help) usage; exit 0 ;;
      *) die "unknown argument: $1 (try --help)" ;;
    esac
  done
}

check_signing_identity() {
  local identities
  identities="$(security find-identity -v -p codesigning 2>/dev/null)" \
    || die "could not inspect code-signing identities"
  case "$identities" in
    *"$APP_IDENTITY"*) ;;
    *) die "Developer ID Application identity not found: $APP_IDENTITY" ;;
  esac
}

prepare_entitlements() {
  local source="$1"
  local destination="$2"

  [[ -f "$source" ]] || die "missing entitlements file: $source"
  # Projucer-generated entitlements may use CRLF line endings. Normalize a
  # temporary copy because codesign's AMFI parser rejects that XML on macOS.
  plutil -convert xml1 -o "$destination" "$source" \
    || die "could not normalize entitlements: $source"
  [[ -s "$destination" ]] || die "normalized entitlements are empty: $destination"
}

# Unsigned universal build settings shared by the archive and the VST3 build.
# The final Developer ID signatures are applied after all binary transformations.
set_xcode_settings() {
  XCODE_SETTINGS=(
    ONLY_ACTIVE_ARCH=NO
    "ARCHS=$BUILD_ARCHS"
    "VALID_ARCHS=$BUILD_ARCHS"
    CODE_SIGN_STYLE=Manual
    CODE_SIGN_IDENTITY=-
    "DEVELOPMENT_TEAM=$TEAM_ID"
    ENABLE_HARDENED_RUNTIME=YES
    CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO
    CODE_SIGNING_ALLOWED=NO
    CODE_SIGNING_REQUIRED=NO
    STRIP_INSTALLED_PRODUCT=NO
    COPY_PHASE_STRIP=NO
  )
}

check_universal() {
  local binary="$1" arch
  for arch in $BUILD_ARCHS; do
    lipo "$binary" -verify_arch "$arch" || die "$binary is missing architecture $arch"
  done
}

archive_app() {
  local archive_log="$WORK_DIR/xcodebuild-archive.log"
  local normalized_appex_entitlements="$WORK_DIR/AUv3_AppExtension.entitlements"
  local normalized_app_entitlements="$WORK_DIR/Standalone_Plugin.entitlements"

  log "Archiving ${PRODUCT} - Standalone Plugin (${BUILD_CONFIGURATION}, ${BUILD_ARCHS})"
  if ! xcodebuild archive \
    -project "$XCODE_PROJECT" \
    -scheme "${PRODUCT} - Standalone Plugin" \
    -configuration "$BUILD_CONFIGURATION" \
    -destination "generic/platform=macOS" \
    -archivePath "$ARCHIVE_PATH" \
    "${XCODE_SETTINGS[@]}" \
    2>&1 | tee "$archive_log" >/dev/null; then
    die "xcodebuild archive failed; see $archive_log"
  fi

  [[ -d "$APP_PATH" ]] || die "archive did not produce: $APP_PATH"
  [[ -d "$APPEX_PATH" ]] || die "AUv3 app extension is missing from the archived app: $APPEX_PATH"
  check_universal "$APP_PATH/Contents/MacOS/${PRODUCT}"
  check_universal "$APPEX_PATH/Contents/MacOS/${PRODUCT}"

  prepare_entitlements "$APPEX_ENTITLEMENTS" "$normalized_appex_entitlements"
  prepare_entitlements "$APP_ENTITLEMENTS" "$normalized_app_entitlements"

  log "Applying final Developer ID signatures (AUv3, then app)"
  codesign --force --sign "$APP_IDENTITY" \
    --verbose=4 --timestamp --options runtime \
    --entitlements "$normalized_appex_entitlements" \
    --generate-entitlement-der \
    "$APPEX_PATH"
  codesign --force --sign "$APP_IDENTITY" \
    --verbose=4 --timestamp --options runtime \
    --entitlements "$normalized_app_entitlements" \
    --generate-entitlement-der \
    "$APP_PATH"

  log "Verifying Developer ID signature on the archived app"
  codesign --verify --deep --strict --verbose=2 "$APP_PATH"
  # Captured first: `codesign | grep -q` fails under pipefail when grep exits early
  local signature_info
  signature_info="$(codesign --display --verbose=4 "$APP_PATH" 2>&1)" \
    || die "codesign display failed"
  case "$signature_info" in
    *"$APP_IDENTITY"*) ;;
    *) die "archived app is not signed with: $APP_IDENTITY" ;;
  esac
}

build_vst3() {
  local build_log="$WORK_DIR/xcodebuild-vst3.log"

  log "Building ${PRODUCT} - VST3 (${BUILD_CONFIGURATION}, ${BUILD_ARCHS})"
  if ! xcodebuild build \
    -project "$XCODE_PROJECT" \
    -scheme "${PRODUCT} - VST3" \
    -configuration "$BUILD_CONFIGURATION" \
    -destination "generic/platform=macOS" \
    -derivedDataPath "$WORK_DIR/vst3-derived" \
    "${XCODE_SETTINGS[@]}" \
    "CONFIGURATION_BUILD_DIR=$WORK_DIR/vst3-build" \
    2>&1 | tee "$build_log" >/dev/null; then
    die "xcodebuild VST3 build failed; see $build_log"
  fi

  # The project builds into its own build/ folder unless told otherwise, which
  # could hold an older plug-in; this build goes into the work folder
  local built="$WORK_DIR/vst3-build/${PRODUCT}.vst3"
  [[ -d "$built" ]] || die "VST3 build did not produce: $built"
  [[ -f "$built/Contents/Resources/moduleinfo.json" ]] \
    || die "VST3 bundle is missing moduleinfo.json: $built"
  COPYFILE_DISABLE=1 ditto --norsrc --noextattr --noqtn "$built" "$VST3_PATH"
  check_universal "$VST3_PATH/Contents/MacOS/${PRODUCT}"

  # A plug-in runs inside the host's process, so it takes no entitlements.
  log "Applying final Developer ID signature (VST3)"
  codesign --force --sign "$APP_IDENTITY" \
    --verbose=4 --timestamp --options runtime \
    "$VST3_PATH"
  codesign --verify --deep --strict --verbose=2 "$VST3_PATH"
}

create_dmg() {
  log "Preparing DMG contents"
  mkdir -p "$DMG_STAGE"
  COPYFILE_DISABLE=1 ditto --norsrc --noextattr --noqtn \
    "$APP_PATH" "$DMG_STAGE/${PRODUCT}.app"
  ln -s /Applications "$DMG_STAGE/Applications"
  COPYFILE_DISABLE=1 ditto --norsrc --noextattr --noqtn \
    "$VST3_PATH" "$DMG_STAGE/${PRODUCT}.vst3"
  ln -s /Library/Audio/Plug-Ins/VST3 "$DMG_STAGE/VST3 Plug-Ins"

  log "Creating DMG: $DMG_PATH"
  hdiutil create \
    -volname "$DMG_VOLUME_NAME" \
    -srcfolder "$DMG_STAGE" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -ov "$DMG_PATH" \
    >"$WORK_DIR/hdiutil-create.log" 2>&1 \
    || { cat "$WORK_DIR/hdiutil-create.log" >&2; die "hdiutil create failed"; }

  [[ -f "$DMG_PATH" ]] || die "DMG was not created: $DMG_PATH"

  log "Signing DMG"
  codesign --force --sign "$APP_IDENTITY" --timestamp "$DMG_PATH"
  codesign --verify --verbose=2 "$DMG_PATH"
  hdiutil verify "$DMG_PATH"
}

notarize_dmg() {
  local notary_output="$WORK_DIR/notary-submit.json"
  local submission_id

  log "Submitting DMG to Apple Notary Service (profile: $NOTARY_PROFILE)"
  if ! xcrun notarytool submit "$DMG_PATH" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait \
    --output-format json 2>&1 | tee "$notary_output"; then
    die "notarytool submit failed; see $notary_output"
  fi

  if ! grep -Eq '"status"[[:space:]]*:[[:space:]]*"Accepted"' "$notary_output"; then
    submission_id="$(sed -n 's/.*"id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
      "$notary_output" | head -n 1)"
    if [[ -n "$submission_id" ]]; then
      xcrun notarytool log "$submission_id" \
        --keychain-profile "$NOTARY_PROFILE" \
        "$WORK_DIR/notary-log.json" >/dev/null 2>&1 || true
    fi
    die "notarization was not accepted; see $notary_output and $WORK_DIR/notary-log.json"
  fi

  log "Stapling notarization ticket to DMG"
  xcrun stapler staple "$DMG_PATH"
  xcrun stapler validate "$DMG_PATH"
  codesign --verify --verbose=2 "$DMG_PATH"
  hdiutil verify "$DMG_PATH"
}

verify_dmg_contents() {
  local attach_log="$WORK_DIR/hdiutil-attach.log"

  MOUNT_POINT="$WORK_DIR/mounted-dmg"
  mkdir -p "$MOUNT_POINT"
  log "Checking DMG contents and drag targets"
  if ! hdiutil attach "$DMG_PATH" \
    -readonly -nobrowse -noautoopen -mountpoint "$MOUNT_POINT" \
    >"$attach_log" 2>&1; then
    cat "$attach_log" >&2
    die "could not mount the notarized DMG"
  fi
  MOUNTED=1

  [[ -d "$MOUNT_POINT/${PRODUCT}.app" ]] || die "DMG is missing ${PRODUCT}.app"
  [[ -d "$MOUNT_POINT/${PRODUCT}.vst3" ]] || die "DMG is missing ${PRODUCT}.vst3"
  [[ "$(readlink "$MOUNT_POINT/Applications")" == "/Applications" ]] \
    || die "DMG is missing the Applications link"
  [[ "$(readlink "$MOUNT_POINT/VST3 Plug-Ins")" == "/Library/Audio/Plug-Ins/VST3" ]] \
    || die "DMG is missing the VST3 Plug-Ins link"
  codesign --verify --deep --strict --verbose=2 "$MOUNT_POINT/${PRODUCT}.app"
  codesign --verify --deep --strict --verbose=2 "$MOUNT_POINT/${PRODUCT}.vst3"

  hdiutil detach "$MOUNT_POINT" >/dev/null \
    || hdiutil detach "$MOUNT_POINT" -force >/dev/null \
    || die "could not detach the DMG after verification"
  MOUNTED=0
}

publish_release() {
  local -a release_args

  assert_source_unchanged

  log "Creating and pushing tag: $RELEASE_TAG"
  git -C "$REPO_ROOT" tag -a "$RELEASE_TAG" "$RELEASE_COMMIT" -m "${PRODUCT} $RELEASE_TAG"
  git -C "$REPO_ROOT" push "$RELEASE_REMOTE" "$RELEASE_TAG"

  release_args=(
    release create "$RELEASE_TAG" "$DMG_PATH"
    --repo "$GH_REPO"
    --verify-tag
    --title "${PRODUCT} ${RELEASE_TAG}"
    --generate-notes
  )
  [[ "$DRAFT_RELEASE" -eq 1 ]] && release_args+=(--draft)

  log "Creating GitHub Release: $GH_REPO/$RELEASE_TAG"
  gh "${release_args[@]}"
}

cleanup() {
  if [[ "${MOUNTED:-0}" -eq 1 && -n "${MOUNT_POINT:-}" ]]; then
    hdiutil detach "$MOUNT_POINT" >/dev/null 2>&1 \
      || hdiutil detach "$MOUNT_POINT" -force >/dev/null 2>&1 \
      || true
  fi
}

main() {
  parse_args "$@"
  [[ "$ALLOW_DIRTY" -eq 0 || "$PACKAGE_ONLY" -eq 1 ]] \
    || die "--allow-dirty is only for --package-only test builds"

  for cmd in git xcodebuild codesign hdiutil ditto security xcrun plutil lipo; do
    require_cmd "$cmd"
  done
  [[ "$PACKAGE_ONLY" -eq 1 ]] || require_cmd gh

  resolve_release_remote
  resolve_version_and_tag

  WORK_DIR="${DIST_DIR}/work/${RELEASE_TAG}/macos"
  ARCHIVE_PATH="${WORK_DIR}/${PRODUCT}.xcarchive"
  APP_PATH="${ARCHIVE_PATH}/Products/Applications/${PRODUCT}.app"
  APPEX_PATH="${APP_PATH}/Contents/PlugIns/${PRODUCT}.appex"
  VST3_PATH="${WORK_DIR}/${PRODUCT}.vst3"
  DMG_STAGE="${WORK_DIR}/dmg-root"
  DMG_VOLUME_NAME="${PRODUCT} ${VERSION}"
  DMG_PATH="${DIST_DIR}/${PRODUCT}-${VERSION}-macOS.dmg"
  MOUNT_POINT=""
  MOUNTED=0
  trap cleanup EXIT

  check_git_state
  if [[ "$PACKAGE_ONLY" -eq 0 ]]; then
    gh auth status >/dev/null 2>&1 || die "gh is not authenticated; run gh auth login first"
    resolve_github_repo
    check_release_target
  fi
  check_signing_identity
  generate_projects
  [[ -d "$XCODE_PROJECT" ]] || die "Projucer did not generate: $XCODE_PROJECT"

  mkdir -p "$DIST_DIR"
  if [[ "$FORCE" -eq 1 ]]; then
    log "Removing stale macOS outputs for ${RELEASE_TAG}"
    rm -rf "$WORK_DIR"
    rm -f "$DMG_PATH"
  else
    [[ ! -e "$WORK_DIR" ]] || die "work directory already exists: $WORK_DIR (use --force to rebuild)"
    [[ ! -e "$DMG_PATH" ]] || die "DMG already exists: $DMG_PATH (use --force to rebuild)"
  fi
  mkdir -p "$WORK_DIR"

  set_xcode_settings
  archive_app
  build_vst3
  create_dmg
  notarize_dmg
  verify_dmg_contents
  [[ "$PACKAGE_ONLY" -eq 1 ]] || publish_release

  cat <<EOF

${PRODUCT} macOS package ready
  tag     : $RELEASE_TAG
  commit  : $RELEASE_COMMIT
  DMG     : $DMG_PATH
  notary  : $NOTARY_PROFILE
EOF
  if [[ "$PACKAGE_ONLY" -eq 1 ]]; then
    printf '  status  : package-only\n'
  elif [[ "$DRAFT_RELEASE" -eq 1 ]]; then
    printf '  status  : draft (%s)\n' "$GH_REPO"
  else
    printf '  status  : published (%s)\n' "$GH_REPO"
  fi
}

main "$@"
