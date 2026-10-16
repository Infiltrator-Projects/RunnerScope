// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github_auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "auth check failed at line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct { const char *json; unsigned int status; unsigned int calls; bool fail; bool bad_request; } Fixture;
static bool post(const char *path, const char *form, RunnerHttpResponse *response,
    char *error, size_t size, void *context)
{
    Fixture *f = context;
    f->calls++;
    if (strcmp(path, "/login/device/code") != 0 && strcmp(path, "/login/oauth/access_token") != 0) f->bad_request = true;
    if (!strstr(form, "client_id=offline-client") || strstr(form, "client_secret=")) f->bad_request = true;
    if (strcmp(path, "/login/device/code") == 0 && !strstr(form, "scope=admin%3Aorg%20repo")) f->bad_request = true;
    if (strcmp(path, "/login/oauth/access_token") == 0 && !strstr(form, "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code")) f->bad_request = true;
    if (f->fail) { snprintf(error, size, "do not display offline-secret"); return false; }
    response->status = f->status;
    response->body = malloc(strlen(f->json) + 1U);
    if (!response->body) return false;
    memcpy(response->body, f->json, strlen(f->json) + 1U);
    return true;
}
static const char *code_json =
    "{\"device_code\":\"fixture-device\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://github.com/login/device\",\"expires_in\":900,\"interval\":5}";
static RunnerAuthPhase step(RunnerGithubAuth *auth, uint64_t now, Fixture *f)
{ return runner_github_auth_step(auth, "offline-client", now, post, f); }

int main(void)
{
    RunnerGithubAuth auth = {0};
    Fixture f = {code_json, 200U, 0U, false, false};
    CHECK(runner_github_auth_step(&auth, "", 0U, post, &f) == RUNNER_AUTH_FAILED);
    CHECK(f.calls == 0U);
    CHECK(strstr(auth.error, "not been configured"));
    runner_github_auth_clear(&auth);
    CHECK(runner_github_auth_step(&auth, "bad&client", 0U, post, &f) == RUNNER_AUTH_FAILED);
    CHECK(f.calls == 0U);
    runner_github_auth_clear(&auth);
    CHECK(step(&auth, 1000U, &f) == RUNNER_AUTH_WAITING);
    CHECK(strcmp(auth.user_code, "ABCD-EFGH") == 0 && auth.next_poll_ms == 6000U);
    CHECK(step(&auth, 5999U, &f) == RUNNER_AUTH_WAITING && f.calls == 1U);
    f.json = "{\"error\":\"authorization_pending\"}";
    CHECK(step(&auth, 6000U, &f) == RUNNER_AUTH_WAITING && f.calls == 2U);
    f.json = "{\"error\":\"slow_down\",\"interval\":20}";
    CHECK(step(&auth, 11000U, &f) == RUNNER_AUTH_WAITING);
    CHECK(auth.interval_seconds == 20U && auth.next_poll_ms == 31000U);
    CHECK(step(&auth, 30999U, &f) == RUNNER_AUTH_WAITING && f.calls == 3U);
    f.json = "{\"access_token\":\"offline-access-token\",\"token_type\":\"bearer\",\"scope\":\"admin:org,repo\"}";
    CHECK(step(&auth, 31000U, &f) == RUNNER_AUTH_COMPLETE);
    CHECK(strcmp(auth.token, "offline-access-token") == 0);
    CHECK(!auth.device_code[0] && !auth.user_code[0] && !f.bad_request);
    CHECK(step(&auth, 32000U, &f) == RUNNER_AUTH_COMPLETE && f.calls == 4U);
    runner_github_auth_clear(&auth);
    for (size_t i = 0U; i < sizeof(auth); ++i) CHECK(((unsigned char *)&auth)[i] == 0U);
    const char *errors[] = {"access_denied", "expired_token", "token_expired", "incorrect_client_credentials", "device_flow_disabled", "offline-secret"};
    for (size_t i = 0U; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        runner_github_auth_clear(&auth);
        f.json = code_json;
        CHECK(step(&auth, 1000U, &f) == RUNNER_AUTH_WAITING);
        char json[256];
        snprintf(json, sizeof(json), "{\"error\":\"%s\",\"error_description\":\"offline-secret\"}", errors[i]);
        f.json = json;
        CHECK(step(&auth, 6000U, &f) == RUNNER_AUTH_FAILED);
        CHECK(!strstr(auth.error, "offline-secret") && !auth.device_code[0] && !auth.token[0]);
    }
    runner_github_auth_clear(&auth); f.json = code_json;
    CHECK(step(&auth, 1000U, &f) == RUNNER_AUTH_WAITING);
    const unsigned int before = f.calls;
    CHECK(step(&auth, 901000U, &f) == RUNNER_AUTH_FAILED && f.calls == before);
    runner_github_auth_clear(&auth); f.fail = true;
    CHECK(step(&auth, 0U, &f) == RUNNER_AUTH_FAILED && !strstr(auth.error, "offline-secret"));
    f.fail = false;
    runner_github_auth_clear(&auth); f.status = 400U;
    f.json = "{\"error\":\"device_flow_disabled\"}";
    CHECK(step(&auth, 0U, &f) == RUNNER_AUTH_FAILED && strstr(auth.error, "Device Flow"));
    f.status = 200U;
    const char *invalid[] = {
        "{}", "{bad-json", "null", "[]",
        "{\"device_code\":\"fixture-device\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://github.com.evil.invalid/login/device\",\"expires_in\":900,\"interval\":5}",
        "{\"device_code\":\"fixture-device\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://github.com/login/device\",\"expires_in\":900,\"interval\":0}"
    };
    for (size_t i = 0U; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        runner_github_auth_clear(&auth); f.json = invalid[i];
        CHECK(step(&auth, 0U, &f) == RUNNER_AUTH_FAILED);
    }
    runner_github_auth_clear(&auth); f.json = code_json;
    CHECK(step(&auth, 0U, &f) == RUNNER_AUTH_WAITING);
    f.json = "{\"access_token\":\"offline-secret\",\"token_type\":\"invalid\"}";
    CHECK(step(&auth, 5000U, &f) == RUNNER_AUTH_FAILED && !auth.token[0] && !strstr(auth.error, "offline-secret"));
    puts("Native OAuth passed: browser codes, polling, slow-down, expiry, cancellation, invalid responses and secret-free errors.");
    return 0;
}
