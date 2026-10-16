// SPDX-License-Identifier: GPL-3.0-or-later
// Native, self-contained GitHub setup. Included by windows_main.c.
static void github_setup_layout(HWND window)
{
    RECT area; GetClientRect(window, &area);
    const int pad = sx(20), width = area.right - 2 * pad;
    HWND guide = GetDlgItem(window, ID_SETUP_GUIDE);
    MoveWindow(guide, pad, sx(55), width, area.bottom - sx(225), TRUE);
    MoveWindow(g_app.setup_status, pad, area.bottom - sx(158), width, sx(40), TRUE);
    MoveWindow(GetDlgItem(window, ID_SETUP_LABEL), pad, area.bottom - sx(112), width, sx(20), TRUE);
    MoveWindow(g_app.setup_client, pad, area.bottom - sx(90), width, sx(30), TRUE);
    MoveWindow(GetDlgItem(window, ID_SETUP_OPEN), pad, area.bottom - sx(44), sx(210), sx(30), TRUE);
    MoveWindow(GetDlgItem(window, ID_SETUP_SAVE), area.right - sx(310), area.bottom - sx(44), sx(180), sx(30), TRUE);
    MoveWindow(GetDlgItem(window, IDCANCEL), area.right - sx(120), area.bottom - sx(44), sx(100), sx(30), TRUE);
}

static void github_setup_status(const char *text)
{
    wchar_t wide[640];
    utf8_to_wide(text, wide, 640U);
    SetWindowTextW(g_app.setup_status, wide);
}

static LRESULT CALLBACK github_setup_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        g_app.setup_window = window;
        HWND title = CreateWindowW(L"STATIC", L"GitHub sign-in setup", WS_CHILD | WS_VISIBLE,
            sx(20), sx(14), sx(600), sx(32), window, NULL, g_app.instance, NULL);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_app.font_title, TRUE);
        char guide[8192]; size_t used = 0U;
        for (const char *p = runner_github_setup_instructions(); *p && used + 2U < sizeof(guide); ++p) {
            if (*p == '\n') guide[used++] = '\r';
            guide[used++] = *p;
        }
        guide[used] = '\0';
        wchar_t wide[8192];
        utf8_to_wide(guide, wide, 8192U);
        HWND instructions = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wide,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)ID_SETUP_GUIDE, g_app.instance, NULL);
        SendMessageW(instructions, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        g_app.setup_status = CreateWindowW(L"STATIC",
            L"Use the public Client ID from the OAuth app page. You can complete every step from this window.",
            WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, window, NULL, g_app.instance, NULL);
        SendMessageW(g_app.setup_status, WM_SETFONT, (WPARAM)g_app.font_small, TRUE);
        HWND label = CreateWindowW(L"STATIC", L"Public &Client ID", WS_CHILD | WS_VISIBLE,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)ID_SETUP_LABEL, g_app.instance, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        utf8_to_wide(runner_github_auth_client_for_config(&g_app.config), wide, 8192U);
        g_app.setup_client = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wide,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)ID_SETUP_CLIENT, g_app.instance, NULL);
        SendMessageW(g_app.setup_client, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        SendMessageW(g_app.setup_client, EM_LIMITTEXT, 127U, 0);
        HWND open = CreateWindowW(L"BUTTON", L"&Open GitHub registration", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)ID_SETUP_OPEN, g_app.instance, NULL);
        HWND save = CreateWindowW(L"BUTTON", L"&Save and sign in", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)ID_SETUP_SAVE, g_app.instance, NULL);
        HWND close = CreateWindowW(L"BUTTON", L"&Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)IDCANCEL, g_app.instance, NULL);
        SendMessageW(open, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        SendMessageW(save, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        SendMessageW(close, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        github_setup_layout(window);
        return 0;
    }
    case WM_SIZE:
        if (g_app.setup_client) github_setup_layout(window);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_SETUP_OPEN) {
            if ((INT_PTR)ShellExecuteW(window, L"open", L"https://github.com/settings/applications/new",
                NULL, NULL, SW_SHOWNORMAL) <= 32)
                github_setup_status("Open https://github.com/settings/applications/new in your browser; all registration fields are listed above.");
            return 0;
        }
        if (LOWORD(wparam) == ID_SETUP_SAVE) {
            wchar_t wide[128]; char error[640] = "";
            GetWindowTextW(g_app.setup_client, wide, 128);
            RunnerConfig candidate = g_app.config;
            bool valid = wide_to_utf8(wide, candidate.github_client_id, sizeof(candidate.github_client_id)) &&
                candidate.github_client_id[0] && runner_config_validate(&candidate, error, sizeof(error));
            if (!valid) {
                github_setup_status(error[0] ? error : "Paste the public Client ID shown after registering the OAuth app.");
                return 0;
            }
            const bool changed = strcmp(candidate.github_client_id,
                runner_github_auth_client_for_config(&g_app.config)) != 0;
            if (changed) SecureZeroMemory(candidate.github_token, sizeof(candidate.github_token));
            if (!runner_windows_config_save(&candidate, error, sizeof(error))) {
                SecureZeroMemory(candidate.github_token, sizeof(candidate.github_token));
                github_setup_status(error);
                return 0;
            }
            auth_cancel();
            SecureZeroMemory(g_app.config.github_token, sizeof(g_app.config.github_token));
            g_app.config = candidate;
            SecureZeroMemory(candidate.github_token, sizeof(candidate.github_token));
            g_app.config_generation++;
            if (g_app.refresh_cancel) SetEvent(g_app.refresh_cancel);
            g_app.config_error[0] = '\0';
            if (changed) mark_rows_stale();
            DestroyWindow(window);
            PostMessageW(g_app.settings_window, WM_COMMAND, MAKEWPARAM(ID_AUTH_START, BN_CLICKED), 0);
            return 0;
        }
        if (LOWORD(wparam) == IDCANCEL) { DestroyWindow(window); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(window); return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wparam, TRANSPARENT);
        SetTextColor((HDC)wparam, rgb(g_app.palette.text_rgb));
        return (LRESULT)g_app.background_brush;
    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)wparam, rgb(g_app.palette.input_rgb));
        SetTextColor((HDC)wparam, rgb(g_app.palette.text_rgb));
        return (LRESULT)g_app.input_brush;
    case WM_ERASEBKGND: {
        RECT area; GetClientRect(window, &area);
        FillRect((HDC)wparam, &area, g_app.background_brush); return 1;
    }
    case WM_DESTROY:
        g_app.setup_window = NULL; g_app.setup_client = NULL; g_app.setup_status = NULL;
        if (g_app.settings_window) {
            EnableWindow(g_app.settings_window, TRUE);
            SetForegroundWindow(g_app.settings_window);
        }
        return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void open_github_setup(void)
{
    if (g_app.setup_window) { SetForegroundWindow(g_app.setup_window); return; }
    auth_cancel();
    RECT area;
    SystemParametersInfoW(SPI_GETWORKAREA, 0U, &area, 0U);
    int width = sx(680), height = sx(650);
    if (width > area.right - area.left - sx(40)) width = area.right - area.left - sx(40);
    if (height > area.bottom - area.top - sx(40)) height = area.bottom - area.top - sx(40);
    EnableWindow(g_app.settings_window, FALSE);
    g_app.setup_window = CreateWindowExW(WS_EX_DLGMODALFRAME, SETUP_CLASS,
        L"GitHub sign-in setup", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        area.left + (area.right - area.left - width) / 2,
        area.top + (area.bottom - area.top - height) / 2, width, height,
        g_app.settings_window, NULL, g_app.instance, NULL);
    if (g_app.setup_window) ShowWindow(g_app.setup_window, SW_SHOWNORMAL);
    else {
        EnableWindow(g_app.settings_window, TRUE);
        settings_status("Unable to open GitHub setup. Try again.");
    }
}
