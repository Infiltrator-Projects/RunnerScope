cmake_minimum_required(VERSION 3.20)
project(RunnerMonitor VERSION 1.1.5 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(RUNNERSCOPE_COMMON_VERSION "1.19.6")
set(RUNNERSCOPE_COMMON_COMMIT "a0e26896cc2a5674a138b29f009a7b30f0d636c6")
set(RUNNERSCOPE_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/infiltratr-common")

if(NOT EXISTS "${RUNNERSCOPE_COMMON_DIR}/VERSION")
    message(FATAL_ERROR "Infiltratr Common is missing. Run: git submodule update --init --recursive")
endif()
file(READ "${RUNNERSCOPE_COMMON_DIR}/VERSION" RUNNERSCOPE_COMMON_ACTUAL_VERSION)
string(STRIP "${RUNNERSCOPE_COMMON_ACTUAL_VERSION}" RUNNERSCOPE_COMMON_ACTUAL_VERSION)
if(NOT RUNNERSCOPE_COMMON_ACTUAL_VERSION STREQUAL RUNNERSCOPE_COMMON_VERSION)
    message(FATAL_ERROR "Runner Monitor requires Common ${RUNNERSCOPE_COMMON_VERSION}; found ${RUNNERSCOPE_COMMON_ACTUAL_VERSION}")
endif()

set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(infiltratr-common EXCLUDE_FROM_ALL)

add_executable(runnerscope-native native/runnerscope_native.c)
target_link_libraries(runnerscope-native PRIVATE InfiltratrCommon::Common)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runnerscope-native PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME runnerscope-native-self-test COMMAND runnerscope-native --self-test)

install(TARGETS runnerscope-native RUNTIME DESTINATION lib/runnerscope)


# First-party native rewrite: Common plus libc/POSIX only.
add_executable(runnerscope-firstparty-core
    src/native2/main.c
    src/native2/model.c
    src/native2/http.c
    src/native2/secure_config.c
    src/native2/linux_local.c)
target_include_directories(runnerscope-firstparty-core PRIVATE src/native2)
target_link_libraries(runnerscope-firstparty-core PRIVATE InfiltratrCommon::Common m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runnerscope-firstparty-core PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2)
endif()
add_executable(runnerscope-firstparty-contract
    tests/first_party_core.c
    src/native2/model.c
    src/native2/http.c)
target_include_directories(runnerscope-firstparty-contract PRIVATE src/native2)
target_link_libraries(runnerscope-firstparty-contract PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runnerscope-firstparty-contract PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2)
endif()
add_test(NAME runnerscope-firstparty-core-self-test
         COMMAND runnerscope-firstparty-core --self-test)
add_test(NAME runnerscope-firstparty-contract
         COMMAND runnerscope-firstparty-contract)
