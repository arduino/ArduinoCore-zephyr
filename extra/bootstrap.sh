#!/bin/bash
#
# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -e
set -o pipefail

log_msg() {
	if [ -n "$GITHUB_WORKSPACE" ] ; then
		echo "::$1::$2"
	else
		echo " - $2"
	fi
}

if [ ! -f boards.txt ]; then
  echo "Launch this script from the root core folder as ./extra/bootstrap.sh"
  exit 2
fi

REQUIRED_TOOLS="cmake curl git jq ninja pip3 python3 wget zip"
MISSING_TOOLS=""
for tool in $REQUIRED_TOOLS; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    MISSING_TOOLS="$MISSING_TOOLS $tool"
  fi
done
if [ -n "$MISSING_TOOLS" ]; then
  echo "Missing required tools:$MISSING_TOOLS" >&2
  echo "Install them before running this script." >&2
  exit 2
fi

if [ -n "$WEST_CACHE" ]; then
  # Set in CI context, uses a shared cache for west blobs and modules
  mkdir -p $WEST_CACHE/blobs $WEST_CACHE/modules
  WEST_BLOBS_CACHE="--cache-dirs $WEST_CACHE/blobs --auto-cache $WEST_CACHE/blobs"
  WEST_MODULES_CACHE="--auto-cache $WEST_CACHE/modules"
fi

PYTHON_VENV_PATH=${PYTHON_VENV_PATH:-"./venv"}

get_unique_field_values() {
  local field="$1"
  local file="$2"
  grep "$field=" "$file" | cut -d '=' -f 2 | xargs -n 1 echo | sort -u | xargs echo
}

get_hals() {
  get_unique_field_values 'build.zephyr_hals' boards.txt
}

get_toolchains() {
  get_unique_field_values 'build.zephyr_toolchain' boards.txt
}

# --- Subcommands ---

setup_venv() {
  log_msg "group" "Bootstrapping Python environment for Zephyr"
  python3 -m venv venv
  [[ -z "${DISABLE_VENV:-}" ]] && { source ${PYTHON_VENV_PATH}/bin/activate; }
  pip3 install west protobuf grpcio-tools
  log_msg "endgroup"
}

west_init() {
  local NEEDED_HALS
  NEEDED_HALS=$(get_hals)

  local HAL_FILTER="-hal_.*"
  for hal in $NEEDED_HALS; do
    HAL_FILTER="$HAL_FILTER,+$hal"
  done

  if ! [ -d ../.west ] ; then
    west init -l . "$@"
  else
    log_msg "warning" "Zephyr workspace already initialized, skipping west init"
  fi
  west config manifest.project-filter -- "$HAL_FILTER"
  log_msg "endgroup"
}

west_update() {
  log_msg "group" "Initializing Zephyr modules"

  west update $WEST_MODULES_CACHE "$@"
  west zephyr-export
  pip3 install -r ../zephyr/scripts/requirements-base.txt
  log_msg "endgroup"
}

install_sdk() {
  local NEEDED_TOOLCHAINS
  NEEDED_TOOLCHAINS=$(get_toolchains)

  local TOOLCHAIN_VERSIONS
  TOOLCHAIN_VERSIONS=$(for tc in $NEEDED_TOOLCHAINS; do
    version=$(jq -r --arg name "$tc" '.toolsDependencies[] | select(.name == $name) | .version' extra/artifacts/_common.json)
    if [ -z "$version" ]; then
      echo "No version found for toolchain '$tc' in extra/artifacts/_common.json" >&2
      exit 1
    fi
    echo "$version $tc"
  done | sort -V)

  for version in $(echo "$TOOLCHAIN_VERSIONS" | cut -d ' ' -f 1 | sort -u -V); do
    toolchains=$(echo "$TOOLCHAIN_VERSIONS" | awk -v v="$version" '$1 == v { print $2 }')
    log_msg "group" "Installing Zephyr SDK ${version}: ${toolchains}"
    west sdk install "$@" --version "$version" -t $toolchains
    log_msg "endgroup"
  done
}

fetch_blobs() {
  local NEEDED_HALS
  NEEDED_HALS="arduino-api $(get_hals)"
  log_msg "group" "Fetching blobs for: $NEEDED_HALS"
  west blobs fetch $WEST_BLOBS_CACHE "$@" $NEEDED_HALS
  log_msg "endgroup"
}

all() {
  setup_venv
  west_init
  west_update $@
  install_sdk
  fetch_blobs
}

show_usage() {
  echo "Usage: $0 [command] [args...]"
  echo ""
  echo "Available commands:"
  echo "  venv         Bootstrap Python venv and basic dependencies"
  echo "  west_init    Initialize West workspace"
  echo "  west_update  Update West workspace and dependencies"
  echo "  sdk          Install required Zephyr SDKs"
  echo "  blobs        Fetch required HAL blobs"
  echo "  all          Execute all commands sequentially (default)"
}

# --- Main Dispatcher ---

SUBCOMMAND="${1:-all}"

case "$SUBCOMMAND" in
  venv)
    setup_venv
    ;;
  west_init)
    shift
    west_init "$@"
    ;;
  west_update)
    shift
    west_update "$@"
    ;;
  sdk)
    shift
    install_sdk "$@"
    ;;
  blobs)
    shift
    fetch_blobs "$@"
    ;;
  all)
    shift
    all $@
    ;;
  -h|--help|help)
    show_usage
    ;;
  *)
    all $@
    ;;
esac
