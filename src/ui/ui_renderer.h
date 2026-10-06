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
bool runner_ui_plan_page_transition(RunnerUiPageId current_page,
                                    RunnerUiPageId requested_page,
                                    bool runners_running,
                                    bool table_view,
                                    RunnerUiPageTransition *transition);

#endif
