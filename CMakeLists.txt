cmake_minimum_required(VERSION 3.20)
project(nes_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(nes_core
    src/cpu.cpp
    src/bus.cpp
)
target_include_directories(nes_core PUBLIC include)

add_executable(nes_emulator
    src/main.cpp
)
target_link_libraries(nes_emulator PRIVATE nes_core)

add_executable(cpu_tests
    tests/test_cpu.cpp
    tests/test_framework.cpp
)
target_include_directories(cpu_tests PRIVATE include tests)
target_link_libraries(cpu_tests PRIVATE nes_core)

add_custom_target(check COMMAND cpu_tests)

