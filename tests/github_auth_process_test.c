// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "platform/github_sign_in.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { bool cancel; bool stop_on_code; unsigned int codes; } Fixture;
static void environment(const char *name, const char *value)
{
#ifdef _WIN32
    assert(_putenv_s(name, value) == 0);
#else
    assert(setenv(name, value, 1) == 0);
#endif
}
static bool cancelled(void *context) { return ((Fixture *)context)->cancel; }
static void challenge(const char *code, void *context)
{
    Fixture *f = context;
    assert(strcmp(code, "ABCD-EFGH") == 0);
    f->codes++;
    if (f->stop_on_code) f->cancel = true;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    environment("GH_TOKEN", "must_not_be_inherited");
    environment("GITHUB_TOKEN", "must_not_be_inherited");
    environment("GH_DEBUG", "api");
    environment("GH_FORCE_TTY", "1");
    environment("GH_CONFIG_DIR", "runnerscope-nonexistent-fixture-directory");
    environment("RUNNERSCOPE_AUTH_FIXTURE", "success");
    Fixture f = {false, false, 0U};
    RunnerAuthCallbacks callbacks = {cancelled, challenge, &f};
    RunnerGithubAuth auth;
    runner_github_sign_in_with_executable(&auth, "FixtureOrg", &callbacks, argv[1], NULL, NULL);
    assert(auth.phase == RUNNER_AUTH_COMPLETE);
    assert(strcmp(auth.token, "offline_process_token_1234567890") == 0 && f.codes == 1U);
    runner_github_auth_clear(&auth);
    f = (Fixture){false, true, 0U}; environment("RUNNERSCOPE_AUTH_FIXTURE", "hang");
    runner_github_sign_in_with_executable(&auth, "FixtureOrg", &callbacks, argv[1], NULL, NULL);
    assert(auth.phase == RUNNER_AUTH_FAILED && !auth.token[0] && f.codes == 1U);
    f = (Fixture){0}; environment("RUNNERSCOPE_AUTH_FIXTURE", "bad-token");
    runner_github_sign_in_with_executable(&auth, "FixtureOrg", &callbacks, argv[1], NULL, NULL);
    assert(auth.phase == RUNNER_AUTH_FAILED && !auth.token[0] && !strstr(auth.error, "secret"));
    f = (Fixture){0}; environment("RUNNERSCOPE_AUTH_FIXTURE", "long-line");
    runner_github_sign_in_with_executable(&auth, "FixtureOrg", &callbacks, argv[1], NULL, NULL);
    assert(auth.phase == RUNNER_AUTH_FAILED && !auth.token[0]);
    puts("Sign-in child process, fragmented code, private environment, cancellation and failure checks passed.");
    return 0;
}
