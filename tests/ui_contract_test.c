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

    puts("Runner Monitor shared UI contract and navigation renderer passed.");
    return 0;
}
