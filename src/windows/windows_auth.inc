// SPDX-License-Identifier: GPL-3.0-or-later
typedef struct {
    HWND target; ULONGLONG generation; HANDLE cancel;
    char organisation[128]; RunnerGithubAuth auth;
} AuthResult;
typedef struct { ULONGLONG generation; char code[32]; } AuthCode;
static void auth_result_destroy(AuthResult *result)
{
    if (!result) return;
    if (result->cancel) CloseHandle(result->cancel);
    runner_github_auth_clear(&result->auth); free(result);
}
static bool auth_cancelled(void *context)
{
    return WaitForSingleObject(((AuthResult *)context)->cancel, 0U) == WAIT_OBJECT_0;
}
static void auth_challenge(const char *code, void *context)
{
    AuthResult *result = context;
    AuthCode *message = calloc(1U, sizeof(*message));
    if (!message) return;
    message->generation = result->generation;
    infiltratr_copy_string(message->code, sizeof(message->code), code);
    if (!PostMessageW(result->target, WM_APP_AUTH_CODE, 0, (LPARAM)message)) free(message);
}
static DWORD WINAPI auth_worker(LPVOID data)
{
    AuthResult *result = data;
    const RunnerAuthCallbacks callbacks = {auth_cancelled, auth_challenge, result};
    runner_github_sign_in(&result->auth, result->organisation, &callbacks);
    if (!PostMessageW(result->target, WM_APP_AUTH_DONE, 0, (LPARAM)result)) auth_result_destroy(result);
    return 0U;
}
static void auth_cancel(void)
{
    g_app.auth_active = false; g_app.auth_generation++;
    if (g_app.auth_cancel) { SetEvent(g_app.auth_cancel); CloseHandle(g_app.auth_cancel); g_app.auth_cancel = NULL; }
    runner_github_auth_clear(&g_app.auth);
    if (g_app.settings_code) SetWindowTextW(g_app.settings_code, L"");
}
static void auth_copy_code(void)
{
    if (!g_app.auth_active || !g_app.auth.user_code[0]) return;
    wchar_t code[32]; utf8_to_wide(g_app.auth.user_code, code, 32U);
    const SIZE_T bytes = (wcslen(code) + 1U) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) return;
    void *text = GlobalLock(memory);
    if (!text) { GlobalFree(memory); return; }
    memcpy(text, code, bytes); GlobalUnlock(memory);
    if (OpenClipboard(g_app.settings_window)) {
        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, memory)) memory = NULL;
        CloseClipboard();
    }
    if (memory) GlobalFree(memory);
}
static void auth_open_browser(void)
{
    if (!g_app.auth_active || !g_app.auth.user_code[0]) return;
    if ((INT_PTR)ShellExecuteW(g_app.settings_window, L"open",
        L"https://github.com/login/device", NULL, NULL, SW_SHOWNORMAL) <= 32)
        settings_status("Open github.com/login/device. The sign-in code is copied.");
}
static void auth_code_apply(AuthCode *message)
{
    if (!message) return;
    if (g_app.auth_active && !g_app.shutting_down && g_app.settings_window &&
        message->generation == g_app.auth_generation) {
        g_app.auth.phase = RUNNER_AUTH_WAITING;
        infiltratr_copy_string(g_app.auth.user_code, sizeof(g_app.auth.user_code), message->code);
        wchar_t code[32]; utf8_to_wide(message->code, code, 32U);
        SetWindowTextW(g_app.settings_code, code);
        auth_copy_code();
        settings_status("Paste the copied code into GitHub and approve sign-in.");
        auth_open_browser();
    }
    free(message);
}
static void auth_start(void)
{
    if (g_app.auth_thread) return;
    auth_cancel();
    AuthResult *result = calloc(1U, sizeof(*result));
    if (!result) { settings_status("Unable to start GitHub sign-in."); return; }
    wchar_t organisation[128]; GetWindowTextW(g_app.settings_org, organisation, 128);
    if (!wide_to_utf8(organisation, result->organisation, sizeof(result->organisation))) {
        auth_result_destroy(result); settings_status("Choose a valid organisation."); return;
    }
    g_app.auth_cancel = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_app.auth_cancel || !DuplicateHandle(GetCurrentProcess(), g_app.auth_cancel,
        GetCurrentProcess(), &result->cancel, 0U, FALSE, DUPLICATE_SAME_ACCESS)) {
        auth_result_destroy(result); auth_cancel(); settings_status("Unable to start GitHub sign-in."); return;
    }
    result->target = g_app.window; result->generation = g_app.auth_generation;
    g_app.auth_active = true;
    settings_status("Connecting to GitHub…");
    g_app.auth_thread = CreateThread(NULL, 0U, auth_worker, result, 0U, NULL);
    if (!g_app.auth_thread) {
        auth_result_destroy(result); auth_cancel(); settings_status("Unable to start GitHub sign-in.");
    }
}
static void auth_apply(AuthResult *result)
{
    if (g_app.auth_thread) { CloseHandle(g_app.auth_thread); g_app.auth_thread = NULL; }
    if (!result) return;
    if (!g_app.auth_active || g_app.shutting_down || !g_app.settings_window ||
        result->generation != g_app.auth_generation) { auth_result_destroy(result); return; }
    g_app.auth_active = false;
    if (result->auth.phase == RUNNER_AUTH_COMPLETE) {
        RunnerConfig candidate = g_app.config;
        infiltratr_copy_string(candidate.github_token, sizeof(candidate.github_token), result->auth.token);
        infiltratr_copy_string(candidate.organisation, sizeof(candidate.organisation), result->auth.organisation);
        for (size_t i = 0U; i < result->auth.organisation_count; ++i) {
            wchar_t org[128]; utf8_to_wide(result->auth.organisations[i], org, 128U);
            SendMessageW(g_app.settings_org, CB_ADDSTRING, 0U, (LPARAM)org);
        }
        wchar_t org[128]; utf8_to_wide(candidate.organisation, org, 128U);
        SetWindowTextW(g_app.settings_org, org);
        char error[640] = "";
        if (runner_windows_config_save(&candidate, error, sizeof(error))) {
            SecureZeroMemory(g_app.config.github_token, sizeof(g_app.config.github_token));
            g_app.config = candidate; g_app.config_generation++;
            g_app.settings_dirty = false; g_app.config_error[0] = '\0';
            auth_cancel(); mark_rows_stale();
            if (g_app.config.organisation[0]) { DestroyWindow(g_app.settings_window); refresh_begin(); }
            else settings_status("Choose the organisation to monitor.");
        } else { auth_cancel(); settings_status(error); }
        SecureZeroMemory(candidate.github_token, sizeof(candidate.github_token));
    } else {
        char error[320]; infiltratr_copy_string(error, sizeof(error), result->auth.error);
        auth_cancel(); settings_status(error);
    }
    auth_result_destroy(result);
}
