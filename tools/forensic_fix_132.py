#!/usr/bin/env python3
from pathlib import Path


def replace_exact(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


linux_path = Path("src/linux_main.c")
linux = linux_path.read_text(encoding="utf-8")

linux = replace_exact(
    linux,
    "#define RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT 50\n#define RUNNERSCOPE_SUMMARY_TICK_INTERVAL 2U\n",
    "#define RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT 50\n",
    "retired summary tick constant",
)
linux = replace_exact(
    linux,
    "    guint history_save_timer;\n    guint initial_activity_timer;\n    guint initial_local_timer;\n    guint tick_count;\n",
    "    guint history_save_timer;\n    guint initial_activity_timer;\n",
    "retired timer state",
)
linux = replace_exact(
    linux,
    "    config->runner_poll_seconds = 2U;\n    config->activity_scan_seconds = 45U;\n    config->repository_scan_limit = 25U;\n    config->local_health_seconds = 10U;\n",
    "    config->runner_poll_seconds = 3U;\n    config->activity_scan_seconds = 45U;\n    config->repository_scan_limit = 25U;\n    config->local_health_seconds = 15U;\n",
    "provider defaults",
)
linux = replace_exact(
    linux,
    "    if (config->runner_poll_seconds < 1U) config->runner_poll_seconds = 1U;\n    if (config->activity_scan_seconds < 10U) config->activity_scan_seconds = 10U;\n    if (config->repository_scan_limit < 1U) config->repository_scan_limit = 1U;\n    if (config->local_health_seconds < 5U) config->local_health_seconds = 5U;\n",
    "    if (config->runner_poll_seconds < 3U) config->runner_poll_seconds = 3U;\n    if (config->activity_scan_seconds < 10U) config->activity_scan_seconds = 10U;\n    if (config->repository_scan_limit < 1U) config->repository_scan_limit = 1U;\n    if (config->local_health_seconds < 15U) config->local_health_seconds = 15U;\n",
    "provider minimums",
)
linux = replace_exact(
    linux,
    "        \"label { color:@rm_text; }\\n\"\n        \"menu {\"\n        \" background-color:@rm_panel; color:@rm_text;\"\n        \" border:1px solid @rm_border;\"\n        \"}\\n\"\n        \"menu menuitem { color:@rm_summary; }\\n\"\n        \"menu menuitem:hover {\"\n        \" background-color:@rm_surface_hover; color:@rm_title;\"\n        \"}\\n\"\n        \"button, combobox button {\"\n",
    "        \"label { color:@rm_text; }\\n\"\n        \"button, combobox button {\"\n",
    "obsolete menu CSS",
)
linux = replace_exact(
    linux,
    "    g_atomic_int_set(&app->activity_refreshing, 0);\n    g_free(result);\n    request_runner_refresh(app);\n    return G_SOURCE_REMOVE;\n",
    "    g_atomic_int_set(&app->activity_refreshing, 0);\n    g_free(result);\n    return G_SOURCE_REMOVE;\n",
    "redundant post-activity runner refresh",
)
linux = replace_exact(
    linux,
    "static gboolean local_timer_cb(gpointer data)\n{\n    request_local_refresh(data);\n    return G_SOURCE_CONTINUE;\n}\n",
    "static gboolean local_timer_cb(gpointer data)\n{\n    RunnerScopeApp *app = data;\n    if (app->notebook &&\n        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) == 3)\n        request_local_refresh(app);\n    return G_SOURCE_CONTINUE;\n}\n",
    "local health visibility gating",
)
linux = replace_exact(
    linux,
    "static gboolean initial_local_refresh_cb(gpointer data)\n{\n    RunnerScopeApp *app = data;\n    app->initial_local_timer = 0U;\n    request_local_refresh(app);\n    return G_SOURCE_REMOVE;\n}\n\n",
    "",
    "obsolete eager local refresh callback",
)
linux = replace_exact(
    linux,
    "    app->tick_count++;\n    if ((app->tick_count % RUNNERSCOPE_SUMMARY_TICK_INTERVAL) == 0U)\n        update_summary(app);\n",
    "    update_summary(app);\n",
    "summary tick divider",
)
linux = replace_exact(
    linux,
    "    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);\n    sync_navigation(app, page);\n    render_page(app, page);\n    update_workspace_context(app, page);\n    clear_selection_card(app);\n",
    "    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);\n    sync_navigation(app, page);\n    render_page(app, page);\n    update_workspace_context(app, page);\n    if (page == 3)\n        request_local_refresh(app);\n    clear_selection_card(app);\n",
    "on-demand local health refresh",
)
linux = replace_exact(
    linux,
    "    /*\n     * Match the current Infiltrator OS shell: product identity and search live\n     * in the title area, while the data surface is left to the application.\n     */\n",
    "    /*\n     * Match the current Infiltrator OS shell: product identity, settings and\n     * window controls stay in the header; filtering belongs to the workspace.\n     */\n",
    "stale shell comment",
)
linux = replace_exact(
    linux,
    "    GtkWidget *header_end =\n        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);\n\n    GtkWidget *window_controls =\n",
    "    GtkWidget *header_end =\n        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);\n    GtkWidget *settings = make_window_control(\n        \"preferences-system-symbolic\", \"Settings\", NULL);\n    g_signal_connect(settings, \"clicked\", G_CALLBACK(on_settings), app);\n    gtk_box_pack_start(GTK_BOX(header_end), settings, FALSE, FALSE, 0);\n\n    GtkWidget *window_controls =\n",
    "Infiltrator OS header settings control",
)
linux = replace_exact(
    linux,
    "    button = gtk_button_new_with_label(\"Settings\");\n    g_signal_connect(button, \"clicked\", G_CALLBACK(on_settings), app);\n    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);\n\n",
    "",
    "duplicate footer settings button",
)
linux = replace_exact(
    linux,
    "    build_ui(app);\n    request_runner_refresh(app);\n    app->initial_local_timer =\n        g_timeout_add(250U, initial_local_refresh_cb, app);\n    app->initial_activity_timer =\n",
    "    build_ui(app);\n    request_runner_refresh(app);\n    app->initial_activity_timer =\n",
    "eager local startup scan",
)
linux = replace_exact(
    linux,
    "    app->tick_timer = g_timeout_add_seconds(1U, tick_timer_cb, app);\n",
    "    app->tick_timer = g_timeout_add_seconds(2U, tick_timer_cb, app);\n",
    "two-second presentation tick",
)
linux = replace_exact(
    linux,
    "    if (app->initial_activity_timer)\n        g_source_remove(app->initial_activity_timer);\n    if (app->initial_local_timer)\n        g_source_remove(app->initial_local_timer);\n",
    "    if (app->initial_activity_timer)\n        g_source_remove(app->initial_activity_timer);\n",
    "retired local startup timer cleanup",
)

linux_path.write_text(linux, encoding="utf-8")

windows_path = Path("src/windows_main.c")
windows = windows_path.read_text(encoding="utf-8")
windows = replace_exact(
    windows,
    '        "  \\"runner_poll_seconds\\": 2,\\r\\n"\n',
    '        "  \\"runner_poll_seconds\\": 3,\\r\\n"\n',
    "Windows config runner default",
)
windows = replace_exact(
    windows,
    '        "  \\"local_health_seconds\\": 10,\\r\\n"\n',
    '        "  \\"local_health_seconds\\": 15,\\r\\n"\n',
    "Windows config local-health default",
)
windows_path.write_text(windows, encoding="utf-8")

Path("VERSION").write_text("1.2.32\n", encoding="utf-8")

changelog_path = Path("CHANGELOG.md")
changelog = changelog_path.read_text(encoding="utf-8")
changelog = replace_exact(
    changelog,
    "## Unreleased\n\nNo unreleased changes.\n\n",
    "## Unreleased\n\nNo unreleased changes.\n\n"
    "## 1.2.32 - 2026-10-03\n\n"
    "- Remove remaining superseded Linux shell CSS and move Settings into the same header control group used by current Infiltrator OS/System Monitor.\n"
    "- Cut presentation churn in half by updating live duration/utilisation cells every two seconds instead of every second.\n"
    "- Stop the Activity scan from forcing a redundant extra runner API poll after every repository sweep.\n"
    "- Make Local Linux Health on-demand: refresh immediately when opened and poll it only while that workspace is visible, eliminating background systemctl and diagnostic-directory scans.\n"
    "- Raise the minimum runner poll to 3 seconds and Local Health poll to 15 seconds to reduce process and filesystem churn without losing useful live visibility.\n"
    "- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.\n\n",
    "CHANGELOG 1.2.32 entry",
)
changelog_path.write_text(changelog, encoding="utf-8")

debian_path = Path("debian/changelog")
debian = debian_path.read_text(encoding="utf-8")
if not debian.startswith("infiltrator-runner-monitor (1.2.31)"):
    raise SystemExit("debian/changelog: unexpected current release")
debian_entry = """infiltrator-runner-monitor (1.2.32) unstable; urgency=medium

  * Remove remaining obsolete shell CSS and align Settings with the current
    Infiltrator OS titlebar control pattern.
  * Reduce live GTK cell updates and runner polling churn.
  * Poll Local Linux Health only while its workspace is visible.
  * Remove the redundant runner refresh after each activity repository scan.
  * Keep the exact latest Infiltratr Common 1.19.38 pin.

 -- Shannon Smith <infiltratr@yandex.com>  Sat, 03 Oct 2026 13:20:00 +1000

"""
debian_path.write_text(debian_entry + debian, encoding="utf-8")
