// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_NATIVE_SECURE_CONFIG_H
#define RUNNERSCOPE_NATIVE_SECURE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

bool rs_secure_write_private_text(const char *path,
                                  const char *text,
                                  size_t length);

bool rs_secure_read_bounded_text(const char *path,
                                 char *buffer,
                                 size_t buffer_size,
                                 size_t *length);

#endif
