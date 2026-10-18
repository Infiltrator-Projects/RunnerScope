// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_JSON_H
#define RUNNERSCOPE_JSON_H
#include "cJSON.h"
#include <stddef.h>

/* All cJSON parsing uses one lock: its last-error pointer is process-global.
 * Trees remain private to their caller. Reject invalid UTF-8, raw controls,
 * embedded NUL escapes and permissive numeric spellings before decoding. */
const cJSON *runner_json_unique_member(const cJSON *object, const char *name);
cJSON *runner_json_parse(const char *json, size_t limit);
#endif
