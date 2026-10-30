// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_LINUX_STORAGE_READER_H
#define RUNNERSCOPE_LINUX_STORAGE_READER_H
#include <glib.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* GLib-owned caller-freed buffer. Missing files produce an empty buffer only
 * when requested via 'missing'; invalid or unsafe files remain untouched. */
char *runner_linux_read_text(const char *path, size_t limit,
    gboolean *missing, GError **error);
#ifdef __cplusplus
}
#endif
#endif
