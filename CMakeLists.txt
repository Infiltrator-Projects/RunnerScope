cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" RUNNERSCOPE_VERSION)
string(STRIP "${RUNNERSCOPE_VERSION}" RUNNERSCOPE_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/VERSION")
if(NOT RUNNERSCOPE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain major.minor.patch")
endif()
project(RunnerScope VERSION "${RUNNERSCOPE_VERSION}" LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
option(RUNNERSCOPE_BUILD_DESKTOP "Build the native desktop application and platform adapters" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(RUNNERSCOPE_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/infiltratr-common")
if(NOT EXISTS "${RUNNERSCOPE_COMMON_DIR}/VERSION" OR
   NOT EXISTS "${RUNNERSCOPE_COMMON_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Libraries is missing. Run: git submodule update --init --recursive")
endif()

file(READ "${RUNNERSCOPE_COMMON_DIR}/VERSION" RUNNERSCOPE_COMMON_VERSION)
string(STRIP "${RUNNERSCOPE_COMMON_VERSION}" RUNNERSCOPE_COMMON_VERSION)
if(RUNNERSCOPE_COMMON_VERSION STREQUAL "")
    message(FATAL_ERROR "Libraries VERSION is empty")
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND
   EXISTS "${RUNNERSCOPE_COMMON_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                rev-parse "HEAD:infiltratr-common"
        RESULT_VARIABLE RUNNERSCOPE_COMMON_EXPECTED_RESULT
        OUTPUT_VARIABLE RUNNERSCOPE_COMMON_EXPECTED_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${RUNNERSCOPE_COMMON_DIR}" rev-parse HEAD
        RESULT_VARIABLE RUNNERSCOPE_COMMON_ACTUAL_RESULT
        OUTPUT_VARIABLE RUNNERSCOPE_COMMON_ACTUAL_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT RUNNERSCOPE_COMMON_EXPECTED_RESULT EQUAL 0 OR
       NOT RUNNERSCOPE_COMMON_ACTUAL_RESULT EQUAL 0 OR
       RUNNERSCOPE_COMMON_EXPECTED_COMMIT STREQUAL "" OR
       NOT RUNNERSCOPE_COMMON_ACTUAL_COMMIT STREQUAL RUNNERSCOPE_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Libraries checkout does not match this revision's gitlink: "
            "expected ${RUNNERSCOPE_COMMON_EXPECTED_COMMIT}, "
            "found ${RUNNERSCOPE_COMMON_ACTUAL_COMMIT}")
    endif()
endif()
message(STATUS "RunnerScope Common: ${RUNNERSCOPE_COMMON_VERSION}")

set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(infiltratr-common EXCLUDE_FROM_ALL)

function(runnerscope_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wformat=2)
    endif()
endfunction()

add_library(runnerscope_core STATIC
    src/core/runner_config.c
    src/core/github.c
    src/core/json.c
    src/core/github_auth.c
    third_party/cjson/cJSON.c
)
target_include_directories(runnerscope_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(runnerscope_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson)
target_compile_definitions(runnerscope_core PRIVATE CJSON_HIDE_SYMBOLS CJSON_NESTING_LIMIT=128)
target_link_libraries(runnerscope_core PUBLIC InfiltratrCommon::Common)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(runnerscope_core PUBLIC Threads::Threads)
endif()
runnerscope_warnings(runnerscope_core)

add_library(runnerscope_ui_contract STATIC
    src/ui/ui_contract.c
    src/ui/ui_renderer.c
)
target_include_directories(runnerscope_ui_contract PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
runnerscope_warnings(runnerscope_ui_contract)

if(RUNNERSCOPE_BUILD_DESKTOP)
    include(cmake/NativeDesktop.cmake)
endif()

if(BUILD_TESTING)
    include(cmake/Tests.cmake)
endif()
