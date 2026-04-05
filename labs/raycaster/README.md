# Raycaster Lab

This package contains the solution for laboratory work 2.

Project layout:
- `main.cpp` and `main_window.*` define the application entry point and main window.
- `widgets/` stores the reusable raycaster widget and all geometry/controller classes.
- `BUILD.bazel` contains Bazel targets for the geometry library, UI library, and final binary.

Run target:
- `bazel run //labs/raycaster`
