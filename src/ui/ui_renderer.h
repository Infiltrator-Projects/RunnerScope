// SPDX-License-Identifier: GPL-3.0-or-later
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
    RunnerUiPageId page;
    const char *metric_values[4];
} RunnerUiWorkspaceSummaryState;

typedef enum {
    RUNNER_UI_RUNNER_IDLE = 0,
    RUNNER_UI_RUNNER_RUNNING,
    RUNNER_UI_RUNNER_OFFLINE,
    RUNNER_UI_RUNNER_UNKNOWN
} RunnerUiRunnerTone;

typedef struct {
    const char *name;
    const char *os;
    const char *state;
    const char *repo;
    const char *job;
    const char *runtime;
    const char *state_for;
    const char *jobs;
    const char *busy_pct;
    const char *labels;
    double busy_fraction;
} RunnerUiRunnerItem;

typedef struct {
    const RunnerUiRunnerItem *item;
    const char *state_label;
    RunnerUiRunnerTone tone;
    double busy_fraction;
    bool selected;
    bool show_job;
} RunnerUiRunnerCardSpec;

typedef struct {
    const char *eyebrow;
    const char *title;
    char primary[512];
    char secondary[1024];
} RunnerUiRunnerSelectionSpec;

typedef struct {
    const RunnerUiRunnerItem *items;
    size_t item_count;
    ptrdiff_t selected_index;
    bool refreshing;
    bool filter_active;
} RunnerUiRunnerCardsState;

typedef struct {
    RunnerUiPageId current_page;
    RunnerUiPageId requested_page;
    bool changed;
    bool show_search;
    bool show_runner_table;
    bool clear_selection;
    bool request_activity_refresh;
    bool request_local_refresh;
} RunnerUiPageTransition;

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

    bool (*begin_workspace_summary)(void *context,
                                    const RunnerUiPageSpec *page,
                                    const char *platform_title);
    bool (*workspace_metric)(void *context,
                             size_t index,
                             const char *caption,
                             const char *value);
    bool (*end_workspace_summary)(void *context);

    bool (*begin_runner_cards)(void *context, size_t item_count);
    bool (*runner_card)(void *context,
                        size_t index,
                        const RunnerUiRunnerCardSpec *card);
    bool (*runner_cards_empty)(void *context, const char *message);
    bool (*runner_selection)(void *context,
                             const RunnerUiRunnerSelectionSpec *selection);
    bool (*end_runner_cards)(void *context);

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
bool runner_ui_render_workspace_summary(
    const RunnerUiRenderer *renderer,
    RunnerUiPlatform platform,
    const RunnerUiWorkspaceSummaryState *state);
bool runner_ui_prepare_runner_card(const RunnerUiRunnerItem *item,
                                   bool selected,
                                   RunnerUiRunnerCardSpec *card);
bool runner_ui_prepare_runner_selection(
    const RunnerUiRunnerItem *item,
    RunnerUiRunnerSelectionSpec *selection);
bool runner_ui_render_runner_cards(const RunnerUiRenderer *renderer,
                                   const RunnerUiRunnerCardsState *state);
bool runner_ui_render_footer(const RunnerUiRenderer *renderer,
                             const RunnerUiFooterState *state);
bool runner_ui_plan_page_transition(RunnerUiPageId current_page,
                                    RunnerUiPageId requested_page,
                                    bool runners_running,
                                    bool table_view,
                                    RunnerUiPageTransition *transition);

#endif
