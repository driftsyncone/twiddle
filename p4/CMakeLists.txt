# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Release image for the web flasher: no Wi-Fi, token, web password or host of the builder is compiled in
# (main/build_config.h then uses main/secrets.release.h and ignores main/secrets.h). Check the result with
# tools/check-release-image.sh. Usage: idf.py -DTWIDDLE_RELEASE=ON build   (change it => idf.py reconfigure).
option(TWIDDLE_RELEASE "Build a release image without any builder secrets" OFF)
if(TWIDDLE_RELEASE)
  idf_build_set_property(COMPILE_DEFINITIONS "-DTWIDDLE_RELEASE=1" APPEND)
  message(STATUS "TWIDDLE_RELEASE: secrets.h is ignored")
endif()

project(twiddle)
