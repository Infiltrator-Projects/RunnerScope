// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/linux_services.h"
#include <string.h>

#define SYSTEMD_NAME "org.freedesktop.systemd1"
#define SYSTEMD_PATH "/org/freedesktop/systemd1"
#define SYSTEMD_MANAGER "org.freedesktop.systemd1.Manager"

typedef struct {
    GMutex mutex;
    GCond condition;
    gboolean done;
    gint64 deadline;
    GCancellable *cancel;
} ServiceDeadline;

static gpointer deadline_worker(gpointer data)
{
    ServiceDeadline *deadline = data;
    g_mutex_lock(&deadline->mutex);
    while (!deadline->done) {
        if (!g_cond_wait_until(&deadline->condition, &deadline->mutex, deadline->deadline)) {
            g_cancellable_cancel(deadline->cancel);
            break;
        }
    }
    g_mutex_unlock(&deadline->mutex);
    return NULL;
}

static void parent_cancelled(GCancellable *parent, gpointer data)
{
    (void)parent;
    g_cancellable_cancel(G_CANCELLABLE(data));
}

static GThread *deadline_start(ServiceDeadline *deadline, GCancellable *parent,
    gint64 timeout, gulong *connection)
{
    g_mutex_init(&deadline->mutex); g_cond_init(&deadline->condition);
    deadline->cancel = g_cancellable_new();
    deadline->deadline = g_get_monotonic_time() + timeout;
    *connection = parent ? g_cancellable_connect(parent,
        G_CALLBACK(parent_cancelled), deadline->cancel, NULL) : 0U;
    return g_thread_new("runner-service-deadline", deadline_worker, deadline);
}

static void deadline_stop(ServiceDeadline *deadline, GThread *watchdog,
    GCancellable *parent, gulong connection)
{
    g_mutex_lock(&deadline->mutex); deadline->done = TRUE;
    g_cond_signal(&deadline->condition); g_mutex_unlock(&deadline->mutex);
    g_thread_join(watchdog);
    if (parent && connection) g_cancellable_disconnect(parent, connection);
    g_object_unref(deadline->cancel);
    g_mutex_clear(&deadline->mutex); g_cond_clear(&deadline->condition);
}

gboolean runner_linux_service_is_runner(const char *name)
{
    return name && g_str_has_prefix(name, "actions.runner.") &&
        g_str_has_suffix(name, ".service") && !strchr(name, '/');
}

gboolean runner_linux_service_matches_runner(const char *unit,
    const char *organisation, const char *runner)
{
    if (!unit || !organisation || !*organisation || !runner || !*runner) return FALSE;
    char *expected = g_strdup_printf("actions.runner.%s.%s.service", organisation, runner);
    const gboolean matches = !g_ascii_strcasecmp(unit, expected);
    g_free(expected);
    return matches;
}

static char *property_string(GVariant *properties, const char *key, const char *fallback)
{
    const char *value = NULL;
    if (properties) (void)g_variant_lookup(properties, key, "&s", &value);
    return g_strdup(value && *value ? value : fallback);
}

RunnerLinuxService *runner_linux_service_from_properties(const char *name,
    GVariant *unit, GVariant *service, const char *file_state)
{
    RunnerLinuxService *row = g_new0(RunnerLinuxService, 1U);
    row->name = g_strdup(name);
    row->description = property_string(unit, "Description", name);
    row->active_state = property_string(unit, "ActiveState", "unknown");
    row->unit_file_state = property_string(unit, "UnitFileState", file_state ? file_state : "unknown");
    row->user = property_string(service, "User", "root");
    if (service) (void)g_variant_lookup(service, "MainPID", "u", &row->pid);
    row->executable = g_strdup("");
    GVariant *commands = service ? g_variant_lookup_value(service, "ExecStart", NULL) : NULL;
    if (commands && g_variant_is_container(commands) && g_variant_n_children(commands)) {
        GVariant *command = g_variant_get_child_value(commands, 0U);
        if (g_variant_is_container(command) && g_variant_n_children(command)) {
            GVariant *path = g_variant_get_child_value(command, 0U);
            if (g_variant_is_of_type(path, G_VARIANT_TYPE_STRING)) {
                g_free(row->executable);
                row->executable = g_variant_dup_string(path, NULL);
            }
            g_variant_unref(path);
        }
        g_variant_unref(command);
    }
    if (commands) g_variant_unref(commands);
    return row;
}

void runner_linux_service_free(gpointer data)
{
    RunnerLinuxService *row = data;
    if (!row) return;
    g_free(row->name); g_free(row->description); g_free(row->active_state);
    g_free(row->unit_file_state); g_free(row->user); g_free(row->executable); g_free(row);
}

static GVariant *systemd_call(GDBusConnection *bus, const char *path,
    const char *interface, const char *method, GVariant *parameters,
    const GVariantType *reply_type, GCancellable *cancel, GError **error)
{
    return g_dbus_connection_call_sync(bus, SYSTEMD_NAME, path, interface, method,
        parameters, reply_type, G_DBUS_CALL_FLAGS_NONE, 5000, cancel, error);
}

static GVariant *get_properties(GDBusConnection *bus, const char *path,
    const char *interface, GCancellable *cancel, GError **error)
{
    GVariant *reply = systemd_call(bus, path, "org.freedesktop.DBus.Properties", "GetAll",
        g_variant_new("(s)", interface), G_VARIANT_TYPE("(a{sv})"), cancel, error);
    if (!reply) return NULL;
    GVariant *properties = g_variant_get_child_value(reply, 0U);
    g_variant_unref(reply);
    return properties;
}

static gboolean append_service(GDBusConnection *bus, GPtrArray *rows,
    const char *name, const char *path, const char *file_state,
    GCancellable *cancel, GError **error)
{
    GVariant *unit = get_properties(bus, path, "org.freedesktop.systemd1.Unit", cancel, error);
    if (!unit) return FALSE;
    GVariant *service = get_properties(bus, path, "org.freedesktop.systemd1.Service", cancel, error);
    if (!service) { g_variant_unref(unit); return FALSE; }
    g_ptr_array_add(rows, runner_linux_service_from_properties(name, unit, service, file_state));
    g_variant_unref(unit); g_variant_unref(service);
    return TRUE;
}

GPtrArray *runner_linux_services_list(GCancellable *parent, GError **error)
{
    ServiceDeadline deadline = {0};
    gulong connection = 0U;
    GThread *watchdog = deadline_start(&deadline, parent, 15000000, &connection);
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, deadline.cancel, error);
    GPtrArray *rows = g_ptr_array_new_with_free_func(runner_linux_service_free);
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GVariant *reply = bus ? systemd_call(bus, SYSTEMD_PATH, SYSTEMD_MANAGER,
        "ListUnits", NULL, G_VARIANT_TYPE("(a(ssssssouso))"), deadline.cancel, error) : NULL;
    gboolean okay = reply != NULL;
    if (reply) {
        GVariant *units = g_variant_get_child_value(reply, 0U);
        for (gsize i = 0U; okay && i < g_variant_n_children(units); ++i) {
            GVariant *unit = g_variant_get_child_value(units, i);
            const char *name = NULL, *path = NULL;
            g_variant_get_child(unit, 0U, "&s", &name);
            g_variant_get_child(unit, 6U, "&o", &path);
            if (runner_linux_service_is_runner(name)) {
                okay = append_service(bus, rows, name, path, NULL, deadline.cancel, error);
                g_hash_table_add(seen, g_strdup(name));
            }
            g_variant_unref(unit);
        }
        g_variant_unref(units); g_variant_unref(reply);
    }
    reply = okay ? systemd_call(bus, SYSTEMD_PATH, SYSTEMD_MANAGER, "ListUnitFiles",
        NULL, G_VARIANT_TYPE("(a(ss))"), deadline.cancel, error) : NULL;
    if (okay && !reply) okay = FALSE;
    if (reply) {
        GVariant *files = g_variant_get_child_value(reply, 0U);
        for (gsize i = 0U; okay && i < g_variant_n_children(files); ++i) {
            const char *filename = NULL, *state = NULL;
            g_variant_get_child(files, i, "(&s&s)", &filename, &state);
            char *name = g_path_get_basename(filename);
            if (runner_linux_service_is_runner(name) && !g_hash_table_contains(seen, name)) {
                GVariant *loaded = systemd_call(bus, SYSTEMD_PATH, SYSTEMD_MANAGER, "LoadUnit",
                    g_variant_new("(s)", name), G_VARIANT_TYPE("(o)"), deadline.cancel, error);
                if (!loaded) okay = FALSE;
                else {
                    const char *path = NULL;
                    g_variant_get(loaded, "(&o)", &path);
                    okay = append_service(bus, rows, name, path, state, deadline.cancel, error);
                    g_variant_unref(loaded);
                    g_hash_table_add(seen, g_strdup(name));
                }
            }
            g_free(name);
        }
        g_variant_unref(files); g_variant_unref(reply);
    }
    g_hash_table_unref(seen);
    g_clear_object(&bus);
    deadline_stop(&deadline, watchdog, parent, connection);
    if (!okay) { g_ptr_array_unref(rows); return NULL; }
    return rows;
}

gboolean runner_linux_service_restart(const char *name, GCancellable *cancel, GError **error)
{
    if (!runner_linux_service_is_runner(name)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Invalid runner service name");
        return FALSE;
    }
    ServiceDeadline deadline = {0};
    gulong connection = 0U;
    GThread *watchdog = deadline_start(&deadline, cancel, 45000000, &connection);
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, deadline.cancel, error);
    GVariant *reply = bus ? g_dbus_connection_call_sync(bus, SYSTEMD_NAME, SYSTEMD_PATH,
        SYSTEMD_MANAGER, "RestartUnit", g_variant_new("(ss)", name, "replace"),
        G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
        30000, deadline.cancel, error) : NULL;
    g_clear_object(&bus);
    deadline_stop(&deadline, watchdog, cancel, connection);
    if (!reply) return FALSE;
    g_variant_unref(reply);
    return TRUE;
}
