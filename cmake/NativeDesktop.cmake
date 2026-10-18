# Platform mechanics and install layout for the shipping native applications.
include(cmake/RuntimeAssets.cmake)

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
    runnerscope_warnings(runnerscope_windows_platform)

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
    runnerscope_warnings(runnerscope)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
    find_package(CURL 7.85 REQUIRED)
    add_library(runnerscope_linux_platform STATIC
        src/platform/linux_http.c
        src/platform/linux_storage.c
        src/platform/linux_services.c
        src/platform/linux_github_sign_in.c)
    target_compile_definitions(runnerscope_linux_platform PRIVATE
        RUNNERSCOPE_GH_VERSION="${RUNNERSCOPE_GH_VERSION}"
        RUNNERSCOPE_BUILD_ROOT="${CMAKE_BINARY_DIR}"
        RUNNERSCOPE_GH_BUILD_EXECUTABLE="${RUNNERSCOPE_GH_BINARY}"
        RUNNERSCOPE_GH_EXECUTABLE="${CMAKE_INSTALL_FULL_LIBEXECDIR}/runnerscope/gh")
    target_include_directories(runnerscope_linux_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(runnerscope_linux_platform PUBLIC
        runnerscope_core PkgConfig::GIO CURL::libcurl Threads::Threads)
    runnerscope_warnings(runnerscope_linux_platform)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope_linux_platform PRIVATE
            -Werror -Wconversion -Wsign-conversion)
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
    runnerscope_warnings(runnerscope)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(runnerscope PRIVATE -Werror -Wconversion -Wsign-conversion)
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
    install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/runnerscope" RENAME COPYING)
    install(FILES third_party/cjson/LICENSE
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/runnerscope"
        RENAME cJSON-LICENSE)
endif()

target_compile_definitions(runnerscope PRIVATE
    RUNNERSCOPE_VERSION="${PROJECT_VERSION}"
)

