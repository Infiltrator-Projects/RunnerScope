// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <wincodec.h>

#include <infiltratr/core.h>
#include <infiltratr/design.h>

#include "core/runner_config.h"
#include "core/github.h"
#include "core/github_auth.h"
#include "platform/github_sign_in.h"
#include "platform/http.h"
#include "platform/windows_config_store.h"
#include "ui/ui_contract.h"
#include "ui/ui_renderer.h"
#include "windows_resources.h"

/*
 * The Win32 shell remains one translation unit so callback-local state is not
 * exported as a second application API. Cohesive implementation slices below
 * keep provider/state, presentation and controller mechanics navigable while
 * the independently testable configuration/provider policies live in real
 * modules.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef RUNNERSCOPE_VERSION
#error "RUNNERSCOPE_VERSION must be provided by CMake"
#endif

#define APP_CLASS L"RunnerScope"
#define SETTINGS_CLASS L"RunnerScopeSettings"
#define APP_TITLE L"RunnerScope"
#define WM_APP_REFRESH_DONE (WM_APP + 1)
#define WM_APP_OPEN_SETTINGS (WM_APP + 2)
#define WM_APP_AUTH_DONE (WM_APP + 3)
#define WM_APP_AUTH_CODE (WM_APP + 4)
#define TIMER_AUTH 3U
#define ID_AUTH_START 1110
#define ID_AUTH_COPY 1111
#define ID_AUTH_OPEN 1112
#define ID_AUTH_CANCEL 1113
#define TIMER_REFRESH 1U
#define TIMER_PRESENT 2U
#define ID_SEARCH 1001
#define ID_RUNNERS 1002
#define ID_SETTINGS_ORG 1101
#define ID_SETTINGS_SAVE 1102
#define ID_SETTINGS_CANCEL 1103
#define ID_SETTINGS_TOKEN 1104
#define ID_SETTINGS_SAVE_TEST 1105

typedef struct {
    char name[192];
    char os[64];
    char state[32];
    char labels[512];
} RunnerRow;

typedef struct {
    char name[192];
    bool running;
    ULONGLONG state_since_ms;
    ULONGLONG busy_started_ms;
    ULONGLONG busy_total_ms;
    unsigned int jobs;
} RunnerSession;

typedef struct {
    HBITMAP bitmap;
    int width;
    int height;
} UiImage;

typedef struct {
    HWND target;
    char organisation[128];
    char token[RUNNER_GITHUB_TOKEN_CAPACITY];
    ULONGLONG generation;
    ULONGLONG deadline;
    HANDLE cancel;
} RefreshRequest;

typedef struct {
    RunnerRow *rows;
    size_t count;
    ULONGLONG generation;
    char error[640];
} RefreshResult;

typedef struct {
    HINSTANCE instance;
    HWND window;
    HWND search;
    HWND list;
    HWND settings_window;
    HWND settings_org;
    HWND settings_code;
    HWND settings_status;
    RunnerGithubAuth auth;
    HANDLE auth_thread;
    HANDLE auth_cancel;
    ULONGLONG auth_generation;
    bool auth_active;
    RunnerConfig config;
    char config_error[320];
    char provider_error[640];
    ULONGLONG config_generation;
    bool settings_dirty;
    bool rows_stale;
    bool shutting_down;
    HANDLE refresh_cancel;

    RunnerRow *rows;
    size_t row_count;
    RunnerSession *sessions;
    size_t session_count;
    size_t session_capacity;
    ULONGLONG session_started_ms;

    bool refreshing;
    bool table_view;
    int page;
    int selected_row;
    int card_scroll_y;
    wchar_t filter[256];

    UINT dpi;
    InfiltratrThemePalette palette;
    HBRUSH background_brush;
    HBRUSH input_brush;
    HFONT font;
    HFONT font_small;
    HFONT font_bold;
    HFONT font_title;
    HFONT font_brand;
    HANDLE private_fonts[3];
    IWICImagingFactory *wic;
    UiImage nav_images[4];
    UiImage hero_images[4];
    UiImage side_image;

    RECT header_rect;
    RECT settings_rect;
    RECT minimize_rect;
    RECT maximize_rect;
    RECT close_rect;
    RECT nav_rects[4];
    RECT cards_toggle_rect;
    RECT table_toggle_rect;
    RECT export_rect;
    RECT about_rect;
    RECT refresh_rect;
    RECT cards_area_rect;
    RECT selection_rect;
    RECT workspace_rect;
} App;

static App g_app;

static void rebuild_table(void);
static void update_layout(HWND window);
static void refresh_begin(void);
static void settings_status(const char *message);

#include "windows/windows_foundation.inc"
#include "windows/windows_runner.inc"
#include "windows/windows_layout.inc"
#include "windows/windows_presentation.inc"
static void save_settings(HWND window, bool test_access);
#include "windows/windows_auth.inc"
#include "windows/windows_actions.inc"
#include "windows/windows_controller.inc"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous,
                    PWSTR command_line, int show_command)
{
    (void)previous;
    (void)command_line;
    ZeroMemory(&g_app, sizeof(g_app));
    g_app.instance = instance;
    g_app.page = RUNNER_UI_PAGE_RUNNERS;
    g_app.selected_row = -1;

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 2 && wcscmp(argv[1], L"--self-test") == 0) {
        const int status = self_test();
        LocalFree(argv);
        return status;
    }
    if (argv) LocalFree(argv);

    if (!runner_windows_config_load(&g_app.config, g_app.config_error, sizeof(g_app.config_error))) {
        RunnerConfig defaults;
        runner_config_defaults(&defaults);
        g_app.config = defaults;
    }
    (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com_status = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&wc)) return 1;

    WNDCLASSEXW settings_wc;
    ZeroMemory(&settings_wc, sizeof(settings_wc));
    settings_wc.cbSize = sizeof(settings_wc);
    settings_wc.lpfnWndProc = settings_proc;
    settings_wc.hInstance = instance;
    settings_wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    settings_wc.lpszClassName = SETTINGS_CLASS;
    if (!RegisterClassExW(&settings_wc)) return 1;

    g_app.window = CreateWindowExW(
        WS_EX_APPWINDOW, APP_CLASS, APP_TITLE,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
        WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1360, 820,
        NULL, NULL, instance, NULL);
    if (!g_app.window) return 1;
    ShowWindow(g_app.window, show_command);
    UpdateWindow(g_app.window);

    MSG message = {0};
    int message_status;
    while ((message_status = (int)GetMessageW(&message, NULL, 0U, 0U)) > 0) {
        if (g_app.settings_window && IsDialogMessageW(g_app.settings_window, &message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (SUCCEEDED(com_status)) CoUninitialize();
    return message_status < 0 ? 1 : (int)message.wParam;
}
