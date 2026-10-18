// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_LINUX_STORAGE_H
#define RUNNERSCOPE_LINUX_STORAGE_H
#include "core/runner_config.h"
#include <glib.h>

#define RUNNER_HISTORY_LIMIT 300U
#define RUNNER_HISTORY_BYTE_LIMIT (4U * 1024U * 1024U)

typedef struct {
    char *time_text;
    char *runner;
    char *event;
    char *detail;
} RunnerHistoryEntry;

char *runner_linux_config_path(void);
char *runner_linux_history_path(void);
gboolean runner_linux_config_load(RunnerConfig *config, GError **error);
gboolean runner_linux_config_save(const RunnerConfig *config, GError **error);

void runner_history_entry_free(gpointer entry);
/* Missing history is empty; malformed/unreadable history is an error, never
 * an empty successful load. The caller must preserve that file on failure. */
GPtrArray *runner_linux_history_load(GError **error);
gboolean runner_linux_history_save(const GPtrArray *entries, GError **error);
#endif
