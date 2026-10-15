// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/windows_config_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (!error || error_size == 0U) return;
    (void)snprintf(error, error_size, "%s", message ? message : "configuration error");
}

static void set_windows_error(char *error, size_t error_size,
                              const char *prefix, DWORD failure)
{
    if (!error || error_size == 0U) return;
    (void)snprintf(error, error_size, "%s (Windows error %lu).",
                   prefix ? prefix : "Configuration operation failed",
                   (unsigned long)failure);
}

static wchar_t *config_path(void)
{
    wchar_t appdata[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (length == 0U || length >= MAX_PATH)
        length = GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, MAX_PATH);
    if (length == 0U || length >= MAX_PATH) return NULL;

    const wchar_t *suffix = L"\\RunnerScope\\config.json";
    const size_t required = wcslen(appdata) + wcslen(suffix) + 1U;
    wchar_t *path = (wchar_t *)calloc(required, sizeof(wchar_t));
    if (!path) return NULL;
    wcscpy_s(path, required, appdata);
    wcscat_s(path, required, suffix);
    return path;
}

static bool wide_to_utf8(const wchar_t *input, char *output, size_t count)
{
    if (!output || count == 0U) return false;
    output[0] = '\0';
    if (!input) return true;
    const int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           input, -1, NULL, 0, NULL, NULL);
    if (needed <= 0 || (size_t)needed > count) return false;
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                               input, -1, output, (int)count, NULL, NULL) > 0;
}

bool runner_windows_config_load(RunnerConfig *config,
                                char *error, size_t error_size)
{
    if (!config) {
        set_error(error, error_size, "Configuration destination is missing.");
        return false;
    }
    runner_config_defaults(config);
    if (error && error_size) error[0] = '\0';

    wchar_t *path = config_path();
    if (!path) {
        set_error(error, error_size,
                  "Unable to resolve the Runner Monitor configuration path.");
        return false;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD failure = GetLastError();
        free(path);
        if (failure == ERROR_FILE_NOT_FOUND || failure == ERROR_PATH_NOT_FOUND)
            return true;
        set_windows_error(error, error_size, "Unable to open configuration", failure);
        return false;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        free(path);
        set_error(error, error_size,
                  "Configuration is empty or unreasonably large.");
        return false;
    }

    char *buffer = (char *)calloc((size_t)size.QuadPart + 1U, 1U);
    if (!buffer) {
        CloseHandle(file);
        free(path);
        set_error(error, error_size, "Unable to allocate configuration buffer.");
        return false;
    }

    DWORD amount = 0U;
    const BOOL ok = ReadFile(file, buffer, (DWORD)size.QuadPart, &amount, NULL);
    CloseHandle(file);
    free(path);
    if (!ok || amount != (DWORD)size.QuadPart) {
        free(buffer);
        set_error(error, error_size, "Unable to read the complete configuration.");
        return false;
    }

    const bool parsed = runner_config_parse_json(
        buffer, config, error, error_size);
    free(buffer);
    return parsed;
}

bool runner_windows_config_save(const RunnerConfig *config,
                                char *error, size_t error_size)
{
    char json[2048];
    size_t json_length = 0U;
    if (!runner_config_write_json(config, json, sizeof(json), &json_length,
                                  error, error_size))
        return false;

    wchar_t *path = config_path();
    if (!path) {
        set_error(error, error_size,
                  "Unable to resolve the Runner Monitor configuration path.");
        return false;
    }

    wchar_t directory[MAX_PATH];
    wcsncpy_s(directory, MAX_PATH, path, _TRUNCATE);
    wchar_t *slash = wcsrchr(directory, L'\\');
    if (!slash) {
        free(path);
        set_error(error, error_size, "Configuration directory is invalid.");
        return false;
    }
    *slash = L'\0';
    if (!CreateDirectoryW(directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        const DWORD failure = GetLastError();
        free(path);
        set_windows_error(error, error_size,
                          "Unable to create configuration directory", failure);
        return false;
    }

    wchar_t temporary[MAX_PATH];
    _snwprintf_s(temporary, MAX_PATH, _TRUNCATE, L"%ls.tmp", path);
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD failure = GetLastError();
        free(path);
        set_windows_error(error, error_size,
                          "Unable to create temporary configuration", failure);
        return false;
    }

    DWORD written = 0U;
    BOOL ok = WriteFile(file, json, (DWORD)json_length, &written, NULL);
    if (ok) ok = FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || written != (DWORD)json_length) {
        DeleteFileW(temporary);
        free(path);
        set_error(error, error_size,
                  "Unable to publish the complete configuration.");
        return false;
    }

    ok = MoveFileExW(temporary, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        const DWORD failure = GetLastError();
        DeleteFileW(temporary);
        free(path);
        set_windows_error(error, error_size,
                          "Unable to replace configuration", failure);
        return false;
    }
    free(path);
    if (error && error_size) error[0] = '\0';
    return true;
}

void runner_windows_config_apply_environment(RunnerConfig *config)
{
    if (!config) return;
    wchar_t wide[128];
    const DWORD amount = GetEnvironmentVariableW(L"GITHUB_RUNNER_ORG", wide, 128U);
    if (amount == 0U || amount >= 128U) return;

    RunnerConfig candidate = *config;
    if (!wide_to_utf8(wide, candidate.organisation, sizeof(candidate.organisation)))
        return;

    /*
     * Environment configuration is untrusted input just like persisted state.
     * Accept only an organisation value that leaves the complete configuration
     * valid; this also prevents command-line metacharacters from reaching the
     * GitHub CLI process adapter.
     */
    if (runner_config_validate(&candidate, NULL, 0U))
        *config = candidate;
}
