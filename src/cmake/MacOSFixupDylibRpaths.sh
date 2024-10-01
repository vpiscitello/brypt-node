#!/usr/bin/env bash
directory=$1
find "${directory}" -maxdepth 1 -type f -name "*.dylib" -exec basename {} ";" | while read -r -d $'\n' file
do
    echo "-- Updating ${file}'s install name to use rpath..."
    install_name_tool -id "@rpath/${file}" "${directory}/${file}"
done


