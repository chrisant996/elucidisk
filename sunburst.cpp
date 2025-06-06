// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "sunburst.h"
#include "data.h"
#include "DarkMode.h"
#include "TextOnPath/PathTextRenderer.h"
#include <cmath>

static ID2D1Factory* s_pD2DFactory = nullptr;
static IDWriteFactory2* s_pDWriteFactory = nullptr;

#ifdef DEBUG
static bool s_fOklab = false;
bool GetUseOklab() { return s_fOklab; }
void SetUseOklab(bool use) { s_fOklab = use; }
#endif

constexpr FLOAT M_PI = 3.14159265358979323846f;
constexpr FLOAT c_centerRadiusRatio = 0.24f;
constexpr FLOAT c_centerRadiusRatioMax = 0.096125f;
constexpr FLOAT c_centerRadiusRatioNonProp = 0.15f;
constexpr int c_centerRadiusMin = 50;
constexpr int c_centerRadiusMax = 100;
constexpr FLOAT c_rotation = -90.0f;

constexpr size_t c_max_depth = 20;

// constexpr int c_max_thickness = 60;     // For proportional area.
constexpr int c_thickness = 25;
constexpr FLOAT c_thicknessRatioNonProp = 0.055f;
constexpr int c_retrograde = 1;
constexpr int c_retrograde_depths = 10;

constexpr WCHAR c_fontface[] = TEXT("Segoe UI");
constexpr FLOAT c_fontsize = 10.0f;
constexpr FLOAT c_headerfontsize = 12.0f;
constexpr FLOAT c_arcfontsize = 8.0f;
constexpr FLOAT c_minArc = 2.5f;
constexpr UINT32 c_minArcTextLength = 1;

constexpr WCHAR c_ellipsis[] = TEXT("...");
constexpr size_t c_ellipsis_len = _countof(c_ellipsis) - 1;

HRESULT InitializeD2D()
{
    return D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), 0, reinterpret_cast<void**>(&s_pD2DFactory));
}

HRESULT InitializeDWrite()
{
    return DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(&s_pDWriteFactory));
}

bool GetD2DFactory(ID2D1Factory** ppFactory)
{
    ID2D1Factory* pFactory = s_pD2DFactory;
    if (!pFactory)
        return false;

    pFactory->AddRef();
    *ppFactory = pFactory;
    return true;
}

bool GetDWriteFactory(IDWriteFactory2** ppFactory)
{
    IDWriteFactory2* pFactory = s_pDWriteFactory;
    if (!pFactory)
        return false;

    pFactory->AddRef();
    *ppFactory = pFactory;
    return true;
}

static FLOAT ArcLength(FLOAT angle, FLOAT radius)
{
    return angle * radius * M_PI / 180.0f;
}

//----------------------------------------------------------------------------
// HSLColorType.

template <typename T> T clamp(T value, T min, T max)
{
    value = value < min ? min : value;
    return value > max ? max : value;
}

namespace colorspace
{

static const FLOAT c_maxHue = 360;
static const FLOAT c_maxSat = 240;
static const FLOAT c_maxLum = 240;

struct HSLColorType
{
                HSLColorType() {}
                HSLColorType(COLORREF rgb) { FromRGB(rgb); }

    void        FromRGB( COLORREF rgb );
    COLORREF    ToRGB() const;

    void        SetSaturation(FLOAT value);
    void        SetLuminance(FLOAT value);

    void        AdjustSaturation(FLOAT delta);
    void        AdjustLuminance(FLOAT delta);

    void        FixLuminance();

    FLOAT h;    // 0..c_maxHue
    FLOAT s;    // 0..c_maxSat
    FLOAT l;    // 0..c_maxLum
};

void HSLColorType::FromRGB(const COLORREF rgb)
{
    const int minVal = std::min<BYTE>(std::min<BYTE>(GetRValue(rgb), GetGValue(rgb)), GetBValue(rgb));
    const int maxVal = std::max<BYTE>(std::max<BYTE>(GetRValue(rgb), GetGValue(rgb)), GetBValue(rgb));
    const int sumMinMax = minVal + maxVal;

    l = sumMinMax * c_maxLum / 255 / 2;

    assert(l >= 0);
    assert(l <= c_maxLum);

    if (minVal == maxVal)
    {
        s = 0;
        h = 0;
    }
    else
    {
        const int delta = maxVal - minVal;

        s = delta * c_maxSat;
        s /= (sumMinMax <= 255) ? sumMinMax : (510 - sumMinMax);

        assert(s >= 0);
        assert(s <= c_maxSat);

        int offset;

        if (maxVal == GetRValue(rgb))
        {
            h = FLOAT(GetGValue(rgb) - GetBValue(rgb));
            offset = 0;
        }
        else if (maxVal == GetGValue(rgb))
        {
            h = FLOAT(GetBValue(rgb) - GetRValue(rgb));
            offset = 2;
        }
        else
        {
            h = FLOAT(GetRValue(rgb) - GetGValue(rgb));
            offset = 4;
        }

        h *= c_maxHue;
        h /= delta;
        h += c_maxHue * offset;
        h /= 6;

        if (h >= c_maxHue)
            h -= c_maxHue;
        if (h < 0)
            h += c_maxHue;

        assert(h >= 0);
        assert(h < c_maxHue);
    }
}

static BYTE ToByteValue(FLOAT rm1, FLOAT rm2, FLOAT h)
{
    if (h >= c_maxHue)
        h -= c_maxHue;
    else if (h < 0)
        h += c_maxHue;

    if (h < c_maxHue / 6)
        rm1 = rm1 + (rm2 - rm1) * h / (c_maxHue / 6);
    else if (h < c_maxHue / 2)
        rm1 = rm2;
    else if (h < c_maxHue - (c_maxHue / 3))
        rm1 = rm1 + (rm2 - rm1) * ((c_maxHue - (c_maxHue / 3)) - h) / (c_maxHue / 6);

    return BYTE((rm1 * 255.0f) + 0.5f);
}

COLORREF HSLColorType::ToRGB() const
{
    const FLOAT validHue = clamp<FLOAT>(h, 0, c_maxHue);
    const FLOAT validSat = clamp<FLOAT>(s, 0, c_maxSat);
    const FLOAT validLum = clamp<FLOAT>(l, 0, c_maxLum);

    const FLOAT satRatio = validSat / c_maxSat;
    const FLOAT lumRatio = validLum / c_maxLum;

    if (!validSat)
    {
        BYTE const gray = BYTE(validLum * 255 / c_maxLum);
        return RGB(gray, gray, gray);
    }

    FLOAT rm2;

    if (validLum <= c_maxLum / 2)
        rm2 = lumRatio + (lumRatio * satRatio);
    else
        rm2 = (lumRatio + satRatio) - (lumRatio * satRatio);

    const FLOAT rm1 = (2.0f * lumRatio) - rm2;

    return RGB(ToByteValue(rm1, rm2, validHue + (c_maxHue / 3)),
               ToByteValue(rm1, rm2, validHue),
               ToByteValue(rm1, rm2, validHue - (c_maxHue / 3)));
}

void HSLColorType::SetSaturation(FLOAT value)
{
    if (value < 0)
        s = 0;
    else if (value > c_maxSat)
        s = c_maxSat;
    else
        s = value;
}

void HSLColorType::SetLuminance(FLOAT value)
{
    if (value < 0)
        l = 0;
    else if (value > c_maxLum)
        l = c_maxLum;
    else
        l = value;
}

void HSLColorType::AdjustSaturation(FLOAT delta)
{
    s += delta;

    if (delta < 0)
        s = std::max<FLOAT>(s, 0);
    else
        s = std::min<FLOAT>(s, c_maxSat);
}

void HSLColorType::AdjustLuminance(FLOAT delta)
{
    l += delta;

    if (delta < 0)
        l = std::max<FLOAT>(l, 0);
    else
        l = std::min<FLOAT>(l, c_maxLum);
}

void HSLColorType::FixLuminance()
{
    // Luminance in the blue/purple range of hue in the HSL color space is
    // disproportionate to the rest of the hue range.  This attempts to
    // compensate -- primarily so that text can have legible contrast.
    const float lo = 180.0f;
    const float hi = 300.0f;
    const float gravity = c_maxLum * 0.65f;
    if (h >= lo && h <= hi)
    {
        constexpr float pi = 3.14159f;
        const float hue_cos = cos((h - lo) * 2 * pi / (hi - lo));
        const float transform = (1.0f - hue_cos) / 2;
        if (l < gravity)
            l += transform * (gravity - l) * 0.8f;
        else
            l += transform * (gravity - l) * 0.6f;
    }
}

}; // namespace colorspace

//----------------------------------------------------------------------------
// Oklab color space.

#ifdef DEBUG

namespace colorspace
{

// The Oklab code here is based on https://bottosson.github.io/posts/oklab, in
// the public domain (and also available under the MIT License).

struct Oklab
{
    Oklab() = default;
    Oklab(COLORREF cr) { from_rgb(cr); }

    void from_rgb(COLORREF cr);
    COLORREF to_rgb() const;

    float L = 0;
    float a = 0;
    float b = 0;

    inline static float rgb_to_linear(BYTE val)
    {
        float x = float(val) / 255.0f;
        return (x > 0.04045f) ? std::pow((x + 0.055f) / 1.055f, 2.4f) : (x / 12.92f);
    }

    inline static BYTE linear_to_rgb(float val)
    {
        float x = (val >= 0.0031308f) ? (1.055f * std::pow(val, 1.0f / 2.4f) - 0.055f) : (12.92f * val);
        return BYTE(clamp(int(x * 255), 0, 255));
    }

    void get_Ch(float& C, float& h) const
    {
        C = sqrtf(a*a + b*b);

#ifdef USE_NATIVE_OKLAB_HUE
        h = h * 180.0f / M_PI;
        h -= M_PI / 2; // Oklab hue is 45 degrees different from HSL hue.
        if (h < 0.0f)
            h += 360.0f;
        assert(h >= 0.0f);
        assert(h <= 360.0f);
#else
        HSLColorType hsl(to_rgb());
        h = hsl.h;
#endif
    }

    void set_Ch(float C, float h)
    {
        assert(h >= 0.0f);
        assert(h <= c_maxHue);

#ifdef USE_NATIVE_OKLAB_HUE
        h += 45.0f;
        if (h >= 180.0f)
            h -= 360.0f;
        h = h * M_PI / 180.0f;
#else
        HSLColorType hsl;
        hsl.h = h;
        hsl.s = c_maxSat;
        hsl.l = c_maxLum / 2;

        Oklab tmp(hsl.ToRGB());
        h = atan2(tmp.b, tmp.a);
#endif

        a = C * cos(h);
        b = C * sin(h);
    }
};

void Oklab::from_rgb(COLORREF cr)
{
    float _r = rgb_to_linear(GetRValue(cr));
    float _g = rgb_to_linear(GetGValue(cr));
    float _b = rgb_to_linear(GetBValue(cr));

    float l = 0.4122214708f * _r + 0.5363325363f * _g + 0.0514459929f * _b;
    float m = 0.2119034982f * _r + 0.6806995451f * _g + 0.1073969566f * _b;
    float s = 0.0883024619f * _r + 0.2817188376f * _g + 0.6299787005f * _b;

    l = std::cbrt(l);
    m = std::cbrt(m);
    s = std::cbrt(s);

    L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

COLORREF Oklab::to_rgb() const
{
    float l = L + 0.3963377774f * a + 0.2158037573f * b;
    float m = L - 0.1055613458f * a - 0.0638541728f * b;
    float s = L - 0.0894841775f * a - 1.2914855480f * b;

    l = l * l * l;
    m = m * m * m;
    s = s * s * s;

    float _r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float _g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float _b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

    return RGB(linear_to_rgb(_r), linear_to_rgb(_g), linear_to_rgb(_b));
}

}; // namespace colorspace

#endif

//----------------------------------------------------------------------------
// DirectHwndRenderTarget.

#define ERRJMP(expr)      do { hr = (expr); assert(SUCCEEDED(hr)); if (FAILED(hr)) goto LError; } while (false)
#define ERRRET(expr)      do { hr = (expr); assert(SUCCEEDED(hr)); if (FAILED(hr)) return hr; } while (false)

HRESULT DirectHwndRenderTarget::Resources::Init(HWND hwnd, const D2D1_SIZE_U& size, const DpiScaler& dpi, bool dark_mode)
{
    HRESULT hr = S_OK;

    if (!m_spFactory && !GetD2DFactory(&m_spFactory))
        return E_UNEXPECTED;
    if (!m_spDWriteFactory && !GetDWriteFactory(&m_spDWriteFactory))
        return E_UNEXPECTED;

    const DpiScaler dpiWithTextScaling(dpi, true);
    const FLOAT dpiF = dpi.ScaleF(96);

    ERRRET(m_spFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), dpiF, dpiF, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE, D2D1_FEATURE_LEVEL_DEFAULT),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &m_spTarget));

    m_spTarget->SetDpi(dpiF, dpiF);

    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(dark_mode ? 0x444444 : 0x000000, 1.0f), &m_spLineBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(0x444444, 0.5f), &m_spFileLineBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_spFillBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(0x000000, 1.0f), &m_spOutlineBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF, 1.0f), &m_spOutlineBrush2));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_spTextBrush));

    const auto rstyle = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
    ERRRET(m_spFactory->CreateStrokeStyle(rstyle, nullptr, 0, &m_spRoundedStroke));

    const auto bstyle = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_BEVEL);
    ERRRET(m_spFactory->CreateStrokeStyle(bstyle, nullptr, 0, &m_spBevelStroke));

    SPI<IDWriteRenderingParams> spRenderingParams;
    ERRRET(m_spDWriteFactory->CreateRenderingParams(&spRenderingParams));

#ifdef THIS_ISNT_WORKING_RIGHT_YET_AND_MIGHT_NOT_BE_NEEDED_ANYWAY
    // Custom text rendering param object is created that uses all default
    // values except for the rendering mode which is now set to outline.  The
    // outline mode is much faster in this case as every time text is relaid
    // out on the path, it is rasterized as geometry.  This saves the extra
    // step of trying to find the text bitmaps in the font cache and then
    // repopulating the cache with the new ones.  Since the text may rotate
    // differently from frame to frame, new glyph bitmaps would be generated
    // often anyway.
    const DWRITE_RENDERING_MODE rendering_mode = DWRITE_RENDERING_MODE_OUTLINE;
#else
    const DWRITE_RENDERING_MODE rendering_mode = DWRITE_RENDERING_MODE_NATURAL;
#endif
    ERRRET(m_spDWriteFactory->CreateCustomRenderingParams(
            spRenderingParams->GetGamma(),
            spRenderingParams->GetEnhancedContrast(),
            spRenderingParams->GetClearTypeLevel(),
            spRenderingParams->GetPixelGeometry(),
            rendering_mode,
            &m_spRenderingParams));

    m_fontSize = FLOAT(-dpiWithTextScaling.PointSizeToHeight(c_fontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_fontSize,
            TEXT("en-US"),
            &m_spTextFormat));
    m_spTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    m_headerFontSize = FLOAT(-dpiWithTextScaling.PointSizeToHeight(c_headerfontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_headerFontSize,
            TEXT("en-US"),
            &m_spHeaderTextFormat));
    m_spHeaderTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    // Intentionally unscaled:  it's not important text, and if it's scaled it
    // can run over the sunburst chart.
    m_appInfoFontSize = FLOAT(-dpi.PointSizeToHeight(c_fontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_appInfoFontSize,
            TEXT("en-US"),
            &m_spAppInfoTextFormat));
    m_spAppInfoTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    m_arcFontSize = FLOAT(-dpiWithTextScaling.PointSizeToHeight(c_arcfontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_arcFontSize,
            TEXT("en-US"),
            &m_spArcTextFormat));
    m_spArcTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    ERRRET(m_spContext.HrQuery(m_spTarget));
    m_spContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    m_spContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
    m_spContext->SetTextRenderingParams(m_spRenderingParams);

    m_spPathTextRenderer = new PathTextRenderer(dpi.ScaleF(96));

    return S_OK;
}

DirectHwndRenderTarget::DirectHwndRenderTarget()
{
    m_resources = std::make_unique<Resources>();
}

DirectHwndRenderTarget::~DirectHwndRenderTarget()
{
    assert(!m_hwnd);
}

HRESULT DirectHwndRenderTarget::CreateDeviceResources(const HWND hwnd, const DpiScaler& dpi, bool dark_mode)
{
    assert(!m_hwnd || hwnd == m_hwnd);

    if (hwnd == m_hwnd && Target())
        return S_OK;

    m_resources.reset();
    m_resources = std::make_unique<Resources>();

    m_hwnd = hwnd;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_resources->Init(m_hwnd, size, dpi, dark_mode);
    if (FAILED(hr))
    {
        ReleaseDeviceResources();
        return hr;
    }

    return S_OK;
}

HRESULT DirectHwndRenderTarget::ResizeDeviceResources()
{
    if (!m_hwnd || !Target())
        return S_OK;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_resources->m_spTarget->Resize(size);
    if (FAILED(hr))
    {
        ReleaseDeviceResources();
        return hr;
    }

    return S_OK;
}

void DirectHwndRenderTarget::ReleaseDeviceResources()
{
    m_resources = std::make_unique<Resources>();
    m_hwnd = 0;
}

static void SetStringWithEllipsis(Shortened& out, const WCHAR* in, size_t len, size_t keep, int ellipsis=1)
{
    if (len && IS_HIGH_SURROGATE(in[len - 1]))
        len--;
    out.m_text.clear();
    if (ellipsis < 0)
    {
        out.m_text.append(c_ellipsis);
        out.m_text.append(in + len - keep);
    }
    else
    {
        out.m_text.append(in, keep);
        if (ellipsis > 0)
            out.m_text.append(c_ellipsis);
    }
}

bool DirectHwndRenderTarget::CreateTextFormat(FLOAT fontsize, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** ppTextFormat) const
{
    SPI<IDWriteTextFormat> spTextFormat;
    if (FAILED(m_resources->m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            fontsize,
            TEXT("en-US"),
            &spTextFormat)))
        return false;

    spTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    *ppTextFormat = spTextFormat.Transfer();
    return true;
}

bool DirectHwndRenderTarget::ShortenText(IDWriteTextFormat* format, const D2D1_RECT_F& rect, const WCHAR* text, const size_t len, FLOAT target, Shortened& out, int ellipsis)
{
    if (len <= 0)
        return false;

#ifdef DEBUG
    {
        D2D1_SIZE_F size;
        MeasureText(format, rect, text, len, size);
        assert(size.width > target);
    }
#endif

    out.m_text.clear();
    out.m_extent = 0.0f;

    size_t lo = 0;
    size_t hi = len - 1;
    Shortened tmp;
    while (lo < hi)
    {
        const size_t mid = (hi + lo) / 2;

        D2D1_SIZE_F size;
        SetStringWithEllipsis(tmp, text, len, mid, ellipsis);
        if (!MeasureText(format, rect, tmp.m_text, size))
            return false;

        if (size.width < target && tmp.m_extent < size.width)
        {
            out.m_text = std::move(tmp.m_text);
            out.m_extent = size.width;
            out.m_orig_offset = mid;
        }

        if (size.width < target)
            lo = mid + 1;
        else
            hi = mid;
    }

    return true;
}

bool DirectHwndRenderTarget::MeasureText(IDWriteTextFormat* format, const D2D1_RECT_F& rect, const std::wstring& text, D2D1_SIZE_F& size, IDWriteTextLayout** ppLayout)
{
    return MeasureText(format, rect, text.c_str(), text.length(), size, ppLayout);
}

bool DirectHwndRenderTarget::MeasureText(IDWriteTextFormat* format, const D2D1_RECT_F& rect, const WCHAR* text, size_t len, D2D1_SIZE_F& size, IDWriteTextLayout** ppLayout)
{
    const FLOAT xExtent = rect.right - rect.left;
    const FLOAT yExtent = rect.bottom - rect.top;

    SPI<IDWriteTextLayout> spTextLayout;
    if (FAILED(DWriteFactory()->CreateTextLayout(text, UINT32(len), format, xExtent, yExtent, &spTextLayout)))
        return false;

    DWRITE_TEXT_METRICS textMetrics;
    if (FAILED(spTextLayout->GetMetrics(&textMetrics)))
        return false;

    size = D2D1::SizeF(FLOAT(ceil(textMetrics.width)), FLOAT(ceil(textMetrics.height)));
    if (ppLayout)
        *ppLayout = spTextLayout.Transfer();
    return true;
}

bool DirectHwndRenderTarget::WriteText(IDWriteTextFormat* format, FLOAT x, FLOAT y, const D2D1_RECT_F& rect, const std::wstring& text, WriteTextOptions options, IDWriteTextLayout* pLayout)
{
    return WriteText(format, x, y, rect, text.c_str(), text.length(), options, pLayout);
}

bool DirectHwndRenderTarget::WriteText(IDWriteTextFormat* format, FLOAT x, FLOAT y, const D2D1_RECT_F& rect, const WCHAR* text, size_t len, WriteTextOptions options, IDWriteTextLayout* pLayout)
{
    const FLOAT xExtent = rect.right - rect.left;
    const FLOAT yExtent = rect.bottom - rect.top;

    SPI<IDWriteTextLayout> _spTextLayout;
    if (!pLayout)
    {
        if (FAILED(DWriteFactory()->CreateTextLayout(text, UINT32(len), format, xExtent, yExtent, &_spTextLayout)))
            return false;
        pLayout = _spTextLayout;
    }

    if (options & (WTO_HCENTER|WTO_VCENTER|WTO_RIGHT_ALIGN|WTO_BOTTOM_ALIGN|WTO_REMEMBER_METRICS))
    {
        DWRITE_TEXT_METRICS textMetrics;
        if (FAILED(pLayout->GetMetrics(&textMetrics)))
            return false;

        const auto size = D2D1::SizeF(FLOAT(ceil(textMetrics.width)), FLOAT(ceil(textMetrics.height)));
        if (options & WTO_HCENTER)
            x = std::max<FLOAT>(0.0f, FLOAT(floor(rect.left + (xExtent - size.width) / 2)));
        if (options & WTO_VCENTER)
            y = FLOAT(floor(rect.top + (yExtent - size.height) / 2));
        if (options & WTO_RIGHT_ALIGN)
            x = rect.right - size.width;
        if (options & WTO_BOTTOM_ALIGN)
            y = rect.bottom - size.height;

        if (options & WTO_REMEMBER_METRICS)
            m_resources->m_lastTextSize = size;
    }

    const auto position = D2D1::Point2F(x, y);
    if (options & WTO_REMEMBER_METRICS)
        m_resources->m_lastTextPosition = position;

    D2D1_DRAW_TEXT_OPTIONS opt = D2D1_DRAW_TEXT_OPTIONS_NONE;
    if (options & WTO_CLIP)
        opt |= D2D1_DRAW_TEXT_OPTIONS_CLIP;

    if (options & WTO_UNDERLINE)
    {
        DWRITE_TEXT_RANGE range = { 0, UINT32(len) };
        pLayout->SetUnderline(true, range);
    }

    Target()->DrawTextLayout(position, pLayout, TextBrush(), opt);
    return true;
}

//----------------------------------------------------------------------------
// SunburstMetrics.

static FLOAT make_center_radius(const DpiScaler& dpi, const FLOAT boundary_radius, const FLOAT max_extent)
{
    if (g_show_proportional_area)
    {
        // winR and maxR use different ratios to accelerate growth of radius
        // when resizing the window larger, but with a maximum beyond which it
        // stops growing.
        const FLOAT winR = std::max<FLOAT>(FLOAT(dpi.Scale(c_centerRadiusMin)), boundary_radius * c_centerRadiusRatio);
        const FLOAT maxR = std::max<FLOAT>(FLOAT(dpi.Scale(c_centerRadiusMin)), max_extent * c_centerRadiusRatioMax);
        return std::min<FLOAT>(winR, maxR);
    }
    else
        return std::max<FLOAT>(FLOAT(dpi.Scale(c_centerRadiusMin)),
                               boundary_radius * c_centerRadiusRatioNonProp);
}

SunburstMetrics::SunburstMetrics(const Sunburst& sunburst)
: SunburstMetrics(sunburst.m_dpi, sunburst.m_bounds, sunburst.m_max_extent)
{
}

SunburstMetrics::SunburstMetrics(const DpiScaler& dpi, const D2D1_RECT_F& bounds, FLOAT max_extent)
: stroke(std::max<FLOAT>(FLOAT(dpi.Scale(1)), FLOAT(1)))
, margin(FLOAT(dpi.Scale(5)))
, indicator_thickness(FLOAT(dpi.Scale(4)))
, boundary_radius(FLOAT(std::min<LONG>(LONG(bounds.right - bounds.left), LONG(bounds.bottom - bounds.top)) / 2 - margin))
, center_radius(make_center_radius(dpi, boundary_radius, max_extent))
, max_radius(boundary_radius - (margin + indicator_thickness + margin))
, range_radius(max_radius - center_radius)
, min_arc(dpi.ScaleF(c_minArc))
{
    if (g_show_proportional_area)
    {
        FLOAT radius = center_radius;
        // const FLOAT coefficient = 0.18f;
        // FLOAT thickness = FLOAT(ceil(std::min<FLOAT>(center_radius * coefficient, 9999999999.9f)));//FLOAT(dpi.Scale(c_max_thickness)))));
        const FLOAT coefficient = 0.67f;
        FLOAT thickness = FLOAT(ceil(center_radius * coefficient));
        for (size_t ii = 0; ii < _countof(thicknesses); ++ii)
        {
            thicknesses[ii] = thickness;
            const FLOAT outer = radius + thickness;
            const FLOAT add = sqrt(2 * outer * outer - radius * radius) - outer;
            thickness = FLOAT(floor(add));
            radius = outer;
        }
    }
    else
    {
        FLOAT thickness = std::max<FLOAT>(FLOAT(dpi.Scale(c_thickness)),
                                          boundary_radius * c_thicknessRatioNonProp);
        const FLOAT retrograde = FLOAT(dpi.Scale(c_retrograde));
        for (size_t ii = 0; ii < _countof(thicknesses); ++ii)
            thicknesses[ii] = thickness - (retrograde * std::min<size_t>(ii, c_retrograde_depths));
    }
}

FLOAT SunburstMetrics::get_thickness(size_t depth) const
{
    if (depth < _countof(thicknesses))
        return thicknesses[depth];
    return g_show_proportional_area ? 0.0f : thicknesses[_countof(thicknesses) - 1];
}

//----------------------------------------------------------------------------
// Sunburst.

Sunburst::Sunburst()
{
}

Sunburst::~Sunburst()
{
}

bool Sunburst::SetBounds(const D2D1_RECT_F& rect, const FLOAT max_extent)
{
    static_assert(sizeof(m_bounds) == sizeof(rect), "data size mismatch");
    const bool changed = (!!memcmp(&m_bounds, &rect, sizeof(rect)) ||
                          m_max_extent != max_extent);

    m_bounds = rect;
    m_max_extent = FLOAT(max_extent);
    m_center.x = floor((rect.left + rect.right) / 2.0f);
    m_center.y = floor((rect.top + rect.bottom) / 2.0f);

    return changed;
}

void Sunburst::MakeArc(std::vector<Arc>& arcs, FLOAT outer_radius, const FLOAT min_arc, const std::shared_ptr<Node>& node, ULONGLONG size, double& sweep, double total, float start, float span, double convert)
{
    const bool zero = (total == 0.0f);
    Arc arc;
    arc.m_start = start + float(zero ? 0.0f : convert * sweep * span / total);
    sweep += size;
    arc.m_end = start + float(zero ? 0.0f : convert * sweep * span / total);

#ifdef DEBUG
    if (arc.m_start > 360.0f || arc.m_end > 360.0f)
        DebugBreak();
    assert(arc.m_end - arc.m_start <= span);
#endif

    if (ArcLength(arc.m_end - arc.m_start, outer_radius) >= min_arc)
    {
        arc.m_node = node;
        arcs.emplace_back(std::move(arc));
    }
}

void Sunburst::BuildRings(const SunburstMetrics& mx, const std::vector<std::shared_ptr<DirNode>>& _roots)
{
    const std::vector<std::shared_ptr<DirNode>> roots = _roots;

    std::vector<double> totals; // Total space (used + free); when FreeSpaceNode is present it's total hardware space.
    std::vector<double> used;   // Used space; when FreeSpaceNode is present it's used hardware space.
    std::vector<double> scale;  // Multiplier to scale used content space into used hardware space.
    std::vector<float> spans;   // Angle span for used space.

    m_roots = roots;
    m_rings.clear();
    m_start_angles.clear();
    m_free_angles.clear();

    {
        bool show_free_space = g_show_free_space;
#ifdef DEBUG
        if (g_fake_data == FDM_COLORWHEEL)
        {
            // This is important to prevent free space in the root, so that
            // the color wheel uses the full 360 degrees.
            show_free_space = false;
        }
#endif

        double grand_total = 0;
        for (const auto dir : roots)
        {
            const double size = double(dir->GetSize());
            std::shared_ptr<FreeSpaceNode> free = show_free_space ? dir->GetFreeSpace() : nullptr;
            if (free)
            {
                totals.emplace_back(double(free->GetTotalSize()));
                used.emplace_back(double(free->GetUsedSize()));
                if (size == 0.0f || used.back() == 0.0f)
                    scale.emplace_back(0.0f);
                else if (dir->IsFinished())
                    scale.emplace_back(used.back() / size);
                else
                    scale.emplace_back(used.back() / std::max<double>(used.back(), size));
            }
            else
            {
                totals.emplace_back(size);
                used.emplace_back(size);
                scale.emplace_back(1.0f);
            }
            grand_total += totals.back();
        }

        m_units = AutoUnitScale(ULONGLONG(grand_total));

        if (grand_total == 0)
            return;

        double sweep = 0;
        for (size_t ii = 0; ii < roots.size(); ++ii)
        {
            const float start = float(sweep * 360 / grand_total);
            const float mid = float((sweep + used[ii]) * 360 / grand_total);
            sweep += totals[ii];
            const float end = float(sweep * 360 / grand_total);
            m_start_angles.emplace_back(start);
            spans.emplace_back(mid - start);

            if (show_free_space)
            {
                std::shared_ptr<FreeSpaceNode> free = m_roots[ii]->GetFreeSpace();
                if (free)
                {
                    const float angle = float((sweep - free->GetFreeSize()) * 360 / grand_total);
                    m_free_angles.emplace_back(angle);
                }
                else
                {
                    m_free_angles.emplace_back(end);
                }
            }
        }
    }

    m_rings.emplace_back();

    std::vector<Arc>& arcs = m_rings.back();

    FLOAT outer_radius = mx.center_radius + mx.get_thickness(0);
    const FLOAT min_arc = mx.min_arc;

    for (size_t ii = 0; ii < roots.size(); ++ii)
    {
        std::shared_ptr<DirNode> root = roots[ii];
        std::vector<std::shared_ptr<DirNode>> dirs = root->CopyDirs(true/*include_recycle*/);
        std::vector<std::shared_ptr<FileNode>> files = root->CopyFiles();
        std::shared_ptr<FreeSpaceNode> free = root->GetFreeSpace();

        const double total = totals[ii];
        const double consumed = used[ii];
        const double convert = scale[ii];
        const float start = m_start_angles[ii];
        const float span = spans[ii];

        double sweep = 0;
        for (const auto dir : dirs)
            MakeArc(arcs, outer_radius, min_arc, std::static_pointer_cast<Node>(dir), dir->GetSize(), sweep, consumed, start, span, convert);
        for (const auto file : files)
            MakeArc(arcs, outer_radius, min_arc, std::static_pointer_cast<Node>(file), file->GetSize(), sweep, consumed, start, span, convert);
#ifdef USE_FREESPACE_RING
        if (free)
        {
            Arc arc;
            arc.m_start = m_free_angles[ii];
            arc.m_end = m_start_angles[(ii + 1) % m_roots.size()];
            if (arc.m_end < arc.m_start)
                arc.m_end += 360.0f;
            arc.m_node = free;
            arcs.emplace_back(std::move(arc));
        }
#endif
    }

    while (m_rings.size() <= c_max_depth)
    {
        outer_radius += mx.get_thickness(m_rings.size() + 1);

        std::vector<Arc> arcs = NextRing(m_rings.back(), outer_radius, min_arc);
        if (arcs.empty())
            break;
        m_rings.emplace_back(std::move(arcs));
    }

#ifdef DEBUG
    for (const auto ring : m_rings)
    {
        float prev = ring.size() ? ring[0].m_start : 0;
        for (const auto arc : ring)
        {
            assert(arc.m_start >= prev);
            prev = arc.m_end;
        }
    }
#endif
}

std::vector<Sunburst::Arc> Sunburst::NextRing(const std::vector<Arc>& parent_ring, const FLOAT outer_radius, const FLOAT min_arc)
{
    std::vector<Arc> arcs;

    for (const auto _parent : parent_ring)
    {
        const DirNode* parent = _parent.m_node->AsDir();
        if (parent && !parent->IsHidden())
        {
            double sweep = 0;

            const std::vector<std::shared_ptr<DirNode>> dirs = parent->CopyDirs();
            const std::vector<std::shared_ptr<FileNode>> files = parent->CopyFiles();

#ifdef DEBUG
            const size_t index = arcs.size();
#endif

            const float start = _parent.m_start;
            const float span = _parent.m_end - _parent.m_start;

            const double range = double(parent->GetSize());
            for (const auto dir : dirs)
                MakeArc(arcs, outer_radius, min_arc, std::static_pointer_cast<Node>(dir), dir->GetSize(), sweep, range, start, span);
            for (const auto file : files)
                MakeArc(arcs, outer_radius, min_arc, std::static_pointer_cast<Node>(file), file->GetSize(), sweep, range, start, span);

#ifdef DEBUG
            if (arcs.size() > index)
            {
                assert(arcs[index].m_start >= _parent.m_start);
                assert(arcs.back().m_end <= _parent.m_end + 0.001f);
            }
#endif
        }
    }

    return arcs;
}

static FLOAT FindAngle(const D2D1_POINT_2F& center, FLOAT x, FLOAT y)
{
    FLOAT angle;

    if (x == center.x)
    {
        angle = (y < center.y) ? 270.0f : 90.0f;
    }
    else if (y == center.y)
    {
        angle = (x < center.x) ? 180.0f : 0.0f;
    }
    else
    {
        angle = atan2((y - center.y), (x - center.x)) * 180.0f / M_PI;
        if (angle < 0.0f)
            angle += 360.0f;
    }

    if (c_rotation != 0.0f)
    {
        angle -= c_rotation;
        if (angle < 0.0f)
            angle += 360.0f;
        else if (angle >= 360.0f)
            angle -= 360.0f;
    }

    return angle;
}

static D2D1_POINT_2F MakePoint(const D2D1_POINT_2F& center, FLOAT radius, FLOAT angle)
{
    // (X - CenterX) / cos(Theta) == (Y - CenterY) / sin(Theta) == Radius
    // X = Radius * cos(Theta) + CenterX
    // Y = Radius * sin(Theta) + CenterY
    D2D1_POINT_2F point;
    point.x = radius * cosf(angle * M_PI / 180.0f) + center.x;
    point.y = radius * sinf(angle * M_PI / 180.0f) + center.y;
    return point;
}


static COLORREF color_from_angle_depth(FLOAT angle, size_t depth, bool highlight, bool file)
{
#ifdef DEBUG
    if (s_fOklab)
    {
        colorspace::Oklab oklab(RGB(0, 255, 0));

        float C;
        float h;
        oklab.get_Ch(C, h);

        h = clamp<FLOAT>(angle, 0.0f, 360.0f);
        C = C * (file ? 0.7f : 0.95f) - (FLOAT(depth) * C / 25);//20);
        if (highlight)
            C = C + 0.1f;

        oklab.set_Ch(C, h);

        oklab.L = (file ? 0.9f : 0.5f) + (FLOAT(depth) * (oklab.L / 20));
        //oklab.L = (file ? 0.8f : 0.7f) + (FLOAT(depth) * (1.0f / 30.0f));

        if (highlight)
            oklab.L = 0.8f;//-= 0.05f;

        return oklab.to_rgb();
    }
    else
#endif
    {
        colorspace::HSLColorType hsl;
        hsl.h = angle * colorspace::c_maxHue / 360;
        hsl.s = highlight ? colorspace::c_maxSat : (colorspace::c_maxSat * (file ? 0.7f : 0.95f)) - (FLOAT(depth) * (colorspace::c_maxSat / 25));
        hsl.l = highlight ? colorspace::c_maxLum*3/5 : (colorspace::c_maxLum * (file ? 0.6f : 0.4f)) + (FLOAT(depth) * (colorspace::c_maxLum / 30));
        hsl.FixLuminance();
        return hsl.ToRGB();
    }
}

inline BYTE blend(BYTE a, BYTE b, FLOAT ratio)
{
    return BYTE(FLOAT(a) * ratio) + BYTE(FLOAT(b) * (1.0f - ratio));
}

D2D1_COLOR_F Sunburst::MakeColor(const Arc& arc, size_t depth, bool highlight)
{
    if (arc.m_node->AsFreeSpace())
        return D2D1::ColorF(highlight ? D2D1::ColorF::LightSteelBlue : (m_dark_mode ? 0xdddddd : D2D1::ColorF::WhiteSmoke));

    DirNode* dir = arc.m_node->AsDir();
    FileNode* file = arc.m_node->AsFile();
    if (file)
        depth = 0;
    else if (dir && dir->IsHidden())
        return D2D1::ColorF(0xB8B8B8);

    if (!is_root_finished(arc.m_node))
        return D2D1::ColorF(highlight ? 0x3078F8 : 0xB8B8B8);

    switch (g_color_mode)
    {
    default:
    case CM_PLAIN:
        return D2D1::ColorF(highlight ? 0x3078F8 : 0x6495ED);

    case CM_RAINBOW:
        {
            const FLOAT angle = (arc.m_start + arc.m_end) / 2.0f;
            const COLORREF rgb = color_from_angle_depth(angle, depth, highlight, file);

            D2D1_COLOR_F color;
            color.r = FLOAT(GetRValue(rgb)) / 255;
            color.g = FLOAT(GetGValue(rgb)) / 255;
            color.b = FLOAT(GetBValue(rgb)) / 255;
            color.a = 1.0f;

            return color;
        }
        break;

    case CM_HEATMAP:
        {
            ULONGLONG root_total = 0;
            ULONGLONG node_total = dir ? dir->GetSize() : file ? file->GetSize() : 0;
            for (auto parent = arc.m_node->GetParent(); parent; parent = parent->GetParent())
            {
                if (parent->AsDrive() && parent->AsDrive()->GetFreeSpace())
                    root_total = parent->AsDrive()->GetFreeSpace()->GetTotalSize();
                else
                    root_total = parent->GetSize();
            }

            const ULONGLONG skew = ULONGLONG(root_total * 0.01f);
            root_total = (root_total > skew) ? root_total - skew : 0;
            node_total = (node_total > skew) ? node_total - skew : 0;

            const FLOAT size_max = FLOAT(root_total) * 0.2f;
            const FLOAT size_node = FLOAT(node_total);

            const FLOAT hue1 = 0.0f;
            const FLOAT hue2 = 90.0f;
            COLORREF rgb;

            {
                // const FLOAT log_max = sqrt(FLOAT(size_max));
                // const FLOAT log_node = log_max - sqrt(FLOAT(size_max - size_node));
                // const FLOAT ratio = (log_max > 0) ? log_node / log_max : 0.0f;
                const FLOAT ratio = size_node / size_max;

                const FLOAT range = hue2 - hue1;
                const FLOAT hue = range - ratio * range;

                rgb = color_from_angle_depth(hue, depth, highlight, !!file);
            }

            D2D1_COLOR_F color;
            color.r = FLOAT(GetRValue(rgb)) / 255;
            color.g = FLOAT(GetGValue(rgb)) / 255;
            color.b = FLOAT(GetBValue(rgb)) / 255;
            color.a = 1.0f;

            return color;
        }
        break;
    }
}

D2D1_COLOR_F Sunburst::MakeRootColor(bool highlight, bool free)
{
    if (highlight)
    {
        if (m_dark_mode)
            return D2D1::ColorF(free ? 0x94ADD4 : 0x728AB8);//0x839FD4);
        else
            return D2D1::ColorF(free ? 0xD0E4FE : 0xB0C4DE);
    }
    else
    {
        if (m_dark_mode)
            return D2D1::ColorF(free ? 0xFFFFFF : 0xD0D0D0, 0.66f);
        else
            return D2D1::ColorF(free ? 0xB1B1B1 : 0x616161, 0.5f);  // (free ? 0xD8D8D8 : 0xB0B0B0, 1.0f)
    }
}

inline D2D1_ARC_SIZE GetArcSize(FLOAT start, FLOAT end)
{
    return (end - start > 180.0f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
}

void Sunburst::AddArcToSink(ID2D1GeometrySink* pSink, const bool counter_clockwise, const FLOAT start, const FLOAT end, const D2D1_POINT_2F& end_point, const FLOAT radius)
{
    // When start and end points of an arc are identical, D2D gets confused
    // where to draw the arc, since it's a full circle.  Split into two arcs.
    const bool split = (start == end || end - start > 270.0f);

    D2D1_ARC_SEGMENT segment;
    segment.size = D2D1::SizeF(radius, radius);
    segment.sweepDirection = counter_clockwise ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE;

    if (split)
    {
        const FLOAT mid = start + std::min<FLOAT>(359.0f, end - 1.0f - start);

        if (!counter_clockwise)
        {
            segment.point = MakePoint(m_center, radius, mid);
            segment.rotationAngle = start;
            segment.arcSize = GetArcSize(start, mid);
            pSink->AddArc(segment);

            segment.point = MakePoint(m_center, radius, end);
            segment.rotationAngle = mid;
            segment.arcSize = GetArcSize(mid, end);
            pSink->AddArc(segment);
        }
        else
        {
            segment.point = MakePoint(m_center, radius, mid);
            segment.rotationAngle = mid;
            segment.arcSize = GetArcSize(mid, end);
            pSink->AddArc(segment);

            segment.point = MakePoint(m_center, radius, start);
            segment.rotationAngle = end;
            segment.arcSize = GetArcSize(start, mid);
            pSink->AddArc(segment);
        }
    }
    else
    {
        if (!counter_clockwise)
        {
            segment.point = MakePoint(m_center, radius, end);
            segment.rotationAngle = start;
            segment.arcSize = GetArcSize(start, end);
            pSink->AddArc(segment);
        }
        else
        {
            segment.point = MakePoint(m_center, radius, start);
            segment.rotationAngle = end;
            segment.arcSize = GetArcSize(start, end);
            pSink->AddArc(segment);
        }
    }
}

bool Sunburst::MakeArcGeometry(DirectHwndRenderTarget& target, FLOAT start, FLOAT end, const FLOAT inner_radius, const FLOAT outer_radius, ID2D1Geometry** ppGeometry)
{
    const bool has_line = (start != end) || (inner_radius > 0.0f);
    if (end <= start)
        end += 360.0f;


    start += c_rotation;
    end += c_rotation;

    D2D1_POINT_2F inner_start_point = MakePoint(m_center, inner_radius, start);
    D2D1_POINT_2F inner_end_point = MakePoint(m_center, inner_radius, end);
    D2D1_POINT_2F outer_start_point = MakePoint(m_center, outer_radius, start);
    D2D1_POINT_2F outer_end_point = MakePoint(m_center, outer_radius, end);

    SPI<ID2D1PathGeometry> spGeometry;
    if (SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
            spSink->BeginFigure(outer_start_point, D2D1_FIGURE_BEGIN_FILLED);

            AddArcToSink(spSink, false, start, end, outer_end_point, outer_radius);

            if (has_line)
                spSink->AddLine(inner_end_point);

            if (inner_radius > 0.0f)
                AddArcToSink(spSink, true, start, end, inner_start_point, inner_radius);

            spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            spSink->Close();
        }
    }

    *ppGeometry = spGeometry.Transfer();
    return !!*ppGeometry;
}

inline bool is_highlight(const std::shared_ptr<Node>& highlight, const std::shared_ptr<Node>& node)
{
    return highlight && highlight == node && is_root_finished(node);
}

// 1 = success, 0 = failure, -1 = can be shortened.
int Sunburst::DrawArcTextInternal(DirectHwndRenderTarget& target, IDWriteFactory* pFactory, const WCHAR* text, UINT32 length, FLOAT start, FLOAT end, FLOAT radius, bool only_test_fit)
{
    SPI<IDWriteTextLayout> spTextLayout;
    if (FAILED(pFactory->CreateTextLayout(text, length, target.ArcTextFormat(), m_bounds.right - m_bounds.left, m_bounds.bottom - m_bounds.top, &spTextLayout)))
        return 0;

    D2D1_POINT_2F outer_start_point = MakePoint(m_center, radius, start);
    D2D1_POINT_2F outer_end_point = MakePoint(m_center, radius, end);

    SPI<ID2D1PathGeometry> spGeometry;
    if (FAILED(target.Factory()->CreatePathGeometry(&spGeometry)))
        return 0;

    SPI<ID2D1GeometrySink> spSink;
    if (FAILED(spGeometry->Open(&spSink)))
        return 0;

    spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
    spSink->BeginFigure(outer_start_point, D2D1_FIGURE_BEGIN_HOLLOW);

    AddArcToSink(spSink, false, start, end, outer_end_point, radius);

    spSink->EndFigure(D2D1_FIGURE_END_OPEN);
    spSink->Close();

    PathTextDrawingContext context;
    context.brush.Set(target.TextBrush());
    context.geometry.Set(spGeometry);
    context.d2DContext.Set(target.Context());

    const HRESULT hr = target.ArcTextRenderer()->TestFit(&context, spTextLayout);
    if (FAILED(hr))
        return 0;
    if (hr == S_FALSE)
        return -1;

    if (!only_test_fit)
        spTextLayout->Draw(&context, target.ArcTextRenderer(), 0, 0);
    return 1;
}

void Sunburst::DrawArcText(DirectHwndRenderTarget& target, const Arc& arc, FLOAT radius)
{
    if (ArcLength(arc.m_end - arc.m_start, radius) < m_min_arc_text_len)
        return;

    IDWriteFactory* pFactory = target.DWriteFactory();
    if (!pFactory)
        return;

    std::wstring text;
    text.append(TEXT(" "));
#ifdef SHOW_ARC_LENGTH
    WCHAR xx[1024];
    swprintf_s(xx, TEXT("%u"), UINT32(ArcLength(arc.m_end - arc.m_start, radius)));
    text.append(xx);
#else
    text.append(arc.m_node->GetName());
#endif
    text.append(TEXT(" "));

    const FLOAT start = arc.m_start + c_rotation;
    const FLOAT end = arc.m_end + c_rotation;

    UINT32 length = UINT32(text.length());
    if (DrawArcTextInternal(target, pFactory, text.c_str(), length, start, end, radius) < 0)
    {
        std::wstring truncated;
        UINT32 lo = c_minArcTextLength;
        UINT32 hi = length - 2 - 2; // -2 for the padding spaces, -2 to ensure truncation doesn't just replace a character with an ellipsis.
        UINT32 fits = 0;
        while (lo <= hi)
        {
            UINT32 mid = (lo + hi) / 2;
            truncated.clear();
            truncated.append(text.c_str(), mid + 1); // +1 for leading space.
            truncated.append(c_ellipsis, c_ellipsis_len);
            truncated.append(TEXT(" "));
            const int result = DrawArcTextInternal(target, pFactory, truncated.c_str(), UINT32(truncated.length()), start, end, radius, true/*only_test_fit*/);
            if (result == 0)
                return;
            else if (result < 0)
                hi = mid - 1;
            else
            {
                lo = mid + 1;
                if (fits < mid)
                    fits = mid;
            }
        }

        if (fits >= c_minArcTextLength && fits < text.length())
        {
            ++fits; // Including the leading space.
            fits -= IS_HIGH_SURROGATE(text[fits - 1]);
            if (fits <= 1) // Must end up with more than just the leading space.
                return;
            truncated.clear();
            truncated.append(text.c_str(), fits);
            truncated.append(c_ellipsis, c_ellipsis_len);
            truncated.append(TEXT(" "));
            DrawArcTextInternal(target, pFactory, truncated.c_str(), UINT32(truncated.length()), start, end, radius);
        }
    }
}

void Sunburst::RenderRings(DirectHwndRenderTarget& target, const SunburstMetrics& mx, const std::shared_ptr<Node>& highlight)
{
    if (m_start_angles.empty())
        return;

    HighlightInfo highlightInfo;

    // Two passes, so files are "beneath" everything else.

    RenderRingsInternal(target, mx, highlight, true/*files*/, highlightInfo);
    RenderRingsInternal(target, mx, highlight, false/*files*/, highlightInfo);

    // Hover highlight.

    if (highlightInfo.m_geometry && (!highlight->GetParent() || is_root_finished(highlight)))
    {
        if (g_show_comparison_bar && highlight->GetParent() && highlightInfo.m_arc.m_node)
        {
            SPI<ID2D1Geometry> spCompBar;
            const FLOAT n = FLOAT(m_dpi.Scale(4));
            const FLOAT outer_radius = mx.center_radius - n;
            const FLOAT inner_radius = outer_radius - n;
            MakeArcGeometry(target, highlightInfo.m_arc.m_start, highlightInfo.m_arc.m_end, inner_radius, outer_radius, &spCompBar);

            target.FillBrush()->SetColor(MakeColor(highlightInfo.m_arc, 0, true));
            target.Target()->FillGeometry(spCompBar, target.FillBrush());
            target.Target()->DrawGeometry(spCompBar, target.OutlineBrush(), mx.stroke * 0.66f, target.BevelStrokeStyle());
        }

        if (target.OutlineBrush2())
        {
            target.Target()->DrawGeometry(highlightInfo.m_geometry, target.OutlineBrush2(), mx.stroke * 3.5f, target.RoundedStrokeStyle());

            if (highlightInfo.m_arc.m_node)
            {
                target.FillBrush()->SetColor(MakeColor(highlightInfo.m_arc, highlightInfo.m_depth, true));
                target.Target()->FillGeometry(highlightInfo.m_geometry, target.FillBrush());
                target.Target()->DrawGeometry(highlightInfo.m_geometry, target.LineBrush(), mx.stroke);
                if (highlightInfo.m_show_names)
                    DrawArcText(target, highlightInfo.m_arc, highlightInfo.m_arctext_radius);
            }
        }

        target.Target()->DrawGeometry(highlightInfo.m_geometry, target.OutlineBrush(), mx.stroke * 2.5f, target.BevelStrokeStyle());
    }
}

void Sunburst::RenderRingsInternal(DirectHwndRenderTarget& target, const SunburstMetrics& mx, const std::shared_ptr<Node>& highlight, bool files, HighlightInfo& highlightInfo)
{
    ID2D1RenderTarget* pTarget = target.Target();

    assert(m_bounds.left < m_bounds.right);
    assert(m_bounds.top < m_bounds.bottom);
    assert(m_start_angles.size() == m_roots.size());

    // FUTURE: Direct2D documentation recommends caching a bitmap for performance,
    // instead of caching geometries.  The highlight can be calculated on the fly
    // as needed.

    SPI<ID2D1Layer> spLayer;
    D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(
        m_bounds, 0, D2D1_ANTIALIAS_MODE_ALIASED, D2D1::Matrix3x2F::Identity(), files ? 0.60f : 1.0f);
    pTarget->CreateLayer(&spLayer);
    pTarget->PushLayer(layerParams, spLayer);

    ID2D1SolidColorBrush* pLineBrush = target.LineBrush();
    ID2D1SolidColorBrush* pFileLineBrush = target.FileLineBrush();
    ID2D1SolidColorBrush* pFillBrush = target.FillBrush();

    bool show_names = (g_show_names && target.DWriteFactory());

    // Outer boundary outline.

#ifdef USE_CHART_OUTLINE
    if (!files)
    {
        D2D1_ELLIPSE ellipse;
        ellipse.point = m_center;
        ellipse.radiusX = mx.boundary_radius;
        ellipse.radiusY = mx.boundary_radius;

        pFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LightGray));
        pTarget->DrawEllipse(ellipse, pFillBrush, mx.stroke);
    }
#endif

    // Center circle or pie slices.

    if (!files)
    {
        D2D1_ELLIPSE ellipse;
        ellipse.point = m_center;
        ellipse.radiusX = mx.center_radius;
        ellipse.radiusY = mx.center_radius;

        SPI<ID2D1EllipseGeometry> spCircle;
        if (SUCCEEDED(target.Factory()->CreateEllipseGeometry(ellipse, &spCircle)))
        {
            assert(m_free_angles.empty() || m_free_angles.size() == m_roots.size());

            if (m_roots.size() > 1 || (m_roots.size() == 1 && m_roots[0]->GetFreeSpace()))
            {
                FLOAT end = m_start_angles[0];
                for (size_t ii = m_roots.size(); ii--;)
                {
                    const FLOAT start = m_start_angles[ii];
                    const FLOAT free = m_free_angles.empty() ? end : m_free_angles[ii];

                    const bool isHighlight = is_highlight(highlight, m_roots[ii]);

                    SPI<ID2D1Geometry> spGeometry;
                    if (SUCCEEDED(MakeArcGeometry(target, start, free, 0.0f, mx.center_radius, &spGeometry)))
                    {
                        pFillBrush->SetColor(MakeRootColor(isHighlight, false));
                        pTarget->FillGeometry(spGeometry, pFillBrush);
                        spGeometry.Release();
                    }
                    if (g_show_free_space &&
                        (free != end) &&
                        SUCCEEDED(MakeArcGeometry(target, free, end, 0.0f, mx.center_radius, &spGeometry)))
                    {
                        pFillBrush->SetColor(MakeRootColor(isHighlight, true));
                        pTarget->FillGeometry(spGeometry, pFillBrush);
                        spGeometry.Release();
                    }

                    end = start;
                }

                if (m_roots.size() > 1)
                {
                    FLOAT prev = -1234.0f;
                    pFillBrush->SetColor(MakeRootColor(false, true));
                    for (size_t ii = m_roots.size(); ii--;)
                    {
                        const FLOAT angle = (m_free_angles.empty() ? m_start_angles[ii] : m_free_angles[ii]) + c_rotation;
                        if (prev != angle)
                            pTarget->DrawLine(m_center, MakePoint(m_center, mx.center_radius, angle), pFillBrush, mx.stroke);
                        prev = angle;
                    }
                }
            }
            else
            {
                const bool isHighlight = (m_roots.size() && is_highlight(highlight, m_roots[0]));
                pFillBrush->SetColor(MakeRootColor(isHighlight, false));
                pTarget->FillGeometry(spCircle, pFillBrush);
            }

            pTarget->DrawGeometry(spCircle, pLineBrush, mx.stroke);
        }

        if (highlight)
        {
            FLOAT end = m_start_angles[0];
            for (size_t ii = m_roots.size(); ii--;)
            {
                const FLOAT start = m_start_angles[ii];

                if (highlight == m_roots[ii])
                {
                    highlightInfo.m_arc.m_node.reset();
                    highlightInfo.m_geometry.Release();
                    if (SUCCEEDED(MakeArcGeometry(target, start, end, 0.0f, mx.center_radius, &highlightInfo.m_geometry)))
                        break;
                }

                end = start;
            }
        }
    }

    // Rings.

    m_min_arc_text_len = FLOAT(m_dpiWithTextScaling.Scale(20));

    size_t depth;
    FLOAT inner_radius = mx.center_radius;
    for (depth = 0; depth < m_rings.size(); ++depth)
    {
        const FLOAT thickness = mx.get_thickness(depth);
        if (thickness <= 0.0f)
            break;

        if (thickness < target.ArcFontSize() + m_dpiWithTextScaling.Scale(4))
            show_names = false;

        const FLOAT outer_radius = inner_radius + thickness;
        if (outer_radius > mx.max_radius)
            break;

        const FLOAT arctext_radius = outer_radius - target.ArcFontSize();

        for (const auto arc : m_rings[depth])
        {
            const bool isFile = !!arc.m_node->AsFile();
            if (isFile != files)
                continue;
            if (isFile && !arc.m_node->IsParentFinished())
                continue;

            SPI<ID2D1Geometry> spGeometry;
            const bool isHighlight = is_highlight(highlight, arc.m_node);
            if (SUCCEEDED(MakeArcGeometry(target, arc.m_start, arc.m_end, inner_radius, outer_radius, &spGeometry)))
            {
                pFillBrush->SetColor(MakeColor(arc, depth, isHighlight));

                pTarget->FillGeometry(spGeometry, pFillBrush);
                pTarget->DrawGeometry(spGeometry, isFile ? pFileLineBrush : pLineBrush, mx.stroke);

                if (show_names)
                    DrawArcText(target, arc, arctext_radius);

                if (isHighlight)
                {
                    highlightInfo.m_arc = arc;
                    highlightInfo.m_geometry.Set(spGeometry);
                    highlightInfo.m_depth = depth;
                    highlightInfo.m_arctext_radius = arctext_radius;
                    highlightInfo.m_show_names = show_names;
                }
            }
        }

        inner_radius = outer_radius;
    }

    // "More" indicators.

    if (depth < m_rings.size())
    {
        inner_radius += mx.margin;
        const FLOAT outer_radius = inner_radius + mx.indicator_thickness;

        for (const auto arc : m_rings[depth])
        {
            const bool isFile = !!arc.m_node->AsFile();
            if (isFile != files)
                continue;
            if (isFile && !arc.m_node->IsParentFinished())
                continue;

            SPI<ID2D1Geometry> spGeometry;
            if (SUCCEEDED(MakeArcGeometry(target, arc.m_start, arc.m_end, inner_radius, outer_radius, &spGeometry)))
            {
                pFillBrush->SetColor(D2D1::ColorF(isFile ? 0x999999 : 0x555555));
                pTarget->FillGeometry(spGeometry, pFillBrush);

                pFillBrush->SetColor(D2D1::ColorF(GetBackColor(m_dark_mode)));
                pTarget->DrawGeometry(spGeometry, pFillBrush, mx.stroke / 2);
            }
        }

    }

    pTarget->PopLayer();
}

void Sunburst::FormatSize(const ULONGLONG size, std::wstring& text, std::wstring& units, int places)
{
    ::FormatSize(size, text, units, m_units, places);
}

std::shared_ptr<Node> Sunburst::HitTest(const SunburstMetrics& mx, POINT pt, bool* is_free)
{
    const FLOAT angle = FindAngle(m_center, FLOAT(pt.x), FLOAT(pt.y));
    const FLOAT xdelta = (pt.x - m_center.x);
    const FLOAT ydelta = (pt.y - m_center.y);
    const FLOAT radius = sqrt((xdelta * xdelta) + (ydelta * ydelta));

    const bool use_parent = (radius <= mx.center_radius);
    if (use_parent)
    {
        for (size_t ii = m_start_angles.size(); ii--;)
        {
            if (m_start_angles[ii] <= angle)
            {
                if (is_free)
                    *is_free = (m_free_angles.size() && m_roots[ii]->GetFreeSpace() && angle > m_free_angles[ii]);
                return m_roots[ii];
            }
        }
    }
    else
    {
        FLOAT inner_radius = mx.center_radius;

        for (size_t depth = 0; depth < m_rings.size(); ++depth)
        {
            const FLOAT thickness = mx.get_thickness(depth);
            if (thickness <= 0.0f)
                break;

            FLOAT outer_radius = inner_radius + thickness;
            if (outer_radius > mx.max_radius)
            {
                inner_radius += mx.margin;
                outer_radius = inner_radius + mx.indicator_thickness;
            }

            if (inner_radius < radius && radius <= outer_radius)
            {
                for (const auto arc : m_rings[depth])
                {
                    if (arc.m_start <= angle && angle < arc.m_end)
                        return arc.m_node;
                    if (arc.m_start <= angle + 360.0f && angle + 360.0f < arc.m_end)
                        return arc.m_node;
                }
                break;
            }

            if (outer_radius > mx.max_radius)
                break;

            inner_radius = outer_radius;
        }
    }

    return nullptr;
}

bool Sunburst::OnDpiChanged(const DpiScaler& dpi)
{
    const bool changed = !m_dpi.IsDpiEqual(dpi);

    m_dpi.OnDpiChanged(dpi);
    m_dpiWithTextScaling.OnDpiChanged(dpi, true);

    m_rings.clear();
    m_start_angles.clear();
    m_free_angles.clear();

    return changed;
}

