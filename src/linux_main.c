// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/escape.h>
#include <infiltratr/format.h>
#include <infiltratr/posix.h>

#include "ui/ui_contract.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef RUNNERSCOPE_VERSION
#error "RUNNERSCOPE_VERSION must be provided by CMake"
#endif
#define RUNNERSCOPE_APP_ID "net.ssmith.runnerscope"
#define RUNNERSCOPE_COMPACT_LAYOUT_THRESHOLD 1100
#define RUNNERSCOPE_COMPACT_LAYOUT_HYSTERESIS 64
#define RUNNERSCOPE_MAIN_NAV_WIDTH 195
#define RUNNERSCOPE_MAIN_NAV_COMPACT_WIDTH 64
#define RUNNERSCOPE_MAIN_NAV_BUTTON_WIDTH 179
#define RUNNERSCOPE_MAIN_NAV_BUTTON_COMPACT_WIDTH 48
#define RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT 50

typedef struct {
    char *name;
    char *os;
    char *status;
    gboolean busy;
    char *labels;
} RawRunner;

typedef struct {
    char *name;
    char *os;
    char *state;
    char *repo;
    char *job;
    char *runtime;
    char *state_for;
    char *jobs;
    char *busy_pct;
    char *labels;
} RunnerRow;

typedef struct {
    char *environment;
    char *repo;
    char *workflow;
    char *job;
    char *step;
    char *status;
    char *runner;
    char *runtime;
    char *event;
    char *branch;
    char *url;
    double started_epoch;
} ActivityRow;

typedef struct {
    char *time_text;
    char *runner;
    char *event;
    char *detail;
} HistoryRow;

typedef struct {
    char *runner;
    char *service_state;
    char *github_state;
    char *pid;
    char *start_mode;
    char *account;
    char *diag;
    char *diag_age;
    char *path;
    char *diag_path;
    char *service_name;
} LocalRow;

typedef struct {
    char state[16];
    double state_since;
    double busy_started;
    double busy_seconds;
    guint jobs;
    char *last_repo;
    char *last_job;
} RunnerSession;

typedef struct {
    char *repo;
    char *job;
    double started_at;
} JobSummary;

typedef struct {
    char organisation[128];
    guint expected_runners;
    guint runner_poll_seconds;
    guint activity_scan_seconds;
    guint repository_scan_limit;
    guint local_health_seconds;
    InfiltratrThemeMode theme_mode;
} RunnerConfig;

enum {
    RUNNER_COL_NAME,
    RUNNER_COL_OS,
    RUNNER_COL_STATE,
    RUNNER_COL_REPO,
    RUNNER_COL_JOB,
    RUNNER_COL_RUNTIME,
    RUNNER_COL_STATE_FOR,
    RUNNER_COL_JOBS,
    RUNNER_COL_BUSY,
    RUNNER_COL_LABELS,
    RUNNER_N_COLS
};

enum {
    ACT_COL_ENV,
    ACT_COL_REPO,
    ACT_COL_WORKFLOW,
    ACT_COL_JOB,
    ACT_COL_STEP,
    ACT_COL_STATUS,
    ACT_COL_RUNNER,
    ACT_COL_RUNTIME,
    ACT_COL_EVENT,
    ACT_COL_BRANCH,
    ACT_COL_URL,
    ACT_N_COLS
};

enum {
    HIST_COL_TIME,
    HIST_COL_RUNNER,
    HIST_COL_EVENT,
    HIST_COL_DETAIL,
    HIST_N_COLS
};

enum {
    LOCAL_COL_RUNNER,
    LOCAL_COL_SERVICE,
    LOCAL_COL_GITHUB,
    LOCAL_COL_PID,
    LOCAL_COL_START,
    LOCAL_COL_ACCOUNT,
    LOCAL_COL_DIAG,
    LOCAL_COL_DIAG_AGE,
    LOCAL_COL_PATH,
    LOCAL_COL_DIAG_PATH,
    LOCAL_COL_SERVICE_NAME,
    LOCAL_N_COLS
};

typedef struct {
    GtkApplication *application;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *filter_entry;
    GtkWidget *status_label;
    GtkWidget *open_job_button;
    GtkWidget *open_diag_button;
    GtkWidget *restart_button;
    GtkWidget *workspace_title;
    GtkWidget *workspace_subtitle;
    GtkWidget *workspace_icon;
    GtkWidget *workspace_art;
    GtkWidget *workspace_art_frame;
    GtkWidget *workspace_metric_caption[4];
    GtkWidget *workspace_metric_value[4];
    GtkWidget *selection_card;
    GtkWidget *selection_title;
    GtkWidget *selection_primary;
    GtkWidget *selection_secondary;
    GtkWidget *nav_buttons[4];
    GtkWidget *main_navigation;
    GtkWidget *navigation_art_frame;
    gboolean navigation_syncing;
    gboolean compact_layout;
    gboolean theme_state_valid;
    InfiltratrThemeMode theme_state_mode;
    gboolean theme_state_dark;
    GtkCssProvider *theme_provider;

    GtkListStore *runner_store;
    GtkListStore *activity_store;
    GtkListStore *history_store;
    GtkListStore *local_store;
    GtkWidget *runner_tree;
    GtkWidget *runner_views;
    GtkWidget *runner_cards;
    GtkWidget *runner_cards_empty;
    GtkWidget *runner_view_switch;
    GHashTable *runner_card_by_name;
    GdkPixbuf *runner_card_art;
    char *selected_runner_name;
    gboolean runner_rendering;
    GtkWidget *activity_tree;
    GtkWidget *history_tree;
    GtkWidget *local_tree;

    RunnerConfig config;
    GHashTable *sessions;
    GHashTable *job_by_runner;
    GHashTable *runner_row_by_name;
    GHashTable *activity_row_by_url;
    GPtrArray *runner_rows;
    GPtrArray *activity_rows;
    GPtrArray *history_rows;
    GPtrArray *local_rows;

    double session_started;
    gint runner_refreshing;
    gint activity_refreshing;
    gint local_refreshing;
    guint runner_timer;
    guint activity_timer;
    guint local_timer;
    guint tick_timer;
    guint filter_timer;
    guint history_save_timer;
    guint initial_activity_timer;
    GThread *runner_thread;
    GThread *activity_thread;
    GThread *local_thread;
    GThread *restart_thread;
    gint restart_refreshing;
    gint shutting_down;
    guint runners_total;
    guint runners_running;
    guint runners_idle;
    guint runners_offline;
    guint local_active;
    guint hosted_active;
    guint queued;
} RunnerScopeApp;

typedef struct {
    RunnerScopeApp *app;
    GPtrArray *rows;
    char *error;
} RunnerRefreshResult;

typedef struct {
    RunnerScopeApp *app;
    GPtrArray *rows;
    guint local_active;
    guint hosted_active;
    guint queued;
    char *error;
} ActivityRefreshResult;

typedef struct {
    RunnerScopeApp *app;
    GPtrArray *rows;
    char *error;
} LocalRefreshResult;

typedef struct {
    RunnerScopeApp *app;
    char *service;
    char *error;
} RestartResult;

static gboolean activity_apply_idle(gpointer data);
static gboolean local_apply_idle(gpointer data);
static void request_activity_refresh(RunnerScopeApp *app);
static void on_export(GtkButton *button, gpointer user_data);
static InfiltratrProjectInfo project_info(void);

static void raw_runner_free(gpointer data)
{
    RawRunner *row = data;
    if (!row) return;
    g_free(row->name);
    g_free(row->os);
    g_free(row->status);
    g_free(row->labels);
    g_free(row);
}

static void runner_row_free(gpointer data)
{
    RunnerRow *row = data;
    if (!row) return;
    g_free(row->name); g_free(row->os); g_free(row->state); g_free(row->repo);
    g_free(row->job); g_free(row->runtime); g_free(row->state_for);
    g_free(row->jobs); g_free(row->busy_pct); g_free(row->labels); g_free(row);
}

static void activity_row_free(gpointer data)
{
    ActivityRow *row = data;
    if (!row) return;
    g_free(row->environment); g_free(row->repo); g_free(row->workflow);
    g_free(row->job); g_free(row->step); g_free(row->status); g_free(row->runner);
    g_free(row->runtime); g_free(row->event); g_free(row->branch); g_free(row->url);
    g_free(row);
}

static void history_row_free(gpointer data)
{
    HistoryRow *row = data;
    if (!row) return;
    g_free(row->time_text); g_free(row->runner); g_free(row->event);
    g_free(row->detail); g_free(row);
}

static void local_row_free(gpointer data)
{
    LocalRow *row = data;
    if (!row) return;
    g_free(row->runner); g_free(row->service_state); g_free(row->github_state);
    g_free(row->pid); g_free(row->start_mode); g_free(row->account); g_free(row->diag);
    g_free(row->diag_age); g_free(row->path); g_free(row->diag_path);
    g_free(row->service_name); g_free(row);
}

static void session_free(gpointer data)
{
    RunnerSession *session = data;
    if (!session) return;
    g_free(session->last_repo);
    g_free(session->last_job);
    g_free(session);
}

static void job_summary_free(gpointer data)
{
    JobSummary *summary = data;
    if (!summary) return;
    g_free(summary->repo);
    g_free(summary->job);
    g_free(summary);
}

static double now_monotonic(void)
{
    const double value = infiltratr_monotonic_seconds();
    return value >= 0.0 ? value : (double)g_get_monotonic_time() / 1000000.0;
}

static char *duration_text(double seconds)
{
    char buffer[64];
    if (seconds < 0.0) return g_strdup("—");
    if (!infiltratr_format_duration_compact(true, (uint64_t)seconds,
                                             buffer, sizeof(buffer)))
        return g_strdup("—");
    return g_strdup(buffer);
}

static char *history_time_text(void)
{
    time_t now = time(NULL);
    struct tm local_tm;
    char buffer[32] = "—";
    if (localtime_r(&now, &local_tm))
        (void)strftime(buffer, sizeof(buffer), "%d/%m %H:%M:%S", &local_tm);
    return g_strdup(buffer);
}

static char *config_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "runnerscope", "config.json", NULL);
}

static char *state_path(void)
{
    return g_build_filename(g_get_user_state_dir(), "runnerscope", "history.tsv", NULL);
}

static const char *json_value_start(const char *text, const char *key)
{
    if (!text || !key) return NULL;
    char *needle = g_strdup_printf("\"%s\"", key);
    const char *found = strstr(text, needle);
    g_free(needle);
    if (!found) return NULL;
    found = strchr(found, ':');
    if (!found) return NULL;
    found++;
    while (*found && g_ascii_isspace(*found)) found++;
    return found;
}

static gboolean json_get_string(const char *text, const char *key,
                                char *out, size_t out_size)
{
    const char *value = json_value_start(text, key);
    if (!value || *value != '"' || out_size == 0U) return FALSE;
    value++;
    size_t used = 0U;
    while (*value && *value != '"' && used + 1U < out_size) {
        if (*value == '\\' && value[1] != '\0') {
            value++;
            if (*value == 'n') out[used++] = '\n';
            else if (*value == 'r') out[used++] = '\r';
            else if (*value == 't') out[used++] = '\t';
            else out[used++] = *value;
            value++;
            continue;
        }
        out[used++] = *value++;
    }
    out[used] = '\0';
    return *value == '"';
}

static gboolean json_get_uint(const char *text, const char *key, guint *out)
{
    const char *value = json_value_start(text, key);
    if (!value || !out) return FALSE;
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || parsed > G_MAXUINT) return FALSE;
    *out = (guint)parsed;
    return TRUE;
}

static void config_defaults(RunnerConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->runner_poll_seconds = 3U;
    config->activity_scan_seconds = 45U;
    config->repository_scan_limit = 25U;
    config->local_health_seconds = 15U;
    config->theme_mode = INFILTRATR_THEME_SYSTEM;
}

static void load_config(RunnerConfig *config)
{
    config_defaults(config);
    char *path = config_path();
    gchar *contents = NULL;
    gsize length = 0U;
    if (g_file_get_contents(path, &contents, &length, NULL) && length != 0U) {
        (void)json_get_string(contents, "organisation",
                              config->organisation, sizeof(config->organisation));
        (void)json_get_uint(contents, "expected_runners", &config->expected_runners);
        (void)json_get_uint(contents, "runner_poll_seconds", &config->runner_poll_seconds);
        (void)json_get_uint(contents, "activity_scan_seconds", &config->activity_scan_seconds);
        (void)json_get_uint(contents, "repository_scan_limit", &config->repository_scan_limit);
        (void)json_get_uint(contents, "local_health_seconds", &config->local_health_seconds);
        char theme[32] = "";
        if (json_get_string(contents, "theme_mode", theme, sizeof(theme))) {
            InfiltratrThemeMode parsed_mode = config->theme_mode;
            if (infiltratr_theme_mode_parse(theme, &parsed_mode))
                config->theme_mode = parsed_mode;
        }
    }
    g_free(contents);
    g_free(path);

    const char *env_org = g_getenv("GITHUB_RUNNER_ORG");
    if (env_org && *env_org)
        g_strlcpy(config->organisation, env_org, sizeof(config->organisation));

    if (config->runner_poll_seconds < 3U) config->runner_poll_seconds = 3U;
    if (config->activity_scan_seconds < 10U) config->activity_scan_seconds = 10U;
    if (config->repository_scan_limit < 1U) config->repository_scan_limit = 1U;
    if (config->local_health_seconds < 15U) config->local_health_seconds = 15U;
}

static gboolean save_config(const RunnerConfig *config, GError **error)
{
    if (!config) return FALSE;
    size_t escaped_size = 0U;
    (void)infiltratr_escape_json(config->organisation, NULL, 0U, &escaped_size);
    char *escaped = g_malloc0(escaped_size + 1U);
    if (!infiltratr_escape_json(config->organisation, escaped,
                                escaped_size + 1U, NULL)) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                            "Unable to escape organisation name");
        g_free(escaped);
        return FALSE;
    }

    const char *mode = infiltratr_theme_mode_key(config->theme_mode);
    char *json = g_strdup_printf(
        "{\n"
        "  \"organisation\": \"%s\",\n"
        "  \"expected_runners\": %u,\n"
        "  \"runner_poll_seconds\": %u,\n"
        "  \"activity_scan_seconds\": %u,\n"
        "  \"repository_scan_limit\": %u,\n"
        "  \"local_health_seconds\": %u,\n"
        "  \"theme_mode\": \"%s\"\n"
        "}\n",
        escaped, config->expected_runners, config->runner_poll_seconds,
        config->activity_scan_seconds, config->repository_scan_limit,
        config->local_health_seconds, mode ? mode : "system");
    g_free(escaped);

    char *path = config_path();
    char *directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Unable to create %s: %s", directory, g_strerror(errno));
        g_free(directory); g_free(path); g_free(json);
        return FALSE;
    }
    g_free(directory);

    const InfiltratrAtomicFileMode file_mode =
        access(path, F_OK) == 0
            ? INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS
            : INFILTRATR_ATOMIC_FILE_PRIVATE;
    const int failure = infiltratr_atomic_file_write_bytes(
        path, file_mode, json, strlen(json));
    g_free(json);
    if (failure != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
                    "Unable to save %s: %s", path, g_strerror(failure));
        g_free(path);
        return FALSE;
    }
    g_free(path);
    return TRUE;
}

static gboolean system_dark_mode(void)
{
    GtkSettings *settings = gtk_settings_get_default();
    gboolean dark = FALSE;
    char *theme_name = NULL;
    if (settings)
        g_object_get(settings,
                     "gtk-application-prefer-dark-theme", &dark,
                     "gtk-theme-name", &theme_name,
                     NULL);
    if (!dark && theme_name) {
        char *lower = g_ascii_strdown(theme_name, -1);
        dark = strstr(lower, "dark") != NULL;
        g_free(lower);
    }
    g_free(theme_name);
    return dark;
}

static void label_set_if_changed(GtkWidget *widget, const char *text)
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

static char *runner_asset_path(const char *name)
{
    char *installed = g_build_filename(RUNNERSCOPE_DATA_DIR, "art", name, NULL);
    if (g_file_test(installed, G_FILE_TEST_IS_REGULAR))
        return installed;
    g_free(installed);
    char *cwd = g_get_current_dir();
    char *local = g_build_filename(cwd, "assets", "art", name, NULL);
    g_free(cwd);
    return local;
}

static GdkPixbuf *runner_asset_pixbuf(const char *name, gint width, gint height)
{
    char *path = runner_asset_path(name);
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
        path, width, height, TRUE, &error);
    if (!pixbuf) {
        g_warning("Runner Monitor artwork %s could not be loaded: %s",
                  path, error ? error->message : "unknown error");
        g_clear_error(&error);
    }
    g_free(path);
    return pixbuf;
}

static GtkWidget *runner_asset_image_new(const char *name, gint width, gint height)
{
    GdkPixbuf *pixbuf = runner_asset_pixbuf(name, width, height);
    if (!pixbuf)
        return gtk_image_new_from_icon_name("image-missing-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    g_object_set_data_full(G_OBJECT(image), "runner-artwork-key",
                          g_strdup_printf("%s:%d:%d", name, width, height), g_free);
    return image;
}

static void runner_asset_image_set(GtkWidget *image, const char *name,
                                   gint width, gint height)
{
    if (!image) return;
    char *key = g_strdup_printf("%s:%d:%d", name, width, height);
    if (g_strcmp0(g_object_get_data(G_OBJECT(image), "runner-artwork-key"), key) == 0) {
        g_free(key);
        return;
    }
    GdkPixbuf *pixbuf = runner_asset_pixbuf(name, width, height);
    if (!pixbuf) {
        g_free(key);
        return;
    }
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
    g_object_unref(pixbuf);
    g_object_set_data_full(G_OBJECT(image), "runner-artwork-key", key, g_free);
}

static gboolean pango_context_has_family(PangoContext *context, const char *wanted)
{
    if (!context || !wanted || !*wanted) return FALSE;
    PangoFontFamily **families = NULL;
    int count = 0;
    pango_context_list_families(context, &families, &count);
    gboolean found = FALSE;
    for (int i = 0; i < count; i++) {
        const char *name = pango_font_family_get_name(families[i]);
        if (name && g_ascii_strcasecmp(name, wanted) == 0) {
            found = TRUE;
            break;
        }
    }
    g_free(families);
    return found;
}

static void enforce_required_typography(GtkWidget *widget)
{
    const InfiltratrTypography *type = infiltratr_typography();
    PangoContext *context = widget ? gtk_widget_get_pango_context(widget) : NULL;
    if (!type || !context ||
        !pango_context_has_family(context, type->ui_family) ||
        !pango_context_has_family(context, type->brand_family)) {
        g_error("Runner Monitor requires the Common-verified MB Corpo UI and brand faces");
    }
}

static void apply_theme(RunnerScopeApp *app)
{
    if (!app) return;

    const gboolean system_dark = system_dark_mode();
    const gboolean night_theme =
        app->config.theme_mode == INFILTRATR_THEME_NIGHT ||
        (app->config.theme_mode == INFILTRATR_THEME_SYSTEM && system_dark);
    if (app->theme_state_valid &&
        app->theme_state_mode == app->config.theme_mode &&
        app->theme_state_dark == night_theme)
        return;

    const InfiltratrThemePalette *p =
        infiltratr_theme_resolve(app->config.theme_mode, system_dark);
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
        ".header-brand-title, .workspace-title, .workspace-metric-value {"
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
        " background-color:@rm_background;"
        " background-image:linear-gradient(145deg, alpha(@rm_neutral, 0.055),"
        " alpha(@rm_operation, 0.018) 42%, transparent 78%);"
        "}\n"
        "label { color:@rm_text; }\n"
        "button, combobox button {"
        " background-color:@rm_button_background;"
        " background-image:linear-gradient(to bottom, alpha(@rm_neutral, 0.16), alpha(@rm_button_background, 0.94));"
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
        " background-color:@rm_titlebar;"
        " background-image:linear-gradient(110deg, alpha(@rm_neutral, 0.24),"
        " @rm_titlebar 42%%, alpha(@rm_operation, 0.18));"
        " border-bottom:1px solid alpha(@rm_neutral, 0.52);"
        " box-shadow:0 3px 14px alpha(#000000, 0.28);"
        "}\n"
        ".header-brand { padding:2px 4px; }\n"
        ".header-brand-icon {"
        " background-color:@rm_card;"
        " background-image:linear-gradient(135deg, alpha(@rm_neutral, 0.28),"
        " alpha(@rm_operation, 0.12));"
        " border:1px solid alpha(@rm_neutral, 0.62);"
        " border-radius:%upx; padding:%upx;"
        " box-shadow:0 2px 9px alpha(@rm_neutral, 0.16);"
        "}\n"
        ".header-brand-icon image { color:@rm_neutral; }\n"
        ".header-brand-title { color:@rm_title; font-size:20px; }\n"
        ".header-brand-subtitle { color:@rm_muted; font-size:11px; }\n"
        ".workspace-search {"
        " min-width:220px; min-height:34px; background:@rm_input; color:@rm_text;"
        " border:1px solid @rm_connection_border; border-radius:%upx;"
        " padding:%upx %upx;"
        "}\n"
        ".runner-window-controls { margin-left:%upx; }\n"
        ".runner-window-control {"
        " min-width:30px; min-height:30px; padding:4px;"
        " background-color:alpha(@rm_surface, 0.72);"
        " background-image:linear-gradient(to bottom, alpha(@rm_neutral, 0.08),"
        " alpha(@rm_surface, 0.42));"
        " border:1px solid alpha(@rm_connection_border, 0.72);"
        " border-radius:%upx; box-shadow:0 1px 4px alpha(#000000, 0.18);"
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
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->small_radius);

    g_string_append_printf(
        css,
        ".runner-content-shell {"
        " background:@rm_card; border:1px solid @rm_connection_border;"
        " border-radius:%upx;"
        " box-shadow:0 5px 18px alpha(#000000, 0.22);"
        "}\n"
        ".runner-workspace {"
        " background:@rm_card; border-radius:0 %upx %upx 0;"
        "}\n"
        ".workspace-header {"
        " min-height:108px; background-color:@rm_connection;"
        " background-image:linear-gradient(105deg, alpha(@rm_neutral, 0.30),"
        " @rm_connection 44%%, alpha(@rm_operation, 0.20));"
        " border-bottom:1px solid alpha(@rm_neutral, 0.58);"
        " padding:%upx %upx; box-shadow:0 5px 18px alpha(#000000, 0.30);"
        "}\n"
        ".workspace-icon-well {"
        " min-width:58px; min-height:58px;"
        " background-color:@rm_surface;"
        " background-image:linear-gradient(135deg, alpha(@rm_neutral, 0.30),"
        " alpha(@rm_operation, 0.12));"
        " border:1px solid alpha(@rm_neutral, 0.56); border-radius:16px;"
        " box-shadow:0 3px 10px alpha(@rm_neutral, 0.14); padding:7px;"
        "}\n"
        ".workspace-icon { color:@rm_accent_foreground; }\n"
        ".workspace-title { color:@rm_heading; font-size:26px; font-weight:%u; }\n"
        ".workspace-subtitle { color:@rm_summary; font-size:10px; letter-spacing:0.4px; }\n"
        ".workspace-art-frame { min-width:200px; min-height:84px; background:@rm_input; border:1px solid alpha(@rm_neutral, 0.52); border-radius:14px; padding:3px; box-shadow:0 4px 16px alpha(#000000, 0.28); }\n"
        ".workspace-toolbar { background:@rm_panel; border-bottom:1px solid @rm_connection_border; padding:6px 10px; }\n"
        ".nav-art-frame { background-image:linear-gradient(to bottom, transparent, alpha(@rm_neutral, 0.07)); border-top:1px solid alpha(@rm_connection_border, 0.65); padding:8px; }\n"
        ".workspace-metric {"
        " min-width:64px; min-height:58px; background-color:@rm_surface;"
        " background-image:linear-gradient(145deg, alpha(@rm_neutral, 0.08),"
        " alpha(@rm_card, 0.82));"
        " border:1px solid @rm_connection_border; border-top:2px solid @rm_neutral;"
        " border-radius:%upx; padding:%upx %upx;"
        " box-shadow:0 2px 8px alpha(#000000, 0.16);"
        "}\n"
        ".workspace-metric-caption {"
        " color:@rm_detail_label; font-size:9px; font-weight:%u;"
        "}\n"
        ".workspace-metric-value {"
        " color:@rm_heading; font-size:20px; font-weight:%u;"
        "}\n"
        "notebook.runner-notebook { background:@rm_card; border:0; }\n"
        ".workspace-metric.metric-one { border-top-color:@rm_info; }\n"
        ".workspace-metric.metric-two { border-top-color:@rm_success; }\n"
        ".workspace-metric.metric-three { border-top-color:@rm_warning; }\n"
        ".workspace-metric.metric-four { border-top-color:@rm_operation; }\n",
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
        " background-color:@rm_panel;"
        " background-image:linear-gradient(to bottom, alpha(@rm_neutral, 0.055),"
        " transparent 38%, alpha(@rm_operation, 0.025));"
        " border-color:@rm_connection_border;"
        "}\n"
        "#runner-main-navigation { border-right:1px solid @rm_border; }\n"
        "#runner-main-nav-button {"
        " background-image:none; background-color:transparent;"
        " color:@rm_summary; border:1px solid transparent;"
        " border-radius:12px; box-shadow:none;"
        " margin:2px 4px; padding:7px 9px;"
        "}\n"
        "#runner-main-nav-button:hover {"
        " background-color:@rm_surface_hover; border-color:@rm_border;"
        "}\n"
        "#runner-main-nav-button:checked {"
        " background-color:@rm_selection;"
        " background-image:linear-gradient(100deg, alpha(@rm_neutral, 0.62),"
        " @rm_selection 58%, alpha(@rm_operation, 0.16));"
        " color:@rm_selection_text; border-color:alpha(@rm_neutral, 0.76);"
        " box-shadow:0 4px 16px alpha(@rm_neutral, 0.32);"
        "}\n"
        "#runner-main-nav-button .runner-main-nav-label {"
        " color:@rm_summary; font-size:14px; font-weight:700;"
        "}\n"
        "#runner-main-nav-button:hover .runner-main-nav-label { color:@rm_title; }\n"
        "#runner-main-nav-button:checked .runner-main-nav-label { color:@rm_selection_text; }\n"
        "#runner-main-nav-button .runner-main-nav-icon {"
        " min-width:38px; min-height:38px; color:@rm_neutral;"
        " background-color:@rm_surface;"
        " background-image:linear-gradient(135deg, alpha(@rm_neutral, 0.20),"
        " alpha(@rm_card, 0.76));"
        " border:1px solid alpha(@rm_connection_border, 0.88);"
        " border-radius:12px; padding:5px;"
        " box-shadow:0 2px 8px alpha(#000000, 0.18);"
        "}\n"
        "#runner-main-nav-button:checked .runner-main-nav-icon {"
        " color:@rm_accent_foreground;"
        " background-image:linear-gradient(135deg, @rm_neutral, @rm_operation);"
        " border-color:alpha(@rm_accent_foreground, 0.48);"
        " box-shadow:0 3px 10px alpha(@rm_neutral, 0.26);"
        "}\n"
        ".runner-main-nav-separator {"
        " background-color:alpha(@rm_connection_border, 0.78); min-height:1px;"
        "}\n");

    g_string_append_printf(
        css,
        "scrolledwindow.runner-table {"
        " background:@rm_card; border:1px solid @rm_connection_border;"
        " border-radius:10px; box-shadow:inset 0 1px 0 alpha(@rm_neutral, 0.08);"
        "}\n"
        "treeview.view {"
        " background:@rm_input; color:@rm_text; border:0;"
        " -GtkTreeView-horizontal-separator:0;"
        " -GtkTreeView-vertical-separator:0;"
        "}\n"
        "treeview.view:selected {"
        " background:@rm_selection; color:@rm_selection_text;"
        "}\n"
        "treeview.view header button {"
        " background-color:@rm_panel; color:@rm_title;"
        " background-image:linear-gradient(to bottom, alpha(@rm_neutral, 0.10),"
        " alpha(@rm_panel, 0.94));"
        " border:0; border-bottom:1px solid @rm_connection_border;"
        " min-height:36px; padding:0 5px; font-weight:%u;"
        "}\n"
        "treeview.view header button:hover { background:@rm_surface_hover; }\n"
        ".selection-card {"
        " background-color:@rm_connection;"
        " background-image:linear-gradient(100deg, alpha(@rm_neutral, 0.12),"
        " @rm_connection 58%%, alpha(@rm_operation, 0.08));"
        " border-top:1px solid alpha(@rm_neutral, 0.34);"
        " padding:%upx %upx;"
        "}\n"
        ".selection-icon-well {"
        " background-image:linear-gradient(135deg, alpha(@rm_neutral, 0.26),"
        " alpha(@rm_operation, 0.10)); border:1px solid @rm_connection_border;"
        " border-radius:12px; padding:6px;"
        "}\n"
        ".selection-icon { color:@rm_neutral; }\n"
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
        " background-color:@rm_card;"
        " background-image:linear-gradient(105deg, alpha(@rm_neutral, 0.06),"
        " @rm_card 54%%, alpha(@rm_operation, 0.035));"
        " border:1px solid @rm_connection_border; border-radius:%upx;"
        " padding:%upx %upx; box-shadow:0 3px 12px alpha(#000000, 0.18);"
        "}\n"
        "#footer-actions button {"
        " border-radius:%upx; min-height:34px; padding:5px 11px;"
        "}\n"
        ".status-text { color:@rm_summary; font-size:11px; }\n"
        ".version-text { color:@rm_detail_label; font-size:10px; }\n"
        ".runner-action-button {"
        " background-color:@rm_surface;"
        " background-image:linear-gradient(to bottom, alpha(@rm_neutral, 0.10),"
        " alpha(@rm_surface, 0.88)); border:1px solid @rm_connection_border;"
        " box-shadow:0 2px 7px alpha(#000000, 0.18);"
        "}\n"
        ".runner-action-button:hover { border-color:@rm_neutral; }\n"
        ".runner-action-primary {"
        " background-image:linear-gradient(135deg, @rm_neutral, @rm_operation);"
        " color:@rm_accent_foreground; border-color:alpha(@rm_accent_foreground, 0.42);"
        "}\n"
        ".runner-action-primary label, .runner-action-primary image { color:@rm_accent_foreground; }\n"
        ".runner-action-warning { border-color:alpha(@rm_warning, 0.58); }\n"
        ".runner-card-grid { background:@rm_card; padding:10px; }\n"
        ".runner-card-grid flowboxchild { background:transparent; border:0; padding:0; }\n"
        ".runner-card { background:@rm_surface;"
        " background-image:linear-gradient(135deg, alpha(@rm_neutral, 0.10), alpha(@rm_card, 0.75));"
        " border:1px solid @rm_connection_border; border-radius:12px; padding:8px; }\n"
        ".runner-card-grid flowboxchild:hover .runner-card { border-color:@rm_neutral; }\n"
        ".runner-card-grid flowboxchild:selected .runner-card {"
        " border-color:@rm_neutral; background-color:@rm_selection;"
        " box-shadow:inset 0 0 0 1px alpha(@rm_neutral, 0.55); }\n"
        ".runner-card-name { color:@rm_heading; font-size:14px; font-weight:700; }\n"
        ".runner-card-os, .runner-card-job { color:@rm_summary; font-size:11px; }\n"
        ".runner-state { font-size:11px; font-weight:700; }\n"
        ".runner-state.running { color:@rm_success; }\n"
        ".runner-state.idle { color:@rm_warning; }\n"
        ".runner-state.offline { color:@rm_fault; }\n"
        ".runner-utilisation trough, .runner-utilisation trough:backdrop { min-height:5px; min-width:72px;"
        " background-image:none; background-color:alpha(@rm_border, 0.45); border:0; border-radius:4px; }\n"
        ".runner-utilisation progress, .runner-utilisation progress:backdrop { min-height:5px;"
        " background-image:none; background-color:@rm_neutral;"
        " border:0; border-radius:4px; }\n"
        ".runner-view-switch button { padding:4px 9px; min-height:26px;"
        " background-image:none; background-color:@rm_surface;"
        " border:1px solid @rm_connection_border; }\n"
        ".runner-view-switch button label { color:@rm_text; text-shadow:none; }\n"
        ".runner-view-switch button:checked { background-color:@rm_selection;"
        " border-color:@rm_neutral; }\n"
        ".runner-view-switch button:checked label { color:@rm_selection_text; }\n",
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
    app->theme_state_valid = TRUE;
    app->theme_state_mode = app->config.theme_mode;
    app->theme_state_dark = night_theme;
    g_string_free(css, TRUE);
}

static char *run_command(char **argv, GError **error)
{
    gchar *stdout_text = NULL;
    gchar *stderr_text = NULL;
    gint status = 0;
    GError *spawn_error = NULL;
    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      &stdout_text, &stderr_text, &status, &spawn_error)) {
        if (error)
            g_propagate_error(error, spawn_error);
        else
            g_clear_error(&spawn_error);
        g_free(stdout_text); g_free(stderr_text);
        return NULL;
    }
    GError *wait_error = NULL;
    if (!g_spawn_check_wait_status(status, &wait_error)) {
        if (stderr_text && *stderr_text) {
            g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                        "%s", g_strstrip(stderr_text));
            g_clear_error(&wait_error);
        } else if (error) {
            g_propagate_error(error, wait_error);
        } else {
            g_clear_error(&wait_error);
        }
        g_free(stdout_text); g_free(stderr_text);
        return NULL;
    }
    g_free(stderr_text);
    return stdout_text;
}

static char *gh_api(const char *endpoint, const char *jq, GError **error)
{
    char *argv[] = {
        (char *)"gh", (char *)"api",
        (char *)"-H", (char *)"Accept: application/vnd.github+json",
        (char *)endpoint, (char *)"--jq", (char *)jq, NULL
    };
    return run_command(argv, error);
}

static GPtrArray *split_tsv_lines(const char *text)
{
    GPtrArray *rows = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev);
    if (!text) return rows;
    char **lines = g_strsplit(text, "\n", -1);
    for (guint i = 0U; lines[i]; i++) {
        if (!*lines[i]) continue;
        char **fields = g_strsplit(lines[i], "\t", -1);
        g_ptr_array_add(rows, fields);
    }
    g_strfreev(lines);
    return rows;
}

static const char *field(char **fields, guint index)
{
    if (!fields) return "";
    for (guint current = 0U; current < index; current++)
        if (!fields[current]) return "";
    return fields[index] ? fields[index] : "";
}

static char *filter_needle_casefold(RunnerScopeApp *app)
{
    if (!app || !app->filter_entry) return NULL;
    const char *needle = gtk_entry_get_text(GTK_ENTRY(app->filter_entry));
    return needle && *needle ? g_utf8_casefold(needle, -1) : NULL;
}

static gboolean text_matches_folded(const char *folded_needle, const char *text)
{
    if (!folded_needle || !*folded_needle) return TRUE;
    char *folded_text = g_utf8_casefold(text ? text : "", -1);
    const gboolean matches = strstr(folded_text, folded_needle) != NULL;
    g_free(folded_text);
    return matches;
}

static void persist_history(RunnerScopeApp *app)
{
    if (!app || !app->history_rows) return;

    GString *state = g_string_new(NULL);
    for (guint i = 0U; i < app->history_rows->len; i++) {
        HistoryRow *entry = g_ptr_array_index(app->history_rows, i);
        g_string_append_printf(state, "%s\t%s\t%s\t%s\n",
                               entry->time_text, entry->runner,
                               entry->event, entry->detail);
    }

    char *path = state_path();
    char *directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) == 0) {
        const InfiltratrAtomicFileMode mode =
            access(path, F_OK) == 0
                ? INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS
                : INFILTRATR_ATOMIC_FILE_PRIVATE;
        (void)infiltratr_atomic_file_write_bytes(
            path, mode, state->str, state->len);
    }
    g_free(directory);
    g_free(path);
    g_string_free(state, TRUE);
}

static gboolean persist_history_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    app->history_save_timer = 0U;
    persist_history(app);
    return G_SOURCE_REMOVE;
}

static void schedule_history_persist(RunnerScopeApp *app)
{
    if (!app || g_atomic_int_get(&app->shutting_down)) return;
    if (app->history_save_timer != 0U)
        g_source_remove(app->history_save_timer);
    app->history_save_timer =
        g_timeout_add(750U, persist_history_cb, app);
}

static void add_history(RunnerScopeApp *app, const char *runner,
                        const char *event, const char *detail)
{
    HistoryRow *row = g_new0(HistoryRow, 1U);
    row->time_text = history_time_text();
    row->runner = g_strdup(runner ? runner : "—");
    row->event = g_strdup(event ? event : "—");
    row->detail = g_strdup(detail && *detail ? detail : "—");
    g_ptr_array_insert(app->history_rows, 0U, row);
    while (app->history_rows->len > 300U)
        g_ptr_array_remove_index(app->history_rows, app->history_rows->len - 1U);

    /*
     * Runner state changes commonly arrive as a batch.  Persist once after the
     * batch settles instead of serialising and atomically rewriting the entire
     * history file once per changed runner on the GTK main thread.
     */
    schedule_history_persist(app);
}

static void load_history(RunnerScopeApp *app)
{
    char *path = state_path();
    gchar *contents = NULL;
    gsize length = 0U;
    if (g_file_get_contents(path, &contents, &length, NULL) && length != 0U) {
        char **lines = g_strsplit(contents, "\n", -1);
        for (guint i = 0U; lines[i] && app->history_rows->len < 300U; i++) {
            if (!*lines[i]) continue;
            char **parts = g_strsplit(lines[i], "\t", 4);
            if (parts[0] && parts[1] && parts[2] && parts[3]) {
                HistoryRow *row = g_new0(HistoryRow, 1U);
                row->time_text = g_strdup(parts[0]);
                row->runner = g_strdup(parts[1]);
                row->event = g_strdup(parts[2]);
                row->detail = g_strdup(parts[3]);
                g_ptr_array_add(app->history_rows, row);
            }
            g_strfreev(parts);
        }
        g_strfreev(lines);
    }
    g_free(contents); g_free(path);
}

static GtkTreeViewColumn *tree_add_text_column(GtkWidget *tree,
                                             const char *title,
                                             gint column,
                                             gint min_width)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(
        renderer,
        "ellipsize", PANGO_ELLIPSIZE_END,
        "ypad", 7,
        "xpad", 5,
        NULL);
    GtkTreeViewColumn *view_column =
        gtk_tree_view_column_new_with_attributes(title, renderer, "text", column, NULL);
    gtk_tree_view_column_set_sizing(view_column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(view_column, min_width);
    gtk_tree_view_column_set_resizable(view_column, TRUE);
    gtk_tree_view_column_set_min_width(view_column, min_width);
    gtk_tree_view_column_set_sort_column_id(view_column, column);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), view_column);
    return view_column;
}

static GtkWidget *scrolled_tree(GtkListStore *store)
{
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tree), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    return scroll;
}

static void tree_model_rebuild_begin(GtkWidget *tree)
{
    if (tree)
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree), NULL);
}

static void tree_model_rebuild_end(GtkWidget *tree, GtkListStore *store)
{
    if (tree && store)
        gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
}

static GtkWidget *make_workspace_metric(const char *caption,
                                        GtkWidget **caption_out,
                                        GtkWidget **value_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *caption_label = gtk_label_new(caption);
    GtkWidget *value_label = gtk_label_new("—");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(box), "workspace-metric");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(caption_label), "workspace-metric-caption");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value_label), "workspace-metric-value");
    gtk_widget_set_halign(caption_label, GTK_ALIGN_START);
    gtk_widget_set_halign(value_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), caption_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), value_label, FALSE, FALSE, 0);
    *caption_out = caption_label;
    *value_out = value_label;
    return box;
}

static void workspace_metric(RunnerScopeApp *app, guint index,
                             const char *caption, const char *value)
{
    if (!app || index >= 4U || !app->workspace_metric_caption[index] ||
        !app->workspace_metric_value[index])
        return;
    label_set_if_changed(
        app->workspace_metric_caption[index], caption ? caption : "—");
    label_set_if_changed(
        app->workspace_metric_value[index], value ? value : "—");
}

static guint count_local_state(RunnerScopeApp *app, const char *state)
{
    guint count = 0U;
    if (!app || !state) return 0U;
    for (guint i = 0U; i < app->local_rows->len; i++) {
        LocalRow *row = g_ptr_array_index(app->local_rows, i);
        if (row->service_state && g_ascii_strcasecmp(row->service_state, state) == 0)
            count++;
    }
    return count;
}

static guint count_local_github(RunnerScopeApp *app)
{
    guint count = 0U;
    if (!app) return 0U;
    for (guint i = 0U; i < app->local_rows->len; i++) {
        LocalRow *row = g_ptr_array_index(app->local_rows, i);
        if (row->github_state && *row->github_state &&
            strcmp(row->github_state, "—") != 0 &&
            g_ascii_strcasecmp(row->github_state, "unknown") != 0)
            count++;
    }
    return count;
}

static guint count_local_diagnostics(RunnerScopeApp *app)
{
    guint count = 0U;
    if (!app) return 0U;
    for (guint i = 0U; i < app->local_rows->len; i++) {
        LocalRow *row = g_ptr_array_index(app->local_rows, i);
        if (row->diag && *row->diag && strcmp(row->diag, "—") != 0)
            count++;
    }
    return count;
}

static void clear_selection_card(RunnerScopeApp *app)
{
    if (!app || !app->selection_card) return;
    gtk_widget_hide(app->selection_card);
}

static void show_selection_card(RunnerScopeApp *app,
                                const char *title,
                                const char *primary,
                                const char *secondary)
{
    if (!app || !app->selection_card) return;
    gtk_label_set_text(GTK_LABEL(app->selection_title), title ? title : "Selection");
    gtk_label_set_text(GTK_LABEL(app->selection_primary), primary ? primary : "—");
    gtk_label_set_text(GTK_LABEL(app->selection_secondary), secondary ? secondary : "—");
    gtk_widget_show_all(app->selection_card);
    gtk_widget_show(app->selection_card);
}

static void show_runner_details(RunnerScopeApp *app, const RunnerRow *row)
{
    if (!row) return;
    char *primary = g_strdup_printf(
        "%s  •  %s for %s  •  %s jobs  •  %s session utilisation",
        row->os, row->state, row->state_for, row->jobs, row->busy_pct);
    char *secondary = g_strdup_printf(
        "Repository: %s  •  Job: %s  •  Runtime: %s\nLabels: %s",
        row->repo, row->job, row->runtime, row->labels);
    show_selection_card(app, row->name, primary, secondary);
    g_free(primary);
    g_free(secondary);
}

static void on_runner_card_selection(GtkFlowBox *box, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    if (app->runner_rendering) return;
    GList *selected = gtk_flow_box_get_selected_children(box);
    g_clear_pointer(&app->selected_runner_name, g_free);
    if (selected) {
        const char *name = g_object_get_data(G_OBJECT(selected->data), "runner-name");
        app->selected_runner_name = g_strdup(name);
        show_runner_details(app, g_hash_table_lookup(app->runner_row_by_name, name));
    } else {
        clear_selection_card(app);
    }
    g_list_free(selected);
}

static void on_runner_card_activated(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer user_data)
{
    (void)box;
    RunnerScopeApp *app = user_data;
    const char *name = g_object_get_data(G_OBJECT(child), "runner-name");
    g_free(app->selected_runner_name);
    app->selected_runner_name = g_strdup(name);
    show_runner_details(app, g_hash_table_lookup(app->runner_row_by_name, name));
}

static GtkWidget *runner_card_label(const char *text, const char *css_class)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_style_context_add_class(gtk_widget_get_style_context(label), css_class);
    return label;
}

static GtkWidget *make_runner_card(RunnerScopeApp *app, const RunnerRow *row)
{
    GtkWidget *child = gtk_flow_box_child_new();
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(card, 205, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "runner-card");
    gtk_container_add(GTK_CONTAINER(child), card);
    g_object_set_data_full(G_OBJECT(child), "runner-name", g_strdup(row->name), g_free);

    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *art = gtk_image_new_from_pixbuf(app->runner_card_art);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name = runner_card_label(row->name, "runner-card-name");
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(name), 15);
    GtkWidget *os = runner_card_label(row->os, "runner-card-os");
    gtk_box_pack_start(GTK_BOX(copy), name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(copy), os, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(identity), art, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(identity), copy, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), identity, FALSE, FALSE, 0);

    GtkWidget *status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *state = runner_card_label("", "runner-state");
    GtkWidget *bar = gtk_progress_bar_new();
    gtk_widget_set_hexpand(bar, TRUE);
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "runner-utilisation");
    gtk_box_pack_start(GTK_BOX(status_row), state, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_row), bar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), status_row, FALSE, FALSE, 0);

    GtkWidget *job = runner_card_label("", "runner-card-job");
    gtk_label_set_ellipsize(GTK_LABEL(job), PANGO_ELLIPSIZE_END);
    gtk_widget_set_no_show_all(job, TRUE);
    gtk_box_pack_start(GTK_BOX(card), job, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(child), "runner-os", os);
    g_object_set_data(G_OBJECT(child), "runner-state", state);
    g_object_set_data(G_OBJECT(child), "runner-utilisation", bar);
    g_object_set_data(G_OBJECT(child), "runner-job", job);
    gtk_flow_box_insert(GTK_FLOW_BOX(app->runner_cards), child, -1);
    return child;
}

static void update_runner_card(GtkWidget *child, const RunnerRow *row)
{
    GtkWidget *state = g_object_get_data(G_OBJECT(child), "runner-state");
    GtkWidget *bar = g_object_get_data(G_OBJECT(child), "runner-utilisation");
    GtkWidget *job = g_object_get_data(G_OBJECT(child), "runner-job");
    GtkWidget *os = g_object_get_data(G_OBJECT(child), "runner-os");
    const gboolean running = strcmp(row->state, "RUNNING") == 0;
    const gboolean offline = strcmp(row->state, "OFFLINE") == 0;
    label_set_if_changed(os, row->os);
    label_set_if_changed(state, running ? "● Running" : offline ? "● Offline" : "● Idle");
    GtkStyleContext *context = gtk_widget_get_style_context(state);
    const char *state_class = running ? "running" : offline ? "offline" : "idle";
    if (!gtk_style_context_has_class(context, state_class)) {
        gtk_style_context_remove_class(context, "running");
        gtk_style_context_remove_class(context, "idle");
        gtk_style_context_remove_class(context, "offline");
        gtk_style_context_add_class(context, state_class);
    }
    const double fraction = CLAMP(g_ascii_strtod(row->busy_pct, NULL) / 100.0, 0.0, 1.0);
    if (gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(bar)) != fraction)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), fraction);
    char *tooltip = g_strdup_printf("%s\nSession utilisation: %s", row->name, row->busy_pct);
    gtk_widget_set_tooltip_text(child, tooltip);
    gtk_widget_set_tooltip_text(bar, tooltip);
    g_free(tooltip);
    label_set_if_changed(job, row->job);
    gtk_widget_show_all(child);
    gtk_widget_set_visible(job, running);
}

static void on_runner_view_toggled(GtkToggleButton *button, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    const char *view = g_object_get_data(G_OBJECT(button), "runner-view");
    if (!gtk_toggle_button_get_active(button)) {
        if (g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(app->runner_views)), view) == 0)
            gtk_toggle_button_set_active(button, TRUE);
        return;
    }
    gtk_stack_set_visible_child_name(GTK_STACK(app->runner_views), view);
    GtkToggleButton *other = g_object_get_data(G_OBJECT(button), "runner-other-view");
    gtk_toggle_button_set_active(other, FALSE);
    if (app->selected_runner_name)
        show_runner_details(app, g_hash_table_lookup(app->runner_row_by_name, app->selected_runner_name));
}

static void update_workspace_context(RunnerScopeApp *app, gint page)
{
    if (!app || !app->workspace_title || !app->workspace_subtitle) return;

    const RunnerUiPageSpec *ui_page = runner_ui_page((RunnerUiPageId)page);
    runner_asset_image_set(app->workspace_icon, ui_page->nav_asset, 48, 48);
    runner_asset_image_set(app->workspace_art, ui_page->hero_asset, 240, 86);
    label_set_if_changed(
        app->workspace_title,
        runner_ui_page_title(ui_page, RUNNER_UI_PLATFORM_LINUX));
    label_set_if_changed(app->workspace_subtitle, ui_page->subtitle);

    char a[32], b[32], d[32], uptime[64];
    switch (page) {
        case 0:
            g_snprintf(a, sizeof(a), "%u", app->runners_total);
            g_snprintf(b, sizeof(b), "%u", app->runners_running);
            g_snprintf(d, sizeof(d), "%u", app->runners_idle);
            workspace_metric(app, 0U, ui_page->metric_labels[0], a);
            workspace_metric(app, 1U, ui_page->metric_labels[1], b);
            workspace_metric(app, 2U, ui_page->metric_labels[2], d);
            g_snprintf(a, sizeof(a), "%u", app->runners_offline);
            workspace_metric(app, 3U, ui_page->metric_labels[3], a);
            break;
        case 1:
            g_snprintf(a, sizeof(a), "%u", app->local_active);
            g_snprintf(b, sizeof(b), "%u", app->hosted_active);
            g_snprintf(d, sizeof(d), "%u", app->queued);
            workspace_metric(app, 0U, ui_page->metric_labels[0], a);
            workspace_metric(app, 1U, ui_page->metric_labels[1], b);
            workspace_metric(app, 2U, ui_page->metric_labels[2], d);
            g_snprintf(a, sizeof(a), "%u", app->local_active + app->hosted_active);
            workspace_metric(app, 3U, ui_page->metric_labels[3], a);
            break;
        case 2: {
            g_snprintf(a, sizeof(a), "%u", app->history_rows->len);
            g_snprintf(b, sizeof(b), "%u", g_hash_table_size(app->sessions));
            const char *filter = gtk_entry_get_text(GTK_ENTRY(app->filter_entry));
            char *uptime_text =
                duration_text(now_monotonic() - app->session_started);
            g_strlcpy(uptime, uptime_text ? uptime_text : "—", sizeof(uptime));
            g_free(uptime_text);
            workspace_metric(app, 0U, ui_page->metric_labels[0], a);
            workspace_metric(app, 1U, ui_page->metric_labels[1], b);
            workspace_metric(app, 2U, ui_page->metric_labels[2], uptime);
            workspace_metric(app, 3U, ui_page->metric_labels[3], filter && *filter ? "ACTIVE" : "ALL");
            break;
        }
        default:
            g_snprintf(a, sizeof(a), "%u", app->local_rows->len);
            g_snprintf(b, sizeof(b), "%u", count_local_state(app, "RUNNING"));
            g_snprintf(d, sizeof(d), "%u", count_local_github(app));
            workspace_metric(app, 0U, ui_page->metric_labels[0], a);
            workspace_metric(app, 1U, ui_page->metric_labels[1], b);
            workspace_metric(app, 2U, ui_page->metric_labels[2], d);
            g_snprintf(a, sizeof(a), "%u", count_local_diagnostics(app));
            workspace_metric(app, 3U, ui_page->metric_labels[3], a);
            break;
    }

    if (app->open_job_button)
        gtk_widget_set_visible(
            app->open_job_button,
            (ui_page->actions & RUNNER_UI_ACTION_OPEN_JOB) != 0U);
    if (app->open_diag_button)
        gtk_widget_set_visible(
            app->open_diag_button,
            (ui_page->actions & RUNNER_UI_ACTION_OPEN_DIAGNOSTIC) != 0U);
    if (app->restart_button)
        gtk_widget_set_visible(
            app->restart_button,
            (ui_page->actions & RUNNER_UI_ACTION_RESTART_RUNNER) != 0U);
    if (app->runner_view_switch)
        gtk_widget_set_visible(app->runner_view_switch, ui_page->show_view_switch);
}

static void render_history(RunnerScopeApp *app)
{
    char *folded_needle = filter_needle_casefold(app);
    tree_model_rebuild_begin(app->history_tree);
    gtk_list_store_clear(app->history_store);
    for (guint i = 0U; i < app->history_rows->len; i++) {
        HistoryRow *row = g_ptr_array_index(app->history_rows, i);
        char *search = g_strdup_printf("%s %s %s %s", row->time_text, row->runner,
                                       row->event, row->detail);
        const gboolean visible = text_matches_folded(folded_needle, search);
        g_free(search);
        if (!visible) continue;
        GtkTreeIter iter;
        gtk_list_store_append(app->history_store, &iter);
        gtk_list_store_set(app->history_store, &iter,
            HIST_COL_TIME, row->time_text,
            HIST_COL_RUNNER, row->runner,
            HIST_COL_EVENT, row->event,
            HIST_COL_DETAIL, row->detail, -1);
    }
    tree_model_rebuild_end(app->history_tree, app->history_store);
    g_free(folded_needle);
}

static void render_runners(RunnerScopeApp *app)
{
    char *folded_needle = filter_needle_casefold(app);
    label_set_if_changed(app->runner_cards_empty, folded_needle && *folded_needle
        ? "No runners match this filter." : "No runners detected.");
    guint visible_cards = 0U;
    app->runner_rendering = TRUE;
    GHashTableIter cards;
    gpointer card_name, card_widget;
    g_hash_table_iter_init(&cards, app->runner_card_by_name);
    while (g_hash_table_iter_next(&cards, &card_name, &card_widget)) {
        if (!g_hash_table_contains(app->runner_row_by_name, card_name)) {
            gtk_widget_destroy(card_widget);
            g_hash_table_iter_remove(&cards);
        } else {
            gtk_widget_hide(card_widget);
        }
    }
    tree_model_rebuild_begin(app->runner_tree);
    gtk_list_store_clear(app->runner_store);
    for (guint i = 0U; i < app->runner_rows->len; i++) {
        RunnerRow *row = g_ptr_array_index(app->runner_rows, i);
        char *search = g_strdup_printf("%s %s %s %s %s %s", row->name, row->os,
                                       row->state, row->repo, row->job, row->labels);
        const gboolean visible = text_matches_folded(folded_needle, search);
        g_free(search);
        if (!visible) continue;
        visible_cards++;
        GtkWidget *card = g_hash_table_lookup(app->runner_card_by_name, row->name);
        if (!card) {
            card = make_runner_card(app, row);
            g_hash_table_insert(app->runner_card_by_name, g_strdup(row->name), card);
        }
        update_runner_card(card, row);
        GtkTreeIter iter;
        gtk_list_store_append(app->runner_store, &iter);
        gtk_list_store_set(app->runner_store, &iter,
            RUNNER_COL_NAME, row->name, RUNNER_COL_OS, row->os,
            RUNNER_COL_STATE, row->state, RUNNER_COL_REPO, row->repo,
            RUNNER_COL_JOB, row->job, RUNNER_COL_RUNTIME, row->runtime,
            RUNNER_COL_STATE_FOR, row->state_for, RUNNER_COL_JOBS, row->jobs,
            RUNNER_COL_BUSY, row->busy_pct, RUNNER_COL_LABELS, row->labels, -1);
    }
    tree_model_rebuild_end(app->runner_tree, app->runner_store);
    gtk_widget_set_visible(app->runner_cards_empty, visible_cards == 0U);
    GtkWidget *selected = app->selected_runner_name
        ? g_hash_table_lookup(app->runner_card_by_name, app->selected_runner_name) : NULL;
    if (selected && gtk_widget_get_visible(selected)) {
        gtk_flow_box_select_child(GTK_FLOW_BOX(app->runner_cards), GTK_FLOW_BOX_CHILD(selected));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app->runner_store), &iter);
        while (valid) {
            char *name = NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(app->runner_store), &iter, RUNNER_COL_NAME, &name, -1);
            const gboolean match = g_strcmp0(name, app->selected_runner_name) == 0;
            g_free(name);
            if (match) {
                gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(app->runner_tree)), &iter);
                break;
            }
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(app->runner_store), &iter);
        }
        show_runner_details(app, g_hash_table_lookup(app->runner_row_by_name, app->selected_runner_name));
    } else {
        g_clear_pointer(&app->selected_runner_name, g_free);
        gtk_flow_box_unselect_all(GTK_FLOW_BOX(app->runner_cards));
        clear_selection_card(app);
    }
    app->runner_rendering = FALSE;
    g_free(folded_needle);
}

static void render_activity(RunnerScopeApp *app)
{
    char *folded_needle = filter_needle_casefold(app);
    tree_model_rebuild_begin(app->activity_tree);
    gtk_list_store_clear(app->activity_store);
    for (guint i = 0U; i < app->activity_rows->len; i++) {
        ActivityRow *row = g_ptr_array_index(app->activity_rows, i);
        char *search = g_strdup_printf("%s %s %s %s %s %s %s %s",
            row->environment, row->repo, row->workflow, row->job,
            row->step, row->status, row->runner, row->branch);
        const gboolean visible = text_matches_folded(folded_needle, search);
        g_free(search);
        if (!visible) continue;
        GtkTreeIter iter;
        gtk_list_store_append(app->activity_store, &iter);
        gtk_list_store_set(app->activity_store, &iter,
            ACT_COL_ENV, row->environment, ACT_COL_REPO, row->repo,
            ACT_COL_WORKFLOW, row->workflow, ACT_COL_JOB, row->job,
            ACT_COL_STEP, row->step, ACT_COL_STATUS, row->status,
            ACT_COL_RUNNER, row->runner, ACT_COL_RUNTIME, row->runtime,
            ACT_COL_EVENT, row->event, ACT_COL_BRANCH, row->branch,
            ACT_COL_URL, row->url, -1);
    }
    tree_model_rebuild_end(app->activity_tree, app->activity_store);
    g_free(folded_needle);
}

static void render_local(RunnerScopeApp *app)
{
    char *folded_needle = filter_needle_casefold(app);
    tree_model_rebuild_begin(app->local_tree);
    gtk_list_store_clear(app->local_store);
    for (guint i = 0U; i < app->local_rows->len; i++) {
        LocalRow *row = g_ptr_array_index(app->local_rows, i);
        char *search = g_strdup_printf("%s %s %s %s %s", row->runner,
            row->service_state, row->github_state, row->diag, row->path);
        const gboolean visible = text_matches_folded(folded_needle, search);
        g_free(search);
        if (!visible) continue;
        GtkTreeIter iter;
        gtk_list_store_append(app->local_store, &iter);
        gtk_list_store_set(app->local_store, &iter,
            LOCAL_COL_RUNNER, row->runner, LOCAL_COL_SERVICE, row->service_state,
            LOCAL_COL_GITHUB, row->github_state, LOCAL_COL_PID, row->pid,
            LOCAL_COL_START, row->start_mode, LOCAL_COL_ACCOUNT, row->account,
            LOCAL_COL_DIAG, row->diag, LOCAL_COL_DIAG_AGE, row->diag_age,
            LOCAL_COL_PATH, row->path, LOCAL_COL_DIAG_PATH, row->diag_path,
            LOCAL_COL_SERVICE_NAME, row->service_name, -1);
    }
    tree_model_rebuild_end(app->local_tree, app->local_store);
    g_free(folded_needle);
}

static void render_page(RunnerScopeApp *app, gint page)
{
    switch (page) {
        case 0: render_runners(app); break;
        case 1: render_activity(app); break;
        case 2: render_history(app); break;
        case 3: render_local(app); break;
        default: break;
    }
}

static gboolean same_text(const char *left, const char *right)
{
    return g_strcmp0(left, right) == 0;
}

static void rebuild_runner_row_index(RunnerScopeApp *app)
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

static gboolean runner_apply_idle(gpointer data)
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
    rebuild_runner_row_index(app);
    g_ptr_array_unref(old_rows);

    const guint previous_running = app->runners_running;
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
    if (page == 0 || (page == 2 && history_changed))
        update_workspace_context(app, page);

    char *status = app->config.expected_runners != 0U
        ? g_strdup_printf("%u / %u expected runners detected",
            app->runners_total, app->config.expected_runners)
        : g_strdup_printf("%u runners detected", app->runners_total);
    label_set_if_changed(app->status_label, status);
    g_free(status);

    g_ptr_array_unref(result->rows);
    g_atomic_int_set(&app->runner_refreshing, 0);
    if (page == 0 && running != 0U &&
        (previous_running == 0U || app->activity_rows->len == 0U))
        request_activity_refresh(app);
    g_free(result);
    return G_SOURCE_REMOVE;
}

static gpointer runner_worker(gpointer data)
{
    RunnerScopeApp *app = data;
    RunnerRefreshResult *result = g_new0(RunnerRefreshResult, 1U);
    result->app = app;
    result->rows = g_ptr_array_new_with_free_func(raw_runner_free);

    char *endpoint = g_strdup_printf("/orgs/%s/actions/runners?per_page=100",
                                     app->config.organisation);
    const char *jq =
        ".runners[] | [.name,.os,.status,(.busy|tostring),"
        "([.labels[].name]|join(\",\"))] | @tsv";
    GError *error = NULL;
    char *output = gh_api(endpoint, jq, &error);
    g_free(endpoint);
    if (!output) {
        result->error = g_strdup_printf("Runner API error: %s",
                                        error ? error->message : "unknown failure");
        g_clear_error(&error);
    } else {
        GPtrArray *lines = split_tsv_lines(output);
        for (guint i = 0U; i < lines->len; i++) {
            char **fields = g_ptr_array_index(lines, i);
            RawRunner *row = g_new0(RawRunner, 1U);
            row->name = g_strdup(field(fields, 0U));
            row->os = g_strdup(field(fields, 1U));
            row->status = g_strdup(field(fields, 2U));
            row->busy = g_ascii_strcasecmp(field(fields, 3U), "true") == 0;
            row->labels = g_strdup(field(fields, 4U));
            g_ptr_array_add(result->rows, row);
        }
        g_ptr_array_unref(lines);
        g_free(output);
    }
    g_idle_add(runner_apply_idle, result);
    return NULL;
}

static void request_runner_refresh(RunnerScopeApp *app)
{
    if (!app->config.organisation[0] || g_atomic_int_get(&app->shutting_down)) return;
    if (!g_atomic_int_compare_and_exchange(&app->runner_refreshing, 0, 1)) return;
    app->runner_thread = g_thread_new("runnerscope-runners", runner_worker, app);
}

static double parse_iso8601_epoch(const char *text)
{
    if (!text || !*text) return 0.0;
    GDateTime *dt = g_date_time_new_from_iso8601(text, NULL);
    if (!dt) return 0.0;
    const double value = (double)g_date_time_to_unix(dt);
    g_date_time_unref(dt);
    return value;
}

static char *job_environment(const char *group, const char *labels)
{
    if (labels && strstr(labels, "self-hosted"))
        return g_strdup("LOCAL");
    if (group && strcmp(group, "GitHub Actions") == 0)
        return g_strdup("GITHUB");
    if (labels && (strstr(labels, "ubuntu-") || strstr(labels, "windows-") ||
                   strstr(labels, "macos-") || strstr(labels, "-latest")))
        return g_strdup("GITHUB");
    return g_strdup("UNKNOWN");
}

static gpointer activity_worker(gpointer data)
{
    RunnerScopeApp *app = data;
    ActivityRefreshResult *result = g_new0(ActivityRefreshResult, 1U);
    result->app = app;
    result->rows = g_ptr_array_new_with_free_func(activity_row_free);

    char *repo_endpoint = g_strdup_printf(
        "/orgs/%s/repos?type=all&sort=pushed&direction=desc&per_page=100",
        app->config.organisation);
    GError *error = NULL;
    char *repo_output = gh_api(
        repo_endpoint,
        ".[] | select(.archived==false and .disabled==false) | .name",
        &error);
    g_free(repo_endpoint);
    if (!repo_output) {
        result->error = g_strdup_printf("Activity scan error: %s",
            error ? error->message : "unable to list repositories");
        g_clear_error(&error);
        g_idle_add(activity_apply_idle, result);
        return NULL;
    }

    char **repos = g_strsplit(repo_output, "\n", -1);
    g_free(repo_output);
    guint scanned = 0U;

    for (guint r = 0U; repos[r] && scanned < app->config.repository_scan_limit; r++) {
        if (!*repos[r]) continue;
        scanned++;
        char *runs_endpoint = g_strdup_printf(
            "/repos/%s/%s/actions/runs?per_page=100&exclude_pull_requests=true",
            app->config.organisation, repos[r]);
        char *runs_output = gh_api(
            runs_endpoint,
            ".workflow_runs[] | select(.status==\"in_progress\" or .status==\"queued\") | "
            "[(.id|tostring),(.name//.display_title//\"Workflow\"),(.event//\"—\"),"
            "(.head_branch//\"—\"),(.run_started_at//.created_at//\"\"),"
            "(.html_url//\"\"),.status] | @tsv",
            &error);
        g_free(runs_endpoint);
        if (!runs_output) {
            g_clear_error(&error);
            continue;
        }
        GPtrArray *runs = split_tsv_lines(runs_output);
        g_free(runs_output);

        for (guint i = 0U; i < runs->len; i++) {
            char **run_fields = g_ptr_array_index(runs, i);
            const char *run_id = field(run_fields, 0U);
            char *jobs_endpoint = g_strdup_printf(
                "/repos/%s/%s/actions/runs/%s/jobs?per_page=100&filter=latest",
                app->config.organisation, repos[r], run_id);
            char *jobs_output = gh_api(
                jobs_endpoint,
                ".jobs[] | [(.name//\"Job\"),.status,(.conclusion//\"\"),"
                "(.runner_name//\"\"),(.runner_group_name//\"\"),"
                "([.labels[]]|join(\",\")),(.started_at//\"\"),(.html_url//\"\"),"
                "([.steps[] | select(.status==\"in_progress\") | .name][0] // \"—\")] | @tsv",
                &error);
            g_free(jobs_endpoint);
            if (!jobs_output) {
                g_clear_error(&error);
                continue;
            }
            GPtrArray *jobs = split_tsv_lines(jobs_output);
            g_free(jobs_output);

            for (guint j = 0U; j < jobs->len; j++) {
                char **job = g_ptr_array_index(jobs, j);
                const char *status = field(job, 1U);
                if (strcmp(status, "completed") == 0) continue;
                ActivityRow *row = g_new0(ActivityRow, 1U);
                row->repo = g_strdup(repos[r]);
                row->workflow = g_strdup(field(run_fields, 1U));
                row->job = g_strdup(field(job, 0U));
                row->step = g_strdup(field(job, 8U));
                row->status = g_ascii_strcasecmp(status, "in_progress") == 0
                    ? g_strdup("IN_PROGRESS") : g_strdup("QUEUED");
                row->runner = g_strdup(*field(job, 3U) ? field(job, 3U) : "—");
                row->environment = job_environment(field(job, 4U), field(job, 5U));
                row->started_epoch = parse_iso8601_epoch(field(job, 6U));
                row->runtime = row->started_epoch > 0.0
                    ? duration_text(MAX(0.0, (double)time(NULL) - row->started_epoch))
                    : g_strdup("—");
                row->event = g_strdup(field(run_fields, 2U));
                row->branch = g_strdup(field(run_fields, 3U));
                row->url = g_strdup(*field(job, 7U)
                    ? field(job, 7U) : field(run_fields, 5U));
                g_ptr_array_add(result->rows, row);
                if (strcmp(row->status, "QUEUED") == 0) result->queued++;
                else if (strcmp(row->environment, "LOCAL") == 0) result->local_active++;
                else if (strcmp(row->environment, "GITHUB") == 0) result->hosted_active++;
            }
            g_ptr_array_unref(jobs);
        }
        g_ptr_array_unref(runs);
    }
    g_strfreev(repos);
    g_idle_add(activity_apply_idle, result);
    return NULL;
}

static gboolean activity_apply_idle(gpointer data)
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
    rebuild_activity_row_index(app);
    g_ptr_array_unref(old_rows);

    app->local_active = result->local_active;
    app->hosted_active = result->hosted_active;
    app->queued = result->queued;
    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (rows_changed && page == 1) {
        render_activity(app);
        update_workspace_context(app, page);
    }

    g_atomic_int_set(&app->activity_refreshing, 0);
    g_free(result);
    return G_SOURCE_REMOVE;
}

static void request_activity_refresh(RunnerScopeApp *app)
{
    if (!app->config.organisation[0] || g_atomic_int_get(&app->shutting_down)) return;
    if (!g_atomic_int_compare_and_exchange(&app->activity_refreshing, 0, 1)) return;
    app->activity_thread = g_thread_new("runnerscope-activity", activity_worker, app);
}

static gpointer local_worker(gpointer data)
{
    RunnerScopeApp *app = data;
    LocalRefreshResult *result = g_new0(LocalRefreshResult, 1U);
    result->app = app;
    result->rows = g_ptr_array_new_with_free_func(local_row_free);
    GError *error = NULL;

    char *argv[] = {(char *)"systemctl", (char *)"list-units", (char *)"--all",
        (char *)"--type=service", (char *)"--no-legend", (char *)"--plain",
        (char *)"actions.runner.*", NULL};
    char *output = run_command(argv, &error);
    if (!output) {
        result->error = g_strdup_printf("Local service health error: %s",
            error ? error->message : "systemctl unavailable");
        g_clear_error(&error);
        g_idle_add(local_apply_idle, result);
        return NULL;
    }

    char **lines = g_strsplit(output, "\n", -1);
    g_free(output);
    for (guint i = 0U; lines[i]; i++) {
        if (!g_str_has_prefix(lines[i], "actions.runner.")) continue;
        char **parts = g_strsplit_set(lines[i], " \t", 2);
        const char *service = parts[0];
        if (!service || !*service) { g_strfreev(parts); continue; }

        char *show_argv[] = {(char *)"systemctl", (char *)"show", (char *)service,
            (char *)"--no-pager", (char *)"-p", (char *)"Description",
            (char *)"-p", (char *)"ActiveState", (char *)"-p", (char *)"MainPID",
            (char *)"-p", (char *)"UnitFileState", (char *)"-p", (char *)"User",
            (char *)"-p", (char *)"ExecStart", NULL};
        char *show = run_command(show_argv, NULL);
        LocalRow *row = g_new0(LocalRow, 1U);
        row->service_name = g_strdup(service);
        row->runner = g_strdup(service);
        row->github_state = g_strdup("—");
        row->service_state = g_strdup("UNKNOWN");
        row->pid = g_strdup("—"); row->start_mode = g_strdup("—");
        row->account = g_strdup("—"); row->diag = g_strdup("—");
        row->diag_age = g_strdup("—"); row->path = g_strdup("—");
        row->diag_path = g_strdup("");

        if (show) {
            char **props = g_strsplit(show, "\n", -1);
            for (guint p = 0U; props[p]; p++) {
                char *equals = strchr(props[p], '=');
                if (!equals) continue;
                *equals = '\0';
                const char *key = props[p], *value = equals + 1;
                if (strcmp(key, "Description") == 0 && *value) {
                    g_free(row->runner); row->runner = g_strdup(value);
                } else if (strcmp(key, "ActiveState") == 0) {
                    g_free(row->service_state);
                    if (strcmp(value, "active") == 0)
                        row->service_state = g_strdup("RUNNING");
                    else
                        row->service_state = g_ascii_strup(value, -1);
                } else if (strcmp(key, "MainPID") == 0 && strcmp(value, "0") != 0) {
                    g_free(row->pid); row->pid = g_strdup(value);
                } else if (strcmp(key, "UnitFileState") == 0 && *value) {
                    g_free(row->start_mode); row->start_mode = g_strdup(value);
                } else if (strcmp(key, "User") == 0 && *value) {
                    g_free(row->account); row->account = g_strdup(value);
                } else if (strcmp(key, "ExecStart") == 0 && *value) {
                    const char *start = strstr(value, "path=");
                    if (start) start += 5;
                    else start = strchr(value, '/');
                    if (start) {
                        const char *end = start;
                        while (*end && *end != ' ' && *end != ';' && *end != '}') end++;
                        char *executable = g_strndup(start, (gsize)(end - start));
                        char *root = g_path_get_dirname(executable);
                        char *base = g_path_get_basename(root);
                        if (g_ascii_strcasecmp(base, "bin") == 0) {
                            char *parent = g_path_get_dirname(root);
                            g_free(root);
                            root = parent;
                        }
                        g_free(base);
                        char *diag = g_build_filename(root, "_diag", NULL);
                        g_free(row->path); row->path = g_strdup(root);
                        if (g_file_test(diag, G_FILE_TEST_IS_DIR)) {
                            g_free(row->diag_path);
                            row->diag_path = g_strdup(diag);
                            GDir *directory = g_dir_open(diag, 0U, NULL);
                            const char *name = NULL;
                            time_t newest_time = 0;
                            char *newest_name = NULL;
                            if (directory) {
                                while ((name = g_dir_read_name(directory)) != NULL) {
                                    if (!g_str_has_prefix(name, "Runner_") &&
                                        !g_str_has_prefix(name, "Worker_"))
                                        continue;
                                    char *candidate = g_build_filename(diag, name, NULL);
                                    GStatBuf stat_buffer;
                                    if (g_stat(candidate, &stat_buffer) == 0 &&
                                        stat_buffer.st_mtime > newest_time) {
                                        newest_time = stat_buffer.st_mtime;
                                        g_free(newest_name);
                                        newest_name = g_strdup(name);
                                    }
                                    g_free(candidate);
                                }
                                g_dir_close(directory);
                            }
                            if (newest_name) {
                                g_free(row->diag); row->diag = newest_name;
                                g_free(row->diag_age);
                                row->diag_age = duration_text(
                                    MAX(0.0, (double)time(NULL) - (double)newest_time));
                            }
                        }
                        g_free(diag); g_free(root); g_free(executable);
                    } else {
                        g_free(row->path); row->path = g_strdup(value);
                    }
                }
            }
            g_strfreev(props); g_free(show);
        }
        g_ptr_array_add(result->rows, row);
        g_strfreev(parts);
    }
    g_strfreev(lines);
    g_idle_add(local_apply_idle, result);
    return NULL;
}

static gboolean local_apply_idle(gpointer data)
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

    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (rows_changed && page == 3) {
        render_local(app);
        update_workspace_context(app, page);
    }

    g_atomic_int_set(&app->local_refreshing, 0);
    g_free(result);
    return G_SOURCE_REMOVE;
}

static void request_local_refresh(RunnerScopeApp *app)
{
    if (g_atomic_int_get(&app->shutting_down)) return;
    if (!g_atomic_int_compare_and_exchange(&app->local_refreshing, 0, 1)) return;
    app->local_thread = g_thread_new("runnerscope-local", local_worker, app);
}

static gboolean runner_timer_cb(gpointer data)
{
    request_runner_refresh(data);
    return G_SOURCE_CONTINUE;
}

static gboolean activity_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    if (!app->notebook) return G_SOURCE_CONTINUE;
    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (page == 1 || (page == 0 && app->runners_running != 0U))
        request_activity_refresh(app);
    return G_SOURCE_CONTINUE;
}

static gboolean local_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    if (app->notebook &&
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)) == 3)
        request_local_refresh(app);
    return G_SOURCE_CONTINUE;
}

static gboolean initial_activity_refresh_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    app->initial_activity_timer = 0U;
    if (!app->notebook) return G_SOURCE_REMOVE;
    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (page == 1 || (page == 0 && app->runners_running != 0U))
        request_activity_refresh(app);
    return G_SOURCE_REMOVE;
}

static void refresh_runner_clock_cells(RunnerScopeApp *app,
                                       double monotonic_now,
                                       double wall_now)
{
    for (guint i = 0U; i < app->runner_rows->len; i++) {
        RunnerRow *row = g_ptr_array_index(app->runner_rows, i);
        RunnerSession *session = g_hash_table_lookup(app->sessions, row->name);
        if (!session) continue;

        g_free(row->state_for);
        row->state_for =
            duration_text(MAX(0.0, monotonic_now - session->state_since));

        double busy_seconds = session->busy_seconds;
        if (strcmp(session->state, "RUNNING") == 0 && session->busy_started > 0.0)
            busy_seconds += monotonic_now - session->busy_started;
        const double elapsed = MAX(1.0, monotonic_now - app->session_started);
        g_free(row->busy_pct);
        row->busy_pct = g_strdup_printf(
            "%.1f%%", MIN(100.0, busy_seconds * 100.0 / elapsed));

        if (strcmp(row->state, "RUNNING") == 0) {
            JobSummary *summary = g_hash_table_lookup(app->job_by_runner, row->name);
            if (summary && summary->started_at > 0.0) {
                g_free(row->runtime);
                row->runtime =
                    duration_text(MAX(0.0, wall_now - summary->started_at));
            }
        }
    }

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(
        GTK_TREE_MODEL(app->runner_store), &iter);
    while (valid) {
        char *name = NULL;
        gtk_tree_model_get(
            GTK_TREE_MODEL(app->runner_store), &iter,
            RUNNER_COL_NAME, &name, -1);
        RunnerRow *row = g_hash_table_lookup(app->runner_row_by_name, name);
        if (row) {
            gtk_list_store_set(
                app->runner_store, &iter,
                RUNNER_COL_RUNTIME, row->runtime,
                RUNNER_COL_STATE_FOR, row->state_for,
                RUNNER_COL_BUSY, row->busy_pct, -1);
            GtkWidget *card = g_hash_table_lookup(app->runner_card_by_name, name);
            if (card && gtk_widget_get_visible(card)) update_runner_card(card, row);
            if (g_strcmp0(name, app->selected_runner_name) == 0 &&
                gtk_widget_get_visible(app->selection_card))
                show_runner_details(app, row);
        }
        g_free(name);
        valid = gtk_tree_model_iter_next(
            GTK_TREE_MODEL(app->runner_store), &iter);
    }
}

static void refresh_activity_clock_cells(RunnerScopeApp *app, double wall_now)
{
    for (guint i = 0U; i < app->activity_rows->len; i++) {
        ActivityRow *row = g_ptr_array_index(app->activity_rows, i);
        if (row->started_epoch <= 0.0) continue;
        g_free(row->runtime);
        row->runtime =
            duration_text(MAX(0.0, wall_now - row->started_epoch));
    }

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(
        GTK_TREE_MODEL(app->activity_store), &iter);
    while (valid) {
        char *url = NULL;
        gtk_tree_model_get(
            GTK_TREE_MODEL(app->activity_store), &iter,
            ACT_COL_URL, &url, -1);
        ActivityRow *row = g_hash_table_lookup(app->activity_row_by_url, url);
        if (row)
            gtk_list_store_set(
                app->activity_store, &iter,
                ACT_COL_RUNTIME, row->runtime, -1);
        g_free(url);
        valid = gtk_tree_model_iter_next(
            GTK_TREE_MODEL(app->activity_store), &iter);
    }
}

static gboolean tick_timer_cb(gpointer data)
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

    return G_SOURCE_CONTINUE;
}

static gboolean apply_filter_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    app->filter_timer = 0U;
    const gint page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    render_page(app, page);
    update_workspace_context(app, page);
    return G_SOURCE_REMOVE;
}

static void on_filter_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    RunnerScopeApp *app = user_data;
    if (app->filter_timer != 0U)
        g_source_remove(app->filter_timer);
    app->filter_timer = g_timeout_add(180U, apply_filter_cb, app);
}

static void request_refresh_for_page(RunnerScopeApp *app, gint page)
{
    if (!app) return;
    request_runner_refresh(app);
    if (page == 1 || (page == 0 && app->runners_running != 0U))
        request_activity_refresh(app);
    if (page == 3)
        request_local_refresh(app);
}

static void on_refresh(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    const gint page = app && app->notebook
        ? gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook))
        : 0;
    request_refresh_for_page(app, page);
}

static gboolean validate_org(const char *text)
{
    if (!text || !*text) return FALSE;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (!(g_ascii_isalnum(*p) || *p == '-' || *p == '_' || *p == '.'))
            return FALSE;
    return TRUE;
}

static gboolean settings_dialog(RunnerScopeApp *app, gboolean first_run)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        first_run ? "Runner Monitor setup" : "Runner Monitor settings",
        app->window ? GTK_WINDOW(app->window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const guint compact_spacing = metrics ? metrics->compact_spacing : 6U;
    const guint control_spacing = metrics ? metrics->control_spacing : 10U;
    const guint content_padding = metrics ? metrics->content_padding : 16U;
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), compact_spacing);
    gtk_grid_set_column_spacing(GTK_GRID(grid), control_spacing);
    gtk_container_set_border_width(GTK_CONTAINER(grid), content_padding);
    gtk_container_add(GTK_CONTAINER(area), grid);

    const char *labels[] = {
        "GitHub organisation", "Expected runners", "Runner refresh (seconds)",
        "Activity scan (seconds)", "Repositories to scan", "Local health (seconds)",
        "Theme"
    };
    GtkWidget *entries[6];
    char numbers[5][32];
    g_snprintf(numbers[0], sizeof(numbers[0]), "%u", app->config.expected_runners);
    g_snprintf(numbers[1], sizeof(numbers[1]), "%u", app->config.runner_poll_seconds);
    g_snprintf(numbers[2], sizeof(numbers[2]), "%u", app->config.activity_scan_seconds);
    g_snprintf(numbers[3], sizeof(numbers[3]), "%u", app->config.repository_scan_limit);
    g_snprintf(numbers[4], sizeof(numbers[4]), "%u", app->config.local_health_seconds);

    for (guint i = 0U; i < 7U; i++) {
        GtkWidget *label = gtk_label_new(labels[i]);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), label, 0, (gint)i, 1, 1);
    }
    entries[0] = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entries[0]), app->config.organisation);
    for (guint i = 1U; i < 6U; i++) {
        entries[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entries[i]), numbers[i - 1U]);
    }
    for (guint i = 0U; i < 6U; i++)
        gtk_grid_attach(GTK_GRID(grid), entries[i], 1, (gint)i, 1, 1);

    GtkWidget *theme = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(theme), "system", "Follow system");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(theme), "day", "Day");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(theme), "night", "Night");
    gtk_combo_box_set_active(GTK_COMBO_BOX(theme), (gint)app->config.theme_mode);
    gtk_grid_attach(GTK_GRID(grid), theme, 1, 6, 1, 1);

    GtkWidget *hint = gtk_label_new(
        "Authentication stays in GitHub CLI (gh auth login). Runner Monitor never stores your token.");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), hint, 0, 7, 2, 1);

    gtk_widget_show_all(dialog);
    gboolean saved = FALSE;
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *org = gtk_entry_get_text(GTK_ENTRY(entries[0]));
        if (!validate_org(org)) {
            GtkWidget *message = gtk_message_dialog_new(
                GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "Enter the GitHub organisation name only.");
            gtk_dialog_run(GTK_DIALOG(message)); gtk_widget_destroy(message);
            continue;
        }
        g_strlcpy(app->config.organisation, org, sizeof(app->config.organisation));
        app->config.expected_runners = (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[1])), NULL, 10);
        app->config.runner_poll_seconds = MAX(3U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[2])), NULL, 10));
        app->config.activity_scan_seconds = MAX(10U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[3])), NULL, 10));
        app->config.repository_scan_limit = MAX(1U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[4])), NULL, 10));
        app->config.local_health_seconds = MAX(15U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[5])), NULL, 10));
        app->config.theme_mode = (InfiltratrThemeMode)gtk_combo_box_get_active(
            GTK_COMBO_BOX(theme));
        GError *error = NULL;
        if (!save_config(&app->config, &error)) {
            GtkWidget *message = gtk_message_dialog_new(
                GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "%s", error ? error->message : "Unable to save settings");
            gtk_dialog_run(GTK_DIALOG(message)); gtk_widget_destroy(message);
            g_clear_error(&error);
            continue;
        }
        saved = TRUE;
        break;
    }
    gtk_widget_destroy(dialog);
    if (saved && app->window)
        apply_theme(app);
    return saved;
}

static void on_settings(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    if (settings_dialog(app, FALSE)) {
        if (app->runner_timer) g_source_remove(app->runner_timer);
        if (app->activity_timer) g_source_remove(app->activity_timer);
        if (app->local_timer) g_source_remove(app->local_timer);
        app->runner_timer = g_timeout_add_seconds(
            app->config.runner_poll_seconds, runner_timer_cb, app);
        app->activity_timer = g_timeout_add_seconds(
            app->config.activity_scan_seconds, activity_timer_cb, app);
        app->local_timer = g_timeout_add_seconds(
            app->config.local_health_seconds, local_timer_cb, app);
        on_refresh(NULL, app);
    }
}


static void on_system_theme_changed(
    GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    RunnerScopeApp *app = user_data;
    if (app && app->config.theme_mode == INFILTRATR_THEME_SYSTEM)
        apply_theme(app);
}

static void on_about(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    InfiltratrProjectInfo info = project_info();
    char comments[384];
    (void)g_snprintf(
        comments, sizeof(comments),
        "%s\n\nNative C / GTK 3\nInfiltratr Common %s",
        info.comments ? info.comments : "GitHub Actions self-hosted runner monitor",
        INFILTRATR_COMMON_VERSION);
    const char *authors[] = {
        "Shannon Smith — Author and project maintainer",
        NULL
    };
    gtk_show_about_dialog(
        app && app->window ? GTK_WINDOW(app->window) : NULL,
        "program-name", info.program_name,
        "version", info.version,
        "comments", comments,
        "authors", authors,
        "website", info.website,
        "website-label", "Project website",
        "copyright", info.copyright_text,
        "license-type", GTK_LICENSE_GPL_3_0,
        "logo-icon-name", "runnerscope",
        NULL);
}

static char *csv_escape(const char *input)
{
    GString *builder = g_string_new("\"");
    for (const char *cursor = input ? input : ""; *cursor; cursor++) {
        if (*cursor == '"')
            g_string_append(builder, "\"\"");
        else
            g_string_append_c(builder, *cursor);
    }
    g_string_append_c(builder, '"');
    return g_string_free(builder, FALSE);
}

static void export_model(GtkWindow *parent, GtkTreeModel *model)
{
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Export CSV", parent, GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), "runnerscope.csv");
    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gtk_widget_destroy(chooser);
    FILE *stream = fopen(filename, "wb");
    g_free(filename);
    if (!stream) return;

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    const gint columns = gtk_tree_model_get_n_columns(model);
    while (valid) {
        for (gint column = 0; column < columns; column++) {
            GValue value = G_VALUE_INIT;
            gtk_tree_model_get_value(model, &iter, column, &value);
            if (G_VALUE_HOLDS_STRING(&value)) {
                char *escaped = csv_escape(g_value_get_string(&value));
                fputs(escaped, stream);
                g_free(escaped);
            }
            g_value_unset(&value);
            if (column + 1 < columns) fputc(',', stream);
        }
        fputc('\n', stream);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    fclose(stream);
}

static void on_export(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    const gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    GtkTreeModel *model = page == 0 ? GTK_TREE_MODEL(app->runner_store)
        : page == 1 ? GTK_TREE_MODEL(app->activity_store)
        : page == 2 ? GTK_TREE_MODEL(app->history_store)
        : GTK_TREE_MODEL(app->local_store);
    export_model(GTK_WINDOW(app->window), model);
}

static gboolean selected_string(GtkWidget *tree, gint column, char **out)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return FALSE;
    gtk_tree_model_get(model, &iter, column, out, -1);
    return *out && **out;
}

static void on_open_job(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    char *url = NULL;
    if (!selected_string(app->activity_tree, ACT_COL_URL, &url)) return;
    gtk_show_uri_on_window(GTK_WINDOW(app->window), url, GDK_CURRENT_TIME, NULL);
    g_free(url);
}

static void on_open_diag(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    char *path = NULL;
    if (!selected_string(app->local_tree, LOCAL_COL_DIAG_PATH, &path)) return;
    char *uri = g_filename_to_uri(path, NULL, NULL);
    if (uri) {
        gtk_show_uri_on_window(GTK_WINDOW(app->window), uri, GDK_CURRENT_TIME, NULL);
        g_free(uri);
    }
    g_free(path);
}

static gboolean restart_apply_idle(gpointer data)
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

static void on_runner_selection(GtkTreeSelection *selection, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    if (app->runner_rendering) return;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    g_clear_pointer(&app->selected_runner_name, g_free);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        clear_selection_card(app);
        return;
    }
    gtk_tree_model_get(model, &iter, RUNNER_COL_NAME, &app->selected_runner_name, -1);
    show_runner_details(app, g_hash_table_lookup(app->runner_row_by_name, app->selected_runner_name));
}

static void on_activity_selection(GtkTreeSelection *selection, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    const gboolean selected =
        gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_widget_set_sensitive(app->open_job_button, selected);
    if (!selected) {
        clear_selection_card(app);
        return;
    }
    char *repo = NULL, *workflow = NULL, *job = NULL, *step = NULL;
    char *status = NULL, *runner = NULL;
    gtk_tree_model_get(model, &iter,
        ACT_COL_REPO, &repo,
        ACT_COL_WORKFLOW, &workflow,
        ACT_COL_JOB, &job,
        ACT_COL_STEP, &step,
        ACT_COL_STATUS, &status,
        ACT_COL_RUNNER, &runner, -1);
    char *primary = g_strdup_printf(
        "%s  •  %s", status ? status : "—", job ? job : "—");
    char *secondary = g_strdup_printf(
        "%s / %s  •  %s  •  %s",
        repo ? repo : "—", workflow ? workflow : "—",
        runner ? runner : "—", step ? step : "—");
    show_selection_card(app, "Selected job", primary, secondary);
    g_free(primary); g_free(secondary);
    g_free(repo); g_free(workflow); g_free(job); g_free(step);
    g_free(status); g_free(runner);
}

static void on_history_selection(GtkTreeSelection *selection, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        clear_selection_card(app);
        return;
    }
    char *time_text = NULL, *runner = NULL, *event = NULL, *detail = NULL;
    gtk_tree_model_get(model, &iter,
        HIST_COL_TIME, &time_text,
        HIST_COL_RUNNER, &runner,
        HIST_COL_EVENT, &event,
        HIST_COL_DETAIL, &detail, -1);
    char *primary = g_strdup_printf(
        "%s  •  %s", event ? event : "—", runner ? runner : "—");
    char *secondary = g_strdup_printf(
        "%s  •  %s", time_text ? time_text : "—", detail ? detail : "—");
    show_selection_card(app, "History event", primary, secondary);
    g_free(primary); g_free(secondary);
    g_free(time_text); g_free(runner); g_free(event); g_free(detail);
}

static void on_local_selection(GtkTreeSelection *selection, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    const gboolean selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_widget_set_sensitive(app->open_diag_button, selected);
    gtk_widget_set_sensitive(
        app->restart_button,
        selected && !g_atomic_int_get(&app->restart_refreshing));
    if (!selected) {
        clear_selection_card(app);
        return;
    }
    char *runner = NULL, *service = NULL, *github = NULL, *pid = NULL;
    char *diag = NULL, *age = NULL;
    gtk_tree_model_get(model, &iter,
        LOCAL_COL_RUNNER, &runner,
        LOCAL_COL_SERVICE, &service,
        LOCAL_COL_GITHUB, &github,
        LOCAL_COL_PID, &pid,
        LOCAL_COL_DIAG, &diag,
        LOCAL_COL_DIAG_AGE, &age, -1);
    char *primary = g_strdup_printf(
        "%s  •  service %s  •  PID %s",
        runner ? runner : "—", service ? service : "—", pid ? pid : "—");
    char *secondary = g_strdup_printf(
        "GitHub %s  •  diagnostic %s  •  age %s",
        github ? github : "—", diag ? diag : "—", age ? age : "—");
    show_selection_card(app, "Selected local runner", primary, secondary);
    g_free(primary); g_free(secondary);
    g_free(runner); g_free(service); g_free(github); g_free(pid); g_free(diag); g_free(age);
}

static void sync_navigation(RunnerScopeApp *app, gint page)
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

    const gint current_page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (current_page == page) return;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);
    sync_navigation(app, page);
    render_page(app, page);
    update_workspace_context(app, page);
    if (page == 1 || (page == 0 && app->runners_running != 0U))
        request_activity_refresh(app);
    else if (page == 3)
        request_local_refresh(app);
    clear_selection_card(app);
}

static GtkWidget *make_nav_button(RunnerScopeApp *app,
                                  gint page,
                                  const char *icon_name,
                                  const char *title,
                                  const char *tooltip)
{
    const gboolean compact = app && app->compact_layout;
    GtkWidget *button = gtk_toggle_button_new();
    GtkWidget *row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, compact ? 0 : 10);
    GtkWidget *icon = runner_asset_image_new(icon_name, 40, 40);
    GtkWidget *label = gtk_label_new(title);

    gtk_widget_set_name(button, "runner-main-nav-button");
    gtk_widget_set_size_request(
        button,
        compact ? RUNNERSCOPE_MAIN_NAV_BUTTON_COMPACT_WIDTH
                : RUNNERSCOPE_MAIN_NAV_BUTTON_WIDTH,
        RUNNERSCOPE_MAIN_NAV_BUTTON_HEIGHT);
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
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
    if (app->workspace_art_frame)
        gtk_widget_set_visible(app->workspace_art_frame, !compact);
    if (app->navigation_art_frame)
        gtk_widget_set_visible(app->navigation_art_frame, !compact);

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

static void minimize_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_iconify(GTK_WINDOW(user_data));
}

static void toggle_maximize_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWindow *window = GTK_WINDOW(user_data);
    if (gtk_window_is_maximized(window))
        gtk_window_unmaximize(window);
    else
        gtk_window_maximize(window);
}

static void close_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_close(GTK_WINDOW(user_data));
}

static GtkWidget *make_window_control(const char *icon_name,
                                      const char *tooltip,
                                      const char *css_class)
{
    GtkWidget *button =
        gtk_button_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), "runner-window-control");
    if (css_class)
        gtk_style_context_add_class(
            gtk_widget_get_style_context(button), css_class);
    gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

static GtkWidget *make_action_button(const char *icon_name,
                                     const char *label,
                                     const char *tooltip,
                                     const char *css_class)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    GtkWidget *text = gtk_label_new(label);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), text, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), row);
    gtk_style_context_add_class(gtk_widget_get_style_context(button), "runner-action-button");
    if (css_class)
        gtk_style_context_add_class(gtk_widget_get_style_context(button), css_class);
    if (tooltip && *tooltip) gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

static void build_ui(RunnerScopeApp *app)
{
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const guint compact_spacing = metrics ? metrics->compact_spacing : 6U;
    const guint control_spacing = metrics ? metrics->control_spacing : 10U;
    const guint screen_padding = metrics ? metrics->screen_padding : 20U;
    const RunnerUiPageSpec *initial_page =
        runner_ui_page(RUNNER_UI_PAGE_RUNNERS);

    app->window = gtk_application_window_new(app->application);
    enforce_required_typography(app->window);
    gtk_window_set_title(GTK_WINDOW(app->window), runner_ui_product_title());
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1280, 800);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), "runnerscope");

    /*
     * Match the current Infiltrator OS shell: product identity, settings and
     * window controls stay in the header; filtering belongs to the workspace.
     */
    GtkWidget *header = gtk_header_bar_new();
    gtk_style_context_add_class(
        gtk_widget_get_style_context(header), "runner-header");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), FALSE);
    gtk_header_bar_set_custom_title(
        GTK_HEADER_BAR(header), gtk_label_new(""));

    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(brand), "header-brand");
    GtkWidget *brand_icon_well = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(brand_icon_well), "header-brand-icon");
    GtkWidget *brand_icon =
        gtk_image_new_from_icon_name("runnerscope", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(brand_icon), 28);
    gtk_box_pack_start(
        GTK_BOX(brand_icon_well), brand_icon, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(brand), brand_icon_well, FALSE, FALSE, 0);

    GtkWidget *brand_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *brand_title = gtk_label_new(runner_ui_product_title());
    GtkWidget *brand_subtitle = gtk_label_new(runner_ui_product_family());
    gtk_style_context_add_class(
        gtk_widget_get_style_context(brand_title), "header-brand-title");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(brand_subtitle), "header-brand-subtitle");
    gtk_widget_set_halign(brand_title, GTK_ALIGN_START);
    gtk_widget_set_halign(brand_subtitle, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(brand_copy), brand_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand_copy), brand_subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand), brand_copy, FALSE, FALSE, 0);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), brand);

    GtkWidget *header_end =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);
    GtkWidget *settings_button = make_window_control(
        "preferences-system-symbolic", "Settings", NULL);
    g_signal_connect(settings_button, "clicked", G_CALLBACK(on_settings), app);
    gtk_box_pack_start(GTK_BOX(header_end), settings_button, FALSE, FALSE, 0);

    GtkWidget *window_controls =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(window_controls), "runner-window-controls");
    GtkWidget *minimize = make_window_control(
        "window-minimize-symbolic", "Minimize", NULL);
    GtkWidget *maximize = make_window_control(
        "window-maximize-symbolic", "Maximize / Restore", NULL);
    GtkWidget *close = make_window_control(
        "window-close-symbolic", "Close", "runner-window-control-close");
    g_signal_connect(minimize, "clicked", G_CALLBACK(minimize_window), app->window);
    g_signal_connect(maximize, "clicked", G_CALLBACK(toggle_maximize_window), app->window);
    g_signal_connect(close, "clicked", G_CALLBACK(close_window), app->window);
    gtk_box_pack_start(GTK_BOX(window_controls), minimize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(window_controls), maximize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(window_controls), close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_end), window_controls, FALSE, FALSE, 0);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), header_end);
    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);
    g_signal_connect(
        app->window, "configure-event",
        G_CALLBACK(on_window_configure), app);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, (gint)control_spacing);
    gtk_widget_set_name(outer, "runner-root");
    gtk_container_set_border_width(GTK_CONTAINER(outer), screen_padding);
    gtk_container_add(GTK_CONTAINER(app->window), outer);

    app->runner_store = gtk_list_store_new(RUNNER_N_COLS,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);
    app->activity_store = gtk_list_store_new(ACT_N_COLS,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);
    app->history_store = gtk_list_store_new(HIST_N_COLS,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);
    app->local_store = gtk_list_store_new(LOCAL_N_COLS,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,
        G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);

    /* GtkNotebook remains the page host; the left rail owns navigation. */
    GtkWidget *content_shell = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(content_shell, TRUE);
    gtk_widget_set_vexpand(content_shell, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(content_shell), "runner-content-shell");
    gtk_box_pack_start(GTK_BOX(outer), content_shell, TRUE, TRUE, 0);

    GtkWidget *navigation = gtk_scrolled_window_new(NULL, NULL);
    app->main_navigation = navigation;
    gtk_widget_set_name(navigation, "runner-main-navigation");
    gtk_widget_set_size_request(
        navigation, RUNNERSCOPE_MAIN_NAV_WIDTH, -1);
    gtk_widget_set_hexpand(navigation, FALSE);
    gtk_widget_set_vexpand(navigation, TRUE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(navigation),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content_shell), navigation, FALSE, TRUE, 0);

    GtkWidget *nav_rail = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(nav_rail), 8);
    gtk_container_add(GTK_CONTAINER(navigation), nav_rail);

    GtkWidget *workspace = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(workspace, TRUE);
    gtk_widget_set_vexpand(workspace, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace), "runner-workspace");
    gtk_box_pack_start(GTK_BOX(content_shell), workspace, TRUE, TRUE, 0);

    GtkWidget *workspace_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_header), "workspace-header");
    gtk_box_pack_start(GTK_BOX(workspace), workspace_header, FALSE, FALSE, 0);

    GtkWidget *workspace_icon_well = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_icon_well), "workspace-icon-well");
    app->workspace_icon = runner_asset_image_new(initial_page->nav_asset, 48, 48);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->workspace_icon), "workspace-icon");
    gtk_box_pack_start(
        GTK_BOX(workspace_icon_well), app->workspace_icon, TRUE, TRUE, 0);
    gtk_box_pack_start(
        GTK_BOX(workspace_header), workspace_icon_well, FALSE, FALSE, 0);

    GtkWidget *workspace_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(workspace_copy, TRUE);
    app->workspace_title = gtk_label_new(
        runner_ui_page_title(initial_page, RUNNER_UI_PLATFORM_LINUX));
    gtk_label_set_ellipsize(GTK_LABEL(app->workspace_title), PANGO_ELLIPSIZE_END);
    app->workspace_subtitle = gtk_label_new(initial_page->subtitle);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->workspace_title), "workspace-title");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->workspace_subtitle), "workspace-subtitle");
    gtk_widget_set_halign(app->workspace_title, GTK_ALIGN_START);
    gtk_widget_set_halign(app->workspace_subtitle, GTK_ALIGN_START);
    gtk_label_set_ellipsize(
        GTK_LABEL(app->workspace_subtitle), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(
        GTK_BOX(workspace_copy), app->workspace_title, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(workspace_copy), app->workspace_subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(workspace_header), workspace_copy, TRUE, TRUE, 0);

    GtkWidget *workspace_art_frame = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    app->workspace_art_frame = workspace_art_frame;
    gtk_widget_set_no_show_all(workspace_art_frame, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_art_frame), "workspace-art-frame");
    app->workspace_art = runner_asset_image_new(initial_page->hero_asset, 240, 86);
    gtk_box_pack_start(
        GTK_BOX(workspace_art_frame), app->workspace_art, TRUE, TRUE, 0);
    gtk_box_pack_start(
        GTK_BOX(workspace_header), workspace_art_frame, FALSE, FALSE, 0);

    GtkWidget *workspace_metrics = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(workspace_metrics), compact_spacing);
    gtk_grid_set_column_homogeneous(GTK_GRID(workspace_metrics), TRUE);
    for (guint i = 0U; i < 4U; i++) {
        GtkWidget *metric = make_workspace_metric(
            "—", &app->workspace_metric_caption[i],
            &app->workspace_metric_value[i]);
        const char *metric_classes[] = {
            "metric-one", "metric-two", "metric-three", "metric-four"
        };
        gtk_style_context_add_class(
            gtk_widget_get_style_context(metric), metric_classes[i]);
        gtk_grid_attach(GTK_GRID(workspace_metrics), metric, (gint)i, 0, 1, 1);
    }
    gtk_box_pack_end(
        GTK_BOX(workspace_header), workspace_metrics, FALSE, FALSE, 0);

    GtkWidget *workspace_toolbar =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_toolbar), "workspace-toolbar");
    app->runner_view_switch = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_no_show_all(app->runner_view_switch, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->runner_view_switch), "linked");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->runner_view_switch), "runner-view-switch");
    GtkWidget *cards_button = gtk_toggle_button_new_with_label("Cards");
    GtkWidget *table_button = gtk_toggle_button_new_with_label("Table");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cards_button), TRUE);
    g_object_set_data(G_OBJECT(cards_button), "runner-other-view", table_button);
    g_object_set_data(G_OBJECT(table_button), "runner-other-view", cards_button);
    g_object_set_data(G_OBJECT(cards_button), "runner-view", "cards");
    g_object_set_data(G_OBJECT(table_button), "runner-view", "table");
    g_signal_connect(cards_button, "toggled", G_CALLBACK(on_runner_view_toggled), app);
    g_signal_connect(table_button, "toggled", G_CALLBACK(on_runner_view_toggled), app);
    gtk_widget_set_tooltip_text(cards_button, "Visual runner overview");
    gtk_widget_set_tooltip_text(table_button, "All runner fields in a sortable table");
    gtk_box_pack_start(GTK_BOX(app->runner_view_switch), cards_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app->runner_view_switch), table_button, FALSE, FALSE, 0);
    gtk_widget_show_all(cards_button);
    gtk_widget_show_all(table_button);
    gtk_box_pack_start(GTK_BOX(workspace_toolbar), app->runner_view_switch, FALSE, FALSE, 0);
    app->filter_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(app->filter_entry), "Filter current view…");
    gtk_widget_set_size_request(app->filter_entry, 300, -1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->filter_entry), "workspace-search");
    g_signal_connect(
        app->filter_entry, "changed", G_CALLBACK(on_filter_changed), app);
    gtk_box_pack_end(
        GTK_BOX(workspace_toolbar), app->filter_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(workspace), workspace_toolbar, FALSE, FALSE, 0);

    app->notebook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(app->notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(app->notebook), FALSE);
    gtk_widget_set_hexpand(app->notebook, TRUE);
    gtk_widget_set_vexpand(app->notebook, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->notebook), "runner-notebook");
    gtk_box_pack_start(GTK_BOX(workspace), app->notebook, TRUE, TRUE, 0);

    app->selection_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_widget_set_no_show_all(app->selection_card, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->selection_card), "selection-card");
    GtkWidget *selection_icon_well =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(selection_icon_well), "selection-icon-well");
    GtkWidget *selection_icon =
        gtk_image_new_from_icon_name("dialog-information-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size(GTK_IMAGE(selection_icon), 22);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(selection_icon), "selection-icon");
    gtk_box_pack_start(
        GTK_BOX(selection_icon_well), selection_icon, TRUE, TRUE, 0);
    gtk_box_pack_start(
        GTK_BOX(app->selection_card), selection_icon_well, FALSE, FALSE, 0);
    GtkWidget *selection_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_hexpand(selection_copy, TRUE);
    app->selection_title = gtk_label_new("Selection");
    app->selection_primary = gtk_label_new("—");
    app->selection_secondary = gtk_label_new("—");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->selection_title), "selection-title");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->selection_primary), "selection-primary");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->selection_secondary), "selection-secondary");
    gtk_widget_set_halign(app->selection_title, GTK_ALIGN_START);
    gtk_widget_set_halign(app->selection_primary, GTK_ALIGN_START);
    gtk_widget_set_halign(app->selection_secondary, GTK_ALIGN_START);
    gtk_label_set_ellipsize(
        GTK_LABEL(app->selection_primary), PANGO_ELLIPSIZE_END);
    gtk_label_set_line_wrap(GTK_LABEL(app->selection_secondary), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(app->selection_secondary), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(app->selection_secondary), 110);
    gtk_box_pack_start(
        GTK_BOX(selection_copy), app->selection_title, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(selection_copy), app->selection_primary, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(selection_copy), app->selection_secondary, FALSE, FALSE, 0);
    gtk_widget_show_all(selection_icon_well);
    gtk_widget_show_all(selection_copy);
    gtk_box_pack_start(
        GTK_BOX(app->selection_card), selection_copy, TRUE, TRUE, 0);
    gtk_box_pack_end(
        GTK_BOX(workspace), app->selection_card, FALSE, FALSE, 0);

    GtkWidget *scroll = scrolled_tree(app->runner_store);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(scroll), "runner-table");
    app->runner_tree = gtk_bin_get_child(GTK_BIN(scroll));
    const char *runner_titles[] = {"Runner","OS","State","Repository","Current job",
        "Runtime","State for","Jobs","Busy","Labels"};
    const gint runner_widths[] = {165,55,70,120,180,72,72,42,48,130};
    for (gint i = 0; i < RUNNER_N_COLS; i++) {
        GtkTreeViewColumn *column =
            tree_add_text_column(app->runner_tree, runner_titles[i], i, runner_widths[i]);
        if (i == RUNNER_COL_JOB || i == RUNNER_COL_LABELS)
            gtk_tree_view_column_set_expand(column, TRUE);
    }
    GtkTreeSelection *selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(app->runner_tree));
    g_signal_connect(
        selection, "changed", G_CALLBACK(on_runner_selection), app);
    app->runner_views = gtk_stack_new();
    gtk_stack_set_hhomogeneous(GTK_STACK(app->runner_views), FALSE);
    gtk_stack_set_vhomogeneous(GTK_STACK(app->runner_views), FALSE);
    gtk_stack_add_named(GTK_STACK(app->runner_views), scroll, "table");
    GtkWidget *cards_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cards_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    app->runner_cards = gtk_flow_box_new();
    gtk_widget_set_valign(app->runner_cards, GTK_ALIGN_START);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(app->runner_cards), GTK_SELECTION_SINGLE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(app->runner_cards), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app->runner_cards), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app->runner_cards), 5);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(app->runner_cards), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(app->runner_cards), 10);
    gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(app->runner_cards), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->runner_cards), "runner-card-grid");
    app->runner_cards_empty = gtk_label_new("Loading runners…");
    gtk_widget_set_no_show_all(app->runner_cards_empty, TRUE);
    gtk_widget_set_margin_top(app->runner_cards_empty, 24);
    gtk_widget_show(app->runner_cards_empty);
    app->runner_card_art = runner_asset_pixbuf("nav-runners.png", 40, 40);
    g_signal_connect(app->runner_cards, "selected-children-changed", G_CALLBACK(on_runner_card_selection), app);
    g_signal_connect(app->runner_cards, "child-activated", G_CALLBACK(on_runner_card_activated), app);
    GtkWidget *cards_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(cards_content), app->runner_cards_empty, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cards_content), app->runner_cards, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(cards_scroll), cards_content);
    gtk_stack_add_named(GTK_STACK(app->runner_views), cards_scroll, "cards");
    gtk_stack_set_visible_child_name(GTK_STACK(app->runner_views), "cards");
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), app->runner_views, NULL);

    scroll = scrolled_tree(app->activity_store);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(scroll), "runner-table");
    app->activity_tree = gtk_bin_get_child(GTK_BIN(scroll));
    const char *act_titles[] = {"Where","Repository","Workflow","Job","Current step",
        "Status","Runner","Runtime","Event","Branch"};
    const gint act_widths[] = {70,105,125,150,135,78,135,70,60,85};
    for (gint i = 0; i < 10; i++) {
        GtkTreeViewColumn *column =
            tree_add_text_column(app->activity_tree, act_titles[i], i, act_widths[i]);
        if (i == ACT_COL_JOB || i == ACT_COL_STEP)
            gtk_tree_view_column_set_expand(column, TRUE);
    }
    gtk_notebook_append_page(
        GTK_NOTEBOOK(app->notebook), scroll, NULL);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->activity_tree));
    g_signal_connect(
        selection, "changed", G_CALLBACK(on_activity_selection), app);

    scroll = scrolled_tree(app->history_store);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(scroll), "runner-table");
    app->history_tree = gtk_bin_get_child(GTK_BIN(scroll));
    tree_add_text_column(app->history_tree, "Time", HIST_COL_TIME, 100);
    tree_add_text_column(app->history_tree, "Runner", HIST_COL_RUNNER, 155);
    tree_add_text_column(app->history_tree, "Event", HIST_COL_EVENT, 90);
    GtkTreeViewColumn *history_detail =
        tree_add_text_column(app->history_tree, "Detail", HIST_COL_DETAIL, 260);
    gtk_tree_view_column_set_expand(history_detail, TRUE);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->history_tree));
    g_signal_connect(
        selection, "changed", G_CALLBACK(on_history_selection), app);
    gtk_notebook_append_page(
        GTK_NOTEBOOK(app->notebook), scroll, NULL);

    scroll = scrolled_tree(app->local_store);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(scroll), "runner-table");
    app->local_tree = gtk_bin_get_child(GTK_BIN(scroll));
    const char *local_titles[] = {"Runner","Service","GitHub","PID","Start","Account",
        "Latest diagnostic","Age","Path"};
    const gint local_widths[] = {175,72,72,52,65,82,155,58,160};
    for (gint i = 0; i < 9; i++) {
        GtkTreeViewColumn *column =
            tree_add_text_column(app->local_tree, local_titles[i], i, local_widths[i]);
        if (i == LOCAL_COL_PATH)
            gtk_tree_view_column_set_expand(column, TRUE);
    }
    gtk_notebook_append_page(
        GTK_NOTEBOOK(app->notebook), scroll, NULL);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->local_tree));
    g_signal_connect(
        selection, "changed", G_CALLBACK(on_local_selection), app);

    for (gint i = 0; i < RUNNER_UI_PAGE_COUNT; i++) {
        const RunnerUiPageSpec *ui_page =
            runner_ui_page((RunnerUiPageId)i);
        if (ui_page->separator_before) {
            GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_style_context_add_class(
                gtk_widget_get_style_context(separator),
                "runner-main-nav-separator");
            gtk_box_pack_start(GTK_BOX(nav_rail), separator, FALSE, FALSE, 5);
        }
        GtkWidget *nav_button = make_nav_button(
            app, i, ui_page->nav_asset,
            runner_ui_page_nav_label(ui_page, RUNNER_UI_PLATFORM_LINUX),
            ui_page->tooltip);
        gtk_box_pack_start(
            GTK_BOX(nav_rail), nav_button, FALSE, TRUE, 0);
    }
    sync_navigation(app, RUNNER_UI_PAGE_RUNNERS);

    GtkWidget *nav_art_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    app->navigation_art_frame = nav_art_frame;
    gtk_widget_set_no_show_all(nav_art_frame, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(nav_art_frame), "nav-art-frame");
    GtkWidget *nav_art = runner_asset_image_new("nav-infiltrator.png", 160, 200);
    gtk_widget_show(nav_art);
    gtk_box_pack_start(GTK_BOX(nav_art_frame), nav_art, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(nav_rail), nav_art_frame, FALSE, FALSE, 8);

    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_VERTICAL, (gint)compact_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(footer), "footer-card");
    gtk_box_pack_start(GTK_BOX(outer), footer, FALSE, FALSE, 0);

    GtkWidget *action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_widget_set_name(action_row, "footer-actions");
    gtk_box_pack_start(GTK_BOX(footer), action_row, FALSE, FALSE, 0);

    GtkWidget *context_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);
    gtk_box_pack_start(GTK_BOX(action_row), context_actions, FALSE, FALSE, 0);

    app->open_job_button = make_action_button("emblem-web-symbolic", "Open selected job", "Open the selected workflow job in GitHub.", NULL);
    gtk_widget_set_sensitive(app->open_job_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->open_job_button, "Select an active job to open it in GitHub.");
    g_signal_connect(
        app->open_job_button, "clicked", G_CALLBACK(on_open_job), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->open_job_button, FALSE, FALSE, 0);

    app->open_diag_button = make_action_button("folder-open-symbolic", "Open diagnostic", "Open the selected runner diagnostic.", NULL);
    gtk_widget_set_sensitive(app->open_diag_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->open_diag_button,
        "Select a local runner to open its latest diagnostic.");
    g_signal_connect(
        app->open_diag_button, "clicked", G_CALLBACK(on_open_diag), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->open_diag_button, FALSE, FALSE, 0);

    app->restart_button = make_action_button("view-refresh-symbolic", "Restart selected runner", "Restart the selected local runner service.", "runner-action-warning");
    gtk_widget_set_sensitive(app->restart_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->restart_button,
        "Select a local runner before restarting its service.");
    g_signal_connect(
        app->restart_button, "clicked", G_CALLBACK(on_restart), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->restart_button, FALSE, FALSE, 0);

    GtkWidget *general_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)compact_spacing);
    gtk_box_pack_end(GTK_BOX(action_row), general_actions, FALSE, FALSE, 0);

    GtkWidget *button = make_action_button("document-save-symbolic", "Export CSV", "Export the current workspace as CSV.", NULL);
    g_signal_connect(button, "clicked", G_CALLBACK(on_export), app);
    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);

    button = make_action_button("help-about-symbolic", "About", "About Runner Monitor.", NULL);
    g_signal_connect(button, "clicked", G_CALLBACK(on_about), app);
    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);

    button = make_action_button("view-refresh-symbolic", "Refresh now", "Refresh the active workspace now.", "runner-action-primary");
    g_signal_connect(button, "clicked", G_CALLBACK(on_refresh), app);
    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);

    GtkWidget *status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_box_pack_start(GTK_BOX(footer), status_row, FALSE, FALSE, 0);
    app->status_label = gtk_label_new("Starting…");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->status_label), "status-text");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(app->status_label, TRUE);
    gtk_label_set_ellipsize(
        GTK_LABEL(app->status_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(
        GTK_BOX(status_row), app->status_label, TRUE, TRUE, 0);

    GtkWidget *version_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(version_box, GTK_ALIGN_END);
    char *app_version =
        g_strdup_printf("Runner Monitor %s", RUNNERSCOPE_VERSION);
    GtkWidget *app_version_label = gtk_label_new(app_version);
    g_free(app_version);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app_version_label), "version-text");
    gtk_widget_set_halign(app_version_label, GTK_ALIGN_END);
    gtk_box_pack_start(
        GTK_BOX(version_box), app_version_label, FALSE, FALSE, 0);

    char *common_version =
        g_strdup_printf("Common %s", INFILTRATR_COMMON_VERSION);
    GtkWidget *common_version_label = gtk_label_new(common_version);
    g_free(common_version);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(common_version_label), "version-text");
    gtk_widget_set_halign(common_version_label, GTK_ALIGN_END);
    gtk_box_pack_start(
        GTK_BOX(version_box), common_version_label, FALSE, FALSE, 0);
    gtk_box_pack_end(
        GTK_BOX(status_row), version_box, FALSE, FALSE, 0);

    update_workspace_context(app, 0);
    clear_selection_card(app);
    apply_theme(app);
    GtkSettings *settings = gtk_settings_get_default();
    if (settings) {
        g_signal_connect(
            settings, "notify::gtk-application-prefer-dark-theme",
            G_CALLBACK(on_system_theme_changed), app);
        g_signal_connect(
            settings, "notify::gtk-theme-name",
            G_CALLBACK(on_system_theme_changed), app);
    }
    gtk_widget_show_all(app->window);
    gtk_stack_set_visible_child_name(GTK_STACK(app->runner_views), "cards");
    gtk_widget_show_all(app->workspace_art);
    gtk_widget_set_visible(app->workspace_art_frame, !app->compact_layout);
    gtk_widget_set_visible(app->navigation_art_frame, !app->compact_layout);
    clear_selection_card(app);
}

static InfiltratrProjectInfo project_info(void)
{
    InfiltratrProjectInfo info = INFILTRATR_PROJECT_INFO_INIT;
    info.program_name = runner_ui_product_title();
    info.executable_name = "runnerscope";
    info.application_id = RUNNERSCOPE_APP_ID;
    info.version = RUNNERSCOPE_VERSION;
    info.source_id = "Infiltrator-Projects/RunnerScope";
    info.build_profile = "native-c-gtk";
    info.author = "Shannon Smith";
    info.website = "https://github.com/Infiltrator-Projects/RunnerScope";
    info.license_id = "GPL-3.0-or-later";
    info.comments = "Native GitHub Actions self-hosted runner monitor";
    info.icon_name = "runnerscope";
    info.copyright_text = "Copyright (c) 2026 Shannon Smith";
    return info;
}

static int self_test(void)
{
    char ui_error[256];
    if (!runner_ui_contract_validate(ui_error, sizeof(ui_error))) {
        fprintf(stderr, "Shared UI contract failed: %s\n", ui_error);
        return 1;
    }
    if (!INFILTRATR_COMMON_VERSION[0]) return 2;
    const InfiltratrThemePalette *day =
        infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false);
    const InfiltratrThemePalette *night =
        infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true);
    if (!day || !night || day->background_rgb == night->background_rgb)
        return 3;
    char duration[64];
    if (!infiltratr_format_duration_compact(true, 3661U, duration, sizeof(duration)))
        return 4;
    if (strstr(duration, "1h") == NULL) return 5;
    printf("Runner Monitor %s native C/Common self-test passed (Common %s)\n",
           RUNNERSCOPE_VERSION, INFILTRATR_COMMON_VERSION);
    return 0;
}

static void activate(GtkApplication *application, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    app->application = application;
    if (app->window) {
        gtk_window_present(GTK_WINDOW(app->window));
        return;
    }
    if (!app->config.organisation[0]) {
        /* A temporary hidden parent lets first-run setup remain native GTK. */
        app->window = gtk_application_window_new(application);
        gtk_widget_hide(app->window);
        if (!settings_dialog(app, TRUE)) {
            gtk_widget_destroy(app->window);
            app->window = NULL;
            return;
        }
        gtk_widget_destroy(app->window);
        app->window = NULL;
    }

    build_ui(app);
    request_runner_refresh(app);
    app->initial_activity_timer =
        g_timeout_add(1200U, initial_activity_refresh_cb, app);
    app->runner_timer = g_timeout_add_seconds(
        app->config.runner_poll_seconds, runner_timer_cb, app);
    app->activity_timer = g_timeout_add_seconds(
        app->config.activity_scan_seconds, activity_timer_cb, app);
    app->local_timer = g_timeout_add_seconds(
        app->config.local_health_seconds, local_timer_cb, app);
    app->tick_timer = g_timeout_add_seconds(4U, tick_timer_cb, app);
}

static void app_init(RunnerScopeApp *app)
{
    memset(app, 0, sizeof(*app));
    load_config(&app->config);
    app->sessions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, session_free);
    app->job_by_runner = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, job_summary_free);
    app->runner_row_by_name = g_hash_table_new(g_str_hash, g_str_equal);
    app->runner_card_by_name = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    app->activity_row_by_url = g_hash_table_new(g_str_hash, g_str_equal);
    app->runner_rows = g_ptr_array_new_with_free_func(runner_row_free);
    app->activity_rows = g_ptr_array_new_with_free_func(activity_row_free);
    app->history_rows = g_ptr_array_new_with_free_func(history_row_free);
    app->local_rows = g_ptr_array_new_with_free_func(local_row_free);
    app->session_started = now_monotonic();
    load_history(app);
}

static void app_destroy(RunnerScopeApp *app)
{
    g_atomic_int_set(&app->shutting_down, 1);
    if (app->runner_timer) g_source_remove(app->runner_timer);
    if (app->activity_timer) g_source_remove(app->activity_timer);
    if (app->local_timer) g_source_remove(app->local_timer);
    if (app->tick_timer) g_source_remove(app->tick_timer);
    if (app->initial_activity_timer)
        g_source_remove(app->initial_activity_timer);
    if (app->filter_timer) {
        g_source_remove(app->filter_timer);
        app->filter_timer = 0U;
    }
    if (app->history_save_timer) {
        g_source_remove(app->history_save_timer);
        app->history_save_timer = 0U;
        persist_history(app);
    }
    if (app->runner_thread) {
        g_thread_join(app->runner_thread);
        app->runner_thread = NULL;
    }
    if (app->activity_thread) {
        g_thread_join(app->activity_thread);
        app->activity_thread = NULL;
    }
    if (app->local_thread) {
        g_thread_join(app->local_thread);
        app->local_thread = NULL;
    }
    if (app->restart_thread) {
        g_thread_join(app->restart_thread);
        app->restart_thread = NULL;
    }
    if (app->theme_provider) {
        GdkScreen *screen = gdk_screen_get_default();
        if (screen) {
            gtk_style_context_remove_provider_for_screen(
                screen, GTK_STYLE_PROVIDER(app->theme_provider));
        }
        g_clear_object(&app->theme_provider);
    }
    g_hash_table_unref(app->sessions);
    g_hash_table_unref(app->job_by_runner);
    g_hash_table_unref(app->runner_row_by_name);
    g_hash_table_unref(app->runner_card_by_name);
    g_clear_object(&app->runner_card_art);
    g_free(app->selected_runner_name);
    g_hash_table_unref(app->activity_row_by_url);
    g_ptr_array_unref(app->runner_rows);
    g_ptr_array_unref(app->activity_rows);
    g_ptr_array_unref(app->history_rows);
    g_ptr_array_unref(app->local_rows);
}

/* Exercise the installed graphical shell without provider or service calls. */
static int ui_self_test(int *argc, char ***argv)
{
    if (!gtk_init_check(argc, argv)) return 1;
    RunnerScopeApp app;
    app_init(&app);
    app.application = gtk_application_new("net.ssmith.runnerscope.ui-test", G_APPLICATION_NON_UNIQUE);
    if (!g_application_register(G_APPLICATION(app.application), NULL, NULL)) return 2;
    build_ui(&app);
    const char *states[] = {"IDLE", "RUNNING", "OFFLINE"};
    for (guint i = 0U; i < 3U; i++) {
        RunnerRow *row = g_new0(RunnerRow, 1U);
        row->name = g_strdup_printf("Fixture-%u", i);
        row->os = g_strdup(i == 2U ? "Windows" : "Linux");
        row->state = g_strdup(states[i]);
        row->repo = g_strdup("Example/Project");
        row->job = g_strdup("Build");
        row->runtime = g_strdup("2m");
        row->state_for = g_strdup("3m");
        row->jobs = g_strdup("1");
        row->busy_pct = g_strdup("25.0%");
        row->labels = g_strdup("self-hosted,fixture-label");
        g_ptr_array_add(app.runner_rows, row);
    }
    rebuild_runner_row_index(&app);
    render_runners(&app);
    g_assert_cmpstr(gtk_stack_get_visible_child_name(GTK_STACK(app.runner_views)), ==, "cards");
    g_assert_false(gtk_widget_get_visible(app.selection_card));
    g_assert_cmpuint(g_hash_table_size(app.runner_card_by_name), ==, 3U);
    g_assert_cmpint(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(app.runner_store), NULL), ==, 3);
    GtkWidget *idle = g_hash_table_lookup(app.runner_card_by_name, "Fixture-0");
    GtkWidget *busy = g_hash_table_lookup(app.runner_card_by_name, "Fixture-1");
    GtkWidget *offline = g_hash_table_lookup(app.runner_card_by_name, "Fixture-2");
    g_assert_false(gtk_widget_get_visible(g_object_get_data(G_OBJECT(idle), "runner-job")));
    g_assert_true(gtk_widget_get_visible(g_object_get_data(G_OBJECT(busy), "runner-job")));
    g_assert_true(gtk_style_context_has_class(gtk_widget_get_style_context(
        g_object_get_data(G_OBJECT(offline), "runner-state")), "offline"));
    GtkWidget *bar = g_object_get_data(G_OBJECT(busy), "runner-utilisation");
    g_assert_cmpfloat(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(bar)), ==, 0.25);
    gtk_flow_box_select_child(GTK_FLOW_BOX(app.runner_cards), GTK_FLOW_BOX_CHILD(busy));
    g_assert_true(gtk_widget_get_visible(app.selection_card));
    g_assert_true(gtk_widget_get_visible(app.selection_primary));
    g_assert_true(gtk_widget_get_visible(app.selection_secondary));
    g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app.selection_secondary)), "fixture-label"));
    render_runners(&app);
    g_assert_true(busy == g_hash_table_lookup(app.runner_card_by_name, "Fixture-1"));
    g_assert_cmpstr(app.selected_runner_name, ==, "Fixture-1");
    g_assert_true(gtk_widget_get_visible(app.selection_card));
    gtk_entry_set_text(GTK_ENTRY(app.filter_entry), "fixture-label");
    render_runners(&app);
    g_assert_true(gtk_widget_get_visible(idle));
    gtk_entry_set_text(GTK_ENTRY(app.filter_entry), "Windows");
    render_runners(&app);
    g_assert_false(gtk_widget_get_visible(idle));
    g_assert_true(gtk_widget_get_visible(offline));
    g_assert_false(gtk_widget_get_visible(app.selection_card));
    gtk_entry_set_text(GTK_ENTRY(app.filter_entry), "no-match");
    render_runners(&app);
    g_assert_true(gtk_widget_get_visible(app.runner_cards_empty));
    gtk_entry_set_text(GTK_ENTRY(app.filter_entry), "");
    render_runners(&app);
    g_assert_false(gtk_widget_get_visible(app.runner_cards_empty));
    gtk_stack_set_visible_child_name(GTK_STACK(app.runner_views), "table");
    GtkTreeIter first;
    g_assert_true(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app.runner_store), &first));
    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(app.runner_tree)), &first);
    g_assert_cmpstr(app.selected_runner_name, ==, "Fixture-0");
    g_ptr_array_remove_index(app.runner_rows, 0U);
    rebuild_runner_row_index(&app);
    render_runners(&app);
    g_assert_false(g_hash_table_contains(app.runner_card_by_name, "Fixture-0"));
    g_assert_false(gtk_widget_get_visible(app.selection_card));
    g_assert_cmpint(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(app.runner_store), NULL), ==, 2);
    gtk_widget_destroy(app.window);
    g_object_unref(app.application);
    app_destroy(&app);
    puts("Runner cards UI contract passed: state, utilisation, details, filtering, reuse, selection and table.");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts(RUNNERSCOPE_VERSION);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--common-version") == 0) {
        puts(INFILTRATR_COMMON_VERSION);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--project-info") == 0) {
        InfiltratrProjectInfo info = project_info();
        return infiltratr_project_info_print(stdout, &info) == 0 ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
        return self_test();
    if (argc == 2 && strcmp(argv[1], "--ui-self-test") == 0) {
        argc = 1;
        return ui_self_test(&argc, &argv);
    }

    RunnerScopeApp app;
    app_init(&app);
    GtkApplication *application = gtk_application_new(
        RUNNERSCOPE_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &app);
    const int status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    app_destroy(&app);
    return status;
}
