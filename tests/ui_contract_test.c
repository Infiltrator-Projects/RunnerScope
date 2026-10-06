// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/ui_contract.h"
#include "ui/ui_renderer.h"

#include <stdio.h>
#include <string.h>

static int require_component(const char *id)
{
    size_t count = 0U;
    const RunnerUiComponentSpec *components = runner_ui_components(&count);
    for (size_t i = 0U; i < count; ++i)
        if (strcmp(components[i].id, id) == 0) return 0;
    fprintf(stderr, "missing UI component: %s\n", id);
    return 1;
}


typedef struct {
    size_t begin_count;
    size_t item_count;
    size_t separator_count;
    size_t end_count;
    RunnerUiPageId selected_page;
    RunnerUiPageId item_ids[RUNNER_UI_PAGE_COUNT];
    RunnerUiPageId separator_before;
    bool selected[RUNNER_UI_PAGE_COUNT];
    char labels[RUNNER_UI_PAGE_COUNT][64];
} NavigationRecorder;

static bool record_begin_navigation(void *context,
                                    size_t item_count,
                                    RunnerUiPageId selected_page)
{
    NavigationRecorder *recorder = context;
    if (!recorder || item_count != RUNNER_UI_PAGE_COUNT) return false;
    recorder->begin_count++;
    recorder->selected_page = selected_page;
    return true;
}

static bool record_navigation_separator(
    void *context, const RunnerUiPageSpec *before_page)
{
    NavigationRecorder *recorder = context;
    if (!recorder || !before_page) return false;
    recorder->separator_count++;
    recorder->separator_before = before_page->id;
    return true;
}

static bool record_navigation_item(void *context,
                                   const RunnerUiPageSpec *page,
                                   const char *platform_label,
                                   bool selected)
{
    NavigationRecorder *recorder = context;
    if (!recorder || !page || !platform_label ||
        recorder->item_count >= RUNNER_UI_PAGE_COUNT)
        return false;
    size_t index = recorder->item_count++;
    recorder->item_ids[index] = page->id;
    recorder->selected[index] = selected;
    (void)snprintf(recorder->labels[index], sizeof(recorder->labels[index]),
                   "%s", platform_label);
    return true;
}

static bool record_end_navigation(void *context)
{
    NavigationRecorder *recorder = context;
    if (!recorder) return false;
    recorder->end_count++;
    return true;
}

static int verify_navigation_renderer(RunnerUiPlatform platform,
                                      RunnerUiPageId selected_page,
                                      const char *local_label)
{
    NavigationRecorder recorder = {0};
    RunnerUiRenderer renderer = {0};
    renderer.context = &recorder;
    renderer.begin_navigation = record_begin_navigation;
    renderer.navigation_separator = record_navigation_separator;
    renderer.navigation_item = record_navigation_item;
    renderer.end_navigation = record_end_navigation;

    if (!runner_ui_render_navigation(&renderer, platform, selected_page)) return 1;
    if (recorder.begin_count != 1U || recorder.end_count != 1U) return 2;
    if (recorder.item_count != RUNNER_UI_PAGE_COUNT) return 3;
    if (recorder.separator_count != 1U ||
        recorder.separator_before != RUNNER_UI_PAGE_LOCAL_HEALTH)
        return 4;
    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        if (recorder.item_ids[i] != (RunnerUiPageId)i) return 5;
        if (recorder.selected[i] != ((RunnerUiPageId)i == selected_page)) return 6;
    }
    if (strcmp(recorder.labels[RUNNER_UI_PAGE_LOCAL_HEALTH], local_label) != 0)
        return 7;
    return 0;
}


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

static int verify_page_transition_policy(void)
{
    RunnerUiPageTransition transition;
    if (!runner_ui_plan_page_transition(
            RUNNER_UI_PAGE_RUNNERS, RUNNER_UI_PAGE_ACTIVE_JOBS,
            false, false, &transition))
        return 1;
    if (!transition.changed || !transition.clear_selection ||
        !transition.request_activity_refresh ||
        transition.request_local_refresh || transition.show_search ||
        transition.show_runner_table)
        return 2;

    if (!runner_ui_plan_page_transition(
            RUNNER_UI_PAGE_ACTIVE_JOBS, RUNNER_UI_PAGE_RUNNERS,
            true, true, &transition))
        return 3;
    if (!transition.changed || !transition.show_search ||
        !transition.show_runner_table ||
        !transition.request_activity_refresh ||
        transition.request_local_refresh)
        return 4;

    if (!runner_ui_plan_page_transition(
            RUNNER_UI_PAGE_HISTORY, RUNNER_UI_PAGE_LOCAL_HEALTH,
            false, false, &transition))
        return 5;
    if (!transition.request_local_refresh ||
        transition.request_activity_refresh)
        return 6;

    if (!runner_ui_plan_page_transition(
            RUNNER_UI_PAGE_RUNNERS, RUNNER_UI_PAGE_RUNNERS,
            true, true, &transition))
        return 7;
    if (transition.changed || transition.clear_selection ||
        transition.request_activity_refresh ||
        transition.request_local_refresh || !transition.show_search ||
        !transition.show_runner_table)
        return 8;

    if (runner_ui_plan_page_transition(
            (RunnerUiPageId)-1, RUNNER_UI_PAGE_RUNNERS,
            false, false, &transition))
        return 9;
    if (runner_ui_plan_page_transition(
            RUNNER_UI_PAGE_RUNNERS, RUNNER_UI_PAGE_COUNT,
            false, false, &transition))
        return 10;
    if (runner_ui_plan_page_transition(
            RUNNER_UI_PAGE_RUNNERS, RUNNER_UI_PAGE_HISTORY,
            false, false, NULL))
        return 11;
    return 0;
}

int main(void)
{
    char error[256];
    if (!runner_ui_contract_validate(error, sizeof(error))) {
        fprintf(stderr, "UI contract invalid: %s\n", error);
        return 1;
    }

    if (strcmp(runner_ui_product_title(), "Runner Monitor") != 0) return 2;
    if (strcmp(runner_ui_product_family(), "Infiltrator OS") != 0) return 3;

    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        const RunnerUiPageSpec *page = runner_ui_page((RunnerUiPageId)i);
        if (!page || (int)page->id != i) return 4;
    }

    const RunnerUiPageSpec *runners = runner_ui_page(RUNNER_UI_PAGE_RUNNERS);
    if (!runners->show_view_switch) return 5;
    if (runners->actions != RUNNER_UI_ACTION_NONE) return 6;

    const RunnerUiPageSpec *active = runner_ui_page(RUNNER_UI_PAGE_ACTIVE_JOBS);
    if ((active->actions & RUNNER_UI_ACTION_OPEN_JOB) == 0U) return 7;

    const RunnerUiPageSpec *local = runner_ui_page(RUNNER_UI_PAGE_LOCAL_HEALTH);
    if (!local->separator_before) return 8;
    if ((local->actions & RUNNER_UI_ACTION_OPEN_DIAGNOSTIC) == 0U ||
        (local->actions & RUNNER_UI_ACTION_RESTART_RUNNER) == 0U)
        return 9;
    if (strcmp(runner_ui_page_title(local, RUNNER_UI_PLATFORM_LINUX),
               "Local Linux health") != 0)
        return 10;
    if (strcmp(runner_ui_page_title(local, RUNNER_UI_PLATFORM_WINDOWS),
               "Local Windows health") != 0)
        return 11;

    const char *required[] = {
        "shell", "shell.header", "shell.navigation", "shell.workspace",
        "shell.workspace.hero", "shell.workspace.metrics",
        "shell.workspace.toolbar", "shell.workspace.toolbar.search",
        "shell.workspace.page-host", "page.runners", "page.active-jobs",
        "page.history", "page.local-health", "shell.workspace.selection",
        "shell.footer", "shell.footer.context-actions",
        "shell.footer.general-actions", "shell.footer.status"
    };
    for (size_t i = 0U; i < sizeof(required) / sizeof(required[0]); ++i)
        if (require_component(required[i]) != 0) return 12;

    if (verify_navigation_renderer(
            RUNNER_UI_PLATFORM_LINUX, RUNNER_UI_PAGE_HISTORY,
            "Local Linux health") != 0)
        return 13;
    if (verify_navigation_renderer(
            RUNNER_UI_PLATFORM_WINDOWS, RUNNER_UI_PAGE_ACTIVE_JOBS,
            "Local Windows health") != 0)
        return 14;
    RunnerUiRenderer incomplete_renderer = {0};
    if (runner_ui_render_navigation(
            &incomplete_renderer, RUNNER_UI_PLATFORM_LINUX,
            RUNNER_UI_PAGE_RUNNERS))
        return 15;
    if (verify_chrome_renderer() != 0) return 16;
    if (verify_page_transition_policy() != 0) return 17;

    puts("Runner Monitor shared UI contract, navigation, page transitions and shell chrome renderers passed.");
    return 0;
}
