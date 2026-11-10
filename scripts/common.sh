# Shared helpers for the TX2000W release scripts. Sourced, not run.
#
# Callers set REPO_ROOT, PRODUCT, RELEASE_BRANCH, RELEASE_REMOTE, GH_REPO,
# VERSION_OVERRIDE and TAG_OVERRIDE before using the functions below.

: "${PROJUCER:=/Applications/Projucer.app/Contents/MacOS/Projucer}"

JUCER_FILE="${REPO_ROOT}/juce/S-MU2000.jucer"
MACOS_PROJECT_DIR="${REPO_ROOT}/juce/Builds/MacOSX"
WINDOWS_PROJECT_DIR="${REPO_ROOT}/juce/Builds/VisualStudio2026"
DIST_DIR="${REPO_ROOT}/dist"

log() {
  printf '==> %s\n' "$*" >&2
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

read_jucer_version() {
  local version
  version="$(tr '\n' ' ' <"$JUCER_FILE" \
    | sed -n 's/.*<JUCERPROJECT[^>]* version="\([^"]*\)".*/\1/p' \
    | head -n 1)"
  [[ -n "$version" ]] || die "could not read the version from $JUCER_FILE"
  printf '%s' "$version"
}

# Sets VERSION and RELEASE_TAG, and checks that they agree.
resolve_version_and_tag() {
  [[ -f "$JUCER_FILE" ]] || die "missing project file: $JUCER_FILE"
  VERSION="${VERSION_OVERRIDE:-$(read_jucer_version)}"
  [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] \
    || die "invalid version (expected X.Y.Z): $VERSION"
  RELEASE_TAG="${TAG_OVERRIDE:-v${VERSION}}"
  [[ "$RELEASE_TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.-]+)?$ ]] \
    || die "invalid release tag: $RELEASE_TAG"
  if [[ "$RELEASE_TAG" != "v${VERSION}" \
    && "$RELEASE_TAG" != "v${VERSION}-"* \
    && "$RELEASE_TAG" != "v${VERSION}."* ]]; then
    die "release tag must match the version (${VERSION}): ${RELEASE_TAG}"
  fi
}

resolve_release_remote() {
  local candidate tracked_remote

  if [[ -n "$RELEASE_REMOTE" ]]; then
    git -C "$REPO_ROOT" remote get-url "$RELEASE_REMOTE" >/dev/null 2>&1 \
      || die "configured release remote does not exist: $RELEASE_REMOTE"
    return
  fi

  tracked_remote="$(git -C "$REPO_ROOT" config --get "branch.${RELEASE_BRANCH}.remote" || true)"
  if [[ -n "$tracked_remote" ]] \
    && git -C "$REPO_ROOT" remote get-url "$tracked_remote" >/dev/null 2>&1; then
    RELEASE_REMOTE="$tracked_remote"
    return
  fi

  if git -C "$REPO_ROOT" remote get-url origin >/dev/null 2>&1; then
    RELEASE_REMOTE="origin"
    return
  fi

  candidate="$(git -C "$REPO_ROOT" remote | head -n 1)"
  [[ -n "$candidate" ]] || die "no Git remote is configured; set RELEASE_REMOTE first"
  RELEASE_REMOTE="$candidate"
}

resolve_github_repo() {
  local remote_url

  [[ -n "$GH_REPO" ]] && return

  remote_url="$(git -C "$REPO_ROOT" remote get-url "$RELEASE_REMOTE")"
  case "$remote_url" in
    https://github.com/*|http://github.com/*) GH_REPO="${remote_url#*github.com/}" ;;
    git@github.com:*) GH_REPO="${remote_url#git@github.com:}" ;;
    ssh://git@github.com/*) GH_REPO="${remote_url#ssh://git@github.com/}" ;;
  esac

  GH_REPO="${GH_REPO%.git}"
  [[ "$GH_REPO" == */* ]] && return

  GH_REPO="$(gh repo view --json nameWithOwner --jq '.nameWithOwner')" \
    || die "could not determine GitHub repository"
}

# The release must be built from a clean, pushed commit on the release branch,
# with the submodules at their recorded commits. Sets RELEASE_COMMIT and
# START_STATUS. ALLOW_DIRTY=1 (package-only test builds) skips the branch,
# clean and pushed checks.
check_git_state() {
  local current_branch status_text remote_head submodules

  RELEASE_COMMIT="$(git -C "$REPO_ROOT" rev-parse HEAD)"
  START_STATUS="$(git -C "$REPO_ROOT" status --porcelain --untracked-files=all)"
  if [[ "${ALLOW_DIRTY:-0}" -eq 1 ]]; then
    log "warning: test build of uncommitted changes; the result must not be published"
    check_submodules
    return
  fi

  current_branch="$(git -C "$REPO_ROOT" branch --show-current)"
  [[ "$current_branch" == "$RELEASE_BRANCH" ]] \
    || die "release must run on branch '$RELEASE_BRANCH' (current: ${current_branch:-detached HEAD})"

  status_text="$(git -C "$REPO_ROOT" status --porcelain --untracked-files=all)"
  [[ -z "$status_text" ]] || {
    printf '%s\n' "$status_text" >&2
    die "working tree is not clean; commit all changes before releasing"
  }

  git -C "$REPO_ROOT" fetch --quiet "$RELEASE_REMOTE" "$RELEASE_BRANCH" \
    || die "could not fetch $RELEASE_REMOTE/$RELEASE_BRANCH"
  remote_head="$(git -C "$REPO_ROOT" rev-parse "$RELEASE_REMOTE/$RELEASE_BRANCH")"
  [[ "$RELEASE_COMMIT" == "$remote_head" ]] \
    || die "HEAD is not pushed to $RELEASE_REMOTE/$RELEASE_BRANCH; commit and push before releasing"

  check_submodules
}

check_submodules() {
  local submodules

  git -C "$REPO_ROOT" submodule update --init --recursive \
    || die "could not initialize/update submodules"
  submodules="$(git -C "$REPO_ROOT" submodule status --recursive)"
  if printf '%s\n' "$submodules" | grep -qE '^[-+U]'; then
    printf '%s\n' "$submodules" >&2
    die "a submodule is not checked out at the recorded commit"
  fi
  [[ -d "${REPO_ROOT}/third_party/JUCE/modules" ]] || die "JUCE submodule is missing"
  [[ -d "${REPO_ROOT}/third_party/R2JUCE/modules" ]] || die "R2JUCE submodule is missing"
}

check_release_target() {
  local remote_status

  if git -C "$REPO_ROOT" rev-parse --verify --quiet "refs/tags/$RELEASE_TAG" >/dev/null; then
    die "local tag already exists: $RELEASE_TAG"
  fi

  if git -C "$REPO_ROOT" ls-remote --exit-code --refs "$RELEASE_REMOTE" \
    "refs/tags/$RELEASE_TAG" >/dev/null 2>&1; then
    die "remote tag already exists: $RELEASE_TAG"
  else
    remote_status=$?
    [[ "$remote_status" -eq 2 ]] \
      || die "could not check whether the remote tag exists: $RELEASE_TAG"
  fi

  if gh release view "$RELEASE_TAG" --repo "$GH_REPO" >/dev/null 2>&1; then
    die "GitHub Release already exists: $GH_REPO/$RELEASE_TAG"
  fi
}

assert_source_unchanged() {
  local current_head status_text

  current_head="$(git -C "$REPO_ROOT" rev-parse HEAD)"
  [[ "$current_head" == "$RELEASE_COMMIT" ]] \
    || die "HEAD changed while building; refusing to release a different commit"
  status_text="$(git -C "$REPO_ROOT" status --porcelain --untracked-files=all)"
  [[ "$status_text" == "${START_STATUS:-}" ]] \
    || die "working tree changed while building; refusing to release"
}

# juce/Builds is not in Git, so the Xcode and Visual Studio projects are
# regenerated from the .jucer for every release.
generate_projects() {
  local before

  [[ -x "$PROJUCER" ]] || die "Projucer not found: $PROJUCER (set PROJUCER)"
  mkdir -p "$DIST_DIR"
  before="$(git -C "$REPO_ROOT" status --porcelain --untracked-files=all)"
  log "Regenerating the IDE projects with $(basename "$PROJUCER")"
  "$PROJUCER" --resave "$JUCER_FILE" >"${DIST_DIR}/projucer-resave.log" 2>&1 \
    || { cat "${DIST_DIR}/projucer-resave.log" >&2; die "Projucer --resave failed"; }
  [[ "$(git -C "$REPO_ROOT" status --porcelain --untracked-files=all)" == "$before" ]] \
    || die "Projucer --resave changed tracked files; commit the resaved .jucer first"
}
