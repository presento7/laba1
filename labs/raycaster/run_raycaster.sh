#!/bin/bash

set -euo pipefail

workspace_dir="${BUILD_WORKSPACE_DIRECTORY:-$(pwd)}"
project_dir="$workspace_dir/labs/raycaster"
binary_dir="$workspace_dir/.bazel-raycaster"
binary_path="$binary_dir/raycaster"

qt_formula=""
for candidate in qtbase qt; do
    if qt_prefix_candidate="$(brew --prefix "$candidate" 2>/dev/null)"; then
        qt_formula="$candidate"
        qt_prefix="$qt_prefix_candidate"
        break
    fi
done

if [[ -z "${qt_formula}" ]]; then
    echo "Qt is not installed. Install it with: brew install qtbase" >&2
    exit 1
fi
plugin_dir=""
for candidate in \
    "$qt_prefix/share/qt/plugins" \
    "$qt_prefix/plugins"; do
    if [[ -d "$candidate" ]]; then
        plugin_dir="$candidate"
        break
    fi
done

if [[ -z "$plugin_dir" ]]; then
    echo "Qt plugins directory was not found under $qt_prefix" >&2
    exit 1
fi

mkdir -p "$binary_dir"

clang++ \
    -std=c++20 \
    -include arm_acle.h \
    -I"$project_dir" \
    -I"$project_dir/widgets" \
    -I"$qt_prefix/include" \
    -I"$qt_prefix/include/QtCore" \
    -I"$qt_prefix/include/QtGui" \
    -I"$qt_prefix/include/QtWidgets" \
    "$project_dir/main.cpp" \
    "$project_dir/main_window.cpp" \
    "$project_dir/widgets/canvas_widget.cpp" \
    "$project_dir/widgets/controller.cpp" \
    "$project_dir/widgets/polygon.cpp" \
    "$project_dir/widgets/ray.cpp" \
    -F"$qt_prefix/lib" \
    -Wl,-rpath,"$qt_prefix/lib" \
    -framework QtCore \
    -framework QtGui \
    -framework QtWidgets \
    -o "$binary_path"

export DYLD_FRAMEWORK_PATH="$qt_prefix/lib"
export QT_PLUGIN_PATH="$plugin_dir"
export QT_QPA_PLATFORM_PLUGIN_PATH="$plugin_dir/platforms"

exec "$binary_path"
