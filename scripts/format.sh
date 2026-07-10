#!/bin/bash
set -euo pipefail

echo "Running clang-format..."
find . -type f \( -name "*.h" -o -name "*.cpp" \) -not -path "./vcpkg/*" -not -path "./build/*" -exec clang-format -i {} +
echo "Formatting complete."
