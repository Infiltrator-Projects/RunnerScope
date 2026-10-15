// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/runner_config.h"

#include <stdio.h>
#include <string.h>

static int expect(bool condition, int code, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "%s\n", message);
    return code;
}

int main(void)
{
    char error[256];
    RunnerConfig config;
    runner_config_defaults(&config);
    if (expect(config.runner_poll_seconds == 3U, 1, "bad runner default")) return 1;
    if (expect(config.activity_scan_seconds == 45U, 2, "bad activity default")) return 2;
    if (expect(config.repository_scan_limit == 25U, 3, "bad repository default")) return 3;
    if (expect(config.local_health_seconds == 15U, 4, "bad health default")) return 4;

    const char *document =
        "{\n"
        "  \"organisation\": \"Infiltrator-Projects\",\n"
        "  \"expected_runners\": 7,\n"
        "  \"runner_poll_seconds\": 6,\n"
        "  \"activity_scan_seconds\": 90,\n"
        "  \"repository_scan_limit\": 42,\n"
        "  \"local_health_seconds\": 30,\n"
        "  \"theme_mode\": \"night\"\n"
        "}\n";
    if (!runner_config_parse_json(document, &config, error, sizeof(error))) {
        fprintf(stderr, "parse failed: %s\n", error);
        return 5;
    }
    if (expect(strcmp(config.organisation, "Infiltrator-Projects") == 0,
               6, "organisation lost")) return 6;
    if (expect(config.expected_runners == 7U &&
               config.runner_poll_seconds == 6U &&
               config.activity_scan_seconds == 90U &&
               config.repository_scan_limit == 42U &&
               config.local_health_seconds == 30U &&
               config.theme_mode == RUNNER_THEME_NIGHT,
               7, "parsed values differ")) return 7;

    /* An organisation-only editor must not reset fields it does not expose. */
    (void)snprintf(config.organisation, sizeof(config.organisation), "%s", "Changed-Org");
    char encoded[1024];
    size_t written = 0U;
    if (!runner_config_write_json(&config, encoded, sizeof(encoded), &written,
                                  error, sizeof(error))) {
        fprintf(stderr, "write failed: %s\n", error);
        return 8;
    }
    if (expect(written == strlen(encoded), 9, "reported length differs")) return 9;

    RunnerConfig roundtrip;
    if (!runner_config_parse_json(encoded, &roundtrip, error, sizeof(error))) {
        fprintf(stderr, "roundtrip parse failed: %s\n", error);
        return 10;
    }
    if (expect(strcmp(roundtrip.organisation, "Changed-Org") == 0 &&
               roundtrip.expected_runners == 7U &&
               roundtrip.runner_poll_seconds == 6U &&
               roundtrip.activity_scan_seconds == 90U &&
               roundtrip.repository_scan_limit == 42U &&
               roundtrip.local_health_seconds == 30U &&
               roundtrip.theme_mode == RUNNER_THEME_NIGHT,
               11, "settings roundtrip clobbered unrelated fields")) return 11;

    const char *duplicate =
        "{\"organisation\":\"A\",\"organisation\":\"B\"}";
    if (runner_config_parse_json(duplicate, &roundtrip, error, sizeof(error))) {
        fprintf(stderr, "duplicate key accepted\n");
        return 12;
    }
    const char *bad_minimum =
        "{\"organisation\":\"A\",\"runner_poll_seconds\":1}";
    if (runner_config_parse_json(bad_minimum, &roundtrip, error, sizeof(error))) {
        fprintf(stderr, "invalid poll interval accepted\n");
        return 13;
    }
    const char *bad_org = "{\"organisation\":\"bad org; rm\"}";
    if (runner_config_parse_json(bad_org, &roundtrip, error, sizeof(error))) {
        fprintf(stderr, "invalid organisation accepted\n");
        return 14;
    }
    const char *bad_json = "{\"organisation\":\"A\" \"runner_poll_seconds\":3}";
    if (runner_config_parse_json(bad_json, &roundtrip, error, sizeof(error))) {
        fprintf(stderr, "malformed object syntax accepted\n");
        return 15;
    }

    if (expect(config.github_token[0] == '\0', 16,
               "legacy settings acquired an invented token")) return 16;
    const char *token = "PAT_PRIVATE_X_\\\"literal";
    (void)snprintf(config.github_token, sizeof(config.github_token), "%s", token);
    char secret_json[8192];
    if (!runner_config_write_json(&config, secret_json, sizeof(secret_json),
                                  &written, error, sizeof(error)) ||
        !runner_config_parse_json(secret_json, &roundtrip, error, sizeof(error)) ||
        strcmp(roundtrip.github_token, token) != 0) {
        fputs("token escaping/roundtrip failed\n", stderr);
        return 17;
    }
    const char *bad_tokens[] = {
        "{\"github_token\":\"PAT_PRIVATE_X\",\"github_token\":\"SECOND_PRIVATE\"}",
        "{\"github_token\":\"PAT_PRIVATE_X space\"}",
        "{\"github_token\":\"PAT_PRIVATE_X\\tcontrol\"}",
        "{\"github_token\":\"PAT_PRIVATE_X\\r\\nheader\"}",
        "{\"github_token\":123}"
    };
    for (size_t i = 0U; i < sizeof(bad_tokens) / sizeof(bad_tokens[0]); ++i) {
        const RunnerConfig unchanged = roundtrip;
        if (runner_config_parse_json(bad_tokens[i], &roundtrip, error, sizeof(error)) ||
            strstr(error, "PAT_PRIVATE_X") || strstr(error, "SECOND_PRIVATE") ||
            memcmp(&roundtrip, &unchanged, sizeof(roundtrip)) != 0) {
            fputs("invalid token accepted, exposed or changed saved state\n", stderr);
            return 18;
        }
    }
    memset(config.github_token, '"', sizeof(config.github_token) - 1U);
    config.github_token[sizeof(config.github_token) - 1U] = '\0';
    if (!runner_config_write_json(&config, secret_json, sizeof(secret_json),
                                  NULL, error, sizeof(error)) ||
        !runner_config_parse_json(secret_json, &roundtrip, error, sizeof(error)) ||
        strcmp(roundtrip.github_token, config.github_token) != 0) return 19;
    char too_long[RUNNER_GITHUB_TOKEN_CAPACITY + 100U];
    memset(too_long, 'x', sizeof(too_long));
    const char *prefix = "{\"github_token\":\"";
    const size_t prefix_length = strlen(prefix);
    memcpy(too_long, prefix, prefix_length);
    memcpy(too_long + prefix_length + RUNNER_GITHUB_TOKEN_CAPACITY, "\"}", 3U);
    if (runner_config_parse_json(too_long, &roundtrip, error, sizeof(error))) return 20;
    memset(config.github_token, 'x', sizeof(config.github_token));
    if (runner_config_validate(&config, error, sizeof(error))) return 21;
    runner_config_defaults(&config);
    if (!runner_config_validate(&config, error, sizeof(error))) return 22;

    puts("Runner configuration contract passed.");
    return 0;
}
