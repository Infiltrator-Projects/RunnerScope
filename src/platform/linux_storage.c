// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "platform/linux_storage.h"
#include <infiltratr/posix.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *runner_linux_config_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "runnerscope", "config.json", NULL);
}

char *runner_linux_history_path(void)
{
    return g_build_filename(g_get_user_state_dir(), "runnerscope", "history.tsv", NULL);
}

static char *read_text(const char *path, size_t limit, gboolean *missing, GError **error)
{
    if (missing) *missing = FALSE;
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT) { if (missing) *missing = TRUE; return g_strdup(""); }
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
            "Unable to read saved data: %s. The original file has been kept.", g_strerror(errno));
        return NULL;
    }
    struct stat status;
    gboolean ok = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) &&
        status.st_size >= 0 && (guint64)status.st_size <= limit;
    char *contents = g_malloc(limit + 2U);
    size_t used = 0U;
    while (ok && used <= limit) {
        const ssize_t count = read(fd, contents + used, limit + 1U - used);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { ok = FALSE; break; }
        if (!count) break;
        used += (size_t)count;
    }
    (void)close(fd);
    if (!ok || used > limit || memchr(contents, '\0', used) ||
        !g_utf8_validate(contents, (gssize)used, NULL)) {
        memset(contents, 0, limit + 2U);
        g_free(contents);
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
            "Saved data is unreadable, oversized or invalid. The original file has been kept.");
        return NULL;
    }
    contents[used] = '\0';
    return contents;
}

static gboolean write_text(const char *path, const char *text, size_t length, GError **error)
{
    char *directory = g_path_get_dirname(path);
    int failure = 0;
    if (g_mkdir_with_parents(directory, 0700) != 0) failure = errno;
    g_free(directory);
    if (!failure) failure = infiltratr_atomic_file_write_bytes(path,
        INFILTRATR_ATOMIC_FILE_PRIVATE, text, length);
    if (!failure) return TRUE;
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
        "Unable to save data: %s", g_strerror(failure));
    return FALSE;
}

gboolean runner_linux_config_load(RunnerConfig *config, GError **error)
{
    if (!config) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "Configuration destination is missing.");
        return FALSE;
    }
    runner_config_defaults(config);
    char *path = runner_linux_config_path();
    /* Distinguish a missing file from an existing empty (corrupt) document. */
    gboolean missing = FALSE;
    char *contents = read_text(path, 65536U, &missing, error);
    g_free(path);
    if (!contents) return FALSE;
    char detail[256] = "";
    const gboolean ok = missing || runner_config_parse_json(contents, config, detail, sizeof(detail));
    memset(contents, 0, strlen(contents));
    g_free(contents);
    if (!ok) g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Unable to read saved settings: %s. The original file has been kept.", detail);
    return ok;
}

gboolean runner_linux_config_save(const RunnerConfig *config, GError **error)
{
    char json[8192], detail[256] = "";
    size_t written = 0U;
    if (!runner_config_write_json(config, json, sizeof(json), &written, detail, sizeof(detail))) {
        memset(json, 0, sizeof(json));
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", detail);
        return FALSE;
    }
    char *path = runner_linux_config_path();
    const gboolean ok = write_text(path, json, written, error);
    memset(json, 0, sizeof(json));
    g_free(path);
    return ok;
}

void runner_history_entry_free(gpointer data)
{
    RunnerHistoryEntry *row = data;
    if (!row) return;
    g_free(row->time_text); g_free(row->runner); g_free(row->event);
    g_free(row->detail); g_free(row);
}

GPtrArray *runner_linux_history_load(GError **error)
{
    char *path = runner_linux_history_path();
    char *contents = read_text(path, RUNNER_HISTORY_BYTE_LIMIT, NULL, error);
    g_free(path);
    if (!contents) return NULL;
    GPtrArray *entries = g_ptr_array_new_with_free_func(runner_history_entry_free);
    char **lines = g_strsplit(contents, "\n", -1);
    gboolean ok = TRUE;
    for (guint i = 0U; lines[i]; ++i) {
        if (!*lines[i]) continue;
        char **parts = g_strsplit(lines[i], "\t", 4);
        if (g_strv_length(parts) != 4U) ok = FALSE;
        else if (entries->len < RUNNER_HISTORY_LIMIT) {
            RunnerHistoryEntry *row = g_new0(RunnerHistoryEntry, 1U);
            row->time_text = g_strdup(parts[0]); row->runner = g_strdup(parts[1]);
            row->event = g_strdup(parts[2]); row->detail = g_strdup(parts[3]);
            g_ptr_array_add(entries, row);
        }
        g_strfreev(parts);
        if (!ok) break;
    }
    g_strfreev(lines); g_free(contents);
    if (ok) return entries;
    g_ptr_array_unref(entries);
    g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Saved history is malformed. The original file has been kept; new events remain in memory.");
    return NULL;
}

gboolean runner_linux_history_save(const GPtrArray *entries, GError **error)
{
    GString *text = g_string_new(NULL);
    gboolean ok = entries && entries->len <= RUNNER_HISTORY_LIMIT;
    for (guint i = 0U; ok && i < entries->len; ++i) {
        const RunnerHistoryEntry *row = g_ptr_array_index((GPtrArray *)entries, i);
        if (!row) { ok = FALSE; break; }
        const char *fields[] = {row->time_text, row->runner, row->event, row->detail};
        for (guint f = 0U; ok && f < G_N_ELEMENTS(fields); ++f) {
            const char *value = fields[f] ? fields[f] : "";
            const size_t length = strlen(value);
            if (length + 1U > RUNNER_HISTORY_BYTE_LIMIT - text->len ||
                !g_utf8_validate(value, -1, NULL)) { ok = FALSE; break; }
            for (const char *p = value; *p; ++p)
                g_string_append_c(text, *p == '\t' || *p == '\r' || *p == '\n' ? ' ' : *p);
            g_string_append_c(text, f == 3U ? '\n' : '\t');
        }
    }
    if (ok) {
        char *path = runner_linux_history_path();
        ok = write_text(path, text->str, text->len, error);
        g_free(path);
    } else g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "History exceeds its limits or contains invalid text. The saved file has been kept.");
    g_string_free(text, TRUE);
    return ok;
}
