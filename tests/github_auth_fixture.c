// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void pause_ms(void) { Sleep(200U); }
#else
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
static void pause_ms(void) { struct timespec t = {0, 200000000}; (void)nanosleep(&t, NULL); }
#endif
int main(int argc, char **argv)
{
    const char *dir = getenv("GH_CONFIG_DIR");
    const char *mode = getenv("RUNNERSCOPE_AUTH_FIXTURE");
    if (argc < 3 || strcmp(argv[1], "auth") != 0) return 21;
    if (getenv("GH_TOKEN") || getenv("GITHUB_TOKEN") || getenv("GH_DEBUG") || getenv("GH_FORCE_TTY")) return 22;
    if (!getenv("GH_PROMPT_DISABLED") || strcmp(getenv("GH_HOST"), "github.com") != 0) return 23;
    char file[32768];
    if (!dir || snprintf(file, sizeof(file), "%s/fixture-session", dir) < 0) return 24;
    if (strcmp(argv[2], "token") == 0) {
        FILE *f = fopen(file, "rb");
        if (!f) return 1;
        fclose(f);
        puts(mode && strcmp(mode, "bad-token") == 0 ? "invalid token secret" : "offline_process_token_1234567890");
        return 0;
    }
    if (strcmp(argv[2], "login") != 0 || argc != 13) return 25;
    if (strcmp(argv[3], "--web") || strcmp(argv[4], "--hostname") || strcmp(argv[5], "github.com") ||
        strcmp(argv[6], "--git-protocol") || strcmp(argv[7], "https") || strcmp(argv[8], "--skip-ssh-key") ||
        strcmp(argv[9], "--scopes") || strcmp(argv[10], "admin:org,repo") ||
        strcmp(argv[11], "--clipboard=false") || strcmp(argv[12], "--insecure-storage")) return 26;
    if (getc(stdin) != EOF) return 27;
#ifndef _WIN32
    struct stat st;
    if (stat(dir, &st) || (st.st_mode & 077U) != 0U || st.st_uid != geteuid()) return 28;
#endif
    fputs("! First copy your one-time co", stderr); fflush(stderr); pause_ms();
    fputs("de: ABCD-EFGH\n", stderr); fflush(stderr);
    if (mode && strcmp(mode, "long-line") == 0) {
        for (size_t i = 0U; i < 8192U; ++i) fputc('X', stderr);
        fflush(stderr);
    }
    if (mode && strcmp(mode, "hang") == 0) for (;;) pause_ms();
    pause_ms();
    FILE *f = fopen(file, "wb"); if (!f) return 29;
    fputs("fixture only", f); fclose(f);
    return 0;
}
