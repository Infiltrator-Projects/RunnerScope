// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github_auth.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned int calls[4]; bool existing, deny, cancel, bad; const char *members; } Fixture;
static bool cancelled(void *context) { return ((Fixture *)context)->cancel; }
static bool execute(RunnerAuthCommand command, char *out, size_t size, const RunnerAuthCallbacks *callbacks, void *context)
{
    (void)callbacks; Fixture *f = context; f->calls[command]++;
    if (command == RUNNER_AUTH_EXISTING_SESSION && !f->existing) return false;
    if (command == RUNNER_AUTH_BROWSER_LOGIN) return !f->deny;
    (void)snprintf(out, size, "%s", f->bad ? "untrusted output\nsecret" : "offline_auth_token_1234567890\n");
    return true;
}
static bool get(const char *url, const char *token, RunnerHttpResponse *response, char *error, size_t size, void *context)
{
    (void)error; (void)size; Fixture *f = context;
    assert(strcmp(token, "offline_auth_token_1234567890") == 0);
    response->status = f->deny ? 403U : 200U; response->link = NULL;
    const char *body = strstr(url, "/memberships/") ? f->members : "{\"runners\":[]}";
    if (!body) body = "[]";
    response->body = malloc(strlen(body) + 1U); assert(response->body);
    strcpy(response->body, body); return true;
}
int main(void)
{
    char code[32] = "", token[128] = "";
    assert(runner_github_auth_code("! First copy your one-time code: ABCD-EFGH\r", code));
    assert(strcmp(code, "ABCD-EFGH") == 0);
    assert(!runner_github_auth_code("! First copy your one-time code: ABCD-EFGH injected", code));
    assert(!runner_github_auth_code("! First copy your one-time code: ABCD-EF", code));
    assert(!runner_github_auth_code("! First copy your one-time code: abcd-EFGH", code));
    assert(!runner_github_auth_code("https://evil.invalid", code));
    assert(runner_github_auth_token("offline_auth_token_1234567890\r\n", token, sizeof(token)));
    assert(!runner_github_auth_token("offline_auth_token_1234567890\nother", token, sizeof(token)));
    assert(!runner_github_auth_token("short", token, sizeof(token)));
    Fixture f = {0}; RunnerGithubAuth auth;
    RunnerAuthCallbacks callbacks = {cancelled, NULL, &f};
    runner_github_authenticate(&auth, "Org", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_COMPLETE && f.calls[RUNNER_AUTH_BROWSER_LOGIN] == 1U);
    assert(strcmp(auth.organisation, "Org") == 0);
    f = (Fixture){0}; f.existing = true;
    runner_github_authenticate(&auth, "Org", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_COMPLETE && f.calls[RUNNER_AUTH_BROWSER_LOGIN] == 0U);
    f = (Fixture){0}; f.deny = true;
    runner_github_authenticate(&auth, "Org", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_FAILED && !auth.token[0]);
    assert(!strstr(auth.error, "offline_auth_token"));
    f = (Fixture){0}; f.cancel = true;
    runner_github_authenticate(&auth, "Org", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_FAILED && f.calls[RUNNER_AUTH_BROWSER_LOGIN] == 0U);
    f = (Fixture){0}; f.bad = true;
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_FAILED && !auth.token[0]);
    assert(!strstr(auth.error, "secret"));
    f = (Fixture){0}; f.members = "[{\"state\":\"active\",\"role\":\"admin\",\"organization\":{\"login\":\"OnlyOrg\"}}]";
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_COMPLETE && strcmp(auth.organisation, "OnlyOrg") == 0);
    f.members = "[{\"state\":\"active\",\"role\":\"admin\",\"organization\":{\"login\":\"One\"}},{\"state\":\"active\",\"role\":\"admin\",\"organization\":{\"login\":\"Two\"}}]";
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_COMPLETE && !auth.organisation[0] && auth.organisation_count == 2U);
    f.members = "[{\"state\":\"active\",\"role\":\"member\",\"organization\":{\"login\":\"MemberOrg\"}}]";
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(!auth.organisation[0] && auth.organisation_count == 0U);
    f = (Fixture){0}; f.existing = true;
    f.members = "[{\"state\":\"active\",\"role\":\"admin\",\"organization\":{\"login\":\"OnlyOrg\"}}]";
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(auth.phase == RUNNER_AUTH_COMPLETE && !strcmp(auth.organisation, "OnlyOrg"));
    assert(f.calls[RUNNER_AUTH_BROWSER_LOGIN] == 0U);
    f = (Fixture){0};
    f.members = "[{\"state\":\"active\",\"role\":\"admin\",\"organization\":{\"login\":\"Org\\u0000suffix\"}}]";
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(!auth.organisation[0] && auth.organisation_count == 0U);
    f.members = "[{\"state\":\"active\",\"role\":\"admin\",\"organization\":{\"login\":\"One\",\"login\":\"Two\"}}]";
    runner_github_authenticate(&auth, "", execute, &f, get, &f, &callbacks);
    assert(!auth.organisation[0] && auth.organisation_count == 0U);
    runner_github_auth_clear(&auth); assert(!auth.token[0] && !auth.organisation[0]);
    puts("GitHub sign-in reuse, browser flow, discovery and failure contracts passed.");
    return 0;
}
