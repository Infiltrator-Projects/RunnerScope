// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/runner_config.h"
#include <infiltratr/escape.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *message)
{
    if (!error || size == 0U) return;
    (void)snprintf(error, size, "%s", message ? message : "configuration error");
}

static const char *skip_space(const char *cursor)
{
    while (cursor && *cursor && isspace((unsigned char)*cursor)) cursor++;
    return cursor;
}

static bool delimiter(char value)
{
    return value == '\0' || value == ',' || value == '}' ||
           isspace((unsigned char)value) != 0;
}

static bool skip_json_string(const char **cursor, char *error, size_t error_size)
{
    const char *p = cursor ? *cursor : NULL;
    if (!p || *p != '"') return false;
    p++;
    while (*p && *p != '"') {
        const unsigned char value = (unsigned char)*p++;
        if (value < 0x20U) {
            set_error(error, error_size,
                      "configuration contains an unescaped control character");
            return false;
        }
        if (value == '\\') {
            const char escaped = *p++;
            if (escaped == '\0' || strchr("\\\"/bfnrt", escaped) == NULL) {
                set_error(error, error_size,
                          "configuration contains an unsupported string escape");
                return false;
            }
        }
    }
    if (*p != '"') {
        set_error(error, error_size, "configuration string is unterminated");
        return false;
    }
    *cursor = p + 1;
    return true;
}

static bool validate_object_syntax(const char *json, char *error, size_t error_size)
{
    const char *p = skip_space(json);
    if (!p || *p != '{') {
        set_error(error, error_size, "configuration must be a JSON object");
        return false;
    }
    p = skip_space(p + 1);
    if (*p == '}') {
        p = skip_space(p + 1);
        if (*p == '\0') return true;
        set_error(error, error_size, "configuration has trailing data");
        return false;
    }

    for (;;) {
        if (*p != '"' || !skip_json_string(&p, error, error_size)) {
            if (error && error_size && !error[0])
                set_error(error, error_size, "configuration object key is malformed");
            return false;
        }
        p = skip_space(p);
        if (*p != ':') {
            set_error(error, error_size, "configuration object key has no value separator");
            return false;
        }
        p = skip_space(p + 1);
        if (*p == '"') {
            if (!skip_json_string(&p, error, error_size)) return false;
        } else {
            const char *start = p;
            while (*p && *p != ',' && *p != '}' && !isspace((unsigned char)*p)) p++;
            if (p == start) {
                set_error(error, error_size, "configuration value is missing");
                return false;
            }
            const size_t length = (size_t)(p - start);
            bool primitive = false;
            if ((length == 4U && strncmp(start, "true", 4U) == 0) ||
                (length == 5U && strncmp(start, "false", 5U) == 0) ||
                (length == 4U && strncmp(start, "null", 4U) == 0)) {
                primitive = true;
            } else {
                char *end = NULL;
                errno = 0;
                (void)strtod(start, &end);
                primitive = errno == 0 && end == p;
            }
            if (!primitive) {
                set_error(error, error_size,
                          "configuration contains an unsupported or malformed value");
                return false;
            }
        }
        p = skip_space(p);
        if (*p == '}') {
            p = skip_space(p + 1);
            if (*p == '\0') return true;
            set_error(error, error_size, "configuration has trailing data");
            return false;
        }
        if (*p != ',') {
            set_error(error, error_size, "configuration object members are not comma separated");
            return false;
        }
        p = skip_space(p + 1);
    }
}

static unsigned int key_count(const char *json, const char *key,
                              const char **first)
{
    char needle[96];
    const int amount = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (amount <= 0 || (size_t)amount >= sizeof(needle)) return 0U;

    unsigned int count = 0U;
    const char *cursor = json;
    while ((cursor = strstr(cursor, needle)) != NULL) {
        const char *after = skip_space(cursor + (size_t)amount);
        if (after && *after == ':') {
            if (count == 0U && first) *first = cursor;
            count++;
        }
        cursor += (size_t)amount;
    }
    return count;
}

static const char *value_start(const char *json, const char *key,
                               bool *present, char *error, size_t error_size)
{
    const char *found = NULL;
    const unsigned int count = key_count(json, key, &found);
    if (present) *present = count != 0U;
    if (count == 0U) return NULL;
    if (count != 1U) {
        char message[160];
        (void)snprintf(message, sizeof(message),
                       "configuration key '%s' appears more than once", key);
        set_error(error, error_size, message);
        return NULL;
    }

    found += strlen(key) + 2U;
    found = skip_space(found);
    if (!found || *found != ':') {
        char message[160];
        (void)snprintf(message, sizeof(message),
                       "configuration key '%s' has no value separator", key);
        set_error(error, error_size, message);
        return NULL;
    }
    return skip_space(found + 1);
}

static bool read_string(const char *json, const char *key,
                        char *output, size_t output_size,
                        bool *present, char *error, size_t error_size)
{
    const char *cursor = value_start(json, key, present, error, error_size);
    if (!present || !*present) return true;
    if (!cursor || *cursor != '"' || output_size == 0U) {
        set_error(error, error_size, "configuration string value is malformed");
        return false;
    }
    cursor++;

    size_t used = 0U;
    while (*cursor && *cursor != '"') {
        unsigned char value = (unsigned char)*cursor++;
        if (value < 0x20U) {
            set_error(error, error_size, "configuration string contains an unescaped control character");
            return false;
        }
        if (value == '\\') {
            const char escaped = *cursor++;
            if (escaped == '\0') {
                set_error(error, error_size, "configuration string ends inside an escape");
                return false;
            }
            switch (escaped) {
            case '"': value = '"'; break;
            case '\\': value = '\\'; break;
            case '/': value = '/'; break;
            case 'b': value = '\b'; break;
            case 'f': value = '\f'; break;
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            default:
                set_error(error, error_size,
                          "configuration contains an unsupported string escape");
                return false;
            }
        }
        if (used + 1U >= output_size) {
            set_error(error, error_size, "configuration string value is too long");
            return false;
        }
        output[used++] = (char)value;
    }
    if (*cursor != '"') {
        set_error(error, error_size, "configuration string is unterminated");
        return false;
    }
    output[used] = '\0';
    cursor = skip_space(cursor + 1);
    if (!delimiter(*cursor)) {
        set_error(error, error_size, "configuration string has trailing data");
        return false;
    }
    return true;
}

static bool read_uint(const char *json, const char *key,
                      unsigned int *output, bool *present,
                      char *error, size_t error_size)
{
    const char *cursor = value_start(json, key, present, error, error_size);
    if (!present || !*present) return true;
    if (!cursor || *cursor == '-' || !isdigit((unsigned char)*cursor)) {
        set_error(error, error_size, "configuration integer value is malformed");
        return false;
    }

    errno = 0;
    char *end = NULL;
    const unsigned long parsed = strtoul(cursor, &end, 10);
    if (errno != 0 || end == cursor || parsed > UINT_MAX) {
        set_error(error, error_size, "configuration integer value is out of range");
        return false;
    }
    end = (char *)skip_space(end);
    if (!delimiter(*end)) {
        set_error(error, error_size, "configuration integer has trailing data");
        return false;
    }
    *output = (unsigned int)parsed;
    return true;
}

static bool valid_org(const char *text)
{
    if (!text) return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (!(isalnum(*p) || *p == '-' || *p == '_' || *p == '.')) return false;
    }
    return true;
}

void runner_config_defaults(RunnerConfig *config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->runner_poll_seconds = 3U;
    config->activity_scan_seconds = 45U;
    config->repository_scan_limit = 25U;
    config->local_health_seconds = 15U;
    config->theme_mode = RUNNER_THEME_SYSTEM;
}

const char *runner_config_theme_key(RunnerThemeMode mode)
{
    switch (mode) {
    case RUNNER_THEME_SYSTEM: return "system";
    case RUNNER_THEME_DAY: return "day";
    case RUNNER_THEME_NIGHT: return "night";
    default: return NULL;
    }
}

bool runner_config_validate(const RunnerConfig *config,
                            char *error, size_t error_size)
{
    if (!config) {
        set_error(error, error_size, "configuration is missing");
        return false;
    }
    if (!memchr(config->organisation, '\0', sizeof(config->organisation)) ||
        !valid_org(config->organisation)) {
        set_error(error, error_size,
                  "organisation may contain only letters, digits, '.', '_' and '-'");
        return false;
    }
    if (!memchr(config->github_token, '\0', sizeof(config->github_token))) {
        set_error(error, error_size, "GitHub token is too long; update it in Settings");
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)config->github_token; *p; ++p) {
        if (*p <= 0x20U || *p >= 0x7fU) {
            set_error(error, error_size, "GitHub token must use visible ASCII characters without whitespace; update it in Settings");
            return false;
        }
    }
    if (config->runner_poll_seconds < 3U) {
        set_error(error, error_size, "runner_poll_seconds must be at least 3");
        return false;
    }
    if (config->activity_scan_seconds < 10U) {
        set_error(error, error_size, "activity_scan_seconds must be at least 10");
        return false;
    }
    if (config->repository_scan_limit < 1U) {
        set_error(error, error_size, "repository_scan_limit must be at least 1");
        return false;
    }
    if (config->local_health_seconds < 15U) {
        set_error(error, error_size, "local_health_seconds must be at least 15");
        return false;
    }
    if (!runner_config_theme_key(config->theme_mode)) {
        set_error(error, error_size, "theme_mode is invalid");
        return false;
    }
    if (error && error_size != 0U) error[0] = '\0';
    return true;
}

bool runner_config_parse_json(const char *json, RunnerConfig *config,
                              char *error, size_t error_size)
{
    if (!json || !config) {
        set_error(error, error_size, "configuration document is missing");
        return false;
    }

    if (!validate_object_syntax(json, error, error_size)) return false;

    RunnerConfig parsed;
    runner_config_defaults(&parsed);
    bool present = false;
    if (!read_string(json, "organisation", parsed.organisation,
                     sizeof(parsed.organisation), &present, error, error_size))
        return false;
    if (!read_string(json, "github_token", parsed.github_token,
                     sizeof(parsed.github_token), &present, error, error_size))
        return false;
    if (!read_uint(json, "expected_runners", &parsed.expected_runners,
                   &present, error, error_size))
        return false;
    if (!read_uint(json, "runner_poll_seconds", &parsed.runner_poll_seconds,
                   &present, error, error_size))
        return false;
    if (!read_uint(json, "activity_scan_seconds", &parsed.activity_scan_seconds,
                   &present, error, error_size))
        return false;
    if (!read_uint(json, "repository_scan_limit", &parsed.repository_scan_limit,
                   &present, error, error_size))
        return false;
    if (!read_uint(json, "local_health_seconds", &parsed.local_health_seconds,
                   &present, error, error_size))
        return false;

    char theme[24] = "system";
    if (!read_string(json, "theme_mode", theme, sizeof(theme),
                     &present, error, error_size))
        return false;
    if (strcmp(theme, "system") == 0) parsed.theme_mode = RUNNER_THEME_SYSTEM;
    else if (strcmp(theme, "day") == 0) parsed.theme_mode = RUNNER_THEME_DAY;
    else if (strcmp(theme, "night") == 0) parsed.theme_mode = RUNNER_THEME_NIGHT;
    else {
        set_error(error, error_size, "theme_mode must be system, day or night");
        return false;
    }

    if (!runner_config_validate(&parsed, error, error_size)) return false;
    *config = parsed;
    return true;
}

bool runner_config_write_json(const RunnerConfig *config,
                              char *output, size_t output_size,
                              size_t *written,
                              char *error, size_t error_size)
{
    if (!runner_config_validate(config, error, error_size)) return false;
    if (!output || output_size == 0U) {
        set_error(error, error_size, "configuration output buffer is missing");
        return false;
    }

    const char *theme = runner_config_theme_key(config->theme_mode);
    char escaped_token[RUNNER_GITHUB_TOKEN_CAPACITY * 6U];
    if (!infiltratr_escape_json(config->github_token, escaped_token,
                               sizeof(escaped_token), NULL)) {
        set_error(error, error_size, "Unable to encode the GitHub token");
        return false;
    }
    const int amount = snprintf(
        output, output_size,
        "{\n"
        "  \"organisation\": \"%s\",\n"
        "  \"github_token\": \"%s\",\n"
        "  \"expected_runners\": %u,\n"
        "  \"runner_poll_seconds\": %u,\n"
        "  \"activity_scan_seconds\": %u,\n"
        "  \"repository_scan_limit\": %u,\n"
        "  \"local_health_seconds\": %u,\n"
        "  \"theme_mode\": \"%s\"\n"
        "}\n",
        config->organisation,
        escaped_token,
        config->expected_runners,
        config->runner_poll_seconds,
        config->activity_scan_seconds,
        config->repository_scan_limit,
        config->local_health_seconds,
        theme);
    if (amount < 0 || (size_t)amount >= output_size) {
        set_error(error, error_size, "configuration output buffer is too small");
        return false;
    }
    if (written) *written = (size_t)amount;
    if (error && error_size != 0U) error[0] = '\0';
    return true;
}
