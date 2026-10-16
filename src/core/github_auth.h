// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_GITHUB_AUTH_H
#define RUNNERSCOPE_GITHUB_AUTH_H
#include "core/runner_config.h"
#include "platform/http.h"
#include <stdbool.h>
#include <stddef.h>

#define RUNNER_AUTH_MAX_ORGANISATIONS 32U
typedef enum { RUNNER_AUTH_NEW, RUNNER_AUTH_WAITING, RUNNER_AUTH_COMPLETE, RUNNER_AUTH_FAILED } RunnerAuthPhase;
typedef struct {
    RunnerAuthPhase phase;
    char user_code[32];
    char token[RUNNER_GITHUB_TOKEN_CAPACITY];
    char error[320];
    char organisation[128];
    char organisations[RUNNER_AUTH_MAX_ORGANISATIONS][128];
    size_t organisation_count;
} RunnerGithubAuth;
typedef struct {
    bool (*cancelled)(void *context);
    void (*challenge)(const char *code, void *context);
    void *context;
} RunnerAuthCallbacks;
typedef enum { RUNNER_AUTH_EXISTING_SESSION, RUNNER_AUTH_BROWSER_LOGIN, RUNNER_AUTH_NEW_SESSION, RUNNER_AUTH_VERSION } RunnerAuthCommand;
/* Execute only these fixed commands. Token output never enters diagnostics. */
typedef bool (*RunnerAuthExecute)(RunnerAuthCommand command, char *output, size_t capacity,
    const RunnerAuthCallbacks *callbacks, void *context);

void runner_github_auth_clear(RunnerGithubAuth *auth);
bool runner_github_auth_cancelled(const RunnerAuthCallbacks *callbacks);
bool runner_github_auth_code(const char *line, char code[32]);
bool runner_github_auth_token(const char *output, char *token, size_t capacity);
void runner_github_authenticate(RunnerGithubAuth *auth, const char *organisation,
    RunnerAuthExecute execute, void *execute_context, RunnerHttpGet get, void *http_context,
    const RunnerAuthCallbacks *callbacks);
#endif
