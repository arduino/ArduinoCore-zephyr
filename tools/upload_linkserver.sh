#!/bin/sh
ELF="$1"
ADDR="$2"

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

if [ -z "$LINKSERVER" ] || [ ! -x "$LINKSERVER" ]; then
    echo "ERROR: LinkServer not found."
    exit 1
fi

echo "Using: $LINKSERVER"

if [ -n "$ADDR" ]; then
    "$LINKSERVER" flash MCXN947:FRDM-MCXN947 load "$ELF:$ADDR"
else
    "$LINKSERVER" flash MCXN947:FRDM-MCXN947 load "$ELF"
fi
