// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/ui_contract.h"

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

    puts("Runner Monitor shared UI contract passed.");
    return 0;
}
