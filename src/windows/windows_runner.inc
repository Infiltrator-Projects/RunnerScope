// Private Win32 implementation slice; included by windows_main.c.
static RunnerSession *session_for(const char *name, bool create)
{
    if (!name || !*name) return NULL;
    for (size_t i = 0U; i < g_app.session_count; ++i)
        if (strcmp(g_app.sessions[i].name, name) == 0) return &g_app.sessions[i];
    if (!create) return NULL;
    if (g_app.session_count == g_app.session_capacity) {
        const size_t next = g_app.session_capacity ? g_app.session_capacity * 2U : 16U;
        RunnerSession *grown = (RunnerSession *)realloc(g_app.sessions,
                                                        next * sizeof(*grown));
        if (!grown) return NULL;
        ZeroMemory(grown + g_app.session_capacity,
                   (next - g_app.session_capacity) * sizeof(*grown));
        g_app.sessions = grown;
        g_app.session_capacity = next;
    }
    RunnerSession *session = &g_app.sessions[g_app.session_count++];
    ZeroMemory(session, sizeof(*session));
    (void)snprintf(session->name, sizeof(session->name), "%s", name);
    session->state_since_ms = GetTickCount64();
    return session;
}

static void update_sessions(void)
{
    const ULONGLONG now = GetTickCount64();
    for (size_t i = 0U; i < g_app.row_count; ++i) {
        RunnerRow *row = &g_app.rows[i];
        RunnerSession *session = session_for(row->name, true);
        if (!session) continue;
        const bool running = strcmp(row->state, "RUNNING") == 0;
        if (running != session->running) {
            if (session->running && session->busy_started_ms != 0U)
                session->busy_total_ms += now - session->busy_started_ms;
            if (running) {
                session->busy_started_ms = now;
                session->jobs++;
            } else {
                session->busy_started_ms = 0U;
            }
            session->running = running;
            session->state_since_ms = now;
        }
    }
}

static double busy_fraction(const RunnerSession *session)
{
    if (!session || g_app.session_started_ms == 0U) return 0.0;
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG elapsed = now - g_app.session_started_ms;
    if (elapsed == 0U) return 0.0;
    ULONGLONG busy = session->busy_total_ms;
    if (session->running && session->busy_started_ms != 0U)
        busy += now - session->busy_started_ms;
    double fraction = (double)busy / (double)elapsed;
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    return fraction;
}

static void format_duration(ULONGLONG ms, wchar_t *output, size_t count)
{
    if (!output || count == 0U) return;
    ULONGLONG seconds = ms / 1000U;
    const ULONGLONG hours = seconds / 3600U;
    const ULONGLONG minutes = (seconds % 3600U) / 60U;
    seconds %= 60U;
    if (hours > 0U)
        _snwprintf_s(output, count, _TRUNCATE, L"%lluh %llum", hours, minutes);
    else if (minutes > 0U)
        _snwprintf_s(output, count, _TRUNCATE, L"%llum %llus", minutes, seconds);
    else
        _snwprintf_s(output, count, _TRUNCATE, L"%llus", seconds);
}

static bool row_matches_filter(const RunnerRow *row)
{
    if (!row || !g_app.filter[0]) return true;
    wchar_t combined[1200];
    wchar_t name[192], os[64], state[32], labels[512];
    utf8_to_wide(row->name, name, 192U);
    utf8_to_wide(row->os, os, 64U);
    utf8_to_wide(row->state, state, 32U);
    utf8_to_wide(row->labels, labels, 512U);
    _snwprintf_s(combined, 1200U, _TRUNCATE, L"%ls %ls %ls %ls", name, os, state, labels);
    return StrStrIW(combined, g_app.filter) != NULL;
}

static size_t visible_count(void)
{
    size_t count = 0U;
    for (size_t i = 0U; i < g_app.row_count; ++i)
        if (row_matches_filter(&g_app.rows[i])) count++;
    return count;
}

static RunnerRow *visible_at(size_t visible_index, size_t *source_index)
{
    size_t current = 0U;
    for (size_t i = 0U; i < g_app.row_count; ++i) {
        if (!row_matches_filter(&g_app.rows[i])) continue;
        if (current++ == visible_index) {
            if (source_index) *source_index = i;
            return &g_app.rows[i];
        }
    }
    return NULL;
}

static void list_add_column(int index, int width, const wchar_t *title)
{
    LVCOLUMNW column;
    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = (LPWSTR)title;
    column.cx = sx(width);
    column.iSubItem = index;
    ListView_InsertColumn(g_app.list, index, &column);
}

static void rebuild_table(void)
{
    if (!g_app.list) return;
    ListView_DeleteAllItems(g_app.list);
    int output_index = 0;
    const ULONGLONG now = GetTickCount64();
    for (size_t i = 0U; i < g_app.row_count; ++i) {
        RunnerRow *row = &g_app.rows[i];
        if (!row_matches_filter(row)) continue;
        RunnerSession *session = session_for(row->name, false);
        wchar_t name[192], os[64], state[32], state_for[64] = L"—";
        wchar_t jobs[32] = L"0", busy[32] = L"0.0%", labels[512];
        utf8_to_wide(row->name, name, 192U);
        utf8_to_wide(row->os, os, 64U);
        utf8_to_wide(row->state, state, 32U);
        utf8_to_wide(row->labels, labels, 512U);
        if (session) {
            format_duration(now - session->state_since_ms, state_for, 64U);
            _snwprintf_s(jobs, 32U, _TRUNCATE, L"%u", session->jobs);
            _snwprintf_s(busy, 32U, _TRUNCATE, L"%.1f%%", busy_fraction(session) * 100.0);
        }
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = output_index++;
        item.pszText = name;
        item.lParam = (LPARAM)i;
        const int inserted = ListView_InsertItem(g_app.list, &item);
        ListView_SetItemText(g_app.list, inserted, 1, os);
        ListView_SetItemText(g_app.list, inserted, 2, state);
        ListView_SetItemText(g_app.list, inserted, 3, L"—");
        ListView_SetItemText(g_app.list, inserted, 4, L"—");
        ListView_SetItemText(g_app.list, inserted, 5,
                             strcmp(row->state, "RUNNING") == 0 ? state_for : L"—");
        ListView_SetItemText(g_app.list, inserted, 6, state_for);
        ListView_SetItemText(g_app.list, inserted, 7, jobs);
        ListView_SetItemText(g_app.list, inserted, 8, busy);
        ListView_SetItemText(g_app.list, inserted, 9, labels);
    }
}

static DWORD WINAPI refresh_worker(LPVOID parameter)
{
    RefreshRequest *request = (RefreshRequest *)parameter;
    RefreshResult *result = (RefreshResult *)calloc(1U, sizeof(*result));
    if (!result) {
        free(request);
        return 0U;
    }

    wchar_t org[128];
    utf8_to_wide(request->organisation, org, 128U);
    wchar_t command[2048];
    _snwprintf_s(command, 2048U, _TRUNCATE,
        L"gh api \"/orgs/%ls/actions/runners?per_page=100\" --paginate "
        L"--jq \".runners[] | [.name, .os, .status, (.busy|tostring), "
        L"([.labels[].name] | join(\\\",\\\"))] | @tsv\"", org);

    RunnerWindowsProcessResult process_result;
    char process_error[320] = "";
    const bool launched = runner_windows_process_capture(
        command, GH_TIMEOUT_MS, &process_result, process_error,
        sizeof(process_error));
    char *output = launched ? process_result.output : NULL;
    if (!launched || process_result.exit_code != 0U) {
        (void)snprintf(result->error, sizeof(result->error),
                       "%s%s%s",
                       process_error[0] ? process_error : "GitHub request failed.",
                       output && *output ? " " : "",
                       output && *output ? output : "");
        runner_windows_process_result_destroy(&process_result);
        HWND target = request->target;
        free(request);
        if (!PostMessageW(target, WM_APP_REFRESH_DONE, 0, (LPARAM)result)) free(result);
        return 0U;
    }

    size_t capacity = 32U;
    result->rows = (RunnerRow *)calloc(capacity, sizeof(*result->rows));
    if (!result->rows) {
        runner_windows_process_result_destroy(&process_result);
        HWND target = request->target;
        free(request);
        free(result);
        (void)PostMessageW(target, WM_APP_REFRESH_DONE, 0, 0);
        return 0U;
    }

    char *context = NULL;
    for (char *line = strtok_s(output, "\r\n", &context);
         line;
         line = strtok_s(NULL, "\r\n", &context)) {
        if (!*line) continue;
        if (result->count == capacity) {
            const size_t next = capacity * 2U;
            RunnerRow *grown = (RunnerRow *)realloc(result->rows,
                                                     next * sizeof(*grown));
            if (!grown) break;
            ZeroMemory(grown + capacity, (next - capacity) * sizeof(*grown));
            result->rows = grown;
            capacity = next;
        }
        char *fields[5] = {0};
        char *field_context = NULL;
        size_t field_count = 0U;
        for (char *field = strtok_s(line, "\t", &field_context);
             field && field_count < 5U;
             field = strtok_s(NULL, "\t", &field_context))
            fields[field_count++] = field;
        if (field_count < 4U) continue;

        RunnerRow *row = &result->rows[result->count++];
        infiltratr_copy_string(row->name, sizeof(row->name), fields[0]);
        infiltratr_copy_string(row->os, sizeof(row->os), fields[1]);
        const bool online = strcmp(fields[2], "online") == 0;
        const bool busy = strcmp(fields[3], "true") == 0;
        infiltratr_copy_string(row->state, sizeof(row->state),
                               !online ? "OFFLINE" : busy ? "RUNNING" : "IDLE");
        if (field_count >= 5U)
            infiltratr_copy_string(row->labels, sizeof(row->labels), fields[4]);
    }
    runner_windows_process_result_destroy(&process_result);
    HWND target = request->target;
    free(request);
    if (!PostMessageW(target, WM_APP_REFRESH_DONE, 0, (LPARAM)result)) {
        free(result->rows);
        free(result);
    }
    return 0U;
}

static void refresh_begin(void)
{
    if (g_app.refreshing || !g_app.window) return;
    if (!g_app.config.organisation[0]) {
        PostMessageW(g_app.window, WM_APP_OPEN_SETTINGS, 0, 0);
        return;
    }
    RefreshRequest *request = (RefreshRequest *)calloc(1U, sizeof(*request));
    if (!request) return;
    request->target = g_app.window;
    infiltratr_copy_string(request->organisation, sizeof(request->organisation),
                           g_app.config.organisation);
    g_app.refreshing = true;
    HANDLE thread = CreateThread(NULL, 0U, refresh_worker, request, 0U, NULL);
    if (thread) CloseHandle(thread);
    else {
        free(request);
        g_app.refreshing = false;
    }
    InvalidateRect(g_app.window, NULL, FALSE);
}

static void apply_refresh(RefreshResult *result)
{
    g_app.refreshing = false;
    if (!result) {
        InvalidateRect(g_app.window, NULL, FALSE);
        return;
    }
    if (result->error[0]) {
        wchar_t message[640];
        utf8_to_wide(result->error, message, 640U);
        MessageBoxW(g_app.window, message, APP_TITLE, MB_OK | MB_ICONWARNING);
    } else {
        free(g_app.rows);
        g_app.rows = result->rows;
        g_app.row_count = result->count;
        result->rows = NULL;
        update_sessions();
        if (g_app.selected_row >= (int)g_app.row_count) g_app.selected_row = -1;
        rebuild_table();
    }
    free(result->rows);
    free(result);
    InvalidateRect(g_app.window, NULL, FALSE);
}
