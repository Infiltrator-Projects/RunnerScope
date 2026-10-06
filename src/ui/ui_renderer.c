// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_renderer.h"

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
