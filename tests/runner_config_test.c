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

    puts("Runner configuration contract passed.");
    return 0;
}
