// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/windows_process.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append_capture(char **buffer, size_t *used, size_t *capacity,
                           const char *data, size_t amount)
{
    if (!buffer || !used || !capacity || (!data && amount != 0U)) return false;
    if (*capacity == 0U) {
        *capacity = 8192U;
        *buffer = (char *)calloc(*capacity, 1U);
        if (!*buffer) return false;
    }
    if (*used + amount + 1U > *capacity) {
        size_t next = *capacity;
        while (next < *used + amount + 1U) {
            if (next > SIZE_MAX / 2U) return false;
            next *= 2U;
        }
        char *grown = (char *)realloc(*buffer, next);
        if (!grown) return false;
        *buffer = grown;
        *capacity = next;
    }
    if (amount != 0U) memcpy(*buffer + *used, data, amount);
    *used += amount;
    (*buffer)[*used] = '\0';
    return true;
}

static wchar_t *provider_environment(void)
{
    wchar_t *environment = GetEnvironmentStringsW();
    if (!environment) return NULL;
    size_t count = 1U;
    for (const wchar_t *entry = environment; *entry; entry += wcslen(entry) + 1U)
        count += wcslen(entry) + 1U;
    wchar_t *filtered = (wchar_t *)calloc(count + 1U, sizeof(wchar_t));
    if (filtered) {
        wchar_t *destination = filtered;
        for (const wchar_t *entry = environment; *entry; entry += wcslen(entry) + 1U) {
            /* gh debug HTTP logs can contain OAuth request/response secrets.
             * Remove these flags only in the child, preserving the app's env. */
            if (_wcsnicmp(entry, L"GH_DEBUG=", 9U) == 0 ||
                _wcsnicmp(entry, L"DEBUG=", 6U) == 0) continue;
            const size_t amount = wcslen(entry) + 1U;
            memcpy(destination, entry, amount * sizeof(wchar_t));
            destination += amount;
        }
    }
    FreeEnvironmentStringsW(environment);
    return filtered;
}

void runner_windows_process_result_destroy(RunnerWindowsProcessResult *result)
{
    if (!result) return;
    free(result->output);
    result->output = NULL;
    result->exit_code = 0U;
    result->timed_out = false;
    result->cancelled = false;
}

bool runner_windows_process_capture_stream(const wchar_t *command,
        unsigned long timeout_ms, RunnerWindowsProcessProgress progress,
        RunnerWindowsProcessCancelled cancelled, void *context,
        RunnerWindowsProcessResult *result, char *error, size_t error_size)
{
    if (!result) return false;
    memset(result, 0, sizeof(*result));
    if (error && error_size) error[0] = '\0';
    if (!command || !*command || timeout_ms == 0U) {
        if (error && error_size)
            (void)snprintf(error, error_size, "Provider command or timeout is invalid.");
        return false;
    }

    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0U) ||
        !SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0U)) {
        if (read_pipe) CloseHandle(read_pipe);
        if (write_pipe) CloseHandle(write_pipe);
        if (error && error_size)
            (void)snprintf(error, error_size, "Unable to create provider capture pipe.");
        return false;
    }

    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    /* A GUI has no console stdin. An explicit EOF handle also prevents provider
     * prompts from waiting on whichever console launched Runner Monitor. */
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        if (error && error_size)
            (void)snprintf(error, error_size, "Unable to create provider input handle.");
        return false;
    }
    startup.hStdInput = input;

    const size_t command_chars = wcslen(command) + 1U;
    wchar_t *mutable_command = (wchar_t *)calloc(command_chars, sizeof(wchar_t));
    if (!mutable_command) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        CloseHandle(input);
        if (error && error_size)
            (void)snprintf(error, error_size, "Unable to allocate provider command buffer.");
        return false;
    }
    wcscpy_s(mutable_command, command_chars, command);

    wchar_t *environment = provider_environment();
    if (!environment) {
        free(mutable_command);
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        CloseHandle(input);
        if (error && error_size)
            (void)snprintf(error, error_size, "Unable to prepare provider environment.");
        return false;
    }

    const BOOL created = CreateProcessW(NULL, mutable_command, NULL, NULL, TRUE,
                                        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                                        environment, NULL,
                                        &startup, &process);
    const DWORD launch_error = created ? ERROR_SUCCESS : GetLastError();
    free(environment);
    free(mutable_command);
    CloseHandle(write_pipe);
    CloseHandle(input);
    if (!created) {
        CloseHandle(read_pipe);
        if (error && error_size)
            (void)snprintf(error, error_size,
                           "Unable to start provider process (Windows error %lu).",
                           (unsigned long)launch_error);
        return false;
    }

    /* Terminate descendants too, including helpers holding the capture pipe.
     * Nested jobs are supported on the Windows versions this app targets. */
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
        ZeroMemory(&limits, sizeof(limits));
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                      &limits, sizeof(limits)) ||
            !AssignProcessToJobObject(job, process.hProcess)) {
            CloseHandle(job);
            job = NULL;
        }
    }
    if (ResumeThread(process.hThread) == (DWORD)-1) {
        (void)TerminateProcess(process.hProcess, ERROR_PROCESS_ABORTED);
        if (job) CloseHandle(job);
        CloseHandle(read_pipe);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        if (error && error_size)
            (void)snprintf(error, error_size, "Unable to resume provider process.");
        return false;
    }

    char *buffer = NULL;
    size_t used = 0U;
    size_t capacity = 0U;
    const ULONGLONG started = GetTickCount64();
    bool exited = false;
    bool capture_ok = true;

    for (;;) {
        if (cancelled && cancelled(context)) {
            result->cancelled = true;
            break;
        }
        if (GetTickCount64() - started >= (ULONGLONG)timeout_ms) {
            result->timed_out = true;
            break;
        }
        DWORD available = 0U;
        const BOOL peek_ok = PeekNamedPipe(read_pipe, NULL, 0U, NULL, &available, NULL);
        if (!peek_ok) {
            const DWORD pipe_error = GetLastError();
            if (pipe_error != ERROR_BROKEN_PIPE) capture_ok = false;
            if (exited || !capture_ok) break;
        }

        while (available > 0U) {
            char chunk[4096];
            const DWORD request = available < (DWORD)sizeof(chunk)
                ? available : (DWORD)sizeof(chunk);
            DWORD amount = 0U;
            if (!ReadFile(read_pipe, chunk, request, &amount, NULL)) {
                if (GetLastError() != ERROR_BROKEN_PIPE) capture_ok = false;
                available = 0U;
                break;
            }
            if (amount == 0U) break;
            if (!append_capture(&buffer, &used, &capacity, chunk, (size_t)amount)) {
                capture_ok = false;
                available = 0U;
                break;
            }
            if (progress) progress(chunk, (size_t)amount, context);
            available -= amount;
            if ((cancelled && cancelled(context)) ||
                GetTickCount64() - started >= (ULONGLONG)timeout_ms)
                break;
        }
        if (!capture_ok) break;

        if (!exited) {
            const DWORD wait = WaitForSingleObject(process.hProcess, 25U);
            if (wait == WAIT_OBJECT_0) exited = true;
            else if (wait == WAIT_FAILED) {
                capture_ok = false;
                break;
            }
        }

        if (exited) {
            DWORD remaining = 0U;
            if (!PeekNamedPipe(read_pipe, NULL, 0U, NULL, &remaining, NULL) ||
                remaining == 0U)
                break;
        }
    }

    if (!exited || !capture_ok || result->timed_out || result->cancelled) {
        if (job) (void)TerminateJobObject(job, ERROR_PROCESS_ABORTED);
        else (void)TerminateProcess(process.hProcess, ERROR_PROCESS_ABORTED);
        (void)WaitForSingleObject(process.hProcess, 5000U);
    }

    DWORD code = 1U;
    (void)GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(read_pipe);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (job) CloseHandle(job);
    result->exit_code = (unsigned long)code;

    if (!buffer) {
        buffer = (char *)calloc(1U, 1U);
        if (!buffer) capture_ok = false;
    }
    if (result->timed_out || result->cancelled) {
        free(buffer);
        if (error && error_size) {
            if (result->cancelled)
                (void)snprintf(error, error_size, "Provider process cancelled.");
            else
                (void)snprintf(error, error_size,
                               "Provider process exceeded the %lu ms timeout.", timeout_ms);
        }
        return false;
    }
    if (!capture_ok) {
        free(buffer);
        if (error && error_size)
            (void)snprintf(error, error_size, "Provider process output capture failed.");
        return false;
    }
    result->output = buffer;
    return true;
}

bool runner_windows_process_capture(const wchar_t *command,
                                    unsigned long timeout_ms,
                                    RunnerWindowsProcessResult *result,
                                    char *error, size_t error_size)
{
    return runner_windows_process_capture_stream(command, timeout_ms, NULL, NULL,
                                                  NULL, result, error, error_size);
}
