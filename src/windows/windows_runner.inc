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
        if (g_app.rows_stale) session->state_since_ms = now;
        const bool running = strcmp(row->state, "RUNNING") == 0;
        if (running != session->running) {
            if (session->running && session->busy_started_ms != 0U)
                session->busy_total_ms += now - session->busy_started_ms;
            if (running) {
                session->busy_started_ms = now;
                if (!g_app.rows_stale) session->jobs++;
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
        wchar_t jobs[32] = L"—", busy[32] = L"—", labels[512];
        utf8_to_wide(row->name, name, 192U);
        utf8_to_wide(row->os, os, 64U);
        utf8_to_wide(g_app.rows_stale ? "STALE" : row->state, state, 32U);
        utf8_to_wide(row->labels, labels, 512U);
        if (session && !g_app.rows_stale) {
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

static size_t split_runner_fields(char *line, char **fields, size_t capacity)
{
    size_t count = 0U;
    for (;;) {
        if (count == capacity) return capacity + 1U;
        fields[count++] = line;
        char *tab = strchr(line, '\t');
        if (!tab) return count;
        *tab = '\0';
        line = tab + 1;
    }
}

static bool refresh_http_get(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    RefreshRequest *request = (RefreshRequest *)context;
    if (WaitForSingleObject(request->cancel, 0U) == WAIT_OBJECT_0 ||
        GetTickCount64() >= request->deadline) {
        if (error && error_size)
            (void)snprintf(error, error_size, "Runner refresh was cancelled or exceeded its 60-second deadline.");
        return false;
    }
    /* Native Windows HTTP accepts an optional absolute monotonic deadline. */
    return runner_http_get(url, token, response, error, error_size, &request->deadline);
}

static DWORD WINAPI refresh_worker(LPVOID parameter)
{
    RefreshRequest *request = (RefreshRequest *)parameter;
    HWND target = request->target;
    RefreshResult *result = (RefreshResult *)calloc(1U, sizeof(*result));
    if (!result) {
        CloseHandle(request->cancel);
        SecureZeroMemory(request, sizeof(*request));
        free(request);
        (void)PostMessageW(target, WM_APP_REFRESH_DONE, 0, 0);
        return 0U;
    }
    result->generation = request->generation;
    char endpoint[256];
    (void)snprintf(endpoint, sizeof(endpoint), "/orgs/%s/actions/runners?per_page=100",
                   request->organisation);
    char *output = runner_github_fetch(endpoint, request->token, RUNNER_GITHUB_RUNNERS,
        refresh_http_get, request, result->error, sizeof(result->error));
    CloseHandle(request->cancel);
    SecureZeroMemory(request, sizeof(*request));
    free(request);
    if (!output) goto done;
    size_t capacity = 32U;
    result->rows = (RunnerRow *)calloc(capacity, sizeof(*result->rows));
    if (!result->rows) {
        infiltratr_copy_string(result->error, sizeof(result->error), "Unable to allocate the GitHub runner snapshot.");
        free(output);
        goto done;
    }
    char *context = NULL;
    for (char *line = strtok_s(output, "\r\n", &context); line;
         line = strtok_s(NULL, "\r\n", &context)) {
        if (!*line) continue;
        char *fields[5];
        const size_t count = split_runner_fields(line, fields, 5U);
        if (count < 4U || count > 5U || !*fields[0] ||
            (strcmp(fields[2], "online") != 0 && strcmp(fields[2], "offline") != 0) ||
            (strcmp(fields[3], "true") != 0 && strcmp(fields[3], "false") != 0)) {
            infiltratr_copy_string(result->error, sizeof(result->error), "GitHub returned an invalid runner snapshot.");
            break;
        }
        if (result->count == capacity) {
            const size_t next = capacity * 2U;
            RunnerRow *grown = (RunnerRow *)realloc(result->rows, next * sizeof(*grown));
            if (!grown) {
                infiltratr_copy_string(result->error, sizeof(result->error), "Unable to allocate the complete runner snapshot.");
                break;
            }
            ZeroMemory(grown + capacity, (next - capacity) * sizeof(*grown));
            result->rows = grown;
            capacity = next;
        }
        RunnerRow *row = &result->rows[result->count++];
        infiltratr_copy_string(row->name, sizeof(row->name), fields[0]);
        infiltratr_copy_string(row->os, sizeof(row->os), fields[1]);
        const bool online = strcmp(fields[2], "online") == 0;
        const bool busy = strcmp(fields[3], "true") == 0;
        infiltratr_copy_string(row->state, sizeof(row->state),
                               !online ? "OFFLINE" : busy ? "RUNNING" : "IDLE");
        if (count == 5U) infiltratr_copy_string(row->labels, sizeof(row->labels), fields[4]);
    }
    free(output);
done:
    if (!PostMessageW(target, WM_APP_REFRESH_DONE, 0, (LPARAM)result)) {
        free(result->rows);
        free(result);
    }
    return 0U;
}

static void mark_rows_stale(void)
{
    if (!g_app.rows_stale) {
        const ULONGLONG now = GetTickCount64();
        for (size_t i = 0U; i < g_app.session_count; ++i) {
            RunnerSession *session = &g_app.sessions[i];
            if (session->running && session->busy_started_ms)
                session->busy_total_ms += now - session->busy_started_ms;
            session->running = false;
            session->busy_started_ms = 0U;
        }
    }
    g_app.rows_stale = true;
    rebuild_table();
}

static void refresh_begin(void)
{
    if (g_app.refreshing || g_app.settings_dirty || g_app.shutting_down || !g_app.window) return;
    if (!g_app.config.organisation[0] || !g_app.config.github_token[0]) {
        infiltratr_copy_string(g_app.provider_error, sizeof(g_app.provider_error),
                               "Open Settings and save the GitHub organisation and token to connect.");
        mark_rows_stale();
        PostMessageW(g_app.window, WM_APP_OPEN_SETTINGS, 0, 0);
        return;
    }
    RefreshRequest *request = (RefreshRequest *)calloc(1U, sizeof(*request));
    if (!request) {
        mark_rows_stale();
        infiltratr_copy_string(g_app.provider_error, sizeof(g_app.provider_error), "Unable to start the runner refresh.");
        return;
    }
    request->target = g_app.window;
    request->generation = g_app.config_generation;
    request->deadline = GetTickCount64() + 60000U;
    g_app.refresh_cancel = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_app.refresh_cancel || !DuplicateHandle(GetCurrentProcess(), g_app.refresh_cancel,
        GetCurrentProcess(), &request->cancel, 0U, FALSE, DUPLICATE_SAME_ACCESS)) {
        if (g_app.refresh_cancel) CloseHandle(g_app.refresh_cancel);
        g_app.refresh_cancel = NULL;
        free(request);
        mark_rows_stale();
        infiltratr_copy_string(g_app.provider_error, sizeof(g_app.provider_error), "Unable to start the runner refresh.");
        return;
    }
    infiltratr_copy_string(request->organisation, sizeof(request->organisation), g_app.config.organisation);
    infiltratr_copy_string(request->token, sizeof(request->token), g_app.config.github_token);
    g_app.refreshing = true;
    HANDLE thread = CreateThread(NULL, 0U, refresh_worker, request, 0U, NULL);
    if (thread) CloseHandle(thread);
    else {
        CloseHandle(request->cancel);
        CloseHandle(g_app.refresh_cancel);
        g_app.refresh_cancel = NULL;
        SecureZeroMemory(request, sizeof(*request));
        free(request);
        g_app.refreshing = false;
        mark_rows_stale();
        infiltratr_copy_string(g_app.provider_error, sizeof(g_app.provider_error), "Unable to start the runner refresh.");
        settings_status(g_app.provider_error);
    }
    InvalidateRect(g_app.window, NULL, FALSE);
}

static void apply_refresh(RefreshResult *result)
{
    g_app.refreshing = false;
    if (g_app.refresh_cancel) { CloseHandle(g_app.refresh_cancel); g_app.refresh_cancel = NULL; }
    if (result && result->generation != g_app.config_generation) {
        free(result->rows);
        free(result);
        if (!g_app.settings_dirty && g_app.config.github_token[0]) refresh_begin();
        return;
    }
    if (!result || result->error[0]) {
        mark_rows_stale();
        infiltratr_copy_string(g_app.provider_error, sizeof(g_app.provider_error),
            result ? result->error : "Unable to load the runner snapshot.");
        settings_status(g_app.provider_error);
    } else {
        free(g_app.rows);
        g_app.rows = result->rows;
        g_app.row_count = result->count;
        result->rows = NULL;
        update_sessions();
        g_app.rows_stale = false;
        g_app.provider_error[0] = '\0';
        settings_status("Connected. Access to this organisation's runners was verified.");
        if (g_app.selected_row >= (int)g_app.row_count) g_app.selected_row = -1;
        rebuild_table();
    }
    if (result) { free(result->rows); free(result); }
    InvalidateRect(g_app.window, NULL, FALSE);
}
