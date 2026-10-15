// Private Win32 layout/chrome slice; included by windows_main.c.
static void update_layout(HWND window)
{
    RECT client;
    GetClientRect(window, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int pad = sx(20);
    const int header = sx(60);
    const int footer = sx(96);
    const int nav = sx(195);

    SetRect(&g_app.header_rect, 0, 0, width, header);
    const int control = sx(32);
    int right = width - sx(14);
    SetRect(&g_app.close_rect, right - control, sx(14), right, sx(46));
    right -= control + sx(6);
    SetRect(&g_app.maximize_rect, right - control, sx(14), right, sx(46));
    right -= control + sx(6);
    SetRect(&g_app.minimize_rect, right - control, sx(14), right, sx(46));
    right -= control + sx(10);
    SetRect(&g_app.settings_rect, right - sx(38), sx(11), right, sx(49));

    RECT content = {pad, header + sx(14), width - pad, height - footer - sx(14)};
    SetRect(&g_app.workspace_rect, content.left + nav, content.top,
            content.right, content.bottom);
    int nav_y = content.top + sx(10);
    for (int i = 0; i < RUNNER_UI_PAGE_COUNT; ++i) {
        if (i == RUNNER_UI_PAGE_LOCAL_HEALTH) nav_y += sx(12);
        SetRect(&g_app.nav_rects[i], content.left + sx(8), nav_y,
                content.left + nav - sx(8), nav_y + sx(50));
        nav_y += sx(53);
    }

    const int workspace_left = g_app.workspace_rect.left + sx(18);
    const int workspace_right = g_app.workspace_rect.right - sx(18);
    const int toolbar_top = g_app.workspace_rect.top + sx(118);
    SetRect(&g_app.cards_toggle_rect, workspace_left, toolbar_top + sx(9),
            workspace_left + sx(70), toolbar_top + sx(43));
    SetRect(&g_app.table_toggle_rect, g_app.cards_toggle_rect.right, toolbar_top + sx(9),
            g_app.cards_toggle_rect.right + sx(70), toolbar_top + sx(43));
    const int search_width = sx(250);
    MoveWindow(g_app.search, workspace_right - search_width, toolbar_top + sx(9),
               search_width, sx(34), TRUE);

    const int selection_height = g_app.selected_row >= 0 && g_app.page == 0 ? sx(102) : 0;
    SetRect(&g_app.selection_rect, workspace_left,
            g_app.workspace_rect.bottom - selection_height - sx(8),
            workspace_right, g_app.workspace_rect.bottom - sx(8));
    SetRect(&g_app.cards_area_rect, workspace_left, toolbar_top + sx(56),
            workspace_right,
            selection_height > 0 ? g_app.selection_rect.top - sx(8)
                                 : g_app.workspace_rect.bottom - sx(8));
    MoveWindow(g_app.list, g_app.cards_area_rect.left, g_app.cards_area_rect.top,
               g_app.cards_area_rect.right - g_app.cards_area_rect.left,
               g_app.cards_area_rect.bottom - g_app.cards_area_rect.top, TRUE);

    const int footer_top = height - footer;
    const int button_y = footer_top + sx(10);
    SetRect(&g_app.export_rect, pad, button_y, pad + sx(112), button_y + sx(34));
    SetRect(&g_app.about_rect, g_app.export_rect.right + sx(8), button_y,
            g_app.export_rect.right + sx(98), button_y + sx(34));
    SetRect(&g_app.refresh_rect, width - pad - sx(126), button_y,
            width - pad, button_y + sx(34));
}

static void draw_button(HDC dc, RECT rect, const wchar_t *label, bool active, bool primary)
{
    uint32_t base = active ? g_app.palette.selection_background_rgb
                           : g_app.palette.button_background_rgb;
    if (primary && !active) base = g_app.palette.operation_rgb;
    fill_round(dc, rect, rgb(base),
               rgb(active || primary ? g_app.palette.neutral_accent_rgb
                                     : g_app.palette.border_rgb), sx(8));
    draw_text_value(dc, label, rect, g_app.font_bold,
                    rgb(active || primary ? g_app.palette.selection_foreground_rgb
                                          : g_app.palette.button_foreground_rgb),
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void draw_outline_icon(HDC dc, RECT rect, int kind, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, sx(1), color);
    HGDIOBJ previous = SelectObject(dc, pen);
    const int cx = (rect.left + rect.right) / 2;
    const int cy = (rect.top + rect.bottom) / 2;
    if (kind == 0) {
        MoveToEx(dc, cx - sx(6), cy + sx(4), NULL); LineTo(dc, cx + sx(6), cy + sx(4));
    } else if (kind == 1) {
        Rectangle(dc, cx - sx(6), cy - sx(5), cx + sx(6), cy + sx(5));
    } else if (kind == 2) {
        MoveToEx(dc, cx - sx(5), cy - sx(5), NULL); LineTo(dc, cx + sx(5), cy + sx(5));
        MoveToEx(dc, cx + sx(5), cy - sx(5), NULL); LineTo(dc, cx - sx(5), cy + sx(5));
    } else {
        Ellipse(dc, cx - sx(5), cy - sx(5), cx + sx(5), cy + sx(5));
        MoveToEx(dc, cx, cy - sx(10), NULL); LineTo(dc, cx, cy - sx(6));
        MoveToEx(dc, cx, cy + sx(6), NULL); LineTo(dc, cx, cy + sx(10));
        MoveToEx(dc, cx - sx(10), cy, NULL); LineTo(dc, cx - sx(6), cy);
        MoveToEx(dc, cx + sx(6), cy, NULL); LineTo(dc, cx + sx(10), cy);
    }
    SelectObject(dc, previous);
    DeleteObject(pen);
}

typedef struct { HDC dc; RECT client; RECT footer; } WinChromeContext;

static bool win_begin_header(void *context, const char *product_title,
                             const char *product_family)
{
    WinChromeContext *ctx = context;
    if (!ctx || !ctx->dc || !product_title || !product_family) return false;
    fill_color(ctx->dc, g_app.header_rect, rgb(g_app.palette.titlebar_rgb));
    RECT accent = g_app.header_rect;
    accent.right = sx(210);
    fill_color(ctx->dc, accent,
               mix_color(g_app.palette.titlebar_rgb, g_app.palette.neutral_accent_rgb, 24U));
    RECT icon = {sx(16), sx(10), sx(56), sx(50)};
    draw_image(ctx->dc, &g_app.nav_images[0], &icon);
    wchar_t title_text[96], family_text[96];
    utf8_to_wide(product_title, title_text, 96U);
    utf8_to_wide(product_family, family_text, 96U);
    RECT title = {sx(70), sx(8), sx(370), sx(34)};
    draw_text_value(ctx->dc, title_text, title, g_app.font_brand,
                    rgb(g_app.palette.title_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = {sx(70), sx(32), sx(370), sx(52)};
    draw_text_value(ctx->dc, family_text, subtitle, g_app.font_small,
                    rgb(g_app.palette.muted_rgb), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    return true;
}

static bool win_header_action(void *context, RunnerUiChromeActionId action,
                              const char *label, const char *tooltip, bool primary)
{
    (void)label; (void)tooltip; (void)primary;
    WinChromeContext *ctx = context;
    if (!ctx || !ctx->dc) return false;
    RECT rect;
    int icon = 0;
    switch (action) {
    case RUNNER_UI_CHROME_SETTINGS: rect = g_app.settings_rect; icon = 3; break;
    case RUNNER_UI_CHROME_MINIMIZE: rect = g_app.minimize_rect; icon = 0; break;
    case RUNNER_UI_CHROME_MAXIMIZE: rect = g_app.maximize_rect; icon = 1; break;
    case RUNNER_UI_CHROME_CLOSE: rect = g_app.close_rect; icon = 2; break;
    default: return false;
    }
    fill_round(ctx->dc, rect, rgb(g_app.palette.surface_rgb),
               rgb(g_app.palette.connection_border_rgb), sx(6));
    draw_outline_icon(ctx->dc, rect, icon, rgb(g_app.palette.button_foreground_rgb));
    return true;
}

static bool win_end_header(void *context) { return context != NULL; }

static void draw_header(HDC dc)
{
    WinChromeContext ctx = {0};
    ctx.dc = dc;
    RunnerUiRenderer renderer = {0};
    renderer.context = &ctx;
    renderer.begin_header = win_begin_header;
    renderer.header_action = win_header_action;
    renderer.end_header = win_end_header;
    (void)runner_ui_render_header(&renderer);
}

typedef struct { HDC dc; } WinNavigationContext;
static bool win_begin_navigation(void *context, size_t item_count, RunnerUiPageId selected_page)
{
    (void)item_count; (void)selected_page;
    WinNavigationContext *ctx = context;
    return ctx && ctx->dc;
}
static bool win_navigation_separator(void *context, const RunnerUiPageSpec *before_page)
{
    WinNavigationContext *ctx = context;
    if (!ctx || !ctx->dc || !before_page) return false;
    const int page = (int)before_page->id;
    if (page < 0 || page >= RUNNER_UI_PAGE_COUNT) return false;
    RECT separator = {g_app.nav_rects[page].left + sx(2),
                      g_app.nav_rects[page].top - sx(7),
                      g_app.nav_rects[page].right - sx(2),
                      g_app.nav_rects[page].top - sx(6)};
    fill_color(ctx->dc, separator, rgb(g_app.palette.border_rgb));
    return true;
}
static bool win_navigation_item(void *context, const RunnerUiPageSpec *page,
                                const char *platform_label, bool selected)
{
    WinNavigationContext *ctx = context;
    if (!ctx || !ctx->dc || !page || !platform_label) return false;
    const int index = (int)page->id;
    if (index < 0 || index >= RUNNER_UI_PAGE_COUNT) return false;
    RECT item = g_app.nav_rects[index];
    if (selected)
        fill_round(ctx->dc, item, rgb(g_app.palette.selection_background_rgb),
                   rgb(g_app.palette.neutral_accent_rgb), sx(12));
    RECT art = {item.left + sx(5), item.top + sx(5), item.left + sx(45), item.bottom - sx(5)};
    draw_image(ctx->dc, &g_app.nav_images[index], &art);
    wchar_t text[128];
    utf8_to_wide(platform_label, text, 128U);
    RECT label = {item.left + sx(54), item.top, item.right - sx(6), item.bottom};
    draw_text_value(ctx->dc, text, label, g_app.font_bold,
                    rgb(selected ? g_app.palette.selection_foreground_rgb
                                 : g_app.palette.text_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return true;
}
static bool win_end_navigation(void *context) { return context != NULL; }

static void draw_navigation(HDC dc, RECT content)
{
    RECT nav = {content.left, content.top, g_app.workspace_rect.left, content.bottom};
    fill_round(dc, nav, rgb(g_app.palette.panel_rgb),
               rgb(g_app.palette.connection_border_rgb), sx(12));
    WinNavigationContext ctx = {dc};
    RunnerUiRenderer renderer = {0};
    renderer.context = &ctx;
    renderer.begin_navigation = win_begin_navigation;
    renderer.navigation_separator = win_navigation_separator;
    renderer.navigation_item = win_navigation_item;
    renderer.end_navigation = win_end_navigation;
    (void)runner_ui_render_navigation(&renderer, RUNNER_UI_PLATFORM_WINDOWS,
                                      (RunnerUiPageId)g_app.page);
    if (g_app.side_image.bitmap) {
        RECT side = {nav.left + sx(18), nav.bottom - sx(194),
                     nav.right - sx(18), nav.bottom - sx(14)};
        draw_image(dc, &g_app.side_image, &side);
    }
}

typedef struct { HDC dc; RECT header; } WinWorkspaceContext;
static bool win_begin_workspace(void *context, const RunnerUiPageSpec *page,
                                const char *platform_title)
{
    WinWorkspaceContext *ctx = context;
    if (!ctx || !ctx->dc || !page || !platform_title) return false;
    const int page_index = (int)page->id;
    if (page_index < 0 || page_index >= RUNNER_UI_PAGE_COUNT) return false;
    ctx->header = (RECT){g_app.workspace_rect.left, g_app.workspace_rect.top,
                         g_app.workspace_rect.right, g_app.workspace_rect.top + sx(108)};
    fill_round(ctx->dc, ctx->header, rgb(g_app.palette.card_rgb),
               rgb(g_app.palette.connection_border_rgb), sx(12));
    RECT icon = {ctx->header.left + sx(16), ctx->header.top + sx(17),
                 ctx->header.left + sx(74), ctx->header.top + sx(75)};
    draw_image(ctx->dc, &g_app.nav_images[page_index], &icon);
    wchar_t title[160], subtitle[192];
    utf8_to_wide(platform_title, title, 160U);
    utf8_to_wide(page->subtitle, subtitle, 192U);
    RECT title_rect = {icon.right + sx(12), ctx->header.top + sx(14),
                       ctx->header.left + sx(430), ctx->header.top + sx(48)};
    draw_text_value(ctx->dc, title, title_rect, g_app.font_title,
                    rgb(g_app.palette.title_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT subtitle_rect = {title_rect.left, ctx->header.top + sx(48),
                          ctx->header.left + sx(440), ctx->header.top + sx(72)};
    draw_text_value(ctx->dc, subtitle, subtitle_rect, g_app.font_small,
                    rgb(g_app.palette.kicker_rgb),
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    const int metrics_width = sx(322);
    RECT hero = {ctx->header.right - metrics_width - sx(258), ctx->header.top + sx(11),
                 ctx->header.right - metrics_width - sx(18), ctx->header.top + sx(97)};
    draw_image(ctx->dc, &g_app.hero_images[page_index], &hero);
    return true;
}
static bool win_workspace_metric(void *context, size_t index,
                                 const char *caption, const char *value)
{
    WinWorkspaceContext *ctx = context;
    if (!ctx || !ctx->dc || index >= 4U || !caption || !value) return false;
    const int metric_w = sx(76), gap = sx(6), slot = 3 - (int)index;
    const int right = ctx->header.right - sx(8) - slot * (metric_w + gap);
    RECT metric = {right - metric_w, ctx->header.top + sx(18), right, ctx->header.top + sx(82)};
    fill_round(ctx->dc, metric,
               mix_color(g_app.palette.surface_rgb, g_app.palette.neutral_accent_rgb,
                         index == 1U ? 12U : 5U),
               rgb(g_app.palette.border_rgb), sx(9));
    wchar_t value_text[64], caption_text[64];
    utf8_to_wide(value, value_text, 64U);
    utf8_to_wide(caption, caption_text, 64U);
    RECT value_rect = {metric.left + sx(4), metric.top + sx(6),
                       metric.right - sx(4), metric.top + sx(37)};
    draw_text_value(ctx->dc, value_text, value_rect, g_app.font_title,
                    rgb(g_app.palette.title_rgb), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT caption_rect = {metric.left + sx(3), metric.top + sx(37),
                         metric.right - sx(3), metric.bottom - sx(4)};
    draw_text_value(ctx->dc, caption_text, caption_rect, g_app.font_small,
                    rgb(g_app.palette.muted_rgb),
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return true;
}
static bool win_end_workspace(void *context) { return context != NULL; }

static void draw_workspace_header(HDC dc)
{
    char storage[4][32] = {{0}};
    const char *values[4] = {"—", "—", "—", "—"};
    if (g_app.page == RUNNER_UI_PAGE_RUNNERS) {
        size_t running = 0U, idle = 0U, offline = 0U;
        for (size_t i = 0U; i < g_app.row_count; ++i) {
            if (strcmp(g_app.rows[i].state, "RUNNING") == 0) running++;
            else if (strcmp(g_app.rows[i].state, "IDLE") == 0) idle++;
            else offline++;
        }
        const size_t raw[4] = {g_app.row_count, running, idle, offline};
        for (size_t i = 0U; i < 4U; ++i) {
            (void)snprintf(storage[i], sizeof(storage[i]), "%zu", raw[i]);
            values[i] = storage[i];
        }
    }
    RunnerUiWorkspaceSummaryState state = {
        (RunnerUiPageId)g_app.page,
        {values[0], values[1], values[2], values[3]}
    };
    WinWorkspaceContext ctx = {0};
    ctx.dc = dc;
    RunnerUiRenderer renderer = {0};
    renderer.context = &ctx;
    renderer.begin_workspace_summary = win_begin_workspace;
    renderer.workspace_metric = win_workspace_metric;
    renderer.end_workspace_summary = win_end_workspace;
    (void)runner_ui_render_workspace_summary(&renderer, RUNNER_UI_PLATFORM_WINDOWS, &state);
}

static int card_columns(void)
{
    const int width = g_app.cards_area_rect.right - g_app.cards_area_rect.left;
    const int card_width = sx(248), gap = sx(10);
    int columns = (width + gap) / (card_width + gap);
    if (columns < 1) columns = 1;
    if (columns > 5) columns = 5;
    return columns;
}
static RECT card_rect(size_t index)
{
    const int columns = card_columns(), gap = sx(10);
    const int total = g_app.cards_area_rect.right - g_app.cards_area_rect.left;
    const int width = (total - gap * (columns - 1)) / columns;
    const int height = sx(154);
    const int row = (int)(index / (size_t)columns), column = (int)(index % (size_t)columns);
    const int x = g_app.cards_area_rect.left + column * (width + gap);
    const int y = g_app.cards_area_rect.top + row * (height + gap) - g_app.card_scroll_y;
    RECT rect = {x, y, x + width, y + height};
    return rect;
}
static int max_card_scroll(void)
{
    const size_t count = visible_count();
    const int columns = card_columns();
    const int rows = (int)((count + (size_t)columns - 1U) / (size_t)columns);
    const int content = rows > 0 ? rows * sx(154) + (rows - 1) * sx(10) : 0;
    const int viewport = g_app.cards_area_rect.bottom - g_app.cards_area_rect.top;
    return content > viewport ? content - viewport : 0;
}
