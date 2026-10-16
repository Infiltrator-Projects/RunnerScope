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
#include "ui/ui_renderer.h"
#include "core/runner_config.h"
#include "core/github.h"
#include "core/github_auth.h"
#include "platform/linux_services.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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
    GtkWidget *connection_bar;
    GtkWidget *connection_label;
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
    RunnerThemeMode theme_state_mode;
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
    char *config_error;
    char *runner_error;
    gboolean runner_data_stale;
    gboolean activity_data_stale;
    char *activity_error;
    RunnerHttpGet fetch_transport;
    void *fetch_context;
    guint config_generation;
    GCancellable *service_cancel;
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
    guint generation;
    GPtrArray *rows;
    char *error;
} RunnerRefreshResult;

typedef struct {
    RunnerScopeApp *app;
    guint generation;
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

typedef struct {
    RunnerScopeApp *app;
    RunnerConfig config;
    guint generation;
    RunnerHttpGet get;
    void *context;
    gint64 deadline;
} ProviderRequest;

static gboolean activity_apply_idle(gpointer data);
static gboolean local_apply_idle(gpointer data);
static void request_activity_refresh(RunnerScopeApp *app);
static void request_runner_refresh(RunnerScopeApp *app);
static void on_settings(GtkButton *button, gpointer user_data);
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

static gboolean load_config(RunnerConfig *config, GError **error)
{
    runner_config_defaults(config);
    char *path = config_path();
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    g_free(path);
    if (descriptor < 0) {
        if (errno == ENOENT) return TRUE;
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
            "Unable to read saved settings: %s. The original file has been kept.", g_strerror(errno));
        return FALSE;
    }
    char contents[65538];
    size_t used = 0U;
    gboolean okay = TRUE;
    GStatBuf status;
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size > 65536) okay = FALSE;
    while (okay && used < sizeof(contents) - 1U) {
        const ssize_t count = read(descriptor, contents + used, sizeof(contents) - 1U - used);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { okay = FALSE; break; }
        if (count == 0) break;
        used += (size_t)count;
    }
    (void)close(descriptor);
    if (!okay || used > 65536U || memchr(contents, '\0', used)) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
            "Unable to read saved settings: expected a JSON file of at most 64 KiB without embedded NUL bytes. The original file has been kept.");
        memset(contents, 0, sizeof(contents));
        return FALSE;
    }
    contents[used] = '\0';
    char detail[256] = "";
    okay = runner_config_parse_json(contents, config, detail, sizeof(detail));
    memset(contents, 0, sizeof(contents));
    if (!okay) g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Unable to read saved settings: %s. The original file has been kept.", detail);
    return okay;
}

static gboolean save_config(const RunnerConfig *config, GError **error)
{
    char json[8192];
    char detail[256] = "";
    size_t written = 0U;
    if (!runner_config_write_json(config, json, sizeof(json), &written, detail, sizeof(detail))) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", detail);
        return FALSE;
    }
    char *path = config_path();
    char *directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
            "Unable to create settings directory: %s", g_strerror(errno));
        g_free(directory); g_free(path);
        memset(json, 0, sizeof(json));
        return FALSE;
    }
    g_free(directory);
    /* Always replace with a private file, including upgrades from old 0644 JSON. */
    const int failure = infiltratr_atomic_file_write_bytes(path,
        INFILTRATR_ATOMIC_FILE_PRIVATE, json, written);
    memset(json, 0, sizeof(json));
    g_free(path);
    if (failure) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
            "Unable to save settings: %s", g_strerror(failure));
        return FALSE;
    }
    return TRUE;
}

static InfiltratrThemeMode common_theme_mode(RunnerThemeMode mode)
{
    switch (mode) {
        case RUNNER_THEME_DAY: return INFILTRATR_THEME_DAY;
        case RUNNER_THEME_NIGHT: return INFILTRATR_THEME_NIGHT;
        default: return INFILTRATR_THEME_SYSTEM;
    }
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
        app->config.theme_mode == RUNNER_THEME_NIGHT ||
        (app->config.theme_mode == RUNNER_THEME_SYSTEM && system_dark);
    if (app->theme_state_valid &&
        app->theme_state_mode == app->config.theme_mode &&
        app->theme_state_dark == night_theme)
        return;

    const InfiltratrThemePalette *p =
        infiltratr_theme_resolve(common_theme_mode(app->config.theme_mode), system_dark);
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

static bool provider_http_get(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    ProviderRequest *request = context;
    if (g_atomic_int_get(&request->app->shutting_down) ||
        g_get_monotonic_time() >= request->deadline) {
        memset(response, 0, sizeof(*response));
        g_strlcpy(error, "GitHub request cancelled or timed out. Try refreshing again.", error_size);
        return false;
    }
    return request->get(url, token, response, error, error_size, request->context);
}

static char *github_api(const char *endpoint, ProviderRequest *request,
    RunnerGithubProjection projection, GError **error)
{
    char detail[512] = "";
    char *output = runner_github_fetch(endpoint, request->config.github_token, projection,
        provider_http_get, request, detail, sizeof(detail));
    if (!output) g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
        "%s", *detail ? detail : "Unable to contact GitHub");
    return output;
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

typedef struct {
    RunnerScopeApp *app;
    GtkWidget *workspace;
    guint compact_spacing;
    guint control_spacing;
} GtkWorkspaceRendererContext;

static bool gtk_renderer_begin_workspace_summary(
    void *context, const RunnerUiPageSpec *page, const char *platform_title)
{
    GtkWorkspaceRendererContext *renderer = context;
    if (!renderer || !renderer->app || !page || !platform_title) return false;
    RunnerScopeApp *app = renderer->app;

    if (app->workspace_title && app->workspace_subtitle &&
        app->workspace_icon && app->workspace_art) {
        runner_asset_image_set(app->workspace_icon, page->nav_asset, 48, 48);
        runner_asset_image_set(app->workspace_art, page->hero_asset, 240, 86);
        label_set_if_changed(app->workspace_title, platform_title);
        label_set_if_changed(app->workspace_subtitle, page->subtitle);
        return true;
    }
    if (!renderer->workspace) return false;

    GtkWidget *workspace_header = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->control_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_header), "workspace-header");
    gtk_box_pack_start(
        GTK_BOX(renderer->workspace), workspace_header, FALSE, FALSE, 0);

    GtkWidget *workspace_icon_well = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_icon_well), "workspace-icon-well");
    app->workspace_icon = runner_asset_image_new(page->nav_asset, 48, 48);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->workspace_icon), "workspace-icon");
    gtk_box_pack_start(
        GTK_BOX(workspace_icon_well), app->workspace_icon, TRUE, TRUE, 0);
    gtk_box_pack_start(
        GTK_BOX(workspace_header), workspace_icon_well, FALSE, FALSE, 0);

    GtkWidget *workspace_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(workspace_copy, TRUE);
    app->workspace_title = gtk_label_new(platform_title);
    gtk_label_set_ellipsize(
        GTK_LABEL(app->workspace_title), PANGO_ELLIPSIZE_END);
    app->workspace_subtitle = gtk_label_new(page->subtitle);
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
    gtk_box_pack_start(
        GTK_BOX(workspace_header), workspace_copy, TRUE, TRUE, 0);

    GtkWidget *workspace_art_frame = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    app->workspace_art_frame = workspace_art_frame;
    gtk_widget_set_no_show_all(workspace_art_frame, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(workspace_art_frame), "workspace-art-frame");
    app->workspace_art = runner_asset_image_new(page->hero_asset, 240, 86);
    gtk_box_pack_start(
        GTK_BOX(workspace_art_frame), app->workspace_art, TRUE, TRUE, 0);
    gtk_box_pack_start(
        GTK_BOX(workspace_header), workspace_art_frame, FALSE, FALSE, 0);

    GtkWidget *workspace_metrics = gtk_grid_new();
    gtk_grid_set_column_spacing(
        GTK_GRID(workspace_metrics), renderer->compact_spacing);
    gtk_grid_set_column_homogeneous(GTK_GRID(workspace_metrics), TRUE);
    const char *metric_classes[] = {
        "metric-one", "metric-two", "metric-three", "metric-four"
    };
    for (guint i = 0U; i < 4U; ++i) {
        GtkWidget *metric = make_workspace_metric(
            "—", &app->workspace_metric_caption[i],
            &app->workspace_metric_value[i]);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(metric), metric_classes[i]);
        gtk_grid_attach(
            GTK_GRID(workspace_metrics), metric, (gint)i, 0, 1, 1);
    }
    gtk_box_pack_end(
        GTK_BOX(workspace_header), workspace_metrics, FALSE, FALSE, 0);
    return true;
}

static bool gtk_renderer_workspace_metric(void *context, size_t index,
                                          const char *caption,
                                          const char *value)
{
    GtkWorkspaceRendererContext *renderer = context;
    if (!renderer || !renderer->app || index >= 4U || !caption || !value)
        return false;
    RunnerScopeApp *app = renderer->app;
    if (!app->workspace_metric_caption[index] ||
        !app->workspace_metric_value[index])
        return false;
    label_set_if_changed(app->workspace_metric_caption[index], caption);
    label_set_if_changed(app->workspace_metric_value[index], value);
    return true;
}

static bool gtk_renderer_end_workspace_summary(void *context)
{
    return context != NULL;
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

static RunnerUiRunnerItem runner_ui_item_from_row(const RunnerRow *row)
{
    RunnerUiRunnerItem item = {0};
    if (!row) return item;
    item.name = row->name;
    item.os = row->os;
    item.state = row->state;
    item.repo = row->repo;
    item.job = row->job;
    item.runtime = row->runtime;
    item.state_for = row->state_for;
    item.jobs = row->jobs;
    item.busy_pct = row->busy_pct;
    item.labels = row->labels;
    item.busy_fraction = CLAMP(
        g_ascii_strtod(row->busy_pct ? row->busy_pct : "0", NULL) / 100.0,
        0.0, 1.0);
    return item;
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
    RunnerUiRunnerItem item = runner_ui_item_from_row(row);
    RunnerUiRunnerSelectionSpec selection;
    if (!runner_ui_prepare_runner_selection(&item, &selection)) return;
    show_selection_card(app, selection.title,
                        selection.primary, selection.secondary);
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

static const char *runner_tone_css(RunnerUiRunnerTone tone)
{
    switch (tone) {
        case RUNNER_UI_RUNNER_RUNNING: return "running";
        case RUNNER_UI_RUNNER_IDLE: return "idle";
        case RUNNER_UI_RUNNER_OFFLINE: return "offline";
        default: return "offline";
    }
}

static GtkWidget *make_runner_card(RunnerScopeApp *app,
                                   const RunnerUiRunnerCardSpec *spec)
{
    const RunnerUiRunnerItem *item = spec->item;
    GtkWidget *child = gtk_flow_box_child_new();
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(card, 205, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "runner-card");
    gtk_container_add(GTK_CONTAINER(child), card);
    g_object_set_data_full(
        G_OBJECT(child), "runner-name", g_strdup(item->name), g_free);

    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *art = gtk_image_new_from_pixbuf(app->runner_card_art);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name = runner_card_label(item->name, "runner-card-name");
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(name), 15);
    GtkWidget *os = runner_card_label(item->os, "runner-card-os");
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
    gtk_style_context_add_class(
        gtk_widget_get_style_context(bar), "runner-utilisation");
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

static void update_runner_card_spec(
    GtkWidget *child, const RunnerUiRunnerCardSpec *spec)
{
    const RunnerUiRunnerItem *item = spec->item;
    GtkWidget *state = g_object_get_data(G_OBJECT(child), "runner-state");
    GtkWidget *bar = g_object_get_data(G_OBJECT(child), "runner-utilisation");
    GtkWidget *job = g_object_get_data(G_OBJECT(child), "runner-job");
    GtkWidget *os = g_object_get_data(G_OBJECT(child), "runner-os");
    label_set_if_changed(os, item->os);
    label_set_if_changed(state, spec->state_label);
    GtkStyleContext *context = gtk_widget_get_style_context(state);
    const char *state_class = runner_tone_css(spec->tone);
    if (!gtk_style_context_has_class(context, state_class)) {
        gtk_style_context_remove_class(context, "running");
        gtk_style_context_remove_class(context, "idle");
        gtk_style_context_remove_class(context, "offline");
        gtk_style_context_add_class(context, state_class);
    }
    if (gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(bar)) != spec->busy_fraction)
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(bar), spec->busy_fraction);
    char *tooltip = g_strdup_printf(
        "%s\nSession utilisation: %s", item->name, item->busy_pct);
    gtk_widget_set_tooltip_text(child, tooltip);
    gtk_widget_set_tooltip_text(bar, tooltip);
    g_free(tooltip);
    label_set_if_changed(job, item->job);
    gtk_widget_show_all(child);
    gtk_widget_set_visible(job, spec->show_job);
}

static void update_runner_card(GtkWidget *child, const RunnerRow *row)
{
    RunnerUiRunnerItem item = runner_ui_item_from_row(row);
    RunnerUiRunnerCardSpec spec;
    if (runner_ui_prepare_runner_card(&item, FALSE, &spec))
        update_runner_card_spec(child, &spec);
}

typedef struct {
    RunnerScopeApp *app;
    bool selection_emitted;
} GtkRunnerCardsRendererContext;

static bool gtk_renderer_begin_runner_cards(void *context, size_t item_count)
{
    (void)item_count;
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app) return false;
    renderer->selection_emitted = false;
    gtk_widget_hide(renderer->app->runner_cards_empty);
    return true;
}

static bool gtk_renderer_runner_card(
    void *context, size_t index, const RunnerUiRunnerCardSpec *spec)
{
    (void)index;
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app || !spec || !spec->item) return false;
    RunnerScopeApp *app = renderer->app;
    GtkWidget *card = g_hash_table_lookup(
        app->runner_card_by_name, spec->item->name);
    if (!card) {
        card = make_runner_card(app, spec);
        g_hash_table_insert(
            app->runner_card_by_name, g_strdup(spec->item->name), card);
    }
    update_runner_card_spec(card, spec);
    if (spec->selected) {
        renderer->selection_emitted = true;
        gtk_flow_box_select_child(
            GTK_FLOW_BOX(app->runner_cards), GTK_FLOW_BOX_CHILD(card));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(
            GTK_TREE_MODEL(app->runner_store), &iter);
        while (valid) {
            char *name = NULL;
            gtk_tree_model_get(
                GTK_TREE_MODEL(app->runner_store), &iter,
                RUNNER_COL_NAME, &name, -1);
            const gboolean match =
                g_strcmp0(name, spec->item->name) == 0;
            g_free(name);
            if (match) {
                gtk_tree_selection_select_iter(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(app->runner_tree)),
                    &iter);
                break;
            }
            valid = gtk_tree_model_iter_next(
                GTK_TREE_MODEL(app->runner_store), &iter);
        }
    }
    return true;
}

static bool gtk_renderer_runner_cards_empty(void *context, const char *message)
{
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app || !message) return false;
    label_set_if_changed(renderer->app->runner_cards_empty, message);
    gtk_widget_show(renderer->app->runner_cards_empty);
    return true;
}

static bool gtk_renderer_runner_selection(
    void *context, const RunnerUiRunnerSelectionSpec *selection)
{
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app || !selection) return false;
    renderer->selection_emitted = true;
    show_selection_card(renderer->app, selection->title,
                        selection->primary, selection->secondary);
    return true;
}

static bool gtk_renderer_end_runner_cards(void *context)
{
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app) return false;
    if (!renderer->selection_emitted) {
        g_clear_pointer(&renderer->app->selected_runner_name, g_free);
        gtk_flow_box_unselect_all(GTK_FLOW_BOX(renderer->app->runner_cards));
        clear_selection_card(renderer->app);
    }
    return true;
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

static void update_connection_banner(RunnerScopeApp *app)
{
    if (!app->connection_label) return;
    char *notice = NULL;
    if (app->runner_data_stale) {
        notice = g_strdup_printf("%s%s", app->runner_error ? app->runner_error :
            app->config.github_token[0] ? "Checking GitHub runner access…" : "Sign in with GitHub in Settings.",
            app->runner_rows->len ? " Displayed runners are last known data." : " Runner status is unavailable.");
    } else if (app->activity_data_stale && app->activity_error) {
        notice = g_strdup_printf("%s Displayed activity is last known data.", app->activity_error);
    }
    if (notice) {
        label_set_if_changed(app->connection_label, notice);
        gtk_widget_show(app->connection_bar);
        g_free(notice);
    } else gtk_widget_hide(app->connection_bar);
}

static void update_workspace_context(RunnerScopeApp *app, gint page)
{
    if (!app || page < 0 || page >= RUNNER_UI_PAGE_COUNT) return;
    update_connection_banner(app);

    char metric_values[4][64] = {{0}};
    switch (page) {
        case RUNNER_UI_PAGE_RUNNERS:
            if (app->runner_data_stale) {
                for (guint i = 0U; i < 4U; ++i) g_strlcpy(metric_values[i], "—", sizeof(metric_values[i]));
                break;
            }
            g_snprintf(metric_values[0], sizeof(metric_values[0]), "%u", app->runners_total);
            g_snprintf(metric_values[1], sizeof(metric_values[1]), "%u", app->runners_running);
            g_snprintf(metric_values[2], sizeof(metric_values[2]), "%u", app->runners_idle);
            g_snprintf(metric_values[3], sizeof(metric_values[3]), "%u", app->runners_offline);
            break;
        case RUNNER_UI_PAGE_ACTIVE_JOBS:
            if (app->activity_data_stale) {
                for (guint i = 0U; i < 4U; ++i) g_strlcpy(metric_values[i], "—", sizeof(metric_values[i]));
                break;
            }
            g_snprintf(metric_values[0], sizeof(metric_values[0]), "%u", app->local_active);
            g_snprintf(metric_values[1], sizeof(metric_values[1]), "%u", app->hosted_active);
            g_snprintf(metric_values[2], sizeof(metric_values[2]), "%u", app->queued);
            g_snprintf(metric_values[3], sizeof(metric_values[3]), "%u",
                       app->local_active + app->hosted_active);
            break;
        case RUNNER_UI_PAGE_HISTORY: {
            g_snprintf(metric_values[0], sizeof(metric_values[0]), "%u",
                       app->history_rows->len);
            g_snprintf(metric_values[1], sizeof(metric_values[1]), "%u",
                       g_hash_table_size(app->sessions));
            char *uptime_text = duration_text(
                now_monotonic() - app->session_started);
            g_strlcpy(metric_values[2], uptime_text ? uptime_text : "—",
                      sizeof(metric_values[2]));
            g_free(uptime_text);
            const char *filter =
                gtk_entry_get_text(GTK_ENTRY(app->filter_entry));
            g_strlcpy(metric_values[3], filter && *filter ? "ACTIVE" : "ALL",
                      sizeof(metric_values[3]));
            break;
        }
        case RUNNER_UI_PAGE_LOCAL_HEALTH:
            g_snprintf(metric_values[0], sizeof(metric_values[0]), "%u",
                       app->local_rows->len);
            g_snprintf(metric_values[1], sizeof(metric_values[1]), "%u",
                       count_local_state(app, "RUNNING"));
            g_snprintf(metric_values[2], sizeof(metric_values[2]), "%u",
                       count_local_github(app));
            g_snprintf(metric_values[3], sizeof(metric_values[3]), "%u",
                       count_local_diagnostics(app));
            break;
        default:
            return;
    }

    RunnerUiWorkspaceSummaryState summary = {
        (RunnerUiPageId)page,
        {metric_values[0], metric_values[1], metric_values[2], metric_values[3]}
    };
    GtkWorkspaceRendererContext workspace_context = {0};
    workspace_context.app = app;
    RunnerUiRenderer workspace_renderer = {0};
    workspace_renderer.context = &workspace_context;
    workspace_renderer.begin_workspace_summary =
        gtk_renderer_begin_workspace_summary;
    workspace_renderer.workspace_metric = gtk_renderer_workspace_metric;
    workspace_renderer.end_workspace_summary =
        gtk_renderer_end_workspace_summary;
    if (!runner_ui_render_workspace_summary(
            &workspace_renderer, RUNNER_UI_PLATFORM_LINUX, &summary))
        return;

    const RunnerUiPageSpec *ui_page = runner_ui_page((RunnerUiPageId)page);
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

    GArray *items = g_array_new(FALSE, FALSE, sizeof(RunnerUiRunnerItem));
    ptrdiff_t selected_index = -1;
    tree_model_rebuild_begin(app->runner_tree);
    gtk_list_store_clear(app->runner_store);
    for (guint i = 0U; i < app->runner_rows->len; i++) {
        RunnerRow *row = g_ptr_array_index(app->runner_rows, i);
        char *search = g_strdup_printf(
            "%s %s %s %s %s %s", row->name, row->os,
            row->state, row->repo, row->job, row->labels);
        const gboolean visible = text_matches_folded(folded_needle, search);
        g_free(search);
        if (!visible) continue;

        RunnerUiRunnerItem item = runner_ui_item_from_row(row);
        if (g_strcmp0(row->name, app->selected_runner_name) == 0)
            selected_index = (ptrdiff_t)items->len;
        g_array_append_val(items, item);

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

    GtkRunnerCardsRendererContext context = {app, false};
    RunnerUiRenderer renderer = {0};
    renderer.context = &context;
    renderer.begin_runner_cards = gtk_renderer_begin_runner_cards;
    renderer.runner_card = gtk_renderer_runner_card;
    renderer.runner_cards_empty = gtk_renderer_runner_cards_empty;
    renderer.runner_selection = gtk_renderer_runner_selection;
    renderer.end_runner_cards = gtk_renderer_end_runner_cards;
    RunnerUiRunnerCardsState state = {
        (const RunnerUiRunnerItem *)items->data,
        items->len,
        selected_index,
        g_atomic_int_get(&app->runner_refreshing) != 0,
        folded_needle && *folded_needle
    };
    if (!runner_ui_render_runner_cards(&renderer, &state))
        g_warning("Unable to render the shared runner-card contract.");

    g_array_unref(items);
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
    if (g_atomic_int_get(&app->shutting_down) || result->generation != app->config_generation) {
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->runner_refreshing, 0);
        g_free(result);
        if (!g_atomic_int_get(&app->shutting_down)) request_runner_refresh(app);
        return G_SOURCE_REMOVE;
    }
    if (result->error) {
        app->runner_data_stale = TRUE;
        g_free(app->runner_error);
        app->runner_error = g_strdup(result->error);
        char *notice = g_strdup_printf("%s%s", result->error,
            app->runner_rows->len ? " Displayed runners are last known data." : " Runner status is unavailable.");
        label_set_if_changed(app->connection_label, notice);
        g_free(notice);
        gtk_widget_show(app->connection_bar);
        label_set_if_changed(app->status_label, "Runner data unavailable — check Settings");
        update_workspace_context(app, gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->runner_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    app->runner_data_stale = FALSE;
    g_clear_pointer(&app->runner_error, g_free);
    update_connection_banner(app);
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
    ProviderRequest *request = data;
    RunnerScopeApp *app = request->app;
    const RunnerConfig *config = &request->config;
    RunnerRefreshResult *result = g_new0(RunnerRefreshResult, 1U);
    result->app = app;
    result->generation = request->generation;
    result->rows = g_ptr_array_new_with_free_func(raw_runner_free);

    char *endpoint = g_strdup_printf("/orgs/%s/actions/runners?per_page=100",
                                     config->organisation);
    GError *error = NULL;
    char *output = github_api(endpoint, request, RUNNER_GITHUB_RUNNERS, &error);
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
    g_free(request);
    g_idle_add(runner_apply_idle, result);
    return NULL;
}

static void request_runner_refresh(RunnerScopeApp *app)
{
    if (!app->config.organisation[0] || g_atomic_int_get(&app->shutting_down)) return;
    if (!app->config.github_token[0]) {
        app->runner_data_stale = TRUE;
        label_set_if_changed(app->connection_label, "Sign in with GitHub in Settings to view organisation runners.");
        gtk_widget_show(app->connection_bar);
        label_set_if_changed(app->status_label, "Sign in with GitHub — open Settings");
        update_workspace_context(app, gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));
        return;
    }
    if (!g_atomic_int_compare_and_exchange(&app->runner_refreshing, 0, 1)) return;
    ProviderRequest *request = g_new0(ProviderRequest, 1U);
    request->app = app;
    request->config = app->config;
    request->generation = app->config_generation;
    request->get = app->fetch_transport;
    request->context = app->fetch_context;
    request->deadline = g_get_monotonic_time() + 60000000;
    app->runner_thread = g_thread_new("runnerscope-runners", runner_worker, request);
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
    ProviderRequest *request = data;
    RunnerScopeApp *app = request->app;
    const RunnerConfig *config = &request->config;
    ActivityRefreshResult *result = g_new0(ActivityRefreshResult, 1U);
    result->app = app;
    result->generation = request->generation;
    result->rows = g_ptr_array_new_with_free_func(activity_row_free);

    char *repo_endpoint = g_strdup_printf(
        "/orgs/%s/repos?type=all&sort=pushed&direction=desc&per_page=100",
        config->organisation);
    GError *error = NULL;
    char *repo_output = github_api(repo_endpoint, request, RUNNER_GITHUB_REPOSITORIES, &error);
    g_free(repo_endpoint);
    if (!repo_output) {
        result->error = g_strdup_printf("Activity scan error: %s",
            error ? error->message : "unable to list repositories");
        g_clear_error(&error);
        g_free(request);
        g_idle_add(activity_apply_idle, result);
        return NULL;
    }

    char **repos = g_strsplit(repo_output, "\n", -1);
    g_free(repo_output);
    guint scanned = 0U;
    for (guint r = 0U; repos[r] && scanned < config->repository_scan_limit; r++) {
        if (g_atomic_int_get(&app->shutting_down) || g_get_monotonic_time() >= request->deadline) {
            result->error = g_strdup("Activity scan cancelled or timed out. Try refreshing again.");
            break;
        }
        if (!*repos[r]) continue;
        scanned++;
        char *runs_endpoint = g_strdup_printf(
            "/repos/%s/%s/actions/runs?per_page=100&exclude_pull_requests=true",
            config->organisation, repos[r]);
        char *runs_output = github_api(runs_endpoint, request, RUNNER_GITHUB_RUNS, &error);
        g_free(runs_endpoint);
        if (!runs_output) {
            result->error = g_strdup_printf("Activity scan failed: %s", error ? error->message : "unable to read workflow runs");
            g_clear_error(&error);
            break;
        }
        GPtrArray *runs = split_tsv_lines(runs_output);
        g_free(runs_output);

        for (guint i = 0U; i < runs->len; i++) {
            if (g_atomic_int_get(&app->shutting_down) || g_get_monotonic_time() >= request->deadline) {
                result->error = g_strdup("Activity scan cancelled or timed out. Try refreshing again.");
                break;
            }
            char **run_fields = g_ptr_array_index(runs, i);
            const char *run_id = field(run_fields, 0U);
            char *jobs_endpoint = g_strdup_printf(
                "/repos/%s/%s/actions/runs/%s/jobs?per_page=100&filter=latest",
                config->organisation, repos[r], run_id);
            char *jobs_output = github_api(jobs_endpoint, request, RUNNER_GITHUB_JOBS, &error);
            g_free(jobs_endpoint);
            if (!jobs_output) {
                result->error = g_strdup_printf("Activity scan failed: %s", error ? error->message : "unable to read jobs");
                g_clear_error(&error);
                break;
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
        if (result->error) break;
    }
    if (!result->error && g_get_monotonic_time() >= request->deadline)
        result->error = g_strdup("Activity scan timed out. Try refreshing again.");
    g_strfreev(repos);
    g_free(request);
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
    if (g_atomic_int_get(&app->shutting_down) || result->generation != app->config_generation) {
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->activity_refreshing, 0);
        g_free(result);
        if (!g_atomic_int_get(&app->shutting_down)) request_activity_refresh(app);
        return G_SOURCE_REMOVE;
    }
    if (result->error) {
        app->activity_data_stale = TRUE;
        g_free(app->activity_error);
        app->activity_error = g_strdup(result->error);
        update_connection_banner(app);
        update_workspace_context(app, gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));

        if (!app->runner_data_stale) label_set_if_changed(app->status_label, result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->activity_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    app->activity_data_stale = FALSE;
    g_clear_pointer(&app->activity_error, g_free);
    update_connection_banner(app);
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
    if (page == 1) {
        if (rows_changed) render_activity(app);
        update_workspace_context(app, page);
    }

    g_atomic_int_set(&app->activity_refreshing, 0);
    g_free(result);
    return G_SOURCE_REMOVE;
}

static void request_activity_refresh(RunnerScopeApp *app)
{
    if (!app->config.organisation[0] || g_atomic_int_get(&app->shutting_down)) return;
    if (!app->config.github_token[0]) return;
    if (!g_atomic_int_compare_and_exchange(&app->activity_refreshing, 0, 1)) return;
    ProviderRequest *request = g_new0(ProviderRequest, 1U);
    request->app = app;
    request->config = app->config;
    request->generation = app->config_generation;
    request->get = app->fetch_transport;
    request->context = app->fetch_context;
    request->deadline = g_get_monotonic_time() + 60000000;
    app->activity_thread = g_thread_new("runnerscope-activity", activity_worker, request);
}

static void find_runner_diagnostics(LocalRow *row, const char *executable)
{
    if (!executable || !g_path_is_absolute(executable)) return;
    char *root = g_path_get_dirname(executable);
    char *base = g_path_get_basename(root);
    if (g_ascii_strcasecmp(base, "bin") == 0) {
        char *parent = g_path_get_dirname(root);
        g_free(root); root = parent;
    }
    g_free(base);
    g_free(row->path); row->path = g_strdup(root);
    char *diag = g_build_filename(root, "_diag", NULL);
    if (g_file_test(diag, G_FILE_TEST_IS_DIR)) {
        g_free(row->diag_path); row->diag_path = g_strdup(diag);
        GDir *directory = g_dir_open(diag, 0U, NULL);
        const char *name = NULL;
        time_t newest_time = 0;
        char *newest_name = NULL;
        guint examined = 0U;
        while (directory && examined++ < 10000U && (name = g_dir_read_name(directory))) {
            if (!g_str_has_prefix(name, "Runner_") && !g_str_has_prefix(name, "Worker_")) continue;
            char *candidate = g_build_filename(diag, name, NULL);
            GStatBuf stat_buffer;
            if (g_stat(candidate, &stat_buffer) == 0 && S_ISREG(stat_buffer.st_mode) &&
                stat_buffer.st_mtime > newest_time) {
                newest_time = stat_buffer.st_mtime;
                g_free(newest_name); newest_name = g_strdup(name);
            }
            g_free(candidate);
        }
        if (directory) g_dir_close(directory);
        if (newest_name) {
            g_free(row->diag); row->diag = newest_name;
            g_free(row->diag_age);
            row->diag_age = duration_text(MAX(0.0, (double)time(NULL) - (double)newest_time));
        }
    }
    g_free(diag); g_free(root);
}

static gpointer local_worker(gpointer data)
{
    RunnerScopeApp *app = data;
    LocalRefreshResult *result = g_new0(LocalRefreshResult, 1U);
    result->app = app;
    result->rows = g_ptr_array_new_with_free_func(local_row_free);
    GError *error = NULL;
    GPtrArray *services = runner_linux_services_list(app->service_cancel, &error);
    if (!services) {
        result->error = g_strdup_printf("Local service health error: %s",
            error ? error->message : "system service manager unavailable");
        g_clear_error(&error);
    } else {
        for (guint i = 0U; i < services->len && !g_atomic_int_get(&app->shutting_down); ++i) {
            const RunnerLinuxService *service = g_ptr_array_index(services, i);
            LocalRow *row = g_new0(LocalRow, 1U);
            row->service_name = g_strdup(service->name);
            row->runner = g_strdup(service->description);
            row->github_state = g_strdup("—");
            row->service_state = strcmp(service->active_state, "active") == 0
                ? g_strdup("RUNNING") : g_ascii_strup(service->active_state, -1);
            row->pid = service->pid ? g_strdup_printf("%u", service->pid) : g_strdup("—");
            row->start_mode = g_strdup(service->unit_file_state);
            row->account = g_strdup(service->user);
            row->diag = g_strdup("—"); row->diag_age = g_strdup("—");
            row->path = g_strdup("—"); row->diag_path = g_strdup("");
            find_runner_diagnostics(row, service->executable);
            g_ptr_array_add(result->rows, row);
        }
        g_ptr_array_unref(services);
    }
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
    if (g_atomic_int_get(&app->shutting_down)) {
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->local_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }
    if (result->error) {
        if (!app->runner_data_stale) label_set_if_changed(app->status_label, result->error);
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
    if (page == 0 && !app->runner_data_stale) {
        const double monotonic_now = now_monotonic();
        refresh_runner_clock_cells(
            app, monotonic_now, (double)time(NULL));
    } else if (page == 1 && !app->activity_data_stale) {
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

static gboolean entry_uint(GtkWidget *entry, unsigned int *value)
{
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!*text) return FALSE;
    for (const char *p = text; *p; ++p) if (!g_ascii_isdigit(*p)) return FALSE;
    char *end = NULL;
    errno = 0;
    const guint64 parsed = g_ascii_strtoull(text, &end, 10);
    if (errno || !end || *end || parsed > G_MAXUINT) return FALSE;
    *value = (unsigned int)parsed;
    return TRUE;
}

#include "linux_auth.inc"

static gboolean settings_dialog(RunnerScopeApp *app, gboolean first_run)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        first_run ? "Runner Monitor setup" : "Runner Monitor settings",
        app->window ? GTK_WINDOW(app->window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save and test", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), metrics ? metrics->compact_spacing : 6U);
    gtk_grid_set_column_spacing(GTK_GRID(grid), metrics ? metrics->control_spacing : 10U);
    gtk_container_set_border_width(GTK_CONTAINER(grid), metrics ? metrics->content_padding : 16U);
    gtk_container_add(GTK_CONTAINER(area), grid);
    const char *labels[] = {"GitHub organisation", "GitHub account", "Expected runners",
        "Runner refresh (seconds)", "Activity scan (seconds)", "Repositories to scan",
        "Local health (seconds)", "Theme"};
    GtkWidget *entries[7] = {NULL};
    const unsigned int values[] = {app->config.expected_runners, app->config.runner_poll_seconds,
        app->config.activity_scan_seconds, app->config.repository_scan_limit, app->config.local_health_seconds};
    for (guint i = 0U; i < G_N_ELEMENTS(labels); ++i) {
        GtkWidget *label = gtk_label_new(labels[i]);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), label, 0, (gint)i, 1, 1);
        if (i < G_N_ELEMENTS(entries) && i != 1U) {
            entries[i] = gtk_entry_new();
            gtk_entry_set_max_length(GTK_ENTRY(entries[i]), i == 1U ? (gint)RUNNER_GITHUB_TOKEN_CAPACITY - 1 : i == 0U ? 127 : 10);
            gtk_grid_attach(GTK_GRID(grid), entries[i], 1, (gint)i, 1, 1);
        }
    }
    gtk_entry_set_text(GTK_ENTRY(entries[0]), app->config.organisation);
    gtk_widget_set_tooltip_text(entries[0], "Your saved GitHub organisation is filled in automatically. Keep it unless you want to monitor another organisation.");
    for (guint i = 2U; i < G_N_ELEMENTS(entries); ++i) {
        char number[32]; g_snprintf(number, sizeof(number), "%u", values[i - 2U]);
        gtk_entry_set_text(GTK_ENTRY(entries[i]), number);
    }
    GtkWidget *theme = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(theme), "system", "Follow system");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(theme), "day", "Day");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(theme), "night", "Night");
    gtk_combo_box_set_active(GTK_COMBO_BOX(theme), (gint)app->config.theme_mode);
    gtk_grid_attach(GTK_GRID(grid), theme, 1, 7, 1, 1);
    GtkWidget *hint = gtk_label_new(
        "Sign in through GitHub in your browser. Enter the one-time code shown above,\n"
        "then approve Runner Monitor. Your account is remembered on this computer.");
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(hint), 65);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0F);
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), hint, 0, 8, 2, 1);
    GtkWidget *error_label = gtk_label_new(app->config_error ? app->config_error : "");
    gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(error_label), 65);
    gtk_widget_set_halign(error_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), error_label, 0, 10, 2, 1);
    GithubSignIn *sign_in = github_sign_in_create(dialog, grid, error_label);
    g_object_set_data(G_OBJECT(dialog), "organisation-entry", entries[0]);
    g_object_set_data(G_OBJECT(dialog), "settings-error", error_label);
    g_object_set_data(G_OBJECT(dialog), "sign-in-help", hint);
    gtk_widget_show_all(dialog);
    gboolean saved = FALSE;
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        RunnerConfig candidate = app->config;
        g_strlcpy(candidate.organisation, gtk_entry_get_text(GTK_ENTRY(entries[0])), sizeof(candidate.organisation));
        if (sign_in->auth.phase == RUNNER_AUTH_COMPLETE)
            g_strlcpy(candidate.github_token, sign_in->auth.token, sizeof(candidate.github_token));
        unsigned int *fields[] = {&candidate.expected_runners, &candidate.runner_poll_seconds,
            &candidate.activity_scan_seconds, &candidate.repository_scan_limit, &candidate.local_health_seconds};
        gboolean valid_numbers = TRUE;
        for (guint i = 0U; i < G_N_ELEMENTS(fields); ++i)
            if (!entry_uint(entries[i + 2U], fields[i])) valid_numbers = FALSE;
        candidate.theme_mode = (RunnerThemeMode)gtk_combo_box_get_active(GTK_COMBO_BOX(theme));
        char detail[256] = "";
        if (!valid_numbers || !candidate.organisation[0] ||
            !runner_config_validate(&candidate, detail, sizeof(detail)) || !candidate.github_token[0]) {
            label_set_if_changed(error_label, !valid_numbers ? "Enter valid whole numbers for the polling settings."
                : !candidate.organisation[0] ? "Enter your GitHub organisation name."
                : !candidate.github_token[0] ? "Select Sign in with GitHub to connect." : detail);
            memset(candidate.github_token, 0, sizeof(candidate.github_token));
            continue;
        }
        GError *error = NULL;
        if (!save_config(&candidate, &error)) {
            label_set_if_changed(error_label, error ? error->message : "Unable to save settings");
            g_clear_error(&error);
            memset(candidate.github_token, 0, sizeof(candidate.github_token));
            continue;
        }
        const gboolean organisation_changed = strcmp(app->config.organisation, candidate.organisation) != 0;
        app->config = candidate;
        memset(candidate.github_token, 0, sizeof(candidate.github_token));
        app->config_generation++;
        app->runner_data_stale = TRUE;
        app->activity_data_stale = TRUE;
        g_clear_pointer(&app->activity_error, g_free);
        g_clear_pointer(&app->runner_error, g_free);
        g_clear_pointer(&app->config_error, g_free);
        if (organisation_changed) {
            g_hash_table_remove_all(app->sessions);
            g_hash_table_remove_all(app->job_by_runner);
            g_ptr_array_set_size(app->runner_rows, 0U);
            g_ptr_array_set_size(app->activity_rows, 0U);
            rebuild_runner_row_index(app); rebuild_activity_row_index(app);
            if (app->runner_cards) render_runners(app);
            if (app->activity_tree) render_activity(app);
        }
        if (app->connection_label) {
            label_set_if_changed(app->connection_label, "Settings saved. Checking GitHub runner access…");
            gtk_widget_show(app->connection_bar);
            update_workspace_context(app, gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));
        }
        saved = TRUE;
        break;
    }
    github_sign_in_close(sign_in);
    gtk_widget_destroy(dialog);
    if (saved && app->window) apply_theme(app);
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
    if (app && app->config.theme_mode == RUNNER_THEME_SYSTEM)
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

typedef struct {
    gint references;
    GtkWidget *dialog;
    GtkTextBuffer *buffer;
    GInputStream *stream;
    GCancellable *cancel;
    char *data;
} DiagnosticViewer;

#define DIAGNOSTIC_LIMIT (2U * 1024U * 1024U)

static void diagnostic_viewer_free(gpointer data)
{
    DiagnosticViewer *viewer = data;
    if (!g_atomic_int_dec_and_test(&viewer->references)) return;
    g_clear_object(&viewer->buffer); g_clear_object(&viewer->stream);
    g_clear_object(&viewer->cancel); g_free(viewer->data); g_free(viewer);
}

static void diagnostic_destroyed(GtkWidget *dialog, gpointer data)
{
    (void)dialog;
    DiagnosticViewer *viewer = data;
    viewer->dialog = NULL;
    g_cancellable_cancel(viewer->cancel);
}

static void diagnostic_response(GtkDialog *dialog, gint response, gpointer data)
{
    (void)response; (void)data;
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void diagnostic_read_complete(GObject *source, GAsyncResult *result, gpointer data)
{
    DiagnosticViewer *viewer = data;
    GError *error = NULL;
    gsize length = 0U;
    const gboolean okay = g_input_stream_read_all_finish(G_INPUT_STREAM(source), result, &length, &error);
    if (viewer->dialog) {
        if (!okay) gtk_text_buffer_set_text(viewer->buffer,
            error ? error->message : "Unable to read diagnostic", -1);
        else {
            char *valid = g_utf8_make_valid(viewer->data, (gssize)MIN(length, DIAGNOSTIC_LIMIT));
            gtk_text_buffer_set_text(viewer->buffer, valid, -1);
            if (length > DIAGNOSTIC_LIMIT) {
                GtkTextIter end;
                gtk_text_buffer_get_end_iter(viewer->buffer, &end);
                gtk_text_buffer_insert(viewer->buffer, &end, "\n\nShowing the first 2 MiB of this diagnostic.", -1);
            }
            g_free(valid);
        }
    }
    g_clear_error(&error);
    diagnostic_viewer_free(viewer);
}

static void diagnostic_open_complete(GObject *source, GAsyncResult *result, gpointer data)
{
    DiagnosticViewer *viewer = data;
    GError *error = NULL;
    viewer->stream = G_INPUT_STREAM(g_file_read_finish(G_FILE(source), result, &error));
    if (!viewer->stream) {
        if (viewer->dialog) gtk_text_buffer_set_text(viewer->buffer,
            error ? error->message : "Unable to open diagnostic", -1);
        g_clear_error(&error);
        diagnostic_viewer_free(viewer);
        return;
    }
    viewer->data = g_malloc(DIAGNOSTIC_LIMIT + 1U);
    g_input_stream_read_all_async(viewer->stream, viewer->data, DIAGNOSTIC_LIMIT + 1U,
        G_PRIORITY_DEFAULT, viewer->cancel, diagnostic_read_complete, viewer);
}

static void on_open_diag(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    char *directory = NULL, *name = NULL;
    if (!selected_string(app->local_tree, LOCAL_COL_DIAG_PATH, &directory)) return;
    if (!selected_string(app->local_tree, LOCAL_COL_DIAG, &name) || strcmp(name, "—") == 0) {
        g_free(directory); g_free(name);
        label_set_if_changed(app->status_label, "No diagnostic file is available for this runner.");
        return;
    }
    char *path = g_build_filename(directory, name, NULL);
    GtkWidget *dialog = gtk_dialog_new_with_buttons(name, GTK_WINDOW(app->window),
        GTK_DIALOG_DESTROY_WITH_PARENT, "_Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 900, 600);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text), TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), text);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), scroll, TRUE, TRUE, 0);
    DiagnosticViewer *viewer = g_new0(DiagnosticViewer, 1U);
    viewer->references = 2;
    viewer->dialog = dialog;
    viewer->buffer = g_object_ref(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)));
    viewer->cancel = g_cancellable_new();
    gtk_text_buffer_set_text(viewer->buffer, "Loading diagnostic…", -1);
    g_object_set_data_full(G_OBJECT(dialog), "diagnostic-viewer", viewer, diagnostic_viewer_free);
    g_signal_connect(dialog, "destroy", G_CALLBACK(diagnostic_destroyed), viewer);
    g_signal_connect(dialog, "response", G_CALLBACK(diagnostic_response), NULL);
    gtk_widget_show_all(dialog);
    GFile *file = g_file_new_for_path(path);
    g_file_read_async(file, G_PRIORITY_DEFAULT, viewer->cancel, diagnostic_open_complete, viewer);
    g_object_unref(file);
    g_free(path); g_free(directory); g_free(name);
}

static gboolean restart_apply_idle(gpointer data)
{
    RestartResult *result = data;
    RunnerScopeApp *app = result->app;

    if (app->restart_thread) {
        g_thread_unref(app->restart_thread);
        app->restart_thread = NULL;
    }

    if (g_atomic_int_get(&app->shutting_down)) {
        g_free(result->service); g_free(result->error); g_free(result);
        return G_SOURCE_REMOVE;
    }
    if (result->error) {
        char *message = g_strdup_printf(
            "Runner restart failed: %s", result->error);
        label_set_if_changed(app->status_label, message);
        g_free(message);
    } else {
        label_set_if_changed(app->status_label, "Runner service restart requested.");
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
    GError *error = NULL;
    if (!runner_linux_service_restart(result->service, result->app->service_cancel, &error)) {
        result->error = g_strdup(error ? error->message : "Unable to restart runner service");
        g_clear_error(&error);
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
    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT) return;

    const gint current_page =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    RunnerUiPageTransition transition;
    if (!runner_ui_plan_page_transition(
            (RunnerUiPageId)current_page,
            (RunnerUiPageId)page,
            app->runners_running != 0U,
            false,
            &transition) ||
        !transition.changed)
        return;

    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(app->notebook), (gint)transition.requested_page);
    sync_navigation(app, (gint)transition.requested_page);
    render_page(app, (gint)transition.requested_page);
    update_workspace_context(app, (gint)transition.requested_page);
    if (transition.request_activity_refresh)
        request_activity_refresh(app);
    else if (transition.request_local_refresh)
        request_local_refresh(app);
    if (transition.clear_selection)
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


typedef struct {
    RunnerScopeApp *app;
    GtkWidget *outer;
    GtkWidget *header;
    GtkWidget *header_end;
    GtkWidget *window_controls;
    GtkWidget *footer_context_actions;
    GtkWidget *footer_general_actions;
    GtkWidget *footer_status_row;
    guint compact_spacing;
    guint control_spacing;
} GtkChromeRendererContext;

static bool gtk_renderer_begin_header(void *context,
                                      const char *product_title,
                                      const char *product_family)
{
    GtkChromeRendererContext *renderer = context;
    if (!renderer || !renderer->app || !renderer->app->window ||
        !product_title || !product_family)
        return false;

    renderer->header = gtk_header_bar_new();
    gtk_style_context_add_class(
        gtk_widget_get_style_context(renderer->header), "runner-header");
    gtk_header_bar_set_show_close_button(
        GTK_HEADER_BAR(renderer->header), FALSE);
    gtk_header_bar_set_custom_title(
        GTK_HEADER_BAR(renderer->header), gtk_label_new(""));

    GtkWidget *brand = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->control_spacing);
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
    GtkWidget *brand_title = gtk_label_new(product_title);
    GtkWidget *brand_subtitle = gtk_label_new(product_family);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(brand_title), "header-brand-title");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(brand_subtitle), "header-brand-subtitle");
    gtk_widget_set_halign(brand_title, GTK_ALIGN_START);
    gtk_widget_set_halign(brand_subtitle, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(brand_copy), brand_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand_copy), brand_subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand), brand_copy, FALSE, FALSE, 0);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(renderer->header), brand);

    renderer->header_end = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->compact_spacing);
    renderer->window_controls = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->compact_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(renderer->window_controls),
        "runner-window-controls");
    return true;
}

static bool gtk_renderer_header_action(void *context,
                                       RunnerUiChromeActionId action,
                                       const char *label,
                                       const char *tooltip,
                                       bool primary)
{
    GtkChromeRendererContext *renderer = context;
    (void)label;
    (void)primary;
    if (!renderer || !renderer->app || !renderer->header_end ||
        !renderer->window_controls || !tooltip)
        return false;

    GtkWidget *button = NULL;
    switch (action) {
    case RUNNER_UI_CHROME_SETTINGS:
        button = make_window_control(
            "preferences-system-symbolic", tooltip, NULL);
        g_signal_connect(
            button, "clicked", G_CALLBACK(on_settings), renderer->app);
        gtk_box_pack_start(
            GTK_BOX(renderer->header_end), button, FALSE, FALSE, 0);
        return true;
    case RUNNER_UI_CHROME_MINIMIZE:
        button = make_window_control("window-minimize-symbolic", tooltip, NULL);
        g_signal_connect(
            button, "clicked", G_CALLBACK(minimize_window), renderer->app->window);
        break;
    case RUNNER_UI_CHROME_MAXIMIZE:
        button = make_window_control("window-maximize-symbolic", tooltip, NULL);
        g_signal_connect(
            button, "clicked", G_CALLBACK(toggle_maximize_window), renderer->app->window);
        break;
    case RUNNER_UI_CHROME_CLOSE:
        button = make_window_control(
            "window-close-symbolic", tooltip, "runner-window-control-close");
        g_signal_connect(
            button, "clicked", G_CALLBACK(close_window), renderer->app->window);
        break;
    default:
        return false;
    }
    gtk_box_pack_start(
        GTK_BOX(renderer->window_controls), button, FALSE, FALSE, 0);
    return true;
}

static bool gtk_renderer_end_header(void *context)
{
    GtkChromeRendererContext *renderer = context;
    if (!renderer || !renderer->app || !renderer->header ||
        !renderer->header_end || !renderer->window_controls)
        return false;
    gtk_box_pack_start(
        GTK_BOX(renderer->header_end), renderer->window_controls,
        FALSE, FALSE, 0);
    gtk_header_bar_pack_end(
        GTK_HEADER_BAR(renderer->header), renderer->header_end);
    gtk_window_set_titlebar(
        GTK_WINDOW(renderer->app->window), renderer->header);
    return true;
}

static bool gtk_renderer_begin_footer(void *context)
{
    GtkChromeRendererContext *renderer = context;
    if (!renderer || !renderer->outer) return false;

    GtkWidget *footer = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, (gint)renderer->compact_spacing);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(footer), "footer-card");
    gtk_box_pack_start(GTK_BOX(renderer->outer), footer, FALSE, FALSE, 0);

    GtkWidget *action_row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->control_spacing);
    gtk_widget_set_name(action_row, "footer-actions");
    gtk_box_pack_start(GTK_BOX(footer), action_row, FALSE, FALSE, 0);

    renderer->footer_context_actions = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->compact_spacing);
    gtk_box_pack_start(
        GTK_BOX(action_row), renderer->footer_context_actions,
        FALSE, FALSE, 0);

    renderer->footer_general_actions = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->compact_spacing);
    gtk_box_pack_end(
        GTK_BOX(action_row), renderer->footer_general_actions,
        FALSE, FALSE, 0);

    renderer->footer_status_row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, (gint)renderer->control_spacing);
    gtk_box_pack_start(
        GTK_BOX(footer), renderer->footer_status_row, FALSE, FALSE, 0);
    return true;
}

static bool gtk_renderer_footer_action(void *context,
                                       RunnerUiChromeActionId action,
                                       const char *label,
                                       const char *tooltip,
                                       bool primary)
{
    GtkChromeRendererContext *renderer = context;
    if (!renderer || !renderer->app || !renderer->footer_general_actions ||
        !label || !tooltip)
        return false;

    const char *icon = NULL;
    GCallback callback = NULL;
    switch (action) {
    case RUNNER_UI_CHROME_EXPORT:
        icon = "document-save-symbolic";
        callback = G_CALLBACK(on_export);
        break;
    case RUNNER_UI_CHROME_ABOUT:
        icon = "help-about-symbolic";
        callback = G_CALLBACK(on_about);
        break;
    case RUNNER_UI_CHROME_REFRESH:
        icon = "view-refresh-symbolic";
        callback = G_CALLBACK(on_refresh);
        break;
    default:
        return false;
    }
    GtkWidget *button = make_action_button(
        icon, label, tooltip, primary ? "runner-action-primary" : NULL);
    g_signal_connect(button, "clicked", callback, renderer->app);
    gtk_box_pack_start(
        GTK_BOX(renderer->footer_general_actions), button, FALSE, FALSE, 0);
    return true;
}

static bool gtk_renderer_footer_status(void *context,
                                       const RunnerUiFooterState *state)
{
    GtkChromeRendererContext *renderer = context;
    if (!renderer || !renderer->app || !renderer->footer_status_row || !state)
        return false;

    renderer->app->status_label = gtk_label_new(state->status_text);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(renderer->app->status_label), "status-text");
    gtk_widget_set_halign(renderer->app->status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(renderer->app->status_label, TRUE);
    gtk_label_set_ellipsize(
        GTK_LABEL(renderer->app->status_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(
        GTK_BOX(renderer->footer_status_row), renderer->app->status_label,
        TRUE, TRUE, 0);

    GtkWidget *version_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(version_box, GTK_ALIGN_END);
    GtkWidget *app_version_label = gtk_label_new(state->app_version);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app_version_label), "version-text");
    gtk_widget_set_halign(app_version_label, GTK_ALIGN_END);
    gtk_box_pack_start(
        GTK_BOX(version_box), app_version_label, FALSE, FALSE, 0);

    GtkWidget *common_version_label = gtk_label_new(state->common_version);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(common_version_label), "version-text");
    gtk_widget_set_halign(common_version_label, GTK_ALIGN_END);
    gtk_box_pack_start(
        GTK_BOX(version_box), common_version_label, FALSE, FALSE, 0);
    gtk_box_pack_end(
        GTK_BOX(renderer->footer_status_row), version_box, FALSE, FALSE, 0);
    return true;
}

static bool gtk_renderer_end_footer(void *context)
{
    return context != NULL;
}


typedef struct {
    RunnerScopeApp *app;
    GtkWidget *nav_rail;
} GtkNavigationRendererContext;

static bool gtk_renderer_begin_navigation(void *context,
                                          size_t item_count,
                                          RunnerUiPageId selected_page)
{
    (void)item_count;
    (void)selected_page;
    GtkNavigationRendererContext *renderer = context;
    return renderer && renderer->app && renderer->nav_rail;
}

static bool gtk_renderer_navigation_separator(
    void *context, const RunnerUiPageSpec *before_page)
{
    GtkNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->nav_rail || !before_page) return false;
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(separator),
        "runner-main-nav-separator");
    gtk_box_pack_start(
        GTK_BOX(renderer->nav_rail), separator, FALSE, FALSE, 5);
    return true;
}

static bool gtk_renderer_navigation_item(void *context,
                                         const RunnerUiPageSpec *page,
                                         const char *platform_label,
                                         bool selected)
{
    GtkNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->app || !renderer->nav_rail ||
        !page || !platform_label)
        return false;
    GtkWidget *button = make_nav_button(
        renderer->app,
        (gint)page->id,
        page->nav_asset,
        platform_label,
        page->tooltip);
    gtk_box_pack_start(
        GTK_BOX(renderer->nav_rail), button, FALSE, TRUE, 0);
    if (selected)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    return true;
}

static bool gtk_renderer_end_navigation(void *context)
{
    return context != NULL;
}

static void on_main_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    RunnerScopeApp *app = user_data;
    g_atomic_int_set(&app->shutting_down, 1);
    g_cancellable_cancel(app->service_cancel);
}

static void build_ui(RunnerScopeApp *app)
{
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const guint compact_spacing = metrics ? metrics->compact_spacing : 6U;
    const guint control_spacing = metrics ? metrics->control_spacing : 10U;
    const guint screen_padding = metrics ? metrics->screen_padding : 20U;

    app->window = gtk_application_window_new(app->application);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_main_destroy), app);
    enforce_required_typography(app->window);
    gtk_window_set_title(GTK_WINDOW(app->window), runner_ui_product_title());
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1280, 800);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), "runnerscope");

    GtkChromeRendererContext chrome_context = {0};
    chrome_context.app = app;
    chrome_context.compact_spacing = compact_spacing;
    chrome_context.control_spacing = control_spacing;
    RunnerUiRenderer chrome_renderer = {0};
    chrome_renderer.context = &chrome_context;
    chrome_renderer.begin_header = gtk_renderer_begin_header;
    chrome_renderer.header_action = gtk_renderer_header_action;
    chrome_renderer.end_header = gtk_renderer_end_header;
    chrome_renderer.begin_footer = gtk_renderer_begin_footer;
    chrome_renderer.footer_action = gtk_renderer_footer_action;
    chrome_renderer.footer_status = gtk_renderer_footer_status;
    chrome_renderer.end_footer = gtk_renderer_end_footer;
    if (!runner_ui_render_header(&chrome_renderer))
        g_error("Unable to render the shared header contract.");
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

    RunnerUiWorkspaceSummaryState initial_summary = {
        RUNNER_UI_PAGE_RUNNERS, {"—", "—", "—", "—"}
    };
    GtkWorkspaceRendererContext workspace_context = {0};
    workspace_context.app = app;
    workspace_context.workspace = workspace;
    workspace_context.compact_spacing = compact_spacing;
    workspace_context.control_spacing = control_spacing;
    RunnerUiRenderer workspace_renderer = {0};
    workspace_renderer.context = &workspace_context;
    workspace_renderer.begin_workspace_summary =
        gtk_renderer_begin_workspace_summary;
    workspace_renderer.workspace_metric = gtk_renderer_workspace_metric;
    workspace_renderer.end_workspace_summary =
        gtk_renderer_end_workspace_summary;
    if (!runner_ui_render_workspace_summary(
            &workspace_renderer, RUNNER_UI_PLATFORM_LINUX, &initial_summary))
        g_error("Unable to render the shared workspace summary contract.");

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

    app->connection_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (gint)control_spacing);
    gtk_widget_set_no_show_all(app->connection_bar, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->connection_bar), "selection-card");
    app->connection_label = gtk_label_new("Sign in with GitHub in Settings to view organisation runners.");
    gtk_label_set_line_wrap(GTK_LABEL(app->connection_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app->connection_label), 0.0F);
    gtk_widget_set_hexpand(app->connection_label, TRUE);
    gtk_box_pack_start(GTK_BOX(app->connection_bar), app->connection_label, TRUE, TRUE, 0);
    GtkWidget *connection_settings = gtk_button_new_with_label("Settings");
    g_signal_connect(connection_settings, "clicked", G_CALLBACK(on_settings), app);
    gtk_box_pack_end(GTK_BOX(app->connection_bar), connection_settings, FALSE, FALSE, 0);
    gtk_widget_show(app->connection_label); gtk_widget_show(connection_settings);
    gtk_box_pack_start(GTK_BOX(workspace), app->connection_bar, FALSE, FALSE, 0);

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

    GtkNavigationRendererContext navigation_context = {app, nav_rail};
    RunnerUiRenderer navigation_renderer = {0};
    navigation_renderer.context = &navigation_context;
    navigation_renderer.begin_navigation = gtk_renderer_begin_navigation;
    navigation_renderer.navigation_separator =
        gtk_renderer_navigation_separator;
    navigation_renderer.navigation_item = gtk_renderer_navigation_item;
    navigation_renderer.end_navigation = gtk_renderer_end_navigation;
    if (!runner_ui_render_navigation(
            &navigation_renderer,
            RUNNER_UI_PLATFORM_LINUX,
            RUNNER_UI_PAGE_RUNNERS))
        g_error("Unable to render the shared navigation contract.");
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

    chrome_context.outer = outer;
    char app_version[128];
    char common_version[128];
    (void)snprintf(app_version, sizeof(app_version),
                   "Runner Monitor %s", RUNNERSCOPE_VERSION);
    (void)snprintf(common_version, sizeof(common_version),
                   "Common %s", INFILTRATR_COMMON_VERSION);
    RunnerUiFooterState footer_state = {
        "Starting…", app_version, common_version, false
    };
    if (!runner_ui_render_footer(&chrome_renderer, &footer_state))
        g_error("Unable to render the shared footer contract.");

    GtkWidget *context_actions = chrome_context.footer_context_actions;
    app->open_job_button = make_action_button(
        "emblem-web-symbolic", "Open selected job",
        "Open the selected workflow job in GitHub.", NULL);
    gtk_widget_set_sensitive(app->open_job_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->open_job_button, "Select an active job to open it in GitHub.");
    g_signal_connect(
        app->open_job_button, "clicked", G_CALLBACK(on_open_job), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->open_job_button, FALSE, FALSE, 0);

    app->open_diag_button = make_action_button(
        "folder-open-symbolic", "Open diagnostic",
        "Open the selected runner diagnostic.", NULL);
    gtk_widget_set_sensitive(app->open_diag_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->open_diag_button,
        "Select a local runner to open its latest diagnostic.");
    g_signal_connect(
        app->open_diag_button, "clicked", G_CALLBACK(on_open_diag), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->open_diag_button, FALSE, FALSE, 0);

    app->restart_button = make_action_button(
        "view-refresh-symbolic", "Restart selected runner",
        "Restart the selected local runner service.",
        "runner-action-warning");
    gtk_widget_set_sensitive(app->restart_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->restart_button,
        "Select a local runner before restarting its service.");
    g_signal_connect(
        app->restart_button, "clicked", G_CALLBACK(on_restart), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->restart_button, FALSE, FALSE, 0);

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
    if (!app->config.organisation[0] || !app->config.github_token[0]) {
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
    GError *config_error = NULL;
    if (!load_config(&app->config, &config_error)) {
        app->config_error = g_strdup(config_error ? config_error->message : "Unable to load settings");
        g_clear_error(&config_error);
        runner_config_defaults(&app->config);
    }
    app->runner_data_stale = TRUE;
    app->activity_data_stale = TRUE;
    app->fetch_transport = runner_http_get;
    app->config_generation = 1U;
    app->service_cancel = g_cancellable_new();
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
    g_cancellable_cancel(app->service_cancel);
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
    while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);
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
    g_free(app->config_error);
    g_free(app->runner_error);
    g_free(app->activity_error);
    memset(app->config.github_token, 0, sizeof(app->config.github_token));
    g_clear_object(&app->service_cancel);
    g_hash_table_unref(app->activity_row_by_url);
    g_ptr_array_unref(app->runner_rows);
    g_ptr_array_unref(app->activity_rows);
    g_ptr_array_unref(app->history_rows);
    g_ptr_array_unref(app->local_rows);
}

typedef struct {
    GMutex mutex;
    GCond condition;
    guint calls;
    guint failure;
    gboolean block_first;
    gboolean entered;
    gboolean released;
    char first_token[RUNNER_GITHUB_TOKEN_CAPACITY];
    char last_token[RUNNER_GITHUB_TOKEN_CAPACITY];
} UiTestTransport;

static bool ui_test_http_get(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    (void)error; (void)error_size;
    UiTestTransport *fixture = context;
    g_mutex_lock(&fixture->mutex);
    fixture->calls++;
    if (fixture->calls == 1U) g_strlcpy(fixture->first_token, token, sizeof(fixture->first_token));
    g_strlcpy(fixture->last_token, token, sizeof(fixture->last_token));
    if (fixture->block_first && fixture->calls == 1U) {
        fixture->entered = TRUE;
        g_cond_signal(&fixture->condition);
        while (!fixture->released) g_cond_wait(&fixture->condition, &fixture->mutex);
    }
    g_mutex_unlock(&fixture->mutex);
    response->status = 200U;
    response->link = NULL;
    if (strstr(url, "/actions/runners")) {
        const char *name = strstr(url, "/NewOrg/") ? "FreshRunner" : "OldRunner";
        response->body = g_strdup_printf("{\"runners\":[{\"name\":\"%s\",\"os\":\"Linux\",\"status\":\"online\",\"busy\":false,\"labels\":[{\"name\":\"self-hosted\"}]}]}", name);
    } else if (strstr(url, "/repos?")) {
        response->body = g_strdup("[{\"name\":\"FixtureRepo\",\"archived\":false,\"disabled\":false}]");
    } else if (strstr(url, "/jobs?")) {
        response->status = fixture->failure == 2U ? 403U : 200U;
        response->body = g_strdup(response->status == 403U ? "{\"message\":\"Job access denied\"}" : "{\"jobs\":[]}");
    } else {
        response->status = fixture->failure == 1U ? 403U : 200U;
        response->body = g_strdup(response->status == 403U ? "{\"message\":\"Workflow access denied\"}" :
            "{\"workflow_runs\":[{\"id\":123,\"name\":\"Build\",\"status\":\"in_progress\",\"event\":\"push\",\"head_branch\":\"main\"}]}");
    }
    return true;
}

static void ui_test_wait_refresh(RunnerScopeApp *app, gboolean activity)
{
    const gint64 deadline = g_get_monotonic_time() + 5000000;
    while (g_atomic_int_get(activity ? &app->activity_refreshing : &app->runner_refreshing) &&
        g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000U);
    }
    g_assert_cmpint(g_atomic_int_get(activity ? &app->activity_refreshing : &app->runner_refreshing), ==, 0);
}

typedef struct {
    guint attempts;
    const RunnerConfig *expected;
} UiTestSettings;

static bool ui_test_oauth_post(const char *request_path, const char *form,
    RunnerHttpResponse *response, char *error, size_t size, void *context)
{
    (void)error; (void)size; (void)context;
    g_assert_nonnull(strstr(form, "client_id=offline-client"));
    response->status = 200U;
    response->link = NULL;
    response->body = g_strdup(strcmp(request_path, "/login/device/code") == 0
        ? "{\"device_code\":\"offline-device\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://github.com/login/device\",\"expires_in\":900,\"interval\":1}"
        : "{\"access_token\":\"offline-settings-fixture\",\"token_type\":\"bearer\"}");
    return true;
}

static void ui_test_sign_in_cancel(RunnerScopeApp *app)
{
    GtkWidget *dialog = gtk_dialog_new();
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *status = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid);
    GithubSignIn *sign_in = github_sign_in_create(dialog, grid, status);
    sign_in->client_id = "offline-client";
    sign_in->post = ui_test_oauth_post;
    sign_in->open_browser = FALSE;
    github_sign_in_start(NULL, sign_in);
    gint64 deadline = g_get_monotonic_time() + 5000000;
    while (sign_in->busy && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000U);
    }
    g_assert_false(sign_in->busy);
    g_assert_cmpint(sign_in->auth.phase, ==, RUNNER_AUTH_WAITING);
    sign_in->auth.next_poll_ms = 0U;
    (void)github_sign_in_tick(sign_in);
    github_sign_in_cancel(sign_in);
    deadline = g_get_monotonic_time() + 5000000;
    while (sign_in->busy && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000U);
    }
    g_assert_false(sign_in->busy);
    g_assert_false(sign_in->active);
    g_assert_cmpstr(sign_in->auth.token, ==, "");
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(sign_in->code)), ==, "");
    g_assert_cmpstr(app->config.github_token, ==, "");
    github_sign_in_close(sign_in);
    gtk_widget_destroy(dialog);
    gtk_widget_destroy(status);
}

static gboolean ui_test_settings_response(gpointer data)
{
    UiTestSettings *test = data;
    GList *windows = gtk_window_list_toplevels();
    gboolean found = FALSE;
    for (GList *item = windows; item; item = item->next) {
        GithubSignIn *sign_in = g_object_get_data(G_OBJECT(item->data), "github-sign-in");
        if (!sign_in) continue;
        GtkWidget *organisation = g_object_get_data(G_OBJECT(item->data), "organisation-entry");
        g_assert_null(g_object_get_data(G_OBJECT(item->data), "token-entry"));
        g_assert_nonnull(g_object_get_data(G_OBJECT(item->data), "github-sign-in-button"));
        if (test->attempts == 0U) {
            g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(organisation)), ==, test->expected->organisation);
            GtkWidget *grid = gtk_widget_get_parent(organisation);
            const unsigned int values[] = {test->expected->expected_runners, test->expected->runner_poll_seconds,
                test->expected->activity_scan_seconds, test->expected->repository_scan_limit, test->expected->local_health_seconds};
            for (guint i = 0U; i < G_N_ELEMENTS(values); ++i) {
                GtkWidget *number_entry = gtk_grid_get_child_at(GTK_GRID(grid), 1, (gint)i + 2);
                unsigned int value = 0U;
                g_assert_true(entry_uint(number_entry, &value));
                g_assert_cmpuint(value, ==, values[i]);
            }
            GtkWidget *theme = gtk_grid_get_child_at(GTK_GRID(grid), 1, 7);
            g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(theme)), ==, (gint)test->expected->theme_mode);
            GtkWidget *help = g_object_get_data(G_OBJECT(item->data), "sign-in-help");
            g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(help)), "browser"));
            gtk_entry_set_text(GTK_ENTRY(organisation), "");
            gtk_dialog_response(GTK_DIALOG(item->data), GTK_RESPONSE_ACCEPT);
        } else {
            GtkWidget *message = g_object_get_data(G_OBJECT(item->data), "settings-error");
            g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(message)), "organisation"));
            gtk_entry_set_text(GTK_ENTRY(organisation), test->expected->organisation);
            sign_in->post = ui_test_oauth_post;
            sign_in->client_id = "offline-client";
            sign_in->open_browser = FALSE;
            github_sign_in_start(NULL, sign_in);
        }
        test->attempts++;
        found = TRUE;
        break;
    }
    g_list_free(windows);
    g_assert_true(found);
    return test->attempts == 1U ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

static void ui_test_provider_failures(RunnerScopeApp *app)
{
    UiTestTransport fixture = {0};
    g_mutex_init(&fixture.mutex); g_cond_init(&fixture.condition);
    app->fetch_transport = ui_test_http_get;
    app->fetch_context = &fixture;
    g_strlcpy(app->config.organisation, "OldOrg", sizeof(app->config.organisation));
    g_strlcpy(app->config.github_token, "offline-old-token", sizeof(app->config.github_token));
    fixture.block_first = TRUE;
    request_runner_refresh(app);
    g_mutex_lock(&fixture.mutex);
    const gint64 deadline = g_get_monotonic_time() + 5000000;
    while (!fixture.entered)
        g_assert_true(g_cond_wait_until(&fixture.condition, &fixture.mutex, deadline));
    g_strlcpy(app->config.organisation, "NewOrg", sizeof(app->config.organisation));
    g_strlcpy(app->config.github_token, "offline-new-token", sizeof(app->config.github_token));
    app->config_generation++;
    fixture.released = TRUE;
    g_cond_signal(&fixture.condition);
    g_mutex_unlock(&fixture.mutex);
    ui_test_wait_refresh(app, FALSE);
    g_assert_cmpuint(fixture.calls, ==, 2U);
    g_assert_cmpstr(fixture.first_token, ==, "offline-old-token");
    g_assert_cmpstr(fixture.last_token, ==, "offline-new-token");
    g_assert_cmpuint(app->runner_rows->len, ==, 1U);
    g_assert_cmpstr(((RunnerRow *)g_ptr_array_index(app->runner_rows, 0U))->name, ==, "FreshRunner");
    g_assert_false(app->runner_data_stale);

    ActivityRow *previous = g_new0(ActivityRow, 1U);
    previous->environment = g_strdup("LOCAL"); previous->repo = g_strdup("FixtureRepo");
    previous->workflow = g_strdup("Build"); previous->job = g_strdup("Last known job");
    previous->step = g_strdup("Compile"); previous->status = g_strdup("IN_PROGRESS");
    previous->runner = g_strdup("FreshRunner"); previous->runtime = g_strdup("frozen runtime");
    previous->event = g_strdup("push"); previous->branch = g_strdup("main");
    previous->url = g_strdup("https://github.com/FixtureOrg/FixtureRepo/actions/runs/1");
    g_ptr_array_add(app->activity_rows, previous);
    for (guint failure = 1U; failure <= 2U; ++failure) {
        fixture.failure = failure;
        app->activity_data_stale = FALSE;
        request_activity_refresh(app);
        ui_test_wait_refresh(app, TRUE);
        g_assert_true(app->activity_data_stale);
        g_assert_cmpuint(app->activity_rows->len, ==, 1U);
        g_assert_true(previous == g_ptr_array_index(app->activity_rows, 0U));
        g_assert_true(gtk_widget_get_visible(app->connection_bar));
        g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app->connection_label)), "Actions: Read"));
        update_workspace_context(app, RUNNER_UI_PAGE_ACTIVE_JOBS);
        g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(app->workspace_metric_value[0])), ==, "—");
        g_assert_false(app->runner_data_stale);
    }
    RunnerRow *runner = g_ptr_array_index(app->runner_rows, 0U);
    g_free(runner->state_for); runner->state_for = g_strdup("frozen state duration");
    RunnerRefreshResult *failure = g_new0(RunnerRefreshResult, 1U);
    failure->app = app; failure->generation = app->config_generation;
    failure->error = g_strdup("Fixture runner access denied (HTTP 403)");
    g_atomic_int_set(&app->runner_refreshing, 1);
    runner_apply_idle(failure);
    tick_timer_cb(app);
    g_assert_true(app->runner_data_stale);
    g_assert_cmpstr(runner->state_for, ==, "frozen state duration");
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(app->workspace_metric_value[0])), ==, "—");
    g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app->connection_label)), "last known"));
    app->fetch_transport = runner_http_get; app->fetch_context = NULL;
    g_cond_clear(&fixture.condition); g_mutex_clear(&fixture.mutex);
}

/* Exercise the installed graphical shell using isolated settings and an offline
 * transport. No user token, account, service or network operation is accessed. */
static int ui_self_test(int *argc, char ***argv)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("runnerscope-ui-test-XXXXXX", &error);
    g_assert_no_error(error);
    char *config_directory = g_build_filename(directory, "config", NULL);
    char *state_directory = g_build_filename(directory, "state", NULL);
    g_setenv("XDG_CONFIG_HOME", config_directory, TRUE);
    g_setenv("XDG_STATE_HOME", state_directory, TRUE);
    if (!gtk_init_check(argc, argv)) return 1;
    RunnerConfig fixture_config;
    runner_config_defaults(&fixture_config);
    g_strlcpy(fixture_config.organisation, "FixtureOrg", sizeof(fixture_config.organisation));
    g_strlcpy(fixture_config.github_token, "offline-config-fixture", sizeof(fixture_config.github_token));
    fixture_config.expected_runners = 15U;
    g_assert_true(save_config(&fixture_config, &error));
    g_assert_no_error(error);
    char *path = config_path();
    g_assert_cmpint(g_chmod(path, 0644), ==, 0);
    g_assert_true(save_config(&fixture_config, &error));
    GStatBuf status;
    g_assert_cmpint(g_stat(path, &status), ==, 0);
    g_assert_cmpuint(status.st_mode & 0777U, ==, 0600U);
    RunnerConfig reloaded;
    g_assert_true(load_config(&reloaded, &error));
    g_assert_cmpstr(reloaded.github_token, ==, fixture_config.github_token);
    g_assert_cmpuint(reloaded.expected_runners, ==, 15U);
    g_assert_true(g_file_set_contents(path, "{bad settings", -1, &error));
    g_assert_false(load_config(&reloaded, &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    char *preserved = NULL;
    g_assert_true(g_file_get_contents(path, &preserved, NULL, &error));
    g_assert_cmpstr(preserved, ==, "{bad settings"); g_free(preserved);
    static const char embedded_nul[] = "{\"organisation\":\"FixtureOrg\"}\0trailing data";
    g_assert_true(g_file_set_contents(path, embedded_nul, (gssize)sizeof(embedded_nul) - 1, &error));
    g_assert_false(load_config(&reloaded, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    char *oversized = g_malloc0(65538U);
    memset(oversized, ' ', 65537U);
    g_assert_true(g_file_set_contents(path, oversized, 65537, &error));
    g_assert_false(load_config(&reloaded, &error));
    g_assert_nonnull(error); g_clear_error(&error); g_free(oversized);
    g_assert_true(g_file_set_contents(path, "{\"organisation\":\"FixtureOrg\"}", -1, &error));
    g_assert_true(load_config(&reloaded, &error));
    g_assert_cmpstr(reloaded.github_token, ==, "");
    /* Exact configuration written by the previous native Linux application.
     * Reading it must preserve the observation cadence and must not publish a
     * replacement before the user explicitly saves a token. */
    static const char legacy_config[] =
        "{\n"
        "  \"organisation\": \"Infiltrator-Projects\",\n"
        "  \"expected_runners\": 15,\n"
        "  \"runner_poll_seconds\": 2,\n"
        "  \"activity_scan_seconds\": 45,\n"
        "  \"repository_scan_limit\": 25,\n"
        "  \"local_health_seconds\": 10,\n"
        "  \"theme_mode\": \"System\"\n"
        "}\n";
    g_assert_true(g_file_set_contents(path, legacy_config, -1, &error));
    g_assert_true(load_config(&reloaded, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(reloaded.organisation, ==, "Infiltrator-Projects");
    g_assert_cmpstr(reloaded.github_token, ==, "");
    g_assert_cmpuint(reloaded.expected_runners, ==, 15U);
    g_assert_cmpuint(reloaded.runner_poll_seconds, ==, 2U);
    g_assert_cmpuint(reloaded.activity_scan_seconds, ==, 45U);
    g_assert_cmpuint(reloaded.repository_scan_limit, ==, 25U);
    g_assert_cmpuint(reloaded.local_health_seconds, ==, 10U);
    g_assert_cmpint(reloaded.theme_mode, ==, RUNNER_THEME_SYSTEM);
    g_assert_true(g_file_get_contents(path, &preserved, NULL, &error));
    g_assert_cmpstr(preserved, ==, legacy_config); g_free(preserved);
    RunnerScopeApp app;
    app_init(&app);
    g_assert_null(app.config_error);
    g_assert_cmpstr(app.config.organisation, ==, "Infiltrator-Projects");
    g_assert_cmpuint(app.config.runner_poll_seconds, ==, 2U);
    g_assert_cmpuint(app.config.local_health_seconds, ==, 10U);
    g_assert_true(g_file_get_contents(path, &preserved, NULL, &error));
    g_assert_cmpstr(preserved, ==, legacy_config); g_free(preserved);
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
    ui_test_sign_in_cancel(&app);
    UiTestSettings settings_test = {0U, &reloaded};
    g_idle_add(ui_test_settings_response, &settings_test);
    g_assert_true(settings_dialog(&app, TRUE));
    g_assert_true(load_config(&reloaded, &error));
    g_assert_cmpstr(reloaded.github_token, ==, "offline-settings-fixture");
    g_assert_cmpstr(reloaded.organisation, ==, "Infiltrator-Projects");
    g_assert_cmpuint(reloaded.expected_runners, ==, 15U);
    g_assert_cmpuint(reloaded.runner_poll_seconds, ==, 2U);
    g_assert_cmpuint(reloaded.activity_scan_seconds, ==, 45U);
    g_assert_cmpuint(reloaded.repository_scan_limit, ==, 25U);
    g_assert_cmpuint(reloaded.local_health_seconds, ==, 10U);
    g_assert_cmpint(reloaded.theme_mode, ==, RUNNER_THEME_SYSTEM);
    g_assert_cmpuint(app.config_generation, ==, 2U);
    ui_test_provider_failures(&app);
    gtk_widget_destroy(app.window);
    g_object_unref(app.application);
    app_destroy(&app);
    char *history = state_path();
    (void)g_unlink(history);
    char *history_directory = g_path_get_dirname(history);
    (void)g_rmdir(history_directory);
    (void)g_unlink(path);
    char *settings_directory = g_path_get_dirname(path);
    (void)g_rmdir(settings_directory);
    (void)g_rmdir(config_directory); (void)g_rmdir(state_directory); (void)g_rmdir(directory);
    g_free(history_directory); g_free(settings_directory); g_free(history); g_free(path);
    g_free(config_directory); g_free(state_directory); g_free(directory);
    puts("Runner UI passed: cards, native browser sign-in, private settings save/reload, corrupt-file preservation, configuration snapshots, stale generations and provider failures.");
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
