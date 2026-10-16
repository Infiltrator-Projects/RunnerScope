// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNER_GITHUB_AUTH_H
#define RUNNER_GITHUB_AUTH_H

#include "core/runner_config.h"
#include "platform/http.h"
#include <stdint.h>

typedef enum {
    RUNNER_AUTH_NEW,
    RUNNER_AUTH_WAITING,
    RUNNER_AUTH_COMPLETE,
    RUNNER_AUTH_FAILED
} RunnerAuthPhase;

typedef struct {
    RunnerAuthPhase phase;
    char device_code[128];
    char user_code[32];
    char token[RUNNER_GITHUB_TOKEN_CAPACITY];
    char error[320];
    uint64_t expires_at_ms;
    uint64_t next_poll_ms;
    unsigned int interval_seconds;
} RunnerGithubAuth;

/* Uses only the project's public OAuth client ID. No client secret, CLI,
 * personal token entry, browser cookies or Git configuration is involved.
 * The caller owns the clock, worker lifetime and cancellation; this state
 * machine enforces GitHub's polling interval and local code expiry. */
const char *runner_github_auth_client_id(void);
const char *runner_github_auth_client_for_config(const RunnerConfig *config);
const char *runner_github_setup_instructions(void);
void runner_github_auth_clear(RunnerGithubAuth *auth);
RunnerAuthPhase runner_github_auth_step(RunnerGithubAuth *auth,
    const char *client_id, uint64_t now_ms, RunnerHttpPost post, void *context);

#endif
