// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_contract.h"

#include <stdio.h>
#include <string.h>

static const RunnerUiPageSpec k_pages[RUNNER_UI_PAGE_COUNT] = {
    {
        RUNNER_UI_PAGE_RUNNERS,
        "runners",
        "Runners",
        "nav-runners.png",
        "hero-runners.png",
        "Runner fleet",
        "SESSION UTILISATION",
        "Fleet state and utilisation",
        {"TOTAL", "RUNNING", "IDLE", "OFFLINE"},
        RUNNER_UI_ACTION_NONE,
        false,
        true
    },
    {
        RUNNER_UI_PAGE_ACTIVE_JOBS,
        "active-jobs",
        "Active jobs",
        "nav-active.png",
        "hero-active.png",
        "Active work",
        "LIVE JOBS  •  QUEUES  •  RUNNERS",
        "Work executing now",
        {"LOCAL", "GITHUB", "QUEUED", "ACTIVE"},
        RUNNER_UI_ACTION_OPEN_JOB,
        false,
        false
    },
    {
        RUNNER_UI_PAGE_HISTORY,
        "history",
        "History",
        "nav-history.png",
        "hero-history.png",
        "Session history",
        "STATE  •  EVENTS  •  SESSION",
        "Session activity",
        {"EVENTS", "RUNNERS", "UPTIME", "FILTER"},
        RUNNER_UI_ACTION_NONE,
        false,
        false
    },
    {
        RUNNER_UI_PAGE_LOCAL_HEALTH,
        "local-health",
        "Local health",
        "nav-health.png",
        "hero-health.png",
        "Local health",
        "SERVICES  •  DIAGNOSTICS  •  LINK",
        "Services and diagnostics",
        {"SERVICES", "RUNNING", "GITHUB", "DIAGNOSTICS"},
        RUNNER_UI_ACTION_OPEN_DIAGNOSTIC | RUNNER_UI_ACTION_RESTART_RUNNER,
        true,
        false
    }
};

static const RunnerUiComponentSpec k_components[] = {
    {"shell", NULL, RUNNER_UI_COMPONENT_SHELL},
    {"shell.header", "shell", RUNNER_UI_COMPONENT_HEADER},
    {"shell.header.brand", "shell.header", RUNNER_UI_COMPONENT_BRAND},
    {"shell.header.settings", "shell.header", RUNNER_UI_COMPONENT_ACTION},
    {"shell.navigation", "shell", RUNNER_UI_COMPONENT_NAVIGATION},
    {"shell.navigation.runners", "shell.navigation", RUNNER_UI_COMPONENT_NAV_ITEM},
    {"shell.navigation.active-jobs", "shell.navigation", RUNNER_UI_COMPONENT_NAV_ITEM},
    {"shell.navigation.history", "shell.navigation", RUNNER_UI_COMPONENT_NAV_ITEM},
    {"shell.navigation.local-health", "shell.navigation", RUNNER_UI_COMPONENT_NAV_ITEM},
    {"shell.workspace", "shell", RUNNER_UI_COMPONENT_WORKSPACE},
    {"shell.workspace.hero", "shell.workspace", RUNNER_UI_COMPONENT_HERO},
    {"shell.workspace.metrics", "shell.workspace", RUNNER_UI_COMPONENT_METRICS},
    {"shell.workspace.toolbar", "shell.workspace", RUNNER_UI_COMPONENT_TOOLBAR},
    {"shell.workspace.toolbar.view-switch", "shell.workspace.toolbar", RUNNER_UI_COMPONENT_ACTION},
    {"shell.workspace.toolbar.search", "shell.workspace.toolbar", RUNNER_UI_COMPONENT_SEARCH},
    {"shell.workspace.page-host", "shell.workspace", RUNNER_UI_COMPONENT_PAGE_HOST},
    {"page.runners", "shell.workspace.page-host", RUNNER_UI_COMPONENT_PAGE},
    {"page.runners.cards", "page.runners", RUNNER_UI_COMPONENT_GRID},
    {"page.runners.cards.runner", "page.runners.cards", RUNNER_UI_COMPONENT_CARD},
    {"page.active-jobs", "shell.workspace.page-host", RUNNER_UI_COMPONENT_PAGE},
    {"page.history", "shell.workspace.page-host", RUNNER_UI_COMPONENT_PAGE},
    {"page.local-health", "shell.workspace.page-host", RUNNER_UI_COMPONENT_PAGE},
    {"shell.workspace.selection", "shell.workspace", RUNNER_UI_COMPONENT_SELECTION},
    {"shell.footer", "shell", RUNNER_UI_COMPONENT_FOOTER},
    {"shell.footer.context-actions", "shell.footer", RUNNER_UI_COMPONENT_ACTION},
    {"shell.footer.general-actions", "shell.footer", RUNNER_UI_COMPONENT_ACTION},
    {"shell.footer.status", "shell.footer", RUNNER_UI_COMPONENT_STATUS}
};

const char *runner_ui_product_title(void)
{
    return "Runner Monitor";
}

const char *runner_ui_product_family(void)
{
    return "Infiltrator OS";
}

const RunnerUiPageSpec *runner_ui_page(RunnerUiPageId id)
{
    if ((int)id < 0 || id >= RUNNER_UI_PAGE_COUNT)
        return &k_pages[RUNNER_UI_PAGE_RUNNERS];
    return &k_pages[id];
}

const char *runner_ui_page_nav_label(const RunnerUiPageSpec *page,
                                     RunnerUiPlatform platform)
{
    if (!page) page = &k_pages[RUNNER_UI_PAGE_RUNNERS];
    if (page->id != RUNNER_UI_PAGE_LOCAL_HEALTH)
        return page->nav_label;
    return platform == RUNNER_UI_PLATFORM_WINDOWS
        ? "Local Windows health" : "Local Linux health";
}

const char *runner_ui_page_title(const RunnerUiPageSpec *page,
                                 RunnerUiPlatform platform)
{
    if (!page) page = &k_pages[RUNNER_UI_PAGE_RUNNERS];
    if (page->id != RUNNER_UI_PAGE_LOCAL_HEALTH)
        return page->title;
    return platform == RUNNER_UI_PLATFORM_WINDOWS
        ? "Local Windows health" : "Local Linux health";
}

const RunnerUiComponentSpec *runner_ui_components(size_t *count)
{
    if (count) *count = sizeof(k_components) / sizeof(k_components[0]);
    return k_components;
}

static bool set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size != 0U) {
        (void)snprintf(error, error_size, "%s", message ? message : "invalid UI contract");
        error[error_size - 1U] = '\0';
    }
    return false;
}

static bool component_exists(const char *id)
{
    const size_t count = sizeof(k_components) / sizeof(k_components[0]);
    for (size_t i = 0U; i < count; ++i)
        if (strcmp(k_components[i].id, id) == 0) return true;
    return false;
}

bool runner_ui_contract_validate(char *error, size_t error_size)
{
    if (!runner_ui_product_title()[0] || !runner_ui_product_family()[0])
        return set_error(error, error_size, "product identity is empty");

    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        const RunnerUiPageSpec *page = &k_pages[i];
        if ((int)page->id != i)
            return set_error(error, error_size, "page IDs are not stable/contiguous");
        if (!page->key || !page->key[0] || !page->nav_asset || !page->nav_asset[0] ||
            !page->hero_asset || !page->hero_asset[0] || !page->title || !page->title[0] ||
            !page->subtitle || !page->subtitle[0] || !page->tooltip || !page->tooltip[0])
            return set_error(error, error_size, "page metadata is incomplete");
        for (int j = i + 1; j < RUNNER_UI_PAGE_COUNT; ++j)
            if (strcmp(page->key, k_pages[j].key) == 0)
                return set_error(error, error_size, "duplicate page key");
        for (size_t m = 0U; m < 4U; ++m)
            if (!page->metric_labels[m] || !page->metric_labels[m][0])
                return set_error(error, error_size, "page metric label is empty");
    }

    const size_t count = sizeof(k_components) / sizeof(k_components[0]);
    for (size_t i = 0U; i < count; ++i) {
        if (!k_components[i].id || !k_components[i].id[0])
            return set_error(error, error_size, "component ID is empty");
        for (size_t j = i + 1U; j < count; ++j)
            if (strcmp(k_components[i].id, k_components[j].id) == 0)
                return set_error(error, error_size, "duplicate component ID");
        if (k_components[i].parent_id &&
            !component_exists(k_components[i].parent_id))
            return set_error(error, error_size, "component parent is missing");
    }

    if (!component_exists("page.runners") ||
        !component_exists("page.active-jobs") ||
        !component_exists("page.history") ||
        !component_exists("page.local-health"))
        return set_error(error, error_size, "required pages are missing from component tree");

    if (error && error_size != 0U) error[0] = '\0';
    return true;
}
