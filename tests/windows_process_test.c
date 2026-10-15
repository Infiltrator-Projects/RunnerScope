// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#include "platform/windows_process.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char error[256];
    RunnerWindowsProcessResult result;
    if (!runner_windows_process_capture(
            L"cmd.exe /d /s /c \"echo ready\"", 5000U,
            &result, error, sizeof(error))) {
        fprintf(stderr, "short command failed: %s\n", error);
        return 1;
    }
    if (result.exit_code != 0U || !result.output ||
        strstr(result.output, "ready") == NULL) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "short command capture was incorrect\n");
        return 2;
    }
    runner_windows_process_result_destroy(&result);

    if (runner_windows_process_capture(
            L"powershell.exe -NoProfile -NonInteractive -Command \"Start-Sleep -Seconds 5\"", 100U,
            &result, error, sizeof(error))) {
        runner_windows_process_result_destroy(&result);
        fprintf(stderr, "long command escaped timeout\n");
        return 3;
    }
    if (!result.timed_out) {
        fprintf(stderr, "timeout failure was not classified\n");
        return 4;
    }
    runner_windows_process_result_destroy(&result);

    puts("Windows bounded process contract passed.");
    return 0;
}
