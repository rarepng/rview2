#!/bin/bash

LOC="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "${BASH_SOURCE[0]}"
for dir in "$SCRIPT_DIR"/*/; do
    if [ -d "$dir" ]; then
        cd "$dir"
        git status
        git pull --recurse-submodules --prune -4
    fi
done

cd $LOC