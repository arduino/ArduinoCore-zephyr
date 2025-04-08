#!/bin/bash

if [ -z "$CORE_TAG" ]; then
  echo "This script can be used in Github CI only."
  exit 1
fi

export ARTIFACT_HASH=$(sha256sum $CORE_ARTIFACT)
export ARTIFACT_SIZE=$(stat -c %s $CORE_ARTIFACT)

envsubst < extra/zephyr-core-template.json > $PACKAGE_INDEX_JSON
