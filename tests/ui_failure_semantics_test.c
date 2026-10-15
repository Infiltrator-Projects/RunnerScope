// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/ui_contract.h"
#include "ui/ui_renderer.h"

#include <stdbool.h>
#include <stddef.h>

static bool begin_navigation(void *context, size_t item_count,
                             RunnerUiPageId selected_page)
{
    unsigned int *calls = context;
    (void)item_count;
    (void)selected_page;
    if (calls) (*calls)++;
    return true;
}

static bool navigation_separator(void *context,
                                 const RunnerUiPageSpec *before_page)
{
    (void)context;
    return before_page != NULL;
}

static bool navigation_item(void *context,
                            const RunnerUiPageSpec *page,
                            const char *platform_label,
                            bool selected)
{
    (void)context;
    (void)selected;
    return page != NULL && platform_label != NULL && platform_label[0] != '\0';
}

static bool end_navigation(void *context)
{
    (void)context;
    return true;
}

int main(void)
{
    if (runner_ui_page((RunnerUiPageId)-1) != NULL) return 1;
    if (runner_ui_page(RUNNER_UI_PAGE_COUNT) != NULL) return 2;

    const RunnerUiPageSpec *runners = runner_ui_page(RUNNER_UI_PAGE_RUNNERS);
    if (!runners) return 3;
    if (runner_ui_page_nav_label(NULL, RUNNER_UI_PLATFORM_LINUX) != NULL)
        return 4;
    if (runner_ui_page_title(NULL, RUNNER_UI_PLATFORM_WINDOWS) != NULL)
        return 5;
    if (runner_ui_page_nav_label(
            runners, (RunnerUiPlatform)99) != NULL)
        return 6;
    if (runner_ui_page_title(
            runners, (RunnerUiPlatform)99) != NULL)
        return 7;

    unsigned int begin_calls = 0U;
    RunnerUiRenderer renderer = {0};
    renderer.context = &begin_calls;
    renderer.begin_navigation = begin_navigation;
    renderer.navigation_separator = navigation_separator;
    renderer.navigation_item = navigation_item;
    renderer.end_navigation = end_navigation;

    if (runner_ui_render_navigation(
            &renderer, (RunnerUiPlatform)99, RUNNER_UI_PAGE_RUNNERS))
        return 8;
    if (begin_calls != 0U) return 9;

    if (!runner_ui_render_navigation(
            &renderer, RUNNER_UI_PLATFORM_LINUX, RUNNER_UI_PAGE_RUNNERS))
        return 10;
    if (begin_calls != 1U) return 11;

    return 0;
}
