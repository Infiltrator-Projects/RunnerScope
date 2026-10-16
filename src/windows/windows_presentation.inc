// Private Win32 content presentation slice; included by windows_main.c.
typedef struct { char state_for[64]; char jobs[32]; char busy_pct[32]; } WinItemStorage;

static void prepare_item(const RunnerRow *row, WinItemStorage *storage,
                         RunnerUiRunnerItem *item)
{
    ZeroMemory(storage, sizeof(*storage));
    ZeroMemory(item, sizeof(*item));
    strcpy_s(storage->state_for, sizeof(storage->state_for), "—");
    strcpy_s(storage->jobs, sizeof(storage->jobs), g_app.rows_stale ? "—" : "0");
    strcpy_s(storage->busy_pct, sizeof(storage->busy_pct), g_app.rows_stale ? "—" : "0.0%");
    double fraction = 0.0;
    RunnerSession *session = session_for(row->name, false);
    if (session && !g_app.rows_stale) {
        wchar_t duration[64];
        format_duration(GetTickCount64() - session->state_since_ms, duration, 64U);
        (void)wide_to_utf8(duration, storage->state_for, sizeof(storage->state_for));
        (void)snprintf(storage->jobs, sizeof(storage->jobs), "%u", session->jobs);
        fraction = busy_fraction(session);
        (void)snprintf(storage->busy_pct, sizeof(storage->busy_pct), "%.1f%%", fraction * 100.0);
    }
    item->name = row->name;
    item->os = row->os;
    item->state = g_app.rows_stale ? "STALE" : row->state;
    item->repo = "—";
    item->job = "—";
    item->runtime = strcmp(row->state, "RUNNING") == 0 ? storage->state_for : "—";
    item->state_for = storage->state_for;
    item->jobs = storage->jobs;
    item->busy_pct = storage->busy_pct;
    item->labels = row->labels[0] ? row->labels : "—";
    item->busy_fraction = fraction;
}

static COLORREF tone_color(RunnerUiRunnerTone tone)
{
    if (tone == RUNNER_UI_RUNNER_RUNNING) return rgb(g_app.palette.success_rgb);
    if (tone == RUNNER_UI_RUNNER_IDLE) return rgb(g_app.palette.info_rgb);
    return rgb(g_app.palette.fault_rgb);
}

static void draw_runner_card(HDC dc, const RunnerUiRunnerCardSpec *spec, RECT card)
{
    const RunnerUiRunnerItem *item = spec->item;
    fill_round(dc, card,
               spec->selected ? mix_color(g_app.palette.card_rgb,
                                          g_app.palette.neutral_accent_rgb, 10U)
                              : rgb(g_app.palette.card_rgb),
               spec->selected ? rgb(g_app.palette.neutral_accent_rgb)
                              : rgb(g_app.palette.border_rgb), sx(12));
    RECT stripe = {card.left + sx(1), card.top + sx(1), card.left + sx(6), card.bottom - sx(1)};
    fill_color(dc, stripe, tone_color(spec->tone));
    RECT icon = {card.left + sx(16), card.top + sx(15), card.left + sx(56), card.top + sx(55)};
    draw_image(dc, &g_app.nav_images[0], &icon);
    wchar_t name[192], os[64], state[64], labels[512];
    utf8_to_wide(item->name, name, 192U);
    utf8_to_wide(item->os, os, 64U);
    utf8_to_wide(g_app.rows_stale ? "● Stale" : spec->state_label, state, 64U);
    utf8_to_wide(item->labels, labels, 512U);
    RECT name_rect = {card.left + sx(66), card.top + sx(12), card.right - sx(12), card.top + sx(38)};
    draw_text_value(dc, name, name_rect, g_app.font_bold, rgb(g_app.palette.title_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT os_rect = {card.left + sx(66), card.top + sx(37), card.right - sx(12), card.top + sx(57)};
    draw_text_value(dc, os, os_rect, g_app.font_small, rgb(g_app.palette.muted_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT state_rect = {card.left + sx(16), card.top + sx(67), card.left + sx(116), card.top + sx(93)};
    fill_round(dc, state_rect, rgb(g_app.palette.surface_rgb), tone_color(spec->tone), sx(8));
    draw_text_value(dc, state, state_rect, g_app.font_bold, rgb(g_app.palette.text_rgb),
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    wchar_t duration[64], jobs[64], busy[64], jobs_value[32], busy_value[32];
    utf8_to_wide(item->state_for, duration, 64U);
    utf8_to_wide(item->jobs, jobs_value, 32U);
    utf8_to_wide(item->busy_pct, busy_value, 32U);
    _snwprintf_s(jobs, 64U, _TRUNCATE, L"Jobs %ls", jobs_value);
    _snwprintf_s(busy, 64U, _TRUNCATE, L"Busy %ls", busy_value);
    RECT duration_rect = {card.left + sx(126), card.top + sx(67), card.right - sx(14), card.top + sx(91)};
    draw_text_value(dc, duration, duration_rect, g_app.font_small, rgb(g_app.palette.muted_rgb),
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    RECT labels_rect = {card.left + sx(16), card.top + sx(99), card.right - sx(16), card.top + sx(121)};
    draw_text_value(dc, labels, labels_rect, g_app.font_small, rgb(g_app.palette.detail_label_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT bar = {card.left + sx(16), card.bottom - sx(31), card.right - sx(16), card.bottom - sx(22)};
    fill_round(dc, bar, rgb(g_app.palette.input_rgb), rgb(g_app.palette.border_rgb), sx(5));
    RECT progress = bar;
    progress.right = progress.left + (int)((double)(bar.right - bar.left) * spec->busy_fraction);
    if (progress.right > progress.left)
        fill_round(dc, progress, rgb(g_app.palette.operation_rgb), rgb(g_app.palette.operation_rgb), sx(5));
    RECT jobs_rect = {card.left + sx(16), card.bottom - sx(20), card.left + sx(112), card.bottom - sx(3)};
    RECT busy_rect = {card.right - sx(128), card.bottom - sx(20), card.right - sx(16), card.bottom - sx(3)};
    draw_text_value(dc, jobs, jobs_rect, g_app.font_small, rgb(g_app.palette.muted_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text_value(dc, busy, busy_rect, g_app.font_small, rgb(g_app.palette.muted_rgb),
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

typedef struct { HDC dc; bool draw_cards; } WinCardsContext;
static bool win_begin_cards(void *context, size_t item_count)
{
    (void)item_count;
    WinCardsContext *ctx = context;
    return ctx && ctx->dc;
}
static bool win_card(void *context, size_t index, const RunnerUiRunnerCardSpec *spec)
{
    WinCardsContext *ctx = context;
    if (!ctx || !ctx->dc || !spec) return false;
    if (!ctx->draw_cards) return true;
    RECT card = card_rect(index);
    if (card.bottom < g_app.cards_area_rect.top || card.top > g_app.cards_area_rect.bottom) return true;
    const int saved = SaveDC(ctx->dc);
    IntersectClipRect(ctx->dc, g_app.cards_area_rect.left, g_app.cards_area_rect.top,
                      g_app.cards_area_rect.right, g_app.cards_area_rect.bottom);
    draw_runner_card(ctx->dc, spec, card);
    RestoreDC(ctx->dc, saved);
    return true;
}
static bool win_cards_empty(void *context, const char *message)
{
    WinCardsContext *ctx = context;
    if (!ctx || !ctx->dc || !message) return false;
    if (!ctx->draw_cards) return true;
    fill_round(ctx->dc, g_app.cards_area_rect, rgb(g_app.palette.surface_rgb),
               rgb(g_app.palette.border_rgb), sx(10));
    wchar_t text[256];
    utf8_to_wide(message, text, 256U);
    RECT rect = g_app.cards_area_rect;
    draw_text_value(ctx->dc, text, rect, g_app.font_bold, rgb(g_app.palette.muted_rgb),
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return true;
}
static bool win_selection(void *context, const RunnerUiRunnerSelectionSpec *selection)
{
    WinCardsContext *ctx = context;
    if (!ctx || !ctx->dc || !selection) return false;
    fill_round(ctx->dc, g_app.selection_rect,
               mix_color(g_app.palette.surface_rgb, g_app.palette.neutral_accent_rgb, 8U),
               rgb(g_app.palette.neutral_accent_rgb), sx(10));
    wchar_t title[192], primary[512], secondary[1024];
    utf8_to_wide(selection->title, title, 192U);
    utf8_to_wide(selection->primary, primary, 512U);
    utf8_to_wide(selection->secondary, secondary, 1024U);
    RECT title_rect = {g_app.selection_rect.left + sx(16), g_app.selection_rect.top + sx(8),
                       g_app.selection_rect.right - sx(16), g_app.selection_rect.top + sx(34)};
    draw_text_value(ctx->dc, title, title_rect, g_app.font_bold, rgb(g_app.palette.title_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT primary_rect = {title_rect.left, g_app.selection_rect.top + sx(34), title_rect.right,
                         g_app.selection_rect.top + sx(58)};
    draw_text_value(ctx->dc, primary, primary_rect, g_app.font_small, rgb(g_app.palette.text_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT secondary_rect = {title_rect.left, g_app.selection_rect.top + sx(58), title_rect.right,
                           g_app.selection_rect.bottom - sx(6)};
    draw_text_value(ctx->dc, secondary, secondary_rect, g_app.font_small,
                    rgb(g_app.palette.muted_rgb), DT_LEFT | DT_WORDBREAK);
    return true;
}
static bool win_end_cards(void *context) { return context != NULL; }

static void draw_content(HDC dc)
{
    if (g_app.page != RUNNER_UI_PAGE_RUNNERS) {
        fill_round(dc, g_app.cards_area_rect, rgb(g_app.palette.surface_rgb),
                   rgb(g_app.palette.border_rgb), sx(10));
        const wchar_t *message = g_app.page == RUNNER_UI_PAGE_ACTIVE_JOBS
            ? L"Active Jobs provider parity is not available on Windows yet."
            : g_app.page == RUNNER_UI_PAGE_HISTORY
                ? L"History provider parity is not available on Windows yet."
                : L"Local Windows service health is not available yet.";
        RECT text = g_app.cards_area_rect;
        text.left += sx(30); text.right -= sx(30);
        draw_text_value(dc, message, text, g_app.font_bold, rgb(g_app.palette.muted_rgb),
                        DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }
    const size_t visible = visible_count();
    RunnerUiRunnerItem *items = visible ? calloc(visible, sizeof(*items)) : NULL;
    WinItemStorage *storage = visible ? calloc(visible, sizeof(*storage)) : NULL;
    if (visible && (!items || !storage)) { free(items); free(storage); return; }
    ptrdiff_t selected = -1;
    for (size_t i = 0U; i < visible; ++i) {
        size_t source = 0U;
        RunnerRow *row = visible_at(i, &source);
        if (!row) continue;
        prepare_item(row, &storage[i], &items[i]);
        if ((int)source == g_app.selected_row) selected = (ptrdiff_t)i;
    }
    WinCardsContext ctx = {dc, !g_app.table_view};
    RunnerUiRenderer renderer = {0};
    renderer.context = &ctx;
    renderer.begin_runner_cards = win_begin_cards;
    renderer.runner_card = win_card;
    renderer.runner_cards_empty = win_cards_empty;
    renderer.runner_selection = win_selection;
    renderer.end_runner_cards = win_end_cards;
    RunnerUiRunnerCardsState state = {items, visible, selected, g_app.refreshing, g_app.filter[0] != L'\0'};
    (void)runner_ui_render_runner_cards(&renderer, &state);
    free(storage);
    free(items);
}

static bool win_begin_footer(void *context)
{
    WinChromeContext *ctx = context;
    if (!ctx || !ctx->dc) return false;
    const int footer_top = ctx->client.bottom - sx(96);
    ctx->footer = (RECT){sx(20), footer_top, ctx->client.right - sx(20), ctx->client.bottom - sx(12)};
    fill_round(ctx->dc, ctx->footer, rgb(g_app.palette.panel_rgb),
               rgb(g_app.palette.connection_border_rgb), sx(12));
    return true;
}
static bool win_footer_action(void *context, RunnerUiChromeActionId action,
                              const char *label, const char *tooltip, bool primary)
{
    (void)tooltip;
    WinChromeContext *ctx = context;
    if (!ctx || !ctx->dc || !label) return false;
    RECT rect;
    switch (action) {
    case RUNNER_UI_CHROME_EXPORT: rect = g_app.export_rect; break;
    case RUNNER_UI_CHROME_ABOUT: rect = g_app.about_rect; break;
    case RUNNER_UI_CHROME_REFRESH: rect = g_app.refresh_rect; break;
    default: return false;
    }
    wchar_t text[96];
    utf8_to_wide(label, text, 96U);
    draw_button(ctx->dc, rect, text, false, primary);
    return true;
}
static bool win_footer_status(void *context, const RunnerUiFooterState *state)
{
    WinChromeContext *ctx = context;
    if (!ctx || !ctx->dc || !state) return false;
    wchar_t status[512], version[128];
    utf8_to_wide(state->status_text, status, 512U);
    RECT status_rect = {ctx->footer.left + sx(12), ctx->footer.top + sx(50),
                        ctx->footer.right - sx(340), ctx->footer.bottom - sx(8)};
    draw_text_value(ctx->dc, status, status_rect, g_app.font, rgb(g_app.palette.text_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    utf8_to_wide(state->app_version, version, 128U);
    RECT version_rect = {ctx->footer.right - sx(310), ctx->footer.top + sx(48),
                         ctx->footer.right - sx(12), ctx->footer.top + sx(67)};
    draw_text_value(ctx->dc, version, version_rect, g_app.font_small, rgb(g_app.palette.muted_rgb),
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    utf8_to_wide(state->common_version, version, 128U);
    RECT common_rect = {version_rect.left, ctx->footer.top + sx(66),
                        version_rect.right, ctx->footer.bottom - sx(5)};
    draw_text_value(ctx->dc, version, common_rect, g_app.font_small, rgb(g_app.palette.muted_rgb),
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    return true;
}
static bool win_end_footer(void *context) { return context != NULL; }

static void draw_footer(HDC dc, RECT client)
{
    char status[512];
    if (g_app.config_error[0]) {
        infiltratr_copy_string(status, sizeof(status), g_app.config_error);
    } else if (g_app.refreshing) {
        infiltratr_copy_string(status, sizeof(status), "Refreshing GitHub runner state…");
    } else if (g_app.provider_error[0]) {
        (void)snprintf(status, sizeof(status), "%s%s", g_app.provider_error,
                      g_app.rows_stale ? " Last known runner data is stale." : "");
    } else if (!g_app.config.github_token[0]) {
        infiltratr_copy_string(status, sizeof(status), "Open Settings and select Sign in with GitHub to connect.");
    } else if (!g_app.config.organisation[0]) {
        infiltratr_copy_string(status, sizeof(status), "Open Settings and enter the GitHub organisation.");
    } else {
        (void)snprintf(status, sizeof(status), "%zu runner%s loaded from %s.",
                       g_app.row_count, g_app.row_count == 1U ? "" : "s", g_app.config.organisation);
    }
    char app_version[128], common_version[128];
    (void)snprintf(app_version, sizeof(app_version), "Runner Monitor %s", RUNNERSCOPE_VERSION);
    (void)snprintf(common_version, sizeof(common_version), "Common %s", INFILTRATR_COMMON_VERSION);
    RunnerUiFooterState state = {status, app_version, common_version, g_app.refreshing};
    WinChromeContext ctx = {0};
    ctx.dc = dc; ctx.client = client;
    RunnerUiRenderer renderer = {0};
    renderer.context = &ctx;
    renderer.begin_footer = win_begin_footer;
    renderer.footer_action = win_footer_action;
    renderer.footer_status = win_footer_status;
    renderer.end_footer = win_end_footer;
    (void)runner_ui_render_footer(&renderer, &state);
}

static void paint_ui(HDC dc)
{
    RECT client;
    GetClientRect(g_app.window, &client);
    fill_color(dc, client, rgb(g_app.palette.background_rgb));
    draw_header(dc);
    RECT content = {sx(20), g_app.header_rect.bottom + sx(14),
                    client.right - sx(20), client.bottom - sx(110)};
    draw_navigation(dc, content);
    draw_workspace_header(dc);
    RECT toolbar = {g_app.workspace_rect.left, g_app.workspace_rect.top + sx(108),
                    g_app.workspace_rect.right, g_app.workspace_rect.top + sx(164)};
    fill_color(dc, toolbar, rgb(g_app.palette.card_rgb));
    if (g_app.page == RUNNER_UI_PAGE_RUNNERS) {
        draw_button(dc, g_app.cards_toggle_rect, L"Cards", !g_app.table_view, false);
        draw_button(dc, g_app.table_toggle_rect, L"Table", g_app.table_view, false);
    }
    draw_content(dc);
    draw_footer(dc, client);
}

static void set_page(int page)
{
    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT) return;
    bool running = false;
    for (size_t i = 0U; i < g_app.row_count; ++i)
        if (strcmp(g_app.rows[i].state, "RUNNING") == 0) { running = true; break; }
    RunnerUiPageTransition transition;
    if (!runner_ui_plan_page_transition((RunnerUiPageId)g_app.page, (RunnerUiPageId)page,
                                        running, g_app.table_view, &transition) || !transition.changed)
        return;
    g_app.page = (int)transition.requested_page;
    if (transition.clear_selection) g_app.selected_row = -1;
    g_app.card_scroll_y = 0;
    ShowWindow(g_app.search, transition.show_search ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.list, transition.show_runner_table ? SW_SHOW : SW_HIDE);
    update_layout(g_app.window);
    InvalidateRect(g_app.window, NULL, FALSE);
}
static void set_table_view(bool table)
{
    if (g_app.table_view == table) return;
    g_app.table_view = table;
    ShowWindow(g_app.list, g_app.page == RUNNER_UI_PAGE_RUNNERS && table ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_app.window, NULL, FALSE);
}
static void select_card(POINT point)
{
    if (g_app.page != RUNNER_UI_PAGE_RUNNERS || g_app.table_view) return;
    const size_t count = visible_count();
    for (size_t i = 0U; i < count; ++i) {
        RECT rect = card_rect(i);
        if (!pt_in_rect(&rect, point)) continue;
        size_t source = 0U;
        if (visible_at(i, &source)) {
            g_app.selected_row = (int)source;
            update_layout(g_app.window);
            InvalidateRect(g_app.window, NULL, FALSE);
        }
        return;
    }
}
