// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "platform/windows_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct { char output[256]; size_t used; bool cancel; } StreamProbe;

static void progress(const char *data, size_t amount, void *context)
{
    StreamProbe *probe = (StreamProbe *)context;
    if (amount > sizeof(probe->output) - probe->used - 1U)
        amount = sizeof(probe->output) - probe->used - 1U;
    memcpy(probe->output + probe->used, data, amount);
    probe->used += amount;
    probe->output[probe->used] = '\0';
    if (strstr(probe->output, "ready")) probe->cancel = true;
}

static bool cancelled(void *context)
{
    return ((StreamProbe *)context)->cancel;
}

static void child_command(wchar_t *command, size_t capacity, const wchar_t *mode)
{
    wchar_t path[MAX_PATH];
    (void)GetModuleFileNameW(NULL, path, MAX_PATH);
    _snwprintf_s(command, capacity, _TRUNCATE, L"\"%ls\" %ls", path, mode);
}

static wchar_t *saved_environment(const wchar_t *name)
{
    const DWORD needed = GetEnvironmentVariableW(name, NULL, 0U);
    if (needed == 0U) return NULL;
    wchar_t *value = (wchar_t *)calloc((size_t)needed, sizeof(wchar_t));
    if (value) (void)GetEnvironmentVariableW(name, value, needed);
    return value;
}

/* Fake providers are this executable's child modes. No gh invocation, network,
 * stored login, browser, or account change is involved in these checks. */
int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "--emit") == 0) {
            fputs("ready\n", stdout);
            fputs("stderr captured\n", stderr);
            return 0;
        }
        if (strcmp(argv[1], "--wait") == 0) {
            fputs("ready\n", stderr);
            fflush(stderr);
            Sleep(5000U);
            return 0;
        }
        if (strcmp(argv[1], "--close-output") == 0) {
            fclose(stdout);
            fclose(stderr);
            Sleep(5000U);
            return 0;
        }
        if (strcmp(argv[1], "--stdin") == 0) {
            char input;
            DWORD count = 1U;
            if (ReadFile(GetStdHandle(STD_INPUT_HANDLE), &input, 1U, &count, NULL) && count == 0U) {
                fputs("input EOF\n", stdout);
                return 0;
            }
            return 1;
        }
        if (strcmp(argv[1], "--environment") == 0) {
            wchar_t value[2];
            if (GetEnvironmentVariableW(L"GH_DEBUG", value, 2U) == 0U &&
                GetEnvironmentVariableW(L"DEBUG", value, 2U) == 0U) {
                fputs("debug flags absent\n", stdout);
                return 0;
            }
            return 1;
        }
    }
    wchar_t command[MAX_PATH + 64];
    char error[256];
    RunnerWindowsProcessResult result;
    child_command(command, sizeof(command) / sizeof(command[0]), L"--emit");
    if (!runner_windows_process_capture(command, 5000U, &result, error, sizeof(error))) {
        fprintf(stderr, "short command failed: %s\n", error);
        return 1;
    }
    if (result.exit_code != 0U || !result.output ||
        !strstr(result.output, "ready") || !strstr(result.output, "stderr captured")) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "stdout/stderr capture was incorrect\n");
        return 2;
    }
    runner_windows_process_result_destroy(&result);

    child_command(command, sizeof(command) / sizeof(command[0]), L"--wait");
    if (runner_windows_process_capture(command, 100U, &result, error, sizeof(error)) ||
        !result.timed_out || result.cancelled) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "timeout failure was not classified\n");
        return 3;
    }
    runner_windows_process_result_destroy(&result);

    StreamProbe probe = {0};
    const ULONGLONG started = GetTickCount64();
    if (runner_windows_process_capture_stream(command, 5000U, progress, cancelled,
            &probe, &result, error, sizeof(error)) || !result.cancelled || result.timed_out ||
        !strstr(probe.output, "ready") || GetTickCount64() - started >= 3000U) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "live progress/cancellation contract failed\n");
        return 4;
    }
    runner_windows_process_result_destroy(&result);

    child_command(command, sizeof(command) / sizeof(command[0]), L"--close-output");
    if (runner_windows_process_capture(command, 100U, &result, error, sizeof(error)) ||
        !result.timed_out) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "closed output escaped bounded process waiting\n");
        return 5;
    }
    runner_windows_process_result_destroy(&result);

    child_command(command, sizeof(command) / sizeof(command[0]), L"--stdin");
    if (!runner_windows_process_capture(command, 5000U, &result, error, sizeof(error)) ||
        result.exit_code != 0U || !result.output || !strstr(result.output, "input EOF")) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "GUI provider stdin was not EOF\n");
        return 6;
    }
    runner_windows_process_result_destroy(&result);

    wchar_t *old_gh_debug = saved_environment(L"GH_DEBUG");
    wchar_t *old_debug = saved_environment(L"DEBUG");
    (void)SetEnvironmentVariableW(L"GH_DEBUG", L"api");
    (void)SetEnvironmentVariableW(L"DEBUG", L"1");
    child_command(command, sizeof(command) / sizeof(command[0]), L"--environment");
    const bool environment_ok = runner_windows_process_capture(command, 5000U,
        &result, error, sizeof(error));
    wchar_t parent_value[8];
    const bool parent_unchanged = GetEnvironmentVariableW(L"GH_DEBUG", parent_value, 8U) == 3U &&
        wcscmp(parent_value, L"api") == 0;
    (void)SetEnvironmentVariableW(L"GH_DEBUG", old_gh_debug);
    (void)SetEnvironmentVariableW(L"DEBUG", old_debug);
    free(old_gh_debug);
    free(old_debug);
    if (!environment_ok || result.exit_code != 0U || !parent_unchanged ||
        !result.output || !strstr(result.output, "debug flags absent")) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "provider debug environment isolation failed\n");
        return 7;
    }
    runner_windows_process_result_destroy(&result);
    puts("Windows bounded streaming process contract passed.");
    return 0;
}
