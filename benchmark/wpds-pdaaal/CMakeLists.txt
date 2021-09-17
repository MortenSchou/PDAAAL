project(WPDS-PDAAAL LANGUAGES CXX)

add_library(wpds-pdaaal INTERFACE) # Provide some of the same interface as PDAAAL, but use WPDS++ internally
add_library(wpds-pdaaal::wpds-pdaaal ALIAS wpds-pdaaal)

target_link_libraries(wpds-pdaaal INTERFACE pdaaal::pdaaal wpds::wpds-lib)
target_include_directories(wpds-pdaaal INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/wpds-pdaaal/*.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wpds-pdaaal)

# More or less boilerplate stuff that ensures everything is installed correctly
install(TARGETS  wpds-pdaaal
        EXPORT   wpds-pdaaal-targets
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT      wpds-pdaaal-targets
        FILE        wpds-pdaaal-targets.cmake
        NAMESPACE   wpds-pdaaal::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wpds-pdaaal)
