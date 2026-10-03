#!/usr/bin/env python3
from pathlib import Path

path = Path("src/linux_main.c")
source = path.read_text()


def replace_once(old: str, new: str) -> None:
    global source
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"expected one occurrence, found {count}: {old[:80]!r}")
    source = source.replace(old, new, 1)


def replace_span(start: str, end: str, replacement: str) -> None:
    global source
    first = source.find(start)
    if first < 0:
        raise SystemExit(f"missing start anchor: {start!r}")
    second = source.find(end, first + len(start))
    if second < 0:
        raise SystemExit(f"missing end anchor: {end!r}")
    source = source[:first] + replacement + source[second:]


replace_once(
    "    GtkWidget *selection_secondary;\n    GtkCssProvider *theme_provider;\n",
    "    GtkWidget *selection_secondary;\n"
    "    GtkWidget *nav_buttons[4];\n"
    "    gboolean navigation_syncing;\n"
    "    GtkCssProvider *theme_provider;\n",
)

replace_once(
    "    GHashTable *sessions;\n    GHashTable *job_by_runner;\n",
    "    GHashTable *sessions;\n"
    "    GHashTable *job_by_runner;\n"
    "    GHashTable *runner_row_by_name;\n"
    "    GHashTable *activity_row_by_url;\n",
)

css_start = '''    g_string_append_printf(
        css,
        ".runner-content-shell {"'''
css_end = '''    g_string_append_printf(
        css,
        "scrolledwindow.runner-table {'''
css_replacement = r'''    g_string_append_printf(
        css,
        ".runner-content-shell {"
        " background:@rm_card; border:1px solid @rm_border;"
        " border-radius:%upx;"
        "}\n"
        ".runner-workspace {"
        " background:@rm_card; border-radius:0 %upx %upx 0;"
        "}\n"
        ".workspace-header {"
        " background:@rm_connection; border-bottom:1px solid @rm_connection_border;"
        " padding:%upx %upx;"
        "}\n"
        ".workspace-title { color:@rm_heading; font-size:18px; font-weight:%u; }\n"
        ".workspace-subtitle { color:@rm_summary; font-size:11px; }\n"
        ".workspace-metric {"
        " min-width:76px; background:@rm_surface;"
        " border:1px solid @rm_connection_border; border-radius:%upx;"
        " padding:%upx %upx;"
        "}\n"
        ".workspace-metric-caption {"
        " color:@rm_detail_label; font-size:9px; font-weight:%u;"
        "}\n"
        ".workspace-metric-value {"
        " color:@rm_heading; font-size:14px; font-weight:%u;"
        "}\n"
        "notebook.runner-notebook { background:@rm_card; border:0; }\n",
        (unsigned int)metrics->panel_radius,
        (unsigned int)metrics->panel_radius,
        (unsigned int)metrics->panel_radius,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->content_padding,
        (unsigned int)type->ui_bold_weight,
        (unsigned int)metrics->control_radius,
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_spacing,
        (unsigned int)type->ui_bold_weight,
        (unsigned int)type->ui_bold_weight);

    g_string_append(
        css,
        "#runner-main-navigation, #runner-main-navigation viewport {"
        " background-image:none; background-color:@rm_panel;"
        " border-color:@rm_connection_border;"
        "}\n"
        "#runner-main-navigation { border-right:1px solid @rm_connection_border; }\n"
        "#runner-main-nav-button {"
        " background-image:none; background-color:transparent;"
        " color:@rm_summary; border:1px solid transparent;"
        " box-shadow:none; margin:2px 4px; padding:6px 8px;"
        "}\n"
        "#runner-main-nav-button:hover {"
        " background-color:@rm_surface_hover; border-color:@rm_border;"
        "}\n"
        "#runner-main-nav-button:checked {"
        " background-image:none; background-color:@rm_selection;"
        " color:@rm_selection_text; border-color:@rm_neutral;"
        " box-shadow:none;"
        "}\n"
        "#runner-main-nav-button .runner-main-nav-label {"
        " color:@rm_summary; font-size:14px; font-weight:700;"
        "}\n"
        "#runner-main-nav-button:hover .runner-main-nav-label { color:@rm_title; }\n"
        "#runner-main-nav-button:checked .runner-main-nav-label { color:@rm_selection_text; }\n"
        "#runner-main-nav-button .runner-main-nav-icon {"
        " color:@rm_neutral; background-color:@rm_surface;"
        " border:1px solid @rm_border; border-radius:10px; padding:5px;"
        "}\n");

'''
replace_span(css_start, css_end, css_replacement)

nav_functions_start = "static GtkWidget *make_nav_content("
nav_functions_end = "static void minimize_window("
nav_functions = r'''static void sync_navigation(RunnerScopeApp *app, gint page)
{
    if (!app) return;
    app->navigation_syncing = TRUE;
    for (gint i = 0; i < 4; i++) {
        if (app->nav_buttons[i])
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(app->nav_buttons[i]), i == page);
    }
    app->navigation_syncing = FALSE;
}

static void on_nav_clicked(GtkButton *button, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    if (!app || app->navigation_syncing || !app->notebook) return;

    const gint encoded = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "runner-nav-page"));
    const gint page = encoded - 1;
    if (page < 0 || page >= 4) return;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);
    sync_navigation(app, page);
    render_page(app, page);
    update_workspace_context(app, page);
    clear_selection_card(app);
}

static GtkWidget *make_nav_button(RunnerScopeApp *app,
                                  gint page,
                                  const char *icon_name,
                                  const char *title,
                                  const char *tooltip)
{
    GtkWidget *button = gtk_toggle_button_new();
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon =
        gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    GtkWidget *label = gtk_label_new(title);

    gtk_widget_set_name(button, "runner-main-nav-button");
    gtk_widget_set_size_request(button, -1, 48);
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(icon), "runner-main-nav-icon");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(label), "runner-main-nav-label");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(button), row);
    if (tooltip && *tooltip)
        gtk_widget_set_tooltip_text(button, tooltip);

    g_object_set_data(
        G_OBJECT(button), "runner-nav-page", GINT_TO_POINTER(page + 1));
    g_signal_connect(
        button, "clicked", G_CALLBACK(on_nav_clicked), app);
    app->nav_buttons[page] = button;
    return button;
}

'''
replace_span(nav_functions_start, nav_functions_end, nav_functions)

nav_build_start = "    GtkWidget *navigation = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);\n"
nav_build_end = "    GtkWidget *workspace = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);\n"
nav_build = r'''    GtkWidget *navigation = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(navigation, "runner-main-navigation");
    gtk_widget_set_size_request(navigation, 204, -1);
    gtk_widget_set_hexpand(navigation, FALSE);
    gtk_widget_set_vexpand(navigation, TRUE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(navigation),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content_shell), navigation, FALSE, TRUE, 0);

    GtkWidget *nav_rail = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(nav_rail), 8);
    gtk_container_add(GTK_CONTAINER(navigation), nav_rail);

'''
replace_span(nav_build_start, nav_build_end, nav_build)

nav_rows_start = "    GtkWidget *nav_runners = make_nav_row(\n"
nav_rows_end = "    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_VERTICAL, (gint)compact_spacing);\n"
nav_rows = r'''    const struct {
        const char *icon;
        const char *title;
        const char *tooltip;
    } nav_items[] = {
        {"computer-symbolic", "Runners", "Fleet state and utilisation"},
        {"media-playback-start-symbolic", "Active jobs", "Work executing now"},
        {"document-open-recent-symbolic", "History", "Session activity"},
        {"utilities-system-monitor-symbolic", "Local Linux health", "Services and diagnostics"}
    };
    for (guint i = 0U; i < G_N_ELEMENTS(nav_items); i++) {
        GtkWidget *nav_button = make_nav_button(
            app, (gint)i, nav_items[i].icon,
            nav_items[i].title, nav_items[i].tooltip);
        gtk_box_pack_start(
            GTK_BOX(nav_rail), nav_button, FALSE, TRUE, 0);
    }
    sync_navigation(app, 0);

'''
replace_span(nav_rows_start, nav_rows_end, nav_rows)

same_text_anchor = '''static gboolean same_text(const char *left, const char *right)
{
    return g_strcmp0(left, right) == 0;
}

'''
index_helpers = same_text_anchor + r'''static void rebuild_runner_row_index(RunnerScopeApp *app)
{
    g_hash_table_remove_all(app->runner_row_by_name);
    for (guint i = 0U; i < app->runner_rows->len; i++) {
        RunnerRow *row = g_ptr_array_index(app->runner_rows, i);
        if (row->name && *row->name)
            g_hash_table_insert(app->runner_row_by_name, row->name, row);
    }
}

static void rebuild_activity_row_index(RunnerScopeApp *app)
{
    g_hash_table_remove_all(app->activity_row_by_url);
    for (guint i = 0U; i < app->activity_rows->len; i++) {
        ActivityRow *row = g_ptr_array_index(app->activity_rows, i);
        if (row->url && *row->url)
            g_hash_table_insert(app->activity_row_by_url, row->url, row);
    }
}

'''
replace_once(same_text_anchor, index_helpers)

replace_once(
    "    GPtrArray *old_rows = app->runner_rows;\n"
    "    app->runner_rows = next_rows;\n"
    "    g_ptr_array_unref(old_rows);\n",
    "    GPtrArray *old_rows = app->runner_rows;\n"
    "    app->runner_rows = next_rows;\n"
    "    rebuild_runner_row_index(app);\n"
    "    g_ptr_array_unref(old_rows);\n",
)

replace_once(
    "    GPtrArray *old_rows = app->activity_rows;\n"
    "    app->activity_rows = result->rows;\n"
    "    result->rows = NULL;\n"
    "    g_ptr_array_unref(old_rows);\n",
    "    GPtrArray *old_rows = app->activity_rows;\n"
    "    app->activity_rows = result->rows;\n"
    "    result->rows = NULL;\n"
    "    rebuild_activity_row_index(app);\n"
    "    g_ptr_array_unref(old_rows);\n",
)

replace_span(
    "static RunnerRow *find_runner_row(",
    "static void refresh_runner_clock_cells(",
    "",
)

replace_once(
    "        RunnerRow *row = find_runner_row(app, name);\n",
    "        RunnerRow *row = g_hash_table_lookup(app->runner_row_by_name, name);\n",
)
replace_once(
    "        ActivityRow *row = find_activity_row(app, url);\n",
    "        ActivityRow *row = g_hash_table_lookup(app->activity_row_by_url, url);\n",
)

tick_anchor = '''static gboolean tick_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    const double monotonic_now = now_monotonic();
'''
tick_replacement = '''static gboolean tick_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    if (app->window) {
        GdkWindow *window = gtk_widget_get_window(app->window);
        if (window &&
            (gdk_window_get_state(window) & GDK_WINDOW_STATE_ICONIFIED) != 0)
            return G_SOURCE_CONTINUE;
    }
    const double monotonic_now = now_monotonic();
'''
replace_once(tick_anchor, tick_replacement)

replace_once(
    "    app->sessions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, session_free);\n"
    "    app->job_by_runner = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, job_summary_free);\n",
    "    app->sessions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, session_free);\n"
    "    app->job_by_runner = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, job_summary_free);\n"
    "    app->runner_row_by_name = g_hash_table_new(g_str_hash, g_str_equal);\n"
    "    app->activity_row_by_url = g_hash_table_new(g_str_hash, g_str_equal);\n",
)

replace_once(
    "    g_hash_table_unref(app->sessions);\n"
    "    g_hash_table_unref(app->job_by_runner);\n"
    "    g_ptr_array_unref(app->runner_rows);\n",
    "    g_hash_table_unref(app->sessions);\n"
    "    g_hash_table_unref(app->job_by_runner);\n"
    "    g_hash_table_unref(app->runner_row_by_name);\n"
    "    g_hash_table_unref(app->activity_row_by_url);\n"
    "    g_ptr_array_unref(app->runner_rows);\n",
)

for marker in (
    "runner-nav-list",
    "runner-sidebar",
    "runner-tab-icon",
    "nav-primary",
    "nav-secondary",
    "make_nav_row",
    "make_nav_content",
    "on_nav_row_selected",
    "find_runner_row",
    "find_activity_row",
):
    if marker in source:
        raise SystemExit(f"stale navigation/per-tick marker remains: {marker}")

path.write_text(source)
Path("VERSION").write_text("1.2.30\n")

changelog_path = Path("CHANGELOG.md")
changelog = changelog_path.read_text()
old_unreleased = "## Unreleased\n\nNo unreleased changes.\n"
new_unreleased = """## Unreleased

No unreleased changes.

## 1.2.30 - 2026-10-03

- Replace the temporary GtkListBox navigation workaround with the same compact toggle-button rail used by the current Infiltrator OS/System Monitor shell.
- Remove the remaining obsolete list-row navigation CSS, subtitle widgets and historical navigation helper code.
- Add direct runner/activity row indexes so one-second live-cell updates no longer perform repeated linear searches through backing arrays.
- Suspend presentation-only one-second repaints while the window is iconified; provider polling continues in the background and the display catches up immediately on restore.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.
"""
if old_unreleased not in changelog:
    raise SystemExit("CHANGELOG Unreleased anchor missing")
changelog_path.write_text(changelog.replace(old_unreleased, new_unreleased, 1))

debian_path = Path("debian/changelog")
debian = debian_path.read_text()
debian_entry = """infiltrator-runner-monitor (1.2.30) unstable; urgency=medium

  * Replace the temporary GtkListBox navigation with the current Infiltrator OS
    toggle-button rail and remove its superseded CSS/helpers.
  * Index live runner and activity rows for constant-time one-second updates.
  * Skip presentation-only repaint work while the window is iconified without
    stopping provider polling.
  * Keep the exact latest Infiltratr Common 1.19.38 pin.

 -- Shannon Smith <infiltratr@yandex.com>  Sat, 03 Oct 2026 12:10:00 +1000

"""
debian_path.write_text(debian_entry + debian)
