
add_executable(generate)

target_sources(generate PRIVATE Generate.cpp)
target_link_libraries(generate PRIVATE pdaaal::pdaaal Boost::program_options)
