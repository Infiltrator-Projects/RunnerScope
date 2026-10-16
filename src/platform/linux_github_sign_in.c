// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "platform/github_sign_in.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
typedef struct { const char *executable; char *directory; } Broker;
static void remove_config(const char *directory)
{
    GDir *dir = g_dir_open(directory, 0U, NULL);
    if (dir) {
        const char *name;
        while ((name = g_dir_read_name(dir))) {
            char *p = g_build_filename(directory, name, NULL);
            (void)g_unlink(p); g_free(p);
        }
        g_dir_close(dir);
    }
    (void)g_rmdir(directory);
}
static bool execute(RunnerAuthCommand command, char *output, size_t capacity,
    const RunnerAuthCallbacks *callbacks, void *context)
{
    Broker *broker = context;
    if (!output || !capacity) return false;
    output[0] = '\0';
    char **environment = g_get_environ();
    const char *clear[] = {"GH_TOKEN", "GITHUB_TOKEN", "GH_ENTERPRISE_TOKEN", "GITHUB_ENTERPRISE_TOKEN",
        "GH_DEBUG", "GH_FORCE_TTY", "CLICOLOR_FORCE", "FORCE_COLOR", "GH_BROWSER", "GH_HTTP_UNIX_SOCKET", NULL};
    for (size_t i = 0U; clear[i]; ++i) environment = g_environ_unsetenv(environment, clear[i]);
    const char *pairs[][2] = {{"GH_PROMPT_DISABLED","1"},{"GH_HOST","github.com"},{"NO_COLOR","1"},
        {"CLICOLOR","0"},{"LC_ALL","C"},{"GH_NO_UPDATE_NOTIFIER","1"},{"GH_NO_EXTENSION_UPDATE_NOTIFIER","1"}};
    for (size_t i = 0U; i < G_N_ELEMENTS(pairs); ++i)
        environment = g_environ_setenv(environment, pairs[i][0], pairs[i][1], TRUE);
    if (command != RUNNER_AUTH_EXISTING_SESSION)
        environment = g_environ_setenv(environment, "GH_CONFIG_DIR", broker->directory, TRUE);
    char *login[] = {(char *)broker->executable, "auth", "login", "--web", "--hostname", "github.com",
        "--git-protocol", "https", "--skip-ssh-key", "--scopes", "admin:org,repo",
        "--clipboard=false", "--insecure-storage", NULL};
    char *read_args[] = {(char *)broker->executable, "auth", "token", "--hostname", "github.com", NULL};
    char *version[] = {(char *)broker->executable, "--version", NULL};
    const bool login_command = command == RUNNER_AUTH_BROWSER_LOGIN;
    GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CLOEXEC_PIPES |
        (login_command ? G_SPAWN_STDOUT_TO_DEV_NULL : G_SPAWN_STDERR_TO_DEV_NULL);
    GError *error = NULL;
    GPid child = 0;
    int pipe_fd = -1;
    const gboolean started = g_spawn_async_with_pipes(NULL,
        login_command ? login : command == RUNNER_AUTH_VERSION ? version : read_args, environment,
        flags, NULL, NULL, &child, NULL, login_command ? NULL : &pipe_fd,
        login_command ? &pipe_fd : NULL, &error);
    g_strfreev(environment); g_clear_error(&error);
    if (!started) return false;
    bool failed = fcntl(pipe_fd, F_SETFL, O_NONBLOCK) != 0;
    bool eof = false, exited = false;
    int status = 0;
    const gint64 deadline = g_get_monotonic_time() +
        (login_command ? 16 * 60 * G_TIME_SPAN_SECOND : 20 * G_TIME_SPAN_SECOND);
    char line[4096] = "";
    size_t used = 0U, captured = 0U;
    while ((!eof || !exited) && !failed) {
        if (runner_github_auth_cancelled(callbacks) || g_get_monotonic_time() >= deadline) { failed = true; break; }
        char bytes[512];
        const ssize_t n = eof ? -1 : read(pipe_fd, bytes, sizeof(bytes));
        if (n == 0) eof = true;
        else if (n < 0 && !eof && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) failed = true;
        else if (n > 0) for (ssize_t i = 0; i < n; ++i) {
            if (!login_command) {
                if (captured + 1U >= capacity || bytes[i] == '\0') { failed = true; break; }
                output[captured++] = bytes[i]; output[captured] = '\0';
            } else if (bytes[i] == '\n') {
                line[used] = '\0'; char code[32];
                if (runner_github_auth_code(line, code) && callbacks && callbacks->challenge)
                    callbacks->challenge(code, callbacks->context);
                memset(line, 0, sizeof(line)); used = 0U;
            } else if (used + 1U < sizeof(line)) line[used++] = bytes[i];
            else { failed = true; break; }
        }
        if (!exited) {
            const pid_t done = waitpid(child, &status, WNOHANG);
            if (done == child) exited = true;
            else if (done < 0 && errno != EINTR) { failed = true; exited = true; }
        }
        if (n <= 0 && !failed && (!eof || !exited)) g_usleep(20000U);
    }
    memset(line, 0, sizeof(line));
    close(pipe_fd);
    if (!exited) {
        (void)kill(child, SIGKILL);
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    }
    g_spawn_close_pid(child);
    const bool success = !failed && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!success) memset(output, 0, capacity);
    return success;
}
static const char *broker_path(void)
{
    char *self = g_file_read_link("/proc/self/exe", NULL);
    char *parent = self ? g_path_get_dirname(self) : NULL;
    const bool build = parent && strcmp(parent, RUNNERSCOPE_BUILD_ROOT) == 0;
    g_free(parent); g_free(self);
    return build ? RUNNERSCOPE_GH_BUILD_EXECUTABLE : RUNNERSCOPE_GH_EXECUTABLE;
}
bool runner_github_sign_in_available(void) { return g_file_test(broker_path(), G_FILE_TEST_IS_EXECUTABLE); }
bool runner_github_sign_in_probe(void)
{
    Broker broker = {broker_path(), NULL};
    char output[512];
    return execute(RUNNER_AUTH_VERSION, output, sizeof(output), NULL, &broker) &&
        strstr(output, "gh version " RUNNERSCOPE_GH_VERSION) != NULL;
}
void runner_github_sign_in_with_executable(RunnerGithubAuth *auth, const char *organisation,
    const RunnerAuthCallbacks *callbacks, const char *executable, RunnerHttpGet get, void *http_context)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("runnerscope-sign-in-XXXXXX", &error);
    g_clear_error(&error);
    if (!directory || g_chmod(directory, 0700) != 0) {
        runner_github_auth_clear(auth); auth->phase = RUNNER_AUTH_FAILED;
        g_strlcpy(auth->error, "Unable to prepare private GitHub sign-in storage.", sizeof(auth->error));
        if (directory) { remove_config(directory); g_free(directory); }
        return;
    }
    Broker broker = {executable, directory};
    runner_github_authenticate(auth, organisation, execute, &broker, get, http_context, callbacks);
    remove_config(directory); g_free(directory);
}
void runner_github_sign_in(RunnerGithubAuth *auth, const char *organisation,
    const RunnerAuthCallbacks *callbacks)
{
    runner_github_sign_in_with_executable(auth, organisation, callbacks,
        broker_path(), runner_http_get, NULL);
}
