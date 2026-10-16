// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_WINDOWS_CONFIG_STORE_H
#define RUNNERSCOPE_WINDOWS_CONFIG_STORE_H

#include "core/runner_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

bool runner_windows_config_load(RunnerConfig *config,
                                char *error,
                                size_t error_size);

bool runner_windows_config_save(const RunnerConfig *config,
                                char *error,
                                size_t error_size);

bool runner_windows_config_path(char *output, size_t size);

/* Creates a new owner/SYSTEM directory; child files inherit its private ACL. */
bool runner_windows_private_directory(const wchar_t *path);

#endif
