project(libco2mon)
cmake_minimum_required(VERSION 3.0)

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
set_target_properties(co2mon PROPERTIES
    SOVERSION 1)

install(TARGETS co2mon
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/co2mon.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(test_co2mon tests/test_co2mon.c)
target_link_libraries(test_co2mon PRIVATE co2mon)
add_test(NAME test_co2mon COMMAND $<TARGET_FILE:test_co2mon>)
