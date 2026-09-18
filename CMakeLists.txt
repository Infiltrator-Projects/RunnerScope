cmake_minimum_required(VERSION 3.20)
project(RunnerScope VERSION 1.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(RUNNERSCOPE_COMMON_VERSION "1.19.2")
set(RUNNERSCOPE_COMMON_COMMIT "44409af17c89b6ece6b4bcb2c0c133213c695c23")
set(RUNNERSCOPE_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/infiltratr-common")

if(NOT EXISTS "${RUNNERSCOPE_COMMON_DIR}/VERSION")
    message(FATAL_ERROR "Infiltratr Common is missing. Run: git submodule update --init --recursive")
endif()
file(READ "${RUNNERSCOPE_COMMON_DIR}/VERSION" RUNNERSCOPE_COMMON_ACTUAL_VERSION)
string(STRIP "${RUNNERSCOPE_COMMON_ACTUAL_VERSION}" RUNNERSCOPE_COMMON_ACTUAL_VERSION)
if(NOT RUNNERSCOPE_COMMON_ACTUAL_VERSION STREQUAL RUNNERSCOPE_COMMON_VERSION)
    message(FATAL_ERROR "RunnerScope requires Infiltratr Common ${RUNNERSCOPE_COMMON_VERSION}; found ${RUNNERSCOPE_COMMON_ACTUAL_VERSION}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(infiltratr-common EXCLUDE_FROM_ALL)

add_executable(runnerscope src/runnerscope.c)
target_link_libraries(runnerscope PRIVATE InfiltratrCommon::Common PkgConfig::GTK3)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runnerscope PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wformat=2 -Wconversion -Wsign-conversion)
endif()

enable_testing()
add_test(NAME runnerscope-self-test COMMAND runnerscope --self-test)

install(TARGETS runnerscope RUNTIME DESTINATION bin)
install(FILES packaging/runnerscope.desktop DESTINATION share/applications)
