load("@rules_qt//:qt.bzl", "qt_cc_binary", "qt_cc_library")

qt_cc_library(
    name = "main_window",
    srcs = ["main_window.cpp"],
    hdrs = ["main_window.h"],
    deps = [
        "@rules_qt//:qt_core",
        "@rules_qt//:qt_gui",
        "@rules_qt//:qt_widgets",
    ],
)

qt_cc_binary(
    name = "app",
    srcs = ["main.cpp"],
    deps = [
        ":main_window",
        "@rules_qt//:qt_core",
        "@rules_qt//:qt_widgets",
    ],
)

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_tar(
    name = "release",
    srcs = [
        ":app",
    ],
    include_runfiles = True,
    package_dir = "app",
)
