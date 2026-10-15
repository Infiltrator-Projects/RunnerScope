// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>

#include "platform/windows_config_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int fail(const char *message, const char *detail, int code)
{
    fprintf(stderr, "%s%s%s\n", message, detail && *detail ? ": " : "",
            detail && *detail ? detail : "");
    return code;
}

static bool private_file(const wchar_t *path)
{
    PSECURITY_DESCRIPTOR descriptor = NULL;
    PACL acl = NULL;
    if (GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        NULL, NULL, &acl, NULL, &descriptor) != ERROR_SUCCESS) return false;
    HANDLE token = NULL;
    DWORD bytes = 0U;
    TOKEN_USER *user = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        (void)GetTokenInformation(token, TokenUser, NULL, 0U, &bytes);
        user = (TOKEN_USER *)calloc((size_t)bytes, 1U);
        if (user && !GetTokenInformation(token, TokenUser, user, bytes, &bytes)) {
            free(user);
            user = NULL;
        }
        CloseHandle(token);
    }
    SECURITY_DESCRIPTOR_CONTROL control;
    DWORD revision = 0U;
    bool ok = user && acl && acl->AceCount == 2U &&
        GetSecurityDescriptorControl(descriptor, &control, &revision) &&
        (control & SE_DACL_PROTECTED) != 0U;
    for (DWORD i = 0U; ok && i < acl->AceCount; ++i) {
        void *raw = NULL;
        if (!GetAce(acl, i, &raw) || ((ACE_HEADER *)raw)->AceType != ACCESS_ALLOWED_ACE_TYPE ||
            (((ACE_HEADER *)raw)->AceFlags & INHERITED_ACE) != 0U) { ok = false; break; }
        PSID sid = (PSID)&((ACCESS_ALLOWED_ACE *)raw)->SidStart;
        if (!EqualSid(sid, user->User.Sid) && !IsWellKnownSid(sid, WinLocalSystemSid)) ok = false;
    }
    free(user);
    LocalFree(descriptor);
    return ok;
}

static bool make_public_fixture(const wchar_t *path)
{
    unsigned char everyone[SECURITY_MAX_SID_SIZE];
    DWORD bytes = sizeof(everyone);
    if (!CreateWellKnownSid(WinWorldSid, NULL, everyone, &bytes)) return false;
    EXPLICIT_ACCESSW entry;
    memset(&entry, 0, sizeof(entry));
    entry.grfAccessPermissions = FILE_GENERIC_READ;
    entry.grfAccessMode = SET_ACCESS;
    entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entry.Trustee.ptstrName = (LPWSTR)everyone;
    PACL acl = NULL;
    if (SetEntriesInAclW(1U, &entry, NULL, &acl) != ERROR_SUCCESS) return false;
    const DWORD status = SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, NULL, NULL, acl, NULL);
    LocalFree(acl);
    return status == ERROR_SUCCESS;
}

int main(void)
{
    wchar_t temp[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp)) return fail("GetTempPathW failed", NULL, 1);
    wchar_t root[MAX_PATH];
    _snwprintf_s(root, MAX_PATH, _TRUNCATE,
                 L"%lsRunnerScope-config-test-%lu", temp,
                 (unsigned long)GetCurrentProcessId());
    if (!CreateDirectoryW(root, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return fail("test directory creation failed", NULL, 2);
    if (!SetEnvironmentVariableW(L"APPDATA", root))
        return fail("APPDATA override failed", NULL, 3);

    RunnerConfig config;
    runner_config_defaults(&config);
    (void)snprintf(config.organisation, sizeof(config.organisation), "%s", "Infiltrator-Projects");
    config.expected_runners = 9U;
    config.runner_poll_seconds = 7U;
    config.activity_scan_seconds = 80U;
    config.repository_scan_limit = 31U;
    config.local_health_seconds = 22U;
    config.theme_mode = RUNNER_THEME_NIGHT;
    (void)snprintf(config.github_token, sizeof(config.github_token), "%s", "test_only_saved_token");

    wchar_t path[MAX_PATH];
    _snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%ls\\RunnerScope\\config.json", root);

    char error[256];
    if (!runner_windows_config_save(&config, error, sizeof(error)))
        return fail("save failed", error, 4);

    RunnerConfig loaded;
    if (!runner_windows_config_load(&loaded, error, sizeof(error)))
        return fail("load failed", error, 5);
    if (strcmp(loaded.organisation, config.organisation) != 0 ||
        strcmp(loaded.github_token, config.github_token) != 0 ||
        loaded.expected_runners != config.expected_runners ||
        loaded.runner_poll_seconds != config.runner_poll_seconds ||
        loaded.activity_scan_seconds != config.activity_scan_seconds ||
        loaded.repository_scan_limit != config.repository_scan_limit ||
        loaded.local_health_seconds != config.local_health_seconds ||
        loaded.theme_mode != config.theme_mode)
        return fail("roundtrip changed configuration", NULL, 6);
    if (!private_file(path)) return fail("token file ACL was not private at publication", NULL, 14);
    char readable_path[4096];
    if (!runner_windows_config_path(readable_path, sizeof(readable_path)) ||
        !strstr(readable_path, "RunnerScope\\config.json"))
        return fail("configuration path was not readable", NULL, 15);

    /* A maximum-length escaped token must not overflow the JSON save buffer. */
    memset(config.github_token, '"', sizeof(config.github_token) - 1U);
    config.github_token[sizeof(config.github_token) - 1U] = '\0';
    if (!runner_windows_config_save(&config, error, sizeof(error)) ||
        !runner_windows_config_load(&loaded, error, sizeof(error)) ||
        strcmp(loaded.github_token, config.github_token) != 0)
        return fail("maximum escaped token roundtrip failed", error, 16);
    if (!make_public_fixture(path)) return fail("public ACL fixture failed", NULL, 17);
    if (runner_windows_config_load(&loaded, error, sizeof(error)) ||
        !strstr(error, "private") || loaded.github_token[0])
        return fail("unsafe token file was accepted", NULL, 18);
    if (!runner_windows_config_save(&config, error, sizeof(error)) || !private_file(path) ||
        !runner_windows_config_load(&loaded, error, sizeof(error)))
        return fail("atomic replacement inherited unsafe permissions", error, 19);

    HANDLE held = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (held == INVALID_HANDLE_VALUE) return fail("atomic failure fixture failed", NULL, 20);
    RunnerConfig changed = config;
    (void)snprintf(changed.organisation, sizeof(changed.organisation), "%s", "Replacement-Org");
    const bool replaced = runner_windows_config_save(&changed, error, sizeof(error));
    CloseHandle(held);
    if (replaced || !runner_windows_config_load(&loaded, error, sizeof(error)) ||
        strcmp(loaded.organisation, config.organisation) != 0 ||
        strcmp(loaded.github_token, config.github_token) != 0)
        return fail("failed atomic save changed prior settings", NULL, 21);

    (void)SetEnvironmentVariableW(L"GITHUB_RUNNER_ORG", L"Ignored-Environment-Org");
    (void)SetEnvironmentVariableW(L"GH_TOKEN", L"test_only_environment_token");
    (void)SetEnvironmentVariableW(L"GITHUB_TOKEN", L"test_only_environment_token");
    if (!runner_windows_config_load(&loaded, error, sizeof(error)) ||
        strcmp(loaded.organisation, config.organisation) != 0 ||
        strcmp(loaded.github_token, config.github_token) != 0)
        return fail("environment replaced saved settings", NULL, 22);

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return fail("NUL fixture open failed", NULL, 25);
    const char nul_fixture[] = "{\"organisation\":\"A\"}\0{\"garbage\":true}";
    DWORD nul_written = 0U;
    const BOOL nul_ok = WriteFile(file, nul_fixture, (DWORD)(sizeof(nul_fixture) - 1U), &nul_written, NULL);
    CloseHandle(file);
    if (!nul_ok || runner_windows_config_load(&loaded, error, sizeof(error)) || !strstr(error, "NUL"))
        return fail("configuration with hidden NUL suffix was accepted", NULL, 26);

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return fail("legacy fixture open failed", NULL, 23);
    const char legacy[] = "{\"organisation\":\"Legacy-Org\",\"expected_runners\":9,\"runner_poll_seconds\":7,"
        "\"activity_scan_seconds\":80,\"repository_scan_limit\":31,\"local_health_seconds\":22,\"theme_mode\":\"night\"}";
    DWORD legacy_written = 0U;
    const BOOL legacy_ok = WriteFile(file, legacy, (DWORD)(sizeof(legacy) - 1U), &legacy_written, NULL);
    CloseHandle(file);
    if (!legacy_ok || !runner_windows_config_load(&loaded, error, sizeof(error)) ||
        loaded.github_token[0] || loaded.expected_runners != 9U ||
        loaded.runner_poll_seconds != 7U || loaded.activity_scan_seconds != 80U ||
        loaded.repository_scan_limit != 31U || loaded.local_health_seconds != 22U ||
        loaded.theme_mode != RUNNER_THEME_NIGHT)
        return fail("legacy preferences were not preserved for token setup", error, 24);

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return fail("malformed fixture open failed", NULL, 11);
    const char bad[] = "{\"organisation\":\"A\",\"organisation\":\"B\"}";
    DWORD written = 0U;
    const BOOL write_ok = WriteFile(file, bad, (DWORD)(sizeof(bad) - 1U), &written, NULL);
    CloseHandle(file);
    if (!write_ok || written != (DWORD)(sizeof(bad) - 1U))
        return fail("malformed fixture write failed", NULL, 12);
    if (runner_windows_config_load(&loaded, error, sizeof(error)))
        return fail("malformed configuration was silently accepted", NULL, 13);

    DeleteFileW(path);
    wchar_t config_dir[MAX_PATH];
    _snwprintf_s(config_dir, MAX_PATH, _TRUNCATE, L"%ls\\RunnerScope", root);
    RemoveDirectoryW(config_dir);
    RemoveDirectoryW(root);
    puts("Windows configuration store contract passed.");
    return 0;
}
