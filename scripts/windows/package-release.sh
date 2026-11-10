#!/usr/bin/env bash
# Build the TX2000W Windows installers from macOS through Parallels Desktop.
#
# The Visual Studio project is regenerated here with the Projucer, then
# package-release.ps1 runs inside the Windows guest. The repository is expected
# to be visible in the guest through Parallels shared folders; by default the
# Mac home directory is mounted at C:\Mac\Home.

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
PRODUCT="TX2000W"
source "${SCRIPT_DIR}/../common.sh"

VM_NAME="${PARALLELS_VM_NAME:-Windows 11}"
relative_repo_root="${REPO_ROOT#"$HOME"/}"
WINDOWS_REPO_ROOT="${PARALLELS_WINDOWS_REPO_ROOT:-C:\Mac\Home\\${relative_repo_root//\//\\}}"
ARCHITECTURE_REQUEST="all"
CONFIGURATION="Release"
VERSION_OVERRIDE=""
SKIP_BUILD=0
SKIP_GENERATE=0
CLEAN=0

usage() {
  cat <<'EOF'
Usage: ./scripts/windows/package-release.sh [options]

Regenerates the Visual Studio project, then runs
scripts/windows/package-release.ps1 inside a running Parallels Desktop VM to
build the standalone app and VST3 plug-in and wrap them in an installer.

Options:
  --vm NAME                Parallels VM name or UUID (default: Windows 11)
  --windows-repo PATH      Repository path inside Windows
                           (default: this checkout under C:\Mac\Home)
  --architecture ARCH      x64, arm64, or all (default: all)
  --configuration NAME     Release or Debug (default: Release)
  --version VERSION        Override the version read from the .jucer
  --skip-build             Package existing Windows build outputs
  --skip-generate          Do not regenerate the Visual Studio project
  --clean                  Remove the selected Windows build output first
  -h, --help               Show this help

Environment overrides:
  PARALLELS_VM_NAME, PARALLELS_WINDOWS_REPO_ROOT, PROJUCER
EOF
}

normalize_architecture() {
  case "$1" in
    x64|X64) printf 'x64' ;;
    arm64|ARM64) printf 'ARM64' ;;
    all|ALL) printf 'all' ;;
    *) die "invalid architecture: $1 (expected x64, arm64, or all)" ;;
  esac
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --vm) [[ $# -ge 2 ]] || die "--vm requires an argument"; VM_NAME="$2"; shift 2 ;;
      --windows-repo) [[ $# -ge 2 ]] || die "--windows-repo requires an argument"; WINDOWS_REPO_ROOT="$2"; shift 2 ;;
      --architecture) [[ $# -ge 2 ]] || die "--architecture requires an argument"; ARCHITECTURE_REQUEST="$(normalize_architecture "$2")"; shift 2 ;;
      --configuration) [[ $# -ge 2 ]] || die "--configuration requires an argument"; CONFIGURATION="$2"; shift 2 ;;
      --version) [[ $# -ge 2 ]] || die "--version requires an argument"; VERSION_OVERRIDE="$2"; shift 2 ;;
      --skip-build) SKIP_BUILD=1; shift ;;
      --skip-generate) SKIP_GENERATE=1; shift ;;
      --clean) CLEAN=1; shift ;;
      -h|--help) usage; exit 0 ;;
      *) die "unknown argument: $1 (try --help)" ;;
    esac
  done
}

check_vm() {
  local status_output vm_status

  if ! status_output="$(prlctl status "$VM_NAME" 2>&1)"; then
    die "could not query Parallels VM '${VM_NAME}': ${status_output}"
  fi
  vm_status="${status_output##* }"
  [[ "$vm_status" == "running" ]] \
    || die "Parallels VM '${VM_NAME}' is not running (status: ${vm_status}); start it before packaging"
}

package_architecture() {
  local architecture="$1"
  local -a powershell_args

  powershell_args=(-Architecture "$architecture" -Configuration "$CONFIGURATION")
  [[ -n "$VERSION_OVERRIDE" ]] && powershell_args+=(-Version "$VERSION_OVERRIDE")
  [[ "$SKIP_BUILD" -eq 1 ]] && powershell_args+=(-SkipBuild)
  [[ "$CLEAN" -eq 1 ]] && powershell_args+=(-Clean)

  log "Packaging Windows ${architecture} in Parallels VM '${VM_NAME}'"
  prlctl exec "$VM_NAME" --current-user powershell.exe \
    -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
    -File "${WINDOWS_REPO_ROOT}\\scripts\\windows\\package-release.ps1" \
    "${powershell_args[@]}" \
    || die "Windows package script failed for ${architecture}"
}

main() {
  local -a architectures

  parse_args "$@"
  case "$CONFIGURATION" in
    Debug|Release) ;;
    *) die "invalid configuration: ${CONFIGURATION} (expected Debug or Release)" ;;
  esac
  case "$ARCHITECTURE_REQUEST" in
    all) architectures=(x64 ARM64) ;;
    *) architectures=("$ARCHITECTURE_REQUEST") ;;
  esac

  require_cmd prlctl
  check_vm

  [[ "$SKIP_GENERATE" -eq 1 ]] || generate_projects
  [[ -f "${WINDOWS_PROJECT_DIR}/${PRODUCT}.sln" ]] \
    || die "missing Visual Studio solution: ${WINDOWS_PROJECT_DIR}/${PRODUCT}.sln"

  for architecture in "${architectures[@]}"; do
    package_architecture "$architecture"
  done

  log "Windows installers are in ${DIST_DIR}"
  for installer in "${DIST_DIR}/${PRODUCT} Windows "*" Setup.exe"; do
    [[ -f "$installer" ]] && printf '%s\n' "$installer"
  done
  return 0
}

main "$@"
