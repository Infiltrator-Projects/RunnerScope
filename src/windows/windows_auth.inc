// Private Win32 GitHub authorization slice; included by windows_main.c.
typedef struct {
    HWND target;
    HANDLE cancel;
    GitHubAuthFailure reason;
    char output[8192];
    size_t used;
    char code[32];
} GitHubAuthRequest;

typedef struct { char code[32]; } GitHubAuthProgress;
typedef struct {
    bool ok;
    GitHubAuthFailure auth_failure;
    char error[640];
} GitHubAuthResult;

static bool github_contains(const char *message, const char *value)
{
    if (!message || !value) return false;
    const size_t count = strlen(value);
    for (const char *cursor = message; *cursor; ++cursor) {
        size_t i = 0U;
        for (; i < count && cursor[i]; ++i) {
            const unsigned char a = (unsigned char)cursor[i];
            const unsigned char b = (unsigned char)value[i];
            if ((a >= 'A' && a <= 'Z' ? a + ('a' - 'A') : a) !=
                (b >= 'A' && b <= 'Z' ? b + ('a' - 'A') : b)) break;
        }
        if (i == count) return true;
    }
    return false;
}

static GitHubAuthFailure github_auth_failure(const char *message)
{
    if (github_contains(message, "gh auth login") ||
        github_contains(message, "not logged") ||
        github_contains(message, "HTTP 401") ||
        github_contains(message, "Bad credentials")) return GITHUB_AUTH_LOGIN;
    if ((github_contains(message, "admin:org") &&
         (github_contains(message, "scope") || github_contains(message, "permission"))) ||
        github_contains(message, "insufficient scope")) return GITHUB_AUTH_SCOPE;
    if (github_contains(message, "HTTP 403")) return GITHUB_AUTH_ACCESS;
    return GITHUB_AUTH_NONE;
}

static bool github_managed_token(void)
{
    wchar_t value[2];
    return GetEnvironmentVariableW(L"GH_TOKEN", value, 2U) > 0U ||
           GetEnvironmentVariableW(L"GITHUB_TOKEN", value, 2U) > 0U;
}

static bool github_auth_cancelled(void *context)
{
    const GitHubAuthRequest *request = (const GitHubAuthRequest *)context;
    return WaitForSingleObject(request->cancel, 0U) == WAIT_OBJECT_0;
}

static bool github_parse_device_code(const char *output, char *parsed, size_t capacity)
{
    if (!output || !parsed || capacity < 10U) return false;
    parsed[0] = '\0';
    const char *code = strstr(output, "one-time code:");
    if (!code) return false;
    code += strlen("one-time code:");
    while (*code == ' ' || *code == '\t') code++;
    size_t used = 0U;
    while (((*code >= 'A' && *code <= 'Z') || (*code >= '0' && *code <= '9') ||
            *code == '-') && used < capacity - 1U)
        parsed[used++] = *code++;
    parsed[used] = '\0';
    /* Do not publish a code until its terminating newline has arrived. */
    return used >= 4U && (*code == '\r' || *code == '\n' || *code == ' ');
}

static void github_auth_progress(const char *data, size_t amount, void *context)
{
    GitHubAuthRequest *request = (GitHubAuthRequest *)context;
    if (amount > sizeof(request->output) - request->used - 1U)
        amount = sizeof(request->output) - request->used - 1U;
    memcpy(request->output + request->used, data, amount);
    request->used += amount;
    request->output[request->used] = '\0';
    /* gh emits this prompt to redirected stderr before waiting for the browser.
     * Keep the one-time code in memory only, never in config or log files. */
    char parsed[32] = "";
    if (!github_parse_device_code(request->output, parsed, sizeof(parsed))) return;
    if (strcmp(parsed, request->code) == 0) return;
    infiltratr_copy_string(request->code, sizeof(request->code), parsed);
    GitHubAuthProgress *progress = (GitHubAuthProgress *)calloc(1U, sizeof(*progress));
    if (!progress) return;
    infiltratr_copy_string(progress->code, sizeof(progress->code), parsed);
    if (!PostMessageW(request->target, WM_APP_AUTH_PROGRESS, 0, (LPARAM)progress))
        free(progress);
}

static DWORD WINAPI github_auth_worker(LPVOID parameter)
{
    GitHubAuthRequest *request = (GitHubAuthRequest *)parameter;
    GitHubAuthResult *result = (GitHubAuthResult *)calloc(1U, sizeof(*result));
    if (!result) {
        HWND target = request->target;
        free(request);
        (void)PostMessageW(target, WM_APP_AUTH_DONE, 0, 0);
        return 0U;
    }
    RunnerWindowsProcessResult captured;
    char error[320] = "";
    const ULONGLONG started = GetTickCount64();
    bool refresh_scope = request->reason == GITHUB_AUTH_SCOPE;
    if (request->reason != GITHUB_AUTH_SCOPE && request->reason != GITHUB_AUTH_LOGIN) {
        const bool checked = runner_windows_process_capture_stream(
            L"gh api --hostname github.com /user --jq .login", GH_TIMEOUT_MS, NULL,
            github_auth_cancelled, request, &captured, error, sizeof(error));
        if (checked && captured.exit_code == 0U) refresh_scope = true;
        else if (!checked || github_auth_failure(captured.output) != GITHUB_AUTH_LOGIN) {
            infiltratr_copy_string(result->error, sizeof(result->error),
                captured.cancelled ? "GitHub authorization was cancelled. Select Connect GitHub to try again."
                : captured.timed_out ? "Checking GitHub sign-in timed out. Select Connect GitHub to try again."
                : !checked ? error
                : "Unable to check the existing GitHub sign-in. Select Connect GitHub to try again.");
            runner_windows_process_result_destroy(&captured);
            HWND target = request->target;
            free(request);
            if (!PostMessageW(target, WM_APP_AUTH_DONE, 0, (LPARAM)result)) free(result);
            return 0U;
        }
        runner_windows_process_result_destroy(&captured);
    }
    const wchar_t *command = refresh_scope
        ? L"gh auth refresh --hostname github.com --scopes admin:org"
        : L"gh auth login --hostname github.com --web --scopes admin:org";
    const ULONGLONG elapsed = GetTickCount64() - started;
    const unsigned long remaining = elapsed < GH_AUTH_TIMEOUT_MS
        ? (unsigned long)(GH_AUTH_TIMEOUT_MS - elapsed) : 1U;
    const bool launched = runner_windows_process_capture_stream(command,
        remaining, github_auth_progress, github_auth_cancelled, request,
        &captured, error, sizeof(error));
    result->ok = launched && captured.exit_code == 0U;
    result->auth_failure = github_auth_failure(captured.output);
    if (!result->ok)
        infiltratr_copy_string(result->error, sizeof(result->error),
            captured.cancelled ? "GitHub authorization was cancelled. Select Connect GitHub to try again."
            : captured.timed_out ? "GitHub authorization expired. Select Connect GitHub to get a new code."
            : !launched ? error
            : "GitHub authorization did not complete. Select Connect GitHub to try again.");
    runner_windows_process_result_destroy(&captured);
    HWND target = request->target;
    free(request);
    if (!PostMessageW(target, WM_APP_AUTH_DONE, 0, (LPARAM)result)) free(result);
    return 0U;
}

static void github_auth_update_code(const char *code)
{
    if (code != g_app.auth_code)
        infiltratr_copy_string(g_app.auth_code, sizeof(g_app.auth_code), code);
    if (!g_app.auth_window) return;
    wchar_t wide[32];
    utf8_to_wide(code, wide, 32U);
    SetWindowTextW(g_app.auth_code_control, wide);
    SetWindowTextW(g_app.auth_status,
        L"Copy this one-time code, open GitHub, and approve the requested organisation permissions. Runner Monitor will check runner access when you finish.");
    EnableWindow(g_app.auth_open_button, TRUE);
    EnableWindow(g_app.auth_copy_button, TRUE);
}

static void github_auth_done(GitHubAuthResult *result)
{
    g_app.authorizing = false;
    if (g_app.auth_thread) { CloseHandle(g_app.auth_thread); g_app.auth_thread = NULL; }
    if (g_app.auth_cancel) { CloseHandle(g_app.auth_cancel); g_app.auth_cancel = NULL; }
    g_app.auth_code[0] = '\0';
    if (g_app.auth_window) {
        SetWindowTextW(g_app.auth_code_control, L"");
        EnableWindow(g_app.auth_open_button, FALSE);
        EnableWindow(g_app.auth_copy_button, FALSE);
        SetWindowTextW(g_app.auth_cancel_button, L"Close");
    }
    if (result && result->ok) {
        infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message),
                               "Authorization complete. Checking organisation runner access…");
        if (g_app.auth_window)
            SetWindowTextW(g_app.auth_status,
                L"Authorization complete. Checking organisation runner access…");
        refresh_begin();
    } else {
        if (result && result->auth_failure == GITHUB_AUTH_LOGIN)
            g_app.auth_failure = GITHUB_AUTH_LOGIN;
        const char *message = result ? result->error : "Unable to complete GitHub authorization. Try again.";
        infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message), message);
        if (g_app.auth_window) {
            wchar_t wide[640];
            utf8_to_wide(message, wide, 640U);
            SetWindowTextW(g_app.auth_status, wide);
        }
    }
    free(result);
    InvalidateRect(g_app.window, NULL, FALSE);
}

static void github_auth_copy_code(HWND window)
{
    if (!g_app.auth_code[0] || !OpenClipboard(window)) return;
    wchar_t wide[32];
    utf8_to_wide(g_app.auth_code, wide, 32U);
    const SIZE_T bytes = (wcslen(wide) + 1U) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void *destination = GlobalLock(memory);
        if (destination) {
            memcpy(destination, wide, bytes);
            GlobalUnlock(memory);
            if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory))
                GlobalFree(memory);
        } else GlobalFree(memory);
    }
    CloseClipboard();
}

static LRESULT CALLBACK github_auth_proc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        g_app.auth_window = window;
        RECT owner;
        GetWindowRect(g_app.window, &owner);
        const int width = sx(570), height = sx(360);
        SetWindowPos(window, HWND_TOP,
            owner.left + (owner.right - owner.left - width) / 2,
            owner.top + (owner.bottom - owner.top - height) / 2,
            width, height, SWP_SHOWWINDOW);
        HWND title = CreateWindowW(L"STATIC", L"Connect GitHub",
            WS_CHILD | WS_VISIBLE, sx(20), sx(16), sx(510), sx(32),
            window, NULL, g_app.instance, NULL);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_app.font_title, TRUE);
        wchar_t status[640];
        utf8_to_wide(g_app.auth_message, status, 640U);
        g_app.auth_status = CreateWindowW(L"STATIC", status,
            WS_CHILD | WS_VISIBLE, sx(20), sx(58), sx(515), sx(110),
            window, NULL, g_app.instance, NULL);
        SendMessageW(g_app.auth_status, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        HWND label = CreateWindowW(L"STATIC", L"One-time GitHub code",
            WS_CHILD | WS_VISIBLE, sx(20), sx(177), sx(240), sx(24),
            window, NULL, g_app.instance, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        g_app.auth_code_control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL,
            sx(20), sx(204), sx(250), sx(34), window, NULL, g_app.instance, NULL);
        SendMessageW(g_app.auth_code_control, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        HWND url = CreateWindowW(L"STATIC", L"https://github.com/login/device",
            WS_CHILD | WS_VISIBLE, sx(20), sx(246), sx(515), sx(24),
            window, NULL, g_app.instance, NULL);
        SendMessageW(url, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        g_app.auth_open_button = CreateWindowW(L"BUTTON", L"Open &GitHub",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, sx(20), sx(284), sx(150), sx(32),
            window, (HMENU)(INT_PTR)ID_AUTH_OPEN, g_app.instance, NULL);
        g_app.auth_copy_button = CreateWindowW(L"BUTTON", L"&Copy code",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, sx(180), sx(284), sx(150), sx(32),
            window, (HMENU)(INT_PTR)ID_AUTH_COPY, g_app.instance, NULL);
        g_app.auth_cancel_button = CreateWindowW(L"BUTTON", g_app.authorizing ? L"&Cancel" : L"&Close",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, sx(385), sx(284), sx(150), sx(32),
            window, (HMENU)(INT_PTR)ID_AUTH_CANCEL, g_app.instance, NULL);
        SendMessageW(g_app.auth_open_button, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        SendMessageW(g_app.auth_copy_button, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        SendMessageW(g_app.auth_cancel_button, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        EnableWindow(g_app.auth_open_button, FALSE);
        EnableWindow(g_app.auth_copy_button, FALSE);
        if (g_app.auth_code[0]) github_auth_update_code(g_app.auth_code);
        SetFocus(g_app.auth_cancel_button);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_AUTH_COPY) { github_auth_copy_code(window); return 0; }
        if (LOWORD(wparam) == ID_AUTH_OPEN && g_app.auth_code[0]) {
            /* Use a fixed trusted URL, never a URL supplied by provider output. */
            const INT_PTR opened = (INT_PTR)ShellExecuteW(window, L"open",
                L"https://github.com/login/device", NULL, NULL, SW_SHOWNORMAL);
            if (opened <= 32)
                MessageBoxW(window, L"Open https://github.com/login/device in your browser and enter the code shown here.",
                            APP_TITLE, MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (LOWORD(wparam) == ID_AUTH_CANCEL || LOWORD(wparam) == IDCANCEL) {
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (g_app.auth_cancel) SetEvent(g_app.auth_cancel);
        DestroyWindow(window);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, rgb(g_app.palette.text_rgb));
        return (LRESULT)g_app.background_brush;
    }
    case WM_ERASEBKGND: {
        RECT rect;
        GetClientRect(window, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }
    case WM_DESTROY:
        g_app.auth_window = NULL;
        g_app.auth_status = NULL;
        g_app.auth_code_control = NULL;
        g_app.auth_open_button = NULL;
        g_app.auth_copy_button = NULL;
        g_app.auth_cancel_button = NULL;
        return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void open_github_auth(void)
{
    if (g_app.auth_window) { SetForegroundWindow(g_app.auth_window); return; }
    if (g_app.authorizing) return;
    if (github_managed_token()) {
        infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message),
            "Runner Monitor is using GH_TOKEN or GITHUB_TOKEN supplied by its environment. Browser authorization cannot update that token. Update its organisation runner permissions, or remove the override and restart Runner Monitor to connect through the browser.");
    } else if (g_app.refreshing) {
        infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message),
                               "A GitHub request is in progress. Close this window and select Connect GitHub when it finishes.");
    } else {
        GitHubAuthRequest *request = (GitHubAuthRequest *)calloc(1U, sizeof(*request));
        g_app.auth_cancel = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!request || !g_app.auth_cancel) {
            free(request);
            if (g_app.auth_cancel) CloseHandle(g_app.auth_cancel);
            g_app.auth_cancel = NULL;
            infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message),
                                   "Unable to start GitHub authorization. Try again.");
        } else {
            request->target = g_app.window;
            request->cancel = g_app.auth_cancel;
            request->reason = g_app.auth_failure;
            g_app.authorizing = true;
            g_app.auth_code[0] = '\0';
            infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message),
                                   "Requesting a one-time code from GitHub…");
            g_app.auth_thread = CreateThread(NULL, 0U, github_auth_worker, request, 0U, NULL);
            if (!g_app.auth_thread) {
                free(request);
                CloseHandle(g_app.auth_cancel);
                g_app.auth_cancel = NULL;
                g_app.authorizing = false;
                infiltratr_copy_string(g_app.auth_message, sizeof(g_app.auth_message),
                                       "Unable to start GitHub authorization. Try again.");
            }
        }
    }
    g_app.auth_window = CreateWindowExW(WS_EX_DLGMODALFRAME, AUTH_CLASS,
        L"Connect GitHub - Runner Monitor", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, sx(570), sx(360),
        g_app.window, NULL, g_app.instance, NULL);
    if (!g_app.auth_window && g_app.auth_cancel) SetEvent(g_app.auth_cancel);
    InvalidateRect(g_app.window, NULL, FALSE);
}
