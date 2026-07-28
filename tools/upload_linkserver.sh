#!/bin/sh
ELF="$1"
ADDR="$2"

LINKSERVER=""
if [ "$(uname)" = "Darwin" ]; then
    LINKSERVER_DIR=$(ls /Applications/ | grep "^LinkServer" | sort -V | tail -1)
    if [ -n "$LINKSERVER_DIR" ]; then
        LINKSERVER="/Applications/$LINKSERVER_DIR/LinkServer"
    fi
fi

if [ -z "$LINKSERVER" ] || [ ! -f "$LINKSERVER" ]; then
    echo "ERROR: LinkServer not found."
    exit 1
fi

echo "Using: $LINKSERVER"

if [ -n "$ADDR" ]; then
    "$LINKSERVER" flash MCXN947:FRDM-MCXN947 load "$ELF:$ADDR"
else
    "$LINKSERVER" flash MCXN947:FRDM-MCXN947 load "$ELF"
fi
