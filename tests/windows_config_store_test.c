// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/windows_config_store.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int fail(const char *message, const char *detail, int code)
{
    fprintf(stderr, "%s%s%s\n", message, detail && *detail ? ": " : "",
            detail && *detail ? detail : "");
    return code;
}

int main(void)
{
    wchar_t temp[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp)) return fail("GetTempPathW failed", NULL, 1);
    wchar_t root[MAX_PATH];
    _snwprintf_s(root, MAX_PATH, _TRUNCATE,
                 L"%lsRunnerScope-config-test-%lu", temp,
                 (unsigned long)GetCurrentProcessId());
    if (!CreateDirectoryW(root, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return fail("test directory creation failed", NULL, 2);
    if (!SetEnvironmentVariableW(L"APPDATA", root))
        return fail("APPDATA override failed", NULL, 3);

    RunnerConfig config;
    runner_config_defaults(&config);
    (void)snprintf(config.organisation, sizeof(config.organisation), "%s", "Infiltrator-Projects");
    config.expected_runners = 9U;
    config.runner_poll_seconds = 7U;
    config.activity_scan_seconds = 80U;
    config.repository_scan_limit = 31U;
    config.local_health_seconds = 22U;
    config.theme_mode = RUNNER_THEME_NIGHT;

    char error[256];
    if (!runner_windows_config_save(&config, error, sizeof(error)))
        return fail("save failed", error, 4);

    RunnerConfig loaded;
    if (!runner_windows_config_load(&loaded, error, sizeof(error)))
        return fail("load failed", error, 5);
    if (strcmp(loaded.organisation, config.organisation) != 0 ||
        loaded.expected_runners != config.expected_runners ||
        loaded.runner_poll_seconds != config.runner_poll_seconds ||
        loaded.activity_scan_seconds != config.activity_scan_seconds ||
        loaded.repository_scan_limit != config.repository_scan_limit ||
        loaded.local_health_seconds != config.local_health_seconds ||
        loaded.theme_mode != config.theme_mode)
        return fail("roundtrip changed configuration", NULL, 6);

    if (!SetEnvironmentVariableW(L"GITHUB_RUNNER_ORG", L"Override-Org"))
        return fail("valid environment override failed", NULL, 7);
    runner_windows_config_apply_environment(&loaded);
    if (strcmp(loaded.organisation, "Override-Org") != 0)
        return fail("valid environment override was ignored", NULL, 8);
    if (!SetEnvironmentVariableW(L"GITHUB_RUNNER_ORG", L"bad org\" --jq ."))
        return fail("invalid environment fixture failed", NULL, 9);
    runner_windows_config_apply_environment(&loaded);
    if (strcmp(loaded.organisation, "Override-Org") != 0)
        return fail("invalid environment override was accepted", NULL, 10);
    (void)SetEnvironmentVariableW(L"GITHUB_RUNNER_ORG", NULL);

    wchar_t path[MAX_PATH];
    _snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%ls\\RunnerScope\\config.json", root);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return fail("malformed fixture open failed", NULL, 11);
    const char bad[] = "{\"organisation\":\"A\",\"organisation\":\"B\"}";
    DWORD written = 0U;
    const BOOL write_ok = WriteFile(file, bad, (DWORD)(sizeof(bad) - 1U), &written, NULL);
    CloseHandle(file);
    if (!write_ok || written != (DWORD)(sizeof(bad) - 1U))
        return fail("malformed fixture write failed", NULL, 12);
    if (runner_windows_config_load(&loaded, error, sizeof(error)))
        return fail("malformed configuration was silently accepted", NULL, 13);

    DeleteFileW(path);
    wchar_t config_dir[MAX_PATH];
    _snwprintf_s(config_dir, MAX_PATH, _TRUNCATE, L"%ls\\RunnerScope", root);
    RemoveDirectoryW(config_dir);
    RemoveDirectoryW(root);
    puts("Windows configuration store contract passed.");
    return 0;
}
