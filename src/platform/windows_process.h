// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_WINDOWS_PROCESS_H
#define RUNNERSCOPE_WINDOWS_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

typedef struct {
    char *output;
    unsigned long exit_code;
    bool timed_out;
    bool cancelled;
} RunnerWindowsProcessResult;

/* Callbacks run on the calling worker thread. Progress contains only the newly
 * captured bytes; cancellation is checked while waiting, even without output.
 * Provider children have EOF stdin and omit GH_DEBUG/DEBUG to keep secrets out
 * of captured GitHub diagnostics; the parent's environment is unchanged. */
typedef void (*RunnerWindowsProcessProgress)(const char *data, size_t amount,
                                              void *context);
typedef bool (*RunnerWindowsProcessCancelled)(void *context);

bool runner_windows_process_capture_stream(const wchar_t *command,
        unsigned long timeout_ms, RunnerWindowsProcessProgress progress,
        RunnerWindowsProcessCancelled cancelled, void *context,
        RunnerWindowsProcessResult *result, char *error, size_t error_size);

bool runner_windows_process_capture(const wchar_t *command,
                                    unsigned long timeout_ms,
                                    RunnerWindowsProcessResult *result,
                                    char *error,
                                    size_t error_size);

void runner_windows_process_result_destroy(RunnerWindowsProcessResult *result);

#endif
