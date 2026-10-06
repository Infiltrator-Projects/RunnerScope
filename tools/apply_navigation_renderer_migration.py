#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


renderer_h = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
'''

renderer_c = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
'''
write("src/ui/ui_renderer.h", renderer_h)
write("src/ui/ui_renderer.c", renderer_c)

cmake = read("CMakeLists.txt")
cmake = replace_once(
    cmake,
    "add_library(runnerscope_ui_contract STATIC\n    src/ui/ui_contract.c\n)\n",
    "add_library(runnerscope_ui_contract STATIC\n    src/ui/ui_contract.c\n    src/ui/ui_renderer.c\n)\n",
    "compile shared UI renderer",
)
write("CMakeLists.txt", cmake)

linux = read("src/linux_main.c")
linux = replace_once(
    linux,
    '#include "ui/ui_contract.h"\n',
    '#include "ui/ui_contract.h"\n#include "ui/ui_renderer.h"\n',
    "Linux renderer include",
)

linux_adapter = r'''
typedef struct {
    RunnerScopeApp *app;
    GtkWidget *nav_rail;
} GtkNavigationRendererContext;

static bool gtk_renderer_begin_navigation(void *context,
                                          size_t item_count,
                                          RunnerUiPageId selected_page)
{
    (void)item_count;
    (void)selected_page;
    GtkNavigationRendererContext *renderer = context;
    return renderer && renderer->app && renderer->nav_rail;
}

static bool gtk_renderer_navigation_separator(
    void *context, const RunnerUiPageSpec *before_page)
{
    GtkNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->nav_rail || !before_page) return false;
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(separator),
        "runner-main-nav-separator");
    gtk_box_pack_start(
        GTK_BOX(renderer->nav_rail), separator, FALSE, FALSE, 5);
    return true;
}

static bool gtk_renderer_navigation_item(void *context,
                                         const RunnerUiPageSpec *page,
                                         const char *platform_label,
                                         bool selected)
{
    GtkNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->app || !renderer->nav_rail ||
        !page || !platform_label)
        return false;
    GtkWidget *button = make_nav_button(
        renderer->app,
        (gint)page->id,
        page->nav_asset,
        platform_label,
        page->tooltip);
    gtk_box_pack_start(
        GTK_BOX(renderer->nav_rail), button, FALSE, TRUE, 0);
    if (selected)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    return true;
}

static bool gtk_renderer_end_navigation(void *context)
{
    return context != NULL;
}

'''
linux = replace_once(
    linux,
    "static void build_ui(RunnerScopeApp *app)\n{\n",
    linux_adapter + "static void build_ui(RunnerScopeApp *app)\n{\n",
    "insert GTK navigation renderer",
)

old_linux_nav = r'''    for (gint i = 0; i < RUNNER_UI_PAGE_COUNT; i++) {
        const RunnerUiPageSpec *ui_page =
            runner_ui_page((RunnerUiPageId)i);
        if (ui_page->separator_before) {
            GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_style_context_add_class(
                gtk_widget_get_style_context(separator),
                "runner-main-nav-separator");
            gtk_box_pack_start(GTK_BOX(nav_rail), separator, FALSE, FALSE, 5);
        }
        GtkWidget *nav_button = make_nav_button(
            app, i, ui_page->nav_asset,
            runner_ui_page_nav_label(ui_page, RUNNER_UI_PLATFORM_LINUX),
            ui_page->tooltip);
        gtk_box_pack_start(
            GTK_BOX(nav_rail), nav_button, FALSE, TRUE, 0);
    }
    sync_navigation(app, RUNNER_UI_PAGE_RUNNERS);
'''
new_linux_nav = r'''    GtkNavigationRendererContext navigation_context = {app, nav_rail};
    RunnerUiRenderer navigation_renderer = {0};
    navigation_renderer.context = &navigation_context;
    navigation_renderer.begin_navigation = gtk_renderer_begin_navigation;
    navigation_renderer.navigation_separator =
        gtk_renderer_navigation_separator;
    navigation_renderer.navigation_item = gtk_renderer_navigation_item;
    navigation_renderer.end_navigation = gtk_renderer_end_navigation;
    if (!runner_ui_render_navigation(
            &navigation_renderer,
            RUNNER_UI_PLATFORM_LINUX,
            RUNNER_UI_PAGE_RUNNERS))
        g_error("Unable to render the shared navigation contract.");
    sync_navigation(app, RUNNER_UI_PAGE_RUNNERS);
'''
linux = replace_once(
    linux, old_linux_nav, new_linux_nav, "route GTK navigation through renderer")
write("src/linux_main.c", linux)

windows = read("src/windows_main.c")
windows = replace_once(
    windows,
    '#include "ui/ui_contract.h"\n',
    '#include "ui/ui_contract.h"\n#include "ui/ui_renderer.h"\n',
    "Windows renderer include",
)

win_adapter = r'''
typedef struct {
    HDC dc;
} WinNavigationRendererContext;

static bool win_renderer_begin_navigation(void *context,
                                          size_t item_count,
                                          RunnerUiPageId selected_page)
{
    (void)item_count;
    (void)selected_page;
    WinNavigationRendererContext *renderer = context;
    return renderer && renderer->dc;
}

static bool win_renderer_navigation_separator(
    void *context, const RunnerUiPageSpec *before_page)
{
    WinNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !before_page) return false;
    int page = (int)before_page->id;
    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT) return false;
    RECT separator = {
        g_nav_rects[page].left + sx(2),
        g_nav_rects[page].top - sx(7),
        g_nav_rects[page].right - sx(2),
        g_nav_rects[page].top - sx(6)
    };
    fill_rect_color(renderer->dc, separator, rgb(g_palette.border_rgb));
    return true;
}

static bool win_renderer_navigation_item(void *context,
                                         const RunnerUiPageSpec *page,
                                         const char *platform_label,
                                         bool selected)
{
    WinNavigationRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !page || !platform_label) return false;
    int index = (int)page->id;
    if (index < 0 || index >= RUNNER_UI_PAGE_COUNT) return false;

    wchar_t nav_label[128];
    utf8_to_wide(platform_label, nav_label,
                 sizeof(nav_label) / sizeof(nav_label[0]));
    RECT item = g_nav_rects[index];
    if (selected) {
        fill_round_rect(renderer->dc, item,
                        rgb(g_palette.selection_background_rgb),
                        rgb(g_palette.neutral_accent_rgb), sx(12));
    }
    RECT image_rect = {item.left + sx(5), item.top + sx(5),
                       item.left + sx(45), item.bottom - sx(5)};
    draw_image(renderer->dc, &g_nav_images[index], &image_rect);
    RECT text_rect = {item.left + sx(54), item.top,
                      item.right - sx(6), item.bottom};
    draw_text(renderer->dc, nav_label, text_rect, g_font_bold,
              selected ? rgb(g_palette.selection_foreground_rgb)
                       : rgb(g_palette.text_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return true;
}

static bool win_renderer_end_navigation(void *context)
{
    return context != NULL;
}

'''
windows = replace_once(
    windows,
    "static void draw_navigation(HDC dc, RECT content)\n{\n",
    win_adapter + "static void draw_navigation(HDC dc, RECT content)\n{\n",
    "insert Win32 navigation renderer",
)

old_win_nav = r'''    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        const RunnerUiPageSpec *ui_page =
            runner_ui_page((RunnerUiPageId)i);
        wchar_t nav_label[128];
        utf8_to_wide(
            runner_ui_page_nav_label(ui_page, RUNNER_UI_PLATFORM_WINDOWS),
            nav_label, sizeof(nav_label) / sizeof(nav_label[0]));
        RECT item = g_nav_rects[i];
        if (i == g_page) {
            fill_round_rect(dc, item, rgb(g_palette.selection_background_rgb),
                            rgb(g_palette.neutral_accent_rgb), sx(12));
        }
        RECT image_rect = {item.left + sx(5), item.top + sx(5),
                           item.left + sx(45), item.bottom - sx(5)};
        draw_image(dc, &g_nav_images[i], &image_rect);
        RECT text_rect = {item.left + sx(54), item.top,
                          item.right - sx(6), item.bottom};
        draw_text(dc, nav_label, text_rect, g_font_bold,
                  i == g_page ? rgb(g_palette.selection_foreground_rgb)
                              : rgb(g_palette.text_rgb),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    RECT separator = {nav.left + sx(10), g_nav_rects[3].top - sx(7),
                      nav.right - sx(10), g_nav_rects[3].top - sx(6)};
    fill_rect_color(dc, separator, rgb(g_palette.border_rgb));
'''
new_win_nav = r'''    WinNavigationRendererContext navigation_context = {dc};
    RunnerUiRenderer navigation_renderer = {0};
    navigation_renderer.context = &navigation_context;
    navigation_renderer.begin_navigation = win_renderer_begin_navigation;
    navigation_renderer.navigation_separator =
        win_renderer_navigation_separator;
    navigation_renderer.navigation_item = win_renderer_navigation_item;
    navigation_renderer.end_navigation = win_renderer_end_navigation;
    if (!runner_ui_render_navigation(
            &navigation_renderer,
            RUNNER_UI_PLATFORM_WINDOWS,
            (RunnerUiPageId)g_page))
        return;
'''
windows = replace_once(
    windows, old_win_nav, new_win_nav, "route Win32 navigation through renderer")
write("src/windows_main.c", windows)

test = read("tests/ui_contract_test.c")
test = replace_once(
    test,
    '#include "ui/ui_contract.h"\n',
    '#include "ui/ui_contract.h"\n#include "ui/ui_renderer.h"\n',
    "test renderer include",
)

recorder = r'''
typedef struct {
    size_t begin_count;
    size_t item_count;
    size_t separator_count;
    size_t end_count;
    RunnerUiPageId selected_page;
    RunnerUiPageId item_ids[RUNNER_UI_PAGE_COUNT];
    RunnerUiPageId separator_before;
    bool selected[RUNNER_UI_PAGE_COUNT];
    char labels[RUNNER_UI_PAGE_COUNT][64];
} NavigationRecorder;

static bool record_begin_navigation(void *context,
                                    size_t item_count,
                                    RunnerUiPageId selected_page)
{
    NavigationRecorder *recorder = context;
    if (!recorder || item_count != RUNNER_UI_PAGE_COUNT) return false;
    recorder->begin_count++;
    recorder->selected_page = selected_page;
    return true;
}

static bool record_navigation_separator(
    void *context, const RunnerUiPageSpec *before_page)
{
    NavigationRecorder *recorder = context;
    if (!recorder || !before_page) return false;
    recorder->separator_count++;
    recorder->separator_before = before_page->id;
    return true;
}

static bool record_navigation_item(void *context,
                                   const RunnerUiPageSpec *page,
                                   const char *platform_label,
                                   bool selected)
{
    NavigationRecorder *recorder = context;
    if (!recorder || !page || !platform_label ||
        recorder->item_count >= RUNNER_UI_PAGE_COUNT)
        return false;
    size_t index = recorder->item_count++;
    recorder->item_ids[index] = page->id;
    recorder->selected[index] = selected;
    (void)snprintf(recorder->labels[index], sizeof(recorder->labels[index]),
                   "%s", platform_label);
    return true;
}

static bool record_end_navigation(void *context)
{
    NavigationRecorder *recorder = context;
    if (!recorder) return false;
    recorder->end_count++;
    return true;
}

static int verify_navigation_renderer(RunnerUiPlatform platform,
                                      RunnerUiPageId selected_page,
                                      const char *local_label)
{
    NavigationRecorder recorder = {0};
    RunnerUiRenderer renderer = {0};
    renderer.context = &recorder;
    renderer.begin_navigation = record_begin_navigation;
    renderer.navigation_separator = record_navigation_separator;
    renderer.navigation_item = record_navigation_item;
    renderer.end_navigation = record_end_navigation;

    if (!runner_ui_render_navigation(&renderer, platform, selected_page)) return 1;
    if (recorder.begin_count != 1U || recorder.end_count != 1U) return 2;
    if (recorder.item_count != RUNNER_UI_PAGE_COUNT) return 3;
    if (recorder.separator_count != 1U ||
        recorder.separator_before != RUNNER_UI_PAGE_LOCAL_HEALTH)
        return 4;
    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        if (recorder.item_ids[i] != (RunnerUiPageId)i) return 5;
        if (recorder.selected[i] != ((RunnerUiPageId)i == selected_page)) return 6;
    }
    if (strcmp(recorder.labels[RUNNER_UI_PAGE_LOCAL_HEALTH], local_label) != 0)
        return 7;
    return 0;
}

'''
test = replace_once(
    test,
    "int main(void)\n{\n",
    recorder + "int main(void)\n{\n",
    "insert navigation renderer recorder",
)

test = replace_once(
    test,
    '    puts("Runner Monitor shared UI contract passed.");\n',
    '    if (verify_navigation_renderer(\n'
    '            RUNNER_UI_PLATFORM_LINUX, RUNNER_UI_PAGE_HISTORY,\n'
    '            "Local Linux health") != 0)\n'
    '        return 13;\n'
    '    if (verify_navigation_renderer(\n'
    '            RUNNER_UI_PLATFORM_WINDOWS, RUNNER_UI_PAGE_ACTIVE_JOBS,\n'
    '            "Local Windows health") != 0)\n'
    '        return 14;\n'
    '    RunnerUiRenderer incomplete_renderer = {0};\n'
    '    if (runner_ui_render_navigation(\n'
    '            &incomplete_renderer, RUNNER_UI_PLATFORM_LINUX,\n'
    '            RUNNER_UI_PAGE_RUNNERS))\n'
    '        return 15;\n\n'
    '    puts("Runner Monitor shared UI contract and navigation renderer passed.");\n',
    "test shared navigation renderer",
)
write("tests/ui_contract_test.c", test)

# One-shot migration tooling must not become part of the product architecture.
for transient in (
    ROOT / "tools/apply_navigation_renderer_migration.py",
    ROOT / ".github/workflows/apply-navigation-renderer-migration.yml",
):
    if transient.exists():
        transient.unlink()
