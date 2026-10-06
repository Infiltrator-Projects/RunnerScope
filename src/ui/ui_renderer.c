// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_renderer.h"

#include <stdio.h>
#include <string.h>

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


static const char *runner_ui_text(const char *value)
{
    return value && value[0] ? value : "—";
}

static bool runner_ui_meaningful(const char *value)
{
    return value && value[0] && strcmp(value, "—") != 0;
}

bool runner_ui_prepare_runner_card(const RunnerUiRunnerItem *item,
                                   bool selected,
                                   RunnerUiRunnerCardSpec *card)
{
    if (!item || !card) return false;
    memset(card, 0, sizeof(*card));
    card->item = item;
    card->selected = selected;
    card->busy_fraction = item->busy_fraction;
    if (card->busy_fraction < 0.0) card->busy_fraction = 0.0;
    if (card->busy_fraction > 1.0) card->busy_fraction = 1.0;

    if (item->state && strcmp(item->state, "RUNNING") == 0) {
        card->tone = RUNNER_UI_RUNNER_RUNNING;
        card->state_label = "● Running";
    } else if (item->state && strcmp(item->state, "IDLE") == 0) {
        card->tone = RUNNER_UI_RUNNER_IDLE;
        card->state_label = "● Idle";
    } else if (item->state && strcmp(item->state, "OFFLINE") == 0) {
        card->tone = RUNNER_UI_RUNNER_OFFLINE;
        card->state_label = "● Offline";
    } else {
        card->tone = RUNNER_UI_RUNNER_UNKNOWN;
        card->state_label = "● Unknown";
    }
    card->show_job = card->tone == RUNNER_UI_RUNNER_RUNNING &&
                     runner_ui_meaningful(item->job);
    return true;
}

bool runner_ui_prepare_runner_selection(
    const RunnerUiRunnerItem *item,
    RunnerUiRunnerSelectionSpec *selection)
{
    if (!item || !selection) return false;
    memset(selection, 0, sizeof(*selection));
    selection->eyebrow = "Selected runner";
    selection->title = runner_ui_text(item->name);
    (void)snprintf(
        selection->primary, sizeof(selection->primary),
        "%s  •  %s for %s  •  %s jobs  •  %s session utilisation",
        runner_ui_text(item->os), runner_ui_text(item->state),
        runner_ui_text(item->state_for), runner_ui_text(item->jobs),
        runner_ui_text(item->busy_pct));
    (void)snprintf(
        selection->secondary, sizeof(selection->secondary),
        "Repository: %s  •  Job: %s  •  Runtime: %s\nLabels: %s",
        runner_ui_text(item->repo), runner_ui_text(item->job),
        runner_ui_text(item->runtime), runner_ui_text(item->labels));
    selection->primary[sizeof(selection->primary) - 1U] = '\0';
    selection->secondary[sizeof(selection->secondary) - 1U] = '\0';
    return true;
}

bool runner_ui_render_runner_cards(const RunnerUiRenderer *renderer,
                                   const RunnerUiRunnerCardsState *state)
{
    if (!renderer || !renderer->begin_runner_cards || !renderer->runner_card ||
        !renderer->runner_cards_empty || !renderer->runner_selection ||
        !renderer->end_runner_cards || !state)
        return false;
    if (state->item_count != 0U && !state->items) return false;
    if (state->selected_index < -1 ||
        (state->selected_index >= 0 &&
         (size_t)state->selected_index >= state->item_count))
        return false;
    if (!renderer->begin_runner_cards(renderer->context, state->item_count))
        return false;

    if (state->item_count == 0U) {
        const char *message = state->refreshing
            ? "Loading runners…"
            : state->filter_active
                ? "No runners match this filter."
                : "No runners detected.";
        if (!renderer->runner_cards_empty(renderer->context, message))
            return false;
        return renderer->end_runner_cards(renderer->context);
    }

    for (size_t i = 0U; i < state->item_count; ++i) {
        RunnerUiRunnerCardSpec card;
        if (!runner_ui_prepare_runner_card(
                &state->items[i], (ptrdiff_t)i == state->selected_index, &card))
            return false;
        if (!renderer->runner_card(renderer->context, i, &card))
            return false;
    }

    if (state->selected_index >= 0) {
        RunnerUiRunnerSelectionSpec selection;
        if (!runner_ui_prepare_runner_selection(
                &state->items[state->selected_index], &selection))
            return false;
        if (!renderer->runner_selection(renderer->context, &selection))
            return false;
    }

    return renderer->end_runner_cards(renderer->context);
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
