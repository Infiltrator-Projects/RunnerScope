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

void runner_windows_process_result_destroy(RunnerWindowsProcessResult *result)
{
    if (!result) return;
    free(result->output);
    result->output = NULL;
    result->exit_code = 0U;
    result->timed_out = false;
}

bool runner_windows_process_capture(const wchar_t *command,
                                    unsigned long timeout_ms,
                                    RunnerWindowsProcessResult *result,
                                    char *error, size_t error_size)
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
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    const size_t command_chars = wcslen(command) + 1U;
    wchar_t *mutable_command = (wchar_t *)calloc(command_chars, sizeof(wchar_t));
    if (!mutable_command) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        if (error && error_size)
            (void)snprintf(error, error_size, "Unable to allocate provider command buffer.");
        return false;
    }
    wcscpy_s(mutable_command, command_chars, command);

    const BOOL created = CreateProcessW(NULL, mutable_command, NULL, NULL, TRUE,
                                        CREATE_NO_WINDOW, NULL, NULL,
                                        &startup, &process);
    free(mutable_command);
    CloseHandle(write_pipe);
    if (!created) {
        const DWORD failure = GetLastError();
        CloseHandle(read_pipe);
        if (error && error_size)
            (void)snprintf(error, error_size,
                           "Unable to start provider process (Windows error %lu).",
                           (unsigned long)failure);
        return false;
    }

    char *buffer = NULL;
    size_t used = 0U;
    size_t capacity = 0U;
    const ULONGLONG started = GetTickCount64();
    bool exited = false;
    bool capture_ok = true;

    for (;;) {
        DWORD available = 0U;
        const BOOL peek_ok = PeekNamedPipe(read_pipe, NULL, 0U, NULL, &available, NULL);
        if (!peek_ok) {
            const DWORD pipe_error = GetLastError();
            if (pipe_error != ERROR_BROKEN_PIPE) capture_ok = false;
            if (exited || pipe_error == ERROR_BROKEN_PIPE) break;
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
            available -= amount;
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

        if (!exited && GetTickCount64() - started >= (ULONGLONG)timeout_ms) {
            result->timed_out = true;
            (void)TerminateProcess(process.hProcess, ERROR_TIMEOUT);
            (void)WaitForSingleObject(process.hProcess, 5000U);
            exited = true;
        }

        if (exited) {
            DWORD remaining = 0U;
            if (!PeekNamedPipe(read_pipe, NULL, 0U, NULL, &remaining, NULL) ||
                remaining == 0U)
                break;
        }
    }

    DWORD code = 1U;
    (void)GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(read_pipe);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    result->exit_code = (unsigned long)code;

    if (!buffer) {
        buffer = (char *)calloc(1U, 1U);
        if (!buffer) capture_ok = false;
    }
    if (result->timed_out) {
        free(buffer);
        if (error && error_size)
            (void)snprintf(error, error_size,
                           "Provider process exceeded the %lu ms timeout.", timeout_ms);
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
