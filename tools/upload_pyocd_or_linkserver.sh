#!/bin/sh

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

ELF="$1"
TARGET="$2"
ADDR="$3"

LINKSERVER=""
if [ "$(uname)" = "Darwin" ]; then
    LINKSERVER_DIR=$(ls /Applications/ | grep "^LinkServer" | sort -V | tail -1)
    if [ -n "$LINKSERVER_DIR" ]; then
        LINKSERVER="/Applications/$LINKSERVER_DIR/LinkServer"
    fi
elif [ "$(uname)" = "Linux" ]; then
    if [ -x /usr/local/LinkServer/LinkServer ]; then
        LINKSERVER=/usr/local/LinkServer/LinkServer
    else
        LINKSERVER_DIR=$(ls -d /usr/local/LinkServer_* 2>/dev/null | sort -V | tail -1)
        if [ -n "$LINKSERVER_DIR" ]; then
            LINKSERVER="$LINKSERVER_DIR/LinkServer"
        fi
    fi
fi

if [ -z "$LINKSERVER" ] || [ ! -x "$LINKSERVER" ]; then
    LINKSERVER=$(command -v LinkServer 2>/dev/null)
fi

if [ -n "$LINKSERVER" ] && [ -x "$LINKSERVER" ]; then
    echo "LinkServer found, using: $LINKSERVER"
    if [ -n "$ADDR" ]; then
        exec "$LINKSERVER" flash MCXN947:FRDM-MCXN947 load "$ELF:$ADDR"
    else
        exec "$LINKSERVER" flash MCXN947:FRDM-MCXN947 load "$ELF"
    fi
fi

echo "LinkServer not found, using: pyocd"
if [ -n "$ADDR" ]; then
    exec pyocd load --target "$TARGET" "$ELF@$ADDR"
else
    exec pyocd load --target "$TARGET" "$ELF"
fi
