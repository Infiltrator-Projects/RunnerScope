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
typedef struct {
    void *context;
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
} RunnerUiRenderer;

bool runner_ui_render_navigation(const RunnerUiRenderer *renderer,
                                 RunnerUiPlatform platform,
                                 RunnerUiPageId selected_page);

#endif
