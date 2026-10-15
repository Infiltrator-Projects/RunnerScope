// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

#include "platform/linux_github_auth.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char *fixture_root;
static char *fixture_mode;
static char *fixture_calls;
static char *fixture_pid;
static char *fixture_descendant;

/* This test binary also serves as an offline gh. No real CLI, login, token or
 * account is used. Keep the helper in one process except the pipe-bound test. */
static int fake_gh(int argc, char **argv)
{
    const char *directory = g_getenv("RUNNER_AUTH_TEST_ROOT");
    if (!directory) return 91;
    char *mode_path = g_build_filename(directory, "mode", NULL);
    char *mode = NULL;
    if (!g_file_get_contents(mode_path, &mode, NULL, NULL)) return 92;
    g_free(mode_path);

    GString *calls = g_string_new(NULL);
    for (int i = 1; i < argc; ++i) g_string_append_printf(calls, "%s\n", argv[i]);
    g_string_append_printf(calls, "GH_PROMPT_DISABLED=%s\nNO_COLOR=%s\nGH_FORCE_TTY=%s\nGH_DEBUG=%s\nDEBUG=%s\n",
        g_getenv("GH_PROMPT_DISABLED") ? g_getenv("GH_PROMPT_DISABLED") : "",
        g_getenv("NO_COLOR") ? g_getenv("NO_COLOR") : "",
        g_getenv("GH_FORCE_TTY") ? g_getenv("GH_FORCE_TTY") : "",
        g_getenv("GH_DEBUG") ? g_getenv("GH_DEBUG") : "",
        g_getenv("DEBUG") ? g_getenv("DEBUG") : "");
    char *calls_path = g_build_filename(directory, "calls", NULL);
    char *previous_calls = NULL;
    if (g_file_get_contents(calls_path, &previous_calls, NULL, NULL)) {
        g_string_prepend(calls, previous_calls);
        g_free(previous_calls);
    }
    if (!g_file_set_contents(calls_path, calls->str, (gssize)calls->len, NULL)) return 93;
    g_free(calls_path);
    g_string_free(calls, TRUE);
    char *pid_path = g_build_filename(directory, "pid", NULL);
    char *pid_text = g_strdup_printf("%ld", (long)getpid());
    if (!g_file_set_contents(pid_path, pid_text, -1, NULL)) return 94;
    g_free(pid_path);
    g_free(pid_text);

    if (argc > 1 && strcmp(argv[1], "api") == 0) {
        fputs("Private probe diagnostics must stay hidden.\n", stdout);
        fputs("Private probe diagnostics from stderr.\n", stderr);
        if (strstr(mode, "invalid")) fputs("gh: Bad credentials (HTTP 401)\n", stderr);
        if (strcmp(mode, "probe-transient") == 0) fputs("dial tcp: network unavailable\n", stderr);
        fflush(stdout);
        fflush(stderr);
        if (strcmp(mode, "probe-cancel") == 0) g_usleep(10000000U);
        const gboolean valid = strstr(mode, "valid") != NULL && strstr(mode, "invalid") == NULL;
        g_free(mode);
        return valid ? 0 : 1;
    }

    if (strcmp(mode, "early-exit") == 0) {
        char text[6000];
        memset(text, 'x', sizeof(text));
        if (fwrite(text, 1U, sizeof(text), stderr) != sizeof(text)) return 95;
        fputs("\nImmediate failure detail retained.\n", stderr);
        fflush(stderr);
        g_free(mode);
        return 1;
    }

    fputs("Fixture output on stdout.\n", stdout);
    fflush(stdout);
    fputs("! First copy your one-time code: ", stderr);
    fflush(stderr);
    g_usleep(80000U);
    fputs("ABCD-", stderr);
    fflush(stderr);
    g_usleep(80000U);
    fputs("1234\nOpen https://github.com/login/device to continue.\n", stderr);
    fflush(stderr);

    if (strcmp(mode, "cancel") == 0) {
        g_usleep(10000000U);
        g_free(mode);
        return 0;
    }
    if (strcmp(mode, "inherited-pipe") == 0) {
        const pid_t descendant = fork();
        if (descendant < 0) return 96;
        if (descendant == 0) {
            g_usleep(10000000U);
            _exit(0);
        }
        char *path = g_build_filename(directory, "descendant", NULL);
        char *text = g_strdup_printf("%ld", (long)descendant);
        if (!g_file_set_contents(path, text, -1, NULL)) return 97;
        g_free(path);
        g_free(text);
        g_free(mode);
        return 0;
    }
    g_usleep(80000U);
    const gboolean failed = strcmp(mode, "failure") == 0;
    fputs(failed ? "Authentication failed: code expired.\n"
                 : "Authentication complete.\n", stderr);
    fflush(stderr);
    g_free(mode);
    return failed ? 1 : 0;
}

typedef struct {
    RunnerGithubAuth *auth;
    guint updates;
    guint completions;
    gboolean succeeded;
    gboolean code_before_completion;
    gboolean partial_before_completion;
    gboolean cancel_on_code;
    char *output;
    char *detail;
} Observation;

static void observe_update(const char *output, gpointer user_data)
{
    Observation *observation = user_data;
    observation->updates++;
    g_assert_cmpuint(observation->completions, ==, 0U);
    g_assert_true(g_utf8_validate(output, -1, NULL));
    g_free(observation->output);
    observation->output = g_strdup(output);
    if (strstr(output, "ABCD-") && !strstr(output, "ABCD-1234"))
        observation->partial_before_completion = TRUE;
    if (strstr(output, "ABCD-1234")) {
        observation->code_before_completion = TRUE;
        if (observation->cancel_on_code)
            runner_github_auth_cancel(observation->auth);
    }
}

static void observe_complete(gboolean succeeded, const char *detail, gpointer user_data)
{
    Observation *observation = user_data;
    observation->completions++;
    observation->succeeded = succeeded;
    observation->detail = g_strdup(detail);
}

static void set_mode(const char *mode)
{
    (void)g_unlink(fixture_calls);
    (void)g_unlink(fixture_pid);
    (void)g_unlink(fixture_descendant);
    g_assert_true(g_file_set_contents(fixture_mode, mode, -1, NULL));
}

static void drain_until_complete(Observation *observation)
{
    const gint64 deadline = g_get_monotonic_time() + 4000000;
    while (!observation->completions && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000U);
    }
    if (!observation->completions) {
        runner_github_auth_cancel(observation->auth);
        const gint64 cancel_deadline = g_get_monotonic_time() + 1000000;
        while (!observation->completions && g_get_monotonic_time() < cancel_deadline) {
            while (g_main_context_iteration(NULL, FALSE)) {}
            g_usleep(1000U);
        }
        g_error("Authentication did not complete within four seconds");
    }
    g_assert_cmpuint(observation->completions, ==, 1U);
}

static void start_observation(Observation *observation, RunnerGithubAuthIssue issue)
{
    GError *error = NULL;
    observation->auth = runner_github_auth_start(issue, observe_update,
        observe_complete, observation, &error);
    g_assert_no_error(error);
    g_assert_nonnull(observation->auth);
}

static void finish_observation(Observation *observation)
{
    runner_github_auth_free(observation->auth);
    g_free(observation->output);
    g_free(observation->detail);
}

static pid_t read_pid(const char *path)
{
    char *text = NULL;
    g_assert_true(g_file_get_contents(path, &text, NULL, NULL));
    const pid_t pid = (pid_t)g_ascii_strtoll(text, NULL, 10);
    g_free(text);
    g_assert_cmpint(pid, >, 0);
    return pid;
}

static void assert_child_reaped(void)
{
    const pid_t pid = read_pid(fixture_pid);
    errno = 0;
    g_assert_cmpint(kill(pid, 0), ==, -1);
    g_assert_cmpint(errno, ==, ESRCH);
    errno = 0;
    g_assert_cmpint(waitpid(pid, NULL, WNOHANG), ==, -1);
    g_assert_cmpint(errno, ==, ECHILD);
}

static void classifier_test(void)
{
    g_assert_cmpint(runner_github_auth_issue(NULL), ==, RUNNER_GITHUB_AUTH_NONE);
    g_assert_cmpint(runner_github_auth_issue("network unavailable"), ==, RUNNER_GITHUB_AUTH_NONE);
    g_assert_cmpint(runner_github_auth_issue("gh: this operation needs the ADMIN:ORG scope (HTTP 403)"), ==, RUNNER_GITHUB_AUTH_SCOPE);
    g_assert_cmpint(runner_github_auth_issue("insufficient scope"), ==, RUNNER_GITHUB_AUTH_SCOPE);
    g_assert_cmpint(runner_github_auth_issue("Run gh auth login"), ==, RUNNER_GITHUB_AUTH_LOGIN);
    g_assert_cmpint(runner_github_auth_issue("not logged in"), ==, RUNNER_GITHUB_AUTH_LOGIN);
    g_assert_cmpint(runner_github_auth_issue("HTTP 401"), ==, RUNNER_GITHUB_AUTH_LOGIN);
    g_assert_cmpint(runner_github_auth_issue("organisation runner access denied (HTTP 403)"), ==, RUNNER_GITHUB_AUTH_ACCESS);
}

static void environment_token_test(void)
{
    const char *variables[] = {"GH_TOKEN", "GITHUB_TOKEN"};
    set_mode("success");
    for (guint i = 0U; i < G_N_ELEMENTS(variables); ++i) {
        g_setenv(variables[i], "offline-fixture-token", TRUE);
        g_assert_true(runner_github_auth_environment_token());
        Observation observation = {0};
        GError *error = NULL;
        RunnerGithubAuth *auth = runner_github_auth_start(RUNNER_GITHUB_AUTH_SCOPE,
            observe_update, observe_complete, &observation, &error);
        g_assert_null(auth);
        g_assert_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
        g_assert_nonnull(strstr(error->message, "GH_TOKEN"));
        g_clear_error(&error);
        g_assert_false(g_file_test(fixture_calls, G_FILE_TEST_EXISTS));
        g_assert_cmpuint(observation.updates, ==, 0U);
        g_assert_cmpuint(observation.completions, ==, 0U);
        g_unsetenv(variables[i]);
    }
    g_setenv("GH_TOKEN", "", TRUE);
    g_setenv("GITHUB_TOKEN", "", TRUE);
    g_assert_false(runner_github_auth_environment_token());
    g_unsetenv("GH_TOKEN");
    g_unsetenv("GITHUB_TOKEN");
}

static void login_stream_test(void)
{
    set_mode("success");
    Observation observation = {0};
    start_observation(&observation, RUNNER_GITHUB_AUTH_LOGIN);
    drain_until_complete(&observation);
    g_assert_true(observation.succeeded);
    g_assert_true(observation.partial_before_completion);
    g_assert_true(observation.code_before_completion);
    g_assert_cmpuint(observation.updates, >=, 3U);
    g_assert_nonnull(strstr(observation.output, "Fixture output on stdout."));
    g_assert_nonnull(strstr(observation.output, "Authentication complete."));
    char *calls = NULL;
    g_assert_true(g_file_get_contents(fixture_calls, &calls, NULL, NULL));
    g_assert_nonnull(strstr(calls, "auth\nlogin\n--hostname\ngithub.com\n--web\n--scopes\nadmin:org\n"));
    g_assert_null(strstr(calls, "--git-protocol"));
    g_assert_nonnull(strstr(calls, "GH_PROMPT_DISABLED=1\nNO_COLOR=1\nGH_FORCE_TTY=\nGH_DEBUG=\nDEBUG=\n"));
    g_free(calls);
    assert_child_reaped();
    finish_observation(&observation);
}

static void refresh_failure_test(void)
{
    set_mode("failure");
    Observation observation = {0};
    start_observation(&observation, RUNNER_GITHUB_AUTH_SCOPE);
    drain_until_complete(&observation);
    g_assert_false(observation.succeeded);
    g_assert_true(observation.code_before_completion);
    g_assert_nonnull(strstr(observation.output, "code expired"));
    char *calls = NULL;
    g_assert_true(g_file_get_contents(fixture_calls, &calls, NULL, NULL));
    g_assert_nonnull(strstr(calls, "auth\nrefresh\n--hostname\ngithub.com\n--scopes\nadmin:org\n"));
    g_assert_null(strstr(calls, "--web"));
    g_free(calls);
    assert_child_reaped();
    finish_observation(&observation);
}

static void cancellation_test(void)
{
    set_mode("cancel");
    Observation observation = {.cancel_on_code = TRUE};
    start_observation(&observation, RUNNER_GITHUB_AUTH_LOGIN);
    const gint64 started = g_get_monotonic_time();
    drain_until_complete(&observation);
    g_assert_false(observation.succeeded);
    g_assert_true(observation.code_before_completion);
    g_assert_nonnull(strstr(observation.detail, "cancelled"));
    g_assert_cmpint(g_get_monotonic_time() - started, <, 2000000);
    assert_child_reaped();
    finish_observation(&observation);
}

static void immediate_exit_test(void)
{
    set_mode("early-exit");
    Observation observation = {0};
    start_observation(&observation, RUNNER_GITHUB_AUTH_LOGIN);
    /* Let process exit before dispatching either async completion. Its output
     * exceeds one read buffer, so the last chunk must survive wait completion. */
    g_usleep(30000U);
    drain_until_complete(&observation);
    g_assert_false(observation.succeeded);
    g_assert_nonnull(strstr(observation.output, "Immediate failure detail retained."));
    assert_child_reaped();
    finish_observation(&observation);
}

static void inherited_pipe_test(void)
{
    set_mode("inherited-pipe");
    Observation observation = {0};
    start_observation(&observation, RUNNER_GITHUB_AUTH_LOGIN);
    const gint64 started = g_get_monotonic_time();
    drain_until_complete(&observation);
    const pid_t descendant = read_pid(fixture_descendant);
    /* The child retaining stdout must not delay completion for ten seconds. */
    g_assert_true(observation.succeeded);
    g_assert_true(observation.code_before_completion);
    g_assert_cmpint(g_get_monotonic_time() - started, <, 2000000);
    assert_child_reaped();
    g_assert_cmpint(kill(descendant, SIGKILL), ==, 0);
    g_assert_cmpint(waitpid(descendant, NULL, 0), ==, descendant);
    finish_observation(&observation);
}

typedef struct {
    const char *mode;
    RunnerGithubAuthIssue issue;
    const char *expected_command;
} ProbeCase;

static void preserved_scope_test(gconstpointer user_data)
{
    const ProbeCase *test_case = user_data;
    set_mode(test_case->mode);
    Observation observation = {0};
    start_observation(&observation, test_case->issue);
    drain_until_complete(&observation);
    g_assert_true(observation.succeeded);
    g_assert_true(observation.code_before_completion);
    g_assert_nonnull(observation.output);
    g_assert_null(strstr(observation.output, "Private probe diagnostics"));
    char *calls = NULL;
    g_assert_true(g_file_get_contents(fixture_calls, &calls, NULL, NULL));
    g_assert_nonnull(strstr(calls, "api\n--hostname\ngithub.com\n/user\n--jq\n.login\n"));
    g_assert_nonnull(strstr(calls, test_case->expected_command));
    g_assert_null(strstr(calls, "--git-protocol"));
    g_free(calls);
    assert_child_reaped();
    finish_observation(&observation);
}

static void probe_cancellation_test(void)
{
    set_mode("probe-cancel");
    Observation observation = {0};
    start_observation(&observation, RUNNER_GITHUB_AUTH_ACCESS);
    const gint64 deadline = g_get_monotonic_time() + 4000000;
    while (!g_file_test(fixture_pid, G_FILE_TEST_EXISTS) && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000U);
    }
    g_assert_true(g_file_test(fixture_pid, G_FILE_TEST_EXISTS));
    runner_github_auth_cancel(observation.auth);
    drain_until_complete(&observation);
    g_assert_false(observation.succeeded);
    g_assert_cmpuint(observation.updates, ==, 0U);
    g_assert_null(observation.output);
    g_assert_nonnull(strstr(observation.detail, "cancelled"));
    char *calls = NULL;
    g_assert_true(g_file_get_contents(fixture_calls, &calls, NULL, NULL));
    g_assert_nonnull(strstr(calls, "api\n--hostname\ngithub.com\n/user\n--jq\n.login\n"));
    g_assert_null(strstr(calls, "auth\nlogin\n"));
    g_assert_null(strstr(calls, "auth\nrefresh\n"));
    g_free(calls);
    assert_child_reaped();
    finish_observation(&observation);
}

static void probe_transient_failure_test(void)
{
    set_mode("probe-transient");
    Observation observation = {0};
    start_observation(&observation, RUNNER_GITHUB_AUTH_NONE);
    drain_until_complete(&observation);
    g_assert_false(observation.succeeded);
    g_assert_nonnull(strstr(observation.detail, "try again"));
    g_assert_nonnull(strstr(observation.detail, "saved credentials were kept"));
    g_assert_nonnull(observation.output);
    g_assert_null(strstr(observation.output, "Private probe diagnostics"));
    g_assert_null(strstr(observation.output, "dial tcp"));
    char *calls = NULL;
    g_assert_true(g_file_get_contents(fixture_calls, &calls, NULL, NULL));
    g_assert_nonnull(strstr(calls, "api\n--hostname\ngithub.com\n/user\n--jq\n.login\n"));
    g_assert_null(strstr(calls, "auth\nlogin\n"));
    g_assert_null(strstr(calls, "auth\nrefresh\n"));
    g_free(calls);
    assert_child_reaped();
    finish_observation(&observation);
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "auth") == 0 || strcmp(argv[1], "api") == 0))
        return fake_gh(argc, argv);
    g_test_init(&argc, &argv, NULL);
    GError *error = NULL;
    fixture_root = g_dir_make_tmp("runnerscope-auth-test-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(fixture_root);
    fixture_mode = g_build_filename(fixture_root, "mode", NULL);
    fixture_calls = g_build_filename(fixture_root, "calls", NULL);
    fixture_pid = g_build_filename(fixture_root, "pid", NULL);
    fixture_descendant = g_build_filename(fixture_root, "descendant", NULL);
    char *executable = g_file_read_link("/proc/self/exe", &error);
    g_assert_no_error(error);
    char *fake_path = g_build_filename(fixture_root, "gh", NULL);
    g_assert_cmpint(symlink(executable, fake_path), ==, 0);
    g_free(executable);
    char *old_path = g_strdup(g_getenv("PATH"));
    char *path = g_strconcat(fixture_root, ":", old_path ? old_path : "", NULL);
    g_setenv("PATH", path, TRUE);
    g_setenv("RUNNER_AUTH_TEST_ROOT", fixture_root, TRUE);
    g_setenv("GH_FORCE_TTY", "120", TRUE);
    g_setenv("GH_DEBUG", "api", TRUE);
    g_setenv("DEBUG", "1", TRUE);
    g_unsetenv("GH_TOKEN");
    g_unsetenv("GITHUB_TOKEN");
    g_free(path);
    /* Adopt the intentional pipe-holder so the test can kill and reap it. */
    g_assert_cmpint(prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0), ==, 0);
    g_test_add_func("/github-auth/classifier", classifier_test);
    g_test_add_func("/github-auth/environment-token", environment_token_test);
    g_test_add_func("/github-auth/login-stream", login_stream_test);
    g_test_add_func("/github-auth/refresh-failure", refresh_failure_test);
    g_test_add_func("/github-auth/cancellation", cancellation_test);
    g_test_add_func("/github-auth/immediate-exit", immediate_exit_test);
    g_test_add_func("/github-auth/inherited-pipe", inherited_pipe_test);
    static const ProbeCase probe_cases[] = {
        {"access-valid", RUNNER_GITHUB_AUTH_ACCESS, "auth\nrefresh\n"},
        {"access-invalid", RUNNER_GITHUB_AUTH_ACCESS, "auth\nlogin\n"},
        {"none-valid", RUNNER_GITHUB_AUTH_NONE, "auth\nrefresh\n"},
        {"none-invalid", RUNNER_GITHUB_AUTH_NONE, "auth\nlogin\n"}
    };
    g_test_add_data_func("/github-auth/access-valid", &probe_cases[0], preserved_scope_test);
    g_test_add_data_func("/github-auth/access-invalid", &probe_cases[1], preserved_scope_test);
    g_test_add_data_func("/github-auth/none-valid", &probe_cases[2], preserved_scope_test);
    g_test_add_data_func("/github-auth/none-invalid", &probe_cases[3], preserved_scope_test);
    g_test_add_func("/github-auth/probe-cancellation", probe_cancellation_test);
    g_test_add_func("/github-auth/probe-transient-failure", probe_transient_failure_test);
    const int result = g_test_run();
    g_free(old_path);
    (void)g_unlink(fake_path);
    (void)g_unlink(fixture_mode);
    (void)g_unlink(fixture_calls);
    (void)g_unlink(fixture_pid);
    (void)g_unlink(fixture_descendant);
    (void)g_rmdir(fixture_root);
    g_free(fake_path);
    g_free(fixture_mode);
    g_free(fixture_calls);
    g_free(fixture_pid);
    g_free(fixture_descendant);
    g_free(fixture_root);
    return result;
}
