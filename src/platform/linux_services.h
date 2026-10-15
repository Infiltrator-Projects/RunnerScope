// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNER_LINUX_SERVICES_H
#define RUNNER_LINUX_SERVICES_H
#include <gio/gio.h>
typedef struct {
    char *name;
    char *description;
    char *active_state;
    char *unit_file_state;
    char *user;
    char *executable;
    guint32 pid;
} RunnerLinuxService;
gboolean runner_linux_service_is_runner(const char *name);
RunnerLinuxService *runner_linux_service_from_properties(const char *name,
    GVariant *unit, GVariant *service, const char *file_state);
void runner_linux_service_free(gpointer service);
/* Bounded native systemd calls; cancellation also interrupts bus acquisition. */
GPtrArray *runner_linux_services_list(GCancellable *cancel, GError **error);
gboolean runner_linux_service_restart(const char *name, GCancellable *cancel,
    GError **error);
#endif
