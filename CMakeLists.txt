cmake_minimum_required(VERSION 3.16)

project(homework-1 CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER clang++-20)
set(CMAKE_C_COMPILER clang-20)

add_executable(cpu-benchmark
    main.cpp
    l1_cache_calc.cpp
)

add_executable(cpu-benchmark-optimized
    main.cpp
    l1_cache_calc.cpp
)

target_compile_options(cpu-benchmark PRIVATE -Wall -Wextra -O0)
target_compile_options(cpu-benchmark-optimized PRIVATE -Wall -Wextra -O2)
