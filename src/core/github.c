// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github.h"
#include "core/runner_config.h"
#include "core/json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GITHUB_ORIGIN "https://api.github.com"
#define GITHUB_PAGE_LIMIT 100U
#define GITHUB_URL_LIMIT 4096U
#define GITHUB_LINK_LIMIT 16384U

typedef struct { char *data; size_t used, capacity; } Text;

static void fail(char *error, size_t size, const char *message)
{
    if (error && size) (void)snprintf(error, size, "%s", message);
}

static size_t bounded_length(const char *text, size_t limit)
{
    size_t length = 0U;
    if (text) while (length < limit && text[length]) length++;
    return length;
}

static bool append(Text *text, const char *value, size_t length)
{
    if (length > RUNNER_HTTP_BODY_LIMIT - text->used) return false;
    const size_t needed = text->used + length + 1U;
    if (needed > text->capacity) {
        size_t capacity = text->capacity ? text->capacity : 256U;
        while (capacity < needed) capacity *= 2U;
        char *grown = realloc(text->data, capacity);
        if (!grown) return false;
        text->data = grown;
        text->capacity = capacity;
    }
    if (length) memcpy(text->data + text->used, value, length);
    text->used += length;
    text->data[text->used] = '\0';
    return true;
}

static bool field(Text *text, const char *value, bool first)
{
    if (!first && !append(text, "\t", 1U)) return false;
    if (!value) value = "";
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        const char byte = *p < 0x20U || *p == 0x7fU ? ' ' : (char)*p;
        if (!append(text, &byte, 1U)) return false;
    }
    return true;
}

static const cJSON *member(const cJSON *object, const char *name, bool *valid)
{
    const cJSON *found = NULL;
    if (!cJSON_IsObject(object)) { *valid = false; return NULL; }
    const cJSON *item;
    cJSON_ArrayForEach(item, object) {
        if (item->string && strcmp(item->string, name) == 0) {
            if (found) { *valid = false; return NULL; }
            found = item;
        }
    }
    return found;
}

static const char *string_member(const cJSON *object, const char *name,
                                  const char *fallback, bool *valid)
{
    const cJSON *item = member(object, name, valid);
    if (!item || cJSON_IsNull(item)) {
        if (!fallback) *valid = false;
        return fallback ? fallback : "";
    }
    if (!cJSON_IsString(item) || !item->valuestring) {
        *valid = false;
        return "";
    }
    if (!fallback && !item->valuestring[0]) *valid = false;
    return item->valuestring;
}

static bool labels(const cJSON *object, bool runner_labels, Text *text, bool *valid)
{
    const cJSON *array = member(object, "labels", valid);
    if (!cJSON_IsArray(array)) { *valid = false; return false; }
    const cJSON *item;
    bool first = true;
    cJSON_ArrayForEach(item, array) {
        const char *name = runner_labels
            ? string_member(item, "name", NULL, valid)
            : cJSON_IsString(item) ? item->valuestring : NULL;
        if (!name) { *valid = false; return false; }
        if ((!first && !append(text, ",", 1U)) || !field(text, name, true)) return false;
        first = false;
    }
    return append(text, "", 0U);
}


const char *runner_github_job_environment(const char *group, const char *labels)
{
    const char *p = labels;
    while (p && *p) {
        const char *end = strchr(p, ',');
        const size_t length = end ? (size_t)(end - p) : strlen(p);
        if (length == strlen("self-hosted") && !strncmp(p, "self-hosted", length))
            return "LOCAL";
        p = end ? end + 1 : NULL;
    }
    return group && !strcmp(group, "GitHub Actions") ? "GITHUB" : "UNKNOWN";
}

char *runner_github_project(const char *json, RunnerGithubProjection projection,
                            char *error, size_t error_size)
{
    if (error && error_size) error[0] = '\0';
    cJSON *root = runner_json_parse(json, RUNNER_HTTP_BODY_LIMIT);
    if (!root) {
        fail(error, error_size, "GitHub returned invalid JSON. Try refreshing again.");
        return NULL;
    }
    bool valid = true;
    const cJSON *rows = NULL;
    switch (projection) {
    case RUNNER_GITHUB_RUNNERS: rows = member(root, "runners", &valid); break;
    case RUNNER_GITHUB_REPOSITORIES: rows = root; break;
    case RUNNER_GITHUB_RUNS: rows = member(root, "workflow_runs", &valid); break;
    case RUNNER_GITHUB_JOBS: rows = member(root, "jobs", &valid); break;
    default: valid = false; break;
    }
    if (!cJSON_IsArray(rows)) valid = false;
    Text output = {0};
    bool allocated = append(&output, "", 0U);
    const cJSON *row;
    cJSON_ArrayForEach(row, rows) {
        if (!valid || !allocated) break;
        if (!cJSON_IsObject(row)) { valid = false; break; }
        if (projection == RUNNER_GITHUB_RUNNERS) {
            const char *name = string_member(row, "name", NULL, &valid);
            const char *os = string_member(row, "os", NULL, &valid);
            const char *status = string_member(row, "status", NULL, &valid);
            const cJSON *busy = member(row, "busy", &valid);
            if (!cJSON_IsBool(busy) ||
                (strcmp(status, "online") && strcmp(status, "offline"))) valid = false;
            Text joined = {0};
            allocated = labels(row, true, &joined, &valid) &&
                field(&output, name, true) && field(&output, os, false) &&
                field(&output, status, false) &&
                field(&output, cJSON_IsTrue(busy) ? "true" : "false", false) &&
                field(&output, joined.data, false);
            free(joined.data);
        } else if (projection == RUNNER_GITHUB_REPOSITORIES) {
            const char *name = string_member(row, "name", NULL, &valid);
            const cJSON *archived = member(row, "archived", &valid);
            const cJSON *disabled = member(row, "disabled", &valid);
            if (!cJSON_IsBool(archived) || !cJSON_IsBool(disabled)) valid = false;
            if (!valid) break;
            if (cJSON_IsTrue(archived) || cJSON_IsTrue(disabled)) continue;
            allocated = field(&output, name, true);
        } else if (projection == RUNNER_GITHUB_RUNS) {
            const char *status = string_member(row, "status", NULL, &valid);
            if (!valid) break;
            if (strcmp(status, "in_progress") && strcmp(status, "queued")) continue;
            const cJSON *id = member(row, "id", &valid);
            if (!cJSON_IsNumber(id) || !isfinite(id->valuedouble) ||
                id->valuedouble < 1.0 || id->valuedouble > 9007199254740991.0 ||
                floor(id->valuedouble) != id->valuedouble) valid = false;
            char identifier[32];
            (void)snprintf(identifier, sizeof(identifier), "%.0f", cJSON_IsNumber(id) ? id->valuedouble : 0.0);
            const char *title = string_member(row, "display_title", "Workflow", &valid);
            const char *name = string_member(row, "name", title, &valid);
            const char *event = string_member(row, "event", "—", &valid);
            const char *branch = string_member(row, "head_branch", "—", &valid);
            const char *created = string_member(row, "created_at", "", &valid);
            const char *started = string_member(row, "run_started_at", created, &valid);
            const char *url = string_member(row, "html_url", "", &valid);
            allocated = field(&output, identifier, true) && field(&output, name, false) &&
                field(&output, event, false) && field(&output, branch, false) &&
                field(&output, started, false) && field(&output, url, false) &&
                field(&output, status, false);
        } else {
            const char *name = string_member(row, "name", "Job", &valid);
            const char *status = string_member(row, "status", NULL, &valid);
            const char *conclusion = string_member(row, "conclusion", "", &valid);
            const char *runner = string_member(row, "runner_name", "", &valid);
            const char *group = string_member(row, "runner_group_name", "", &valid);
            const char *started = string_member(row, "started_at", "", &valid);
            const char *url = string_member(row, "html_url", "", &valid);
            const cJSON *steps = member(row, "steps", &valid);
            if (!cJSON_IsArray(steps)) valid = false;
            const char *step_name = "—";
            const cJSON *step;
            bool found = false;
            cJSON_ArrayForEach(step, steps) {
                const char *state = string_member(step, "status", NULL, &valid);
                if (!found && strcmp(state, "in_progress") == 0) {
                    step_name = string_member(step, "name", NULL, &valid);
                    found = true;
                }
            }
            Text joined = {0};
            allocated = labels(row, false, &joined, &valid) &&
                field(&output, name, true) && field(&output, status, false) &&
                field(&output, conclusion, false) && field(&output, runner, false) &&
                field(&output, group, false) && field(&output, joined.data, false) &&
                field(&output, started, false) && field(&output, url, false) &&
                field(&output, step_name, false);
            free(joined.data);
        }
        allocated = allocated && append(&output, "\n", 1U);
    }
    cJSON_Delete(root);
    if (!valid || !allocated) {
        free(output.data);
        fail(error, error_size, valid ? "GitHub data exceeds the supported size. Try again later."
            : "GitHub returned incomplete or invalid data. Try refreshing again.");
        return NULL;
    }
    return output.data;
}

void runner_http_response_destroy(RunnerHttpResponse *response)
{
    if (!response) return;
    free(response->body);
    free(response->link);
    memset(response, 0, sizeof(*response));
}

static bool token_valid(const char *token)
{
    if (!token || !token[0]) return false;
    const size_t length = bounded_length(token, RUNNER_GITHUB_TOKEN_CAPACITY);
    if (length == RUNNER_GITHUB_TOKEN_CAPACITY) return false;
    for (size_t i = 0U; i < length; i++) {
        const unsigned char byte = (unsigned char)token[i];
        if (byte <= 0x20U || byte >= 0x7fU) return false;
    }
    return true;
}

static bool safe_url(const char *url)
{
    const size_t prefix = sizeof(GITHUB_ORIGIN) - 1U;
    if (!url || strncmp(url, GITHUB_ORIGIN "/", prefix + 1U) != 0 ||
        bounded_length(url, GITHUB_URL_LIMIT) >= GITHUB_URL_LIMIT) return false;
    for (const unsigned char *p = (const unsigned char *)url; *p; p++)
        if (*p <= 0x20U || *p >= 0x7fU || *p == '\\' || *p == '#') return false;
    return true;
}

static char *copy_range(const char *start, const char *end)
{
    const size_t length = (size_t)(end - start);
    char *copy = malloc(length + 1U);
    if (copy) { memcpy(copy, start, length); copy[length] = '\0'; }
    return copy;
}

static bool equal_word(const char *start, size_t length, const char *word)
{
    if (strlen(word) != length) return false;
    for (size_t i = 0U; i < length; i++)
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)word[i])) return false;
    return true;
}

/* GitHub's next link is absolute. Parse its relation instead of assuming the
 * first URL is next; prev/last can legitimately precede it in the header. */
static bool next_link(const char *header, char **next)
{
    *next = NULL;
    if (!header || !*header) return true;
    if (bounded_length(header, GITHUB_LINK_LIMIT) >= GITHUB_LINK_LIMIT) return false;
    const char *p = header;
    while (*p) {
        while (isspace((unsigned char)*p) || *p == ',') p++;
        if (!*p) break;
        if (*p++ != '<') goto invalid;
        const char *start = p;
        const char *end = strchr(p, '>');
        if (!end) goto invalid;
        p = end + 1;
        bool is_next = false;
        while (*p && *p != ',') {
            while (isspace((unsigned char)*p)) p++;
            if (!*p || *p == ',') break;
            if (*p++ != ';') goto invalid;
            while (isspace((unsigned char)*p)) p++;
            const char *key = p;
            while (isalnum((unsigned char)*p) || *p == '_' || *p == '-') p++;
            const size_t key_length = (size_t)(p - key);
            while (isspace((unsigned char)*p)) p++;
            if (!key_length || *p++ != '=') goto invalid;
            while (isspace((unsigned char)*p)) p++;
            const bool quoted = *p == '"';
            if (quoted) p++;
            const char *value = p;
            while (*p && (quoted ? *p != '"' : !isspace((unsigned char)*p) && *p != ';' && *p != ',')) p++;
            const char *value_end = p;
            if (quoted && *p++ != '"') goto invalid;
            if (equal_word(key, key_length, "rel")) {
                while (value < value_end) {
                    while (value < value_end && isspace((unsigned char)*value)) value++;
                    const char *word = value;
                    while (value < value_end && !isspace((unsigned char)*value)) value++;
                    if (equal_word(word, (size_t)(value - word), "next")) is_next = true;
                }
            }
        }
        if (is_next) {
            if (*next) goto invalid;
            *next = copy_range(start, end);
            if (!*next || !safe_url(*next)) goto invalid;
        }
    }
    return true;
invalid:
    free(*next);
    *next = NULL;
    return false;
}

static bool contains(const char *text, const char *word)
{
    if (!text) return false;
    const size_t length = strlen(word);
    for (const char *p = text; *p; p++) {
        size_t i = 0U;
        while (i < length && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)word[i])) i++;
        if (i == length) return true;
    }
    return false;
}

char *runner_github_fetch(const char *endpoint, const char *token,
    RunnerGithubProjection projection, RunnerHttpGet get, void *context,
    char *error, size_t error_size)
{
    if (error && error_size) error[0] = '\0';
    if (!token_valid(token)) {
        fail(error, error_size, "Sign in with GitHub in Settings to connect.");
        return NULL;
    }
    if (!get || !endpoint || bounded_length(endpoint, GITHUB_URL_LIMIT) >= GITHUB_URL_LIMIT) {
        fail(error, error_size, "GitHub request configuration is invalid. Check Settings.");
        return NULL;
    }

    /* The activity view only consumes queued and in-progress runs. Asking the
     * unfiltered workflow-runs endpoint to paginate through completed history
     * makes response volume grow forever with repository age. Split that
     * logical request into GitHub's two active-state filters instead, so old
     * completed runs are never transferred merely to be discarded. */
    if (projection == RUNNER_GITHUB_RUNS && !strstr(endpoint, "status=")) {
        const char *states[] = {"in_progress", "queued"};
        Text active = {0};
        if (!append(&active, "", 0U)) {
            fail(error, error_size, "Unable to allocate GitHub request data.");
            return NULL;
        }
        for (size_t i = 0U; i < sizeof(states) / sizeof(states[0]); ++i) {
            char filtered[GITHUB_URL_LIMIT];
            const char separator = strchr(endpoint, '?') ? '&' : '?';
            const int amount = snprintf(filtered, sizeof(filtered), "%s%cstatus=%s",
                                        endpoint, separator, states[i]);
            if (amount < 0 || (size_t)amount >= sizeof(filtered)) {
                free(active.data);
                fail(error, error_size, "GitHub request configuration is invalid. Check Settings.");
                return NULL;
            }
            char *rows = runner_github_fetch(filtered, token, projection, get, context,
                                             error, error_size);
            if (!rows) {
                free(active.data);
                return NULL;
            }
            const bool combined = append(&active, rows, strlen(rows));
            free(rows);
            if (!combined) {
                free(active.data);
                fail(error, error_size, "GitHub data exceeds the supported size. Try again later.");
                return NULL;
            }
        }
        return active.data;
    }

    char initial[GITHUB_URL_LIMIT];
    const int written = endpoint[0] == '/' && endpoint[1] != '/'
        ? snprintf(initial, sizeof(initial), "%s%s", GITHUB_ORIGIN, endpoint)
        : snprintf(initial, sizeof(initial), "%s", endpoint);
    if (written < 0 || (size_t)written >= sizeof(initial) || !safe_url(initial)) {
        fail(error, error_size, "GitHub request must use the trusted HTTPS API. Check Settings.");
        return NULL;
    }
    char *urls[GITHUB_PAGE_LIMIT] = {0};
    size_t count = 0U, total = 0U;
    Text output = {0};
    bool ok = append(&output, "", 0U);
    char *url = copy_range(initial, initial + strlen(initial));
    if (!ok || !url) {
        free(url);
        free(output.data);
        fail(error, error_size, "Unable to allocate GitHub request data.");
        return NULL;
    }
    while (ok && url) {
        if (count == GITHUB_PAGE_LIMIT) {
            fail(error, error_size, "GitHub pagination exceeds the supported limit. Try again later.");
            ok = false;
            free(url);
            break;
        }
        for (size_t i = 0U; i < count; i++) if (strcmp(url, urls[i]) == 0) ok = false;
        if (!ok) {
            fail(error, error_size, "GitHub returned repeated pagination data. Try refreshing again.");
            free(url);
            break;
        }
        urls[count++] = url;
        RunnerHttpResponse response = {0};
        char ignored_error[256] = "";
        if (!get(url, token, &response, ignored_error, sizeof(ignored_error), context)) {
            fail(error, error_size, "Unable to reach GitHub. Check the network connection and try again in Settings.");
            ok = false;
        } else if (response.status != 200U) {
            const char *message = response.status == 401U
                ? "GitHub sign-in expired or was revoked. Sign in again in Settings."
                : response.status == 429U || (response.status == 403U && contains(response.body, "rate limit"))
                ? "GitHub's request limit was reached. Wait before refreshing again."
                : response.status == 403U
                ? projection == RUNNER_GITHUB_RUNNERS
                    ? "GitHub denied runner access. Sign in with GitHub in Settings using an account authorized for organisation Self-hosted runners: Read and SSO where required."
                    : "GitHub denied activity access. Sign in with GitHub in Settings using an account authorized for these repositories, Actions: Read and SSO where required."
                : response.status == 404U
                ? "GitHub could not access this organisation or repository. Check the organisation in Settings and sign in again."
                : "GitHub is temporarily unavailable or returned an unexpected response. Try refreshing again.";
            fail(error, error_size, message);
            ok = false;
        }
        char *projected = NULL, *next = NULL;
        if (ok) {
            const size_t length = bounded_length(response.body, RUNNER_HTTP_BODY_LIMIT + 1U);
            if (!response.body || length > RUNNER_HTTP_BODY_LIMIT - total) {
                fail(error, error_size, "GitHub data exceeds the supported size. Try again later.");
                ok = false;
            } else {
                total += length;
                projected = runner_github_project(response.body, projection, error, error_size);
                ok = projected != NULL;
            }
        }
        if (ok && !next_link(response.link, &next)) {
            fail(error, error_size, "GitHub returned an unsafe or invalid pagination link. Try refreshing again.");
            ok = false;
        }
        if (ok && !append(&output, projected, strlen(projected))) {
            fail(error, error_size, "GitHub data exceeds the supported size. Try again later.");
            ok = false;
        }
        free(projected);
        runner_http_response_destroy(&response);
        if (!ok) free(next);
        url = ok ? next : NULL;
    }
    for (size_t i = 0U; i < count; i++) free(urls[i]);
    if (!ok || count == 0U) {
        free(output.data);
        if (error && error_size && !error[0]) fail(error, error_size, "Unable to allocate GitHub request data.");
        return NULL;
    }
    return output.data;
}
