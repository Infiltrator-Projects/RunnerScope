// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_RUNNER_CONFIG_H
#define RUNNERSCOPE_RUNNER_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define RUNNER_GITHUB_TOKEN_CAPACITY 1024U

typedef enum {
    RUNNER_THEME_SYSTEM = 0,
    RUNNER_THEME_DAY = 1,
    RUNNER_THEME_NIGHT = 2
} RunnerThemeMode;

typedef struct {
    char organisation[128];
    char github_token[RUNNER_GITHUB_TOKEN_CAPACITY];
    unsigned int expected_runners;
    unsigned int runner_poll_seconds;
    unsigned int activity_scan_seconds;
    unsigned int repository_scan_limit;
    unsigned int local_health_seconds;
    RunnerThemeMode theme_mode;
} RunnerConfig;

void runner_config_defaults(RunnerConfig *config);

bool runner_config_validate(const RunnerConfig *config,
                            char *error,
                            size_t error_size);

/* Older native theme display names are accepted case-insensitively. Legacy
 * timer values such as 2.0 are accepted only when the fraction is all zero;
 * counts remain JSON integers. Failed parsing leaves the destination intact. */
bool runner_config_parse_json(const char *json,
                              RunnerConfig *config,
                              char *error,
                              size_t error_size);

bool runner_config_write_json(const RunnerConfig *config,
                              char *output,
                              size_t output_size,
                              size_t *written,
                              char *error,
                              size_t error_size);

const char *runner_config_theme_key(RunnerThemeMode mode);

#endif
