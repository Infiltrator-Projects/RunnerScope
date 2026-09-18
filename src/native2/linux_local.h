// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_NATIVE_LINUX_LOCAL_H
#define RUNNERSCOPE_NATIVE_LINUX_LOCAL_H

#include <stddef.h>

typedef struct {
    char service_name[256];
    char unit_path[512];
} RsLocalRunnerUnit;

size_t rs_linux_discover_runner_units(RsLocalRunnerUnit *units,
                                      size_t capacity);

#endif
