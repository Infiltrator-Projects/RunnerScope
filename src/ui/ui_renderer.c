// SPDX-License-Identifier: GPL-3.0-or-later
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

bool runner_ui_render_workspace_summary(
    const RunnerUiRenderer *renderer,
    RunnerUiPlatform platform,
    const RunnerUiWorkspaceSummaryState *state)
{
    if (!renderer || !renderer->begin_workspace_summary ||
        !renderer->workspace_metric || !renderer->end_workspace_summary ||
        !state || (int)state->page < 0 || state->page >= RUNNER_UI_PAGE_COUNT ||
        (platform != RUNNER_UI_PLATFORM_LINUX &&
         platform != RUNNER_UI_PLATFORM_WINDOWS))
        return false;

    for (size_t i = 0U; i < 4U; ++i)
        if (!state->metric_values[i]) return false;

    const RunnerUiPageSpec *page = runner_ui_page(state->page);
    const char *platform_title = runner_ui_page_title(page, platform);
    if (!platform_title || !platform_title[0]) return false;
    if (!renderer->begin_workspace_summary(
            renderer->context, page, platform_title))
        return false;

    for (size_t i = 0U; i < 4U; ++i)
        if (!renderer->workspace_metric(
                renderer->context, i, page->metric_labels[i],
                state->metric_values[i]))
            return false;

    return renderer->end_workspace_summary(renderer->context);
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

bool runner_ui_plan_page_transition(RunnerUiPageId current_page,
                                    RunnerUiPageId requested_page,
                                    bool runners_running,
                                    bool table_view,
                                    RunnerUiPageTransition *transition)
{
    if (!transition || (int)current_page < 0 ||
        current_page >= RUNNER_UI_PAGE_COUNT ||
        (int)requested_page < 0 || requested_page >= RUNNER_UI_PAGE_COUNT)
        return false;

    transition->current_page = current_page;
    transition->requested_page = requested_page;
    transition->changed = current_page != requested_page;
    transition->show_search = requested_page == RUNNER_UI_PAGE_RUNNERS;
    transition->show_runner_table = transition->show_search && table_view;
    transition->clear_selection = transition->changed;
    transition->request_activity_refresh =
        transition->changed &&
        (requested_page == RUNNER_UI_PAGE_ACTIVE_JOBS ||
         (requested_page == RUNNER_UI_PAGE_RUNNERS && runners_running));
    transition->request_local_refresh =
        transition->changed && requested_page == RUNNER_UI_PAGE_LOCAL_HEALTH;
    return true;
}
