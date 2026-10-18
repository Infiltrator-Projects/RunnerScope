// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "platform/linux_storage.h"
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    GError *error = NULL;
    char *directory = g_dir_make_tmp("runnerscope-storage-test-XXXXXX", &error);
    g_assert_no_error(error);
    g_setenv("XDG_CONFIG_HOME", directory, TRUE);
    g_setenv("XDG_STATE_HOME", directory, TRUE);
    RunnerConfig config, loaded;
    g_assert_true(runner_linux_config_load(&loaded, &error));
    g_assert_no_error(error);
    runner_config_defaults(&config);
    g_strlcpy(config.github_token, "offline_storage_fixture", sizeof(config.github_token));
    g_strlcpy(config.organisation, "FixtureOrg", sizeof(config.organisation));
    config.runner_poll_seconds = 2U;
    g_assert_true(runner_linux_config_save(&config, &error));
    g_assert_true(runner_linux_config_load(&loaded, &error));
    g_assert_cmpmem(&loaded, sizeof(loaded), &config, sizeof(config));
    char *path = runner_linux_config_path();
    GStatBuf status;
    g_assert_cmpint(g_stat(path, &status), ==, 0);
    g_assert_cmpuint(status.st_mode & 0777U, ==, 0600U);
    g_assert_cmpint(g_chmod(path, 0644), ==, 0);
    g_assert_true(runner_linux_config_save(&config, &error));
    g_assert_cmpint(g_stat(path, &status), ==, 0);
    g_assert_cmpuint(status.st_mode & 0777U, ==, 0600U);
    const char *invalid[] = {"", "{bad config", "{\"organisation\":\"FixtureOrg\",\"expected_runners\":-1}"};
    for (size_t i = 0U; i < G_N_ELEMENTS(invalid); ++i) {
        g_assert_true(g_file_set_contents(path, invalid[i], -1, &error));
        g_assert_false(runner_linux_config_load(&loaded, &error));
        g_assert_nonnull(error); g_clear_error(&error);
        char *preserved = NULL;
        g_assert_true(g_file_get_contents(path, &preserved, NULL, &error));
        g_assert_cmpstr(preserved, ==, invalid[i]); g_free(preserved);
    }
    g_assert_cmpint(g_unlink(path), ==, 0);
    g_assert_cmpint(mkfifo(path, 0600), ==, 0);
    g_assert_false(runner_linux_config_load(&loaded, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    g_assert_cmpint(g_unlink(path), ==, 0);
    g_assert_cmpint(symlink("missing-target", path), ==, 0);
    g_assert_false(runner_linux_config_load(&loaded, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    g_assert_cmpint(g_unlink(path), ==, 0);
    char *history_path = runner_linux_history_path();
    GPtrArray *entries = runner_linux_history_load(&error);
    g_assert_no_error(error); g_assert_cmpuint(entries->len, ==, 0U);
    RunnerHistoryEntry *row = g_new0(RunnerHistoryEntry, 1U);
    row->time_text = g_strdup("18/10 10:30:00"); row->runner = g_strdup("Fixture\trunner");
    row->event = g_strdup("RUNNING"); row->detail = g_strdup("Build\nnew line\rreturn");
    g_ptr_array_add(entries, row);
    g_assert_true(runner_linux_history_save(entries, &error));
    GPtrArray *roundtrip = runner_linux_history_load(&error);
    g_assert_no_error(error); g_assert_cmpuint(roundtrip->len, ==, 1U);
    RunnerHistoryEntry *actual = g_ptr_array_index(roundtrip, 0U);
    g_assert_cmpstr(actual->runner, ==, "Fixture runner");
    g_assert_cmpstr(actual->detail, ==, "Build new line return");
    g_ptr_array_unref(roundtrip);
    static const char nul_history[] = "time\trunner\tRUNNING\tdetail\0hidden";
    g_assert_true(g_file_set_contents(history_path, nul_history, sizeof(nul_history) - 1U, &error));
    g_assert_null(runner_linux_history_load(&error));
    g_assert_nonnull(error); g_clear_error(&error);
    g_assert_true(g_file_set_contents(history_path, "malformed history", -1, &error));
    g_assert_null(runner_linux_history_load(&error));
    g_assert_nonnull(error); g_clear_error(&error);
    char *huge = g_malloc0(RUNNER_HISTORY_BYTE_LIMIT + 2U);
    memset(huge, 'x', RUNNER_HISTORY_BYTE_LIMIT + 1U);
    g_assert_true(g_file_set_contents(history_path, huge, RUNNER_HISTORY_BYTE_LIMIT + 1U, &error));
    g_assert_null(runner_linux_history_load(&error));
    g_assert_nonnull(error); g_clear_error(&error); g_free(huge);
    /* A rejected in-memory write also leaves the previous file intact. */
    g_assert_true(g_file_set_contents(history_path, "time\trunner\tIDLE\tdetail\n", -1, &error));
    g_free(row->detail); row->detail = g_strdup("\xff");
    g_assert_false(runner_linux_history_save(entries, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    roundtrip = runner_linux_history_load(&error);
    g_assert_no_error(error); g_assert_cmpuint(roundtrip->len, ==, 1U);
    g_ptr_array_unref(roundtrip); g_ptr_array_unref(entries);
    g_assert_cmpint(g_unlink(history_path), ==, 0);
    char *parent = g_path_get_dirname(path);
    g_assert_cmpint(g_rmdir(parent), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(parent); g_free(history_path); g_free(path); g_free(directory);
    g_print("Linux settings/history: private publication, bounded reads, corruption, FIFO and symlink handling passed.\n");
    return 0;
}
