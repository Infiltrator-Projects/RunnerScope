#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def regex_once(text, pattern, replacement, label, flags=0):
    updated, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return updated


# CMake: compile one toolkit-neutral contract and link the same object code into
# both native products. Also run a toolkit-free contract test everywhere.
cmake = read("CMakeLists.txt")
cmake = replace_once(
    cmake,
    "add_subdirectory(infiltratr-common EXCLUDE_FROM_ALL)\n\n",
    "add_subdirectory(infiltratr-common EXCLUDE_FROM_ALL)\n\n"
    "add_library(runnerscope_ui_contract STATIC\n"
    "    src/ui/ui_contract.c\n"
    ")\n"
    "target_include_directories(runnerscope_ui_contract PUBLIC\n"
    "    ${CMAKE_CURRENT_SOURCE_DIR}/src\n"
    ")\n"
    "if(MSVC)\n"
    "    target_compile_options(runnerscope_ui_contract PRIVATE /W4 /utf-8)\n"
    "elseif(CMAKE_C_COMPILER_ID MATCHES \"GNU|Clang\")\n"
    "    target_compile_options(runnerscope_ui_contract PRIVATE\n"
    "        -Wall -Wextra -Wpedantic -Wshadow -Wformat=2\n"
    "    )\n"
    "endif()\n\n",
    "add shared UI contract library",
)
cmake = replace_once(
    cmake,
    "    target_link_libraries(runnerscope PRIVATE\n        InfiltratrCommon::Common\n",
    "    target_link_libraries(runnerscope PRIVATE\n        runnerscope_ui_contract\n        InfiltratrCommon::Common\n",
    "link Windows UI contract",
)
cmake = replace_once(
    cmake,
    "    target_link_libraries(runnerscope PRIVATE\n        InfiltratrCommon::Common\n        PkgConfig::GTK3\n",
    "    target_link_libraries(runnerscope PRIVATE\n        runnerscope_ui_contract\n        InfiltratrCommon::Common\n        PkgConfig::GTK3\n",
    "link Linux UI contract",
)
cmake = replace_once(
    cmake,
    "add_test(NAME runnerscope-native-self-test COMMAND runnerscope --self-test)\n",
    "add_test(NAME runnerscope-native-self-test COMMAND runnerscope --self-test)\n\n"
    "add_executable(runnerscope-ui-contract-test\n"
    "    tests/ui_contract_test.c\n"
    ")\n"
    "target_link_libraries(runnerscope-ui-contract-test PRIVATE runnerscope_ui_contract)\n"
    "add_test(NAME runnerscope-ui-contract-test COMMAND runnerscope-ui-contract-test)\n",
    "register shared UI contract test",
)
write("CMakeLists.txt", cmake)


# Linux: make page identity/navigation/action visibility consume the shared
# contract while keeping GTK mechanics native.
linux = read("src/linux_main.c")
linux = replace_once(
    linux,
    "#include <infiltratr/posix.h>\n\n",
    "#include <infiltratr/posix.h>\n\n#include \"ui/ui_contract.h\"\n\n",
    "Linux include UI contract",
)
linux = replace_once(
    linux,
    "    if (!app || !app->workspace_title || !app->workspace_subtitle) return;\n\n"
    "    char a[32], b[32], d[32], uptime[64];\n",
    "    if (!app || !app->workspace_title || !app->workspace_subtitle) return;\n\n"
    "    const RunnerUiPageSpec *ui_page = runner_ui_page((RunnerUiPageId)page);\n"
    "    runner_asset_image_set(app->workspace_icon, ui_page->nav_asset, 48, 48);\n"
    "    runner_asset_image_set(app->workspace_art, ui_page->hero_asset, 240, 86);\n"
    "    label_set_if_changed(\n"
    "        app->workspace_title,\n"
    "        runner_ui_page_title(ui_page, RUNNER_UI_PLATFORM_LINUX));\n"
    "    label_set_if_changed(app->workspace_subtitle, ui_page->subtitle);\n\n"
    "    char a[32], b[32], d[32], uptime[64];\n",
    "Linux shared workspace context",
)
for old in [
    '            runner_asset_image_set(app->workspace_icon, "nav-runners.png", 48, 48);\n            runner_asset_image_set(app->workspace_art, "hero-runners.png", 240, 86);\n            label_set_if_changed(app->workspace_title, "Runner fleet");\n            label_set_if_changed(app->workspace_subtitle, "SESSION UTILISATION");\n',
    '            runner_asset_image_set(app->workspace_icon, "nav-active.png", 48, 48);\n            runner_asset_image_set(app->workspace_art, "hero-active.png", 240, 86);\n            label_set_if_changed(app->workspace_title, "Active work");\n            label_set_if_changed(app->workspace_subtitle, "LIVE JOBS  •  QUEUES  •  RUNNERS");\n',
    '            runner_asset_image_set(app->workspace_icon, "nav-history.png", 48, 48);\n            runner_asset_image_set(app->workspace_art, "hero-history.png", 240, 86);\n            label_set_if_changed(app->workspace_title, "Session history");\n            label_set_if_changed(app->workspace_subtitle, "STATE  •  EVENTS  •  SESSION");\n',
    '            runner_asset_image_set(app->workspace_icon, "nav-health.png", 48, 48);\n            runner_asset_image_set(app->workspace_art, "hero-health.png", 240, 86);\n            label_set_if_changed(app->workspace_title, "Local Linux health");\n            label_set_if_changed(app->workspace_subtitle, "SERVICES  •  DIAGNOSTICS  •  LINK");\n',
]:
    linux = replace_once(linux, old, "", "remove duplicated Linux page identity")

# All metric captions in update_workspace_context become contract-owned.
context_start = linux.index("static void update_workspace_context")
context_end = linux.index("static void render_history", context_start)
context = linux[context_start:context_end]
context, metric_count = re.subn(
    r'workspace_metric\(app, ([0-3])U, "[A-Z]+",',
    r'workspace_metric(app, \1U, ui_page->metric_labels[\1],',
    context,
)
if metric_count != 16:
    raise RuntimeError(f"Linux metric contract: expected 16 replacements, found {metric_count}")
context = context.replace(
    "gtk_widget_set_visible(app->open_job_button, page == 1);",
    "gtk_widget_set_visible(\n            app->open_job_button,\n            (ui_page->actions & RUNNER_UI_ACTION_OPEN_JOB) != 0U);",
)
context = context.replace(
    "gtk_widget_set_visible(app->open_diag_button, page == 3);",
    "gtk_widget_set_visible(\n            app->open_diag_button,\n            (ui_page->actions & RUNNER_UI_ACTION_OPEN_DIAGNOSTIC) != 0U);",
)
context = context.replace(
    "gtk_widget_set_visible(app->restart_button, page == 3);",
    "gtk_widget_set_visible(\n            app->restart_button,\n            (ui_page->actions & RUNNER_UI_ACTION_RESTART_RUNNER) != 0U);",
)
context = context.replace(
    "gtk_widget_set_visible(app->runner_view_switch, page == 0);",
    "gtk_widget_set_visible(app->runner_view_switch, ui_page->show_view_switch);",
)
linux = linux[:context_start] + context + linux[context_end:]

linux = replace_once(
    linux,
    "    const guint screen_padding = metrics ? metrics->screen_padding : 20U;\n\n"
    "    app->window = gtk_application_window_new(app->application);\n",
    "    const guint screen_padding = metrics ? metrics->screen_padding : 20U;\n"
    "    const RunnerUiPageSpec *initial_page =\n"
    "        runner_ui_page(RUNNER_UI_PAGE_RUNNERS);\n\n"
    "    app->window = gtk_application_window_new(app->application);\n",
    "Linux initial page contract",
)
linux = linux.replace(
    'gtk_window_set_title(GTK_WINDOW(app->window), "Runner Monitor");',
    'gtk_window_set_title(GTK_WINDOW(app->window), runner_ui_product_title());',
    1,
)
linux = linux.replace(
    'GtkWidget *brand_title = gtk_label_new("Runner Monitor");\n    GtkWidget *brand_subtitle = gtk_label_new("Infiltrator OS");',
    'GtkWidget *brand_title = gtk_label_new(runner_ui_product_title());\n    GtkWidget *brand_subtitle = gtk_label_new(runner_ui_product_family());',
    1,
)
linux = linux.replace(
    'app->workspace_icon = runner_asset_image_new("nav-runners.png", 48, 48);',
    'app->workspace_icon = runner_asset_image_new(initial_page->nav_asset, 48, 48);',
    1,
)
linux = linux.replace(
    'app->workspace_title = gtk_label_new("Runner fleet");',
    'app->workspace_title = gtk_label_new(\n        runner_ui_page_title(initial_page, RUNNER_UI_PLATFORM_LINUX));',
    1,
)
linux = linux.replace(
    'app->workspace_subtitle = gtk_label_new(\n        "CAPACITY  •  WORKLOAD  •  UTILISATION");',
    'app->workspace_subtitle = gtk_label_new(initial_page->subtitle);',
    1,
)
linux = linux.replace(
    'app->workspace_art = runner_asset_image_new("hero-runners.png", 240, 86);',
    'app->workspace_art = runner_asset_image_new(initial_page->hero_asset, 240, 86);',
    1,
)

nav_pattern = re.compile(
    r"    const struct \{\n"
    r"        const char \*icon;\n"
    r"        const char \*title;\n"
    r"        const char \*tooltip;\n"
    r"    \} nav_items\[\] = \{.*?"
    r"    sync_navigation\(app, 0\);\n",
    re.S,
)
nav_replacement = '''    for (gint i = 0; i < RUNNER_UI_PAGE_COUNT; i++) {
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
linux, nav_count = nav_pattern.subn(nav_replacement, linux, count=1)
if nav_count != 1:
    raise RuntimeError(f"Linux navigation contract: expected one block, found {nav_count}")
linux = linux.replace(
    '    info.program_name = "Runner Monitor";',
    '    info.program_name = runner_ui_product_title();',
    1,
)
linux = replace_once(
    linux,
    "static int self_test(void)\n{\n    if (!INFILTRATR_COMMON_VERSION[0]) return 1;\n",
    "static int self_test(void)\n{\n"
    "    char ui_error[256];\n"
    "    if (!runner_ui_contract_validate(ui_error, sizeof(ui_error))) {\n"
    "        fprintf(stderr, \"Shared UI contract failed: %s\\n\", ui_error);\n"
    "        return 1;\n"
    "    }\n"
    "    if (!INFILTRATR_COMMON_VERSION[0]) return 2;\n",
    "Linux self-test validates contract",
)
# Keep subsequent Linux self-test failure codes distinct after adding the new gate.
linux = linux.replace("        return 2;\n    char duration[64];", "        return 3;\n    char duration[64];", 1)
linux = linux.replace("        return 3;\n    if (strstr(duration, \"1h\") == NULL) return 4;", "        return 4;\n    if (strstr(duration, \"1h\") == NULL) return 5;", 1)
write("src/linux_main.c", linux)


# Windows: consume the same page contract while retaining native Win32
# layout/drawing/resource translation.
windows = read("src/windows_main.c")
windows = replace_once(
    windows,
    '#include "windows_resources.h"\n\n',
    '#include "windows_resources.h"\n#include "ui/ui_contract.h"\n\n',
    "Windows include UI contract",
)
windows = regex_once(
    windows,
    r"static const wchar_t \*page_title\(int page\)\n\{.*?\n\}\n\n"
    r"static const wchar_t \*page_subtitle\(int page\)\n\{.*?\n\}\n",
    '''static const wchar_t *page_title(int page)
{
    static wchar_t value[128];
    const RunnerUiPageSpec *ui_page =
        runner_ui_page((RunnerUiPageId)page);
    utf8_to_wide(
        runner_ui_page_title(ui_page, RUNNER_UI_PLATFORM_WINDOWS),
        value, sizeof(value) / sizeof(value[0]));
    return value;
}

static const wchar_t *page_subtitle(int page)
{
    static wchar_t value[160];
    const RunnerUiPageSpec *ui_page =
        runner_ui_page((RunnerUiPageId)page);
    utf8_to_wide(ui_page->subtitle, value, sizeof(value) / sizeof(value[0]));
    return value;
}
''',
    "Windows page identity contract",
    flags=re.S,
)
windows = replace_once(
    windows,
    "    RECT title = {sx(70), sx(8), sx(370), sx(34)};\n"
    "    draw_text(dc, L\"Runner Monitor\", title, g_font_brand,\n"
    "              rgb(g_palette.title_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);\n"
    "    RECT subtitle = {sx(70), sx(32), sx(370), sx(52)};\n"
    "    draw_text(dc, L\"Infiltrator OS\", subtitle, g_font_small,\n"
    "              rgb(g_palette.muted_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);\n",
    "    wchar_t product_title[96];\n"
    "    wchar_t product_family[96];\n"
    "    utf8_to_wide(runner_ui_product_title(), product_title, 96U);\n"
    "    utf8_to_wide(runner_ui_product_family(), product_family, 96U);\n"
    "    RECT title = {sx(70), sx(8), sx(370), sx(34)};\n"
    "    draw_text(dc, product_title, title, g_font_brand,\n"
    "              rgb(g_palette.title_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);\n"
    "    RECT subtitle = {sx(70), sx(32), sx(370), sx(52)};\n"
    "    draw_text(dc, product_family, subtitle, g_font_small,\n"
    "              rgb(g_palette.muted_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);\n",
    "Windows shared product identity",
)
windows = replace_once(
    windows,
    "    const wchar_t *labels[4] = {\n"
    "        L\"Runners\", L\"Active jobs\", L\"History\", L\"Local Windows health\"\n"
    "    };\n"
    "    for (int i = 0; i < 4; ++i) {\n",
    "    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {\n"
    "        const RunnerUiPageSpec *ui_page =\n"
    "            runner_ui_page((RunnerUiPageId)i);\n"
    "        wchar_t nav_label[128];\n"
    "        utf8_to_wide(\n"
    "            runner_ui_page_nav_label(ui_page, RUNNER_UI_PLATFORM_WINDOWS),\n"
    "            nav_label, sizeof(nav_label) / sizeof(nav_label[0]));\n",
    "Windows navigation labels contract",
)
windows = windows.replace(
    "        draw_text(dc, labels[i], text_rect, g_font_bold,",
    "        draw_text(dc, nav_label, text_rect, g_font_bold,",
    1,
)
windows = windows.replace(
    "    if (page < 0 || page > 3 || page == g_page) return;",
    "    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT || page == g_page) return;",
    1,
)
windows = windows.replace(
    "    for (int i = 0; i < 4; ++i) {\n        if (pt_in_rect(&g_nav_rects[i], point)) { set_page(i); return; }\n    }",
    "    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {\n        if (pt_in_rect(&g_nav_rects[i], point)) { set_page(i); return; }\n    }",
    1,
)
windows = replace_once(
    windows,
    "static int self_test(void)\n{\n    const InfiltratrThemePalette *day =\n",
    "static int self_test(void)\n{\n"
    "    char ui_error[256];\n"
    "    if (!runner_ui_contract_validate(ui_error, sizeof(ui_error))) return 1;\n"
    "    const InfiltratrThemePalette *day =\n",
    "Windows self-test validates contract",
)
write("src/windows_main.c", windows)


# The migration helper and its workflow are one-shot scaffolding. Remove both
# from the resulting branch so the product tree contains only the migration.
for temporary in [
    ROOT / "tools" / "apply_ui_contract_migration.py",
    ROOT / ".github" / "workflows" / "apply-ui-contract-migration.yml",
]:
    if temporary.exists():
        temporary.unlink()

print("Applied first shared UI-contract tranche to Linux, Windows and CMake.")
