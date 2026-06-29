cmake_minimum_required(VERSION 3.16)
project(poker_solver CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Header-only game model + (upcoming) solver.
add_library(poker_solver INTERFACE)
target_include_directories(poker_solver INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(poker_solver INTERFACE cxx_std_20)

# Demo / solver entry point.
add_executable(poker_solver_app src/main.cpp)
target_link_libraries(poker_solver_app PRIVATE poker_solver)

# Tests.
enable_testing()
add_executable(test_kuhn tests/test_kuhn.cpp)
target_link_libraries(test_kuhn PRIVATE poker_solver)
add_test(NAME test_kuhn COMMAND test_kuhn)

add_executable(test_cfr tests/test_cfr.cpp)
target_link_libraries(test_cfr PRIVATE poker_solver)
add_test(NAME test_cfr COMMAND test_cfr)
