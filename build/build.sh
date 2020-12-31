#!/usr/bin/env bash
set -e

TARGET="cmake-build-debug"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

(
    cd $DIR/../service

    mkdir -p $TARGET
    cd $TARGET

    cmake -DCMAKE_BUILD_TYPE=Debug ../
    make

    if [[ "$1" == -r ]]; then
        echo "RUNNING..."
        ./easy-gestured
    fi
)
