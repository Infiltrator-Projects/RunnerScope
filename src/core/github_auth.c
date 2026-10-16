// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github_auth.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RUNNERSCOPE_GITHUB_CLIENT_ID
#define RUNNERSCOPE_GITHUB_CLIENT_ID ""
#endif

const char *runner_github_auth_client_id(void)
{
    return RUNNERSCOPE_GITHUB_CLIENT_ID;
}

const char *runner_github_auth_client_for_config(const RunnerConfig *config)
{
    return config && config->github_client_id[0] ? config->github_client_id : runner_github_auth_client_id();
}

void runner_github_auth_clear(RunnerGithubAuth *auth)
{
    if (!auth) return;
    volatile unsigned char *p = (volatile unsigned char *)auth;
    for (size_t n = 0U; n < sizeof(*auth); ++n) p[n] = 0U;
}

static bool identifier(const char *s, size_t capacity)
{
    if (!s || !s[0]) return false;
    for (size_t i = 0U; i < capacity; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (!c) return true;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return false;
}

static RunnerAuthPhase failed(RunnerGithubAuth *auth, const char *message)
{
    runner_github_auth_clear(auth);
    auth->phase = RUNNER_AUTH_FAILED;
    (void)snprintf(auth->error, sizeof(auth->error), "%s", message);
    return auth->phase;
}

static bool copy_field(const cJSON *json, const char *name, char *out, size_t size)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(json, name);
    if (!cJSON_IsString(value) || !identifier(value->valuestring, size)) return false;
    memcpy(out, value->valuestring, strlen(value->valuestring) + 1U);
    return true;
}

static bool seconds_field(const cJSON *json, const char *name,
    unsigned int maximum, unsigned int *out)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(json, name);
    if (!cJSON_IsNumber(value) || !(value->valuedouble >= 1.0) ||
        !(value->valuedouble <= (double)maximum)) return false;
    const unsigned int number = (unsigned int)value->valuedouble;
    if ((double)number != value->valuedouble) return false;
    *out = number;
    return true;
}

static const char *oauth_error(const char *code)
{
    if (code && strcmp(code, "access_denied") == 0)
        return "GitHub sign-in was cancelled. Select Sign in with GitHub to try again.";
    if (code && (strcmp(code, "expired_token") == 0 || strcmp(code, "token_expired") == 0))
        return "The GitHub sign-in code expired. Select Sign in with GitHub for a new code.";
    if (code && strcmp(code, "device_flow_disabled") == 0)
        return "GitHub sign-in is unavailable: Device Flow must be enabled for the Runner Monitor OAuth app.";
    if (code && strcmp(code, "incorrect_client_credentials") == 0)
        return "GitHub sign-in is unavailable: the Runner Monitor OAuth app registration needs attention.";
    return "GitHub could not complete sign-in. Select Sign in with GitHub to try again.";
}

RunnerAuthPhase runner_github_auth_step(RunnerGithubAuth *auth,
    const char *client_id, uint64_t now_ms, RunnerHttpPost post, void *context)
{
    if (!auth) return RUNNER_AUTH_FAILED;
    if (auth->phase == RUNNER_AUTH_COMPLETE || auth->phase == RUNNER_AUTH_FAILED)
        return auth->phase;
    if (!identifier(client_id, 128U))
        return failed(auth, "GitHub sign-in is unavailable in this build: the Runner Monitor OAuth app has not been configured.");
    if (!post) return failed(auth, "GitHub sign-in transport is unavailable.");
    const bool starting = auth->phase == RUNNER_AUTH_NEW;
    if (!starting) {
        if (now_ms >= auth->expires_at_ms)
            return failed(auth, oauth_error("expired_token"));
        if (now_ms < auth->next_poll_ms) return auth->phase;
        if (!identifier(auth->device_code, sizeof(auth->device_code)))
            return failed(auth, "The GitHub sign-in session is invalid. Start sign-in again.");
    }
    char form[512];
    if (starting) {
        /* admin:org is required by GitHub's organisation runner endpoint for
         * OAuth tokens; repo permits activity discovery in private projects. */
        (void)snprintf(form, sizeof(form), "client_id=%s&scope=admin%%3Aorg%%20repo", client_id);
    } else {
        (void)snprintf(form, sizeof(form),
            "client_id=%s&device_code=%s&grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code",
            client_id, auth->device_code);
    }
    RunnerHttpResponse response = {0U, NULL, NULL};
    char transport_error[320] = "";
    const bool sent = post(starting ? "/login/device/code" : "/login/oauth/access_token",
        form, &response, transport_error, sizeof(transport_error), context);
    memset(form, 0, sizeof(form));
    /* Do not propagate arbitrary provider/server text: it can contain tokens. */
    cJSON *json = sent && (response.status == 200U || response.status == 400U) && response.body &&
        strlen(response.body) <= 16384U ? cJSON_ParseWithOpts(response.body, NULL, true) : NULL;
    if (response.body) { memset(response.body, 0, strlen(response.body)); free(response.body); }
    free(response.link);
    if (!json || !cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return failed(auth, sent ? "GitHub returned an invalid sign-in response. Try again."
            : "Unable to reach GitHub for sign-in. Check the connection and try again.");
    }
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(json, "error");
    if (cJSON_IsString(error)) {
        if (!starting && strcmp(error->valuestring, "authorization_pending") == 0) {
            auth->next_poll_ms = now_ms + (uint64_t)auth->interval_seconds * 1000U;
        } else if (!starting && strcmp(error->valuestring, "slow_down") == 0) {
            unsigned int interval = 0U;
            if (auth->interval_seconds > 3595U) failed(auth, "GitHub sign-in timed out. Try again.");
            else {
                auth->interval_seconds += 5U;
                if (seconds_field(json, "interval", 3600U, &interval) && interval > auth->interval_seconds)
                    auth->interval_seconds = interval;
                auth->next_poll_ms = now_ms + (uint64_t)auth->interval_seconds * 1000U;
            }
        } else failed(auth, oauth_error(error->valuestring));
    } else if (response.status != 200U) {
        failed(auth, "GitHub could not complete sign-in. Try again.");
    } else if (starting) {
        unsigned int expires = 0U, interval = 0U;
        const cJSON *uri = cJSON_GetObjectItemCaseSensitive(json, "verification_uri");
        if (!copy_field(json, "device_code", auth->device_code, sizeof(auth->device_code)) ||
            !copy_field(json, "user_code", auth->user_code, sizeof(auth->user_code)) ||
            !cJSON_IsString(uri) || strcmp(uri->valuestring, "https://github.com/login/device") != 0 ||
            !seconds_field(json, "expires_in", 3600U, &expires) ||
            !seconds_field(json, "interval", 3600U, &interval) ||
            now_ms > UINT64_MAX - 3600000U) {
            failed(auth, "GitHub returned an invalid sign-in code. Try again.");
        } else {
            auth->phase = RUNNER_AUTH_WAITING;
            auth->interval_seconds = interval;
            auth->expires_at_ms = now_ms + (uint64_t)expires * 1000U;
            auth->next_poll_ms = now_ms + (uint64_t)interval * 1000U;
        }
    } else {
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "token_type");
        if (!cJSON_IsString(type) || strcmp(type->valuestring, "bearer") != 0 ||
            !copy_field(json, "access_token", auth->token, sizeof(auth->token))) {
            failed(auth, "GitHub returned an invalid sign-in session. Try again.");
        } else {
            memset(auth->device_code, 0, sizeof(auth->device_code));
            memset(auth->user_code, 0, sizeof(auth->user_code));
            auth->phase = RUNNER_AUTH_COMPLETE;
        }
    }
    cJSON *secret = cJSON_GetObjectItemCaseSensitive(json, "access_token");
    if (cJSON_IsString(secret)) memset(secret->valuestring, 0, strlen(secret->valuestring));
    secret = cJSON_GetObjectItemCaseSensitive(json, "refresh_token");
    if (cJSON_IsString(secret)) memset(secret->valuestring, 0, strlen(secret->valuestring));
    cJSON_Delete(json);
    return auth->phase;
}
