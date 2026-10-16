// SPDX-License-Identifier: GPL-3.0-or-later
// Private native OAuth controller; included by windows_main.c.
typedef struct {
    HWND target;
    ULONGLONG generation;
    RunnerGithubAuth auth;
} AuthResult;

static void auth_result_destroy(AuthResult *result)
{
    if (!result) return;
    runner_github_auth_clear(&result->auth);
    free(result);
}

static DWORD WINAPI auth_worker(LPVOID data)
{
    AuthResult *result = (AuthResult *)data;
    runner_github_auth_step(&result->auth, runner_github_auth_client_id(),
        GetTickCount64(), runner_http_post_oauth, NULL);
    if (!PostMessageW(result->target, WM_APP_AUTH_DONE, 0, (LPARAM)result))
        auth_result_destroy(result);
    return 0U;
}

static void auth_cancel(void)
{
    g_app.auth_active = false;
    g_app.auth_generation++;
    KillTimer(g_app.window, TIMER_AUTH);
    runner_github_auth_clear(&g_app.auth);
    if (g_app.settings_code) SetWindowTextW(g_app.settings_code, L"");
}

static void auth_tick(void)
{
    if (!g_app.auth_active || g_app.auth_thread || !g_app.settings_window) return;
    if (g_app.auth.phase == RUNNER_AUTH_WAITING &&
        GetTickCount64() < g_app.auth.next_poll_ms &&
        GetTickCount64() < g_app.auth.expires_at_ms) return;
    AuthResult *result = (AuthResult *)calloc(1U, sizeof(*result));
    if (!result) {
        auth_cancel();
        settings_status("Unable to start GitHub sign-in. Try again.");
        return;
    }
    result->target = g_app.window;
    result->generation = g_app.auth_generation;
    result->auth = g_app.auth;
    g_app.auth_thread = CreateThread(NULL, 0U, auth_worker, result, 0U, NULL);
    if (!g_app.auth_thread) {
        auth_result_destroy(result);
        auth_cancel();
        settings_status("Unable to start GitHub sign-in. Try again.");
    }
}

static void auth_open_browser(void)
{
    if (!g_app.auth_active || g_app.auth.phase != RUNNER_AUTH_WAITING) return;
    if ((INT_PTR)ShellExecuteW(g_app.settings_window, L"open",
            L"https://github.com/login/device", NULL, NULL, SW_SHOWNORMAL) <= 32)
        settings_status("Open https://github.com/login/device in your browser and enter the displayed code.");
}

static void auth_copy_code(void)
{
    if (!g_app.auth_active || !g_app.auth.user_code[0]) return;
    wchar_t code[32];
    utf8_to_wide(g_app.auth.user_code, code, 32U);
    const SIZE_T bytes = (wcslen(code) + 1U) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) return;
    void *text = GlobalLock(memory);
    if (!text) { GlobalFree(memory); return; }
    memcpy(text, code, bytes);
    GlobalUnlock(memory);
    if (OpenClipboard(g_app.settings_window)) {
        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, memory)) memory = NULL;
        CloseClipboard();
    }
    if (memory) GlobalFree(memory);
}

static void auth_start(void)
{
    if (g_app.auth_thread) {
        settings_status("The previous sign-in request is finishing. Please try again shortly.");
        return;
    }
    save_settings(g_app.settings_window, false);
    if (g_app.settings_dirty || g_app.config_error[0] || !g_app.config.organisation[0]) return;
    auth_cancel();
    g_app.auth_active = true;
    settings_status("Connecting to GitHub for browser sign-in…");
    SetTimer(g_app.window, TIMER_AUTH, 250U, NULL);
    auth_tick();
}

static void auth_apply(AuthResult *result)
{
    if (g_app.auth_thread) {
        CloseHandle(g_app.auth_thread);
        g_app.auth_thread = NULL;
    }
    if (!result) return;
    if (!g_app.auth_active || g_app.shutting_down || !g_app.settings_window ||
        result->generation != g_app.auth_generation) {
        auth_result_destroy(result);
        return;
    }
    const bool first_code = g_app.auth.phase == RUNNER_AUTH_NEW;
    runner_github_auth_clear(&g_app.auth);
    g_app.auth = result->auth;
    auth_result_destroy(result);
    if (g_app.auth.phase == RUNNER_AUTH_WAITING) {
        wchar_t code[32];
        utf8_to_wide(g_app.auth.user_code, code, 32U);
        SetWindowTextW(g_app.settings_code, code);
        settings_status("Enter this code at github.com/login/device and approve Runner Monitor. This window will connect automatically. You can cancel at any time.");
        if (first_code) auth_open_browser();
    } else if (g_app.auth.phase == RUNNER_AUTH_COMPLETE) {
        RunnerConfig candidate = g_app.config;
        infiltratr_copy_string(candidate.github_token, sizeof(candidate.github_token), g_app.auth.token);
        char error[640] = "";
        if (runner_windows_config_save(&candidate, error, sizeof(error))) {
            SecureZeroMemory(g_app.config.github_token, sizeof(g_app.config.github_token));
            g_app.config = candidate;
            g_app.config_generation++;
            auth_cancel();
            settings_status("Signed in. Checking organisation runner access…");
            refresh_begin();
        } else {
            auth_cancel();
            settings_status(error);
        }
        SecureZeroMemory(candidate.github_token, sizeof(candidate.github_token));
    } else {
        char error[320];
        infiltratr_copy_string(error, sizeof(error), g_app.auth.error);
        auth_cancel();
        settings_status(error);
    }
}
