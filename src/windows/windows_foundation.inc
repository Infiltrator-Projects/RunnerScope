// Private Win32 implementation slice; included by windows_main.c.
static int sx(int value)
{
    return MulDiv(value, (int)(g_app.dpi ? g_app.dpi : 96U), 96);
}

static COLORREF rgb(uint32_t value)
{
    return RGB((BYTE)((value >> 16U) & 0xffU),
               (BYTE)((value >> 8U) & 0xffU),
               (BYTE)(value & 0xffU));
}

static COLORREF mix_color(uint32_t base, uint32_t accent, unsigned int accent_percent)
{
    if (accent_percent > 100U) accent_percent = 100U;
    const unsigned int base_percent = 100U - accent_percent;
    const unsigned int br = (base >> 16U) & 0xffU;
    const unsigned int bg = (base >> 8U) & 0xffU;
    const unsigned int bb = base & 0xffU;
    const unsigned int ar = (accent >> 16U) & 0xffU;
    const unsigned int ag = (accent >> 8U) & 0xffU;
    const unsigned int ab = accent & 0xffU;
    return RGB((br * base_percent + ar * accent_percent) / 100U,
               (bg * base_percent + ag * accent_percent) / 100U,
               (bb * base_percent + ab * accent_percent) / 100U);
}

static bool pt_in_rect(const RECT *rect, POINT point)
{
    return rect && PtInRect(rect, point) != FALSE;
}

static void utf8_to_wide(const char *input, wchar_t *output, size_t count)
{
    if (!output || count == 0U) return;
    output[0] = L'\0';
    if (!input || !*input) return;
    int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1,
                                      output, (int)count);
    if (written <= 0)
        (void)MultiByteToWideChar(CP_ACP, 0, input, -1, output, (int)count);
    output[count - 1U] = L'\0';
}

static bool wide_to_utf8(const wchar_t *input, char *output, size_t count)
{
    if (!output || count == 0U) return false;
    output[0] = '\0';
    if (!input) return true;
    const int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           input, -1, NULL, 0, NULL, NULL);
    if (needed <= 0 || (size_t)needed > count) return false;
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                               input, -1, output, (int)count, NULL, NULL) > 0;
}

static bool system_prefers_dark(void)
{
    DWORD value = 1U;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL,
                     &value, &size) != ERROR_SUCCESS)
        return true;
    return value == 0U;
}

static InfiltratrThemeMode common_theme_mode(RunnerThemeMode mode)
{
    switch (mode) {
    case RUNNER_THEME_DAY: return INFILTRATR_THEME_DAY;
    case RUNNER_THEME_NIGHT: return INFILTRATR_THEME_NIGHT;
    case RUNNER_THEME_SYSTEM:
    default: return INFILTRATR_THEME_SYSTEM;
    }
}

static void resolve_theme(void)
{
    const InfiltratrThemePalette *palette = infiltratr_theme_resolve(
        common_theme_mode(g_app.config.theme_mode), system_prefers_dark());
    if (palette) g_app.palette = *palette;
}

static void delete_brushes(void)
{
    if (g_app.background_brush) DeleteObject(g_app.background_brush);
    if (g_app.input_brush) DeleteObject(g_app.input_brush);
    g_app.background_brush = NULL;
    g_app.input_brush = NULL;
}

static void create_brushes(void)
{
    delete_brushes();
    g_app.background_brush = CreateSolidBrush(rgb(g_app.palette.background_rgb));
    g_app.input_brush = CreateSolidBrush(rgb(g_app.palette.input_rgb));
}

static HANDLE load_private_font_resource(int resource_id)
{
    HRSRC resource = FindResourceW(g_app.instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return NULL;
    HGLOBAL loaded = LoadResource(g_app.instance, resource);
    if (!loaded) return NULL;
    const DWORD size = SizeofResource(g_app.instance, resource);
    void *data = LockResource(loaded);
    if (!data || size == 0U) return NULL;
    DWORD fonts = 0U;
    return AddFontMemResourceEx(data, size, NULL, &fonts);
}

static bool load_private_fonts(void)
{
    g_app.private_fonts[0] = load_private_font_resource(IDR_FONT_BRAND);
    g_app.private_fonts[1] = load_private_font_resource(IDR_FONT_UI_BOLD);
    g_app.private_fonts[2] = load_private_font_resource(IDR_FONT_UI_REGULAR);
    return g_app.private_fonts[0] && g_app.private_fonts[1] && g_app.private_fonts[2];
}

static void unload_private_fonts(void)
{
    for (size_t i = 0U; i < 3U; ++i) {
        if (g_app.private_fonts[i]) RemoveFontMemResourceEx(g_app.private_fonts[i]);
        g_app.private_fonts[i] = NULL;
    }
}

static void delete_fonts(void)
{
    if (g_app.font) DeleteObject(g_app.font);
    if (g_app.font_small) DeleteObject(g_app.font_small);
    if (g_app.font_bold) DeleteObject(g_app.font_bold);
    if (g_app.font_title) DeleteObject(g_app.font_title);
    if (g_app.font_brand) DeleteObject(g_app.font_brand);
    g_app.font = NULL;
    g_app.font_small = NULL;
    g_app.font_bold = NULL;
    g_app.font_title = NULL;
    g_app.font_brand = NULL;
}

static void create_fonts(void)
{
    const InfiltratrTypography *typography = infiltratr_typography();
    wchar_t ui_family[96] = L"";
    wchar_t brand_family[96] = L"";
    if (typography) {
        if (typography->ui_family) utf8_to_wide(typography->ui_family, ui_family, 96U);
        if (typography->brand_family) utf8_to_wide(typography->brand_family, brand_family, 96U);
    }
    g_app.font = CreateFontW(-sx(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_family);
    g_app.font_small = CreateFontW(-sx(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_family);
    g_app.font_bold = CreateFontW(-sx(16), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_family);
    g_app.font_title = CreateFontW(-sx(25), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, brand_family);
    g_app.font_brand = CreateFontW(-sx(22), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, brand_family);
}

static UiImage load_png_resource(int resource_id)
{
    UiImage image = {0};
    if (!g_app.wic) return image;
    HRSRC resource = FindResourceW(g_app.instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return image;
    HGLOBAL loaded = LoadResource(g_app.instance, resource);
    if (!loaded) return image;
    const DWORD size = SizeofResource(g_app.instance, resource);
    const BYTE *data = (const BYTE *)LockResource(loaded);
    if (!data || size == 0U) return image;

    IStream *stream = SHCreateMemStream(data, size);
    if (!stream) return image;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HRESULT status = IWICImagingFactory_CreateDecoderFromStream(
        g_app.wic, stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(status)) status = IWICBitmapDecoder_GetFrame(decoder, 0U, &frame);
    if (SUCCEEDED(status))
        status = IWICImagingFactory_CreateFormatConverter(g_app.wic, &converter);
    if (SUCCEEDED(status)) {
        status = IWICFormatConverter_Initialize(
            converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }
    UINT width = 0U;
    UINT height = 0U;
    if (SUCCEEDED(status))
        status = IWICBitmapSource_GetSize((IWICBitmapSource *)converter, &width, &height);

    HBITMAP bitmap = NULL;
    void *bits = NULL;
    if (SUCCEEDED(status) && width > 0U && height > 0U) {
        BITMAPINFO info;
        ZeroMemory(&info, sizeof(info));
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = (LONG)width;
        info.bmiHeader.biHeight = -(LONG)height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0U);
        if (bitmap && bits) {
            const UINT stride = width * 4U;
            status = IWICBitmapSource_CopyPixels((IWICBitmapSource *)converter,
                                                  NULL, stride, stride * height,
                                                  (BYTE *)bits);
            if (FAILED(status)) {
                DeleteObject(bitmap);
                bitmap = NULL;
            }
        }
    }
    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    IStream_Release(stream);
    if (bitmap) {
        image.bitmap = bitmap;
        image.width = (int)width;
        image.height = (int)height;
    }
    return image;
}

static void load_images(void)
{
    const int nav_ids[4] = {IDR_NAV_RUNNERS, IDR_NAV_ACTIVE, IDR_NAV_HISTORY, IDR_NAV_HEALTH};
    const int hero_ids[4] = {IDR_HERO_RUNNERS, IDR_HERO_ACTIVE, IDR_HERO_HISTORY, IDR_HERO_HEALTH};
    for (int i = 0; i < 4; ++i) {
        g_app.nav_images[i] = load_png_resource(nav_ids[i]);
        g_app.hero_images[i] = load_png_resource(hero_ids[i]);
    }
    g_app.side_image = load_png_resource(IDR_NAV_INFILTRATOR);
}

static void unload_images(void)
{
    for (int i = 0; i < 4; ++i) {
        if (g_app.nav_images[i].bitmap) DeleteObject(g_app.nav_images[i].bitmap);
        if (g_app.hero_images[i].bitmap) DeleteObject(g_app.hero_images[i].bitmap);
        ZeroMemory(&g_app.nav_images[i], sizeof(g_app.nav_images[i]));
        ZeroMemory(&g_app.hero_images[i], sizeof(g_app.hero_images[i]));
    }
    if (g_app.side_image.bitmap) DeleteObject(g_app.side_image.bitmap);
    ZeroMemory(&g_app.side_image, sizeof(g_app.side_image));
}

static void draw_image(HDC dc, const UiImage *image, const RECT *rect)
{
    if (!dc || !image || !image->bitmap || !rect) return;
    HDC source = CreateCompatibleDC(dc);
    if (!source) return;
    HGDIOBJ previous = SelectObject(source, image->bitmap);
    const BLENDFUNCTION blend = {AC_SRC_OVER, 0U, 255U, AC_SRC_ALPHA};
    AlphaBlend(dc, rect->left, rect->top,
               rect->right - rect->left, rect->bottom - rect->top,
               source, 0, 0, image->width, image->height, blend);
    SelectObject(source, previous);
    DeleteDC(source);
}

static void fill_color(HDC dc, RECT rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

static void fill_round(HDC dc, RECT rect, COLORREF fill, COLORREF border, int radius)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_text_value(HDC dc, const wchar_t *text, RECT rect, HFONT font,
                            COLORREF color, UINT format)
{
    HGDIOBJ previous = SelectObject(dc, font ? font : g_app.font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text ? text : L"", -1, &rect, format);
    SelectObject(dc, previous);
}
