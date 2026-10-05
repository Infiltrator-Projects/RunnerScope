cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" RUNNERSCOPE_VERSION)
string(STRIP "${RUNNERSCOPE_VERSION}" RUNNERSCOPE_VERSION)
project(RunnerMonitor VERSION "${RUNNERSCOPE_VERSION}" LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(RUNNERSCOPE_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/infiltratr-common")
set(RUNNERSCOPE_COMMON_EXPECTED_VERSION "1.19.38")
set(RUNNERSCOPE_COMMON_EXPECTED_COMMIT "7070c5812b50821fd7580101cb2289a3184f6b2c")
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

# Runner Monitor is one visual product on both native platforms.  The same
# Common-verified MB faces are therefore mandatory on Windows and Linux.
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

    # Keep the Windows release as a single native executable while shipping the
    # exact bitmap identity and MB faces used by the Linux shell.  The Win32
    # frontend loads these RCDATA resources directly at runtime.
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
        "2103 RCDATA \"${RUNNERSCOPE_FONT_RC}/${INFILTRATR_MB_CORPO_UI_REGULAR_FILE}\"\n")

    add_executable(runnerscope WIN32
        src/windows_main.c
        src/windows_resources.h
        "${RUNNERSCOPE_WINDOWS_RC}"
    )
    set_target_properties(runnerscope PROPERTIES OUTPUT_NAME "Runner-Monitor")
    target_link_libraries(runnerscope PRIVATE
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

    add_executable(runnerscope
        src/linux_main.c
    )
    target_link_libraries(runnerscope PRIVATE
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
endif()

target_compile_definitions(runnerscope PRIVATE
    RUNNERSCOPE_VERSION="${PROJECT_VERSION}"
)

add_test(NAME runnerscope-native-self-test COMMAND runnerscope --self-test)
