cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" RUNNERSCOPE_VERSION)
string(STRIP "${RUNNERSCOPE_VERSION}" RUNNERSCOPE_VERSION)
project(RunnerMonitor VERSION "${RUNNERSCOPE_VERSION}" LANGUAGES C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(RUNNERSCOPE_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/infiltratr-common")
set(RUNNERSCOPE_COMMON_EXPECTED_VERSION "1.19.35")
set(RUNNERSCOPE_COMMON_EXPECTED_COMMIT "7cc5de3de0e94ed2cfcff0840bbb5346eb5c9c9f")
if(NOT EXISTS "${RUNNERSCOPE_COMMON_DIR}/VERSION")
    message(FATAL_ERROR "Infiltratr Common is missing. Run: git submodule update --init --recursive")
endif()
file(READ "${RUNNERSCOPE_COMMON_DIR}/VERSION" RUNNERSCOPE_COMMON_VERSION)
string(STRIP "${RUNNERSCOPE_COMMON_VERSION}" RUNNERSCOPE_COMMON_VERSION)
if(NOT RUNNERSCOPE_COMMON_VERSION STREQUAL RUNNERSCOPE_COMMON_EXPECTED_VERSION)
    message(FATAL_ERROR
        "Runner Monitor requires Common ${RUNNERSCOPE_COMMON_EXPECTED_VERSION}; "
        "found ${RUNNERSCOPE_COMMON_VERSION}")
endif()
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${RUNNERSCOPE_COMMON_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${RUNNERSCOPE_COMMON_DIR}" rev-parse HEAD
        RESULT_VARIABLE RUNNERSCOPE_COMMON_GIT_RESULT
        OUTPUT_VARIABLE RUNNERSCOPE_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT RUNNERSCOPE_COMMON_GIT_RESULT EQUAL 0 OR
       NOT RUNNERSCOPE_COMMON_COMMIT STREQUAL RUNNERSCOPE_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Runner Monitor requires Common commit "
            "${RUNNERSCOPE_COMMON_EXPECTED_COMMIT}; found "
            "${RUNNERSCOPE_COMMON_COMMIT}")
    endif()
endif()

set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(infiltratr-common EXCLUDE_FROM_ALL)

if(WIN32)
    add_executable(runnerscope WIN32
        src/windows_main.c
    )
    set_target_properties(runnerscope PROPERTIES OUTPUT_NAME "Runner-Monitor")
    target_link_libraries(runnerscope PRIVATE
        InfiltratrCommon::Common
        user32
        gdi32
        comctl32
        advapi32
        shell32
    )
    if(MSVC)
        target_compile_options(runnerscope PRIVATE /W4 /utf-8)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(runnerscope PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
        )
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

    add_executable(runnerscope
        src/linux_main.c
    )
    target_link_libraries(runnerscope PRIVATE
        InfiltratrCommon::Common
        PkgConfig::GTK3
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
            -Wconversion -Wsign-conversion
        )
    endif()

    install(TARGETS runnerscope RUNTIME DESTINATION bin)
    install(FILES packaging/runnerscope.desktop DESTINATION share/applications)
    install(FILES packaging/runnerscope.svg
            DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES packaging/runnerscope.png
            DESTINATION share/icons/hicolor/128x128/apps)
endif()

target_compile_definitions(runnerscope PRIVATE
    RUNNERSCOPE_VERSION="${PROJECT_VERSION}"
)

add_test(NAME runnerscope-native-self-test COMMAND runnerscope --self-test)

# The dependency-minimisation core remains a qualification target, not the
# shipping application. It can progressively replace platform/helper
# dependencies without ever making Python/Tk the product again.
if(UNIX)
    add_executable(runnerscope-firstparty-core
        src/native2/main.c
        src/native2/model.c
        src/native2/http.c
        src/native2/secure_config.c
        src/native2/linux_local.c
    )
    target_include_directories(runnerscope-firstparty-core PRIVATE src/native2)
    target_link_libraries(runnerscope-firstparty-core PRIVATE
        InfiltratrCommon::Common
        m
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope-firstparty-core PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
        )
    endif()

    add_executable(runnerscope-firstparty-contract
        tests/first_party_core.c
        src/native2/model.c
        src/native2/http.c
    )
    target_include_directories(runnerscope-firstparty-contract PRIVATE src/native2)
    target_link_libraries(runnerscope-firstparty-contract PRIVATE m)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope-firstparty-contract PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
        )
    endif()

    add_test(NAME runnerscope-firstparty-core-self-test
             COMMAND runnerscope-firstparty-core --self-test)
    add_test(NAME runnerscope-firstparty-contract
             COMMAND runnerscope-firstparty-contract)
endif()
