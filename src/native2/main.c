// SPDX-License-Identifier: GPL-3.0-or-later
#include "http.h"
#include "linux_local.h"
#include "model.h"
#include "secure_config.h"

#include <infiltratr/core.h>

#include <stdio.h>
#include <string.h>

#define RUNNERSCOPE_NATIVE_NEXT_VERSION "1.2.0-dev"

static int self_test(void)
{
    RsRunnerSession session;
    rs_runner_session_init(&session, RS_RUNNER_IDLE, 10.0);
    if (!rs_runner_session_apply(&session, RS_RUNNER_RUNNING, 20.0))
        return 1;
    if (!rs_runner_session_apply(&session, RS_RUNNER_IDLE, 30.0))
        return 2;
    if (session.jobs_started != 1U ||
        rs_runner_session_busy_seconds(&session, 40.0) != 10.0)
        return 3;

    char request[1024];
    size_t request_length = 0U;
    if (!rs_http_build_get(request, sizeof(request),
                           "api.github.com",
                           "/orgs/Infiltrator-Projects/actions/runners",
                           "token-example",
                           "RunnerScope/1.2.0-dev",
                           &request_length))
        return 4;
    if (request_length == 0U ||
        strstr(request, "Authorization: Bearer token-example") == NULL)
        return 5;

    puts("RunnerScope first-party native core self-test passed");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
        return self_test();
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts(RUNNERSCOPE_NATIVE_NEXT_VERSION);
        return 0;
    }

    RsLocalRunnerUnit units[64];
    const size_t count = rs_linux_discover_runner_units(
        units, sizeof(units) / sizeof(units[0]));
    printf("RunnerScope native-next %s\n", RUNNERSCOPE_NATIVE_NEXT_VERSION);
    printf("Common %s\n", INFILTRATR_COMMON_VERSION);
    printf("Local runner units: %zu\n", count);
    for (size_t i = 0U; i < count; ++i)
        printf("%s\t%s\n", units[i].service_name, units[i].unit_path);
    return 0;
}
