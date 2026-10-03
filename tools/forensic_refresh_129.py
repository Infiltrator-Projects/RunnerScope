#!/usr/bin/env python3
from pathlib import Path

SOURCE = Path("src/linux_main.c")
source = SOURCE.read_text()


def replace_span(text: str, start: str, end: str, replacement: str) -> str:
    first = text.find(start)
    if first < 0:
        raise SystemExit(f"missing start anchor: {start!r}")
    second = text.find(end, first + len(start))
    if second < 0:
        raise SystemExit(f"missing end anchor after {start!r}: {end!r}")
    return text[:first] + replacement + text[second:]


theme = r'''static void label_set_if_changed(GtkWidget *widget, const char *text)
{
    if (!widget) return;
    const char *next = text ? text : "";
    const char *current = gtk_label_get_text(GTK_LABEL(widget));
    if (g_strcmp0(current, next) != 0)
        gtk_label_set_text(GTK_LABEL(widget), next);
}

static void css_define_color(GString *css, const char *name, uint32_t rgb)
{
    g_string_append_printf(
        css, "@define-color %s #%06x;\n", name, rgb & 0x00ffffffU);
}

static void apply_theme(RunnerScopeApp *app)
{
    if (!app) return;

    const InfiltratrThemePalette *p =
        infiltratr_theme_resolve(app->config.theme_mode, system_dark_mode());
    const InfiltratrTypography *type = infiltratr_typography();
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    if (!p || !type || !metrics || !type->ui_family || !type->brand_family)
        return;

    GString *css = g_string_new(NULL);
    if (!css) return;

    css_define_color(css, "rm_background", p->background_rgb);
    css_define_color(css, "rm_panel", p->panel_rgb);
    css_define_color(css, "rm_card", p->card_rgb);
    css_define_color(css, "rm_surface", p->surface_rgb);
    css_define_color(css, "rm_input", p->input_rgb);
    css_define_color(css, "rm_border", p->border_rgb);
    css_define_color(css, "rm_text", p->text_rgb);
    css_define_color(css, "rm_title", p->title_rgb);
    css_define_color(css, "rm_muted", p->muted_rgb);
    css_define_color(css, "rm_subtle", p->subtle_rgb);
    css_define_color(css, "rm_button_background", p->button_background_rgb);
    css_define_color(css, "rm_button_foreground", p->button_foreground_rgb);
    css_define_color(css, "rm_selection", p->selection_background_rgb);
    css_define_color(css, "rm_selection_text", p->selection_foreground_rgb);
    css_define_color(css, "rm_neutral", p->neutral_accent_rgb);
    css_define_color(css, "rm_success", p->success_rgb);
    css_define_color(css, "rm_warning", p->warning_rgb);
    css_define_color(css, "rm_fault", p->fault_rgb);
    css_define_color(css, "rm_info", p->info_rgb);
    css_define_color(css, "rm_operation", p->operation_rgb);
    css_define_color(css, "rm_card_hover", p->card_hover_rgb);
    css_define_color(css, "rm_surface_hover", p->surface_hover_rgb);
    css_define_color(css, "rm_titlebar", p->titlebar_rgb);
    css_define_color(css, "rm_connection", p->connection_rgb);
    css_define_color(css, "rm_connection_border", p->connection_border_rgb);
    css_define_color(css, "rm_heading", p->heading_rgb);
    css_define_color(css, "rm_summary", p->summary_rgb);
    css_define_color(css, "rm_kicker", p->kicker_rgb);
    css_define_color(css, "rm_detail_label", p->detail_label_rgb);
    css_define_color(css, "rm_note", p->note_rgb);
    css_define_color(css, "rm_status_border", p->status_border_rgb);
    css_define_color(css, "rm_accent_foreground", p->accent_foreground_rgb);
    css_define_color(css, "rm_accent_hover", p->accent_hover_rgb);
    css_define_color(css, "rm_selected_summary", p->selected_summary_rgb);

    g_string_append_printf(
        css,
        "* { font-family:\"%s\"; font-weight:%u; }\n"
        ".header-brand-title, .page-title, .hero-title {"
        " font-family:\"%s\", \"%s\"; font-weight:%u; }\n"
        "button, treeview header button { font-weight:%u; }\n",
        type->ui_family,
        (unsigned int)type->ui_regular_weight,
        type->brand_family,
        type->ui_family,
        (unsigned int)type->brand_weight,
        (unsigned int)type->ui_bold_weight);

    g_string_append(
        css,
        "window, dialog, .background {"
        " background-color:@rm_background; color:@rm_text;"
        "}\n"
        "#runner-root {"
        " background-image:none; background-color:@rm_background;"
        "}\n"
        "label { color:@rm_text; }\n"
        "menu {"
        " background-color:@rm_panel; color:@rm_text;"
        " border:1px solid @rm_border;"
        "}\n"
        "menu menuitem { color:@rm_summary; }\n"
        "menu menuitem:hover {"
        " background-color:@rm_surface_hover; color:@rm_title;"
        "}\n"
        "button, combobox button {"
        " background-image:none; background-color:@rm_button_background;"
        " color:@rm_button_foreground; border:1px solid @rm_border;"
        " box-shadow:none;"
        "}\n"
        "button > label, button > image,"
        "combobox button > label, combobox button > image {"
        " color:@rm_button_foreground;"
        "}\n"
        "button:hover, combobox button:hover {"
        " background-color:@rm_neutral; border-color:@rm_neutral;"
        "}\n"
        "button:active, button:checked {"
        " background-color:@rm_selection; color:@rm_selection_text;"
        " border-color:@rm_neutral;"
        "}\n"
        "button:disabled {"
        " background-color:@rm_input; color:@rm_subtle;"
        " border-color:@rm_border;"
        "}\n"
        "button:disabled > label, button:disabled > image { color:@rm_subtle; }\n"
        "entry, spinbutton {"
        " background-image:none; background-color:@rm_input; color:@rm_text;"
        " border:1px solid @rm_connection_border; box-shadow:none;"
        "}\n"
        "entry:focus, spinbutton:focus { border-color:@rm_neutral; }\n"
        "tooltip {"
        " background-color:@rm_card; color:@rm_title;"
        " border:1px solid @rm_border;"
        "}\n");

    g_string_append_printf(
        css,
        "headerbar.runner-header {"
        " min-height:58px; padding:%upx %upx;"
        " background-image:none; background-color:@rm_titlebar;"
        " border-bottom:1px solid @rm_border;"
        "}\n"
        ".header-brand { padding:2px 4px; }\n"
        ".header-brand-icon {"
        " background-color:@rm_card; border:1px solid @rm_border;"
        " border-radius:%upx; padding:%upx; box-shadow:none;"
        "}\n"
        ".header-brand-icon image { color:@rm_neutral; }\n"
        ".header-brand-title { color:@rm_title; font-size:20px; }\n"
        ".header-brand-subtitle { color:@rm_muted; font-size:11px; }\n"
        ".header-search {"
        " min-width:300px; min-height:34px; background:@rm_input; color:@rm_text;"
        " border:1px solid @rm_connection_border; border-radius:%upx;"
        " padding:%upx %upx;"
        "}\n"
        ".runner-menubar, .runner-menubar menuitem {"
        " background:transparent; color:@rm_summary;"
        "}\n"
        ".runner-menubar menuitem { padding:5px 7px; border-radius:%upx; }\n"
        ".runner-menubar menuitem:hover {"
        " background:@rm_surface_hover; color:@rm_title;"
        "}\n"
        ".runner-window-controls { margin-left:%upx; }\n"
        ".runner-window-control {"
        " min-width:30px; min-height:30px; padding:4px;"
        " background-image:none; background-color:transparent;"
        " border:1px solid transparent; border-radius:%upx; box-shadow:none;"
        "}\n"
        ".runner-window-control:hover {"
        " background-color:@rm_surface_hover; border-color:@rm_border;"
        "}\n"
        ".runner-window-control-close:hover {"
        " background-color:@rm_fault; color:@rm_accent_foreground;"
        "}\n",
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_radius,
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->small_radius,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->small_radius);

    g_string_append_printf(
        css,
        ".page-header { padding:2px 2px 4px 2px; }\n"
        ".page-eyebrow { color:@rm_kicker; font-size:10px; font-weight:%u; }\n"
        ".page-title { color:@rm_heading; font-size:28px; }\n"
        ".page-summary { color:@rm_summary; font-size:12px; }\n"
        ".meta { color:@rm_detail_label; font-size:11px; }\n"
        ".hero-card {"
        " background-image:none; background-color:@rm_card;"
        " border:1px solid @rm_border; border-radius:%upx; padding:%upx;"
        "}\n"
        ".hero-icon-well {"
        " min-width:76px; min-height:76px; background:@rm_surface;"
        " border:1px solid @rm_connection_border; border-radius:%upx;"
        " padding:%upx;"
        "}\n"
        ".hero-kicker { color:@rm_kicker; font-size:10px; font-weight:%u; }\n"
        ".hero-title { color:@rm_heading; font-size:20px; }\n"
        "#summary { color:@rm_summary; font-size:11px; }\n"
        "#scan { color:@rm_note; font-size:11px; }\n"
        ".counter {"
        " background:@rm_surface; border:1px solid @rm_status_border;"
        " border-radius:%upx; padding:%upx %upx; min-height:44px;"
        " font-size:15px; font-weight:%u;"
        "}\n"
        ".counter:hover {"
        " background:@rm_surface_hover; border-color:@rm_neutral;"
        "}\n"
        ".counter-running { color:@rm_success; border-top:2px solid @rm_success; }\n"
        ".counter-idle { color:@rm_info; border-top:2px solid @rm_info; }\n"
        ".counter-offline { color:@rm_fault; border-top:2px solid @rm_fault; }\n"
        ".counter-local { color:@rm_success; border-top:2px solid @rm_success; }\n"
        ".counter-hosted { color:@rm_operation; border-top:2px solid @rm_operation; }\n"
        ".counter-queued { color:@rm_warning; border-top:2px solid @rm_warning; }\n",
        (unsigned int)type->ui_bold_weight,
        (unsigned int)metrics->panel_radius,
        (unsigned int)metrics->content_padding,
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->control_spacing,
        (unsigned int)type->ui_bold_weight,
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->content_padding,
        (unsigned int)type->ui_bold_weight);

    g_string_append_printf(
        css,
        ".runner-content-shell {"
        " background:@rm_card; border:1px solid @rm_border;"
        " border-radius:%upx;"
        "}\n"
        ".runner-sidebar {"
        " background-image:none; background-color:@rm_panel;"
        " border-right:1px solid @rm_connection_border;"
        " border-radius:%upx 0 0 %upx; padding:%upx;"
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
        (unsigned int)metrics->compact_spacing,
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

    g_string_append_printf(
        css,
        ".runner-nav-list { background:transparent; color:@rm_summary; border:0; }\n"
        ".runner-nav-list row {"
        " background-image:none; background-color:transparent;"
        " color:@rm_summary; border:1px solid transparent;"
        " border-radius:%upx; margin:2px 0; min-height:48px;"
        " padding:%upx %upx;"
        "}\n"
        ".runner-nav-list row:hover {"
        " background-color:@rm_surface_hover; border-color:@rm_border;"
        "}\n"
        ".runner-nav-list row:selected {"
        " background-image:none; background-color:@rm_selection;"
        " color:@rm_selection_text; border-color:@rm_neutral;"
        " box-shadow:none;"
        "}\n"
        ".runner-tab-icon {"
        " background:@rm_surface; border:1px solid @rm_border;"
        " border-radius:%upx; padding:5px;"
        "}\n"
        ".runner-tab-icon image { color:@rm_neutral; }\n"
        ".nav-primary { color:@rm_summary; font-size:14px; font-weight:%u; }\n"
        ".nav-secondary { color:@rm_detail_label; font-size:10px; }\n"
        ".runner-nav-list row:selected .nav-primary { color:@rm_selection_text; }\n"
        ".runner-nav-list row:selected .nav-secondary { color:@rm_selected_summary; }\n",
        (unsigned int)metrics->control_radius,
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->control_radius,
        (unsigned int)type->ui_bold_weight);

    g_string_append_printf(
        css,
        "scrolledwindow.runner-table { background:@rm_card; border:0; }\n"
        "treeview.view {"
        " background:@rm_input; color:@rm_text; border:0;"
        " -GtkTreeView-horizontal-separator:0;"
        " -GtkTreeView-vertical-separator:0;"
        "}\n"
        "treeview.view:selected {"
        " background:@rm_selection; color:@rm_selection_text;"
        "}\n"
        "treeview.view header button {"
        " background-image:none; background:@rm_panel; color:@rm_title;"
        " border:0; border-bottom:1px solid @rm_connection_border;"
        " min-height:36px; padding:0 5px; font-weight:%u;"
        "}\n"
        "treeview.view header button:hover { background:@rm_surface_hover; }\n"
        ".selection-card {"
        " background:@rm_connection; border-top:1px solid @rm_connection_border;"
        " padding:%upx %upx;"
        "}\n"
        ".selection-icon { color:@rm_neutral; padding:4px; }\n"
        ".selection-title { color:@rm_detail_label; font-size:9px; font-weight:%u; }\n"
        ".selection-primary { color:@rm_heading; font-size:13px; font-weight:%u; }\n"
        ".selection-secondary { color:@rm_summary; font-size:10px; }\n"
        "scrollbar, scrollbar trough { background-color:transparent; }\n"
        "scrollbar.vertical { min-width:10px; }\n"
        "scrollbar.horizontal { min-height:10px; }\n"
        "scrollbar.vertical slider {"
        " min-width:8px; min-height:28px; border-radius:999px; background:@rm_border;"
        "}\n"
        "scrollbar.horizontal slider {"
        " min-width:28px; min-height:8px; border-radius:999px; background:@rm_border;"
        "}\n"
        "scrollbar slider:hover { background:@rm_neutral; }\n",
        (unsigned int)type->ui_bold_weight,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->content_padding,
        (unsigned int)type->ui_bold_weight,
        (unsigned int)type->ui_bold_weight);

    g_string_append_printf(
        css,
        ".footer-card {"
        " background:@rm_card; border:1px solid @rm_border;"
        " border-radius:%upx; padding:%upx %upx;"
        "}\n"
        "#footer-actions button {"
        " border-radius:%upx; min-height:30px; padding:4px 10px;"
        "}\n"
        ".status-text { color:@rm_summary; font-size:11px; }\n"
        ".version-text { color:@rm_detail_label; font-size:10px; }\n",
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->control_radius);

    GdkScreen *screen = gdk_screen_get_default();
    if (!app->theme_provider) {
        app->theme_provider = gtk_css_provider_new();
        if (screen) {
            gtk_style_context_add_provider_for_screen(
                screen, GTK_STYLE_PROVIDER(app->theme_provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 50U);
        }
    }
    gtk_css_provider_load_from_data(app->theme_provider, css->str, -1, NULL);
    g_string_free(css, TRUE);
}

'''

source = replace_span(
    source,
    "static void rgb_text(uint32_t rgb, char out[8])\n{",
    "static char *run_command(char **argv, GError **error)\n{",
    theme + "static char *run_command(char **argv, GError **error)\n{",
)

helpers = r'''static gboolean same_text(const char *left, const char *right)
{
    return g_strcmp0(left, right) == 0;
}

static gboolean runner_rows_static_equal(const GPtrArray *left,
                                         const GPtrArray *right)
{
    if (!left || !right || left->len != right->len) return FALSE;
    for (guint i = 0U; i < left->len; i++) {
        const RunnerRow *a = g_ptr_array_index((GPtrArray *)left, i);
        const RunnerRow *b = g_ptr_array_index((GPtrArray *)right, i);
        if (!same_text(a->name, b->name) ||
            !same_text(a->os, b->os) ||
            !same_text(a->state, b->state) ||
            !same_text(a->repo, b->repo) ||
            !same_text(a->job, b->job) ||
            !same_text(a->jobs, b->jobs) ||
            !same_text(a->labels, b->labels))
            return FALSE;
    }
    return TRUE;
}

static gboolean activity_rows_static_equal(const GPtrArray *left,
                                           const GPtrArray *right)
{
    if (!left || !right || left->len != right->len) return FALSE;
    for (guint i = 0U; i < left->len; i++) {
        const ActivityRow *a = g_ptr_array_index((GPtrArray *)left, i);
        const ActivityRow *b = g_ptr_array_index((GPtrArray *)right, i);
        if (!same_text(a->environment, b->environment) ||
            !same_text(a->repo, b->repo) ||
            !same_text(a->workflow, b->workflow) ||
            !same_text(a->job, b->job) ||
            !same_text(a->step, b->step) ||
            !same_text(a->status, b->status) ||
            !same_text(a->runner, b->runner) ||
            !same_text(a->event, b->event) ||
            !same_text(a->branch, b->branch) ||
            !same_text(a->url, b->url) ||
            a->started_epoch != b->started_epoch)
            return FALSE;
    }
    return TRUE;
}

static gboolean local_rows_equal(const GPtrArray *left,
                                 const GPtrArray *right)
{
    if (!left || !right || left->len != right->len) return FALSE;
    for (guint i = 0U; i < left->len; i++) {
        const LocalRow *a = g_ptr_array_index((GPtrArray *)left, i);
        const LocalRow *b = g_ptr_array_index((GPtrArray *)right, i);
        if (!same_text(a->runner, b->runner) ||
            !same_text(a->service_state, b->service_state) ||
            !same_text(a->github_state, b->github_state) ||
            !same_text(a->pid, b->pid) ||
            !same_text(a->start_mode, b->start_mode) ||
            !same_text(a->account, b->account) ||
            !same_text(a->diag, b->diag) ||
            !same_text(a->diag_age, b->diag_age) ||
            !same_text(a->path, b->path) ||
            !same_text(a->diag_path, b->diag_path) ||
            !same_text(a->service_name, b->service_name))
            return FALSE;
    }
    return TRUE;
}

'''

runner_apply = r'''static gboolean runner_apply_idle(gpointer data)
{
    RunnerRefreshResult *result = data;
    RunnerScopeApp *app = result->app;
    if (app->runner_thread) {
        g_thread_unref(app->runner_thread);
        app->runner_thread = NULL;
    }
    if (result->error) {
        label_set_if_changed(app->status_label, result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->runner_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    GPtrArray *next_rows = g_ptr_array_new_with_free_func(runner_row_free);
    guint total = 0U, running = 0U, idle = 0U, offline = 0U;
    gboolean history_changed = FALSE;
    const double now = now_monotonic();

    for (guint i = 0U; i < result->rows->len; i++) {
        RawRunner *raw = g_ptr_array_index(result->rows, i);
        const char *state = g_ascii_strcasecmp(raw->status, "online") != 0
            ? "OFFLINE" : (raw->busy ? "RUNNING" : "IDLE");

        RunnerSession *session = g_hash_table_lookup(app->sessions, raw->name);
        if (!session) {
            session = g_new0(RunnerSession, 1U);
            g_strlcpy(session->state, state, sizeof(session->state));
            session->state_since = now;
            session->busy_started = strcmp(state, "RUNNING") == 0 ? now : 0.0;
            session->jobs = strcmp(state, "RUNNING") == 0 ? 1U : 0U;
            g_hash_table_insert(app->sessions, g_strdup(raw->name), session);
            add_history(app, raw->name, state, "");
            history_changed = TRUE;
        } else if (strcmp(session->state, state) != 0) {
            if (strcmp(session->state, "RUNNING") == 0 &&
                session->busy_started > 0.0) {
                session->busy_seconds += now - session->busy_started;
                session->busy_started = 0.0;
            }
            if (strcmp(state, "RUNNING") == 0) {
                session->busy_started = now;
                session->jobs++;
            }
            g_strlcpy(session->state, state, sizeof(session->state));
            session->state_since = now;
            JobSummary *summary =
                g_hash_table_lookup(app->job_by_runner, raw->name);
            add_history(app, raw->name, state,
                        summary && summary->job ? summary->job : "");
            history_changed = TRUE;
        }

        RunnerRow *row = g_new0(RunnerRow, 1U);
        row->name = g_strdup(raw->name);
        row->os = g_strdup(raw->os);
        row->state = g_strdup(state);
        JobSummary *summary = g_hash_table_lookup(app->job_by_runner, raw->name);
        if (strcmp(state, "RUNNING") == 0 && summary) {
            row->repo = g_strdup(summary->repo);
            row->job = g_strdup(summary->job);
            row->runtime = duration_text(summary->started_at > 0.0
                ? MAX(0.0, (double)time(NULL) - summary->started_at) : -1.0);
            g_free(session->last_repo);
            session->last_repo = g_strdup(summary->repo);
            g_free(session->last_job);
            session->last_job = g_strdup(summary->job);
        } else if (strcmp(state, "RUNNING") == 0) {
            row->repo = g_strdup("Resolving…");
            row->job = g_strdup("GitHub reports runner busy");
            row->runtime = duration_text(now - session->state_since);
        } else {
            row->repo = g_strdup(session->last_repo ? session->last_repo : "—");
            row->job = session->last_job
                ? g_strdup_printf("Last: %s", session->last_job)
                : g_strdup("—");
            row->runtime = g_strdup("—");
        }
        row->state_for = duration_text(now - session->state_since);
        row->jobs = g_strdup_printf("%u", session->jobs);
        double busy_seconds = session->busy_seconds;
        if (strcmp(state, "RUNNING") == 0 && session->busy_started > 0.0)
            busy_seconds += now - session->busy_started;
        const double elapsed = MAX(1.0, now - app->session_started);
        row->busy_pct = g_strdup_printf(
            "%.1f%%", MIN(100.0, busy_seconds * 100.0 / elapsed));
        row->labels = g_strdup(raw->labels && *raw->labels ? raw->labels : "—");
        g_ptr_array_add(next_rows, row);

        total++;
        if (strcmp(state, "RUNNING") == 0) running++;
        else if (strcmp(state, "IDLE") == 0) idle++;
        else offline++;
    }

    const gboolean rows_changed =
        !runner_rows_static_equal(app->runner_rows, next_rows);
    GPtrArray *old_rows = app->runner_rows;
    app->runner_rows = next_rows;
    g_ptr_array_unref(old_rows);

    app->runners_total = total;
    app->runners_running = running;
    app->runners_idle = idle;
    app->runners_offline = offline;

    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (page == 0 && rows_changed)
        render_runners(app);
    else if (page == 2 && history_changed)
        render_history(app);

    update_summary(app);
    char *clock = clock_text();
    char *updated = g_strdup_printf("Runner data: %s", clock);
    label_set_if_changed(app->updated_label, updated);
    g_free(updated);
    g_free(clock);

    char *status = app->config.expected_runners != 0U
        ? g_strdup_printf(
            "%u runners detected; expected %u. %u running, %u idle, %u offline.",
            app->runners_total, app->config.expected_runners,
            app->runners_running, app->runners_idle, app->runners_offline)
        : g_strdup_printf(
            "%u runners detected. %u running, %u idle, %u offline.",
            app->runners_total, app->runners_running,
            app->runners_idle, app->runners_offline);
    label_set_if_changed(app->status_label, status);
    g_free(status);

    g_ptr_array_unref(result->rows);
    g_atomic_int_set(&app->runner_refreshing, 0);
    g_free(result);
    return G_SOURCE_REMOVE;
}

'''

source = replace_span(
    source,
    "static gboolean runner_apply_idle(gpointer data)\n{",
    "static gpointer runner_worker(gpointer data)\n{",
    helpers + runner_apply + "static gpointer runner_worker(gpointer data)\n{",
)

activity_apply = r'''static gboolean activity_apply_idle(gpointer data)
{
    ActivityRefreshResult *result = data;
    RunnerScopeApp *app = result->app;
    if (app->activity_thread) {
        g_thread_unref(app->activity_thread);
        app->activity_thread = NULL;
    }
    if (result->error) {
        label_set_if_changed(app->status_label, result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->activity_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    const gboolean rows_changed =
        !activity_rows_static_equal(app->activity_rows, result->rows);

    g_hash_table_remove_all(app->job_by_runner);
    for (guint i = 0U; i < result->rows->len; i++) {
        ActivityRow *row = g_ptr_array_index(result->rows, i);
        if (strcmp(row->status, "IN_PROGRESS") == 0 &&
            row->runner && strcmp(row->runner, "—") != 0) {
            JobSummary *summary = g_new0(JobSummary, 1U);
            summary->repo = g_strdup(row->repo);
            summary->job = g_strdup_printf("%s › %s", row->workflow, row->job);
            summary->started_at = row->started_epoch;
            g_hash_table_replace(
                app->job_by_runner, g_strdup(row->runner), summary);
        }
    }

    GPtrArray *old_rows = app->activity_rows;
    app->activity_rows = result->rows;
    result->rows = NULL;
    g_ptr_array_unref(old_rows);

    app->local_active = result->local_active;
    app->hosted_active = result->hosted_active;
    app->queued = result->queued;
    if (rows_changed &&
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) == 1)
        render_activity(app);

    update_summary(app);
    char *scan = g_strdup_printf(
        "Runner poll %us  •  Activity scan %us  •  %u repositories scanned",
        app->config.runner_poll_seconds, app->config.activity_scan_seconds,
        result->repos_scanned);
    label_set_if_changed(app->scan_label, scan);
    g_free(scan);

    g_atomic_int_set(&app->activity_refreshing, 0);
    g_free(result);
    request_runner_refresh(app);
    return G_SOURCE_REMOVE;
}

'''

source = replace_span(
    source,
    "static gboolean activity_apply_idle(gpointer data)\n{",
    "static void request_activity_refresh(RunnerScopeApp *app)\n{",
    activity_apply + "static void request_activity_refresh(RunnerScopeApp *app)\n{",
)

local_apply = r'''static gboolean local_apply_idle(gpointer data)
{
    LocalRefreshResult *result = data;
    RunnerScopeApp *app = result->app;
    if (app->local_thread) {
        g_thread_unref(app->local_thread);
        app->local_thread = NULL;
    }
    if (result->error) {
        label_set_if_changed(app->status_label, result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->local_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    for (guint i = 0U; i < result->rows->len; i++) {
        LocalRow *row = g_ptr_array_index(result->rows, i);
        char *service_lower = g_ascii_strdown(row->service_name, -1);
        char *description_lower = g_ascii_strdown(row->runner, -1);
        for (guint runner_index = 0U;
             runner_index < app->runner_rows->len;
             runner_index++) {
            RunnerRow *runner =
                g_ptr_array_index(app->runner_rows, runner_index);
            char *runner_lower = g_ascii_strdown(runner->name, -1);
            const gboolean matches =
                strstr(service_lower, runner_lower) != NULL ||
                strstr(description_lower, runner_lower) != NULL;
            g_free(runner_lower);
            if (matches) {
                g_free(row->runner);
                row->runner = g_strdup(runner->name);
                g_free(row->github_state);
                row->github_state = g_strdup(runner->state);
                break;
            }
        }
        g_free(service_lower);
        g_free(description_lower);
    }

    const gboolean rows_changed =
        !local_rows_equal(app->local_rows, result->rows);
    GPtrArray *old_rows = app->local_rows;
    app->local_rows = result->rows;
    result->rows = NULL;
    g_ptr_array_unref(old_rows);

    if (rows_changed &&
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) == 3)
        render_local(app);

    g_atomic_int_set(&app->local_refreshing, 0);
    g_free(result);
    return G_SOURCE_REMOVE;
}

'''

source = replace_span(
    source,
    "static gboolean local_apply_idle(gpointer data)\n{",
    "static void request_local_refresh(RunnerScopeApp *app)\n{",
    local_apply + "static void request_local_refresh(RunnerScopeApp *app)\n{",
)

if source.count("make_tab_label") != 2:
    raise SystemExit(
        f"expected two make_tab_label references, found {source.count('make_tab_label')}")
source = source.replace("make_tab_label", "make_nav_content")

SOURCE.write_text(source)
Path("VERSION").write_text("1.2.29\n")

changelog = Path("CHANGELOG.md")
text = changelog.read_text()
anchor = "## Unreleased\n\nNo unreleased changes.\n"
if text.count(anchor) != 1:
    raise SystemExit("CHANGELOG Unreleased anchor mismatch")
release_notes = '''## Unreleased

No unreleased changes.

## 1.2.29 - 2026-10-03

- Remove the superseded first-pass GTK CSS layer so the Linux shell has one authoritative Common-driven style projection instead of stacked historical overrides.
- Match the current Infiltrator OS shell grammar with flat semantic surfaces, restrained selection, Common typography roles and no decorative shell gradients.
- Stop rebuilding visible runner, activity and local-service models when a refresh produces the same static data.
- Transfer worker-owned activity/local rows into the UI state instead of deep-copying every record after each scan.
- Release completed worker references without waiting on the GTK main thread.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

'''
changelog.write_text(text.replace(anchor, release_notes, 1))

debian = Path("debian/changelog")
text = debian.read_text()
stanza = '''infiltrator-runner-monitor (1.2.29) unstable; urgency=medium

  * Remove the superseded duplicate GTK CSS projection and use one flat,
    Common-driven Infiltrator OS shell.
  * Avoid rebuilding visible GTK models when provider refresh data is unchanged.
  * Transfer worker-owned activity and local-service rows without redundant
    deep copies and avoid waiting for completed workers on the GTK main thread.
  * Keep the exact latest Infiltratr Common 1.19.38 pin.

 -- Shannon Smith <infiltratr@yandex.com>  Sat, 03 Oct 2026 11:35:00 +1000

'''
debian.write_text(stanza + text)

design = Path("docs/DESIGN.md")
text = design.read_text()
old = '''Repeated telemetry updates update only visible/time-dependent cells. Theme
changes reuse one CSS provider; provider and service work stays off the GTK
main thread.
'''
new = '''Repeated telemetry updates update only visible/time-dependent cells and do not
rebuild a visible model when its static provider snapshot is unchanged. Theme
changes reuse one CSS provider and one Common-driven CSS projection; provider
and service work stays off the GTK main thread.
'''
if text.count(old) != 1:
    raise SystemExit("DESIGN performance paragraph mismatch")
design.write_text(text.replace(old, new, 1))
