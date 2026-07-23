cmake_minimum_required(VERSION 3.22)

# Delegate to the native/ CMakeLists at the repository root.
# This file exists so Gradle can find it via externalNativeBuild in app/build.gradle.
add_subdirectory(${CMAKE_SOURCE_DIR}/../native native_build)