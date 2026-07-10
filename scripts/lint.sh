#!/bin/bash
set -euo pipefail

if [ ! -f "build/release/compile_commands.json" ]; then
    echo "Error: compile_commands.json not found!"
    echo "Please configure the project first to generate it:"
    echo "  cmake --preset release"
    exit 1
fi

echo "Running clang-tidy..."
find . -type f \( -name "*.h" -o -name "*.cpp" \) -not -path "./vcpkg/*" -not -path "./build/*" | xargs -I {} clang-tidy -p build/release {}
echo "Linting complete."
