project(co2mond)
cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_search_module(HIDAPI REQUIRED hidapi-libusb hidapi)

include_directories(
    ../libco2mon/include
    ${HIDAPI_INCLUDE_DIRS})

link_directories(${HIDAPI_LIBRARY_DIRS})

aux_source_directory(src SRC_LIST)
add_executable(co2mond ${SRC_LIST})
target_link_libraries(co2mond
    co2mon
    ${HIDAPI_LIBRARIES})
