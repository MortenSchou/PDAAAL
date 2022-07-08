cmake_minimum_required(VERSION 3.15...3.22)

find_package(Python 3.6 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(py-pdaaal EXCLUDE_FROM_ALL py-pdaaal.cpp)

target_link_libraries(py-pdaaal PRIVATE pdaaal::pdaaal)
set_target_properties(py-pdaaal PROPERTIES OUTPUT_NAME pdaaal)
