#!/usr/bin/env python3
from pathlib import Path

path = Path("src/linux_main.c")
source = path.read_text()


def replace_once(old: str, new: str) -> None:
    global source
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"expected one occurrence, found {count}: {old[:120]!r}")
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


# Match the current System Monitor shell density constants.
replace_once(
    '#define RUNNERSCOPE_APP_ID "net.ssmith.runnerscope"\n',
    '#define RUNNERSCOPE_APP_ID "net.ssmith.runnerscope"\n'
    '#define RUNNERSCOPE_COMPACT_LAYOUT_THRESHOLD 1100\n'
    '#define RUNNERSCOPE_COMPACT_LAYOUT_HYSTERESIS 64\n'
    '#define RUNNERSCOPE_MAIN_NAV_WIDTH 195\n'
    '#define RUNNERSCOPE_MAIN_NAV_COMPACT_WIDTH 64\n'
    '#define RUNNERSCOPE_MAIN_NAV_BUTTON_WIDTH 179\n'
    '#define RUNNERSCOPE_MAIN_NAV_BUTTON_COMPACT_WIDTH 48\n'
    '#define RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT 50\n'
    '#define RUNNERSCOPE_SUMMARY_TICK_INTERVAL 2U\n',
)

replace_once(
    '    GtkWidget *selection_secondary;\n'
    '    GtkWidget *nav_buttons[4];\n'
    '    gboolean navigation_syncing;\n'
    '    GtkCssProvider *theme_provider;\n',
    '    GtkWidget *selection_secondary;\n'
    '    GtkWidget *nav_buttons[4];\n'
    '    GtkWidget *main_navigation;\n'
    '    gboolean navigation_syncing;\n'
    '    gboolean compact_layout;\n'
    '    GtkCssProvider *theme_provider;\n',
)

replace_once(
    '    guint tick_timer;\n'
    '    guint filter_timer;\n'
    '    guint history_save_timer;\n',
    '    guint tick_timer;\n'
    '    guint filter_timer;\n'
    '    guint history_save_timer;\n'
    '    guint initial_activity_timer;\n'
    '    guint initial_local_timer;\n'
    '    guint tick_count;\n',
)

replace_once(
    '    GThread *runner_thread;\n'
    '    GThread *activity_thread;\n'
    '    GThread *local_thread;\n'
    '    gint shutting_down;\n',
    '    GThread *runner_thread;\n'
    '    GThread *activity_thread;\n'
    '    GThread *local_thread;\n'
    '    GThread *restart_thread;\n'
    '    gint restart_refreshing;\n'
    '    gint shutting_down;\n',
)

restart_result_anchor = '''typedef struct {
    RunnerScopeApp *app;
    GPtrArray *rows;
    char *error;
} LocalRefreshResult;

'''
restart_result_block = restart_result_anchor + '''typedef struct {
    RunnerScopeApp *app;
    char *service;
    char *error;
} RestartResult;

'''
replace_once(restart_result_anchor, restart_result_block)

# Remove the retired static page-header chrome from the style projection.
replace_once(
    '".header-brand-title, .page-title, .hero-title {"\n',
    '".header-brand-title, .hero-title {"\n',
)

replace_once(
    '        ".page-header { padding:2px 2px 4px 2px; }\\n"\n'
    '        ".page-eyebrow { color:@rm_kicker; font-size:10px; font-weight:%u; }\\n"\n'
    '        ".page-title { color:@rm_heading; font-size:28px; }\\n"\n'
    '        ".page-summary { color:@rm_summary; font-size:12px; }\\n"\n'
    '        ".meta { color:@rm_detail_label; font-size:11px; }\\n"\n',
    '        ".meta { color:@rm_detail_label; font-size:11px; }\\n"\n',
)

# Search now lives with the workspace it filters, not in the title bar.
replace_once(
    '        ".header-search {"\n'
    '        " min-width:300px; min-height:34px; background:@rm_input; color:@rm_text;"\n'
    '        " border:1px solid @rm_connection_border; border-radius:%upx;"\n'
    '        " padding:%upx %upx;"\n'
    '        "}\\n"\n'
    '        ".runner-menubar, .runner-menubar menuitem {"\n'
    '        " background:transparent; color:@rm_summary;"\n'
    '        "}\\n"\n'
    '        ".runner-menubar menuitem { padding:5px 7px; border-radius:%upx; }\\n"\n'
    '        ".runner-menubar menuitem:hover {"\n'
    '        " background:@rm_surface_hover; color:@rm_title;"\n'
    '        "}\\n"\n'
    '        ".runner-window-controls { margin-left:%upx; }\\n"\n',
    '        ".workspace-search {"\n'
    '        " min-width:220px; min-height:34px; background:@rm_input; color:@rm_text;"\n'
    '        " border:1px solid @rm_connection_border; border-radius:%upx;"\n'
    '        " padding:%upx %upx;"\n'
    '        "}\\n"\n'
    '        ".runner-window-controls { margin-left:%upx; }\\n"\n',
)

# The removed menu block consumed one small-radius printf argument.
replace_once(
    '        (unsigned int)metrics->compact_spacing,\n'
    '        (unsigned int)metrics->control_spacing,\n'
    '        (unsigned int)metrics->small_radius,\n'
    '        (unsigned int)metrics->control_spacing,\n'
    '        (unsigned int)metrics->small_radius);\n',
    '        (unsigned int)metrics->compact_spacing,\n'
    '        (unsigned int)metrics->control_spacing,\n'
    '        (unsigned int)metrics->control_spacing,\n'
    '        (unsigned int)metrics->small_radius);\n',
)

# Keep the existing About dialog, but expose it as a direct footer action and
# remove the superseded conventional Help menu helpers.
replace_once(
    'static void on_menu_about(GtkMenuItem *item, gpointer user_data)\n'
    '{\n'
    '    (void)item;\n',
    'static void on_about(GtkButton *button, gpointer user_data)\n'
    '{\n'
    '    (void)button;\n',
)
replace_span(
    'static GtkWidget *menu_item(\n',
    'static char *csv_escape(',
    '',
)

# The summary label is live, but workspace metrics are updated only by the data
# source that owns them (plus History uptime). Avoid a full workspace pass on
# every one-second presentation tick.
replace_once(
    '    guint jobs = 0U;\n'
    '    double busy = 0.0;\n'
    '    GHashTableIter iter;\n',
    '    guint jobs = 0U;\n'
    '    double busy = 0.0;\n'
    '    const double now = now_monotonic();\n'
    '    GHashTableIter iter;\n',
)
replace_once(
    '        if (strcmp(session->state, "RUNNING") == 0 && session->busy_started > 0.0)\n'
    '            busy += now_monotonic() - session->busy_started;\n'
    '    }\n'
    '    const double elapsed = MAX(1.0, now_monotonic() - app->session_started);\n',
    '        if (strcmp(session->state, "RUNNING") == 0 && session->busy_started > 0.0)\n'
    '            busy += now - session->busy_started;\n'
    '    }\n'
    '    const double elapsed = MAX(1.0, now - app->session_started);\n',
)
replace_once(
    '    g_free(summary);\n'
    '    g_free(up_text);\n'
    '\n'
    '    if (app->notebook)\n'
    '        update_workspace_context(\n'
    '            app, gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));\n'
    '}\n',
    '    g_free(summary);\n'
    '    g_free(up_text);\n'
    '}\n',
)

# Refresh only the workspace metrics whose backing data actually changed.
replace_once(
    '    if (page == 0 && rows_changed)\n'
    '        render_runners(app);\n'
    '    else if (page == 2 && history_changed)\n'
    '        render_history(app);\n'
    '\n'
    '    update_summary(app);\n',
    '    if (page == 0 && rows_changed)\n'
    '        render_runners(app);\n'
    '    else if (page == 2 && history_changed)\n'
    '        render_history(app);\n'
    '    if (page == 0 || (page == 2 && history_changed))\n'
    '        update_workspace_context(app, page);\n'
    '\n'
    '    update_summary(app);\n',
)

replace_once(
    '    if (rows_changed &&\n'
    '        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) == 1)\n'
    '        render_activity(app);\n'
    '\n'
    '    update_summary(app);\n',
    '    const gint page =\n'
    '        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));\n'
    '    if (rows_changed && page == 1) {\n'
    '        render_activity(app);\n'
    '        update_workspace_context(app, page);\n'
    '    }\n'
    '\n'
    '    update_summary(app);\n',
)

replace_once(
    '    if (rows_changed &&\n'
    '        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) == 3)\n'
    '        render_local(app);\n'
    '\n'
    '    g_atomic_int_set(&app->local_refreshing, 0);\n',
    '    const gint page =\n'
    '        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));\n'
    '    if (rows_changed && page == 3) {\n'
    '        render_local(app);\n'
    '        update_workspace_context(app, page);\n'
    '    }\n'
    '\n'
    '    g_atomic_int_set(&app->local_refreshing, 0);\n',
)

# One-second ticks now touch only genuinely time-dependent cells. Global hero
# utilisation is refreshed every two seconds; Local Health does no periodic GTK
# work at all unless its provider data changes.
tick_old = '''static gboolean tick_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    if (app->window) {
        GdkWindow *window = gtk_widget_get_window(app->window);
        if (window &&
            (gdk_window_get_state(window) & GDK_WINDOW_STATE_ICONIFIED) != 0)
            return G_SOURCE_CONTINUE;
    }
    const double monotonic_now = now_monotonic();
    const double wall_now = (double)time(NULL);

    /*
     * Time-dependent cells used to trigger complete GtkListStore rebuilds once
     * a second.  Update those cells in place instead; this preserves selection,
     * scroll position and responsiveness while keeping the clocks live.
     */
    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (page == 0)
        refresh_runner_clock_cells(app, monotonic_now, wall_now);
    else if (page == 1)
        refresh_activity_clock_cells(app, wall_now);
    update_summary(app);
    return G_SOURCE_CONTINUE;
}
'''
tick_new = '''static gboolean tick_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    if (app->window) {
        GdkWindow *window = gtk_widget_get_window(app->window);
        if (window &&
            (gdk_window_get_state(window) & GDK_WINDOW_STATE_ICONIFIED) != 0)
            return G_SOURCE_CONTINUE;
    }

    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (page == 0) {
        const double monotonic_now = now_monotonic();
        refresh_runner_clock_cells(
            app, monotonic_now, (double)time(NULL));
    } else if (page == 1) {
        refresh_activity_clock_cells(app, (double)time(NULL));
    } else if (page == 2) {
        update_workspace_context(app, page);
    }

    app->tick_count++;
    if ((app->tick_count % RUNNERSCOPE_SUMMARY_TICK_INTERVAL) == 0U)
        update_summary(app);
    return G_SOURCE_CONTINUE;
}
'''
replace_once(tick_old, tick_new)

# Asynchronous restart: pkexec/systemctl must never block the GTK main loop.
restart_old_start = 'static void on_restart(GtkButton *button, gpointer user_data)\n'
restart_old_end = 'static void on_runner_selection('
restart_new = r'''static gboolean restart_apply_idle(gpointer data)
{
    RestartResult *result = data;
    RunnerScopeApp *app = result->app;

    if (app->restart_thread) {
        g_thread_unref(app->restart_thread);
        app->restart_thread = NULL;
    }

    if (result->error) {
        char *message = g_strdup_printf(
            "Runner restart failed: %s", result->error);
        label_set_if_changed(app->status_label, message);
        g_free(message);
    } else {
        label_set_if_changed(app->status_label, "Runner service restarted.");
        request_local_refresh(app);
    }

    g_atomic_int_set(&app->restart_refreshing, 0);
    if (app->restart_button && app->local_tree) {
        GtkTreeSelection *selection =
            gtk_tree_view_get_selection(GTK_TREE_VIEW(app->local_tree));
        GtkTreeModel *model = NULL;
        GtkTreeIter iter;
        gtk_widget_set_sensitive(
            app->restart_button,
            gtk_tree_selection_get_selected(selection, &model, &iter));
    }

    g_free(result->service);
    g_free(result->error);
    g_free(result);
    return G_SOURCE_REMOVE;
}

static gpointer restart_worker(gpointer data)
{
    RestartResult *result = data;
    char *argv[] = {
        (char *)"pkexec", (char *)"systemctl", (char *)"restart",
        result->service, NULL
    };
    GError *error = NULL;
    char *output = run_command(argv, &error);
    if (!output) {
        result->error = g_strdup(
            error && error->message ? error->message : "unknown error");
        g_clear_error(&error);
    } else {
        g_free(output);
    }

    if (g_atomic_int_get(&result->app->shutting_down)) {
        g_free(result->service);
        g_free(result->error);
        g_free(result);
        return NULL;
    }

    g_idle_add(restart_apply_idle, result);
    return NULL;
}

static void on_restart(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    if (g_atomic_int_get(&app->restart_refreshing)) return;

    char *service = NULL;
    if (!selected_string(app->local_tree, LOCAL_COL_SERVICE_NAME, &service))
        return;

    GtkWidget *confirm = gtk_message_dialog_new(
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "Restart %s?\n\nIf the runner is executing a job, that job will be interrupted.",
        service);
    const gint response = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (response != GTK_RESPONSE_YES) {
        g_free(service);
        return;
    }

    if (!g_atomic_int_compare_and_exchange(
            &app->restart_refreshing, 0, 1)) {
        g_free(service);
        return;
    }

    RestartResult *result = g_new0(RestartResult, 1U);
    result->app = app;
    result->service = service;
    char *status = g_strdup_printf("Restarting %s…", service);
    label_set_if_changed(app->status_label, status);
    g_free(status);
    gtk_widget_set_sensitive(app->restart_button, FALSE);
    app->restart_thread =
        g_thread_new("runnerscope-restart", restart_worker, result);
}

'''
replace_span(restart_old_start, restart_old_end, restart_new)

replace_once(
    '    gtk_widget_set_sensitive(app->restart_button, selected);\n',
    '    gtk_widget_set_sensitive(\n'
    '        app->restart_button,\n'
    '        selected && !g_atomic_int_get(&app->restart_refreshing));\n',
)

# Match System Monitor's responsive 195 px / 64 px navigation rail and hide
# labels below the same 1100 px threshold with hysteresis.
nav_function_anchor = '''static GtkWidget *make_nav_button(RunnerScopeApp *app,
                                  gint page,
                                  const char *icon_name,
                                  const char *title,
                                  const char *tooltip)
{
'''
replace_span(
    nav_function_anchor,
    'static void minimize_window(',
    r'''static GtkWidget *make_nav_button(RunnerScopeApp *app,
                                  gint page,
                                  const char *icon_name,
                                  const char *title,
                                  const char *tooltip)
{
    const gboolean compact = app && app->compact_layout;
    GtkWidget *button = gtk_toggle_button_new();
    GtkWidget *row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, compact ? 0 : 10);
    GtkWidget *icon =
        gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    GtkWidget *label = gtk_label_new(title);

    gtk_widget_set_name(button, "runner-main-nav-button");
    gtk_widget_set_size_request(
        button,
        compact ? RUNNERSCOPE_MAIN_NAV_BUTTON_COMPACT_WIDTH
                : RUNNERSCOPE_MAIN_NAV_BUTTON_WIDTH,
        RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT);
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(icon), "runner-main-nav-icon");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(label), "runner-main-nav-label");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_no_show_all(label, TRUE);
    gtk_widget_set_visible(label, !compact);
    gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(button), row);
    if (tooltip && *tooltip)
        gtk_widget_set_tooltip_text(button, tooltip);

    g_object_set_data(
        G_OBJECT(button), "runner-nav-page", GINT_TO_POINTER(page + 1));
    g_object_set_data(G_OBJECT(button), "runner-nav-label-widget", label);
    g_object_set_data(G_OBJECT(button), "runner-nav-row-widget", row);
    g_signal_connect(
        button, "clicked", G_CALLBACK(on_nav_clicked), app);
    app->nav_buttons[page] = button;
    return button;
}

static void apply_navigation_density(RunnerScopeApp *app)
{
    if (!app) return;
    const gboolean compact = app->compact_layout;

    if (app->main_navigation)
        gtk_widget_set_size_request(
            app->main_navigation,
            compact ? RUNNERSCOPE_MAIN_NAV_COMPACT_WIDTH
                    : RUNNERSCOPE_MAIN_NAV_WIDTH,
            -1);
    if (app->filter_entry)
        gtk_widget_set_size_request(
            app->filter_entry, compact ? 180 : 240, -1);
    if (app->workspace_subtitle)
        gtk_widget_set_visible(app->workspace_subtitle, !compact);

    for (gint page = 0; page < 4; page++) {
        GtkWidget *button = app->nav_buttons[page];
        if (!button) continue;
        gtk_widget_set_size_request(
            button,
            compact ? RUNNERSCOPE_MAIN_NAV_BUTTON_COMPACT_WIDTH
                    : RUNNERSCOPE_MAIN_NAV_BUTTON_WIDTH,
            RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT);
        GtkWidget *label = g_object_get_data(
            G_OBJECT(button), "runner-nav-label-widget");
        GtkWidget *row = g_object_get_data(
            G_OBJECT(button), "runner-nav-row-widget");
        if (label) gtk_widget_set_visible(label, !compact);
        if (row) gtk_box_set_spacing(GTK_BOX(row), compact ? 0 : 10);
    }
}

static gboolean on_window_configure(GtkWidget *widget,
                                    GdkEventConfigure *event,
                                    gpointer user_data)
{
    (void)widget;
    RunnerScopeApp *app = user_data;
    if (!app || !event || event->width <= 0) return FALSE;

    const gint compact_limit = app->compact_layout
        ? RUNNERSCOPE_COMPACT_LAYOUT_THRESHOLD +
            RUNNERSCOPE_COMPACT_LAYOUT_HYSTERESIS
        : RUNNERSCOPE_COMPACT_LAYOUT_THRESHOLD;
    const gboolean compact = event->width < compact_limit;
    if (compact != app->compact_layout) {
        app->compact_layout = compact;
        apply_navigation_density(app);
    }
    return FALSE;
}

''',
)

# Keep the titlebar limited to product identity and window controls, as in the
# current Infiltrator OS shell.
header_old = '''    GtkWidget *header_end = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);
    GtkWidget *menu_bar = build_menu_bar(app);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(menu_bar), "runner-menubar");
    gtk_box_pack_start(GTK_BOX(header_end), menu_bar, FALSE, FALSE, 0);

    app->filter_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(app->filter_entry), "Filter current view…");
    gtk_widget_set_size_request(app->filter_entry, 320, -1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->filter_entry), "header-search");
    g_signal_connect(
        app->filter_entry, "changed", G_CALLBACK(on_filter_changed), app);
    gtk_box_pack_start(
        GTK_BOX(header_end), app->filter_entry, FALSE, FALSE, 0);

    GtkWidget *window_controls =
'''
header_new = '''    GtkWidget *header_end =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);

    GtkWidget *window_controls =
'''
replace_once(header_old, header_new)

# Remove the duplicate static page header. Keep the last-update label and place
# it inside the live fleet hero beside the organisation identity.
page_header_start = '''    GtkWidget *page_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)section_spacing);
'''
page_header_end = '''    /* Fleet state is grouped into one primary summary surface. */
'''
page_header_replacement = '''    app->updated_label = gtk_label_new("Runner data: —");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->updated_label), "meta");
    gtk_widget_set_halign(app->updated_label, GTK_ALIGN_START);

'''
replace_span(page_header_start, page_header_end, page_header_replacement)

replace_once(
    '    gtk_box_pack_start(GTK_BOX(hero_copy), org, FALSE, FALSE, 0);\n'
    '    gtk_box_pack_start(GTK_BOX(hero_top), hero_copy, TRUE, TRUE, 0);\n',
    '    gtk_box_pack_start(GTK_BOX(hero_copy), org, FALSE, FALSE, 0);\n'
    '    gtk_box_pack_start(\n'
    '        GTK_BOX(hero_copy), app->updated_label, FALSE, FALSE, 0);\n'
    '    gtk_box_pack_start(GTK_BOX(hero_top), hero_copy, TRUE, TRUE, 0);\n',
)

# Put the filter in the workspace header where it directly scopes the active
# table, rather than consuming titlebar space.
workspace_copy_anchor = '''    gtk_box_pack_start(GTK_BOX(workspace_header), workspace_copy, TRUE, TRUE, 0);

    GtkWidget *workspace_metrics = gtk_grid_new();
'''
workspace_copy_new = '''    gtk_box_pack_start(GTK_BOX(workspace_header), workspace_copy, TRUE, TRUE, 0);

    app->filter_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(app->filter_entry), "Filter current view…");
    gtk_widget_set_size_request(app->filter_entry, 240, -1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->filter_entry), "workspace-search");
    g_signal_connect(
        app->filter_entry, "changed", G_CALLBACK(on_filter_changed), app);
    gtk_box_pack_start(
        GTK_BOX(workspace_header), app->filter_entry, FALSE, FALSE, 0);

    GtkWidget *workspace_metrics = gtk_grid_new();
'''
replace_once(workspace_copy_anchor, workspace_copy_new)

replace_once(
    '    GtkWidget *navigation = gtk_scrolled_window_new(NULL, NULL);\n'
    '    gtk_widget_set_name(navigation, "runner-main-navigation");\n'
    '    gtk_widget_set_size_request(navigation, 204, -1);\n',
    '    GtkWidget *navigation = gtk_scrolled_window_new(NULL, NULL);\n'
    '    app->main_navigation = navigation;\n'
    '    gtk_widget_set_name(navigation, "runner-main-navigation");\n'
    '    gtk_widget_set_size_request(\n'
    '        navigation, RUNNERSCOPE_MAIN_NAV_WIDTH, -1);\n',
)

# Direct footer About action replaces the removed Help menu.
replace_once(
    '    button = gtk_button_new_with_label("Settings");\n'
    '    g_signal_connect(button, "clicked", G_CALLBACK(on_settings), app);\n'
    '    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);\n'
    '\n'
    '    button = gtk_button_new_with_label("Refresh now");\n',
    '    button = gtk_button_new_with_label("Settings");\n'
    '    g_signal_connect(button, "clicked", G_CALLBACK(on_settings), app);\n'
    '    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);\n'
    '\n'
    '    button = gtk_button_new_with_label("About");\n'
    '    g_signal_connect(button, "clicked", G_CALLBACK(on_about), app);\n'
    '    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);\n'
    '\n'
    '    button = gtk_button_new_with_label("Refresh now");\n',
)

# Apply responsive navigation after every settled configure event.
replace_once(
    '    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);\n',
    '    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);\n'
    '    g_signal_connect(\n'
    '        app->window, "configure-event",\n'
    '        G_CALLBACK(on_window_configure), app);\n',
)

# Stagger startup work so the GTK shell paints before the expensive activity
# scanner starts, and do not eagerly populate the hidden History model.
timer_anchor = '''static gboolean local_timer_cb(gpointer data)
{
    request_local_refresh(data);
    return G_SOURCE_CONTINUE;
}

'''
timer_new = timer_anchor + '''static gboolean initial_local_refresh_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    app->initial_local_timer = 0U;
    request_local_refresh(app);
    return G_SOURCE_REMOVE;
}

static gboolean initial_activity_refresh_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    app->initial_activity_timer = 0U;
    request_activity_refresh(app);
    return G_SOURCE_REMOVE;
}

'''
replace_once(timer_anchor, timer_new)

replace_once(
    '    build_ui(app);\n'
    '    render_history(app);\n'
    '    request_runner_refresh(app);\n'
    '    request_activity_refresh(app);\n'
    '    request_local_refresh(app);\n',
    '    build_ui(app);\n'
    '    request_runner_refresh(app);\n'
    '    app->initial_local_timer =\n'
    '        g_timeout_add(250U, initial_local_refresh_cb, app);\n'
    '    app->initial_activity_timer =\n'
    '        g_timeout_add(750U, initial_activity_refresh_cb, app);\n',
)

# Ensure delayed startup work and an asynchronous restart cannot outlive app
# teardown.
replace_once(
    '    if (app->tick_timer) g_source_remove(app->tick_timer);\n'
    '    if (app->filter_timer) {\n',
    '    if (app->tick_timer) g_source_remove(app->tick_timer);\n'
    '    if (app->initial_activity_timer)\n'
    '        g_source_remove(app->initial_activity_timer);\n'
    '    if (app->initial_local_timer)\n'
    '        g_source_remove(app->initial_local_timer);\n'
    '    if (app->filter_timer) {\n',
)

replace_once(
    '    if (app->local_thread) {\n'
    '        g_thread_join(app->local_thread);\n'
    '        app->local_thread = NULL;\n'
    '    }\n'
    '    if (app->theme_provider) {\n',
    '    if (app->local_thread) {\n'
    '        g_thread_join(app->local_thread);\n'
    '        app->local_thread = NULL;\n'
    '    }\n'
    '    if (app->restart_thread) {\n'
    '        g_thread_join(app->restart_thread);\n'
    '        app->restart_thread = NULL;\n'
    '    }\n'
    '    if (app->theme_provider) {\n',
)

# Stale iterative-shell code and retired chrome must not survive this pass.
for marker in (
    "GtkListBox",
    "GtkRadioButton",
    "runner-sidebar",
    "runner-nav-list",
    "make_nav_row",
    "make_nav_content",
    "on_nav_row_selected",
    "runner-menubar",
    "build_menu_bar",
    "static GtkWidget *menu_item(",
    ".page-header",
    ".page-eyebrow",
    ".page-title",
    ".page-summary",
    "header-search",
):
    if marker in source:
        raise SystemExit(f"stale iterative UI marker remains: {marker}")

# Qualify that the current responsive rail really matches the suite constants.
for marker in (
    "RUNNERSCOPE_COMPACT_LAYOUT_THRESHOLD 1100",
    "RUNNERSCOPE_MAIN_NAV_WIDTH 195",
    "RUNNERSCOPE_MAIN_NAV_COMPACT_WIDTH 64",
    "RUNNERSCOPE_MAIN_NAV_BUTTON_WIDTH 179",
    "RUNNERSCOPE_MAIN_NAV_BUTTON_COMPACT_WIDTH 48",
    "RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT 50",
    "gtk_search_entry_new()",
    "initial_activity_refresh_cb",
    "restart_worker",
):
    if marker not in source:
        raise SystemExit(f"required forensic/UI marker missing: {marker}")

path.write_text(source)
Path("VERSION").write_text("1.2.31\n")

changelog_path = Path("CHANGELOG.md")
changelog = changelog_path.read_text()
old_unreleased = "## Unreleased\n\nNo unreleased changes.\n"
new_unreleased = '''## Unreleased

No unreleased changes.

## 1.2.31 - 2026-10-03

- Remove the redundant static page-header layer and legacy Help menubar so the Linux shell follows the current Infiltrator OS brand/header/navigation/workspace hierarchy.
- Move filtering into the active workspace and match System Monitor's responsive 195 px / 64 px navigation rail, 50 px navigation controls and 1100 px compact threshold.
- Reduce one-second GTK churn by updating only visible time-dependent cells, refreshing global utilisation every two seconds and avoiding repeated static Local Health metric scans.
- Defer the expensive activity/local startup scans until after first paint and stop eagerly rendering the hidden History model.
- Run privileged runner restarts off the GTK main thread so authentication and systemd latency no longer freeze the interface.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.
'''
if old_unreleased not in changelog:
    raise SystemExit("CHANGELOG Unreleased anchor missing")
changelog_path.write_text(changelog.replace(old_unreleased, new_unreleased, 1))

debian_path = Path("debian/changelog")
debian = debian_path.read_text()
debian_entry = '''infiltrator-runner-monitor (1.2.31) unstable; urgency=medium

  * Align the shell with current Infiltrator OS responsive navigation and move
    filtering from the title bar into the active workspace.
  * Remove redundant page-header and legacy Help-menu chrome.
  * Reduce periodic GTK work and stagger expensive startup provider scans.
  * Run runner service restarts outside the GTK main thread.
  * Keep the exact latest Infiltratr Common 1.19.38 pin.

 -- Shannon Smith <infiltratr@yandex.com>  Sat, 03 Oct 2026 11:30:00 +1000

'''
debian_path.write_text(debian_entry + debian)
