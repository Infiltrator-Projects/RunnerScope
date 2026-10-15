// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNER_LINUX_GITHUB_AUTH_H
#define RUNNER_LINUX_GITHUB_AUTH_H

#include <gio/gio.h>

typedef enum {
    RUNNER_GITHUB_AUTH_NONE,
    RUNNER_GITHUB_AUTH_LOGIN,
    RUNNER_GITHUB_AUTH_SCOPE,
    RUNNER_GITHUB_AUTH_ACCESS
} RunnerGithubAuthIssue;

typedef struct RunnerGithubAuth RunnerGithubAuth;
typedef void (*RunnerGithubAuthUpdate)(const char *output, gpointer user_data);
typedef void (*RunnerGithubAuthComplete)(gboolean succeeded, const char *detail,
                                         gpointer user_data);

RunnerGithubAuthIssue runner_github_auth_issue(const char *error);
gboolean runner_github_auth_environment_token(void);
/* The caller owns the returned operation until its completion callback runs.
 * All callbacks run on the starting thread's main context. No token is read. */
RunnerGithubAuth *runner_github_auth_start(
    RunnerGithubAuthIssue issue, RunnerGithubAuthUpdate update,
    RunnerGithubAuthComplete complete, gpointer user_data, GError **error);
void runner_github_auth_cancel(RunnerGithubAuth *auth);
/* Only release after completion; cancel and drain callbacks on shutdown. */
void runner_github_auth_free(RunnerGithubAuth *auth);

#endif
