// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "sunburst.h"
#include "data.h"
#include "TextOnPath/PathTextRenderer.h"
#include <cmath>

static ID2D1Factory* s_pD2DFactory = nullptr;
static IDWriteFactory2* s_pDWriteFactory = nullptr;

constexpr FLOAT M_PI = 3.14159265358979323846f;
#ifdef USE_FIXED_CENTER_RADIUS
constexpr int c_centerRadius = 50;
#elif defined(USE_LARGER_RINGS)
constexpr FLOAT c_centerRadiusRatio = 0.25f;
constexpr int c_centerRadiusMin = 50;
#else
constexpr FLOAT c_centerRadiusRatio = 0.25f;
constexpr int c_centerRadiusMin = 50;
#endif
constexpr FLOAT c_rotation = -90.0f;

constexpr WCHAR c_fontface[] = TEXT("Segoe UI");
constexpr FLOAT c_fontsize = 10.0f;
constexpr FLOAT c_centerfontsize = 12.0f;
constexpr FLOAT c_arcfontsize = 8.0f;

#ifdef USE_MIN_ARC_LENGTH
# define _PASS_MIN_ARC_LENGTH   , outer_radius, min_arc
constexpr FLOAT c_minArc = 2.5f;
#else
# define _PASS_MIN_ARC_LENGTH
constexpr FLOAT c_minAngle = 1.1f;
#endif

#ifdef USE_PROPORTIONAL_RING_THICKNESS
constexpr int c_max_thickness = 45;
#else
constexpr int c_thickness = 25;
constexpr int c_retrograde = 1;
constexpr int c_retrograde_depths = 10;
#endif

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
    const FLOAT validHue = std::min<FLOAT>(std::max<FLOAT>(0, h), c_maxHue);
    const FLOAT validSat = std::min<FLOAT>(std::max<FLOAT>(0, s), c_maxSat);
    const FLOAT validLum = std::min<FLOAT>(std::max<FLOAT>(0, l), c_maxLum);

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

//----------------------------------------------------------------------------
// DirectHwndRenderTarget.

#define ERRJMP(expr)      do { hr = (expr); assert(SUCCEEDED(hr)); if (FAILED(hr)) goto LError; } while (false)
#define ERRRET(expr)      do { hr = (expr); assert(SUCCEEDED(hr)); if (FAILED(hr)) return hr; } while (false)

HRESULT DirectHwndRenderTarget::Resources::Init(HWND hwnd, const D2D1_SIZE_U& size, const DpiScaler& dpi)
{
    HRESULT hr = S_OK;

    if (!m_spFactory && !GetD2DFactory(&m_spFactory))
        return E_UNEXPECTED;
    if (!m_spDWriteFactory && !GetDWriteFactory(&m_spDWriteFactory))
        return E_UNEXPECTED;

    ERRRET(m_spFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &m_spTarget));

    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_spLineBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(0x444444, 0.5f), &m_spFileLineBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_spFillBrush));
    ERRRET(m_spTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_spTextBrush));

    const auto rstyle = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
    ERRRET(m_spFactory->CreateStrokeStyle(rstyle, nullptr, 0, &m_spRoundedStroke));

    const auto bstyle = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_BEVEL);
    ERRRET(m_spFactory->CreateStrokeStyle(bstyle, nullptr, 0, &m_spBevelStroke));

    SPI<IDWriteRenderingParams> spRenderingParams;
    ERRRET(m_spDWriteFactory->CreateRenderingParams(&spRenderingParams));

    // Custom text rendering param object is created that uses all default
    // values except for the rendering mode which is now set to outline.  The
    // outline mode is much faster in this case as every time text is relaid
    // out on the path, it is rasterized as geometry.  This saves the extra
    // step of trying to find the text bitmaps in the font cache and then
    // repopulating the cache with the new ones.  Since the text may rotate
    // differently from frame to frame, new glyph bitmaps would be generated
    // often anyway.
    ERRRET(m_spDWriteFactory->CreateCustomRenderingParams(
            spRenderingParams->GetGamma(),
            spRenderingParams->GetEnhancedContrast(),
            spRenderingParams->GetClearTypeLevel(),
            spRenderingParams->GetPixelGeometry(),
            DWRITE_RENDERING_MODE_NATURAL,
            &m_spRenderingParams));

    m_fontSize = FLOAT(-dpi.PointSizeToHeight(c_fontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_fontSize,
            TEXT("en-US"),
            &m_spTextFormat));

    m_centerFontSize = FLOAT(-dpi.PointSizeToHeight(c_centerfontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_centerFontSize,
            TEXT("en-US"),
            &m_spCenterTextFormat));

    m_arcFontSize = FLOAT(-dpi.PointSizeToHeight(c_arcfontsize));
    ERRRET(m_spDWriteFactory->CreateTextFormat(
            c_fontface,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            m_arcFontSize,
            TEXT("en-US"),
            &m_spArcTextFormat));

    ERRRET(m_spContext.HrQuery(m_spTarget));
    m_spContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
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

HRESULT DirectHwndRenderTarget::CreateDeviceResources(const HWND hwnd, const DpiScaler& dpi)
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

    HRESULT hr = m_resources->Init(m_hwnd, size, dpi);
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

// TODO: Path ellipsis; probably using a custom renderer is most efficient for
// finding whether/where to inject an ellipsis, and how much text to discard.
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

struct SunburstMetrics
{
    SunburstMetrics(const DpiScaler& dpi, const D2D1_RECT_F& bounds)
    : stroke(FLOAT(dpi.Scale(1)))
    , margin(FLOAT(dpi.Scale(5)))
    , indicator_thickness(FLOAT(dpi.Scale(4)))
    , boundary_radius(FLOAT(std::min<LONG>(LONG(bounds.right - bounds.left), LONG(bounds.bottom - bounds.top)) / 2 - margin))
#ifdef USE_FIXED_CENTER_RADIUS
    , center_radius(FLOAT(dpi.Scale(c_centerRadius)))
#else
    , center_radius(std::max<FLOAT>(boundary_radius * c_centerRadiusRatio, FLOAT(dpi.Scale(c_centerRadiusMin))))
#endif
    , max_radius(boundary_radius - (margin + indicator_thickness + margin))
    , range_radius(max_radius - center_radius)
#ifdef USE_MIN_ARC_LENGTH
    , min_arc(dpi.ScaleF(c_minArc))
#endif
#ifndef USE_PROPORTIONAL_RING_THICKNESS
    , thickness(FLOAT(dpi.Scale(c_thickness)))
    , retrograde(FLOAT(dpi.Scale(c_retrograde)))
#endif
    {
#ifdef USE_PROPORTIONAL_RING_THICKNESS
#ifdef USE_LARGER_RINGS
        const FLOAT coefficient = 0.18f;
        FLOAT thickness = std::min<FLOAT>(boundary_radius * coefficient, FLOAT(dpi.Scale(c_max_thickness)));
        FLOAT retrograde = 0.25f;
        for (size_t ii = 0; ii < _countof(thicknesses); ++ii)
        {
            thicknesses[ii] = FLOAT(LONG(thickness));
            thickness -= thickness * retrograde;
            retrograde *= 0.666f;
        }
#else
        FLOAT coefficient = 0.48f;
        const FLOAT retrograde = 0.87f;
        for (size_t ii = 0; ii < _countof(thicknesses); ++ii)
        {
            const FLOAT thickness = center_radius * coefficient;
            thicknesses[ii] = thickness;
            coefficient *= retrograde;
        }
#endif
#endif
    }

    FLOAT get_thickness(size_t depth) const
    {
#ifdef USE_PROPORTIONAL_RING_THICKNESS
        return (depth < _countof(thicknesses)) ? thicknesses[depth] : 0.0f;
#else
        return thickness - (retrograde * std::min<size_t>(depth, c_retrograde_depths));
#endif
    }

    const FLOAT stroke;
    const FLOAT margin;
    const FLOAT indicator_thickness;
    const FLOAT boundary_radius;
    const FLOAT center_radius;
    const FLOAT max_radius;
    const FLOAT range_radius;
#ifdef USE_MIN_ARC_LENGTH
    const FLOAT min_arc;
#endif

private:
#ifdef USE_PROPORTIONAL_RING_THICKNESS
    FLOAT thicknesses[10];
#else
    const FLOAT thickness;
    const FLOAT retrograde;
#endif
};

//----------------------------------------------------------------------------
// Sunburst.

Sunburst::Sunburst()
{
}

Sunburst::~Sunburst()
{
}

bool Sunburst::SetBounds(const D2D1_RECT_F& rect)
{
    static_assert(sizeof(m_bounds) == sizeof(rect), "data size mismatch");
    const bool changed = !!memcmp(&m_bounds, &rect, sizeof(rect));

    m_bounds = rect;
    m_center.x = floor((rect.left + rect.right) / 2.0f);
    m_center.y = floor((rect.top + rect.bottom) / 2.0f);

    return changed;
}

#ifdef USE_MIN_ARC_LENGTH
void Sunburst::MakeArc(std::vector<Arc>& arcs, FLOAT outer_radius, const FLOAT min_arc, const std::shared_ptr<Node>& node, ULONGLONG size, double& sweep, double total, float start, float span, double convert)
#else
void Sunburst::MakeArc(std::vector<Arc>& arcs, const std::shared_ptr<Node>& node, ULONGLONG size, double& sweep, double total, float start, float span, double convert)
#endif
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

// NOTE: USE_PROPORTIONAL_RING_THICKNESS interacts poorly with this.  The
// async scan can cause the max depth to jitter, which rescales the whole
// chart, and the arc filtering mechanisms can vacillate disorientingly.
#ifdef USE_MIN_ARC_LENGTH
    if (ArcLength(arc.m_end - arc.m_start, outer_radius) >= min_arc)
#else
    if (arc.m_end - arc.m_start >= c_minAngle)
#endif
    {
        arc.m_node = node;
        arcs.emplace_back(std::move(arc));
    }
}

void Sunburst::BuildRings(const std::vector<std::shared_ptr<DirNode>>& _roots)
{
#ifdef USE_MIN_ARC_LENGTH
    SunburstMetrics mx(m_dpi, m_bounds);
#endif

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
        double grand_total = 0;
        for (const auto dir : roots)
        {
            const double size = double(dir->GetSize());
            std::shared_ptr<FreeSpaceNode> free = g_show_free_space ? dir->GetFreeSpace() : nullptr;
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

            if (g_show_free_space)
            {
                std::shared_ptr<FreeSpaceNode> free = m_roots[ii]->GetFreeSpace();
                if (free)
                {
                    const float angle = float((sweep - free->GetFreeSize()) * 360 / grand_total);
                    m_free_angles.emplace_back(angle);
                }
            }
        }
    }

    m_rings.emplace_back();

    std::vector<Arc>& arcs = m_rings.back();

#ifdef USE_MIN_ARC_LENGTH
    FLOAT outer_radius = mx.center_radius + mx.get_thickness(0);
    const FLOAT min_arc = mx.min_arc;
#endif

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
            MakeArc(arcs _PASS_MIN_ARC_LENGTH, std::static_pointer_cast<Node>(dir), dir->GetSize(), sweep, consumed, start, span, convert);
        for (const auto file : files)
            MakeArc(arcs _PASS_MIN_ARC_LENGTH, std::static_pointer_cast<Node>(file), file->GetSize(), sweep, consumed, start, span, convert);
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

    while (m_rings.size() <= 20)
    {
#ifdef USE_MIN_ARC_LENGTH
        outer_radius += mx.get_thickness(m_rings.size() + 1);
#endif

        std::vector<Arc> arcs = NextRing(m_rings.back() _PASS_MIN_ARC_LENGTH);
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

#ifdef USE_MIN_ARC_LENGTH
std::vector<Sunburst::Arc> Sunburst::NextRing(const std::vector<Arc>& parent_ring, const FLOAT outer_radius, const FLOAT min_arc)
#else
std::vector<Sunburst::Arc> Sunburst::NextRing(const std::vector<Arc>& parent_ring)
#endif
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
                MakeArc(arcs _PASS_MIN_ARC_LENGTH, std::static_pointer_cast<Node>(dir), dir->GetSize(), sweep, range, start, span);
            for (const auto file : files)
                MakeArc(arcs _PASS_MIN_ARC_LENGTH, std::static_pointer_cast<Node>(file), file->GetSize(), sweep, range, start, span);

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

COLORREF FixLuminance(COLORREF cr)
{
    // Luminance in the blue/purple range of hue in the HSL color space is
    // disproportionate to the rest of the hue range.  This attempts to
    // compensate -- primarily so that text can have legible contrast.
    const float lo = 180.0f;
    const float hi = 300.0f;
    const float gravity = c_maxLum * 0.65f;
    HSLColorType hsl(cr);
    if (hsl.h >= lo && hsl.h <= hi)
    {
        constexpr float pi = 3.14159f;
        const float hue_cos = cos((hsl.h - lo) * 2 * pi / (hi - lo));
        const float transform = (1.0f - hue_cos) / 2;
        if (hsl.l < gravity)
            hsl.l += transform * (gravity - hsl.l) * 0.8f;
        else
            hsl.l += transform * (gravity - hsl.l) * 0.6f;
        return hsl.ToRGB();
    }
    return cr;
}

D2D1_COLOR_F Sunburst::MakeColor(const Arc& arc, size_t depth, bool highlight)
{
    if (arc.m_node->AsFreeSpace())
        return D2D1::ColorF(highlight ? D2D1::ColorF::LightSteelBlue : D2D1::ColorF::WhiteSmoke);

    const bool file = !!arc.m_node->AsFile();
    if (file)
        depth = 0;
    else if (arc.m_node->AsDir() && arc.m_node->AsDir()->IsHidden())
        return D2D1::ColorF(0xB8B8B8);

    if (!is_root_finished(arc.m_node))
        return D2D1::ColorF(highlight ? 0x3078F8 : 0xB8B8B8);

    if (!g_rainbow)
        return D2D1::ColorF(highlight ? 0x3078F8 : 0x6495ED);
// TODO: Option to color based on size vs total?

    const FLOAT angle = (arc.m_start + arc.m_end) / 2.0f;

// TODO: Dark theme.
    HSLColorType hsl;
    hsl.h = angle * c_maxHue / 360;
    hsl.s = highlight ? c_maxSat : (c_maxSat * (file ? 0.7f : 0.95f)) - (FLOAT(depth) * (c_maxSat / 25));
    hsl.l = highlight ? c_maxLum*3/5 : (c_maxLum * (file ? 0.6f : 0.4f)) + (FLOAT(depth) * (c_maxLum / 30));

    const COLORREF rgb = FixLuminance(hsl.ToRGB());

    D2D1_COLOR_F color;
    color.r = FLOAT(GetRValue(rgb)) / 255;
    color.g = FLOAT(GetGValue(rgb)) / 255;
    color.b = FLOAT(GetBValue(rgb)) / 255;
    color.a = 1.0f;

    return color;
}

D2D1_COLOR_F Sunburst::MakeRootColor(bool highlight, bool free)
{
    if (highlight)
        return D2D1::ColorF(free ? 0xD0E4FE : D2D1::ColorF::LightSteelBlue);
    else
        return D2D1::ColorF(free ? 0xD8D8D8 : 0xB0B0B0);
}

inline D2D1_ARC_SIZE GetArcSize(FLOAT start, FLOAT end)
{
    return (end - start > 180.0f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
}

bool Sunburst::MakeArcGeometry(DirectHwndRenderTarget& target, FLOAT start, FLOAT end, const FLOAT inner_radius, const FLOAT outer_radius, ID2D1Geometry** ppGeometry)
{
    const bool has_line = (start != end) || (inner_radius > 0.0f);
    if (end <= start)
        end += 360.0f;

    // When start and end points of an arc are identical, D2D gets confused
    // where to draw the arc, since it's a full circle.  Split into two arcs.
    const bool split = (start == end || end - start > 270.0f);

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

            D2D1_ARC_SEGMENT outer;
            outer.size = D2D1::SizeF(outer_radius, outer_radius);
            outer.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
            if (split)
            {
                const FLOAT mid = start + std::min<FLOAT>(359.0f, end - 1.0f - start);
                outer.point = MakePoint(m_center, outer_radius, mid);
                outer.rotationAngle = start;
                outer.arcSize = GetArcSize(start, mid);
                spSink->AddArc(outer);
                outer.point = outer_end_point;
                outer.rotationAngle = mid;
                outer.arcSize = GetArcSize(mid, end);
            }
            else
            {
                outer.point = outer_end_point;
                outer.rotationAngle = start;
                outer.arcSize = GetArcSize(start, end);
            }
            spSink->AddArc(outer);

            if (has_line)
                spSink->AddLine(inner_end_point);

            if (inner_radius > 0.0f)
            {
                D2D1_ARC_SEGMENT inner;
                inner.size = D2D1::SizeF(inner_radius, inner_radius);
                inner.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                if (split)
                {
                    const FLOAT mid = start + std::min<FLOAT>(359.0f, end - 1.0f - start);
                    inner.point = MakePoint(m_center, inner_radius, mid);
                    inner.rotationAngle = mid;
                    inner.arcSize = GetArcSize(mid, end);
                    spSink->AddArc(inner);
                    inner.point = inner_start_point;
                    inner.rotationAngle = end;
                    inner.arcSize = GetArcSize(start, mid);
                }
                else
                {
                    inner.point = inner_start_point;
                    inner.rotationAngle = end;
                    inner.arcSize = GetArcSize(start, end);
                }
                spSink->AddArc(inner);
            }

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

void Sunburst::DrawArcText(DirectHwndRenderTarget& target, const Arc& arc, FLOAT radius)
{
    if (ArcLength(arc.m_end - arc.m_start, radius) < m_min_arc_text_len)
        return;

    IDWriteFactory* pFactory = target.DWriteFactory();
    if (!pFactory)
        return;

    std::wstring text;
    text.append(TEXT(" "));
    text.append(arc.m_node->GetName());
    text.append(TEXT(" "));

    SPI<IDWriteTextLayout> spTextLayout;
    if (FAILED(pFactory->CreateTextLayout(text.c_str(), UINT32(text.length()), target.ArcTextFormat(), m_bounds.right - m_bounds.left, m_bounds.bottom - m_bounds.top, &spTextLayout)))
        return;

    const FLOAT start = arc.m_start + c_rotation;
    const FLOAT end = arc.m_end + c_rotation;

    D2D1_POINT_2F outer_start_point = MakePoint(m_center, radius, start);
    D2D1_POINT_2F outer_end_point = MakePoint(m_center, radius, end);

    SPI<ID2D1PathGeometry> spGeometry;
    if (SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
            spSink->BeginFigure(outer_start_point, D2D1_FIGURE_BEGIN_HOLLOW);

            D2D1_ARC_SEGMENT outer;
            outer.size = D2D1::SizeF(radius, radius);
            outer.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
            outer.point = outer_end_point;
            outer.rotationAngle = start;
            outer.arcSize = GetArcSize(start, end);
            spSink->AddArc(outer);

            spSink->EndFigure(D2D1_FIGURE_END_OPEN);
            spSink->Close();
        }

        PathTextDrawingContext context;
        context.brush.Set(target.LineBrush());
        context.geometry.Set(spGeometry);
        context.d2DContext.Set(target.Context());

        if (target.ArcTextRenderer()->TestFit(&context, spTextLayout))
            spTextLayout->Draw(&context, target.ArcTextRenderer(), 0, 0);
    }
}

void Sunburst::RenderRings(DirectHwndRenderTarget& target, const std::shared_ptr<Node>& highlight)
{
    SPI<ID2D1Geometry> spHighlight;

    // Two passes, so files are "beneath" everything else.

    SunburstMetrics mx(m_dpi, m_bounds);
    RenderRingsInternal(target, mx, highlight, true/*files*/, spHighlight);
    RenderRingsInternal(target, mx, highlight, false/*files*/, spHighlight);

    // Hover highlight.

    if (spHighlight && (!highlight->GetParent() || is_root_finished(highlight)))
        target.Target()->DrawGeometry(spHighlight, target.LineBrush(), mx.stroke * 2.0f, target.BevelStrokeStyle());
}

void Sunburst::RenderRingsInternal(DirectHwndRenderTarget& target, const SunburstMetrics& mx, const std::shared_ptr<Node>& highlight, bool files, SPI<ID2D1Geometry>& spHighlight)
{
    ID2D1RenderTarget* pTarget = target.Target();

    assert(m_bounds.left < m_bounds.right);
    assert(m_bounds.top < m_bounds.bottom);

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
            if (m_roots.size() &&
                (m_roots.size() > 1 || m_roots[0]->GetFreeSpace()) &&
                m_start_angles.size() == m_roots.size())
            {
                assert(m_free_angles.empty() || m_free_angles.size() == m_roots.size());

                FLOAT end = m_start_angles[0];
                for (size_t ii = m_roots.size(); ii--;)
                {
                    const FLOAT start = m_start_angles[ii];
                    const FLOAT free = m_free_angles.empty() ? end : m_free_angles[ii];

                    const bool isHighlight = (highlight == m_roots[ii]);

                    SPI<ID2D1Geometry> spGeometry;
                    if (SUCCEEDED(MakeArcGeometry(target, start, free, 0.0f, mx.center_radius, &spGeometry)))
                    {
                        pFillBrush->SetColor(MakeRootColor(isHighlight, false));
                        pTarget->FillGeometry(spGeometry, pFillBrush);
                        spGeometry.Release();
                    }
                    if (g_show_free_space && SUCCEEDED(MakeArcGeometry(target, free, end, 0.0f, mx.center_radius, &spGeometry)))
                    {
                        pFillBrush->SetColor(MakeRootColor(isHighlight, true));
                        pTarget->FillGeometry(spGeometry, pFillBrush);
                        spGeometry.Release();
                    }

                    end = start;
                }

            }
            else
            {
                const bool isHighlight = (m_roots.size() && is_highlight(highlight, m_roots[0]));
                pFillBrush->SetColor(MakeRootColor(isHighlight, false));
                pTarget->FillGeometry(spCircle, pFillBrush);
            }

            pTarget->DrawGeometry(spCircle, pLineBrush, mx.stroke);

            if (!g_show_free_space && m_roots.size() > 1)
            {
                FLOAT prev = -1234.0f;
                pFillBrush->SetColor(MakeRootColor(false, true));
                for (size_t ii = m_roots.size(); ii--;)
                {
                    const FLOAT start = m_start_angles[ii] + c_rotation;
                    if (prev != start)
                        pTarget->DrawLine(m_center, MakePoint(m_center, mx.center_radius, start), pFillBrush, mx.stroke);
                    prev = start;
                }
            }
        }

        if (highlight)
        {
            FLOAT end = m_start_angles[0];
            for (size_t ii = m_roots.size(); ii--;)
            {
                const FLOAT start = m_start_angles[ii];

                if (highlight == m_roots[ii])
                {
                    spHighlight.Release();
                    if (SUCCEEDED(MakeArcGeometry(target, start, end, 0.0f, mx.center_radius, &spHighlight)))
                        break;
                }

                end = start;
            }
        }
    }

    // Rings.

    m_min_arc_text_len = FLOAT(m_dpi.Scale(20));

    size_t depth;
    FLOAT inner_radius = mx.center_radius;
    for (depth = 0; depth < m_rings.size(); ++depth)
    {
        const FLOAT thickness = mx.get_thickness(depth);
        if (thickness <= 0.0f)
            break;

        if (thickness < target.ArcFontSize() + m_dpi.Scale(4))
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
            if (SUCCEEDED(MakeArcGeometry(target, arc.m_start, arc.m_end, inner_radius, outer_radius, &spGeometry)))
            {
                const bool isHighlight = is_highlight(highlight, arc.m_node);

                pFillBrush->SetColor(MakeColor(arc, depth, isHighlight));

                pTarget->FillGeometry(spGeometry, pFillBrush);
                pTarget->DrawGeometry(spGeometry, isFile ? pFileLineBrush : pLineBrush, mx.stroke);

                if (show_names)
                    DrawArcText(target, arc, arctext_radius);

                if (isHighlight)
                    spHighlight.Set(spGeometry);
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

                pFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
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

std::shared_ptr<Node> Sunburst::HitTest(POINT pt, bool* is_free)
{
    const FLOAT angle = FindAngle(m_center, FLOAT(pt.x), FLOAT(pt.y));
    const FLOAT xdelta = (pt.x - m_center.x);
    const FLOAT ydelta = (pt.y - m_center.y);
    const FLOAT radius = sqrt((xdelta * xdelta) + (ydelta * ydelta));

    SunburstMetrics mx(m_dpi, m_bounds);

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

    m_rings.clear();
    m_start_angles.clear();
    m_free_angles.clear();

    return changed;
}

