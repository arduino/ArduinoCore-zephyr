#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -e

TOOL_DIR=$(readlink -f ${1:-.})
TOOL_NAME=$(basename $TOOL_DIR)
VERSION=${2:-dev}

if ! [ -f "$TOOL_DIR/go.mod" ] ; then
	echo "Not a valid tool directory: $TOOL_DIR"
	echo "Usage: $0 [<tool_dir>] [<version>]"
	exit 1
fi

BASE_DIR=$(readlink -f $(dirname $0)/..)
DIR=${BASE_DIR}/distrib

hash_file() {
	plat=$1
	file=$2
	name=$(basename $file)
	hash=$(shasum -a 256 $file | cut -d ' ' -f 1)
	len=$(wc -c < $file | tr -d ' ')
	cat << EOF
{
  "host": "$plat",
  "url": "https://downloads.arduino.cc/tools/$name",
  "archiveFileName": "$name",
  "checksum": "SHA-256:$hash",
  "size": "$len"
}
EOF
}

build_for_arch() {
	local os=$1
	local arch=$2
	local plat=$3

	if [ "$os" == "windows" ]; then
		app_ext=".exe"
		pkg_ext=".zip"
		pkg_cmd="zip -qr"
	else
		app_ext=""
		pkg_ext=".tar.gz"
		pkg_cmd="tar -czf"
	fi

	local tool_stem="$DIR/$TOOL_NAME-$VERSION"
	local build_dir="$tool_stem/$plat"
	local build_file="$TOOL_NAME$app_ext"
	local package_file="$tool_stem-$plat$pkg_ext"

	# The macOS binaries of MCUmgr need cgo, which Go only enables for the
	# native target: force it on for both macOS targets when the host is a Mac
	local cgo_env=""
	[ "$os" == "darwin" ] && [ "$(uname)" == "Darwin" ] && cgo_env="CGO_ENABLED=1"

	echo "Building $TOOL_NAME for $os/$arch ($plat)"
	mkdir -p "$build_dir"
	(cd $TOOL_DIR && env $cgo_env GOOS="$os" GOARCH="$arch" go build -o "$build_dir/$build_file")
	(cd "$tool_stem" && $pkg_cmd "$package_file" $plat/)
	hash_file $plat "$package_file" > $build_dir.json
}

build_json() {
	temp_file=$(mktemp)
	echo "{ \"packages\": [ { \"tools\": [ { \"name\": \"$TOOL_NAME\", \"version\": \"$VERSION\", \"systems\":" > $temp_file
	ls $DIR/$TOOL_NAME-$VERSION/*.json | sort | xargs cat | jq -s . >> $temp_file
	echo "} ] } ] }" >> $temp_file
	jq . $temp_file > $DIR/$TOOL_NAME-$VERSION.json
	rm -f $temp_file $DIR/$TOOL_NAME-$VERSION/*.json
}

build_for_arch "linux" "amd64" "x86_64-linux-gnu"
build_for_arch "linux" "arm64" "aarch64-linux-gnu"
build_for_arch "darwin" "arm64" "arm64-apple-darwin"
build_for_arch "darwin" "amd64" "x86_64-apple-darwin"
build_for_arch "windows" "386" "i686-mingw32"
if [ "${VERSION}" == "dev" ] ; then
	echo "Build completed for $TOOL_NAME $VERSION in $DIR"
else
	build_json
	echo "Build completed for $TOOL_NAME $VERSION: $DIR/$TOOL_NAME-$VERSION.json"
fi
