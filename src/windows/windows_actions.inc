// Private Win32 actions/settings slice; included by windows_main.c.
static void csv_field(FILE *stream, const char *value)
{
    const char *text = value ? value : "";
    const bool quote = strpbrk(text, ",\"\r\n") != NULL;
    if (!quote) { fputs(text, stream); return; }
    fputc('"', stream);
    for (const char *cursor = text; *cursor; ++cursor) {
        if (*cursor == '"') fputc('"', stream);
        fputc(*cursor, stream);
    }
    fputc('"', stream);
}

static void export_csv(void)
{
    wchar_t path[MAX_PATH] = L"runner-monitor.csv";
    OPENFILENAMEW dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_app.window;
    dialog.lpstrFilter = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"csv";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return;
    FILE *stream = NULL;
    if (_wfopen_s(&stream, path, L"wb") != 0 || !stream) return;
    fputs("Runner,OS,State,Repository,Current job,Runtime,State for,Jobs,Busy,Labels\r\n", stream);
    for (size_t i = 0U; i < g_app.row_count; ++i) {
        RunnerRow *row = &g_app.rows[i];
        RunnerSession *session = session_for(row->name, false);
        char duration[64] = "—", jobs[32] = "0", busy[32] = "0.0%";
        if (g_app.rows_stale) {
            strcpy_s(jobs, sizeof(jobs), "—");
            strcpy_s(busy, sizeof(busy), "—");
        }
        if (session && !g_app.rows_stale) {
            wchar_t wide[64];
            format_duration(GetTickCount64() - session->state_since_ms, wide, 64U);
            (void)wide_to_utf8(wide, duration, sizeof(duration));
            (void)snprintf(jobs, sizeof(jobs), "%u", session->jobs);
            (void)snprintf(busy, sizeof(busy), "%.1f%%", busy_fraction(session) * 100.0);
        }
        csv_field(stream, row->name); fputc(',', stream);
        csv_field(stream, row->os); fputc(',', stream);
        csv_field(stream, g_app.rows_stale ? "STALE" : row->state); fputc(',', stream);
        csv_field(stream, ""); fputc(',', stream);
        csv_field(stream, ""); fputc(',', stream);
        csv_field(stream, !g_app.rows_stale && session && session->running ? duration : ""); fputc(',', stream);
        csv_field(stream, duration); fputc(',', stream);
        csv_field(stream, jobs); fputc(',', stream);
        csv_field(stream, busy); fputc(',', stream);
        csv_field(stream, row->labels); fputs("\r\n", stream);
    }
    fclose(stream);
}

static void show_about(void)
{
    wchar_t text[512];
    _snwprintf_s(text, 512U, _TRUNCATE,
                 L"Runner Monitor %hs\r\nInfiltrator OS\r\nCommon %hs\r\n\r\n"
                 L"Native Windows runner fleet monitor with bounded GitHub provider execution.",
                 RUNNERSCOPE_VERSION, INFILTRATR_COMMON_VERSION);
    MessageBoxW(g_app.window, text, L"About Runner Monitor", MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK settings_proc(HWND window, UINT message,
                                      WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    switch (message) {
    case WM_CREATE: {
        RECT owner;
        GetWindowRect(g_app.window, &owner);
        const int width = sx(520), height = sx(300);
        SetWindowPos(window, HWND_TOP,
                     owner.left + ((owner.right - owner.left) - width) / 2,
                     owner.top + ((owner.bottom - owner.top) - height) / 2,
                     width, height, SWP_SHOWWINDOW);
        HWND title = CreateWindowW(L"STATIC", L"Runner Monitor settings",
            WS_CHILD | WS_VISIBLE, sx(20), sx(18), sx(430), sx(30),
            window, NULL, g_app.instance, NULL);
        SendMessageW(title, WM_SETFONT, (WPARAM)g_app.font_title, TRUE);
        HWND label = CreateWindowW(L"STATIC", L"GitHub organisation",
            WS_CHILD | WS_VISIBLE, sx(20), sx(62), sx(180), sx(24),
            window, NULL, g_app.instance, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        wchar_t organisation[128];
        utf8_to_wide(g_app.config.organisation, organisation, 128U);
        g_app.settings_org = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", organisation,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            sx(20), sx(88), sx(460), sx(31), window,
            (HMENU)(INT_PTR)ID_SETTINGS_ORG, g_app.instance, NULL);
        SendMessageW(g_app.settings_org, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        HWND hint = CreateWindowW(L"STATIC",
            L"Connect GitHub saves this organisation and opens browser authorization.",
            WS_CHILD | WS_VISIBLE, sx(20), sx(124), sx(460), sx(22),
            window, NULL, g_app.instance, NULL);
        SendMessageW(hint, WM_SETFONT, (WPARAM)g_app.font_small, TRUE);
        HWND connect = CreateWindowW(L"BUTTON", L"Connect &GitHub…",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            sx(20), sx(162), sx(180), sx(32), window,
            (HMENU)(INT_PTR)ID_CONNECT_GITHUB, g_app.instance, NULL);
        SendMessageW(connect, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        HWND save = CreateWindowW(L"BUTTON", L"&Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            sx(300), sx(224), sx(86), sx(32), window,
            (HMENU)(INT_PTR)ID_SETTINGS_SAVE, g_app.instance, NULL);
        HWND cancel = CreateWindowW(L"BUTTON", L"&Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            sx(394), sx(224), sx(86), sx(32), window,
            (HMENU)(INT_PTR)ID_SETTINGS_CANCEL, g_app.instance, NULL);
        SendMessageW(save, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        SendMessageW(cancel, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_SETTINGS_SAVE || LOWORD(wparam) == ID_CONNECT_GITHUB) {
            wchar_t wide[128];
            GetWindowTextW(g_app.settings_org, wide, 128);
            RunnerConfig next = g_app.config;
            if (!wide_to_utf8(wide, next.organisation, sizeof(next.organisation)) ||
                !next.organisation[0]) {
                MessageBoxW(window, L"Enter a valid GitHub organisation.", APP_TITLE,
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            char error[320];
            if (!runner_config_validate(&next, error, sizeof(error))) {
                wchar_t message[320];
                utf8_to_wide(error, message, 320U);
                MessageBoxW(window, message, APP_TITLE, MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (!runner_windows_config_save(&next, error, sizeof(error))) {
                wchar_t message[320];
                utf8_to_wide(error, message, 320U);
                MessageBoxW(window, message, APP_TITLE, MB_OK | MB_ICONERROR);
                return 0;
            }
            if (strcmp(g_app.config.organisation, next.organisation) != 0)
                mark_rows_stale();
            g_app.config = next;
            g_app.config_error[0] = '\0';
            KillTimer(g_app.window, TIMER_REFRESH);
            SetTimer(g_app.window, TIMER_REFRESH,
                     g_app.config.runner_poll_seconds * 1000U, NULL);
            DestroyWindow(window);
            if (LOWORD(wparam) == ID_CONNECT_GITHUB) open_github_auth();
            else refresh_begin();
            return 0;
        }
        if (LOWORD(wparam) == ID_SETTINGS_CANCEL || LOWORD(wparam) == IDCANCEL) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wparam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, rgb(g_app.palette.text_rgb));
        return (LRESULT)g_app.background_brush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wparam;
        SetBkColor(dc, rgb(g_app.palette.input_rgb));
        SetTextColor(dc, rgb(g_app.palette.text_rgb));
        return (LRESULT)g_app.input_brush;
    }
    case WM_ERASEBKGND: {
        RECT rect;
        GetClientRect(window, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }
    case WM_DESTROY:
        g_app.settings_window = NULL;
        g_app.settings_org = NULL;
        EnableWindow(g_app.window, TRUE);
        SetForegroundWindow(g_app.window);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void open_settings(void)
{
    if (g_app.settings_window) {
        SetForegroundWindow(g_app.settings_window);
        return;
    }
    EnableWindow(g_app.window, FALSE);
    g_app.settings_window = CreateWindowExW(
        WS_EX_DLGMODALFRAME, SETTINGS_CLASS, L"Runner Monitor settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, sx(520), sx(300),
        g_app.window, NULL, g_app.instance, NULL);
    if (!g_app.settings_window) EnableWindow(g_app.window, TRUE);
}

static void init_list(void)
{
    g_app.list = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, g_app.window, (HMENU)(INT_PTR)ID_RUNNERS,
        g_app.instance, NULL);
    ListView_SetExtendedListViewStyle(g_app.list,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    ListView_SetBkColor(g_app.list, rgb(g_app.palette.surface_rgb));
    ListView_SetTextBkColor(g_app.list, rgb(g_app.palette.surface_rgb));
    ListView_SetTextColor(g_app.list, rgb(g_app.palette.text_rgb));
    SendMessageW(g_app.list, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    list_add_column(0, 165, L"Runner");
    list_add_column(1, 55, L"OS");
    list_add_column(2, 70, L"State");
    list_add_column(3, 120, L"Repository");
    list_add_column(4, 180, L"Current job");
    list_add_column(5, 72, L"Runtime");
    list_add_column(6, 72, L"State for");
    list_add_column(7, 42, L"Jobs");
    list_add_column(8, 58, L"Busy");
    list_add_column(9, 160, L"Labels");
}

static void create_search(void)
{
    g_app.search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, sx(250), sx(34), g_app.window,
        (HMENU)(INT_PTR)ID_SEARCH, g_app.instance, NULL);
    SendMessageW(g_app.search, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    SendMessageW(g_app.search, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter current view…");
}

static LRESULT hit_test_non_client(HWND window, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(window, &point);
    RECT client;
    GetClientRect(window, &client);
    const int border = sx(7);
    const bool left = point.x < border;
    const bool right = point.x >= client.right - border;
    const bool top = point.y < border;
    const bool bottom = point.y >= client.bottom - border;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    if (point.y < g_app.header_rect.bottom &&
        !pt_in_rect(&g_app.settings_rect, point) &&
        !pt_in_rect(&g_app.minimize_rect, point) &&
        !pt_in_rect(&g_app.maximize_rect, point) &&
        !pt_in_rect(&g_app.close_rect, point))
        return HTCAPTION;
    return HTCLIENT;
}

static void handle_click(POINT point)
{
    if (pt_in_rect(&g_app.settings_rect, point)) { open_settings(); return; }
    if (pt_in_rect(&g_app.minimize_rect, point)) { ShowWindow(g_app.window, SW_MINIMIZE); return; }
    if (pt_in_rect(&g_app.maximize_rect, point)) {
        ShowWindow(g_app.window, IsZoomed(g_app.window) ? SW_RESTORE : SW_MAXIMIZE); return;
    }
    if (pt_in_rect(&g_app.close_rect, point)) { PostMessageW(g_app.window, WM_CLOSE, 0, 0); return; }
    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i)
        if (pt_in_rect(&g_app.nav_rects[i], point)) { set_page(i); return; }
    if (g_app.page == RUNNER_UI_PAGE_RUNNERS && pt_in_rect(&g_app.cards_toggle_rect, point)) {
        set_table_view(false); return;
    }
    if (g_app.page == RUNNER_UI_PAGE_RUNNERS && pt_in_rect(&g_app.table_toggle_rect, point)) {
        set_table_view(true); return;
    }
    if (pt_in_rect(&g_app.export_rect, point)) { export_csv(); return; }
    if (pt_in_rect(&g_app.about_rect, point)) { show_about(); return; }
    if (pt_in_rect(&g_app.refresh_rect, point)) { refresh_begin(); return; }
    select_card(point);
}
