
add_subdirectory(wpds)
add_subdirectory(wpds-pdaaal)

add_executable(generate)
target_sources(generate PRIVATE Generate.cpp)
target_link_libraries(generate PRIVATE pdaaal::pdaaal Boost::program_options)

add_executable(test-before-isabelle)
target_sources(test-before-isabelle PRIVATE TestBeforeIsabelle.cpp)
target_link_libraries(test-before-isabelle PRIVATE pdaaal::pdaaal Boost::program_options)

add_executable(delta-debug)
target_sources(delta-debug PRIVATE DeltaDebug.cpp)
target_link_libraries(delta-debug PRIVATE nlohmann_json::nlohmann_json Boost::program_options)
