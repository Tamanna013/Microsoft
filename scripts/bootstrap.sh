#!/bin/bash
set -euo pipefail

echo "Bootstrapping vcpkg..."

if [ ! -d "vcpkg/.git" ]; then
    git clone https://github.com/microsoft/vcpkg.git vcpkg
fi

cd vcpkg
git fetch
# Pin to the same baseline as vcpkg.json
git checkout 3d9943660bf1cb54de93b9cd41eb43ef5be5241b

if [ "$OS" = "Windows_NT" ]; then
    ./bootstrap-vcpkg.bat -disableMetrics
else
    ./bootstrap-vcpkg.sh -disableMetrics
fi

cd ..
echo "vcpkg bootstrapped successfully."
