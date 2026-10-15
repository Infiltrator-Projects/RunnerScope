// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/linux_github_auth.h"

#include <string.h>

#define AUTH_OUTPUT_LIMIT 16384U
#define AUTH_TIMEOUT_SECONDS 900U

struct RunnerGithubAuth {
    GSubprocess *process;
    GCancellable *read_cancel;
    GString *output;
    RunnerGithubAuthUpdate update;
    RunnerGithubAuthComplete complete;
    gpointer user_data;
    guint timeout;
    guint drain_timeout;
    gboolean read_done;
    gboolean wait_done;
    gboolean succeeded;
    gboolean cancelled;
    gboolean expired;
    gboolean probing;
    const char *failure_detail;
};

static void auth_wait(GObject *source, GAsyncResult *result, gpointer user_data);
static void auth_begin_io(RunnerGithubAuth *auth);

static GSubprocess *auth_spawn(gboolean probe, gboolean refresh, GError **error)
{
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE);
    g_subprocess_launcher_unsetenv(launcher, "GH_FORCE_TTY");
    g_subprocess_launcher_unsetenv(launcher, "GH_DEBUG");
    g_subprocess_launcher_unsetenv(launcher, "DEBUG");
    g_subprocess_launcher_setenv(launcher, "GH_PROMPT_DISABLED", "1", TRUE);
    g_subprocess_launcher_setenv(launcher, "NO_COLOR", "1", TRUE);
    const char *status[] = {"gh", "api", "--hostname", "github.com", "/user", "--jq", ".login", NULL};
    const char *login[] = {"gh", "auth", "login", "--hostname", "github.com",
        "--web", "--scopes", "admin:org", NULL};
    const char *renew[] = {"gh", "auth", "refresh", "--hostname", "github.com",
        "--scopes", "admin:org", NULL};
    GSubprocess *process = g_subprocess_launcher_spawnv(launcher,
        probe ? status : refresh ? renew : login, error);
    g_object_unref(launcher);
    return process;
}

RunnerGithubAuthIssue runner_github_auth_issue(const char *error)
{
    if (!error) return RUNNER_GITHUB_AUTH_NONE;
    char *lower = g_ascii_strdown(error, -1);
    RunnerGithubAuthIssue issue = RUNNER_GITHUB_AUTH_NONE;
    if ((strstr(lower, "admin:org") && strstr(lower, "scope")) ||
        strstr(lower, "insufficient scope"))
        issue = RUNNER_GITHUB_AUTH_SCOPE;
    else if (strstr(lower, "gh auth login") ||
             strstr(lower, "not logged") || strstr(lower, "http 401"))
        issue = RUNNER_GITHUB_AUTH_LOGIN;
    else if (strstr(lower, "http 403"))
        issue = RUNNER_GITHUB_AUTH_ACCESS;
    g_free(lower);
    return issue;
}

gboolean runner_github_auth_environment_token(void)
{
    const char *gh_token = g_getenv("GH_TOKEN");
    const char *github_token = g_getenv("GITHUB_TOKEN");
    return (gh_token && *gh_token) || (github_token && *github_token);
}

static void auth_finish_if_done(RunnerGithubAuth *auth)
{
    if (!auth->read_done || !auth->wait_done) return;
    if (auth->drain_timeout) {
        g_source_remove(auth->drain_timeout);
        auth->drain_timeout = 0U;
    }
    if (auth->probing && !auth->cancelled) {
        /* Probe only the active credential. Invalid inactive saved accounts do
         * not force a replacement login or discard this account's scopes. */
        const gboolean refresh = auth->succeeded;
        const gboolean login = runner_github_auth_issue(auth->output->str) == RUNNER_GITHUB_AUTH_LOGIN;
        auth->probing = FALSE;
        g_string_truncate(auth->output, 0U);
        if (refresh || login) {
            GError *error = NULL;
            GSubprocess *process = auth_spawn(FALSE, refresh, &error);
            if (process) {
                g_clear_object(&auth->process);
                auth->process = process;
                g_cancellable_reset(auth->read_cancel);
                auth_begin_io(auth);
                return;
            }
            g_clear_error(&error);
            auth->failure_detail = "Unable to start GitHub authorization. Select Connect GitHub to try again. Your saved credentials were kept.";
        } else {
            auth->failure_detail = "Unable to check your GitHub sign-in. Check the connection and select Connect GitHub to try again. Your saved credentials were kept.";
        }
        auth->succeeded = FALSE;
        /* The probe's successful username and raw diagnostics stay private. */
        g_string_append(auth->output, auth->failure_detail);
        if (auth->update) auth->update(auth->output->str, auth->user_data);
    }
    if (auth->timeout) {
        g_source_remove(auth->timeout);
        auth->timeout = 0U;
    }
    const char *detail = auth->expired
        ? "GitHub authorization timed out. Connect again to request a new code."
        : auth->cancelled ? "GitHub authorization cancelled."
        : auth->failure_detail ? auth->failure_detail
        : auth->succeeded ? "Authorization completed. Checking runner access…"
        : "GitHub authorization did not complete. Try again, using the account with access to your organisation.";
    /* Completion may release auth. Keep this the final operation. */
    auth->complete(auth->succeeded && !auth->cancelled, detail, auth->user_data);
}

static void auth_read(GObject *source, GAsyncResult *result, gpointer user_data)
{
    RunnerGithubAuth *auth = user_data;
    GError *error = NULL;
    GBytes *bytes = g_input_stream_read_bytes_finish(G_INPUT_STREAM(source), result, &error);
    gsize length = 0U;
    const char *data = bytes ? g_bytes_get_data(bytes, &length) : NULL;
    if (length) {
        const gsize room = AUTH_OUTPUT_LIMIT - auth->output->len;
        g_string_append_len(auth->output, data, (gssize)MIN(room, length));
        if (!auth->probing) {
            char *valid = g_utf8_make_valid(auth->output->str, (gssize)auth->output->len);
            if (auth->update) auth->update(valid, auth->user_data);
            g_free(valid);
        }
        g_input_stream_read_bytes_async(G_INPUT_STREAM(source), 4096U,
            G_PRIORITY_DEFAULT, auth->read_cancel, auth_read, auth);
    } else {
        auth->read_done = TRUE;
    }
    g_clear_pointer(&bytes, g_bytes_unref);
    g_clear_error(&error);
    auth_finish_if_done(auth);
}

static void auth_begin_io(RunnerGithubAuth *auth)
{
    auth->read_done = FALSE;
    auth->wait_done = FALSE;
    auth->succeeded = FALSE;
    g_input_stream_read_bytes_async(g_subprocess_get_stdout_pipe(auth->process), 4096U,
        G_PRIORITY_DEFAULT, auth->read_cancel, auth_read, auth);
    g_subprocess_wait_async(auth->process, NULL, auth_wait, auth);
}

static gboolean auth_drain_expired(gpointer user_data)
{
    RunnerGithubAuth *auth = user_data;
    auth->drain_timeout = 0U;
    g_cancellable_cancel(auth->read_cancel);
    return G_SOURCE_REMOVE;
}

static void auth_wait(GObject *source, GAsyncResult *result, gpointer user_data)
{
    RunnerGithubAuth *auth = user_data;
    GError *error = NULL;
    const gboolean waited = g_subprocess_wait_finish(G_SUBPROCESS(source), result, &error);
    auth->wait_done = TRUE;
    auth->succeeded = waited && g_subprocess_get_successful(auth->process);
    g_clear_error(&error);
    /* Drain final diagnostics, bounded if a descendant retains the pipe. */
    if (!auth->read_done)
        auth->drain_timeout = g_timeout_add(250U, auth_drain_expired, auth);
    auth_finish_if_done(auth);
}

void runner_github_auth_cancel(RunnerGithubAuth *auth)
{
    if (!auth) return;
    auth->cancelled = TRUE;
    g_subprocess_force_exit(auth->process);
    g_cancellable_cancel(auth->read_cancel);
}

static gboolean auth_expired(gpointer user_data)
{
    RunnerGithubAuth *auth = user_data;
    auth->timeout = 0U;
    auth->expired = TRUE;
    runner_github_auth_cancel(auth);
    return G_SOURCE_REMOVE;
}

RunnerGithubAuth *runner_github_auth_start(
    RunnerGithubAuthIssue issue, RunnerGithubAuthUpdate update,
    RunnerGithubAuthComplete complete, gpointer user_data, GError **error)
{
    g_return_val_if_fail(complete != NULL, NULL);
    if (runner_github_auth_environment_token()) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "GitHub access is supplied by GH_TOKEN or GITHUB_TOKEN. Update that token's organisation runner permissions, or remove the override and connect here.");
        return NULL;
    }
    const gboolean probe = issue == RUNNER_GITHUB_AUTH_NONE || issue == RUNNER_GITHUB_AUTH_ACCESS;
    GSubprocess *process = auth_spawn(probe, issue == RUNNER_GITHUB_AUTH_SCOPE, error);
    if (!process) return NULL;
    RunnerGithubAuth *auth = g_new0(RunnerGithubAuth, 1U);
    auth->process = process;
    auth->read_cancel = g_cancellable_new();
    auth->output = g_string_new(NULL);
    auth->update = update;
    auth->complete = complete;
    auth->user_data = user_data;
    auth->probing = probe;
    auth->timeout = g_timeout_add_seconds(AUTH_TIMEOUT_SECONDS, auth_expired, auth);
    auth_begin_io(auth);
    return auth;
}

void runner_github_auth_free(RunnerGithubAuth *auth)
{
    if (!auth) return;
    g_return_if_fail(auth->read_done && auth->wait_done);
    g_clear_object(&auth->process);
    g_clear_object(&auth->read_cancel);
    g_string_free(auth->output, TRUE);
    g_free(auth);
}
