// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_GITHUB_SIGN_IN_H
#define RUNNERSCOPE_GITHUB_SIGN_IN_H
#include "core/github_auth.h"
void runner_github_sign_in(RunnerGithubAuth *auth, const char *organisation,
    const RunnerAuthCallbacks *callbacks);
/* Explicit executable injection for offline process qualification; production never searches PATH. */
void runner_github_sign_in_with_executable(RunnerGithubAuth *auth, const char *organisation,
    const RunnerAuthCallbacks *callbacks, const char *executable, RunnerHttpGet get, void *http_context);
bool runner_github_sign_in_available(void);
bool runner_github_sign_in_probe(void);
#endif
