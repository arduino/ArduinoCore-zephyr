#!/bin/bash

DIR=$(dirname "$(readlink -f "$0")")
VERSION=$(grep "define BUILD_VERSION" ${DIR}/llext-edk/include/zephyr/include/generated/zephyr/version.h | cut -f3 -d " ")
if [ "$VERSION" != "$1" ]; then
    echo "Loader version mismatch: expected $1, got $VERSION, reflashing loader"
    exit 1
fi
exit 0