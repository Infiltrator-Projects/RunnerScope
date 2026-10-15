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
} RunnerWindowsProcessResult;

bool runner_windows_process_capture(const wchar_t *command,
                                    unsigned long timeout_ms,
                                    RunnerWindowsProcessResult *result,
                                    char *error,
                                    size_t error_size);

void runner_windows_process_result_destroy(RunnerWindowsProcessResult *result);

#endif
