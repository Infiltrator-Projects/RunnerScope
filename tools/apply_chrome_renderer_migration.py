#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8")


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f"missing replacement anchor: {label}")
    if text.count(old) != 1:
        raise SystemExit(f"replacement anchor not unique: {label}")
    return text.replace(old, new, 1)


# 1) Extend the thin renderer seam with shared header/footer emission.
write("src/ui/ui_renderer.h", r'''// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_UI_RENDERER_H
#define RUNNERSCOPE_UI_RENDERER_H

#include "ui_contract.h"

/*
 * Thin native-renderer adapter. Application structure is emitted once by the
 * shared layer; GTK/Win32 callbacks translate that structure into native UI.
 * New surfaces are added here only as they migrate, avoiding a general-purpose
 * widget toolkit.
 */
typedef enum {
    RUNNER_UI_CHROME_SETTINGS = 0,
    RUNNER_UI_CHROME_MINIMIZE,
    RUNNER_UI_CHROME_MAXIMIZE,
    RUNNER_UI_CHROME_CLOSE,
    RUNNER_UI_CHROME_EXPORT,
    RUNNER_UI_CHROME_ABOUT,
    RUNNER_UI_CHROME_REFRESH
} RunnerUiChromeActionId;

typedef struct {
    const char *status_text;
    const char *app_version;
    const char *common_version;
    bool refreshing;
} RunnerUiFooterState;

typedef struct {
    void *context;

    bool (*begin_header)(void *context,
                         const char *product_title,
                         const char *product_family);
    bool (*header_action)(void *context,
                          RunnerUiChromeActionId action,
                          const char *label,
                          const char *tooltip,
                          bool primary);
    bool (*end_header)(void *context);

    bool (*begin_navigation)(void *context,
                             size_t item_count,
                             RunnerUiPageId selected_page);
    bool (*navigation_separator)(void *context,
                                 const RunnerUiPageSpec *before_page);
    bool (*navigation_item)(void *context,
                            const RunnerUiPageSpec *page,
                            const char *platform_label,
                            bool selected);
    bool (*end_navigation)(void *context);

    bool (*begin_footer)(void *context);
    bool (*footer_action)(void *context,
                          RunnerUiChromeActionId action,
                          const char *label,
                          const char *tooltip,
                          bool primary);
    bool (*footer_status)(void *context,
                          const RunnerUiFooterState *state);
    bool (*end_footer)(void *context);
} RunnerUiRenderer;

bool runner_ui_render_header(const RunnerUiRenderer *renderer);
bool runner_ui_render_navigation(const RunnerUiRenderer *renderer,
                                 RunnerUiPlatform platform,
                                 RunnerUiPageId selected_page);
bool runner_ui_render_footer(const RunnerUiRenderer *renderer,
                             const RunnerUiFooterState *state);

#endif
''')

write("src/ui/ui_renderer.c", r'''// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_renderer.h"

typedef struct {
    RunnerUiChromeActionId id;
    const char *label;
    const char *tooltip;
    bool primary;
} RunnerUiChromeActionSpec;

static const RunnerUiChromeActionSpec k_header_actions[] = {
    {RUNNER_UI_CHROME_SETTINGS, "Settings", "Settings", false},
    {RUNNER_UI_CHROME_MINIMIZE, "Minimize", "Minimize", false},
    {RUNNER_UI_CHROME_MAXIMIZE, "Maximize / Restore", "Maximize / Restore", false},
    {RUNNER_UI_CHROME_CLOSE, "Close", "Close", false}
};

static const RunnerUiChromeActionSpec k_footer_actions[] = {
    {RUNNER_UI_CHROME_EXPORT, "Export CSV", "Export the current workspace as CSV.", false},
    {RUNNER_UI_CHROME_ABOUT, "About", "About Runner Monitor.", false},
    {RUNNER_UI_CHROME_REFRESH, "Refresh now", "Refresh the active workspace now.", true}
};

bool runner_ui_render_header(const RunnerUiRenderer *renderer)
{
    if (!renderer || !renderer->begin_header || !renderer->header_action ||
        !renderer->end_header)
        return false;
    if (!renderer->begin_header(renderer->context,
                                runner_ui_product_title(),
                                runner_ui_product_family()))
        return false;
    for (size_t i = 0U;
         i < sizeof(k_header_actions) / sizeof(k_header_actions[0]); ++i) {
        const RunnerUiChromeActionSpec *action = &k_header_actions[i];
        if (!renderer->header_action(renderer->context, action->id,
                                     action->label, action->tooltip,
                                     action->primary))
            return false;
    }
    return renderer->end_header(renderer->context);
}

bool runner_ui_render_navigation(const RunnerUiRenderer *renderer,
                                 RunnerUiPlatform platform,
                                 RunnerUiPageId selected_page)
{
    if (!renderer || !renderer->begin_navigation ||
        !renderer->navigation_separator || !renderer->navigation_item ||
        !renderer->end_navigation)
        return false;
    if ((int)selected_page < 0 || selected_page >= RUNNER_UI_PAGE_COUNT)
        return false;

    if (!renderer->begin_navigation(
            renderer->context, RUNNER_UI_PAGE_COUNT, selected_page))
        return false;

    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        const RunnerUiPageSpec *page = runner_ui_page((RunnerUiPageId)i);
        if (page->separator_before &&
            !renderer->navigation_separator(renderer->context, page))
            return false;
        if (!renderer->navigation_item(
                renderer->context,
                page,
                runner_ui_page_nav_label(page, platform),
                page->id == selected_page))
            return false;
    }

    return renderer->end_navigation(renderer->context);
}

bool runner_ui_render_footer(const RunnerUiRenderer *renderer,
                             const RunnerUiFooterState *state)
{
    if (!renderer || !renderer->begin_footer || !renderer->footer_action ||
        !renderer->footer_status || !renderer->end_footer || !state ||
        !state->status_text || !state->app_version || !state->common_version)
        return false;
    if (!renderer->begin_footer(renderer->context)) return false;
    for (size_t i = 0U;
         i < sizeof(k_footer_actions) / sizeof(k_footer_actions[0]); ++i) {
        RunnerUiChromeActionSpec action = k_footer_actions[i];
        if (action.id == RUNNER_UI_CHROME_REFRESH && state->refreshing)
            action.label = "Refreshing…";
        if (!renderer->footer_action(renderer->context, action.id,
                                     action.label, action.tooltip,
                                     action.primary))
            return false;
    }
    if (!renderer->footer_status(renderer->context, state)) return false;
    return renderer->end_footer(renderer->context);
}
''')

# 2) Add toolkit-free header/footer parity coverage beside navigation coverage.
test = read("tests/ui_contract_test.c")
insert_anchor = "\nint main(void)\n{\n"
recorder = r'''
typedef struct {
    size_t header_begin_count;
    size_t header_action_count;
    size_t header_end_count;
    size_t footer_begin_count;
    size_t footer_action_count;
    size_t footer_status_count;
    size_t footer_end_count;
    RunnerUiChromeActionId header_ids[4];
    RunnerUiChromeActionId footer_ids[3];
    char header_labels[4][64];
    char footer_labels[3][64];
    char product_title[64];
    char product_family[64];
    char status_text[128];
    char app_version[64];
    char common_version[64];
} ChromeRecorder;

static bool record_begin_header(void *context,
                                const char *product_title,
                                const char *product_family)
{
    ChromeRecorder *recorder = context;
    if (!recorder || !product_title || !product_family) return false;
    recorder->header_begin_count++;
    (void)snprintf(recorder->product_title, sizeof(recorder->product_title),
                   "%s", product_title);
    (void)snprintf(recorder->product_family, sizeof(recorder->product_family),
                   "%s", product_family);
    return true;
}

static bool record_header_action(void *context,
                                 RunnerUiChromeActionId action,
                                 const char *label,
                                 const char *tooltip,
                                 bool primary)
{
    ChromeRecorder *recorder = context;
    (void)tooltip;
    (void)primary;
    if (!recorder || !label || recorder->header_action_count >= 4U)
        return false;
    size_t index = recorder->header_action_count++;
    recorder->header_ids[index] = action;
    (void)snprintf(recorder->header_labels[index],
                   sizeof(recorder->header_labels[index]), "%s", label);
    return true;
}

static bool record_end_header(void *context)
{
    ChromeRecorder *recorder = context;
    if (!recorder) return false;
    recorder->header_end_count++;
    return true;
}

static bool record_begin_footer(void *context)
{
    ChromeRecorder *recorder = context;
    if (!recorder) return false;
    recorder->footer_begin_count++;
    return true;
}

static bool record_footer_action(void *context,
                                 RunnerUiChromeActionId action,
                                 const char *label,
                                 const char *tooltip,
                                 bool primary)
{
    ChromeRecorder *recorder = context;
    (void)tooltip;
    (void)primary;
    if (!recorder || !label || recorder->footer_action_count >= 3U)
        return false;
    size_t index = recorder->footer_action_count++;
    recorder->footer_ids[index] = action;
    (void)snprintf(recorder->footer_labels[index],
                   sizeof(recorder->footer_labels[index]), "%s", label);
    return true;
}

static bool record_footer_status(void *context,
                                 const RunnerUiFooterState *state)
{
    ChromeRecorder *recorder = context;
    if (!recorder || !state) return false;
    recorder->footer_status_count++;
    (void)snprintf(recorder->status_text, sizeof(recorder->status_text),
                   "%s", state->status_text);
    (void)snprintf(recorder->app_version, sizeof(recorder->app_version),
                   "%s", state->app_version);
    (void)snprintf(recorder->common_version, sizeof(recorder->common_version),
                   "%s", state->common_version);
    return true;
}

static bool record_end_footer(void *context)
{
    ChromeRecorder *recorder = context;
    if (!recorder) return false;
    recorder->footer_end_count++;
    return true;
}

static int verify_chrome_renderer(void)
{
    ChromeRecorder recorder = {0};
    RunnerUiRenderer renderer = {0};
    renderer.context = &recorder;
    renderer.begin_header = record_begin_header;
    renderer.header_action = record_header_action;
    renderer.end_header = record_end_header;
    renderer.begin_footer = record_begin_footer;
    renderer.footer_action = record_footer_action;
    renderer.footer_status = record_footer_status;
    renderer.end_footer = record_end_footer;

    if (!runner_ui_render_header(&renderer)) return 1;
    if (recorder.header_begin_count != 1U ||
        recorder.header_action_count != 4U ||
        recorder.header_end_count != 1U)
        return 2;
    const RunnerUiChromeActionId expected_header[] = {
        RUNNER_UI_CHROME_SETTINGS, RUNNER_UI_CHROME_MINIMIZE,
        RUNNER_UI_CHROME_MAXIMIZE, RUNNER_UI_CHROME_CLOSE
    };
    for (size_t i = 0U; i < 4U; ++i)
        if (recorder.header_ids[i] != expected_header[i]) return 3;
    if (strcmp(recorder.product_title, "Runner Monitor") != 0 ||
        strcmp(recorder.product_family, "Infiltrator OS") != 0)
        return 4;

    RunnerUiFooterState state = {
        "Ready", "Runner Monitor test", "Common test", false
    };
    if (!runner_ui_render_footer(&renderer, &state)) return 5;
    if (recorder.footer_begin_count != 1U ||
        recorder.footer_action_count != 3U ||
        recorder.footer_status_count != 1U ||
        recorder.footer_end_count != 1U)
        return 6;
    const RunnerUiChromeActionId expected_footer[] = {
        RUNNER_UI_CHROME_EXPORT, RUNNER_UI_CHROME_ABOUT,
        RUNNER_UI_CHROME_REFRESH
    };
    for (size_t i = 0U; i < 3U; ++i)
        if (recorder.footer_ids[i] != expected_footer[i]) return 7;
    if (strcmp(recorder.footer_labels[2], "Refresh now") != 0 ||
        strcmp(recorder.status_text, "Ready") != 0 ||
        strcmp(recorder.app_version, "Runner Monitor test") != 0 ||
        strcmp(recorder.common_version, "Common test") != 0)
        return 8;

    ChromeRecorder refreshing = {0};
    renderer.context = &refreshing;
    state.refreshing = true;
    if (!runner_ui_render_footer(&renderer, &state)) return 9;
    if (strcmp(refreshing.footer_labels[2], "Refreshing…") != 0) return 10;

    RunnerUiRenderer incomplete = {0};
    if (runner_ui_render_header(&incomplete)) return 11;
    if (runner_ui_render_footer(&incomplete, &state)) return 12;
    return 0;
}
'''
test = replace_once(test, insert_anchor, "\n" + recorder + insert_anchor, "test recorder insertion")
old_tail = r'''    RunnerUiRenderer incomplete_renderer = {0};
    if (runner_ui_render_navigation(
            &incomplete_renderer, RUNNER_UI_PLATFORM_LINUX,
            RUNNER_UI_PAGE_RUNNERS))
        return 15;

    puts("Runner Monitor shared UI contract and navigation renderer passed.");
    return 0;
}'''
new_tail = r'''    RunnerUiRenderer incomplete_renderer = {0};
    if (runner_ui_render_navigation(
            &incomplete_renderer, RUNNER_UI_PLATFORM_LINUX,
            RUNNER_UI_PAGE_RUNNERS))
        return 15;
    if (verify_chrome_renderer() != 0) return 16;

    puts("Runner Monitor shared UI contract, navigation and shell chrome renderers passed.");
    return 0;
}'''
test = replace_once(test, old_tail, new_tail, "test main tail")
write("tests/ui_contract_test.c", test)

# 3) GTK: translate the shared header/footer contract into native GTK widgets.
linux = read("src/linux_main.c")
nav_context_anchor = r'''
typedef struct {
    RunnerScopeApp *app;
    GtkWidget *nav_rail;
} GtkNavigationRendererContext;
'''
gtk_chrome = r'''
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

'''
linux = replace_once(linux, nav_context_anchor, gtk_chrome + nav_context_anchor,
                     "GTK chrome adapter insertion")

header_pattern = re.compile(
    r'''    /\*\n     \* Match the current Infiltrator OS shell: product identity, settings and\n     \* window controls stay in the header; filtering belongs to the workspace\.\n     \*/\n.*?    gtk_window_set_titlebar\(GTK_WINDOW\(app->window\), header\);\n''',
    re.S)
header_replacement = r'''    GtkChromeRendererContext chrome_context = {0};
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
'''
linux, count = header_pattern.subn(header_replacement, linux, count=1)
if count != 1:
    raise SystemExit(f"GTK header replacement count {count}")

footer_pattern = re.compile(
    r'''    GtkWidget \*footer = gtk_box_new\(GTK_ORIENTATION_VERTICAL, \(gint\)compact_spacing\);\n.*?    gtk_box_pack_end\(\n        GTK_BOX\(status_row\), version_box, FALSE, FALSE, 0\);\n''',
    re.S)
footer_replacement = r'''    chrome_context.outer = outer;
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
'''
linux, count = footer_pattern.subn(footer_replacement, linux, count=1)
if count != 1:
    raise SystemExit(f"GTK footer replacement count {count}")
write("src/linux_main.c", linux)

# 4) Win32: draw the same header/footer emission through native GDI adapters.
win = read("src/windows_main.c")
header_start = win.index("static void draw_header(HDC dc)\n{")
header_end = win.index("\n\ntypedef struct {\n    HDC dc;\n} WinNavigationRendererContext;", header_start)
win_header = r'''typedef struct {
    HDC dc;
    RECT client;
    RECT footer;
} WinChromeRendererContext;

static bool win_renderer_begin_header(void *context,
                                      const char *product_title,
                                      const char *product_family)
{
    WinChromeRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !product_title || !product_family)
        return false;
    fill_rect_color(renderer->dc, g_header_rect, rgb(g_palette.titlebar_rgb));
    RECT accent = g_header_rect;
    accent.right = sx(210);
    fill_rect_color(renderer->dc, accent,
                    mix_color(g_palette.titlebar_rgb,
                              g_palette.neutral_accent_rgb, 24U));

    RECT icon_well = {sx(16), sx(10), sx(58), sx(50)};
    fill_round_rect(renderer->dc, icon_well,
                    mix_color(g_palette.card_rgb,
                              g_palette.neutral_accent_rgb, 18U),
                    rgb(g_palette.neutral_accent_rgb), sx(10));
    RECT icon = {sx(20), sx(12), sx(54), sx(48)};
    draw_image(renderer->dc, &g_nav_images[0], &icon);

    wchar_t title_text[96];
    wchar_t family_text[96];
    utf8_to_wide(product_title, title_text, 96U);
    utf8_to_wide(product_family, family_text, 96U);
    RECT title = {sx(70), sx(8), sx(370), sx(34)};
    draw_text(renderer->dc, title_text, title, g_font_brand,
              rgb(g_palette.title_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = {sx(70), sx(32), sx(370), sx(52)};
    draw_text(renderer->dc, family_text, subtitle, g_font_small,
              rgb(g_palette.muted_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    return true;
}

static bool win_renderer_header_action(void *context,
                                       RunnerUiChromeActionId action,
                                       const char *label,
                                       const char *tooltip,
                                       bool primary)
{
    WinChromeRendererContext *renderer = context;
    (void)label;
    (void)tooltip;
    (void)primary;
    if (!renderer || !renderer->dc) return false;

    RECT rect;
    int icon = 0;
    switch (action) {
    case RUNNER_UI_CHROME_SETTINGS:
        rect = g_settings_rect;
        fill_round_rect(renderer->dc, rect,
                        mix_color(g_palette.surface_rgb,
                                  g_palette.neutral_accent_rgb, 8U),
                        rgb(g_palette.connection_border_rgb), sx(7));
        draw_outline_icon(renderer->dc, rect, 3,
                          rgb(g_palette.button_foreground_rgb));
        return true;
    case RUNNER_UI_CHROME_MINIMIZE:
        rect = g_minimize_rect;
        icon = 0;
        break;
    case RUNNER_UI_CHROME_MAXIMIZE:
        rect = g_maximize_rect;
        icon = 1;
        break;
    case RUNNER_UI_CHROME_CLOSE:
        rect = g_close_rect;
        icon = 2;
        break;
    default:
        return false;
    }
    fill_round_rect(renderer->dc, rect, rgb(g_palette.surface_rgb),
                    rgb(g_palette.connection_border_rgb), sx(6));
    draw_outline_icon(renderer->dc, rect, icon,
                      rgb(g_palette.button_foreground_rgb));
    return true;
}

static bool win_renderer_end_header(void *context)
{
    return context != NULL;
}

static void draw_header(HDC dc)
{
    WinChromeRendererContext chrome = {0};
    chrome.dc = dc;
    RunnerUiRenderer renderer = {0};
    renderer.context = &chrome;
    renderer.begin_header = win_renderer_begin_header;
    renderer.header_action = win_renderer_header_action;
    renderer.end_header = win_renderer_end_header;
    (void)runner_ui_render_header(&renderer);
}
'''
win = win[:header_start] + win_header + win[header_end:]

footer_start = win.index("static void draw_footer(HDC dc, RECT client)\n{")
footer_end = win.index("\n\nstatic void paint_ui(HDC dc)", footer_start)
win_footer = r'''static bool win_renderer_begin_footer(void *context)
{
    WinChromeRendererContext *renderer = context;
    if (!renderer || !renderer->dc) return false;
    int footer_top = renderer->client.bottom - sx(96);
    renderer->footer = (RECT){
        sx(20), footer_top,
        renderer->client.right - sx(20), renderer->client.bottom - sx(12)
    };
    fill_round_rect(renderer->dc, renderer->footer,
                    rgb(g_palette.panel_rgb),
                    rgb(g_palette.connection_border_rgb), sx(12));
    return true;
}

static bool win_renderer_footer_action(void *context,
                                       RunnerUiChromeActionId action,
                                       const char *label,
                                       const char *tooltip,
                                       bool primary)
{
    WinChromeRendererContext *renderer = context;
    (void)tooltip;
    if (!renderer || !renderer->dc || !label) return false;
    RECT rect;
    switch (action) {
    case RUNNER_UI_CHROME_EXPORT:
        rect = g_export_rect;
        break;
    case RUNNER_UI_CHROME_ABOUT:
        rect = g_about_rect;
        break;
    case RUNNER_UI_CHROME_REFRESH:
        rect = g_refresh_rect;
        break;
    default:
        return false;
    }
    wchar_t text[96];
    utf8_to_wide(label, text, sizeof(text) / sizeof(text[0]));
    draw_button(renderer->dc, rect, text, false, primary);
    return true;
}

static bool win_renderer_footer_status(void *context,
                                       const RunnerUiFooterState *state)
{
    WinChromeRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !state) return false;

    wchar_t status_text[256];
    utf8_to_wide(state->status_text, status_text,
                 sizeof(status_text) / sizeof(status_text[0]));
    RECT status = {
        renderer->footer.left + sx(12), renderer->footer.top + sx(50),
        renderer->footer.right - sx(340), renderer->footer.bottom - sx(8)
    };
    draw_text(renderer->dc, status_text, status, g_font,
              rgb(g_palette.text_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    wchar_t version[128];
    utf8_to_wide(state->app_version, version,
                 sizeof(version) / sizeof(version[0]));
    RECT app_version = {
        renderer->footer.right - sx(310), renderer->footer.top + sx(48),
        renderer->footer.right - sx(12), renderer->footer.top + sx(67)
    };
    draw_text(renderer->dc, version, app_version, g_font_small,
              rgb(g_palette.muted_rgb),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    utf8_to_wide(state->common_version, version,
                 sizeof(version) / sizeof(version[0]));
    RECT common_version = {
        app_version.left, renderer->footer.top + sx(66),
        app_version.right, renderer->footer.bottom - sx(5)
    };
    draw_text(renderer->dc, version, common_version, g_font_small,
              rgb(g_palette.muted_rgb),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    return true;
}

static bool win_renderer_end_footer(void *context)
{
    return context != NULL;
}

static void draw_footer(HDC dc, RECT client)
{
    char status[256];
    if (g_refreshing) {
        infiltratr_copy_string(
            status, sizeof(status), "Refreshing GitHub runner state…");
    } else if (!g_organisation[0]) {
        infiltratr_copy_string(
            status, sizeof(status),
            "Open Settings and enter the GitHub organisation.");
    } else {
        char organisation[128] = "";
        wide_to_utf8(g_organisation, organisation, sizeof(organisation));
        (void)snprintf(status, sizeof(status), "%zu runner%s loaded from %s.",
                       g_row_count, g_row_count == 1U ? "" : "s",
                       organisation);
    }

    char app_version[128];
    char common_version[128];
    (void)snprintf(app_version, sizeof(app_version),
                   "Runner Monitor %s", RUNNERSCOPE_VERSION);
    (void)snprintf(common_version, sizeof(common_version),
                   "Common %s", INFILTRATR_COMMON_VERSION);
    RunnerUiFooterState state = {
        status, app_version, common_version, g_refreshing
    };

    WinChromeRendererContext chrome = {0};
    chrome.dc = dc;
    chrome.client = client;
    RunnerUiRenderer renderer = {0};
    renderer.context = &chrome;
    renderer.begin_footer = win_renderer_begin_footer;
    renderer.footer_action = win_renderer_footer_action;
    renderer.footer_status = win_renderer_footer_status;
    renderer.end_footer = win_renderer_end_footer;
    (void)runner_ui_render_footer(&renderer, &state);
}
'''
win = win[:footer_start] + win_footer + win[footer_end:]
write("src/windows_main.c", win)

# The migration helpers must not survive into the product commit.
for transient in [
    ROOT / ".github/workflows/apply-chrome-renderer-migration.yml",
    ROOT / "tools/apply_chrome_renderer_migration.py",
]:
    if transient.exists():
        transient.unlink()
