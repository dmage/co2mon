project(libco2mon)
cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

include_directories(
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${LIBUSB_INCLUDE_DIRS})

link_directories(
    ${LIBUSB_LIBRARY_DIRS})

include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${LIBUSB_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${LIBUSB_LIBRARIES})
check_symbol_exists(libusb_strerror "libusb.h" HAVE_LIBUSB_STRERROR)
set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)

aux_source_directory(src SRC_LIST)
add_library(co2mon ${SRC_LIST})
target_link_libraries(co2mon
    ${LIBUSB_LIBRARIES})
