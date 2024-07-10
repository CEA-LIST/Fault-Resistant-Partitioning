cmake_minimum_required(VERSION 3.25)
project(verifier)
set(CMAKE_CXX_STANDARD 20)
set(PROJECT_TEMPORARY_DIR ${PROJECT_SOURCE_DIR}/tmp)
add_subdirectory(cxxsat)

add_library(libsim Cell.cpp cell_types.cpp Circuit.cpp)
target_include_directories(libsim PUBLIC "${PROJECT_SOURCE_DIR}/json/single_include/nlohmann")

add_executable(k-partitions k-partitions.cpp utils.cpp config.cpp)

target_link_libraries(k-partitions cxxsat)
add_dependencies(k-partitions cadical)
target_include_directories(k-partitions PRIVATE cxxsat)

target_link_libraries(k-partitions libsim)
