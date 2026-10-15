// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>

#include "platform/windows_config_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size)
        (void)snprintf(error, error_size, "%s", message ? message : "Configuration error.");
}

static void set_windows_error(char *error, size_t error_size,
                              const char *prefix, DWORD failure)
{
    if (error && error_size)
        (void)snprintf(error, error_size, "%s (Windows error %lu).",
                       prefix ? prefix : "Configuration operation failed",
                       (unsigned long)failure);
}

static wchar_t *config_path(void)
{
    const wchar_t *name = L"APPDATA";
    DWORD length = GetEnvironmentVariableW(name, NULL, 0U);
    if (!length) { name = L"LOCALAPPDATA"; length = GetEnvironmentVariableW(name, NULL, 0U); }
    if (!length || length > 32000U) return NULL;
    const wchar_t *suffix = L"\\RunnerScope\\config.json";
    const size_t capacity = (size_t)length + wcslen(suffix) + 1U;
    wchar_t *path = (wchar_t *)calloc(capacity, sizeof(wchar_t));
    if (!path) return NULL;
    const DWORD amount = GetEnvironmentVariableW(name, path, length);
    if (!amount || amount >= length) { free(path); return NULL; }
    wcscat_s(path, capacity, suffix);
    return path;
}

static bool wide_to_utf8(const wchar_t *input, char *output, size_t count)
{
    if (!output || !count) return false;
    output[0] = '\0';
    const int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        input, -1, NULL, 0, NULL, NULL);
    return needed > 0 && (size_t)needed <= count &&
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1,
                            output, (int)count, NULL, NULL) > 0;
}

bool runner_windows_config_path(char *output, size_t size)
{
    wchar_t *path = config_path();
    if (!path) { if (output && size) output[0] = '\0'; return false; }
    const bool ok = wide_to_utf8(path, output, size);
    free(path);
    return ok;
}

typedef struct {
    SECURITY_DESCRIPTOR descriptor;
    SECURITY_ATTRIBUTES attributes;
    TOKEN_USER *user;
    PACL acl;
    unsigned char system[SECURITY_MAX_SID_SIZE];
} PrivateSecurity;

static void private_security_destroy(PrivateSecurity *security)
{
    if (security->acl) LocalFree(security->acl);
    free(security->user);
    memset(security, 0, sizeof(*security));
}

static bool private_security_create(PrivateSecurity *security)
{
    memset(security, 0, sizeof(*security));
    HANDLE token = NULL;
    DWORD bytes = 0U;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    (void)GetTokenInformation(token, TokenUser, NULL, 0U, &bytes);
    security->user = (TOKEN_USER *)calloc((size_t)bytes, 1U);
    const BOOL user_ok = security->user &&
        GetTokenInformation(token, TokenUser, security->user, bytes, &bytes);
    CloseHandle(token);
    if (!user_ok) goto failed;
    DWORD system_size = sizeof(security->system);
    if (!CreateWellKnownSid(WinLocalSystemSid, NULL, security->system, &system_size)) goto failed;
    PSID principals[2] = {security->user->User.Sid, security->system};
    EXPLICIT_ACCESSW access[2];
    memset(access, 0, sizeof(access));
    for (size_t i = 0U; i < 2U; ++i) {
        access[i].grfAccessPermissions = FILE_ALL_ACCESS;
        access[i].grfAccessMode = SET_ACCESS;
        access[i].grfInheritance = NO_INHERITANCE;
        access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access[i].Trustee.TrusteeType = i == 0U ? TRUSTEE_IS_USER : TRUSTEE_IS_WELL_KNOWN_GROUP;
        access[i].Trustee.ptstrName = (LPWSTR)principals[i];
    }
    if (SetEntriesInAclW(2U, access, NULL, &security->acl) != ERROR_SUCCESS ||
        !InitializeSecurityDescriptor(&security->descriptor, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(&security->descriptor, TRUE, security->acl, FALSE) ||
        !SetSecurityDescriptorControl(&security->descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) goto failed;
    security->attributes.nLength = sizeof(security->attributes);
    security->attributes.lpSecurityDescriptor = &security->descriptor;
    security->attributes.bInheritHandle = FALSE;
    return true;
failed:
    private_security_destroy(security);
    return false;
}

static bool file_is_private(HANDLE file, const PrivateSecurity *security)
{
    PSECURITY_DESCRIPTOR descriptor = NULL;
    PACL acl = NULL;
    if (GetSecurityInfo(file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                        NULL, NULL, &acl, NULL, &descriptor) != ERROR_SUCCESS) return false;
    SECURITY_DESCRIPTOR_CONTROL control;
    DWORD revision = 0U;
    bool ok = acl && GetSecurityDescriptorControl(descriptor, &control, &revision) &&
              (control & SE_DACL_PROTECTED) != 0U;
    for (DWORD i = 0U; ok && i < acl->AceCount; ++i) {
        void *raw = NULL;
        if (!GetAce(acl, i, &raw) || ((ACE_HEADER *)raw)->AceType != ACCESS_ALLOWED_ACE_TYPE) {
            ok = false;
            break;
        }
        ACCESS_ALLOWED_ACE *ace = (ACCESS_ALLOWED_ACE *)raw;
        PSID sid = (PSID)&ace->SidStart;
        if (!EqualSid(sid, security->user->User.Sid) &&
            !EqualSid(sid, (PSID)security->system)) ok = false;
    }
    LocalFree(descriptor);
    return ok;
}

bool runner_windows_config_load(RunnerConfig *config, char *error, size_t error_size)
{
    if (!config) { set_error(error, error_size, "Configuration destination is missing."); return false; }
    runner_config_defaults(config);
    if (error && error_size) error[0] = '\0';
    wchar_t *path = config_path();
    if (!path) { set_error(error, error_size, "Unable to resolve the Runner Monitor configuration path."); return false; }
    HANDLE file = CreateFileW(path, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    const DWORD open_error = file == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    free(path);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD failure = open_error;
        if (failure == ERROR_FILE_NOT_FOUND || failure == ERROR_PATH_NOT_FOUND) return true;
        set_windows_error(error, error_size, "Unable to open configuration", failure);
        return false;
    }
    BY_HANDLE_FILE_INFORMATION info;
    LARGE_INTEGER size;
    if (!GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
        !GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        set_error(error, error_size, "Configuration is empty, unsafe, or unreasonably large.");
        return false;
    }
    char *buffer = (char *)calloc((size_t)size.QuadPart + 1U, 1U);
    if (!buffer) { CloseHandle(file); set_error(error, error_size, "Unable to allocate configuration buffer."); return false; }
    DWORD amount = 0U;
    const BOOL read_ok = ReadFile(file, buffer, (DWORD)size.QuadPart, &amount, NULL);
    bool parsed = false;
    if (!read_ok || amount != (DWORD)size.QuadPart)
        set_error(error, error_size, "Unable to read the complete configuration.");
    else if (memchr(buffer, '\0', (size_t)amount))
        set_error(error, error_size, "Configuration contains an unexpected NUL byte.");
    else parsed = runner_config_parse_json(buffer, config, error, error_size);
    SecureZeroMemory(buffer, (size_t)size.QuadPart + 1U);
    free(buffer);
    if (parsed && config->github_token[0]) {
        PrivateSecurity security;
        if (!private_security_create(&security)) {
            set_error(error, error_size, "Unable to verify private token file permissions.");
            parsed = false;
        } else {
            if (!file_is_private(file, &security)) {
                set_error(error, error_size, "The token file permissions are not private. Save Settings to secure this file before connecting.");
                parsed = false;
            }
            private_security_destroy(&security);
        }
    }
    CloseHandle(file);
    if (!parsed) SecureZeroMemory(config->github_token, sizeof(config->github_token));
    return parsed;
}

bool runner_windows_config_save(const RunnerConfig *config, char *error, size_t error_size)
{
    char json[8192];
    size_t length = 0U;
    if (!runner_config_write_json(config, json, sizeof(json), &length, error, error_size)) {
        SecureZeroMemory(json, sizeof(json));
        return false;
    }
    wchar_t *path = config_path();
    PrivateSecurity security;
    if (!path || !private_security_create(&security)) {
        free(path);
        SecureZeroMemory(json, sizeof(json));
        set_error(error, error_size, "Unable to prepare private configuration storage.");
        return false;
    }
    const size_t capacity = wcslen(path) + 80U;
    wchar_t *directory = (wchar_t *)calloc(capacity, sizeof(wchar_t));
    wchar_t *temporary = (wchar_t *)calloc(capacity, sizeof(wchar_t));
    HANDLE file = INVALID_HANDLE_VALUE, directory_handle = INVALID_HANDLE_VALUE;
    bool ok = false;
    bool temporary_created = false;
    if (!directory || !temporary) { set_error(error, error_size, "Unable to allocate configuration paths."); goto done; }
    wcscpy_s(directory, capacity, path);
    wchar_t *slash = wcsrchr(directory, L'\\');
    if (!slash) { set_error(error, error_size, "Configuration directory is invalid."); goto done; }
    *slash = L'\0';
    if (!CreateDirectoryW(directory, &security.attributes) && GetLastError() != ERROR_ALREADY_EXISTS) {
        set_windows_error(error, error_size, "Unable to create private configuration directory", GetLastError());
        goto done;
    }
    directory_handle = CreateFileW(directory, READ_CONTROL | WRITE_DAC,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    BY_HANDLE_FILE_INFORMATION info;
    if (directory_handle == INVALID_HANDLE_VALUE ||
        !GetFileInformationByHandle(directory_handle, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
        SetSecurityInfo(directory_handle, SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            NULL, NULL, security.acl, NULL) != ERROR_SUCCESS) {
        set_error(error, error_size, "Unable to secure the configuration directory. No token was written.");
        goto done;
    }
    static volatile LONG sequence = 0;
    _snwprintf_s(temporary, capacity, _TRUNCATE, L"%ls.tmp-%lu-%llu-%ld", path,
        (unsigned long)GetCurrentProcessId(), GetTickCount64(), InterlockedIncrement(&sequence));
    file = CreateFileW(temporary, GENERIC_WRITE | READ_CONTROL, 0, &security.attributes,
                        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    temporary_created = file != INVALID_HANDLE_VALUE;
    if (file == INVALID_HANDLE_VALUE || !file_is_private(file, &security)) {
        set_error(error, error_size, "Unable to create a private token file. No token was written.");
        goto done;
    }
    DWORD written = 0U;
    if (!WriteFile(file, json, (DWORD)length, &written, NULL) || written != (DWORD)length ||
        !FlushFileBuffers(file)) {
        set_error(error, error_size, "Unable to publish the complete configuration.");
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    if (!MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        set_windows_error(error, error_size, "Unable to replace configuration", GetLastError());
        goto done;
    }
    ok = true;
    if (error && error_size) error[0] = '\0';
done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (directory_handle != INVALID_HANDLE_VALUE) CloseHandle(directory_handle);
    if (!ok && temporary_created) DeleteFileW(temporary);
    private_security_destroy(&security);
    SecureZeroMemory(json, sizeof(json));
    free(temporary);
    free(directory);
    free(path);
    return ok;
}
