cmake_minimum_required(VERSION 3.14)
project(WPDS C CXX)

# This CMakeLists fetches wpds++ from the internet, and provides an interface target for using it.

# Fetch and include WPDS++ library
include(ExternalProject)
ExternalProject_add(wpds
    URL https://research.cs.wisc.edu/wpis/wpds/wpds++/wpds++-latest.tar.gz
    URL_HASH SHA512=ebacc3b4f8e381d995befe83c87cc0fccbd5a27fe332e72b08e00259267609673e73724f9028774bae1bf2bee723b66736647968b2960b9f30fb3711dc2ccfad
    BUILD_COMMAND make
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_property(wpds SOURCE_DIR)
add_library(wpds-lib INTERFACE)
add_dependencies(wpds-lib wpds)
add_library(wpds::wpds ALIAS wpds-lib)

include(GNUInstallDirs)
target_link_libraries(wpds-lib INTERFACE
    $<BUILD_INTERFACE:${SOURCE_DIR}/libwpds++.a>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}/libwpds++.a>
)
target_include_directories(wpds-lib INTERFACE
    $<BUILD_INTERFACE:${SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_options(wpds-lib INTERFACE -fpermissive) # WPDS contains a void* to int cast, which is an error without -fpermissive

install(FILES ${SOURCE_DIR}/*.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wpds)
install(FILES ${SOURCE_DIR}/libwpds++.a
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
