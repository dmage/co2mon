project(libco2mon)
cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
# hidapi-libusb - Ubuntu 14.04 (trusty)
# hidapi        - homebrew on OS X 10.10 (Yosemite)
pkg_search_module(HIDAPI REQUIRED hidapi-libusb hidapi)

include_directories(
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${HIDAPI_INCLUDE_DIRS})

link_directories(
    ${HIDAPI_LIBRARY_DIRS})

include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${HIDAPI_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${HIDAPI_LIBRARIES})
check_symbol_exists(libusb_strerror "libusb.h" HAVE_HIDAPI_STRERROR)
set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)

aux_source_directory(src SRC_LIST)
add_library(co2mon ${SRC_LIST})
target_link_libraries(co2mon
    ${HIDAPI_LIBRARIES})

install(TARGETS co2mon
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
