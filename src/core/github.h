// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNER_GITHUB_H
#define RUNNER_GITHUB_H

#include "platform/http.h"

typedef enum {
    RUNNER_GITHUB_RUNNERS,
    RUNNER_GITHUB_REPOSITORIES,
    RUNNER_GITHUB_RUNS,
    RUNNER_GITHUB_JOBS
} RunnerGithubProjection;

/* Returns owned, normalized tab-separated rows for the existing native view
 * models. JSON decoding/pagination/error policy is shared by both platforms;
 * embedded field separators are normalized and never interpreted as commands. */
char *runner_github_fetch(const char *endpoint, const char *token,
    RunnerGithubProjection projection, RunnerHttpGet get, void *context,
    char *error, size_t error_size);

/* Decoder seam for deterministic offline protocol tests. */
char *runner_github_project(const char *json, RunnerGithubProjection projection,
    char *error, size_t error_size);

#endif
