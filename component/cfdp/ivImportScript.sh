#!/bin/bash

user_input=$(zenity --entry --title="Enter the Application Process name to adjust data type names" --text="Enter the Application Process name to adjust data type names")

if [ $? -eq 0 ]; then
    echo "You entered: $user_input"
    user_input="${user_input//_/-}"
    sed -i "s/APP-MARKER/$user_input/" interfaceview.xml.tmp
    echo "$user_input" > /tmp/app_marker.tmp
else
    echo "User canceled the input."
fi

exit 0