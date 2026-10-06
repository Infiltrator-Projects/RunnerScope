// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_UI_CONTRACT_H
#define RUNNERSCOPE_UI_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    RUNNER_UI_PLATFORM_LINUX = 0,
    RUNNER_UI_PLATFORM_WINDOWS = 1
} RunnerUiPlatform;

typedef enum {
    RUNNER_UI_PAGE_RUNNERS = 0,
    RUNNER_UI_PAGE_ACTIVE_JOBS = 1,
    RUNNER_UI_PAGE_HISTORY = 2,
    RUNNER_UI_PAGE_LOCAL_HEALTH = 3,
    RUNNER_UI_PAGE_COUNT = 4
} RunnerUiPageId;

typedef enum {
    RUNNER_UI_ACTION_NONE = 0U,
    RUNNER_UI_ACTION_OPEN_JOB = 1U << 0,
    RUNNER_UI_ACTION_OPEN_DIAGNOSTIC = 1U << 1,
    RUNNER_UI_ACTION_RESTART_RUNNER = 1U << 2
} RunnerUiActionFlags;

typedef struct {
    RunnerUiPageId id;
    const char *key;
    const char *nav_label;
    const char *nav_asset;
    const char *hero_asset;
    const char *title;
    const char *subtitle;
    const char *tooltip;
    const char *metric_labels[4];
    unsigned int actions;
    bool separator_before;
    bool show_view_switch;
} RunnerUiPageSpec;

typedef enum {
    RUNNER_UI_COMPONENT_SHELL = 0,
    RUNNER_UI_COMPONENT_HEADER,
    RUNNER_UI_COMPONENT_BRAND,
    RUNNER_UI_COMPONENT_ACTION,
    RUNNER_UI_COMPONENT_NAVIGATION,
    RUNNER_UI_COMPONENT_NAV_ITEM,
    RUNNER_UI_COMPONENT_WORKSPACE,
    RUNNER_UI_COMPONENT_HERO,
    RUNNER_UI_COMPONENT_METRICS,
    RUNNER_UI_COMPONENT_TOOLBAR,
    RUNNER_UI_COMPONENT_SEARCH,
    RUNNER_UI_COMPONENT_PAGE_HOST,
    RUNNER_UI_COMPONENT_PAGE,
    RUNNER_UI_COMPONENT_GRID,
    RUNNER_UI_COMPONENT_CARD,
    RUNNER_UI_COMPONENT_SELECTION,
    RUNNER_UI_COMPONENT_FOOTER,
    RUNNER_UI_COMPONENT_STATUS
} RunnerUiComponentKind;

typedef struct {
    const char *id;
    const char *parent_id;
    RunnerUiComponentKind kind;
} RunnerUiComponentSpec;

const char *runner_ui_product_title(void);
const char *runner_ui_product_family(void);
const RunnerUiPageSpec *runner_ui_page(RunnerUiPageId id);
const char *runner_ui_page_nav_label(const RunnerUiPageSpec *page,
                                     RunnerUiPlatform platform);
const char *runner_ui_page_title(const RunnerUiPageSpec *page,
                                 RunnerUiPlatform platform);
const RunnerUiComponentSpec *runner_ui_components(size_t *count);
bool runner_ui_contract_validate(char *error, size_t error_size);

#endif
