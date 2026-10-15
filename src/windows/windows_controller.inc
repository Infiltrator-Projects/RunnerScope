// Private Win32 window/controller slice; included by windows_main.c.
static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        g_app.window = window;
        g_app.dpi = GetDpiForWindow(window);
        if (g_app.dpi == 0U) g_app.dpi = 96U;
        resolve_theme();
        create_brushes();
        load_private_fonts();
        create_fonts();
        const HRESULT image_status = CoCreateInstance(
            &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void **)&g_app.wic);
        if (FAILED(image_status)) g_app.wic = NULL;
        load_images();
        create_search();
        init_list();
        g_app.session_started_ms = GetTickCount64();
        BOOL dark = system_prefers_dark() ? TRUE : FALSE;
        const DWORD attribute = 20U;
        (void)DwmSetWindowAttribute(window, attribute, &dark, sizeof(dark));
        SetTimer(window, TIMER_REFRESH, g_app.config.runner_poll_seconds * 1000U, NULL);
        SetTimer(window, TIMER_PRESENT, 1000U, NULL);
        update_layout(window);
        if (g_app.config_error[0]) {
            wchar_t text[320];
            utf8_to_wide(g_app.config_error, text, 320U);
            MessageBoxW(window, text, L"Runner Monitor configuration",
                        MB_OK | MB_ICONWARNING);
        }
        if (g_app.config.organisation[0] && !g_app.config_error[0]) refresh_begin();
        else PostMessageW(window, WM_APP_OPEN_SETTINGS, 0, 0);
        return 0;
    }
    case WM_APP_OPEN_SETTINGS:
        open_settings();
        return 0;
    case WM_NCCALCSIZE:
        if (wparam) return 0;
        break;
    case WM_NCHITTEST:
        return hit_test_non_client(window, lparam);
    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)lparam;
        info->ptMinTrackSize.x = sx(980);
        info->ptMinTrackSize.y = sx(660);
        return 0;
    }
    case WM_DPICHANGED: {
        g_app.dpi = HIWORD(wparam);
        if (g_app.dpi == 0U) g_app.dpi = 96U;
        delete_fonts();
        create_fonts();
        SendMessageW(g_app.search, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        SendMessageW(g_app.list, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        RECT *suggested = (RECT *)lparam;
        SetWindowPos(window, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        update_layout(window);
        InvalidateRect(window, NULL, TRUE);
        return 0;
    }
    case WM_SIZE:
        update_layout(window);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_TIMER:
        if (wparam == TIMER_REFRESH) {
            if (!g_app.refreshing && g_app.config.organisation[0] && !g_app.config_error[0])
                refresh_begin();
            return 0;
        }
        if (wparam == TIMER_PRESENT) {
            if (g_app.table_view) rebuild_table();
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_SEARCH && HIWORD(wparam) == EN_CHANGE) {
            GetWindowTextW(g_app.search, g_app.filter,
                           (int)(sizeof(g_app.filter) / sizeof(g_app.filter[0])));
            g_app.card_scroll_y = 0;
            g_app.selected_row = -1;
            rebuild_table();
            update_layout(window);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_NOTIFY:
        if (((NMHDR *)lparam)->idFrom == ID_RUNNERS &&
            ((NMHDR *)lparam)->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *change = (NMLISTVIEW *)lparam;
            if ((change->uNewState & LVIS_SELECTED) != 0U) {
                LVITEMW item;
                ZeroMemory(&item, sizeof(item));
                item.mask = LVIF_PARAM;
                item.iItem = change->iItem;
                if (ListView_GetItem(g_app.list, &item)) {
                    g_app.selected_row = (int)item.lParam;
                    update_layout(window);
                    InvalidateRect(window, NULL, FALSE);
                }
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP: {
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        handle_click(point);
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (g_app.page == RUNNER_UI_PAGE_RUNNERS && !g_app.table_view) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            g_app.card_scroll_y -= (delta / WHEEL_DELTA) * sx(60);
            if (g_app.card_scroll_y < 0) g_app.card_scroll_y = 0;
            const int maximum = max_card_scroll();
            if (g_app.card_scroll_y > maximum) g_app.card_scroll_y = maximum;
            InvalidateRect(window, &g_app.cards_area_rect, FALSE);
            return 0;
        }
        break;
    case WM_APP_REFRESH_DONE:
        apply_refresh((RefreshResult *)lparam);
        return 0;
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
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        HDC memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc,
            client.right - client.left, client.bottom - client.top);
        HGDIOBJ previous = SelectObject(memory, bitmap);
        paint_ui(memory);
        BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window, TIMER_REFRESH);
        KillTimer(window, TIMER_PRESENT);
        unload_images();
        if (g_app.wic) IWICImagingFactory_Release(g_app.wic);
        g_app.wic = NULL;
        delete_fonts();
        unload_private_fonts();
        delete_brushes();
        free(g_app.rows);
        free(g_app.sessions);
        g_app.rows = NULL;
        g_app.sessions = NULL;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static bool resource_exists(int resource_id)
{
    return FindResourceW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(resource_id), RT_RCDATA) != NULL;
}

static int self_test(void)
{
    char ui_error[256];
    if (!runner_ui_contract_validate(ui_error, sizeof(ui_error))) return 1;
    RunnerConfig config;
    runner_config_defaults(&config);
    (void)snprintf(config.organisation, sizeof(config.organisation), "%s", "Infiltrator-Projects");
    char json[1024], config_error[256];
    size_t amount = 0U;
    if (!runner_config_write_json(&config, json, sizeof(json), &amount,
                                  config_error, sizeof(config_error)) || amount == 0U)
        return 2;
    RunnerConfig parsed;
    if (!runner_config_parse_json(json, &parsed, config_error, sizeof(config_error)) ||
        strcmp(parsed.organisation, config.organisation) != 0)
        return 3;
    const InfiltratrThemePalette *day = infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false);
    const InfiltratrThemePalette *night = infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true);
    if (!day || !night || day->background_rgb == night->background_rgb) return 4;
    if (!INFILTRATR_COMMON_VERSION[0] || !RUNNERSCOPE_VERSION[0]) return 5;
    const int required[] = {
        IDR_NAV_RUNNERS, IDR_NAV_ACTIVE, IDR_NAV_HISTORY, IDR_NAV_HEALTH,
        IDR_NAV_INFILTRATOR, IDR_HERO_RUNNERS, IDR_HERO_ACTIVE,
        IDR_HERO_HISTORY, IDR_HERO_HEALTH, IDR_FONT_BRAND,
        IDR_FONT_UI_BOLD, IDR_FONT_UI_REGULAR
    };
    for (size_t i = 0U; i < sizeof(required) / sizeof(required[0]); ++i)
        if (!resource_exists(required[i])) return 6;
    return 0;
}
