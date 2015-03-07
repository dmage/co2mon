project(co2mond)
cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.40)
pkg_check_modules(GIO2 REQUIRED gio-2.0)

include_directories(
    ../libco2mon/include
    ${LIBUSB_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${GIO2_INCLUDE_DIRS})

link_directories(
    ${LIBUSB_LIBRARY_DIRS}
    ${GLIB2_LIBRARY_DIRS}
    ${GIO2_LIBRARY_DIRS})

aux_source_directory(src SRC_LIST)
add_executable(co2mond ${SRC_LIST})
target_link_libraries(co2mond
    co2mon
    ${GLIB2_LIBRARIES}
    ${GIO2_LIBRARIES})
