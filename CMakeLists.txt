
option(NX_MODULE_STORE_APP_GALLERY "Build the Huawei App Gallery (HMS IAP Kit) storefront backend" OFF)

if (NOT NX_MODULE_STORE_APP_GALLERY)
    return()
endif ()

# nx_module()'s own PLATFORMS gate runs too late to help here - a project
# that also builds for Windows/Linux/macOS/iOS still passes
# -DNX_MODULE_STORE_APP_GALLERY=ON for this platform-irrelevant module (its
# module list isn't per-platform), so this must skip itself before ever
# reaching that gate, the same "skipped on <platform>" outcome
# _nx_module_gate would otherwise report.
if (NOT ANDROID)
    message(STATUS "nx2d: module 'store_app_gallery' skipped on this platform")
    return()
endif ()

# No vendor SDK to resolve here - HMS IAP Kit has no native/NDK API at all,
# only a plain public Gradle dependency (android/app/build.gradle.kts, gated
# on this same NX_MODULE_STORE_APP_GALLERY flag), the AGConnect Gradle
# plugin (reads agconnect-services.json, also gated there), and a
# hand-written Kotlin JNI shim
# (modules/store_app_gallery/android/java/com/nx2d/runtime/NxHuaweiIap.kt,
# picked up by that same build script's per-module source-dir loop). This
# CMakeLists.txt only builds the C++ side of the JNI boundary.
nx_module(store_app_gallery
        DISPLAY "Huawei App Gallery (HMS IAP Kit) storefront backend"
        SOURCES store_app_gallery_module.cpp store_app_gallery_platform.cpp
                store_app_gallery_services.cpp
        DEPENDS nx_module_store nx::sdl
        PLATFORMS android)
