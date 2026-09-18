// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "linux_local.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

static int runner_unit_name(const char *name)
{
    static const char prefix[] = "actions.runner.";
    static const char suffix[] = ".service";
    const size_t length = name ? strlen(name) : 0U;
    const size_t prefix_length = sizeof(prefix) - 1U;
    const size_t suffix_length = sizeof(suffix) - 1U;
    return length > prefix_length + suffix_length &&
           strncmp(name, prefix, prefix_length) == 0 &&
           strcmp(name + length - suffix_length, suffix) == 0;
}

static size_t scan_directory(const char *directory,
                             RsLocalRunnerUnit *units,
                             size_t capacity,
                             size_t used)
{
    DIR *dir = opendir(directory);
    if (!dir) return used;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && used < capacity) {
        if (!runner_unit_name(entry->d_name)) continue;

        int duplicate = 0;
        for (size_t i = 0U; i < used; ++i)
            if (strcmp(units[i].service_name, entry->d_name) == 0) {
                duplicate = 1;
                break;
            }
        if (duplicate) continue;

        const int written = snprintf(
            units[used].unit_path, sizeof(units[used].unit_path),
            "%s/%s", directory, entry->d_name);
        if (written < 0 ||
            (size_t)written >= sizeof(units[used].unit_path))
            continue;
        (void)snprintf(units[used].service_name,
                       sizeof(units[used].service_name),
                       "%s", entry->d_name);
        used++;
    }
    closedir(dir);
    return used;
}

size_t rs_linux_discover_runner_units(RsLocalRunnerUnit *units,
                                      size_t capacity)
{
    if (!units || capacity == 0U) return 0U;
    static const char *const paths[] = {
        "/etc/systemd/system",
        "/usr/lib/systemd/system",
        "/lib/systemd/system"
    };
    size_t used = 0U;
    for (size_t i = 0U; i < sizeof(paths) / sizeof(paths[0]); ++i)
        used = scan_directory(paths[i], units, capacity, used);
    return used;
}
