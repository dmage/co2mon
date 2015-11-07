project(co2mon)
cmake_minimum_required(VERSION 2.8)

# Known configuration points:
#
#   -DBUILD_SHARED_LIBS=OFF
#   -DCMAKE_INSTALL_BINDIR=bin
#   -DCMAKE_INSTALL_LIBDIR=lib
#
# More variables you may find at https://cmake.org/Wiki/CMake_Useful_Variables

include(GNUInstallDirs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra")

add_subdirectory(libco2mon)
add_subdirectory(co2mond)
