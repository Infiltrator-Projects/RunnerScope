cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" RUNNERSCOPE_VERSION)
string(STRIP "${RUNNERSCOPE_VERSION}" RUNNERSCOPE_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/VERSION")
project(RunnerScope VERSION "${RUNNERSCOPE_VERSION}" LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
include(cmake/GitHubSignIn.cmake)

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

add_library(runnerscope_core STATIC
    src/core/runner_config.c
    src/core/github.c
    src/core/github_auth.c
    third_party/cjson/cJSON.c
)
target_include_directories(runnerscope_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(runnerscope_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson)
target_compile_definitions(runnerscope_core PRIVATE CJSON_HIDE_SYMBOLS CJSON_NESTING_LIMIT=128)
target_link_libraries(runnerscope_core PUBLIC InfiltratrCommon::Common)
if(MSVC)
    target_compile_options(runnerscope_core PRIVATE /W4 /utf-8)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runnerscope_core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
    )
endif()

add_library(runnerscope_ui_contract STATIC
    src/ui/ui_contract.c
    src/ui/ui_renderer.c
)
target_include_directories(runnerscope_ui_contract PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(runnerscope_ui_contract PRIVATE /W4 /utf-8)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runnerscope_ui_contract PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
    )
endif()

include("${RUNNERSCOPE_COMMON_DIR}/cmake/InfiltratrTypographyAssets.cmake")
set(RUNNERSCOPE_FONT_ARCHIVE "${CMAKE_BINARY_DIR}/mb-corpo-fonts.tar.xz")
if(NOT EXISTS "${RUNNERSCOPE_FONT_ARCHIVE}")
    file(DOWNLOAD
        "${INFILTRATR_MB_CORPO_ARCHIVE_URL}"
        "${RUNNERSCOPE_FONT_ARCHIVE}"
        EXPECTED_HASH "SHA256=${INFILTRATR_MB_CORPO_ARCHIVE_SHA256}"
        TLS_VERIFY ON
        STATUS RUNNERSCOPE_FONT_DOWNLOAD_STATUS)
    list(GET RUNNERSCOPE_FONT_DOWNLOAD_STATUS 0 RUNNERSCOPE_FONT_DOWNLOAD_CODE)
    if(NOT RUNNERSCOPE_FONT_DOWNLOAD_CODE EQUAL 0)
        list(GET RUNNERSCOPE_FONT_DOWNLOAD_STATUS 1 RUNNERSCOPE_FONT_DOWNLOAD_MESSAGE)
        message(FATAL_ERROR "Unable to obtain mandatory MB Corpo assets: ${RUNNERSCOPE_FONT_DOWNLOAD_MESSAGE}")
    endif()
endif()
file(SHA256 "${RUNNERSCOPE_FONT_ARCHIVE}" RUNNERSCOPE_FONT_ARCHIVE_ACTUAL)
if(NOT RUNNERSCOPE_FONT_ARCHIVE_ACTUAL STREQUAL INFILTRATR_MB_CORPO_ARCHIVE_SHA256)
    message(FATAL_ERROR "Mandatory MB Corpo archive failed Common hash verification")
endif()
set(RUNNERSCOPE_FONT_DIR "${CMAKE_BINARY_DIR}/mb-corpo-fonts")
file(REMOVE_RECURSE "${RUNNERSCOPE_FONT_DIR}")
file(MAKE_DIRECTORY "${RUNNERSCOPE_FONT_DIR}")
file(ARCHIVE_EXTRACT INPUT "${RUNNERSCOPE_FONT_ARCHIVE}" DESTINATION "${RUNNERSCOPE_FONT_DIR}")
foreach(RUNNERSCOPE_FONT_ROLE BRAND_REGULAR UI_BOLD UI_REGULAR)
    set(RUNNERSCOPE_FONT_FILE_VAR "INFILTRATR_MB_CORPO_${RUNNERSCOPE_FONT_ROLE}_FILE")
    set(RUNNERSCOPE_FONT_SHA_VAR "INFILTRATR_MB_CORPO_${RUNNERSCOPE_FONT_ROLE}_SHA256")
    set(RUNNERSCOPE_FONT_PATH "${RUNNERSCOPE_FONT_DIR}/${${RUNNERSCOPE_FONT_FILE_VAR}}")
    if(NOT EXISTS "${RUNNERSCOPE_FONT_PATH}")
        message(FATAL_ERROR "Mandatory MB Corpo face missing: ${${RUNNERSCOPE_FONT_FILE_VAR}}")
    endif()
    file(SHA256 "${RUNNERSCOPE_FONT_PATH}" RUNNERSCOPE_FONT_ACTUAL_SHA)
    if(NOT RUNNERSCOPE_FONT_ACTUAL_SHA STREQUAL "${${RUNNERSCOPE_FONT_SHA_VAR}}")
        message(FATAL_ERROR "Mandatory MB Corpo face failed Common hash verification: ${${RUNNERSCOPE_FONT_FILE_VAR}}")
    endif()
endforeach()

if(WIN32)
    enable_language(RC)

    add_library(runnerscope_windows_platform STATIC
        src/platform/windows_config_store.c
        src/platform/windows_github_sign_in.c
        src/platform/windows_http.c
    )
    target_include_directories(runnerscope_windows_platform PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(runnerscope_windows_platform PUBLIC runnerscope_core winhttp advapi32 bcrypt)
    target_compile_definitions(runnerscope_windows_platform PRIVATE RUNNERSCOPE_GH_VERSION="${RUNNERSCOPE_GH_VERSION}")
    if(MSVC)
        target_compile_options(runnerscope_windows_platform PRIVATE /W4 /utf-8)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(runnerscope_windows_platform PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
        )
    endif()

    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" RUNNERSCOPE_SOURCE_RC)
    file(TO_CMAKE_PATH "${RUNNERSCOPE_FONT_DIR}" RUNNERSCOPE_FONT_RC)
    set(RUNNERSCOPE_WINDOWS_RC "${CMAKE_CURRENT_BINARY_DIR}/runnerscope_windows.rc")
    file(WRITE "${RUNNERSCOPE_WINDOWS_RC}"
        "2001 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/nav-runners.png\"\n"
        "2002 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/nav-active.png\"\n"
        "2003 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/nav-history.png\"\n"
        "2004 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/nav-health.png\"\n"
        "2005 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/nav-infiltrator.png\"\n"
        "2011 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/hero-runners.png\"\n"
        "2012 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/hero-active.png\"\n"
        "2013 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/hero-history.png\"\n"
        "2014 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/assets/art/hero-health.png\"\n"
        "2101 RCDATA \"${RUNNERSCOPE_FONT_RC}/${INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE}\"\n"
        "2102 RCDATA \"${RUNNERSCOPE_FONT_RC}/${INFILTRATR_MB_CORPO_UI_BOLD_FILE}\"\n"
        "2103 RCDATA \"${RUNNERSCOPE_FONT_RC}/${INFILTRATR_MB_CORPO_UI_REGULAR_FILE}\"\n"
        "2201 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/third_party/cjson/LICENSE\"\n")

    file(APPEND "${RUNNERSCOPE_WINDOWS_RC}" "2301 RCDATA \"${RUNNERSCOPE_GH_BINARY}\"\n")

    file(APPEND "${RUNNERSCOPE_WINDOWS_RC}" "2302 RCDATA \"${RUNNERSCOPE_SOURCE_RC}/third_party/github-cli/LICENSE\"\n")

    add_executable(runnerscope WIN32
        src/windows_main.c
        src/windows_resources.h
        src/windows/windows_foundation.inc
        src/windows/windows_runner.inc
        src/windows/windows_layout.inc
        src/windows/windows_presentation.inc
        src/windows/windows_auth.inc
        src/windows/windows_actions.inc
        src/windows/windows_controller.inc
        "${RUNNERSCOPE_WINDOWS_RC}"
    )
    set_target_properties(runnerscope PROPERTIES OUTPUT_NAME "RunnerScope")
    target_link_libraries(runnerscope PRIVATE
        runnerscope_core
        runnerscope_windows_platform
        runnerscope_ui_contract
        InfiltratrCommon::Common
        user32
        gdi32
        comctl32
        comdlg32
        advapi32
        shell32
        shlwapi
        ole32
        oleaut32
        windowscodecs
        msimg32
        dwmapi
        uxtheme
        uuid
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
    find_package(CURL 7.85 REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(runnerscope_core PUBLIC Threads::Threads)
    add_library(runnerscope_linux_platform STATIC
        src/platform/linux_http.c
        src/platform/linux_services.c
        src/platform/linux_github_sign_in.c)
    target_compile_definitions(runnerscope_linux_platform PRIVATE
        RUNNERSCOPE_GH_VERSION="${RUNNERSCOPE_GH_VERSION}"
        RUNNERSCOPE_BUILD_ROOT="${CMAKE_BINARY_DIR}"
        RUNNERSCOPE_GH_BUILD_EXECUTABLE="${RUNNERSCOPE_GH_BINARY}"
        RUNNERSCOPE_GH_EXECUTABLE="${CMAKE_INSTALL_FULL_LIBEXECDIR}/runnerscope/gh")
    target_include_directories(runnerscope_linux_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(runnerscope_linux_platform PUBLIC
        runnerscope_core PkgConfig::GTK3 CURL::libcurl Threads::Threads)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope_linux_platform PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
            -Wconversion -Wsign-conversion)
    endif()

    add_executable(runnerscope
        src/linux_main.c
        src/linux_auth.inc
    )
    target_link_libraries(runnerscope PRIVATE
        runnerscope_core
        runnerscope_linux_platform
        runnerscope_ui_contract
        InfiltratrCommon::Common
        PkgConfig::GTK3
    )
    target_compile_definitions(runnerscope PRIVATE
        RUNNERSCOPE_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/runnerscope"
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
            -Wconversion -Wsign-conversion
        )
    endif()

    install(TARGETS runnerscope RUNTIME DESTINATION bin)
    install(PROGRAMS "${RUNNERSCOPE_GH_BINARY}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/runnerscope")
    install(FILES third_party/github-cli/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/runnerscope" RENAME GitHub-CLI-LICENSE)
    install(FILES packaging/runnerscope.desktop DESTINATION share/applications)
    install(FILES packaging/runnerscope.svg
            DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES packaging/runnerscope.png
            DESTINATION share/icons/hicolor/128x128/apps)
    install(DIRECTORY assets/art/
            DESTINATION "${CMAKE_INSTALL_DATADIR}/runnerscope/art"
            FILES_MATCHING PATTERN "*.png")
    install(FILES
        "${RUNNERSCOPE_FONT_DIR}/${INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE}"
        "${RUNNERSCOPE_FONT_DIR}/${INFILTRATR_MB_CORPO_UI_BOLD_FILE}"
        "${RUNNERSCOPE_FONT_DIR}/${INFILTRATR_MB_CORPO_UI_REGULAR_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fonts/truetype/runnerscope")
    install(FILES third_party/cjson/LICENSE
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/runnerscope"
        RENAME cJSON-LICENSE)
endif()

target_compile_definitions(runnerscope PRIVATE
    RUNNERSCOPE_VERSION="${PROJECT_VERSION}"
)

add_test(NAME runnerscope-native-self-test COMMAND runnerscope --self-test)

add_executable(runnerscope-config-test
    tests/runner_config_test.c
)
target_link_libraries(runnerscope-config-test PRIVATE runnerscope_core)
add_test(NAME runnerscope-config-test COMMAND runnerscope-config-test)

add_executable(runnerscope-github-test tests/github_test.c)
target_link_libraries(runnerscope-github-test PRIVATE runnerscope_core)
add_test(NAME runnerscope-github-test COMMAND runnerscope-github-test)

add_executable(runnerscope-ui-contract-test
    tests/ui_contract_test.c
)
target_link_libraries(runnerscope-ui-contract-test PRIVATE runnerscope_ui_contract)
add_test(NAME runnerscope-ui-contract-test COMMAND runnerscope-ui-contract-test)

add_executable(runnerscope-ui-failure-semantics-test
    tests/ui_failure_semantics_test.c
)
target_link_libraries(runnerscope-ui-failure-semantics-test PRIVATE runnerscope_ui_contract)
add_test(NAME runnerscope-ui-failure-semantics-test COMMAND runnerscope-ui-failure-semantics-test)

if(WIN32)
    add_executable(runnerscope-windows-config-store-test
        tests/windows_config_store_test.c
    )
    target_link_libraries(runnerscope-windows-config-store-test PRIVATE runnerscope_windows_platform)
    add_test(NAME runnerscope-windows-config-store-test COMMAND runnerscope-windows-config-store-test)
else()
    add_executable(runnerscope-linux-services-test tests/linux_services_test.c)
    target_link_libraries(runnerscope-linux-services-test PRIVATE runnerscope_linux_platform)
    add_test(NAME runnerscope-linux-services-test COMMAND runnerscope-linux-services-test)
endif()

add_executable(runnerscope-github-auth-test tests/github_auth_test.c)
target_link_libraries(runnerscope-github-auth-test PRIVATE runnerscope_core)
add_test(NAME runnerscope-github-auth-test COMMAND runnerscope-github-auth-test)

add_executable(runnerscope-http-policy-test tests/http_policy_test.c)
if(WIN32)
    target_link_libraries(runnerscope-http-policy-test PRIVATE runnerscope_windows_platform)
else()
    target_link_libraries(runnerscope-http-policy-test PRIVATE runnerscope_linux_platform)
endif()
add_test(NAME runnerscope-http-policy-test COMMAND runnerscope-http-policy-test)

add_executable(runnerscope-github-auth-fixture tests/github_auth_fixture.c)
add_executable(runnerscope-github-auth-process-test tests/github_auth_process_test.c)
if(WIN32)
    target_link_libraries(runnerscope-github-auth-process-test PRIVATE runnerscope_windows_platform)
else()
    target_link_libraries(runnerscope-github-auth-process-test PRIVATE runnerscope_linux_platform)
endif()
add_test(NAME runnerscope-github-auth-process-test COMMAND runnerscope-github-auth-process-test $<TARGET_FILE:runnerscope-github-auth-fixture>)
set_tests_properties(runnerscope-github-auth-process-test PROPERTIES TIMEOUT 15)
