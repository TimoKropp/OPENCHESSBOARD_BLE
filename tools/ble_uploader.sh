#!/bin/bash

SCRIPT_DIR=$( cd $( dirname $0 ) && pwd )
VSCODE_LAUNCH_PATH=$SCRIPT_DIR/../.vscode/launch.json
CURRENT_ENV=$( awk '/projectEnvName/ {print $2; exit}' $VSCODE_LAUNCH_PATH | sed 's/[",]//g' )
SCRIPT_PATH=$SCRIPT_DIR/../.pio/libdeps/$CURRENT_ENV/ArduinoBleOTA/tools/uploader.py
FIRMWARE_BIN_PATH=$SCRIPT_DIR/../.pio/build/$CURRENT_ENV/firmware.bin

python3 $SCRIPT_PATH $FIRMWARE_BIN_PATH