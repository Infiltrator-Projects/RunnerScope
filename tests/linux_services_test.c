// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/linux_services.h"

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_assert_true(runner_linux_service_is_runner("actions.runner.Org.Project.service"));
    g_assert_false(runner_linux_service_is_runner("unrelated.service"));
    g_assert_false(runner_linux_service_is_runner("actions.runner.Org.timer"));
    g_assert_false(runner_linux_service_is_runner("actions.runner./bad.service"));
    g_assert_false(runner_linux_service_is_runner(NULL));

    GVariantBuilder unit_builder, service_builder, commands;
    g_variant_builder_init(&unit_builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&unit_builder, "{sv}", "Description", g_variant_new_string("Fixture runner"));
    g_variant_builder_add(&unit_builder, "{sv}", "ActiveState", g_variant_new_string("active"));
    g_variant_builder_add(&unit_builder, "{sv}", "UnitFileState", g_variant_new_string("enabled"));
    GVariant *unit = g_variant_ref_sink(g_variant_builder_end(&unit_builder));
    g_variant_builder_init(&service_builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&service_builder, "{sv}", "MainPID", g_variant_new_uint32(1234U));
    g_variant_builder_add(&service_builder, "{sv}", "User", g_variant_new_string("fixture-user"));
    g_variant_builder_init(&commands, G_VARIANT_TYPE("a(sasbttttuii)"));
    const char *arguments[] = {"/opt/Runner With Spaces/bin/Runner.Listener", "run", NULL};
    g_variant_builder_add(&commands, "(s^asbttttuii)", arguments[0], arguments,
        FALSE, (guint64)0, (guint64)0, (guint64)0, (guint64)0, 0U, 0, 0);
    g_variant_builder_add(&service_builder, "{sv}", "ExecStart", g_variant_builder_end(&commands));
    GVariant *service = g_variant_ref_sink(g_variant_builder_end(&service_builder));
    RunnerLinuxService *row = runner_linux_service_from_properties(
        "actions.runner.fixture.service", unit, service, "disabled");
    g_assert_cmpstr(row->name, ==, "actions.runner.fixture.service");
    g_assert_cmpstr(row->description, ==, "Fixture runner");
    g_assert_cmpstr(row->active_state, ==, "active");
    g_assert_cmpstr(row->unit_file_state, ==, "enabled");
    g_assert_cmpstr(row->user, ==, "fixture-user");
    g_assert_cmpstr(row->executable, ==, arguments[0]);
    g_assert_cmpuint(row->pid, ==, 1234U);
    runner_linux_service_free(row);
    g_variant_unref(unit); g_variant_unref(service);
    row = runner_linux_service_from_properties("actions.runner.offline.service", NULL, NULL, "disabled");
    g_assert_cmpstr(row->active_state, ==, "unknown");
    g_assert_cmpstr(row->unit_file_state, ==, "disabled");
    g_assert_cmpstr(row->user, ==, "root");
    g_assert_cmpstr(row->executable, ==, "");
    g_assert_cmpuint(row->pid, ==, 0U);
    runner_linux_service_free(row);

    GError *error = NULL;
    g_assert_false(runner_linux_service_restart("unrelated.service", NULL, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
    GCancellable *cancel = g_cancellable_new();
    g_cancellable_cancel(cancel);
    GPtrArray *rows = runner_linux_services_list(cancel, &error);
    g_assert_null(rows);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    g_assert_false(runner_linux_service_restart("actions.runner.fixture.service", cancel, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    g_object_unref(cancel);
    g_print("Native services: runner filtering, properties, executable paths, defaults, cancellation and restart validation passed.\n");
    return 0;
}
