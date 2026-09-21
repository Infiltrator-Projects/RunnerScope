// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/escape.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RUNNERSCOPE_VERSION
#error "RUNNERSCOPE_VERSION must be provided by CMake"
#endif

#define APP_CLASS L"InfiltratrRunnerMonitor"
#define APP_TITLE L"Runner Monitor"
#define WM_APP_REFRESH_DONE (WM_APP + 1)

enum {
    ID_ORG = 1001,
    ID_SAVE = 1002,
    ID_REFRESH = 1003,
    ID_RUNNERS = 1100
};

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

static HINSTANCE g_instance;
static HWND g_main;
static HWND g_org_edit;
static HWND g_status;
static HWND g_summary;
static HWND g_list;
static HWND g_footer_app;
static HWND g_footer_common;
static HFONT g_font;
static HFONT g_bold;
static HBRUSH g_background_brush;
static HBRUSH g_panel_brush;
static InfiltratrThemePalette g_palette;
static wchar_t g_organisation[128];

static COLORREF rgb(uint32_t value)
{
    return RGB((BYTE)((value >> 16U) & 0xffU),
               (BYTE)((value >> 8U) & 0xffU),
               (BYTE)(value & 0xffU));
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
    if (g_panel_brush) DeleteObject(g_panel_brush);
    g_background_brush = NULL;
    g_panel_brush = NULL;
}

static void create_brushes(void)
{
    delete_brushes();
    g_background_brush = CreateSolidBrush(rgb(g_palette.background_rgb));
    g_panel_brush = CreateSolidBrush(rgb(g_palette.panel_rgb));
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
        "  \"runner_poll_seconds\": 2,\r\n"
        "  \"activity_scan_seconds\": 45,\r\n"
        "  \"repository_scan_limit\": 25,\r\n"
        "  \"repository_cache_seconds\": 300,\r\n"
        "  \"history_entries\": 300,\r\n"
        "  \"local_health_seconds\": 10,\r\n"
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
                 "Enter the GitHub organisation and click Save.");
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
    PostMessageW(window, WM_APP_REFRESH_DONE, 0, (LPARAM)result);
    return 0U;
}

static void list_add_column(int index, int width, const wchar_t *title)
{
    LVCOLUMNW column;
    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = (LPWSTR)title;
    column.cx = width;
    column.iSubItem = index;
    ListView_InsertColumn(g_list, index, &column);
}

static void refresh_begin(void)
{
    GetWindowTextW(g_org_edit, g_organisation,
                   (int)(sizeof(g_organisation) / sizeof(g_organisation[0])));
    SetWindowTextW(g_status, L"Refreshing GitHub runner state…");
    EnableWindow(GetDlgItem(g_main, ID_REFRESH), FALSE);
    HANDLE thread = CreateThread(NULL, 0U, refresh_worker, g_main, 0U, NULL);
    if (thread) CloseHandle(thread);
}

static void apply_refresh(RefreshResult *result)
{
    ListView_DeleteAllItems(g_list);
    size_t running = 0U, idle = 0U, offline = 0U;
    if (result && !result->error[0]) {
        for (size_t i = 0U; i < result->count; ++i) {
            RunnerRow *row = &result->rows[i];
            wchar_t name[192], os[64], state[32], labels[512];
            utf8_to_wide(row->name, name, 192U);
            utf8_to_wide(row->os, os, 64U);
            utf8_to_wide(row->state, state, 32U);
            utf8_to_wide(row->labels, labels, 512U);
            LVITEMW item;
            ZeroMemory(&item, sizeof(item));
            item.mask = LVIF_TEXT;
            item.iItem = (int)i;
            item.pszText = name;
            int inserted = ListView_InsertItem(g_list, &item);
            ListView_SetItemText(g_list, inserted, 1, os);
            ListView_SetItemText(g_list, inserted, 2, state);
            ListView_SetItemText(g_list, inserted, 3, L"—");
            ListView_SetItemText(g_list, inserted, 4, L"—");
            ListView_SetItemText(g_list, inserted, 5, L"—");
            ListView_SetItemText(g_list, inserted, 6, L"—");
            ListView_SetItemText(g_list, inserted, 7, L"—");
            ListView_SetItemText(g_list, inserted, 8, L"—");
            ListView_SetItemText(g_list, inserted, 9, labels);
            if (strcmp(row->state, "RUNNING") == 0) running++;
            else if (strcmp(row->state, "IDLE") == 0) idle++;
            else offline++;
        }
        wchar_t summary[256];
        _snwprintf_s(summary, 256U, _TRUNCATE,
                     L"TOTAL  %zu     RUNNING  %zu     IDLE  %zu     OFFLINE  %zu",
                     result->count, running, idle, offline);
        SetWindowTextW(g_summary, summary);
        SetWindowTextW(g_status, L"Runner state refreshed.");
    } else if (result) {
        wchar_t error[512];
        utf8_to_wide(result->error, error, 512U);
        SetWindowTextW(g_status, error);
    }
    EnableWindow(GetDlgItem(g_main, ID_REFRESH), TRUE);
    if (result) {
        free(result->rows);
        free(result);
    }
}

static void layout(HWND window)
{
    RECT client;
    GetClientRect(window, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    const int margin = 18;
    const int row = 30;
    MoveWindow(g_org_edit, margin + 100, 64, 300, 28, TRUE);
    MoveWindow(GetDlgItem(window, ID_SAVE), margin + 410, 64, 80, 28, TRUE);
    MoveWindow(GetDlgItem(window, ID_REFRESH), margin + 500, 64, 100, 28, TRUE);
    MoveWindow(g_summary, margin, 104, width - margin * 2, row, TRUE);
    MoveWindow(g_list, margin, 142, width - margin * 2, height - 230, TRUE);
    MoveWindow(g_status, margin, height - 76, width - 360, row, TRUE);
    MoveWindow(g_footer_app, width - 330, height - 76, 310, 22, TRUE);
    MoveWindow(g_footer_common, width - 330, height - 52, 310, 22, TRUE);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        resolve_theme();
        create_brushes();
        const InfiltratrTypography *typography = infiltratr_typography();
        const wchar_t *family = L"Segoe UI";
        wchar_t common_family[96];
        if (typography && typography->windows_fallback) {
            utf8_to_wide(typography->windows_fallback, common_family, 96U);
            if (common_family[0]) family = common_family;
        }
        g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, family);
        g_bold = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, family);

        HWND title = CreateWindowW(L"STATIC", L"Runner Monitor",
            WS_CHILD | WS_VISIBLE, 18, 16, 500, 34, window, NULL, g_instance, NULL);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_bold, TRUE);

        HWND org_label = CreateWindowW(L"STATIC", L"Organisation:",
            WS_CHILD | WS_VISIBLE, 18, 68, 100, 24, window, NULL, g_instance, NULL);
        SendMessageW(org_label, WM_SETFONT, (WPARAM)g_font, TRUE);
        g_org_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_organisation,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 118, 64, 300, 28,
            window, (HMENU)(INT_PTR)ID_ORG, g_instance, NULL);
        SendMessageW(g_org_edit, WM_SETFONT, (WPARAM)g_font, TRUE);

        HWND save = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE,
            428, 64, 80, 28, window, (HMENU)(INT_PTR)ID_SAVE, g_instance, NULL);
        HWND refresh = CreateWindowW(L"BUTTON", L"Refresh",
            WS_CHILD | WS_VISIBLE, 518, 64, 100, 28,
            window, (HMENU)(INT_PTR)ID_REFRESH, g_instance, NULL);
        SendMessageW(save, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(refresh, WM_SETFONT, (WPARAM)g_font, TRUE);

        g_summary = CreateWindowW(L"STATIC",
            L"TOTAL  0     RUNNING  0     IDLE  0     OFFLINE  0",
            WS_CHILD | WS_VISIBLE, 18, 104, 900, 28,
            window, NULL, g_instance, NULL);
        SendMessageW(g_summary, WM_SETFONT, (WPARAM)g_font, TRUE);

        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            18, 142, 1200, 500, window, (HMENU)(INT_PTR)ID_RUNNERS,
            g_instance, NULL);
        ListView_SetExtendedListViewStyle(g_list,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        ListView_SetBkColor(g_list, rgb(g_palette.surface_rgb));
        ListView_SetTextBkColor(g_list, rgb(g_palette.surface_rgb));
        ListView_SetTextColor(g_list, rgb(g_palette.text_rgb));
        SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font, TRUE);
        list_add_column(0, 180, L"Runner");
        list_add_column(1, 70, L"OS");
        list_add_column(2, 95, L"State");
        list_add_column(3, 145, L"Repository");
        list_add_column(4, 230, L"Current job");
        list_add_column(5, 90, L"Runtime");
        list_add_column(6, 90, L"State for");
        list_add_column(7, 55, L"Jobs");
        list_add_column(8, 65, L"Busy");
        list_add_column(9, 280, L"Labels");

        g_status = CreateWindowW(L"STATIC", L"Ready.",
            WS_CHILD | WS_VISIBLE, 18, 660, 700, 26,
            window, NULL, g_instance, NULL);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)g_font, TRUE);

        wchar_t footer[128];
        _snwprintf_s(footer, 128U, _TRUNCATE, L"Runner Monitor %hs",
                     RUNNERSCOPE_VERSION);
        g_footer_app = CreateWindowW(L"STATIC", footer,
            WS_CHILD | WS_VISIBLE | SS_RIGHT, 900, 660, 300, 22,
            window, NULL, g_instance, NULL);
        _snwprintf_s(footer, 128U, _TRUNCATE, L"Common %hs",
                     INFILTRATR_COMMON_VERSION);
        g_footer_common = CreateWindowW(L"STATIC", footer,
            WS_CHILD | WS_VISIBLE | SS_RIGHT, 900, 684, 300, 22,
            window, NULL, g_instance, NULL);
        SendMessageW(g_footer_app, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_footer_common, WM_SETFONT, (WPARAM)g_font, TRUE);

        layout(window);
        if (g_organisation[0]) refresh_begin();
        return 0;
    }
    case WM_SIZE:
        layout(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_SAVE:
            GetWindowTextW(g_org_edit, g_organisation,
                           (int)(sizeof(g_organisation) /
                                 sizeof(g_organisation[0])));
            SetWindowTextW(g_status,
                save_organisation() ? L"Organisation saved."
                                    : L"Could not save configuration.");
            return 0;
        case ID_REFRESH:
            refresh_begin();
            return 0;
        default:
            break;
        }
        break;
    case WM_APP_REFRESH_DONE:
        apply_refresh((RefreshResult *)lparam);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        SetTextColor(dc, rgb(g_palette.text_rgb));
        SetBkColor(dc, rgb(g_palette.background_rgb));
        return (LRESULT)g_background_brush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wparam;
        SetTextColor(dc, rgb(g_palette.text_rgb));
        SetBkColor(dc, rgb(g_palette.input_rgb));
        return (LRESULT)g_panel_brush;
    }
    case WM_ERASEBKGND: {
        RECT rect;
        GetClientRect(window, &rect);
        FillRect((HDC)wparam, &rect, g_background_brush);
        return 1;
    }
    case WM_DESTROY:
        delete_brushes();
        if (g_font) DeleteObject(g_font);
        if (g_bold) DeleteObject(g_bold);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static int self_test(void)
{
    const InfiltratrThemePalette *day =
        infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false);
    const InfiltratrThemePalette *night =
        infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true);
    if (!day || !night) return 2;
    if (day->background_rgb == night->background_rgb) return 3;
    if (!INFILTRATR_COMMON_VERSION[0] || !RUNNERSCOPE_VERSION[0]) return 4;
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
        LocalFree(argv);
        return self_test();
    }
    if (argv) LocalFree(argv);

    g_instance = instance;
    load_organisation();

    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hbrBackground = NULL;
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&wc)) return 1;

    g_main = CreateWindowExW(0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1480, 780,
        NULL, NULL, instance, NULL);
    if (!g_main) return 1;

    ShowWindow(g_main, show_command);
    UpdateWindow(g_main);

    MSG message;
    while (GetMessageW(&message, NULL, 0U, 0U) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
