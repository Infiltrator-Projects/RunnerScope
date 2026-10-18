// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github_auth.h"
#include "core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void runner_github_auth_clear(RunnerGithubAuth *auth)
{
    if (!auth) return;
    volatile unsigned char *p = (volatile unsigned char *)auth;
    for (size_t i = 0U; i < sizeof(*auth); ++i) p[i] = 0U;
}
bool runner_github_auth_cancelled(const RunnerAuthCallbacks *callbacks)
{
    return callbacks && callbacks->cancelled && callbacks->cancelled(callbacks->context);
}
static bool org_name(const char *s)
{
    if (!s || !s[0] || strlen(s) >= 128U) return false;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.')) return false;
    return true;
}
bool runner_github_auth_code(const char *line, char code[32])
{
    if (!line || !code) return false;
    /* Pinned official CLI, NO_COLOR=1 and C locale. Never interpret a URL or command. */
    const char *prefix = "First copy your one-time code: ";
    const char *p = strstr(line, prefix);
    if (!p) return false;
    p += strlen(prefix);
    for (size_t i = 0U; i < 9U; ++i) {
        if (!p[i]) return false;
        if (i == 4U) { if (p[i] != '-') return false; }
        else if (!((p[i] >= 'A' && p[i] <= 'Z') || (p[i] >= '0' && p[i] <= '9'))) return false;
    }
    if (p[9] && p[9] != '\r' && p[9] != '\n') return false;
    memcpy(code, p, 9U); code[9] = '\0';
    return true;
}
bool runner_github_auth_token(const char *output, char *token, size_t capacity)
{
    if (!output || !token || !capacity) return false;
    size_t n = strlen(output);
    while (n && (output[n - 1U] == '\r' || output[n - 1U] == '\n')) --n;
    if (n < 20U || n >= capacity) return false;
    for (size_t i = 0U; i < n; ++i) {
        const unsigned char c = (unsigned char)output[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    memcpy(token, output, n); token[n] = '\0';
    return true;
}
static cJSON *request_json(const char *url, const char *token, RunnerHttpGet get, void *context)
{
    if (!get) return NULL;
    RunnerHttpResponse response = {0U, NULL, NULL};
    char error[320] = "";
    const bool ok = get(url, token, &response, error, sizeof(error), context);
    cJSON *json = ok && response.status == 200U
        ? runner_json_parse(response.body, 1024U * 1024U) : NULL;
    free(response.body); free(response.link);
    return json;
}
static bool can_monitor(const char *org, const char *token, RunnerHttpGet get, void *context)
{
    if (!org_name(org)) return false;
    char url[320];
    (void)snprintf(url, sizeof(url), "https://api.github.com/orgs/%s/actions/runners?per_page=1", org);
    cJSON *json = request_json(url, token, get, context);
    const bool ok = json && cJSON_IsArray(runner_json_unique_member(json, "runners"));
    cJSON_Delete(json);
    return ok;
}
static void discover_organisations(RunnerGithubAuth *auth, RunnerHttpGet get, void *context)
{
    if (auth->organisation[0]) return;
    cJSON *json = request_json("https://api.github.com/user/memberships/orgs?state=active&per_page=100",
        auth->token, get, context);
    if (!cJSON_IsArray(json)) { cJSON_Delete(json); return; }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, json) {
        const cJSON *state = runner_json_unique_member(entry, "state");
        const cJSON *role = runner_json_unique_member(entry, "role");
        const cJSON *org = runner_json_unique_member(entry, "organization");
        const cJSON *login = runner_json_unique_member(org, "login");
        if (!cJSON_IsString(state) || strcmp(state->valuestring, "active") != 0 ||
            !cJSON_IsString(role) || strcmp(role->valuestring, "admin") != 0 ||
            !cJSON_IsString(login) || !org_name(login->valuestring)) continue;
        bool duplicate = false;
        for (size_t i = 0U; i < auth->organisation_count; ++i)
            if (strcmp(auth->organisations[i], login->valuestring) == 0) duplicate = true;
        if (!duplicate && auth->organisation_count < RUNNER_AUTH_MAX_ORGANISATIONS) {
            (void)snprintf(auth->organisations[auth->organisation_count], 128U, "%s", login->valuestring);
            ++auth->organisation_count;
        }
    }
    /* A full page might have more memberships. Never guess a sole organisation. */
    if (cJSON_IsArray(json) && cJSON_GetArraySize(json) < 100 && auth->organisation_count == 1U)
        (void)snprintf(auth->organisation, sizeof(auth->organisation), "%s", auth->organisations[0]);
    cJSON_Delete(json);
}
static void fail(RunnerGithubAuth *auth, const char *message)
{
    runner_github_auth_clear(auth);
    auth->phase = RUNNER_AUTH_FAILED;
    (void)snprintf(auth->error, sizeof(auth->error), "%s", message);
}
void runner_github_authenticate(RunnerGithubAuth *auth, const char *organisation,
    RunnerAuthExecute execute, void *execute_context, RunnerHttpGet get, void *http_context,
    const RunnerAuthCallbacks *callbacks)
{
    if (!auth) return;
    runner_github_auth_clear(auth);
    if (organisation && organisation[0]) {
        if (!org_name(organisation)) { fail(auth, "Choose a valid GitHub organisation."); return; }
        (void)snprintf(auth->organisation, sizeof(auth->organisation), "%s", organisation);
    }
    char output[RUNNER_GITHUB_TOKEN_CAPACITY + 64U] = "";
    bool reused = false;
    if (execute && !runner_github_auth_cancelled(callbacks) &&
        execute(RUNNER_AUTH_EXISTING_SESSION, output, sizeof(output), callbacks, execute_context) &&
        runner_github_auth_token(output, auth->token, sizeof(auth->token)) &&
        !runner_github_auth_cancelled(callbacks)) {
        if (!auth->organisation[0]) discover_organisations(auth, get, http_context);
        reused = can_monitor(auth->organisation, auth->token, get, http_context);
        for (size_t i = 0U; !reused && i < auth->organisation_count &&
                !runner_github_auth_cancelled(callbacks); ++i)
            reused = can_monitor(auth->organisations[i], auth->token, get, http_context);
    }
    memset(output, 0, sizeof(output));
    if (!reused) {
        memset(auth->token, 0, sizeof(auth->token));
        memset(auth->organisations, 0, sizeof(auth->organisations));
        auth->organisation_count = 0U;
        (void)snprintf(auth->organisation, sizeof(auth->organisation), "%s", organisation ? organisation : "");
        if (!execute || runner_github_auth_cancelled(callbacks) ||
            !execute(RUNNER_AUTH_BROWSER_LOGIN, output, sizeof(output), callbacks, execute_context) ||
            runner_github_auth_cancelled(callbacks) ||
            !execute(RUNNER_AUTH_NEW_SESSION, output, sizeof(output), callbacks, execute_context) ||
            !runner_github_auth_token(output, auth->token, sizeof(auth->token))) {
            memset(output, 0, sizeof(output));
            fail(auth, runner_github_auth_cancelled(callbacks) ? "Sign-in cancelled. Your saved account has been kept."
                : "GitHub sign-in did not complete. Select Sign in with GitHub to try again.");
            return;
        }
    }
    memset(output, 0, sizeof(output));
    if (runner_github_auth_cancelled(callbacks)) { fail(auth, "Sign-in cancelled."); return; }
    discover_organisations(auth, get, http_context);
    if (runner_github_auth_cancelled(callbacks)) { fail(auth, "Sign-in cancelled."); return; }
    auth->phase = RUNNER_AUTH_COMPLETE;
}
