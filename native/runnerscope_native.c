// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/posix.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define RUNNERSCOPE_VERSION "1.1.2"
#define RUNNERSCOPE_COMMON_VERSION "1.19.2"

static InfiltratrProjectInfo project_info(void)
{
    InfiltratrProjectInfo info = INFILTRATR_PROJECT_INFO_INIT;
    info.program_name = "Runner Monitor";
    info.executable_name = "runnerscope-native";
    info.application_id = "net.ssmith.runnerscope";
    info.version = RUNNERSCOPE_VERSION;
    info.source_id = "Infiltrator-Projects/RunnerScope";
    info.build_profile = "native-common-bridge";
    info.author = "Shannon Smith";
    info.website = "https://github.com/Infiltrator-Projects/RunnerScope";
    info.license_id = "GPL-3.0-or-later";
    info.comments = "GitHub Actions self-hosted runner monitor native bridge";
    info.icon_name = "runnerscope";
    info.copyright_text = "Copyright (c) 2026 Shannon Smith";
    return info;
}

static void print_rgb(const char *name, uint32_t rgb)
{
    (void)printf("%s=#%06" PRIx32 "\n", name, rgb & UINT32_C(0x00ffffff));
}

static int print_palette(const char *name)
{
    InfiltratrThemeMode mode = INFILTRATR_THEME_NIGHT;
    bool system_is_dark = true;

    if (!name) return EINVAL;
    if (strcmp(name, "night") == 0) {
        mode = INFILTRATR_THEME_NIGHT;
    } else if (strcmp(name, "day") == 0) {
        mode = INFILTRATR_THEME_DAY;
        system_is_dark = false;
    } else if (strcmp(name, "system-dark") == 0) {
        mode = INFILTRATR_THEME_SYSTEM;
    } else if (strcmp(name, "system-light") == 0) {
        mode = INFILTRATR_THEME_SYSTEM;
        system_is_dark = false;
    } else {
        return EINVAL;
    }

    const InfiltratrThemePalette *palette = infiltratr_theme_resolve(mode, system_is_dark);
    if (!palette) return EINVAL;

    print_rgb("background", palette->background_rgb);
    print_rgb("panel", palette->panel_rgb);
    print_rgb("card", palette->card_rgb);
    print_rgb("surface", palette->surface_rgb);
    print_rgb("border", palette->border_rgb);
    print_rgb("text", palette->text_rgb);
    print_rgb("title", palette->title_rgb);
    print_rgb("muted", palette->muted_rgb);
    print_rgb("subtle", palette->subtle_rgb);
    print_rgb("button_background", palette->button_background_rgb);
    print_rgb("button_foreground", palette->button_foreground_rgb);
    print_rgb("selection_background", palette->selection_background_rgb);
    print_rgb("selection_foreground", palette->selection_foreground_rgb);
    print_rgb("neutral_accent", palette->neutral_accent_rgb);
    print_rgb("success", palette->success_rgb);
    print_rgb("warning", palette->warning_rgb);
    print_rgb("fault", palette->fault_rgb);
    print_rgb("info", palette->info_rgb);
    print_rgb("operation", palette->operation_rgb);
    return ferror(stdout) ? EIO : 0;
}

static bool copy_standard_input(FILE *stream, const void *unused)
{
    (void)unused;
    unsigned char buffer[64U * 1024U];
    for (;;) {
        const size_t amount = fread(buffer, 1U, sizeof(buffer), stdin);
        if (amount != 0U && fwrite(buffer, 1U, amount, stream) != amount) {
            if (errno == 0) errno = EIO;
            return false;
        }
        if (amount < sizeof(buffer)) {
            if (ferror(stdin)) {
                if (errno == 0) errno = EIO;
                return false;
            }
            return feof(stdin) != 0;
        }
    }
}

static int self_test(void)
{
    InfiltratrProjectInfo info = project_info();
    if (!infiltratr_project_info_is_valid(&info)) return 1;
    if (strcmp(INFILTRATR_COMMON_VERSION, RUNNERSCOPE_COMMON_VERSION) != 0) return 2;

    const InfiltratrThemePalette *day = infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false);
    const InfiltratrThemePalette *night = infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true);
    if (!day || !night || day == night) return 3;
    if (day->background_rgb == night->background_rgb) return 4;

    (void)printf("Runner Monitor %s native/Common self-test passed (Common %s)\n",
                 RUNNERSCOPE_VERSION, INFILTRATR_COMMON_VERSION);
    return ferror(stdout) ? 5 : 0;
}

static void usage(FILE *stream)
{
    (void)fprintf(stream,
        "usage: runnerscope-native --version | --common-version | --project-info | "
        "--palette MODE | --atomic-write PATH | --self-test\n");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        (void)puts(RUNNERSCOPE_VERSION);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--common-version") == 0) {
        (void)puts(INFILTRATR_COMMON_VERSION);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--project-info") == 0) {
        InfiltratrProjectInfo info = project_info();
        return infiltratr_project_info_print(stdout, &info) == 0 ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "--palette") == 0) {
        const int failure = print_palette(argv[2]);
        if (failure != 0) {
            errno = failure;
            perror("runnerscope-native palette");
            return 1;
        }
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--atomic-write") == 0) {
        if (!argv[2] || !*argv[2]) return 2;
        const int failure = infiltratr_atomic_file_write(
            argv[2], INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS, copy_standard_input, NULL);
        if (failure != 0) {
            errno = failure;
            perror("runnerscope-native atomic-write");
            return 1;
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return self_test();

    usage(stderr);
    return 2;
}
