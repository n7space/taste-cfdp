#!/bin/bash

filepath="/tmp/app_marker.tmp"

if [[ -f "$filepath" ]]; then
    user_input=$(<"$filepath")
    echo "You entered: $user_input"
    user_input="${user_input//-/_}"
    sed -i "s/APP_MARKER/$user_input/" C/src/filesystem.c
    user_input="${user_input//_/-}"
    sed -i "s/APP-MARKER/$user_input/" ../../filesystem.asn
    sed -i "s/APP-MARKER/$user_input/" ../../filesystem.acn
    rm -f "$filepath"
fi

exit 0