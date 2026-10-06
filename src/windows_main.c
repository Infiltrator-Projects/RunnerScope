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
#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <wincodec.h>

#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/escape.h>

#include "windows_resources.h"
#include "ui/ui_contract.h"
#include "ui/ui_renderer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef RUNNERSCOPE_VERSION
#error "RUNNERSCOPE_VERSION must be provided by CMake"
#endif

#define APP_CLASS L"InfiltratrRunnerMonitor"
#define SETTINGS_CLASS L"InfiltratrRunnerMonitorSettings"
#define APP_TITLE L"Runner Monitor"
#define WM_APP_REFRESH_DONE (WM_APP + 1)
#define WM_APP_OPEN_SETTINGS (WM_APP + 2)
#define TIMER_REFRESH 1U
#define TIMER_PRESENT 2U

#define ID_SEARCH 1001
#define ID_RUNNERS 1002
#define ID_SETTINGS_ORG 1101
#define ID_SETTINGS_SAVE 1102
#define ID_SETTINGS_CANCEL 1103

typedef struct {
    char name[192];
    char os[64];
    char state[32];
    char labels[512];
} RunnerRow;

typedef struct {
    RunnerRow *rows;
    size_t count;
    char error[512];
} RefreshResult;

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

static HINSTANCE g_instance;
static HWND g_main;
static HWND g_search;
static HWND g_list;
static HWND g_settings_window;
static HWND g_settings_org;
static HFONT g_font;
static HFONT g_font_small;
static HFONT g_font_bold;
static HFONT g_font_title;
static HFONT g_font_brand;
static HBRUSH g_background_brush;
static HBRUSH g_input_brush;
static InfiltratrThemePalette g_palette;
static wchar_t g_organisation[128];
static wchar_t g_filter[256];
static RunnerRow *g_rows;
static size_t g_row_count;
static RunnerSession *g_sessions;
static size_t g_session_count;
static size_t g_session_capacity;
static ULONGLONG g_session_started_ms;
static bool g_refreshing;
static bool g_table_view;
static int g_page;
static int g_selected_row = -1;
static int g_card_scroll_y;
static UINT g_dpi = 96U;
static IWICImagingFactory *g_wic;
static HANDLE g_private_fonts[3];
static UiImage g_nav_images[4];
static UiImage g_hero_images[4];
static UiImage g_side_image;

static RECT g_header_rect;
static RECT g_settings_rect;
static RECT g_minimize_rect;
static RECT g_maximize_rect;
static RECT g_close_rect;
static RECT g_nav_rects[4];
static RECT g_cards_toggle_rect;
static RECT g_table_toggle_rect;
static RECT g_export_rect;
static RECT g_about_rect;
static RECT g_refresh_rect;
static RECT g_cards_area_rect;
static RECT g_selection_rect;
static RECT g_workspace_rect;

static COLORREF rgb(uint32_t value)
{
    return RGB((BYTE)((value >> 16U) & 0xffU),
               (BYTE)((value >> 8U) & 0xffU),
               (BYTE)(value & 0xffU));
}

static COLORREF mix_color(uint32_t base, uint32_t accent, unsigned int accent_percent)
{
    if (accent_percent > 100U) accent_percent = 100U;
    unsigned int base_percent = 100U - accent_percent;
    unsigned int br = (base >> 16U) & 0xffU;
    unsigned int bg = (base >> 8U) & 0xffU;
    unsigned int bb = base & 0xffU;
    unsigned int ar = (accent >> 16U) & 0xffU;
    unsigned int ag = (accent >> 8U) & 0xffU;
    unsigned int ab = accent & 0xffU;
    return RGB((br * base_percent + ar * accent_percent) / 100U,
               (bg * base_percent + ag * accent_percent) / 100U,
               (bb * base_percent + ab * accent_percent) / 100U);
}

static int sx(int value)
{
    return MulDiv(value, (int)g_dpi, 96);
}

static bool pt_in_rect(const RECT *rect, POINT point)
{
    return rect && PtInRect(rect, point) != FALSE;
}

static void utf8_to_wide(const char *input, wchar_t *output, size_t count)
{
    if (!output || count == 0U) return;
    output[0] = L'\0';
    if (!input || !*input) return;
    int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1,
                                      output, (int)count);
    if (written <= 0)
        (void)MultiByteToWideChar(CP_ACP, 0, input, -1, output, (int)count);
    output[count - 1U] = L'\0';
}

static bool wide_to_utf8(const wchar_t *input, char *output, size_t count)
{
    if (!output || count == 0U) return false;
    output[0] = '\0';
    if (!input) return true;
    int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1,
                                     NULL, 0, NULL, NULL);
    if (needed <= 0 || (size_t)needed > count) return false;
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1,
                               output, (int)count, NULL, NULL) > 0;
}

static bool system_prefers_dark(void)
{
    DWORD value = 1U;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL,
                     &value, &size) != ERROR_SUCCESS)
        return true;
    return value == 0U;
}

static void resolve_theme(void)
{
    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM, system_prefers_dark());
    if (palette) g_palette = *palette;
}

static void delete_brushes(void)
{
    if (g_background_brush) DeleteObject(g_background_brush);
    if (g_input_brush) DeleteObject(g_input_brush);
    g_background_brush = NULL;
    g_input_brush = NULL;
}

static void create_brushes(void)
{
    delete_brushes();
    g_background_brush = CreateSolidBrush(rgb(g_palette.background_rgb));
    g_input_brush = CreateSolidBrush(rgb(g_palette.input_rgb));
}

static wchar_t *config_path(void)
{
    wchar_t appdata[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (length == 0U || length >= MAX_PATH)
        length = GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, MAX_PATH);
    if (length == 0U || length >= MAX_PATH) return NULL;

    const wchar_t *suffix = L"\\RunnerScope\\config.json";
    size_t required = wcslen(appdata) + wcslen(suffix) + 1U;
    wchar_t *path = (wchar_t *)calloc(required, sizeof(wchar_t));
    if (!path) return NULL;
    wcscpy_s(path, required, appdata);
    wcscat_s(path, required, suffix);
    return path;
}

static void load_organisation(void)
{
    DWORD env = GetEnvironmentVariableW(L"GITHUB_RUNNER_ORG",
                                        g_organisation,
                                        (DWORD)(sizeof(g_organisation) /
                                                sizeof(g_organisation[0])));
    if (env > 0U && env < (DWORD)(sizeof(g_organisation) /
                                  sizeof(g_organisation[0])))
        return;

    wchar_t *path = config_path();
    if (!path) return;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(path);
    if (file == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return;
    }
    char *buffer = (char *)calloc((size_t)size.QuadPart + 1U, 1U);
    if (!buffer) {
        CloseHandle(file);
        return;
    }
    DWORD read = 0U;
    if (ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL)) {
        const char *key = strstr(buffer, "\"organisation\"");
        if (key) {
            const char *colon = strchr(key, ':');
            const char *quote = colon ? strchr(colon, '"') : NULL;
            if (quote) {
                quote++;
                const char *end = strchr(quote, '"');
                if (end && end > quote) {
                    size_t bytes = (size_t)(end - quote);
                    if (bytes < 127U) {
                        char org[128];
                        memcpy(org, quote, bytes);
                        org[bytes] = '\0';
                        utf8_to_wide(org, g_organisation,
                                     sizeof(g_organisation) /
                                     sizeof(g_organisation[0]));
                    }
                }
            }
        }
    }
    free(buffer);
    CloseHandle(file);
}

static bool save_organisation(void)
{
    wchar_t *path = config_path();
    if (!path) return false;

    wchar_t directory[MAX_PATH];
    wcsncpy_s(directory, MAX_PATH, path, _TRUNCATE);
    wchar_t *slash = wcsrchr(directory, L'\\');
    if (!slash) {
        free(path);
        return false;
    }
    *slash = L'\0';
    CreateDirectoryW(directory, NULL);

    char org[384];
    if (!wide_to_utf8(g_organisation, org, sizeof(org))) {
        free(path);
        return false;
    }
    size_t needed = 0U;
    (void)infiltratr_escape_json(org, NULL, 0U, &needed);
    char *escaped = (char *)calloc(needed + 1U, 1U);
    if (!escaped) {
        free(path);
        return false;
    }
    if (!infiltratr_escape_json(org, escaped, needed + 1U, NULL)) {
        free(escaped);
        free(path);
        return false;
    }

    char json[1024];
    int json_len = snprintf(json, sizeof(json),
        "{\r\n"
        "  \"organisation\": \"%s\",\r\n"
        "  \"expected_runners\": 0,\r\n"
        "  \"runner_poll_seconds\": 3,\r\n"
        "  \"activity_scan_seconds\": 45,\r\n"
        "  \"repository_scan_limit\": 25,\r\n"
        "  \"local_health_seconds\": 15,\r\n"
        "  \"theme_mode\": \"system\"\r\n"
        "}\r\n", escaped);
    free(escaped);
    if (json_len <= 0 || (size_t)json_len >= sizeof(json)) {
        free(path);
        return false;
    }

    wchar_t temp[MAX_PATH];
    _snwprintf_s(temp, MAX_PATH, _TRUNCATE, L"%ls.tmp", path);
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        free(path);
        return false;
    }
    DWORD written = 0U;
    BOOL ok = WriteFile(file, json, (DWORD)json_len, &written, NULL);
    if (ok) ok = FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || written != (DWORD)json_len) {
        DeleteFileW(temp);
        free(path);
        return false;
    }
    ok = MoveFileExW(temp, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temp);
    free(path);
    return ok != FALSE;
}

static char *run_capture(const wchar_t *command, DWORD *exit_code)
{
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0U)) return NULL;
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0U);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    size_t chars = wcslen(command) + 1U;
    wchar_t *mutable_command = (wchar_t *)calloc(chars, sizeof(wchar_t));
    if (!mutable_command) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return NULL;
    }
    wcscpy_s(mutable_command, chars, command);

    BOOL created = CreateProcessW(NULL, mutable_command, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(mutable_command);
    CloseHandle(write_pipe);
    if (!created) {
        CloseHandle(read_pipe);
        return NULL;
    }

    size_t capacity = 8192U;
    size_t used = 0U;
    char *buffer = (char *)calloc(capacity, 1U);
    if (!buffer) {
        TerminateProcess(pi.hProcess, 1U);
        CloseHandle(read_pipe);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return NULL;
    }

    for (;;) {
        char chunk[4096];
        DWORD amount = 0U;
        BOOL ok = ReadFile(read_pipe, chunk, sizeof(chunk), &amount, NULL);
        if (!ok || amount == 0U) break;
        if (used + (size_t)amount + 1U > capacity) {
            size_t next = capacity * 2U;
            while (next < used + (size_t)amount + 1U) next *= 2U;
            char *grown = (char *)realloc(buffer, next);
            if (!grown) {
                free(buffer);
                buffer = NULL;
                break;
            }
            buffer = grown;
            capacity = next;
        }
        memcpy(buffer + used, chunk, amount);
        used += amount;
        buffer[used] = '\0';
    }

    WaitForSingleObject(pi.hProcess, 30000U);
    DWORD code = 1U;
    GetExitCodeProcess(pi.hProcess, &code);
    if (exit_code) *exit_code = code;
    CloseHandle(read_pipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return buffer;
}

static DWORD WINAPI refresh_worker(LPVOID parameter)
{
    HWND window = (HWND)parameter;
    RefreshResult *result = (RefreshResult *)calloc(1U, sizeof(*result));
    if (!result) return 0U;

    wchar_t organisation[128];
    wcsncpy_s(organisation, 128U, g_organisation, _TRUNCATE);
    if (!organisation[0]) {
        strcpy_s(result->error, sizeof(result->error),
                 "Open Settings and enter the GitHub organisation.");
        PostMessageW(window, WM_APP_REFRESH_DONE, 0, (LPARAM)result);
        return 0U;
    }

    wchar_t command[2048];
    _snwprintf_s(command, 2048U, _TRUNCATE,
        L"gh api \"/orgs/%ls/actions/runners?per_page=100\" --paginate "
        L"--jq \".runners[] | [.name, .os, .status, (.busy|tostring), "
        L"([.labels[].name] | join(\\\",\\\"))] | @tsv\"",
        organisation);

    DWORD code = 1U;
    char *output = run_capture(command, &code);
    if (!output || code != 0U) {
        _snprintf_s(result->error, sizeof(result->error), _TRUNCATE,
                    "GitHub request failed. Ensure gh is installed and authenticated.%s%s",
                    output && *output ? " " : "", output && *output ? output : "");
        free(output);
        PostMessageW(window, WM_APP_REFRESH_DONE, 0, (LPARAM)result);
        return 0U;
    }

    size_t rows_capacity = 32U;
    result->rows = (RunnerRow *)calloc(rows_capacity, sizeof(RunnerRow));
    if (!result->rows) {
        free(output);
        free(result);
        return 0U;
    }

    char *context = NULL;
    for (char *line = strtok_s(output, "\r\n", &context);
         line;
         line = strtok_s(NULL, "\r\n", &context)) {
        if (!*line) continue;
        if (result->count == rows_capacity) {
            size_t next = rows_capacity * 2U;
            RunnerRow *grown =
                (RunnerRow *)realloc(result->rows, next * sizeof(RunnerRow));
            if (!grown) break;
            ZeroMemory(grown + rows_capacity,
                       (next - rows_capacity) * sizeof(RunnerRow));
            result->rows = grown;
            rows_capacity = next;
        }
        RunnerRow *row = &result->rows[result->count];
        char *fields[5] = {0};
        char *field_context = NULL;
        size_t index = 0U;
        for (char *field = strtok_s(line, "\t", &field_context);
             field && index < 5U;
             field = strtok_s(NULL, "\t", &field_context))
            fields[index++] = field;
        if (index < 4U) continue;
        infiltratr_copy_string(row->name, sizeof(row->name), fields[0]);
        infiltratr_copy_string(row->os, sizeof(row->os), fields[1]);
        bool online = strcmp(fields[2], "online") == 0;
        bool busy = strcmp(fields[3], "true") == 0;
        infiltratr_copy_string(row->state, sizeof(row->state),
                               !online ? "OFFLINE" : busy ? "RUNNING" : "IDLE");
        if (index >= 5U)
            infiltratr_copy_string(row->labels, sizeof(row->labels), fields[4]);
        result->count++;
    }
    free(output);
    if (!PostMessageW(window, WM_APP_REFRESH_DONE, 0, (LPARAM)result)) {
        free(result->rows);
        free(result);
    }
    return 0U;
}

static RunnerSession *session_for(const char *name, bool create)
{
    if (!name || !*name) return NULL;
    for (size_t i = 0U; i < g_session_count; ++i) {
        if (strcmp(g_sessions[i].name, name) == 0) return &g_sessions[i];
    }
    if (!create) return NULL;
    if (g_session_count == g_session_capacity) {
        size_t next = g_session_capacity ? g_session_capacity * 2U : 16U;
        RunnerSession *grown =
            (RunnerSession *)realloc(g_sessions, next * sizeof(*grown));
        if (!grown) return NULL;
        ZeroMemory(grown + g_session_capacity,
                   (next - g_session_capacity) * sizeof(*grown));
        g_sessions = grown;
        g_session_capacity = next;
    }
    RunnerSession *session = &g_sessions[g_session_count++];
    ZeroMemory(session, sizeof(*session));
    infiltratr_copy_string(session->name, sizeof(session->name), name);
    session->state_since_ms = GetTickCount64();
    return session;
}

static void update_sessions(void)
{
    const ULONGLONG now = GetTickCount64();
    for (size_t i = 0U; i < g_row_count; ++i) {
        RunnerRow *row = &g_rows[i];
        RunnerSession *session = session_for(row->name, true);
        if (!session) continue;
        const bool running = strcmp(row->state, "RUNNING") == 0;
        if (session->state_since_ms == 0U) session->state_since_ms = now;
        if (running != session->running) {
            if (session->running && session->busy_started_ms != 0U)
                session->busy_total_ms += now - session->busy_started_ms;
            if (running) {
                session->busy_started_ms = now;
                session->jobs++;
            } else {
                session->busy_started_ms = 0U;
            }
            session->running = running;
            session->state_since_ms = now;
        }
    }
}

static void format_duration(ULONGLONG ms, wchar_t *output, size_t count)
{
    if (!output || count == 0U) return;
    ULONGLONG seconds = ms / 1000U;
    ULONGLONG hours = seconds / 3600U;
    ULONGLONG minutes = (seconds % 3600U) / 60U;
    seconds %= 60U;
    if (hours > 0U)
        _snwprintf_s(output, count, _TRUNCATE, L"%lluh %llum", hours, minutes);
    else if (minutes > 0U)
        _snwprintf_s(output, count, _TRUNCATE, L"%llum %llus", minutes, seconds);
    else
        _snwprintf_s(output, count, _TRUNCATE, L"%llus", seconds);
}

static double session_busy_fraction(const RunnerSession *session)
{
    if (!session || g_session_started_ms == 0U) return 0.0;
    ULONGLONG now = GetTickCount64();
    ULONGLONG elapsed = now - g_session_started_ms;
    if (elapsed == 0U) return 0.0;
    ULONGLONG busy = session->busy_total_ms;
    if (session->running && session->busy_started_ms != 0U)
        busy += now - session->busy_started_ms;
    double value = (double)busy / (double)elapsed;
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    return value;
}

static bool row_matches_filter(const RunnerRow *row)
{
    if (!row || !g_filter[0]) return true;
    wchar_t value[1200];
    wchar_t name[192], os[64], state[32], labels[512];
    utf8_to_wide(row->name, name, 192U);
    utf8_to_wide(row->os, os, 64U);
    utf8_to_wide(row->state, state, 32U);
    utf8_to_wide(row->labels, labels, 512U);
    _snwprintf_s(value, 1200U, _TRUNCATE, L"%ls %ls %ls %ls", name, os, state, labels);
    return StrStrIW(value, g_filter) != NULL;
}

static size_t visible_runner_count(void)
{
    size_t count = 0U;
    for (size_t i = 0U; i < g_row_count; ++i)
        if (row_matches_filter(&g_rows[i])) count++;
    return count;
}

static RunnerRow *visible_runner_at(size_t visible_index, size_t *source_index)
{
    size_t index = 0U;
    for (size_t i = 0U; i < g_row_count; ++i) {
        if (!row_matches_filter(&g_rows[i])) continue;
        if (index == visible_index) {
            if (source_index) *source_index = i;
            return &g_rows[i];
        }
        index++;
    }
    return NULL;
}

static void list_add_column(int index, int width, const wchar_t *title)
{
    LVCOLUMNW column;
    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = (LPWSTR)title;
    column.cx = sx(width);
    column.iSubItem = index;
    ListView_InsertColumn(g_list, index, &column);
}

static void rebuild_table(void)
{
    if (!g_list) return;
    ListView_DeleteAllItems(g_list);
    const ULONGLONG now = GetTickCount64();
    int out = 0;
    for (size_t i = 0U; i < g_row_count; ++i) {
        RunnerRow *row = &g_rows[i];
        if (!row_matches_filter(row)) continue;
        RunnerSession *session = session_for(row->name, false);
        wchar_t name[192], os[64], state[32], labels[512];
        wchar_t state_for[64] = L"—";
        wchar_t jobs[32] = L"0";
        wchar_t busy[32] = L"0.0%";
        utf8_to_wide(row->name, name, 192U);
        utf8_to_wide(row->os, os, 64U);
        utf8_to_wide(row->state, state, 32U);
        utf8_to_wide(row->labels, labels, 512U);
        if (session) {
            format_duration(now - session->state_since_ms, state_for, 64U);
            _snwprintf_s(jobs, 32U, _TRUNCATE, L"%u", session->jobs);
            _snwprintf_s(busy, 32U, _TRUNCATE, L"%.1f%%",
                         session_busy_fraction(session) * 100.0);
        }
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = out;
        item.pszText = name;
        item.lParam = (LPARAM)i;
        int inserted = ListView_InsertItem(g_list, &item);
        ListView_SetItemText(g_list, inserted, 1, os);
        ListView_SetItemText(g_list, inserted, 2, state);
        ListView_SetItemText(g_list, inserted, 3, L"—");
        ListView_SetItemText(g_list, inserted, 4, L"—");
        ListView_SetItemText(g_list, inserted, 5,
                             strcmp(row->state, "RUNNING") == 0 ? state_for : L"—");
        ListView_SetItemText(g_list, inserted, 6, state_for);
        ListView_SetItemText(g_list, inserted, 7, jobs);
        ListView_SetItemText(g_list, inserted, 8, busy);
        ListView_SetItemText(g_list, inserted, 9, labels);
        out++;
    }
}

static void refresh_begin(void)
{
    if (g_refreshing) return;
    if (!g_organisation[0]) {
        PostMessageW(g_main, WM_APP_OPEN_SETTINGS, 0, 0);
        return;
    }
    g_refreshing = true;
    InvalidateRect(g_main, NULL, FALSE);
    HANDLE thread = CreateThread(NULL, 0U, refresh_worker, g_main, 0U, NULL);
    if (thread) CloseHandle(thread);
    else g_refreshing = false;
}

static void apply_refresh(RefreshResult *result)
{
    g_refreshing = false;
    if (!result) return;
    if (!result->error[0]) {
        free(g_rows);
        g_rows = result->rows;
        g_row_count = result->count;
        result->rows = NULL;
        update_sessions();
        if (g_selected_row >= (int)g_row_count) g_selected_row = -1;
        rebuild_table();
    } else {
        wchar_t error[512];
        utf8_to_wide(result->error, error, 512U);
        MessageBoxW(g_main, error, APP_TITLE, MB_OK | MB_ICONWARNING);
    }
    free(result->rows);
    free(result);
    InvalidateRect(g_main, NULL, FALSE);
}

static HANDLE load_private_font_resource(int resource_id)
{
    HRSRC resource = FindResourceW(g_instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return NULL;
    HGLOBAL loaded = LoadResource(g_instance, resource);
    if (!loaded) return NULL;
    DWORD size = SizeofResource(g_instance, resource);
    void *data = LockResource(loaded);
    if (!data || size == 0U) return NULL;
    DWORD fonts = 0U;
    return AddFontMemResourceEx(data, size, NULL, &fonts);
}

static void load_private_fonts(void)
{
    g_private_fonts[0] = load_private_font_resource(IDR_FONT_BRAND);
    g_private_fonts[1] = load_private_font_resource(IDR_FONT_UI_BOLD);
    g_private_fonts[2] = load_private_font_resource(IDR_FONT_UI_REGULAR);
}

static void unload_private_fonts(void)
{
    for (size_t i = 0U; i < 3U; ++i) {
        if (g_private_fonts[i]) RemoveFontMemResourceEx(g_private_fonts[i]);
        g_private_fonts[i] = NULL;
    }
}

static UiImage load_png_resource(int resource_id)
{
    UiImage image = {0};
    if (!g_wic) return image;
    HRSRC resource = FindResourceW(g_instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return image;
    HGLOBAL loaded = LoadResource(g_instance, resource);
    if (!loaded) return image;
    DWORD size = SizeofResource(g_instance, resource);
    const BYTE *data = (const BYTE *)LockResource(loaded);
    if (!data || size == 0U) return image;

    IStream *stream = SHCreateMemStream(data, size);
    if (!stream) return image;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HRESULT hr = IWICImagingFactory_CreateDecoderFromStream(
        g_wic, stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(decoder, 0U, &frame);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateFormatConverter(g_wic, &converter);
    if (SUCCEEDED(hr)) {
        hr = IWICFormatConverter_Initialize(
            converter, (IWICBitmapSource *)frame,
            &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }
    UINT width = 0U, height = 0U;
    if (SUCCEEDED(hr))
        hr = IWICBitmapSource_GetSize((IWICBitmapSource *)converter, &width, &height);

    HBITMAP bitmap = NULL;
    void *bits = NULL;
    if (SUCCEEDED(hr) && width > 0U && height > 0U) {
        BITMAPINFO info;
        ZeroMemory(&info, sizeof(info));
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = (LONG)width;
        info.bmiHeader.biHeight = -(LONG)height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0U);
        if (bitmap && bits) {
            UINT stride = width * 4U;
            hr = IWICBitmapSource_CopyPixels((IWICBitmapSource *)converter,
                                             NULL, stride, stride * height,
                                             (BYTE *)bits);
            if (FAILED(hr)) {
                DeleteObject(bitmap);
                bitmap = NULL;
            }
        }
    }

    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    IStream_Release(stream);

    if (bitmap) {
        image.bitmap = bitmap;
        image.width = (int)width;
        image.height = (int)height;
    }
    return image;
}

static void load_images(void)
{
    const int nav_ids[4] = {
        IDR_NAV_RUNNERS, IDR_NAV_ACTIVE, IDR_NAV_HISTORY, IDR_NAV_HEALTH
    };
    const int hero_ids[4] = {
        IDR_HERO_RUNNERS, IDR_HERO_ACTIVE, IDR_HERO_HISTORY, IDR_HERO_HEALTH
    };
    for (int i = 0; i < 4; ++i) {
        g_nav_images[i] = load_png_resource(nav_ids[i]);
        g_hero_images[i] = load_png_resource(hero_ids[i]);
    }
    g_side_image = load_png_resource(IDR_NAV_INFILTRATOR);
}

static void unload_images(void)
{
    for (int i = 0; i < 4; ++i) {
        if (g_nav_images[i].bitmap) DeleteObject(g_nav_images[i].bitmap);
        if (g_hero_images[i].bitmap) DeleteObject(g_hero_images[i].bitmap);
        ZeroMemory(&g_nav_images[i], sizeof(g_nav_images[i]));
        ZeroMemory(&g_hero_images[i], sizeof(g_hero_images[i]));
    }
    if (g_side_image.bitmap) DeleteObject(g_side_image.bitmap);
    ZeroMemory(&g_side_image, sizeof(g_side_image));
}

static void draw_image(HDC dc, const UiImage *image, const RECT *rect)
{
    if (!dc || !image || !image->bitmap || !rect) return;
    HDC source = CreateCompatibleDC(dc);
    if (!source) return;
    HGDIOBJ previous = SelectObject(source, image->bitmap);
    BLENDFUNCTION blend = {AC_SRC_OVER, 0U, 255U, AC_SRC_ALPHA};
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    AlphaBlend(dc, rect->left, rect->top, width, height,
               source, 0, 0, image->width, image->height, blend);
    SelectObject(source, previous);
    DeleteDC(source);
}

static void fill_round_rect(HDC dc, RECT rect, COLORREF fill,
                            COLORREF border, int radius)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom,
              radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void fill_rect_color(HDC dc, RECT rect, COLORREF fill)
{
    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

static void draw_text(HDC dc, const wchar_t *text, RECT rect, HFONT font,
                      COLORREF color, UINT format)
{
    HGDIOBJ previous = SelectObject(dc, font ? font : g_font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text ? text : L"", -1, &rect, format);
    SelectObject(dc, previous);
}

static void draw_outline_icon(HDC dc, RECT rect, int kind, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, sx(1), color);
    HGDIOBJ previous = SelectObject(dc, pen);
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    if (kind == 0) {
        MoveToEx(dc, cx - sx(6), cy + sx(4), NULL);
        LineTo(dc, cx + sx(6), cy + sx(4));
    } else if (kind == 1) {
        Rectangle(dc, cx - sx(6), cy - sx(5), cx + sx(6), cy + sx(5));
    } else if (kind == 2) {
        MoveToEx(dc, cx - sx(5), cy - sx(5), NULL);
        LineTo(dc, cx + sx(5), cy + sx(5));
        MoveToEx(dc, cx + sx(5), cy - sx(5), NULL);
        LineTo(dc, cx - sx(5), cy + sx(5));
    } else {
        Ellipse(dc, cx - sx(5), cy - sx(5), cx + sx(5), cy + sx(5));
        MoveToEx(dc, cx, cy - sx(10), NULL); LineTo(dc, cx, cy - sx(6));
        MoveToEx(dc, cx, cy + sx(6), NULL); LineTo(dc, cx, cy + sx(10));
        MoveToEx(dc, cx - sx(10), cy, NULL); LineTo(dc, cx - sx(6), cy);
        MoveToEx(dc, cx + sx(6), cy, NULL); LineTo(dc, cx + sx(10), cy);
    }
    SelectObject(dc, previous);
    DeleteObject(pen);
}

static void draw_button(HDC dc, RECT rect, const wchar_t *label, bool active,
                        bool primary)
{
    uint32_t base = active ? g_palette.selection_background_rgb
                           : g_palette.button_background_rgb;
    if (primary && !active) base = g_palette.operation_rgb;
    COLORREF fill = rgb(base);
    COLORREF border = active || primary ? rgb(g_palette.neutral_accent_rgb)
                                        : rgb(g_palette.border_rgb);
    fill_round_rect(dc, rect, fill, border, sx(8));
    COLORREF text = active || primary ? rgb(g_palette.selection_foreground_rgb)
                                      : rgb(g_palette.button_foreground_rgb);
    draw_text(dc, label, rect, g_font_bold, text,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void update_layout(HWND window)
{
    RECT client;
    GetClientRect(window, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    int pad = sx(20);
    int header = sx(60);
    int footer = sx(96);
    int nav = sx(195);

    SetRect(&g_header_rect, 0, 0, width, header);
    int control = sx(32);
    int right = width - sx(14);
    SetRect(&g_close_rect, right - control, sx(14), right, sx(46));
    right -= control + sx(6);
    SetRect(&g_maximize_rect, right - control, sx(14), right, sx(46));
    right -= control + sx(6);
    SetRect(&g_minimize_rect, right - control, sx(14), right, sx(46));
    right -= control + sx(10);
    SetRect(&g_settings_rect, right - sx(38), sx(11), right, sx(49));

    RECT content = {pad, header + sx(14), width - pad, height - footer - sx(14)};
    SetRect(&g_workspace_rect, content.left + nav, content.top,
            content.right, content.bottom);

    int nav_x = content.left + sx(8);
    int nav_y = content.top + sx(10);
    for (int i = 0; i < 4; ++i) {
        if (i == 3) nav_y += sx(12);
        SetRect(&g_nav_rects[i], nav_x, nav_y,
                content.left + nav - sx(8), nav_y + sx(50));
        nav_y += sx(53);
    }

    int workspace_left = g_workspace_rect.left + sx(18);
    int workspace_right = g_workspace_rect.right - sx(18);
    int toolbar_top = g_workspace_rect.top + sx(118);
    SetRect(&g_cards_toggle_rect, workspace_left, toolbar_top + sx(9),
            workspace_left + sx(70), toolbar_top + sx(43));
    SetRect(&g_table_toggle_rect, g_cards_toggle_rect.right, toolbar_top + sx(9),
            g_cards_toggle_rect.right + sx(70), toolbar_top + sx(43));

    int search_width = sx(250);
    int search_x = workspace_right - search_width;
    MoveWindow(g_search, search_x, toolbar_top + sx(9), search_width, sx(34), TRUE);

    int selection_height = g_selected_row >= 0 && g_page == 0 ? sx(76) : 0;
    SetRect(&g_selection_rect, workspace_left,
            g_workspace_rect.bottom - selection_height - sx(8),
            workspace_right, g_workspace_rect.bottom - sx(8));
    SetRect(&g_cards_area_rect, workspace_left, toolbar_top + sx(56),
            workspace_right,
            selection_height > 0 ? g_selection_rect.top - sx(8)
                                 : g_workspace_rect.bottom - sx(8));
    MoveWindow(g_list, g_cards_area_rect.left, g_cards_area_rect.top,
               g_cards_area_rect.right - g_cards_area_rect.left,
               g_cards_area_rect.bottom - g_cards_area_rect.top, TRUE);

    int footer_top = height - footer;
    int button_y = footer_top + sx(10);
    SetRect(&g_export_rect, pad, button_y, pad + sx(112), button_y + sx(34));
    SetRect(&g_about_rect, g_export_rect.right + sx(8), button_y,
            g_export_rect.right + sx(98), button_y + sx(34));
    SetRect(&g_refresh_rect, width - pad - sx(126), button_y,
            width - pad, button_y + sx(34));
}

static const wchar_t *page_title(int page)
{
    static wchar_t value[128];
    const RunnerUiPageSpec *ui_page =
        runner_ui_page((RunnerUiPageId)page);
    utf8_to_wide(
        runner_ui_page_title(ui_page, RUNNER_UI_PLATFORM_WINDOWS),
        value, sizeof(value) / sizeof(value[0]));
    return value;
}

static const wchar_t *page_subtitle(int page)
{
    static wchar_t value[160];
    const RunnerUiPageSpec *ui_page =
        runner_ui_page((RunnerUiPageId)page);
    utf8_to_wide(ui_page->subtitle, value, sizeof(value) / sizeof(value[0]));
    return value;
}

static void draw_header(HDC dc)
{
    fill_rect_color(dc, g_header_rect, rgb(g_palette.titlebar_rgb));
    RECT accent = g_header_rect;
    accent.right = sx(210);
    fill_rect_color(dc, accent,
                    mix_color(g_palette.titlebar_rgb,
                              g_palette.neutral_accent_rgb, 24U));

    RECT icon_well = {sx(16), sx(10), sx(58), sx(50)};
    fill_round_rect(dc, icon_well,
                    mix_color(g_palette.card_rgb, g_palette.neutral_accent_rgb, 18U),
                    rgb(g_palette.neutral_accent_rgb), sx(10));
    RECT icon = {sx(20), sx(12), sx(54), sx(48)};
    draw_image(dc, &g_nav_images[0], &icon);

    wchar_t product_title[96];
    wchar_t product_family[96];
    utf8_to_wide(runner_ui_product_title(), product_title, 96U);
    utf8_to_wide(runner_ui_product_family(), product_family, 96U);
    RECT title = {sx(70), sx(8), sx(370), sx(34)};
    draw_text(dc, product_title, title, g_font_brand,
              rgb(g_palette.title_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = {sx(70), sx(32), sx(370), sx(52)};
    draw_text(dc, product_family, subtitle, g_font_small,
              rgb(g_palette.muted_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    fill_round_rect(dc, g_settings_rect,
                    mix_color(g_palette.surface_rgb, g_palette.neutral_accent_rgb, 8U),
                    rgb(g_palette.connection_border_rgb), sx(7));
    draw_outline_icon(dc, g_settings_rect, 3, rgb(g_palette.button_foreground_rgb));
    fill_round_rect(dc, g_minimize_rect, rgb(g_palette.surface_rgb),
                    rgb(g_palette.connection_border_rgb), sx(6));
    fill_round_rect(dc, g_maximize_rect, rgb(g_palette.surface_rgb),
                    rgb(g_palette.connection_border_rgb), sx(6));
    fill_round_rect(dc, g_close_rect, rgb(g_palette.surface_rgb),
                    rgb(g_palette.connection_border_rgb), sx(6));
    draw_outline_icon(dc, g_minimize_rect, 0, rgb(g_palette.button_foreground_rgb));
    draw_outline_icon(dc, g_maximize_rect, 1, rgb(g_palette.button_foreground_rgb));
    draw_outline_icon(dc, g_close_rect, 2, rgb(g_palette.button_foreground_rgb));
}


typedef struct {
    HDC dc;
} WinNavigationRendererContext;

static bool win_renderer_begin_navigation(void *context,
                                          size_t item_count,
                                          RunnerUiPageId selected_page)
{
    (void)item_count;
    (void)selected_page;
    WinNavigationRendererContext *renderer = context;
    return renderer && renderer->dc;
}

static bool win_renderer_navigation_separator(
    void *context, const RunnerUiPageSpec *before_page)
{
    WinNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !before_page) return false;
    int page = (int)before_page->id;
    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT) return false;
    RECT separator = {
        g_nav_rects[page].left + sx(2),
        g_nav_rects[page].top - sx(7),
        g_nav_rects[page].right - sx(2),
        g_nav_rects[page].top - sx(6)
    };
    fill_rect_color(renderer->dc, separator, rgb(g_palette.border_rgb));
    return true;
}

static bool win_renderer_navigation_item(void *context,
                                         const RunnerUiPageSpec *page,
                                         const char *platform_label,
                                         bool selected)
{
    WinNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !page || !platform_label) return false;
    int index = (int)page->id;
    if (index < 0 || index >= RUNNER_UI_PAGE_COUNT) return false;

    wchar_t nav_label[128];
    utf8_to_wide(platform_label, nav_label,
                 sizeof(nav_label) / sizeof(nav_label[0]));
    RECT item = g_nav_rects[index];
    if (selected) {
        fill_round_rect(renderer->dc, item,
                        rgb(g_palette.selection_background_rgb),
                        rgb(g_palette.neutral_accent_rgb), sx(12));
    }
    RECT image_rect = {item.left + sx(5), item.top + sx(5),
                       item.left + sx(45), item.bottom - sx(5)};
    draw_image(renderer->dc, &g_nav_images[index], &image_rect);
    RECT text_rect = {item.left + sx(54), item.top,
                      item.right - sx(6), item.bottom};
    draw_text(renderer->dc, nav_label, text_rect, g_font_bold,
              selected ? rgb(g_palette.selection_foreground_rgb)
                       : rgb(g_palette.text_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return true;
}

static bool win_renderer_end_navigation(void *context)
{
    return context != NULL;
}

static void draw_navigation(HDC dc, RECT content)
{
    RECT nav = {content.left, content.top, g_workspace_rect.left, content.bottom};
    fill_round_rect(dc, nav, rgb(g_palette.panel_rgb),
                    rgb(g_palette.connection_border_rgb), sx(12));
    WinNavigationRendererContext navigation_context = {dc};
    RunnerUiRenderer navigation_renderer = {0};
    navigation_renderer.context = &navigation_context;
    navigation_renderer.begin_navigation = win_renderer_begin_navigation;
    navigation_renderer.navigation_separator =
        win_renderer_navigation_separator;
    navigation_renderer.navigation_item = win_renderer_navigation_item;
    navigation_renderer.end_navigation = win_renderer_end_navigation;
    if (!runner_ui_render_navigation(
            &navigation_renderer,
            RUNNER_UI_PLATFORM_WINDOWS,
            (RunnerUiPageId)g_page))
        return;

    if (g_side_image.bitmap) {
        int image_h = sx(180);
        RECT side = {nav.left + sx(18), nav.bottom - image_h - sx(14),
                     nav.right - sx(18), nav.bottom - sx(14)};
        draw_image(dc, &g_side_image, &side);
    }
}

static void draw_metrics(HDC dc, RECT header)
{
    size_t total = g_row_count, running = 0U, idle = 0U, offline = 0U;
    for (size_t i = 0U; i < g_row_count; ++i) {
        if (strcmp(g_rows[i].state, "RUNNING") == 0) running++;
        else if (strcmp(g_rows[i].state, "IDLE") == 0) idle++;
        else offline++;
    }
    const wchar_t *captions[4] = {L"TOTAL", L"RUNNING", L"IDLE", L"OFFLINE"};
    size_t values[4] = {total, running, idle, offline};
    int metric_w = sx(76);
    int gap = sx(6);
    int right = header.right - sx(8);
    for (int i = 3; i >= 0; --i) {
        RECT metric = {right - metric_w, header.top + sx(18),
                       right, header.top + sx(82)};
        fill_round_rect(dc, metric,
                        mix_color(g_palette.surface_rgb, g_palette.neutral_accent_rgb,
                                  i == 1 ? 12U : 5U),
                        rgb(g_palette.border_rgb), sx(9));
        wchar_t value[32];
        _snwprintf_s(value, 32U, _TRUNCATE, L"%zu", values[i]);
        RECT value_rect = {metric.left + sx(4), metric.top + sx(7),
                           metric.right - sx(4), metric.top + sx(37)};
        draw_text(dc, value, value_rect, g_font_title,
                  rgb(g_palette.title_rgb), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT caption_rect = {metric.left + sx(3), metric.top + sx(38),
                             metric.right - sx(3), metric.bottom - sx(5)};
        draw_text(dc, captions[i], caption_rect, g_font_small,
                  rgb(g_palette.muted_rgb), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        right = metric.left - gap;
    }
}

static void draw_workspace_header(HDC dc)
{
    RECT header = {g_workspace_rect.left, g_workspace_rect.top,
                   g_workspace_rect.right, g_workspace_rect.top + sx(108)};
    fill_round_rect(dc, header, rgb(g_palette.card_rgb),
                    rgb(g_palette.connection_border_rgb), sx(12));

    RECT icon_well = {header.left + sx(16), header.top + sx(17),
                      header.left + sx(74), header.top + sx(75)};
    fill_round_rect(dc, icon_well,
                    mix_color(g_palette.surface_rgb, g_palette.neutral_accent_rgb, 11U),
                    rgb(g_palette.neutral_accent_rgb), sx(11));
    RECT icon = {icon_well.left + sx(5), icon_well.top + sx(5),
                 icon_well.right - sx(5), icon_well.bottom - sx(5)};
    draw_image(dc, &g_nav_images[g_page], &icon);

    RECT title = {icon_well.right + sx(12), header.top + sx(16),
                  header.left + sx(420), header.top + sx(47)};
    draw_text(dc, page_title(g_page), title, g_font_brand,
              rgb(g_palette.title_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT subtitle = {title.left, header.top + sx(50),
                     header.left + sx(430), header.top + sx(73)};
    draw_text(dc, page_subtitle(g_page), subtitle, g_font_small,
              rgb(g_palette.kicker_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int metrics_width = sx(322);
    RECT hero = {header.right - metrics_width - sx(258), header.top + sx(11),
                 header.right - metrics_width - sx(18), header.top + sx(97)};
    draw_image(dc, &g_hero_images[g_page], &hero);
    draw_metrics(dc, header);
}

static COLORREF state_color(const char *state)
{
    if (state && strcmp(state, "RUNNING") == 0) return rgb(g_palette.success_rgb);
    if (state && strcmp(state, "IDLE") == 0) return rgb(g_palette.info_rgb);
    return rgb(g_palette.fault_rgb);
}

static void draw_runner_card(HDC dc, const RunnerRow *row, size_t source_index,
                             RECT card)
{
    bool selected = g_selected_row == (int)source_index;
    COLORREF border = selected ? rgb(g_palette.neutral_accent_rgb)
                               : rgb(g_palette.border_rgb);
    fill_round_rect(dc, card,
                    selected ? mix_color(g_palette.card_rgb,
                                         g_palette.neutral_accent_rgb, 10U)
                             : rgb(g_palette.card_rgb),
                    border, sx(12));
    RECT stripe = {card.left + sx(1), card.top + sx(1),
                   card.left + sx(6), card.bottom - sx(1)};
    fill_rect_color(dc, stripe, state_color(row->state));

    RECT icon = {card.left + sx(16), card.top + sx(16),
                 card.left + sx(56), card.top + sx(56)};
    draw_image(dc, &g_nav_images[0], &icon);

    wchar_t name[192], os[64], state[32], labels[512];
    utf8_to_wide(row->name, name, 192U);
    utf8_to_wide(row->os, os, 64U);
    utf8_to_wide(row->state, state, 32U);
    utf8_to_wide(row->labels, labels, 512U);

    RECT name_rect = {card.left + sx(66), card.top + sx(13),
                      card.right - sx(12), card.top + sx(38)};
    draw_text(dc, name, name_rect, g_font_bold, rgb(g_palette.title_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT os_rect = {card.left + sx(66), card.top + sx(37),
                    card.right - sx(12), card.top + sx(57)};
    draw_text(dc, os, os_rect, g_font_small, rgb(g_palette.muted_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT state_rect = {card.left + sx(16), card.top + sx(67),
                       card.left + sx(105), card.top + sx(93)};
    uint32_t accent = strcmp(row->state, "RUNNING") == 0 ? g_palette.success_rgb :
                      strcmp(row->state, "IDLE") == 0 ? g_palette.info_rgb :
                      g_palette.fault_rgb;
    fill_round_rect(dc, state_rect,
                    mix_color(g_palette.surface_rgb, accent, 18U),
                    state_color(row->state), sx(8));
    draw_text(dc, state, state_rect, g_font_bold, rgb(g_palette.text_rgb),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RunnerSession *session = session_for(row->name, false);
    wchar_t state_for[64] = L"—";
    wchar_t jobs[48] = L"Jobs 0";
    wchar_t busy[64] = L"Busy 0.0%";
    double fraction = 0.0;
    if (session) {
        format_duration(GetTickCount64() - session->state_since_ms, state_for, 64U);
        _snwprintf_s(jobs, 48U, _TRUNCATE, L"Jobs %u", session->jobs);
        fraction = session_busy_fraction(session);
        _snwprintf_s(busy, 64U, _TRUNCATE, L"Busy %.1f%%", fraction * 100.0);
    }
    RECT state_for_rect = {card.left + sx(116), card.top + sx(67),
                           card.right - sx(14), card.top + sx(91)};
    draw_text(dc, state_for, state_for_rect, g_font_small,
              rgb(g_palette.muted_rgb), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    RECT labels_rect = {card.left + sx(16), card.top + sx(99),
                        card.right - sx(16), card.top + sx(121)};
    draw_text(dc, labels[0] ? labels : L"No labels", labels_rect, g_font_small,
              rgb(g_palette.detail_label_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT bar = {card.left + sx(16), card.bottom - sx(31),
                card.right - sx(16), card.bottom - sx(22)};
    fill_round_rect(dc, bar, rgb(g_palette.input_rgb), rgb(g_palette.border_rgb), sx(5));
    RECT progress = bar;
    progress.right = progress.left + (int)((double)(bar.right - bar.left) * fraction);
    if (progress.right > progress.left)
        fill_round_rect(dc, progress, rgb(g_palette.operation_rgb),
                        rgb(g_palette.operation_rgb), sx(5));
    RECT jobs_rect = {card.left + sx(16), card.bottom - sx(20),
                      card.left + sx(105), card.bottom - sx(3)};
    RECT busy_rect = {card.right - sx(125), card.bottom - sx(20),
                      card.right - sx(16), card.bottom - sx(3)};
    draw_text(dc, jobs, jobs_rect, g_font_small, rgb(g_palette.muted_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(dc, busy, busy_rect, g_font_small, rgb(g_palette.muted_rgb),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static int card_columns(void)
{
    int width = g_cards_area_rect.right - g_cards_area_rect.left;
    int card_width = sx(248);
    int gap = sx(10);
    int columns = (width + gap) / (card_width + gap);
    if (columns < 1) columns = 1;
    if (columns > 5) columns = 5;
    return columns;
}

static RECT card_rect_for_visible_index(size_t index)
{
    int columns = card_columns();
    int gap = sx(10);
    int total_width = g_cards_area_rect.right - g_cards_area_rect.left;
    int card_width = (total_width - gap * (columns - 1)) / columns;
    int card_height = sx(154);
    int row = (int)(index / (size_t)columns);
    int column = (int)(index % (size_t)columns);
    int x = g_cards_area_rect.left + column * (card_width + gap);
    int y = g_cards_area_rect.top + row * (card_height + gap) - g_card_scroll_y;
    RECT rect;
    SetRect(&rect, x, y, x + card_width, y + card_height);
    return rect;
}

static int max_card_scroll(void)
{
    size_t count = visible_runner_count();
    int columns = card_columns();
    int rows = (int)((count + (size_t)columns - 1U) / (size_t)columns);
    int content_height = rows > 0 ? rows * sx(154) + (rows - 1) * sx(10) : 0;
    int viewport = g_cards_area_rect.bottom - g_cards_area_rect.top;
    return content_height > viewport ? content_height - viewport : 0;
}

static void draw_cards(HDC dc)
{
    int saved = SaveDC(dc);
    IntersectClipRect(dc, g_cards_area_rect.left, g_cards_area_rect.top,
                     g_cards_area_rect.right, g_cards_area_rect.bottom);
    if (g_page != 0) {
        fill_round_rect(dc, g_cards_area_rect, rgb(g_palette.surface_rgb),
                        rgb(g_palette.border_rgb), sx(10));
        RECT text = g_cards_area_rect;
        text.left += sx(30);
        text.right -= sx(30);
        const wchar_t *message =
            g_page == 1 ? L"Active Jobs uses the same Windows parity workspace." :
            g_page == 2 ? L"History uses the same Windows parity workspace." :
                          L"Local Windows health uses the same Windows parity workspace.";
        draw_text(dc, message, text, g_font_bold, rgb(g_palette.muted_rgb),
                  DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        RestoreDC(dc, saved);
        return;
    }

    size_t visible = visible_runner_count();
    if (visible == 0U) {
        fill_round_rect(dc, g_cards_area_rect, rgb(g_palette.surface_rgb),
                        rgb(g_palette.border_rgb), sx(10));
        RECT text = g_cards_area_rect;
        draw_text(dc, g_refreshing ? L"Loading runners…" : L"No runners match the current view.",
                  text, g_font_bold, rgb(g_palette.muted_rgb),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RestoreDC(dc, saved);
        return;
    }
    for (size_t i = 0U; i < visible; ++i) {
        size_t source_index = 0U;
        RunnerRow *row = visible_runner_at(i, &source_index);
        RECT card = card_rect_for_visible_index(i);
        if (card.bottom < g_cards_area_rect.top || card.top > g_cards_area_rect.bottom)
            continue;
        draw_runner_card(dc, row, source_index, card);
    }
    RestoreDC(dc, saved);
}

static void draw_selection(HDC dc)
{
    if (g_selected_row < 0 || g_selected_row >= (int)g_row_count || g_page != 0)
        return;
    RunnerRow *row = &g_rows[g_selected_row];
    fill_round_rect(dc, g_selection_rect,
                    mix_color(g_palette.surface_rgb, g_palette.neutral_accent_rgb, 8U),
                    rgb(g_palette.neutral_accent_rgb), sx(10));
    wchar_t name[192], labels[512];
    utf8_to_wide(row->name, name, 192U);
    utf8_to_wide(row->labels, labels, 512U);
    RECT title = {g_selection_rect.left + sx(16), g_selection_rect.top + sx(8),
                  g_selection_rect.right - sx(16), g_selection_rect.top + sx(32)};
    draw_text(dc, L"Selected runner", title, g_font_small,
              rgb(g_palette.kicker_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT primary = {title.left, g_selection_rect.top + sx(29),
                    title.right, g_selection_rect.top + sx(53)};
    draw_text(dc, name, primary, g_font_bold, rgb(g_palette.title_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT secondary = {title.left, g_selection_rect.top + sx(51),
                      title.right, g_selection_rect.bottom - sx(5)};
    draw_text(dc, labels[0] ? labels : L"No labels", secondary, g_font_small,
              rgb(g_palette.muted_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void draw_footer(HDC dc, RECT client)
{
    int footer_top = client.bottom - sx(96);
    RECT footer = {sx(20), footer_top, client.right - sx(20), client.bottom - sx(12)};
    fill_round_rect(dc, footer, rgb(g_palette.panel_rgb),
                    rgb(g_palette.connection_border_rgb), sx(12));
    draw_button(dc, g_export_rect, L"Export CSV", false, false);
    draw_button(dc, g_about_rect, L"About", false, false);
    draw_button(dc, g_refresh_rect, g_refreshing ? L"Refreshing…" : L"Refresh now",
                false, true);

    RECT status = {footer.left + sx(12), footer.top + sx(50),
                   footer.right - sx(340), footer.bottom - sx(8)};
    wchar_t status_text[256];
    if (g_refreshing)
        wcscpy_s(status_text, 256U, L"Refreshing GitHub runner state…");
    else if (!g_organisation[0])
        wcscpy_s(status_text, 256U, L"Open Settings and enter the GitHub organisation.");
    else
        _snwprintf_s(status_text, 256U, _TRUNCATE,
                     L"%zu runner%ls loaded from %ls.", g_row_count,
                     g_row_count == 1U ? L"" : L"s", g_organisation);
    draw_text(dc, status_text, status, g_font,
              rgb(g_palette.text_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    wchar_t version[128];
    _snwprintf_s(version, 128U, _TRUNCATE, L"Runner Monitor %hs", RUNNERSCOPE_VERSION);
    RECT app_version = {footer.right - sx(310), footer.top + sx(48),
                        footer.right - sx(12), footer.top + sx(67)};
    draw_text(dc, version, app_version, g_font_small, rgb(g_palette.muted_rgb),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    _snwprintf_s(version, 128U, _TRUNCATE, L"Common %hs", INFILTRATR_COMMON_VERSION);
    RECT common_version = {app_version.left, footer.top + sx(66),
                           app_version.right, footer.bottom - sx(5)};
    draw_text(dc, version, common_version, g_font_small, rgb(g_palette.muted_rgb),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void paint_ui(HDC dc)
{
    RECT client;
    GetClientRect(g_main, &client);
    fill_rect_color(dc, client, rgb(g_palette.background_rgb));
    draw_header(dc);

    RECT content = {sx(20), g_header_rect.bottom + sx(14),
                    client.right - sx(20), client.bottom - sx(110)};
    draw_navigation(dc, content);
    draw_workspace_header(dc);

    RECT toolbar = {g_workspace_rect.left, g_workspace_rect.top + sx(108),
                    g_workspace_rect.right, g_workspace_rect.top + sx(164)};
    fill_rect_color(dc, toolbar, rgb(g_palette.card_rgb));
    if (g_page == 0) {
        draw_button(dc, g_cards_toggle_rect, L"Cards", !g_table_view, false);
        draw_button(dc, g_table_toggle_rect, L"Table", g_table_view, false);
    }

    if (!g_table_view || g_page != 0) draw_cards(dc);
    draw_selection(dc);
    draw_footer(dc, client);
}

static void set_page(int page)
{
    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT || page == g_page) return;
    g_page = page;
    g_selected_row = -1;
    g_card_scroll_y = 0;
    ShowWindow(g_search, page == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_list, page == 0 && g_table_view ? SW_SHOW : SW_HIDE);
    update_layout(g_main);
    InvalidateRect(g_main, NULL, FALSE);
}

static void set_table_view(bool table)
{
    if (g_table_view == table) return;
    g_table_view = table;
    ShowWindow(g_list, g_page == 0 && table ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_main, NULL, FALSE);
}

static void select_card_at(POINT point)
{
    if (g_page != 0 || g_table_view) return;
    size_t visible = visible_runner_count();
    for (size_t i = 0U; i < visible; ++i) {
        RECT card = card_rect_for_visible_index(i);
        if (!pt_in_rect(&card, point)) continue;
        size_t source_index = 0U;
        if (visible_runner_at(i, &source_index)) {
            g_selected_row = (int)source_index;
            update_layout(g_main);
            InvalidateRect(g_main, NULL, FALSE);
        }
        return;
    }
}

static void csv_write_field(FILE *stream, const char *value)
{
    const char *text = value ? value : "";
    bool quoted = strpbrk(text, ",\"\r\n") != NULL;
    if (!quoted) {
        fputs(text, stream);
        return;
    }
    fputc('"', stream);
    for (const char *p = text; *p; ++p) {
        if (*p == '"') fputc('"', stream);
        fputc(*p, stream);
    }
    fputc('"', stream);
}

static void export_csv(void)
{
    wchar_t path[MAX_PATH] = L"runner-monitor.csv";
    OPENFILENAMEW dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"csv";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return;
    FILE *stream = NULL;
    if (_wfopen_s(&stream, path, L"wb") != 0 || !stream) return;
    fputs("Runner,OS,State,Repository,Current job,Runtime,State for,Jobs,Busy,Labels\r\n", stream);
    for (size_t i = 0U; i < g_row_count; ++i) {
        RunnerRow *row = &g_rows[i];
        RunnerSession *session = session_for(row->name, false);
        char state_for[64] = "";
        char runtime[64] = "";
        char jobs[32] = "0";
        char busy[32] = "0.0%";
        if (session) {
            wchar_t duration_w[64];
            format_duration(GetTickCount64() - session->state_since_ms, duration_w, 64U);
            wide_to_utf8(duration_w, state_for, sizeof(state_for));
            if (session->running) infiltratr_copy_string(runtime, sizeof(runtime), state_for);
            snprintf(jobs, sizeof(jobs), "%u", session->jobs);
            snprintf(busy, sizeof(busy), "%.1f%%", session_busy_fraction(session) * 100.0);
        }
        csv_write_field(stream, row->name); fputc(',', stream);
        csv_write_field(stream, row->os); fputc(',', stream);
        csv_write_field(stream, row->state); fputc(',', stream);
        csv_write_field(stream, ""); fputc(',', stream);
        csv_write_field(stream, ""); fputc(',', stream);
        csv_write_field(stream, runtime); fputc(',', stream);
        csv_write_field(stream, state_for); fputc(',', stream);
        csv_write_field(stream, jobs); fputc(',', stream);
        csv_write_field(stream, busy); fputc(',', stream);
        csv_write_field(stream, row->labels); fputs("\r\n", stream);
    }
    fclose(stream);
}

static void show_about(void)
{
    wchar_t text[512];
    _snwprintf_s(text, 512U, _TRUNCATE,
                 L"Runner Monitor %hs\r\nInfiltrator OS\r\nCommon %hs\r\n\r\n"
                 L"Native Windows presentation aligned with the Linux Runner Monitor shell.",
                 RUNNERSCOPE_VERSION, INFILTRATR_COMMON_VERSION);
    MessageBoxW(g_main, text, L"About Runner Monitor", MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK settings_proc(HWND window, UINT message,
                                      WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    switch (message) {
    case WM_CREATE: {
        RECT owner;
        GetWindowRect(g_main, &owner);
        int width = sx(520), height = sx(210);
        SetWindowPos(window, HWND_TOP,
                     owner.left + ((owner.right - owner.left) - width) / 2,
                     owner.top + ((owner.bottom - owner.top) - height) / 2,
                     width, height, SWP_SHOWWINDOW);
        HWND title = CreateWindowW(L"STATIC", L"Runner Monitor settings",
            WS_CHILD | WS_VISIBLE, sx(20), sx(18), sx(430), sx(30),
            window, NULL, g_instance, NULL);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_font_title, TRUE);
        HWND label = CreateWindowW(L"STATIC", L"GitHub organisation",
            WS_CHILD | WS_VISIBLE, sx(20), sx(62), sx(180), sx(24),
            window, NULL, g_instance, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)g_font, TRUE);
        g_settings_org = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_organisation,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            sx(20), sx(88), sx(460), sx(31),
            window, (HMENU)(INT_PTR)ID_SETTINGS_ORG, g_instance, NULL);
        SendMessageW(g_settings_org, WM_SETFONT, (WPARAM)g_font, TRUE);
        HWND save = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE,
            sx(300), sx(140), sx(86), sx(32),
            window, (HMENU)(INT_PTR)ID_SETTINGS_SAVE, g_instance, NULL);
        HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
            sx(394), sx(140), sx(86), sx(32),
            window, (HMENU)(INT_PTR)ID_SETTINGS_CANCEL, g_instance, NULL);
        SendMessageW(save, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
        SendMessageW(cancel, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_SETTINGS_SAVE) {
            GetWindowTextW(g_settings_org, g_organisation,
                           (int)(sizeof(g_organisation) / sizeof(g_organisation[0])));
            if (!g_organisation[0]) {
                MessageBoxW(window, L"Enter a GitHub organisation.", APP_TITLE,
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (!save_organisation()) {
                MessageBoxW(window, L"Could not save the Runner Monitor configuration.",
                            APP_TITLE, MB_OK | MB_ICONERROR);
                return 0;
            }
            DestroyWindow(window);
            refresh_begin();
            return 0;
        }
        if (LOWORD(wparam) == ID_SETTINGS_CANCEL) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, rgb(g_palette.text_rgb));
        return (LRESULT)g_background_brush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wparam;
        SetBkColor(dc, rgb(g_palette.input_rgb));
        SetTextColor(dc, rgb(g_palette.text_rgb));
        return (LRESULT)g_input_brush;
    }
    case WM_ERASEBKGND: {
        RECT rect;
        GetClientRect(window, &rect);
        FillRect((HDC)wparam, &rect, g_background_brush);
        return 1;
    }
    case WM_DESTROY:
        g_settings_window = NULL;
        g_settings_org = NULL;
        EnableWindow(g_main, TRUE);
        SetForegroundWindow(g_main);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void open_settings(void)
{
    if (g_settings_window) {
        SetForegroundWindow(g_settings_window);
        return;
    }
    EnableWindow(g_main, FALSE);
    g_settings_window = CreateWindowExW(
        WS_EX_DLGMODALFRAME, SETTINGS_CLASS, L"Runner Monitor settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, sx(520), sx(210),
        g_main, NULL, g_instance, NULL);
    if (!g_settings_window) EnableWindow(g_main, TRUE);
}

static void create_fonts(void)
{
    const InfiltratrTypography *typography = infiltratr_typography();
    wchar_t ui_family[96] = L"Segoe UI";
    wchar_t brand_family[96] = L"Segoe UI";
    if (typography) {
        if (typography->ui_family) utf8_to_wide(typography->ui_family, ui_family, 96U);
        if (typography->brand_family) utf8_to_wide(typography->brand_family, brand_family, 96U);
    }
    g_font = CreateFontW(-sx(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_family);
    g_font_small = CreateFontW(-sx(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_family);
    g_font_bold = CreateFontW(-sx(16), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_family);
    g_font_title = CreateFontW(-sx(25), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, brand_family);
    g_font_brand = CreateFontW(-sx(22), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, brand_family);
}

static void delete_fonts(void)
{
    if (g_font) DeleteObject(g_font);
    if (g_font_small) DeleteObject(g_font_small);
    if (g_font_bold) DeleteObject(g_font_bold);
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_brand) DeleteObject(g_font_brand);
    g_font = g_font_small = g_font_bold = g_font_title = g_font_brand = NULL;
}

static void init_list(void)
{
    g_list = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, g_main, (HMENU)(INT_PTR)ID_RUNNERS, g_instance, NULL);
    ListView_SetExtendedListViewStyle(g_list,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    ListView_SetBkColor(g_list, rgb(g_palette.surface_rgb));
    ListView_SetTextBkColor(g_list, rgb(g_palette.surface_rgb));
    ListView_SetTextColor(g_list, rgb(g_palette.text_rgb));
    SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font, TRUE);
    SetWindowTheme(g_list, system_prefers_dark() ? L"DarkMode_Explorer" : L"Explorer", NULL);
    HWND header = ListView_GetHeader(g_list);
    if (header)
        SetWindowTheme(header, system_prefers_dark() ? L"DarkMode_ItemsView" : L"Explorer", NULL);
    list_add_column(0, 165, L"Runner");
    list_add_column(1, 55, L"OS");
    list_add_column(2, 70, L"State");
    list_add_column(3, 120, L"Repository");
    list_add_column(4, 180, L"Current job");
    list_add_column(5, 72, L"Runtime");
    list_add_column(6, 72, L"State for");
    list_add_column(7, 42, L"Jobs");
    list_add_column(8, 58, L"Busy");
    list_add_column(9, 160, L"Labels");
}

static void create_search(void)
{
    g_search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, sx(250), sx(34), g_main,
        (HMENU)(INT_PTR)ID_SEARCH, g_instance, NULL);
    SendMessageW(g_search, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageW(g_search, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter current view…");
}

static LRESULT hit_test_non_client(HWND window, LPARAM lparam)
{
    POINT screen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    POINT point = screen;
    ScreenToClient(window, &point);
    RECT client;
    GetClientRect(window, &client);
    int border = sx(7);
    bool left = point.x < border;
    bool right = point.x >= client.right - border;
    bool top = point.y < border;
    bool bottom = point.y >= client.bottom - border;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    if (point.y < g_header_rect.bottom &&
        !pt_in_rect(&g_settings_rect, point) &&
        !pt_in_rect(&g_minimize_rect, point) &&
        !pt_in_rect(&g_maximize_rect, point) &&
        !pt_in_rect(&g_close_rect, point))
        return HTCAPTION;
    return HTCLIENT;
}

static void handle_click(POINT point)
{
    if (pt_in_rect(&g_settings_rect, point)) { open_settings(); return; }
    if (pt_in_rect(&g_minimize_rect, point)) { ShowWindow(g_main, SW_MINIMIZE); return; }
    if (pt_in_rect(&g_maximize_rect, point)) {
        ShowWindow(g_main, IsZoomed(g_main) ? SW_RESTORE : SW_MAXIMIZE); return;
    }
    if (pt_in_rect(&g_close_rect, point)) { PostMessageW(g_main, WM_CLOSE, 0, 0); return; }
    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        if (pt_in_rect(&g_nav_rects[i], point)) { set_page(i); return; }
    }
    if (g_page == 0 && pt_in_rect(&g_cards_toggle_rect, point)) { set_table_view(false); return; }
    if (g_page == 0 && pt_in_rect(&g_table_toggle_rect, point)) { set_table_view(true); return; }
    if (pt_in_rect(&g_export_rect, point)) { export_csv(); return; }
    if (pt_in_rect(&g_about_rect, point)) { show_about(); return; }
    if (pt_in_rect(&g_refresh_rect, point)) { refresh_begin(); return; }
    select_card_at(point);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        g_main = window;
        g_dpi = GetDpiForWindow(window);
        if (g_dpi == 0U) g_dpi = 96U;
        resolve_theme();
        create_brushes();
        load_private_fonts();
        create_fonts();
        HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                                      CLSCTX_INPROC_SERVER,
                                      &IID_IWICImagingFactory,
                                      (void **)&g_wic);
        if (FAILED(hr)) g_wic = NULL;
        load_images();
        create_search();
        init_list();
        g_session_started_ms = GetTickCount64();
        BOOL dark = system_prefers_dark() ? TRUE : FALSE;
        const DWORD attribute = 20U;
        DwmSetWindowAttribute(window, attribute, &dark, sizeof(dark));
        SetTimer(window, TIMER_REFRESH, 3000U, NULL);
        SetTimer(window, TIMER_PRESENT, 1000U, NULL);
        update_layout(window);
        if (g_organisation[0]) refresh_begin();
        else PostMessageW(window, WM_APP_OPEN_SETTINGS, 0, 0);
        return 0;
    }
    case WM_APP_OPEN_SETTINGS:
        open_settings();
        return 0;
    case WM_NCCALCSIZE:
        if (wparam) return 0;
        break;
    case WM_NCHITTEST:
        return hit_test_non_client(window, lparam);
    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)lparam;
        info->ptMinTrackSize.x = sx(980);
        info->ptMinTrackSize.y = sx(660);
        return 0;
    }
    case WM_DPICHANGED: {
        g_dpi = HIWORD(wparam);
        if (g_dpi == 0U) g_dpi = 96U;
        delete_fonts();
        create_fonts();
        SendMessageW(g_search, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font, TRUE);
        RECT *suggested = (RECT *)lparam;
        SetWindowPos(window, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        update_layout(window);
        InvalidateRect(window, NULL, TRUE);
        return 0;
    }
    case WM_SIZE:
        update_layout(window);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_TIMER:
        if (wparam == TIMER_REFRESH) {
            if (!g_refreshing && g_organisation[0]) refresh_begin();
            return 0;
        }
        if (wparam == TIMER_PRESENT) {
            if (g_table_view) rebuild_table();
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_SEARCH && HIWORD(wparam) == EN_CHANGE) {
            GetWindowTextW(g_search, g_filter,
                           (int)(sizeof(g_filter) / sizeof(g_filter[0])));
            g_card_scroll_y = 0;
            g_selected_row = -1;
            rebuild_table();
            update_layout(window);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_NOTIFY:
        if (((NMHDR *)lparam)->idFrom == ID_RUNNERS &&
            ((NMHDR *)lparam)->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *change = (NMLISTVIEW *)lparam;
            if ((change->uNewState & LVIS_SELECTED) != 0U) {
                LVITEMW item;
                ZeroMemory(&item, sizeof(item));
                item.mask = LVIF_PARAM;
                item.iItem = change->iItem;
                if (ListView_GetItem(g_list, &item)) {
                    g_selected_row = (int)item.lParam;
                    update_layout(window);
                    InvalidateRect(window, NULL, FALSE);
                }
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        handle_click(point);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (point.y < g_header_rect.bottom &&
            !pt_in_rect(&g_settings_rect, point) &&
            !pt_in_rect(&g_minimize_rect, point) &&
            !pt_in_rect(&g_maximize_rect, point) &&
            !pt_in_rect(&g_close_rect, point)) {
            ShowWindow(window, IsZoomed(window) ? SW_RESTORE : SW_MAXIMIZE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (g_page == 0 && !g_table_view) {
            int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            g_card_scroll_y -= (delta / WHEEL_DELTA) * sx(60);
            if (g_card_scroll_y < 0) g_card_scroll_y = 0;
            int maximum = max_card_scroll();
            if (g_card_scroll_y > maximum) g_card_scroll_y = maximum;
            InvalidateRect(window, &g_cards_area_rect, FALSE);
            return 0;
        }
        break;
    case WM_APP_REFRESH_DONE:
        apply_refresh((RefreshResult *)lparam);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, rgb(g_palette.text_rgb));
        return (LRESULT)g_background_brush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wparam;
        SetBkColor(dc, rgb(g_palette.input_rgb));
        SetTextColor(dc, rgb(g_palette.text_rgb));
        return (LRESULT)g_input_brush;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        HDC memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc,
            client.right - client.left, client.bottom - client.top);
        HGDIOBJ previous = SelectObject(memory, bitmap);
        paint_ui(memory);
        BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window, TIMER_REFRESH);
        KillTimer(window, TIMER_PRESENT);
        unload_images();
        if (g_wic) IWICImagingFactory_Release(g_wic);
        g_wic = NULL;
        delete_fonts();
        unload_private_fonts();
        delete_brushes();
        free(g_rows);
        free(g_sessions);
        g_rows = NULL;
        g_sessions = NULL;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static bool resource_exists(int resource_id)
{
    return FindResourceW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(resource_id),
                         RT_RCDATA) != NULL;
}

static int self_test(void)
{
    char ui_error[256];
    if (!runner_ui_contract_validate(ui_error, sizeof(ui_error))) return 1;
    const InfiltratrThemePalette *day =
        infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false);
    const InfiltratrThemePalette *night =
        infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true);
    if (!day || !night) return 2;
    if (day->background_rgb == night->background_rgb) return 3;
    if (!INFILTRATR_COMMON_VERSION[0] || !RUNNERSCOPE_VERSION[0]) return 4;
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const InfiltratrTypography *type = infiltratr_typography();
    if (!metrics || !type || !type->ui_family || !type->brand_family) return 5;
    const int required[] = {
        IDR_NAV_RUNNERS, IDR_NAV_ACTIVE, IDR_NAV_HISTORY, IDR_NAV_HEALTH,
        IDR_NAV_INFILTRATOR, IDR_HERO_RUNNERS, IDR_HERO_ACTIVE,
        IDR_HERO_HISTORY, IDR_HERO_HEALTH, IDR_FONT_BRAND,
        IDR_FONT_UI_BOLD, IDR_FONT_UI_REGULAR
    };
    for (size_t i = 0U; i < sizeof(required) / sizeof(required[0]); ++i)
        if (!resource_exists(required[i])) return 6;
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous,
                    PWSTR command_line, int show_command)
{
    (void)previous;
    (void)command_line;

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 2 && wcscmp(argv[1], L"--self-test") == 0) {
        g_instance = instance;
        int status = self_test();
        LocalFree(argv);
        return status;
    }
    if (argv) LocalFree(argv);

    g_instance = instance;
    load_organisation();
    (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com_status = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX controls = {
        sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES
    };
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
    wc.hbrBackground = NULL;
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&wc)) return 1;

    WNDCLASSEXW settings_wc;
    ZeroMemory(&settings_wc, sizeof(settings_wc));
    settings_wc.cbSize = sizeof(settings_wc);
    settings_wc.lpfnWndProc = settings_proc;
    settings_wc.hInstance = instance;
    settings_wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    settings_wc.hbrBackground = NULL;
    settings_wc.lpszClassName = SETTINGS_CLASS;
    if (!RegisterClassExW(&settings_wc)) return 1;

    g_main = CreateWindowExW(WS_EX_APPWINDOW, APP_CLASS, APP_TITLE,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
        WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, sx(1360), sx(820),
        NULL, NULL, instance, NULL);
    if (!g_main) return 1;

    ShowWindow(g_main, show_command);
    UpdateWindow(g_main);

    MSG message;
    while (GetMessageW(&message, NULL, 0U, 0U) > 0) {
        if (g_settings_window && IsDialogMessageW(g_settings_window, &message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (SUCCEEDED(com_status)) CoUninitialize();
    return (int)message.wParam;
}
