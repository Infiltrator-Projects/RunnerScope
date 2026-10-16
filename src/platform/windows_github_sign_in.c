// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "platform/github_sign_in.h"
#include "platform/windows_config_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define GH_RESOURCE 2301
typedef struct { const wchar_t *executable; const wchar_t *directory; } Broker;
static bool temp_directory(wchar_t directory[32768])
{
    wchar_t root[32768];
    const DWORD n = GetTempPathW(32768U, root);
    unsigned char random[16];
    if (!n || n > 32600U ||
        BCryptGenRandom(NULL, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
    wchar_t name[33];
    for (size_t i = 0U; i < sizeof(random); ++i)
        (void)swprintf_s(name + i * 2U, 33U - i * 2U, L"%02x", (unsigned int)random[i]);
    if (swprintf_s(directory, 32768U, L"%srunnerscope-sign-in-%s", root, name) < 0) return false;
    return runner_windows_private_directory(directory);
}
static void remove_config(const wchar_t *directory)
{
    wchar_t pattern[32768];
    (void)swprintf_s(pattern, 32768U, L"%s\\*", directory);
    WIN32_FIND_DATAW item;
    HANDLE find = FindFirstFileW(pattern, &item);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) continue;
            wchar_t file[32768];
            if (swprintf_s(file, 32768U, L"%s\\%s", directory, item.cFileName) > 0)
                (void)DeleteFileW(file);
        } while (FindNextFileW(find, &item));
        FindClose(find);
    }
    (void)RemoveDirectoryW(directory);
}
static bool extract_broker(const wchar_t *directory, wchar_t executable[32768])
{
    HMODULE module = GetModuleHandleW(NULL);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(GH_RESOURCE), RT_RCDATA);
    HGLOBAL loaded = resource ? LoadResource(module, resource) : NULL;
    const void *bytes = loaded ? LockResource(loaded) : NULL;
    const DWORD size = resource ? SizeofResource(module, resource) : 0U;
    if (!bytes || !size || swprintf_s(executable, 32768U, L"%s\\gh.exe", directory) < 0) return false;
    HANDLE file = CreateFileW(executable, GENERIC_WRITE, 0U, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0U;
    const bool ok = WriteFile(file, bytes, size, &written, NULL) && written == size && FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}
static bool env_key(const wchar_t *value, const wchar_t *name)
{
    const size_t length = wcslen(name);
    return _wcsnicmp(value, name, length) == 0 && value[length] == L'=';
}
static wchar_t *child_environment(RunnerAuthCommand command, const wchar_t *directory)
{
    LPWCH original = GetEnvironmentStringsW();
    if (!original) return NULL;
    size_t capacity = 65536U + (directory ? wcslen(directory) : 0U);
    for (const wchar_t *p = original; *p; p += wcslen(p) + 1U) capacity += wcslen(p) + 1U;
    wchar_t *result = calloc(capacity, sizeof(wchar_t));
    size_t used = 0U;
    const wchar_t *blocked[] = {L"GH_TOKEN",L"GITHUB_TOKEN",L"GH_ENTERPRISE_TOKEN",L"GITHUB_ENTERPRISE_TOKEN",
        L"GH_DEBUG",L"GH_FORCE_TTY",L"CLICOLOR_FORCE",L"FORCE_COLOR",L"GH_BROWSER",L"GH_HTTP_UNIX_SOCKET",L"GH_PROMPT_DISABLED",
        L"GH_HOST",L"NO_COLOR",L"CLICOLOR",L"LC_ALL",L"GH_NO_UPDATE_NOTIFIER",L"GH_NO_EXTENSION_UPDATE_NOTIFIER"};
    if (result) for (const wchar_t *p = original; *p; p += wcslen(p) + 1U) {
        bool skip = false;
        for (size_t i = 0U; i < sizeof(blocked) / sizeof(blocked[0]); ++i)
            if (env_key(p, blocked[i])) skip = true;
        if (command != RUNNER_AUTH_EXISTING_SESSION && env_key(p, L"GH_CONFIG_DIR")) skip = true;
        if (!skip) { const size_t n = wcslen(p) + 1U; memcpy(result + used, p, n * sizeof(wchar_t)); used += n; }
    }
    FreeEnvironmentStringsW(original);
    if (!result) return NULL;
    const wchar_t *settings[] = {L"GH_PROMPT_DISABLED=1",L"GH_HOST=github.com",L"NO_COLOR=1",
        L"CLICOLOR=0",L"LC_ALL=C",L"GH_NO_UPDATE_NOTIFIER=1",L"GH_NO_EXTENSION_UPDATE_NOTIFIER=1"};
    for (size_t i = 0U; i < sizeof(settings) / sizeof(settings[0]); ++i) {
        const size_t n = wcslen(settings[i]) + 1U;
        memcpy(result + used, settings[i], n * sizeof(wchar_t)); used += n;
    }
    if (command != RUNNER_AUTH_EXISTING_SESSION && directory) {
        const int n = swprintf_s(result + used, capacity - used, L"GH_CONFIG_DIR=%s", directory);
        if (n < 0) { free(result); return NULL; }
        used += (size_t)n + 1U;
    }
    result[used] = L'\0';
    return result;
}
static bool execute(RunnerAuthCommand command, char *output, size_t capacity,
    const RunnerAuthCallbacks *callbacks, void *context)
{
    Broker *broker = context;
    if (!output || !capacity) return false;
    output[0] = '\0';
    wchar_t *environment = child_environment(command, broker->directory);
    if (!environment) return false;
    wchar_t args[32768];
    const wchar_t *tail = command == RUNNER_AUTH_BROWSER_LOGIN
        ? L"auth login --web --hostname github.com --git-protocol https --skip-ssh-key --scopes admin:org,repo --clipboard=false --insecure-storage"
        : command == RUNNER_AUTH_VERSION ? L"--version" : L"auth token --hostname github.com";
    if (swprintf_s(args, 32768U, L"\"%s\" %s", broker->executable, tail) < 0) { free(environment); return false; }
    SECURITY_ATTRIBUTES inherit = {sizeof(inherit), NULL, TRUE};
    HANDLE reader = NULL, writer = NULL, input = INVALID_HANDLE_VALUE, job = NULL;
    PROCESS_INFORMATION process; ZeroMemory(&process, sizeof(process));
    STARTUPINFOEXW startup; ZeroMemory(&startup, sizeof(startup));
    startup.StartupInfo.cb = sizeof(startup);
    SIZE_T attribute_size = 0U;
    bool ok = false, attributes_ready = false;
    if (!CreatePipe(&reader, &writer, &inherit, 0U) ||
        !SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0U)) goto cleanup;
    input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0U, NULL);
    if (input == INVALID_HANDLE_VALUE) goto cleanup;
    (void)InitializeProcThreadAttributeList(NULL, 1U, 0U, &attribute_size);
    startup.lpAttributeList = malloc(attribute_size);
    if (!startup.lpAttributeList ||
        !InitializeProcThreadAttributeList(startup.lpAttributeList, 1U, 0U, &attribute_size)) goto cleanup;
    attributes_ready = true;
    HANDLE handles[2] = {writer, input};
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        handles, sizeof(handles), NULL, NULL)) goto cleanup;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input;
    startup.StartupInfo.hStdOutput = writer;
    startup.StartupInfo.hStdError = writer;
    job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits; ZeroMemory(&limits, sizeof(limits));
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
        !CreateProcessW(broker->executable, args, NULL, NULL, TRUE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED,
            environment, NULL, &startup.StartupInfo, &process)) goto cleanup;
    if (!AssignProcessToJobObject(job, process.hProcess)) {
        (void)TerminateProcess(process.hProcess, 1U); goto cleanup;
    }
    if (ResumeThread(process.hThread) == (DWORD)-1) goto cleanup;
    CloseHandle(writer); writer = NULL;
    const bool login = command == RUNNER_AUTH_BROWSER_LOGIN;
    const ULONGLONG deadline = GetTickCount64() + (login ? 16U * 60U * 1000U : 20000U);
    char line[4096] = "";
    size_t used = 0U, captured = 0U;
    bool failed = false;
    for (;;) {
        if (runner_github_auth_cancelled(callbacks) || GetTickCount64() >= deadline) { failed = true; break; }
        DWORD available = 0U;
        const BOOL peek = PeekNamedPipe(reader, NULL, 0U, NULL, &available, NULL);
        if (!peek && GetLastError() != ERROR_BROKEN_PIPE) { failed = true; break; }
        if (available) {
            char bytes[512]; DWORD amount = 0U;
            if (!ReadFile(reader, bytes, available < sizeof(bytes) ? available : (DWORD)sizeof(bytes), &amount, NULL)) { failed = true; break; }
            for (DWORD i = 0U; i < amount; ++i) {
                if (!login) {
                    if (captured + 1U >= capacity || bytes[i] == '\0') { failed = true; break; }
                    output[captured++] = bytes[i]; output[captured] = '\0';
                } else if (bytes[i] == '\n') {
                    line[used] = '\0'; char code[32];
                    if (runner_github_auth_code(line, code) && callbacks && callbacks->challenge)
                        callbacks->challenge(code, callbacks->context);
                    SecureZeroMemory(line, sizeof(line)); used = 0U;
                } else if (used + 1U < sizeof(line)) line[used++] = bytes[i];
                else { failed = true; break; }
            }
            if (failed) break;
        } else if (WaitForSingleObject(process.hProcess, 0U) == WAIT_OBJECT_0) break;
        else Sleep(20U);
    }
    SecureZeroMemory(line, sizeof(line));
    if (!failed) { DWORD status = 1U; ok = GetExitCodeProcess(process.hProcess, &status) && status == 0U; }
cleanup:
    if (job) CloseHandle(job);
    if (process.hProcess) { (void)WaitForSingleObject(process.hProcess, 5000U); CloseHandle(process.hProcess); }
    if (process.hThread) CloseHandle(process.hThread);
    if (reader) CloseHandle(reader);
    if (writer) CloseHandle(writer);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (startup.lpAttributeList) { if (attributes_ready) DeleteProcThreadAttributeList(startup.lpAttributeList); free(startup.lpAttributeList); }
    free(environment);
    if (!ok) SecureZeroMemory(output, capacity);
    return ok;
}
bool runner_github_sign_in_available(void)
{
    return FindResourceW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(GH_RESOURCE), RT_RCDATA) != NULL;
}
bool runner_github_sign_in_probe(void)
{
    wchar_t directory[32768], executable[32768];
    if (!temp_directory(directory)) return false;
    bool ok = false;
    if (extract_broker(directory, executable)) {
        Broker broker = {executable, directory}; char output[512];
        ok = execute(RUNNER_AUTH_VERSION, output, sizeof(output), NULL, &broker) &&
            strstr(output, "gh version " RUNNERSCOPE_GH_VERSION) != NULL;
    }
    remove_config(directory); return ok;
}
void runner_github_sign_in_with_executable(RunnerGithubAuth *auth, const char *organisation,
    const RunnerAuthCallbacks *callbacks, const char *executable, RunnerHttpGet get, void *http_context)
{
    wchar_t directory[32768], file[32768];
    if (!temp_directory(directory)) goto failed;
    if (executable) {
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, executable, -1, file, 32768)) {
            remove_config(directory); goto failed;
        }
    } else if (!extract_broker(directory, file)) { remove_config(directory); goto failed; }
    Broker broker = {file, directory};
    runner_github_authenticate(auth, organisation, execute, &broker, get, http_context, callbacks);
    remove_config(directory); return;
failed:
    runner_github_auth_clear(auth); auth->phase = RUNNER_AUTH_FAILED;
    (void)snprintf(auth->error, sizeof(auth->error), "%s", "Unable to prepare the supplied GitHub sign-in helper.");
}
void runner_github_sign_in(RunnerGithubAuth *auth, const char *organisation, const RunnerAuthCallbacks *callbacks)
{
    runner_github_sign_in_with_executable(auth, organisation, callbacks, NULL, runner_http_get, NULL);
}
