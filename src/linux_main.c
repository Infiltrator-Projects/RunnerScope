// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/escape.h>
#include <infiltratr/format.h>
#include <infiltratr/posix.h>

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
    GtkWidget *summary_label;
    GtkWidget *updated_label;
    GtkWidget *scan_label;
    GtkWidget *open_job_button;
    GtkWidget *open_diag_button;
    GtkWidget *restart_button;
    GtkWidget *theme_menu_items[3];
    GtkWidget *counter_labels[7];

    GtkListStore *runner_store;
    GtkListStore *activity_store;
    GtkListStore *history_store;
    GtkListStore *local_store;
    GtkWidget *runner_tree;
    GtkWidget *activity_tree;
    GtkWidget *history_tree;
    GtkWidget *local_tree;

    RunnerConfig config;
    GHashTable *sessions;
    GHashTable *job_by_runner;
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
    GThread *runner_thread;
    GThread *activity_thread;
    GThread *local_thread;
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
    guint repos_scanned;
    char *error;
} ActivityRefreshResult;

typedef struct {
    RunnerScopeApp *app;
    GPtrArray *rows;
    char *error;
} LocalRefreshResult;

static gboolean activity_apply_idle(gpointer data);
static gboolean local_apply_idle(gpointer data);
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

static char *clock_text(void)
{
    time_t now = time(NULL);
    struct tm local_tm;
    char buffer[32] = "—";
    if (localtime_r(&now, &local_tm))
        (void)strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_tm);
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
    config->runner_poll_seconds = 2U;
    config->activity_scan_seconds = 45U;
    config->repository_scan_limit = 25U;
    config->local_health_seconds = 10U;
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
            if (g_ascii_strcasecmp(theme, "day") == 0)
                config->theme_mode = INFILTRATR_THEME_DAY;
            else if (g_ascii_strcasecmp(theme, "night") == 0)
                config->theme_mode = INFILTRATR_THEME_NIGHT;
            else
                config->theme_mode = INFILTRATR_THEME_SYSTEM;
        }
    }
    g_free(contents);
    g_free(path);

    const char *env_org = g_getenv("GITHUB_RUNNER_ORG");
    if (env_org && *env_org)
        g_strlcpy(config->organisation, env_org, sizeof(config->organisation));

    if (config->runner_poll_seconds < 1U) config->runner_poll_seconds = 1U;
    if (config->activity_scan_seconds < 10U) config->activity_scan_seconds = 10U;
    if (config->repository_scan_limit < 1U) config->repository_scan_limit = 1U;
    if (config->local_health_seconds < 5U) config->local_health_seconds = 5U;
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

    const char *mode = infiltratr_theme_mode_name(config->theme_mode);
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
        config->local_health_seconds, mode ? mode : "System");
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

static void rgb_text(uint32_t rgb, char out[8])
{
    (void)g_snprintf(out, 8U, "#%06x", rgb & 0x00ffffffU);
}

static void apply_theme(RunnerScopeApp *app)
{
    if (!app) return;
    const InfiltratrThemePalette *p =
        infiltratr_theme_resolve(app->config.theme_mode, system_dark_mode());
    const InfiltratrTypography *type = infiltratr_typography();
    if (!p || !type || !type->ui_family || !type->brand_family) return;

    char bg[8], panel[8], card[8], surface[8], border[8], text[8], title[8];
    char muted[8], subtle[8], button_bg[8], button_fg[8], select_bg[8], select_fg[8];
    char success[8], warning[8], fault[8], info[8], operation[8], card_hover[8];
    rgb_text(p->background_rgb, bg); rgb_text(p->panel_rgb, panel);
    rgb_text(p->card_rgb, card); rgb_text(p->surface_rgb, surface);
    rgb_text(p->border_rgb, border); rgb_text(p->text_rgb, text);
    rgb_text(p->title_rgb, title); rgb_text(p->muted_rgb, muted);
    rgb_text(p->subtle_rgb, subtle); rgb_text(p->button_background_rgb, button_bg);
    rgb_text(p->button_foreground_rgb, button_fg);
    rgb_text(p->selection_background_rgb, select_bg);
    rgb_text(p->selection_foreground_rgb, select_fg);
    rgb_text(p->success_rgb, success); rgb_text(p->warning_rgb, warning);
    rgb_text(p->fault_rgb, fault); rgb_text(p->info_rgb, info);
    rgb_text(p->operation_rgb, operation); rgb_text(p->card_hover_rgb, card_hover);

    char *css = g_strdup_printf(
        "window { background:%s; color:%s; }"
        "box, notebook, notebook > stack { background:%s; color:%s; }"
        "label { color:%s; }"
        "#title { color:%s; font-size:28px; font-weight:700; }"
        "#meta { color:%s; }"
        "#summary { color:%s; }"
        ".counter { background:%s; border:1px solid %s; border-radius:6px;"
        " padding:6px 10px; font-weight:700; }"
        ".counter-running { color:%s; } .counter-idle { color:%s; }"
        ".counter-offline { color:%s; } .counter-local { color:%s; }"
        ".counter-hosted { color:%s; } .counter-queued { color:%s; }"
        "entry { background:%s; color:%s; border-color:%s; }"
        "button { background:%s; color:%s; border:1px solid %s; border-radius:6px; min-height:30px; }"
        "button:hover { background:%s; }"
        "notebook tab { background:%s; color:%s; border-color:%s; border-radius:6px; padding:5px 10px; }"
        "notebook tab:checked { background:transparent; color:%s; border-bottom:2px solid %s; }"
        "treeview.view { background:%s; color:%s; }"
        "treeview.view:selected { background:%s; color:%s; }"
        "treeview header button { background:%s; color:%s; }"
        "scrolledwindow { border:1px solid %s; }",
        bg, text, bg, text, text, title, muted, text, card, border,
        success, info, fault, success, operation, warning, surface, text, border,
        button_bg, button_fg, border, card_hover, panel, muted, border, select_fg,
        operation, surface, text, select_bg, select_fg, panel, title, border);

    char *chrome_css = g_strdup_printf(
        "menubar, menu { background:%s; color:%s; }"
        "menubar { border-bottom:1px solid %s; }"
        "menuitem { color:%s; }"
        "menuitem:hover { background:%s; color:%s; }"
        "#footer-actions button { background:%s; color:%s; border:1px solid %s;"
        " min-height:30px; padding:4px 10px; }"
        "#footer-actions button:hover { background:%s; }"
        "#footer-actions button:disabled { background:%s; color:%s; border-color:%s; }",
        panel, text, border, text, card_hover, title,
        surface, text, border, card_hover, panel, subtle, border);
    char *combined_css = g_strconcat(css, chrome_css, NULL);
    g_free(chrome_css);
    g_free(css);

    char *typography_css = g_strdup_printf(
        "window, window *, popover, popover * { font-family:\"%s\"; font-weight:%u; }"
        "#title { font-family:\"%s\"; font-size:28px; font-weight:%u; }",
        type->ui_family,
        (unsigned int)type->ui_regular_weight,
        type->brand_family,
        (unsigned int)type->brand_weight);
    css = g_strconcat(combined_css, typography_css, NULL);
    g_free(typography_css);
    g_free(combined_css);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    GdkScreen *screen = gdk_screen_get_default();
    if (screen)
        gtk_style_context_add_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    g_free(css);
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

static gboolean text_matches_filter(RunnerScopeApp *app, const char *text)
{
    const char *needle = gtk_entry_get_text(GTK_ENTRY(app->filter_entry));
    if (!needle || !*needle) return TRUE;
    char *folded_text = g_utf8_casefold(text ? text : "", -1);
    char *folded_needle = g_utf8_casefold(needle, -1);
    const gboolean matches = strstr(folded_text, folded_needle) != NULL;
    g_free(folded_text); g_free(folded_needle);
    return matches;
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
    g_free(directory); g_free(path); g_string_free(state, TRUE);
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

static void tree_add_text_column(GtkWidget *tree, const char *title,
                                 gint column, gint min_width)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *view_column =
        gtk_tree_view_column_new_with_attributes(title, renderer, "text", column, NULL);
    gtk_tree_view_column_set_resizable(view_column, TRUE);
    gtk_tree_view_column_set_min_width(view_column, min_width);
    gtk_tree_view_column_set_sort_column_id(view_column, column);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), view_column);
}

static GtkWidget *scrolled_tree(GtkListStore *store)
{
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    return scroll;
}

static void render_history(RunnerScopeApp *app)
{
    gtk_list_store_clear(app->history_store);
    for (guint i = 0U; i < app->history_rows->len; i++) {
        HistoryRow *row = g_ptr_array_index(app->history_rows, i);
        char *search = g_strdup_printf("%s %s %s %s", row->time_text, row->runner,
                                       row->event, row->detail);
        const gboolean visible = text_matches_filter(app, search);
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
}

static void render_runners(RunnerScopeApp *app)
{
    gtk_list_store_clear(app->runner_store);
    for (guint i = 0U; i < app->runner_rows->len; i++) {
        RunnerRow *row = g_ptr_array_index(app->runner_rows, i);
        char *search = g_strdup_printf("%s %s %s %s %s %s", row->name, row->os,
                                       row->state, row->repo, row->job, row->labels);
        const gboolean visible = text_matches_filter(app, search);
        g_free(search);
        if (!visible) continue;
        GtkTreeIter iter;
        gtk_list_store_append(app->runner_store, &iter);
        gtk_list_store_set(app->runner_store, &iter,
            RUNNER_COL_NAME, row->name, RUNNER_COL_OS, row->os,
            RUNNER_COL_STATE, row->state, RUNNER_COL_REPO, row->repo,
            RUNNER_COL_JOB, row->job, RUNNER_COL_RUNTIME, row->runtime,
            RUNNER_COL_STATE_FOR, row->state_for, RUNNER_COL_JOBS, row->jobs,
            RUNNER_COL_BUSY, row->busy_pct, RUNNER_COL_LABELS, row->labels, -1);
    }
}

static void render_activity(RunnerScopeApp *app)
{
    gtk_list_store_clear(app->activity_store);
    for (guint i = 0U; i < app->activity_rows->len; i++) {
        ActivityRow *row = g_ptr_array_index(app->activity_rows, i);
        char *search = g_strdup_printf("%s %s %s %s %s %s %s %s",
            row->environment, row->repo, row->workflow, row->job,
            row->step, row->status, row->runner, row->branch);
        const gboolean visible = text_matches_filter(app, search);
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
}

static void render_local(RunnerScopeApp *app)
{
    gtk_list_store_clear(app->local_store);
    for (guint i = 0U; i < app->local_rows->len; i++) {
        LocalRow *row = g_ptr_array_index(app->local_rows, i);
        char *search = g_strdup_printf("%s %s %s %s %s", row->runner,
            row->service_state, row->github_state, row->diag, row->path);
        const gboolean visible = text_matches_filter(app, search);
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
}

static void update_counter(RunnerScopeApp *app, guint index,
                           const char *name, guint value)
{
    char *text = g_strdup_printf("%s  %u", name, value);
    gtk_label_set_text(GTK_LABEL(app->counter_labels[index]), text);
    g_free(text);
}

static void update_summary(RunnerScopeApp *app)
{
    update_counter(app, 0U, "TOTAL", app->runners_total);
    update_counter(app, 1U, "RUNNING", app->runners_running);
    update_counter(app, 2U, "IDLE", app->runners_idle);
    update_counter(app, 3U, "OFFLINE", app->runners_offline);
    update_counter(app, 4U, "LOCAL ACTIVE", app->local_active);
    update_counter(app, 5U, "GITHUB ACTIVE", app->hosted_active);
    update_counter(app, 6U, "QUEUED", app->queued);

    guint jobs = 0U;
    double busy = 0.0;
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, app->sessions);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        RunnerSession *session = value;
        jobs += session->jobs;
        busy += session->busy_seconds;
        if (strcmp(session->state, "RUNNING") == 0 && session->busy_started > 0.0)
            busy += now_monotonic() - session->busy_started;
    }
    const double elapsed = MAX(1.0, now_monotonic() - app->session_started);
    const guint runner_count = MAX(1U, g_hash_table_size(app->sessions));
    char *up_text = duration_text(elapsed);
    char *summary = g_strdup_printf(
        "Observed this session: self-hosted %u jobs  •  GitHub-hosted %u active  •  "
        "self-hosted share %.0f%%  •  monitor up %s",
        jobs, app->hosted_active,
        MIN(100.0, (busy * 100.0) / (elapsed * (double)runner_count)),
        up_text);
    gtk_label_set_text(GTK_LABEL(app->summary_label), summary);
    g_free(summary);
    g_free(up_text);
}

static gboolean runner_apply_idle(gpointer data)
{
    RunnerRefreshResult *result = data;
    RunnerScopeApp *app = result->app;
    if (app->runner_thread) {
        g_thread_join(app->runner_thread);
        app->runner_thread = NULL;
    }
    if (result->error) {
        gtk_label_set_text(GTK_LABEL(app->status_label), result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->runner_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    g_ptr_array_set_size(app->runner_rows, 0U);
    app->runners_total = app->runners_running = app->runners_idle = app->runners_offline = 0U;
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
        } else if (strcmp(session->state, state) != 0) {
            if (strcmp(session->state, "RUNNING") == 0 && session->busy_started > 0.0) {
                session->busy_seconds += now - session->busy_started;
                session->busy_started = 0.0;
            }
            if (strcmp(state, "RUNNING") == 0) {
                session->busy_started = now;
                session->jobs++;
            }
            g_strlcpy(session->state, state, sizeof(session->state));
            session->state_since = now;
            JobSummary *summary = g_hash_table_lookup(app->job_by_runner, raw->name);
            add_history(app, raw->name, state,
                        summary && summary->job ? summary->job : "");
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
            g_free(session->last_repo); session->last_repo = g_strdup(summary->repo);
            g_free(session->last_job); session->last_job = g_strdup(summary->job);
        } else if (strcmp(state, "RUNNING") == 0) {
            row->repo = g_strdup("Resolving…");
            row->job = g_strdup("GitHub reports runner busy");
            row->runtime = duration_text(now - session->state_since);
        } else {
            row->repo = g_strdup(session->last_repo ? session->last_repo : "—");
            if (session->last_job)
                row->job = g_strdup_printf("Last: %s", session->last_job);
            else
                row->job = g_strdup("—");
            row->runtime = g_strdup("—");
        }
        row->state_for = duration_text(now - session->state_since);
        row->jobs = g_strdup_printf("%u", session->jobs);
        double busy_seconds = session->busy_seconds;
        if (strcmp(state, "RUNNING") == 0 && session->busy_started > 0.0)
            busy_seconds += now - session->busy_started;
        const double elapsed = MAX(1.0, now - app->session_started);
        row->busy_pct = g_strdup_printf("%.1f%%",
            MIN(100.0, busy_seconds * 100.0 / elapsed));
        row->labels = g_strdup(raw->labels && *raw->labels ? raw->labels : "—");
        g_ptr_array_add(app->runner_rows, row);

        app->runners_total++;
        if (strcmp(state, "RUNNING") == 0) app->runners_running++;
        else if (strcmp(state, "IDLE") == 0) app->runners_idle++;
        else app->runners_offline++;
    }

    render_runners(app);
    render_history(app);
    update_summary(app);
    char *clock = clock_text();
    char *updated = g_strdup_printf("Runner data: %s", clock);
    gtk_label_set_text(GTK_LABEL(app->updated_label), updated);
    g_free(updated); g_free(clock);
    char *status = app->config.expected_runners != 0U
        ? g_strdup_printf("%u runners detected; expected %u. %u running, %u idle, %u offline.",
            app->runners_total, app->config.expected_runners, app->runners_running,
            app->runners_idle, app->runners_offline)
        : g_strdup_printf("%u runners detected. %u running, %u idle, %u offline.",
            app->runners_total, app->runners_running, app->runners_idle, app->runners_offline);
    gtk_label_set_text(GTK_LABEL(app->status_label), status);
    g_free(status);

    g_ptr_array_unref(result->rows);
    g_atomic_int_set(&app->runner_refreshing, 0);
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
    result->repos_scanned = scanned;
    g_idle_add(activity_apply_idle, result);
    return NULL;
}

static gboolean activity_apply_idle(gpointer data)
{
    ActivityRefreshResult *result = data;
    RunnerScopeApp *app = result->app;
    if (app->activity_thread) {
        g_thread_join(app->activity_thread);
        app->activity_thread = NULL;
    }
    if (result->error) {
        gtk_label_set_text(GTK_LABEL(app->status_label), result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->activity_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }

    g_ptr_array_set_size(app->activity_rows, 0U);
    g_hash_table_remove_all(app->job_by_runner);
    for (guint i = 0U; i < result->rows->len; i++) {
        ActivityRow *source = g_ptr_array_index(result->rows, i);
        ActivityRow *row = g_new0(ActivityRow, 1U);
        row->environment = g_strdup(source->environment); row->repo = g_strdup(source->repo);
        row->workflow = g_strdup(source->workflow); row->job = g_strdup(source->job);
        row->step = g_strdup(source->step); row->status = g_strdup(source->status);
        row->runner = g_strdup(source->runner); row->runtime = g_strdup(source->runtime);
        row->event = g_strdup(source->event); row->branch = g_strdup(source->branch);
        row->url = g_strdup(source->url);
        row->started_epoch = source->started_epoch;
        g_ptr_array_add(app->activity_rows, row);

        if (strcmp(row->status, "IN_PROGRESS") == 0 && row->runner &&
            strcmp(row->runner, "—") != 0) {
            JobSummary *summary = g_new0(JobSummary, 1U);
            summary->repo = g_strdup(row->repo);
            summary->job = g_strdup_printf("%s › %s", row->workflow, row->job);
            summary->started_at = row->started_epoch;
            g_hash_table_replace(app->job_by_runner, g_strdup(row->runner), summary);
        }
    }
    app->local_active = result->local_active;
    app->hosted_active = result->hosted_active;
    app->queued = result->queued;
    render_activity(app);
    update_summary(app);
    char *scan = g_strdup_printf(
        "Runner poll %us  •  Activity scan %us  •  %u repositories scanned",
        app->config.runner_poll_seconds, app->config.activity_scan_seconds,
        result->repos_scanned);
    gtk_label_set_text(GTK_LABEL(app->scan_label), scan);
    g_free(scan);

    g_ptr_array_unref(result->rows);
    g_atomic_int_set(&app->activity_refreshing, 0);
    g_free(result);
    request_runner_refresh(app);
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
        g_thread_join(app->local_thread);
        app->local_thread = NULL;
    }
    if (result->error) {
        gtk_label_set_text(GTK_LABEL(app->status_label), result->error);
        g_free(result->error);
        if (result->rows) g_ptr_array_unref(result->rows);
        g_atomic_int_set(&app->local_refreshing, 0);
        g_free(result);
        return G_SOURCE_REMOVE;
    }
    g_ptr_array_set_size(app->local_rows, 0U);
    for (guint i = 0U; i < result->rows->len; i++) {
        LocalRow *src = g_ptr_array_index(result->rows, i);
        LocalRow *row = g_new0(LocalRow, 1U);
        row->runner=g_strdup(src->runner); row->service_state=g_strdup(src->service_state);
        row->github_state=g_strdup(src->github_state); row->pid=g_strdup(src->pid);
        row->start_mode=g_strdup(src->start_mode); row->account=g_strdup(src->account);
        row->diag=g_strdup(src->diag); row->diag_age=g_strdup(src->diag_age);
        row->path=g_strdup(src->path); row->diag_path=g_strdup(src->diag_path);
        row->service_name=g_strdup(src->service_name);
        for (guint runner_index = 0U; runner_index < app->runner_rows->len; runner_index++) {
            RunnerRow *runner = g_ptr_array_index(app->runner_rows, runner_index);
            char *service_lower = g_ascii_strdown(row->service_name, -1);
            char *description_lower = g_ascii_strdown(row->runner, -1);
            char *runner_lower = g_ascii_strdown(runner->name, -1);
            const gboolean matches =
                strstr(service_lower, runner_lower) != NULL ||
                strstr(description_lower, runner_lower) != NULL;
            g_free(service_lower); g_free(description_lower); g_free(runner_lower);
            if (matches) {
                g_free(row->runner); row->runner = g_strdup(runner->name);
                g_free(row->github_state); row->github_state = g_strdup(runner->state);
                break;
            }
        }
        g_ptr_array_add(app->local_rows, row);
    }
    render_local(app);
    g_ptr_array_unref(result->rows);
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
    request_activity_refresh(data);
    return G_SOURCE_CONTINUE;
}

static gboolean local_timer_cb(gpointer data)
{
    request_local_refresh(data);
    return G_SOURCE_CONTINUE;
}

static gboolean tick_timer_cb(gpointer data)
{
    RunnerScopeApp *app = data;
    const double wall_now = (double)time(NULL);
    for (guint i = 0U; i < app->activity_rows->len; i++) {
        ActivityRow *row = g_ptr_array_index(app->activity_rows, i);
        if (row->started_epoch > 0.0) {
            g_free(row->runtime);
            row->runtime = duration_text(MAX(0.0, wall_now - row->started_epoch));
        }
    }
    render_runners(app);
    render_activity(app);
    update_summary(app);
    return G_SOURCE_CONTINUE;
}

static void on_filter_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    RunnerScopeApp *app = user_data;
    render_runners(app); render_activity(app); render_history(app); render_local(app);
}

static void on_refresh(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    request_runner_refresh(app);
    request_activity_refresh(app);
    request_local_refresh(app);
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
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);
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
        app->config.runner_poll_seconds = MAX(1U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[2])), NULL, 10));
        app->config.activity_scan_seconds = MAX(10U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[3])), NULL, 10));
        app->config.repository_scan_limit = MAX(1U, (guint)g_ascii_strtoull(
            gtk_entry_get_text(GTK_ENTRY(entries[4])), NULL, 10));
        app->config.local_health_seconds = MAX(5U, (guint)g_ascii_strtoull(
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
    if (saved && app->window) {
        apply_theme(app);
        for (gint mode = INFILTRATR_THEME_SYSTEM;
             mode <= INFILTRATR_THEME_NIGHT; mode++) {
            GtkWidget *item = app->theme_menu_items[mode];
            if (item)
                gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(item),
                    mode == (gint)app->config.theme_mode);
        }
    }
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


static void on_menu_refresh(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    on_refresh(NULL, user_data);
}

static void on_menu_export(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    on_export(NULL, user_data);
}

static void on_menu_settings(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    on_settings(NULL, user_data);
}

static void on_menu_quit(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    RunnerScopeApp *app = user_data;
    if (app && app->application)
        g_application_quit(G_APPLICATION(app->application));
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

static void on_theme_menu_selected(
    GtkCheckMenuItem *item, gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    RunnerScopeApp *app = user_data;
    if (!app) return;

    const gint mode = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(item), "runner-monitor-theme-mode"));
    if (mode < INFILTRATR_THEME_SYSTEM || mode > INFILTRATR_THEME_NIGHT)
        return;
    if ((gint)app->config.theme_mode == mode) return;

    app->config.theme_mode = (InfiltratrThemeMode)mode;
    GError *error = NULL;
    if (!save_config(&app->config, &error)) {
        if (app->status_label)
            gtk_label_set_text(
                GTK_LABEL(app->status_label),
                error ? error->message : "Unable to save theme setting");
        g_clear_error(&error);
    }
    apply_theme(app);
}

static void on_menu_about(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
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

static GtkWidget *menu_item(
    const char *label, GCallback callback, gpointer user_data)
{
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
    if (callback)
        g_signal_connect(item, "activate", callback, user_data);
    return item;
}

static GtkWidget *build_menu_bar(RunnerScopeApp *app)
{
    GtkWidget *bar = gtk_menu_bar_new();

    GtkWidget *file_root = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_shell_append(
        GTK_MENU_SHELL(file_menu),
        menu_item("_Export CSV…", G_CALLBACK(on_menu_export), app));
    gtk_menu_shell_append(
        GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(
        GTK_MENU_SHELL(file_menu),
        menu_item("_Quit", G_CALLBACK(on_menu_quit), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_root), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), file_root);

    GtkWidget *edit_root = gtk_menu_item_new_with_mnemonic("_Edit");
    GtkWidget *edit_menu = gtk_menu_new();
    gtk_menu_shell_append(
        GTK_MENU_SHELL(edit_menu),
        menu_item("_Settings…", G_CALLBACK(on_menu_settings), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_root), edit_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), edit_root);

    GtkWidget *view_root = gtk_menu_item_new_with_mnemonic("_View");
    GtkWidget *view_menu = gtk_menu_new();
    gtk_menu_shell_append(
        GTK_MENU_SHELL(view_menu),
        menu_item("_Refresh now", G_CALLBACK(on_menu_refresh), app));
    gtk_menu_shell_append(
        GTK_MENU_SHELL(view_menu), gtk_separator_menu_item_new());

    GtkWidget *theme_root = gtk_menu_item_new_with_mnemonic("_Theme");
    GtkWidget *theme_menu = gtk_menu_new();
    GSList *theme_group = NULL;
    for (gint mode = INFILTRATR_THEME_SYSTEM;
         mode <= INFILTRATR_THEME_NIGHT; mode++) {
        const char *label = mode == INFILTRATR_THEME_SYSTEM
            ? "Follow system"
            : infiltratr_theme_mode_name((InfiltratrThemeMode)mode);
        GtkWidget *radio = gtk_radio_menu_item_new_with_label(theme_group, label);
        theme_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(radio));
        g_object_set_data(
            G_OBJECT(radio), "runner-monitor-theme-mode",
            GINT_TO_POINTER(mode));
        g_signal_connect(
            radio, "toggled", G_CALLBACK(on_theme_menu_selected), app);
        app->theme_menu_items[mode] = radio;
        gtk_menu_shell_append(GTK_MENU_SHELL(theme_menu), radio);
    }
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(
            app->theme_menu_items[(gint)app->config.theme_mode]), TRUE);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(theme_root), theme_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), theme_root);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_root), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), view_root);

    GtkWidget *help_root = gtk_menu_item_new_with_mnemonic("_Help");
    GtkWidget *help_menu = gtk_menu_new();
    gtk_menu_shell_append(
        GTK_MENU_SHELL(help_menu),
        menu_item("_About Runner Monitor", G_CALLBACK(on_menu_about), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_root), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), help_root);

    return bar;
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

static void on_restart(GtkButton *button, gpointer user_data)
{
    (void)button;
    RunnerScopeApp *app = user_data;
    char *service = NULL;
    if (!selected_string(app->local_tree, LOCAL_COL_SERVICE_NAME, &service)) return;

    GtkWidget *confirm = gtk_message_dialog_new(
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "Restart %s?\n\nIf the runner is executing a job, that job will be interrupted.",
        service);
    const gint response = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (response != GTK_RESPONSE_YES) { g_free(service); return; }

    char *argv[] = {(char *)"pkexec", (char *)"systemctl",
                    (char *)"restart", service, NULL};
    GError *error = NULL;
    char *output = run_command(argv, &error);
    if (!output) {
        char *message = g_strdup_printf("Runner restart failed: %s",
            error ? error->message : "unknown error");
        gtk_label_set_text(GTK_LABEL(app->status_label), message);
        g_free(message); g_clear_error(&error);
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Runner service restarted.");
        g_free(output);
        request_local_refresh(app);
    }
    g_free(service);
}

static void on_activity_selection(GtkTreeSelection *selection, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    gtk_widget_set_sensitive(app->open_job_button,
        gtk_tree_selection_get_selected(selection, &model, &iter));
}

static void on_local_selection(GtkTreeSelection *selection, gpointer user_data)
{
    RunnerScopeApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    const gboolean selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_widget_set_sensitive(app->open_diag_button, selected);
    gtk_widget_set_sensitive(app->restart_button, selected);
}

static GtkWidget *make_counter(const char *name, const char *css_class)
{
    GtkWidget *label = gtk_label_new(name);
    GtkStyleContext *context = gtk_widget_get_style_context(label);
    gtk_style_context_add_class(context, "counter");
    if (css_class) gtk_style_context_add_class(context, css_class);
    return label;
}

static void build_ui(RunnerScopeApp *app)
{
    app->window = gtk_application_window_new(app->application);
    gtk_window_set_title(GTK_WINDOW(app->window), "Runner Monitor");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1260, 720);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), "runnerscope");

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 20);
    gtk_container_add(GTK_CONTAINER(app->window), outer);

    GtkWidget *menu_bar = build_menu_bar(app);
    gtk_box_pack_start(GTK_BOX(outer), menu_bar, FALSE, FALSE, 0);

    GtkWidget *heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(outer), heading, FALSE, FALSE, 0);
    GtkWidget *title = gtk_label_new("GitHub Self-Hosted Runner Monitor");
    gtk_widget_set_name(title, "title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(heading), title, FALSE, FALSE, 0);
    app->updated_label = gtk_label_new("Runner data: —    Activity: —");
    gtk_widget_set_name(app->updated_label, "meta");
    gtk_widget_set_halign(app->updated_label, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(heading), app->updated_label, FALSE, FALSE, 0);

    GtkWidget *meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_pack_start(GTK_BOX(outer), meta, FALSE, FALSE, 0);
    char *org_text = g_strdup_printf("Organisation: %s", app->config.organisation);
    GtkWidget *org = gtk_label_new(org_text); g_free(org_text);
    gtk_box_pack_start(GTK_BOX(meta), org, FALSE, FALSE, 0);
    app->scan_label = gtk_label_new("Preparing activity scan…");
    gtk_widget_set_name(app->scan_label, "meta");
    gtk_box_pack_start(GTK_BOX(meta), app->scan_label, FALSE, FALSE, 0);

    GtkWidget *counters = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(outer), counters, FALSE, FALSE, 0);
    const char *counter_names[] = {
        "TOTAL  0", "RUNNING  0", "IDLE  0", "OFFLINE  0",
        "LOCAL ACTIVE  0", "GITHUB ACTIVE  0", "QUEUED  0"
    };
    const char *counter_classes[] = {
        NULL, "counter-running", "counter-idle", "counter-offline",
        "counter-local", "counter-hosted", "counter-queued"
    };
    for (guint i = 0U; i < 7U; i++) {
        app->counter_labels[i] = make_counter(counter_names[i], counter_classes[i]);
        gtk_box_pack_start(GTK_BOX(counters), app->counter_labels[i], FALSE, FALSE, 0);
    }

    app->summary_label = gtk_label_new("Observed this session: —");
    gtk_widget_set_name(app->summary_label, "summary");
    gtk_widget_set_halign(app->summary_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), app->summary_label, FALSE, FALSE, 0);

    GtkWidget *filter = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(outer), filter, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(filter), gtk_label_new("Filter:"), FALSE, FALSE, 0);
    app->filter_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->filter_entry), "Filter selected data");
    gtk_box_pack_start(GTK_BOX(filter), app->filter_entry, FALSE, FALSE, 0);
    g_signal_connect(app->filter_entry, "changed", G_CALLBACK(on_filter_changed), app);

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

    app->notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(app->notebook, TRUE);
    gtk_widget_set_vexpand(app->notebook, TRUE);
    gtk_box_pack_start(GTK_BOX(outer), app->notebook, TRUE, TRUE, 0);

    GtkWidget *scroll = scrolled_tree(app->runner_store);
    app->runner_tree = gtk_bin_get_child(GTK_BIN(scroll));
    const char *runner_titles[] = {"Runner","OS","State","Repository","Current job",
        "Runtime","State for","Jobs","Busy","Labels"};
    const gint runner_widths[] = {180,65,80,140,250,90,80,50,55,280};
    for (gint i = 0; i < RUNNER_N_COLS; i++)
        tree_add_text_column(app->runner_tree, runner_titles[i], i, runner_widths[i]);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), scroll, gtk_label_new("Runners"));

    scroll = scrolled_tree(app->activity_store);
    app->activity_tree = gtk_bin_get_child(GTK_BIN(scroll));
    const char *act_titles[] = {"Where","Repository","Workflow","Job","Current step",
        "Status","Runner","Runtime","Event","Branch"};
    const gint act_widths[] = {85,140,180,210,180,90,170,90,70,100};
    for (gint i = 0; i < 10; i++)
        tree_add_text_column(app->activity_tree, act_titles[i], i, act_widths[i]);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), scroll, gtk_label_new("Active jobs"));
    GtkTreeSelection *selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(app->activity_tree));
    g_signal_connect(selection, "changed", G_CALLBACK(on_activity_selection), app);

    scroll = scrolled_tree(app->history_store);
    app->history_tree = gtk_bin_get_child(GTK_BIN(scroll));
    tree_add_text_column(app->history_tree, "Time", HIST_COL_TIME, 110);
    tree_add_text_column(app->history_tree, "Runner", HIST_COL_RUNNER, 180);
    tree_add_text_column(app->history_tree, "Event", HIST_COL_EVENT, 100);
    tree_add_text_column(app->history_tree, "Detail", HIST_COL_DETAIL, 500);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), scroll, gtk_label_new("History"));

    scroll = scrolled_tree(app->local_store);
    app->local_tree = gtk_bin_get_child(GTK_BIN(scroll));
    const char *local_titles[] = {"Runner","Service","GitHub","PID","Start","Account",
        "Latest diagnostic","Age","Path"};
    const gint local_widths[] = {180,80,80,60,80,100,180,80,300};
    for (gint i = 0; i < 9; i++)
        tree_add_text_column(app->local_tree, local_titles[i], i, local_widths[i]);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), scroll,
                             gtk_label_new("Local Linux health"));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->local_tree));
    g_signal_connect(selection, "changed", G_CALLBACK(on_local_selection), app);

    GtkWidget *action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(action_row, "footer-actions");
    gtk_box_pack_start(GTK_BOX(outer), action_row, FALSE, FALSE, 0);

    GtkWidget *context_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(action_row), context_actions, FALSE, FALSE, 0);

    app->open_job_button = gtk_button_new_with_label("Open selected job");
    gtk_widget_set_sensitive(app->open_job_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->open_job_button, "Select an active job to open it in GitHub.");
    g_signal_connect(app->open_job_button, "clicked", G_CALLBACK(on_open_job), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->open_job_button, FALSE, FALSE, 0);

    app->open_diag_button = gtk_button_new_with_label("Open _diag");
    gtk_widget_set_sensitive(app->open_diag_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->open_diag_button, "Select a local runner to open its latest diagnostic.");
    g_signal_connect(app->open_diag_button, "clicked", G_CALLBACK(on_open_diag), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->open_diag_button, FALSE, FALSE, 0);

    app->restart_button = gtk_button_new_with_label("Restart selected runner");
    gtk_widget_set_sensitive(app->restart_button, FALSE);
    gtk_widget_set_tooltip_text(
        app->restart_button, "Select a local runner before restarting its service.");
    g_signal_connect(app->restart_button, "clicked", G_CALLBACK(on_restart), app);
    gtk_box_pack_start(
        GTK_BOX(context_actions), app->restart_button, FALSE, FALSE, 0);

    GtkWidget *general_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_end(GTK_BOX(action_row), general_actions, FALSE, FALSE, 0);

    GtkWidget *button = gtk_button_new_with_label("Export CSV");
    g_signal_connect(button, "clicked", G_CALLBACK(on_export), app);
    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);

    button = gtk_button_new_with_label("Settings");
    g_signal_connect(button, "clicked", G_CALLBACK(on_settings), app);
    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);

    button = gtk_button_new_with_label("Refresh now");
    g_signal_connect(button, "clicked", G_CALLBACK(on_refresh), app);
    gtk_box_pack_start(GTK_BOX(general_actions), button, FALSE, FALSE, 0);

    GtkWidget *status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(outer), status_row, FALSE, FALSE, 0);
    app->status_label = gtk_label_new("Starting…");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(app->status_label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(app->status_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(status_row), app->status_label, TRUE, TRUE, 0);

    GtkWidget *version_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(version_box, GTK_ALIGN_END);
    char *app_version = g_strdup_printf("Runner Monitor %s", RUNNERSCOPE_VERSION);
    GtkWidget *app_version_label = gtk_label_new(app_version);
    g_free(app_version);
    gtk_widget_set_halign(app_version_label, GTK_ALIGN_END);
    gtk_widget_set_name(app_version_label, "meta");
    gtk_box_pack_start(GTK_BOX(version_box), app_version_label, FALSE, FALSE, 0);
    char *common_version = g_strdup_printf("Common %s", INFILTRATR_COMMON_VERSION);
    GtkWidget *common_version_label = gtk_label_new(common_version);
    g_free(common_version);
    gtk_widget_set_halign(common_version_label, GTK_ALIGN_END);
    gtk_widget_set_name(common_version_label, "meta");
    gtk_box_pack_start(GTK_BOX(version_box), common_version_label, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(status_row), version_box, FALSE, FALSE, 0);

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
}

static InfiltratrProjectInfo project_info(void)
{
    InfiltratrProjectInfo info = INFILTRATR_PROJECT_INFO_INIT;
    info.program_name = "Runner Monitor";
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
    if (!INFILTRATR_COMMON_VERSION[0]) return 1;
    const InfiltratrThemePalette *day =
        infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false);
    const InfiltratrThemePalette *night =
        infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true);
    if (!day || !night || day->background_rgb == night->background_rgb)
        return 2;
    char duration[64];
    if (!infiltratr_format_duration_compact(true, 3661U, duration, sizeof(duration)))
        return 3;
    if (strstr(duration, "1h") == NULL) return 4;
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
    render_history(app);
    request_runner_refresh(app);
    request_activity_refresh(app);
    request_local_refresh(app);
    app->runner_timer = g_timeout_add_seconds(
        app->config.runner_poll_seconds, runner_timer_cb, app);
    app->activity_timer = g_timeout_add_seconds(
        app->config.activity_scan_seconds, activity_timer_cb, app);
    app->local_timer = g_timeout_add_seconds(
        app->config.local_health_seconds, local_timer_cb, app);
    app->tick_timer = g_timeout_add_seconds(1U, tick_timer_cb, app);
}

static void app_init(RunnerScopeApp *app)
{
    memset(app, 0, sizeof(*app));
    load_config(&app->config);
    app->sessions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, session_free);
    app->job_by_runner = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, job_summary_free);
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
    g_hash_table_unref(app->sessions);
    g_hash_table_unref(app->job_by_runner);
    g_ptr_array_unref(app->runner_rows);
    g_ptr_array_unref(app->activity_rows);
    g_ptr_array_unref(app->history_rows);
    g_ptr_array_unref(app->local_rows);
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
