#!/bin/bash

if [ ! -f platform.txt ]; then
  echo Launch this script from the root core folder as ./extras/package.sh VERSION
  exit 2
fi

FOLDER=`basename $PWD`

VERSION=$1

cd ..
tar  --exclude=extras/** --exclude=.git* --exclude=build --exclude=venv --exclude=samples -cjhf ArduinoCore-zephyr-${VERSION}.tar.bz2 $FOLDER
