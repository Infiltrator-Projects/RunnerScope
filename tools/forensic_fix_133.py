#!/usr/bin/env python3
from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected 1 match, found {count}")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    out, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{label}: expected 1 match, found {count}")
    return out


path = Path("src/linux_main.c")
s = path.read_text(encoding="utf-8")

s = replace_once(
    s,
    "    GtkWidget *filter_entry;\n"
    "    GtkWidget *status_label;\n"
    "    GtkWidget *summary_label;\n"
    "    GtkWidget *updated_label;\n"
    "    GtkWidget *scan_label;\n"
    "    GtkWidget *open_job_button;\n",
    "    GtkWidget *filter_entry;\n"
    "    GtkWidget *status_label;\n"
    "    GtkWidget *open_job_button;\n",
    "legacy summary widget fields",
)
s = replace_once(
    s,
    "    GtkWidget *restart_button;\n"
    "    GtkWidget *counter_labels[7];\n"
    "    GtkWidget *workspace_title;\n",
    "    GtkWidget *restart_button;\n"
    "    GtkWidget *workspace_title;\n",
    "legacy counter fields",
)
s = replace_once(
    s,
    "    gboolean navigation_syncing;\n"
    "    gboolean compact_layout;\n"
    "    GtkCssProvider *theme_provider;\n",
    "    gboolean navigation_syncing;\n"
    "    gboolean compact_layout;\n"
    "    gboolean theme_state_valid;\n"
    "    InfiltratrThemeMode theme_state_mode;\n"
    "    gboolean theme_state_dark;\n"
    "    GtkCssProvider *theme_provider;\n",
    "theme cache fields",
)

s = replace_once(
    s,
    "    const InfiltratrThemePalette *p =\n"
    "        infiltratr_theme_resolve(app->config.theme_mode, system_dark_mode());\n"
    "    const InfiltratrTypography *type = infiltratr_typography();\n",
    "    const gboolean system_dark = system_dark_mode();\n"
    "    const gboolean night_theme =\n"
    "        app->config.theme_mode == INFILTRATR_THEME_NIGHT ||\n"
    "        (app->config.theme_mode == INFILTRATR_THEME_SYSTEM && system_dark);\n"
    "    if (app->theme_state_valid &&\n"
    "        app->theme_state_mode == app->config.theme_mode &&\n"
    "        app->theme_state_dark == night_theme)\n"
    "        return;\n\n"
    "    const InfiltratrThemePalette *p =\n"
    "        infiltratr_theme_resolve(app->config.theme_mode, system_dark);\n"
    "    const InfiltratrTypography *type = infiltratr_typography();\n",
    "theme cache guard",
)
s = replace_once(
    s,
    '        ".header-brand-title, .hero-title {"\n',
    '        ".header-brand-title {"\n',
    "hero typography selector",
)
s = regex_once(
    s,
    r'\n    g_string_append_printf\(\n        css,\n        "\.meta \{.*?\n    g_string_append_printf\(\n        css,\n        "\.runner-content-shell \{"',
    '\n    g_string_append_printf(\n        css,\n        ".runner-content-shell {"',
    "legacy hero/counter CSS",
)
s = replace_once(
    s,
    "    gtk_css_provider_load_from_data(app->theme_provider, css->str, -1, NULL);\n"
    "    g_string_free(css, TRUE);\n",
    "    gtk_css_provider_load_from_data(app->theme_provider, css->str, -1, NULL);\n"
    "    app->theme_state_valid = TRUE;\n"
    "    app->theme_state_mode = app->config.theme_mode;\n"
    "    app->theme_state_dark = night_theme;\n"
    "    g_string_free(css, TRUE);\n",
    "theme cache commit",
)

s = regex_once(
    s,
    r'\nstatic char \*clock_text\(void\)\n\{.*?\n\}\n\nstatic char \*history_time_text\(void\)',
    '\nstatic char *history_time_text(void)',
    "retired hero clock helper",
)
s = regex_once(
    s,
    r'\nstatic void update_counter\(.*?\nstatic gboolean same_text\(',
    '\nstatic gboolean same_text(',
    "legacy summary traversal",
)
s = regex_once(
    s,
    r'\nstatic GtkWidget \*make_counter\(.*?\nstatic void sync_navigation\(',
    '\nstatic void sync_navigation(',
    "legacy counter widget helper",
)

summary_call = "    update_summary(app);\n"
if s.count(summary_call) != 3:
    raise SystemExit(f"summary calls: expected 3, found {s.count(summary_call)}")
s = s.replace(summary_call, "")
s = replace_once(
    s,
    "    char *clock = clock_text();\n"
    "    char *updated = g_strdup_printf(\"Runner data: %s\", clock);\n"
    "    label_set_if_changed(app->updated_label, updated);\n"
    "    g_free(updated);\n"
    "    g_free(clock);\n\n",
    "",
    "legacy runner updated label",
)
s = replace_once(
    s,
    "    char *scan = g_strdup_printf(\n"
    "        \"Runner poll %us  •  Activity scan %us  •  %u repositories scanned\",\n"
    "        app->config.runner_poll_seconds, app->config.activity_scan_seconds,\n"
    "        result->repos_scanned);\n"
    "    label_set_if_changed(app->scan_label, scan);\n"
    "    g_free(scan);\n\n",
    "",
    "legacy activity scan label",
)

s = replace_once(
    s,
    "static gboolean activity_timer_cb(gpointer data)\n"
    "{\n"
    "    request_activity_refresh(data);\n"
    "    return G_SOURCE_CONTINUE;\n"
    "}\n",
    "static gboolean activity_timer_cb(gpointer data)\n"
    "{\n"
    "    RunnerScopeApp *app = data;\n"
    "    if (!app->notebook ||\n"
    "        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) != 3)\n"
    "        request_activity_refresh(app);\n"
    "    return G_SOURCE_CONTINUE;\n"
    "}\n",
    "activity visibility gating",
)
s = replace_once(
    s,
    "static void on_refresh(GtkButton *button, gpointer user_data)\n"
    "{\n"
    "    (void)button;\n"
    "    RunnerScopeApp *app = user_data;\n"
    "    request_runner_refresh(app);\n"
    "    request_activity_refresh(app);\n"
    "    request_local_refresh(app);\n"
    "}\n",
    "static void request_refresh_for_page(RunnerScopeApp *app, gint page)\n"
    "{\n"
    "    if (!app) return;\n"
    "    request_runner_refresh(app);\n"
    "    if (page == 0 || page == 1)\n"
    "        request_activity_refresh(app);\n"
    "    if (page == 3)\n"
    "        request_local_refresh(app);\n"
    "}\n\n"
    "static void on_refresh(GtkButton *button, gpointer user_data)\n"
    "{\n"
    "    (void)button;\n"
    "    RunnerScopeApp *app = user_data;\n"
    "    const gint page = app && app->notebook\n"
    "        ? gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook))\n"
    "        : 0;\n"
    "    request_refresh_for_page(app, page);\n"
    "}\n",
    "contextual manual refresh",
)
s = replace_once(
    s,
    "    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);\n"
    "    sync_navigation(app, page);\n"
    "    render_page(app, page);\n"
    "    update_workspace_context(app, page);\n"
    "    if (page == 3)\n"
    "        request_local_refresh(app);\n"
    "    clear_selection_card(app);\n",
    "    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);\n"
    "    sync_navigation(app, page);\n"
    "    render_page(app, page);\n"
    "    update_workspace_context(app, page);\n"
    "    if (page == 1)\n"
    "        request_activity_refresh(app);\n"
    "    else if (page == 3)\n"
    "        request_local_refresh(app);\n"
    "    clear_selection_card(app);\n",
    "navigation contextual refresh",
)

s = replace_once(
    s,
    "        app->config.runner_poll_seconds = MAX(1U, (guint)g_ascii_strtoull(\n"
    "            gtk_entry_get_text(GTK_ENTRY(entries[2])), NULL, 10));\n",
    "        app->config.runner_poll_seconds = MAX(3U, (guint)g_ascii_strtoull(\n"
    "            gtk_entry_get_text(GTK_ENTRY(entries[2])), NULL, 10));\n",
    "runner poll minimum",
)
s = replace_once(
    s,
    "        app->config.local_health_seconds = MAX(5U, (guint)g_ascii_strtoull(\n"
    "            gtk_entry_get_text(GTK_ENTRY(entries[5])), NULL, 10));\n",
    "        app->config.local_health_seconds = MAX(15U, (guint)g_ascii_strtoull(\n"
    "            gtk_entry_get_text(GTK_ENTRY(entries[5])), NULL, 10));\n",
    "local health minimum",
)
s = replace_once(
    s,
    "g_timeout_add(120U, apply_filter_cb, app);",
    "g_timeout_add(180U, apply_filter_cb, app);",
    "filter debounce",
)
s = replace_once(
    s,
    "g_timeout_add(750U, initial_activity_refresh_cb, app);",
    "g_timeout_add(1200U, initial_activity_refresh_cb, app);",
    "first paint activity delay",
)

s = replace_once(
    s,
    "    const guint control_spacing = metrics ? metrics->control_spacing : 10U;\n"
    "    const guint section_spacing = metrics ? metrics->section_spacing : 18U;\n"
    "    const guint screen_padding = metrics ? metrics->screen_padding : 20U;\n",
    "    const guint control_spacing = metrics ? metrics->control_spacing : 10U;\n"
    "    const guint screen_padding = metrics ? metrics->screen_padding : 20U;\n",
    "retired hero section spacing",
)
s = replace_once(
    s,
    "    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, (gint)section_spacing);\n",
    "    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, (gint)control_spacing);\n",
    "root spacing",
)
s = regex_once(
    s,
    r'    app->updated_label = gtk_label_new\("Runner data: —"\);.*?    gtk_box_pack_start\(GTK_BOX\(hero\), counter_grid, TRUE, TRUE, 0\);\n\n',
    "",
    "legacy duplicate hero and counters",
)

path.write_text(s, encoding="utf-8")

windows_path = Path("src/windows_main.c")
w = windows_path.read_text(encoding="utf-8")
w = replace_once(
    w,
    '        "  \\"repository_cache_seconds\\": 300,\\r\\n"\n',
    "",
    "Windows stale repository cache key",
)
w = replace_once(
    w,
    '        "  \\"history_entries\\": 300,\\r\\n"\n',
    "",
    "Windows stale history key",
)
windows_path.write_text(w, encoding="utf-8")

Path("config.example.json").write_text(
    "{\n"
    '  "organisation": "your-github-organisation",\n'
    '  "expected_runners": 0,\n'
    '  "runner_poll_seconds": 3,\n'
    '  "activity_scan_seconds": 45,\n'
    '  "repository_scan_limit": 25,\n'
    '  "local_health_seconds": 15,\n'
    '  "theme_mode": "system"\n'
    "}\n",
    encoding="utf-8",
)

readme_path = Path("README.md")
readme = readme_path.read_text(encoding="utf-8")
readme = replace_once(
    readme,
    "Both native shells consume Common directly. On Linux, direct buttons own refresh, export and settings; theme selection lives inside Settings, and the only title-bar menu is Help → About Runner Monitor. The selected theme mode is persisted in the existing Runner Monitor configuration, and Follow system tracks live GTK host-theme changes.\n\n"
    "The Linux footer keeps selection-specific actions in a dedicated action row and status/version information in a separate row so controls remain readable at ordinary desktop widths. Enabled and disabled actions use Common palette roles instead of falling back to bright toolkit defaults in Night mode.\n",
    "Both native shells consume Common directly. On Linux, the shell follows the current Infiltrator OS structure: branded header with Settings and window controls, responsive navigation rail, active workspace header/search/metrics, table surface, contextual selection actions and a compact status/version footer. The retired menubar and duplicate hero/counter summary from earlier iterations are not part of the current shell. Theme selection lives inside Settings, and Follow system tracks live GTK host-theme changes.\n\n"
    "The Linux footer keeps selection-specific actions in a dedicated action row and status/version information in a separate row so controls remain readable at ordinary desktop widths. Enabled and disabled actions use Common palette roles instead of falling back to bright toolkit defaults in Night mode.\n",
    "README appearance description",
)
readme_path.write_text(readme, encoding="utf-8")

Path("VERSION").write_text("1.2.33\n", encoding="utf-8")

changelog_path = Path("CHANGELOG.md")
changelog = changelog_path.read_text(encoding="utf-8")
changelog = replace_once(
    changelog,
    "## Unreleased\n\nNo unreleased changes.\n\n",
    "## Unreleased\n\nNo unreleased changes.\n\n"
    "## 1.2.33 - 2026-10-03\n\n"
    "- Remove the duplicate hero/counter summary left from the pre-standard Linux shell; the window now follows the current Infiltrator OS header → navigation → workspace → footer hierarchy without repeated fleet state surfaces.\n"
    "- Remove the associated two-second global summary traversal/allocations and cache effective theme state so duplicate GTK theme notifications do not rebuild and reload the full CSS projection.\n"
    "- Make manual refresh contextual and stop the expensive activity repository sweep while Local Linux Health is active; opening Active Jobs still refreshes it immediately.\n"
    "- Enforce the same 3-second runner and 15-second local-health minimums in Settings that configuration loading already enforced, debounce filtering slightly longer, and give first paint more time before the initial activity sweep.\n"
    "- Purge stale repository-cache/history configuration keys and documentation left from earlier implementations.\n"
    "- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.\n\n",
    "CHANGELOG 1.2.33",
)
changelog_path.write_text(changelog, encoding="utf-8")

debian_path = Path("debian/changelog")
debian = debian_path.read_text(encoding="utf-8")
if not debian.startswith("infiltrator-runner-monitor (1.2.32)"):
    raise SystemExit("debian/changelog: unexpected current release")
entry = """infiltrator-runner-monitor (1.2.33) unstable; urgency=medium

  * Remove the obsolete duplicate hero/counter summary and align the Linux
    shell with the current Infiltrator OS workspace hierarchy.
  * Remove periodic global-summary churn and cache effective theme state.
  * Make refresh work contextual and suppress Activity scanning on Local Health.
  * Correct Settings poll minimums and remove stale configuration keys/docs.
  * Keep the exact latest Infiltratr Common 1.19.38 pin.

 -- Shannon Smith <infiltratr@yandex.com>  Sat, 03 Oct 2026 14:35:00 +1000

"""
debian_path.write_text(entry + debian, encoding="utf-8")
