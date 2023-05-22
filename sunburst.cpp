// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "sunburst.h"
#include "data.h"
#include <assert.h>
#include <cmath>

static ID2D1Factory* s_pD2DFactory = nullptr;

constexpr FLOAT M_PI = 3.14159265358979323846f;
constexpr FLOAT c_minAngle = 1.0f;
constexpr int c_centerRadius = 50;

HRESULT InitializeD2D()
{
    CoInitialize(0);

    return D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), 0, reinterpret_cast<void**>(&s_pD2DFactory));
// TODO: CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&s_pWICFactory));
}

static bool GetD2DFactory(ID2D1Factory** ppFactory)
{
    ID2D1Factory* pFactory = s_pD2DFactory;
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

DirectHwndRenderTarget::DirectHwndRenderTarget()
{
}

DirectHwndRenderTarget::~DirectHwndRenderTarget()
{
    assert(!m_hwnd);
}

HRESULT DirectHwndRenderTarget::CreateDeviceResources(const HWND hwnd)
{
    assert(!m_hwnd || hwnd == m_hwnd);

    if (hwnd == m_hwnd && m_pTarget)
        return S_OK;
    if (!m_pFactory && !GetD2DFactory(&m_pFactory))
        return E_UNEXPECTED;

    m_hwnd = hwnd;
    ReleaseI(m_pTarget);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_pTarget);
    if (FAILED(hr))
    {
LError:
        ReleaseI(m_pTarget);
        return hr;
    }

    hr = m_pTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_pLineBrush);
    if (FAILED(hr))
        goto LError;

    hr = m_pTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_pFillBrush);
    if (FAILED(hr))
        goto LError;

    return S_OK;
}

HRESULT DirectHwndRenderTarget::ResizeDeviceResources()
{
    if (!m_hwnd || !m_pTarget)
        return S_OK;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_pTarget->Resize(size);
    if (FAILED(hr))
    {
        ReleaseI(m_pTarget);
        return hr;
    }

    return S_OK;
}

void DirectHwndRenderTarget::ReleaseDeviceResources()
{
    ReleaseI(m_pLineBrush);
    ReleaseI(m_pFillBrush);
    ReleaseI(m_pTarget);
    ReleaseI(m_pFactory);
    m_hwnd = 0;
}

//----------------------------------------------------------------------------
// SunburstMetrics.

struct SunburstMetrics
{
    SunburstMetrics(const DpiScaler& dpi, const D2D1_RECT_F& bounds, size_t max_depth)
    : stroke(FLOAT(dpi.Scale(1)))
    , margin(FLOAT(dpi.Scale(4)))
    , indicator_thickness(FLOAT(dpi.Scale(3)))
    , center_radius(FLOAT(dpi.Scale(c_centerRadius)))
    , boundary_radius(std::min<FLOAT>(bounds.right - bounds.left, bounds.bottom - bounds.top) / 2 - margin)
    , max_radius(boundary_radius - (margin + indicator_thickness + margin))
    , range_radius(max_radius - center_radius)
    {
        static constexpr float c_rings[] =
        {
            0.20f,
            0.17f,
            0.14f,
            0.12f,
            0.10f,
            0.08f,
            0.065f,
            0.05f,
            0.04f,
            0.035f,
        };
        static_assert(_countof(c_rings) == _countof(thicknesses), "array size mismatch");
        static_assert(c_rings[0] + c_rings[1] + c_rings[2] + c_rings[3] +
                      c_rings[4] + c_rings[5] + c_rings[6] + c_rings[7] +
                      c_rings[8] + c_rings[9] > 0.999f, "must total 100%");
        static_assert(c_rings[0] + c_rings[1] + c_rings[2] + c_rings[3] +
                      c_rings[4] + c_rings[5] + c_rings[6] + c_rings[7] +
                      c_rings[8] + c_rings[9] < 1.001f, "must total 100%");
        FLOAT scale = 0;
        if (max_depth > _countof(c_rings))
            max_depth = _countof(c_rings);
        for (size_t ii = 0; ii < std::max<size_t>(max_depth, 5); ++ii)
            scale += c_rings[ii];
        for (size_t ii = 0; ii < max_depth; ++ii)
            thicknesses[ii] = c_rings[ii] * range_radius / scale;
        for (size_t ii = max_depth; ii < _countof(c_rings); ++ii)
            thicknesses[ii] = 0.0f;
    }

    FLOAT get_thickness(size_t depth)
    {
        return (depth < _countof(thicknesses)) ? thicknesses[depth] : 0.0f;
    }

    const FLOAT stroke;
    const FLOAT margin;
    const FLOAT indicator_thickness;
    const FLOAT center_radius;
    const FLOAT boundary_radius;
    const FLOAT max_radius;
    const FLOAT range_radius;

private:
    FLOAT thicknesses[10];
};

//----------------------------------------------------------------------------
// Sunburst.

Sunburst::Sunburst()
{
}

Sunburst::~Sunburst()
{
}

void Sunburst::Init(const Sunburst& other)
{
    m_dpi = other.m_dpi;
    m_bounds = other.m_bounds;
    m_center = other.m_center;
}

void Sunburst::MakeArc(std::vector<Arc>& arcs, const std::shared_ptr<Node>& node, ULONGLONG size, double& sweep, double total, float start, float span)
{
    Arc arc;
    arc.m_start = start + float(sweep * span / total);
    sweep += size;
    arc.m_end = start + float(sweep * span / total);

#ifdef DEBUG
    if (arc.m_start > 360.0f || arc.m_end > 360.0f)
        DebugBreak();
    assert(arc.m_end - arc.m_start <= span);
#endif

// TODO: Base it on a percentage of total size when multiple roots are present?
// TODO: Unclear what to do when only there's only one root and it isn't a drive...
// NOTE: The problem is the async scan can cause the max depth to jitter, which rescales the whole chart.
    if (arc.m_end - arc.m_start >= c_minAngle)
    {
        arc.m_node = node;
        arcs.emplace_back(std::move(arc));
    }
}

void Sunburst::BuildRings(const std::vector<std::shared_ptr<DirNode>>& _roots)
{
    const std::vector<std::shared_ptr<DirNode>> roots = _roots;

    std::vector<double> totals;
    std::vector<float> starts;
    std::vector<float> spans;

    m_roots = roots;
    m_rings.clear();

    {
        double grand_total = 0;
        for (const auto dir : roots)
        {
            std::shared_ptr<FreeSpaceNode> free = dir->GetFreeSpace();
            if (free)
                totals.emplace_back(double(free->GetTotalSize()));
            else
                totals.emplace_back(double(dir->GetSize()));
            grand_total += totals.back();
        }

        double sweep = 0;
        for (size_t ii = 0; ii < roots.size(); ++ii)
        {
            const float start = float(sweep * 360 / grand_total);
            sweep += totals[ii];
            const float end = float(sweep * 360 / grand_total);
            starts.emplace_back(start);
            spans.emplace_back(end - start);
        }
    }

    m_rings.emplace_back();

    std::vector<Arc>& arcs = m_rings.back();

    for (size_t ii = 0; ii < roots.size(); ++ii)
    {
        std::shared_ptr<DirNode> root = roots[ii];
        std::vector<std::shared_ptr<DirNode>> dirs = root->CopyDirs();
        std::vector<std::shared_ptr<FileNode>> files = root->CopyFiles();
        std::shared_ptr<FreeSpaceNode> free = root->GetFreeSpace();

        const double total = totals[ii];
        const float start = starts[ii];
        const float span = spans[ii];
        const double scale = free ? double(free->GetTotalSize() - free->GetFreeSize()) / root->GetSize() : 1.0f;
// TODO: The proportions are wrong while painting during async scan.

        double sweep = 0;
        for (const auto dir : dirs)
            MakeArc(arcs, std::static_pointer_cast<Node>(dir), ULONGLONG(dir->GetSize() * scale), sweep, total, start, span);
        for (const auto file : files)
            MakeArc(arcs, std::static_pointer_cast<Node>(file), ULONGLONG(file->GetSize() * scale), sweep, total, start, span);
        if (free)
            MakeArc(arcs, std::static_pointer_cast<Node>(free), free->GetFreeSize(), sweep, total, start, span);
    }

// TODO: Smart depth limiting.
    while (m_rings.size() <= 10)
    {
        std::vector<Arc> arcs = NextRing(m_rings.back());
        if (arcs.empty())
            break;
        m_rings.emplace_back(std::move(arcs));
    }

#ifdef DEBUG
    for (const auto ring : m_rings)
    {
        float prev = 0;
        for (const auto arc : ring)
        {
            assert(arc.m_start >= prev);
            prev = arc.m_end;
        }
    }
#endif
}

std::vector<Sunburst::Arc> Sunburst::NextRing(const std::vector<Arc>& parent_ring)
{
    std::vector<Arc> arcs;

    for (const auto _parent : parent_ring)
    {
        const DirNode* parent = _parent.m_node->AsDir();
// TODO: How to ignore nodes that have been marked "hidden" in the UI?
        if (parent)
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
                MakeArc(arcs, std::static_pointer_cast<Node>(dir), dir->GetSize(), sweep, range, start, span);
            for (const auto file : files)
                MakeArc(arcs, std::static_pointer_cast<Node>(file), file->GetSize(), sweep, range, start, span);

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

bool Sunburst::IsRoot(const std::shared_ptr<Node>& node)
{
    for (const auto root : m_roots)
    {
        if (node == root)
            return true;
    }

    return false;
}

static FLOAT FindAngle(const D2D1_POINT_2F& center, FLOAT x, FLOAT y)
{
    if (x == center.x)
        return (y < center.y) ? 270.0f : 90.0f;
    if (y == center.y)
        return (x < center.x) ? 180.0f : 0.0f;

    FLOAT angle = atan2((y - center.y), (x - center.x)) * 180.0f / M_PI;
    if (angle < 0.0f)
        angle += 360.0f;
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

D2D1_COLOR_F Sunburst::MakeColor(const Arc& arc, size_t depth, bool highlight)
{
    if (arc.m_node->AsFreeSpace())
        return D2D1::ColorF(highlight ? D2D1::ColorF::LightSteelBlue : D2D1::ColorF::WhiteSmoke);

    const bool file = !!arc.m_node->AsFile();
    if (file)
        depth = 0;

    const FLOAT angle = (arc.m_start + arc.m_end) / 2.0f;

// TODO: Dark theme.
    HSLColorType hsl;
    hsl.h = angle * c_maxHue / 360;
    hsl.s = highlight ? c_maxSat : (c_maxSat * 0.8f) - (FLOAT(depth) * (c_maxSat / 20));
    hsl.l = highlight ? c_maxLum / 2 : (c_maxLum * (file ? 0.6f : 0.4f)) + (FLOAT(depth) * (c_maxLum / 20));

    const COLORREF rgb = hsl.ToRGB();

    D2D1_COLOR_F color;
    color.r = FLOAT(GetRValue(rgb)) / 255;
    color.g = FLOAT(GetGValue(rgb)) / 255;
    color.b = FLOAT(GetBValue(rgb)) / 255;
    color.a = 1.0f;

    return color;
}

void Sunburst::RenderRings(DirectHwndRenderTarget& target, const D2D1_RECT_F& rect, const std::shared_ptr<Node>& highlight)
{
    ID2D1RenderTarget* pTarget = target.Target();
    ID2D1Geometry* pHighlight = nullptr;

// BUGBUG: Centering drifts as window size changes.
    m_bounds = rect;
    m_center = D2D1::Point2F((rect.left + rect.right) / 2.0f,
                             (rect.top + rect.bottom) / 2.0f);

    ID2D1Layer* pFileLayer = nullptr;
    D2D1_LAYER_PARAMETERS fileLayerParams = D2D1::LayerParameters(
// TODO: The layer size is wrong; that's why some files don't show up.
        rect, 0, D2D1_ANTIALIAS_MODE_ALIASED, D2D1::Matrix3x2F::Identity(), 0.35f);
    pTarget->CreateLayer(&pFileLayer);

    SunburstMetrics mx(m_dpi, m_bounds, m_rings.size());

    ID2D1SolidColorBrush* pLineBrush = target.LineBrush();
    ID2D1SolidColorBrush* pFillBrush = target.FillBrush();

    // Outer boundary outline.

    {
        D2D1_ELLIPSE ellipse;
        ellipse.point = m_center;
        ellipse.radiusX = mx.boundary_radius;
        ellipse.radiusY = mx.boundary_radius;

        pFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LightGray));
        pTarget->DrawEllipse(ellipse, pFillBrush, mx.stroke);
    }

    // Center circle.

    {
        D2D1_ELLIPSE ellipse;
        ellipse.point = m_center;
        ellipse.radiusX = mx.center_radius;
        ellipse.radiusY = mx.center_radius;

// TODO: Also draw separator lines between roots.
        ID2D1EllipseGeometry* pGeometry = nullptr;
        if (SUCCEEDED(target.Factory()->CreateEllipseGeometry(ellipse, &pGeometry)))
        {
            if (highlight && IsRoot(highlight))
                pFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LightSteelBlue));
            else
                pFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LightGray));
            pTarget->FillGeometry(pGeometry, pFillBrush);
            pTarget->DrawGeometry(pGeometry, pLineBrush, mx.stroke);

            for (const auto root : m_roots)
            {
                if (highlight == root)
                {
                    ReleaseI(pHighlight);
                    pHighlight = pGeometry;
                    pHighlight->AddRef();
                    break;
                }
            }

            ReleaseI(pGeometry);
        }
    }

    // Rings.

    FLOAT inner_radius = mx.center_radius;
    for (size_t depth = 0; depth < m_rings.size(); ++depth)
    {
        const FLOAT thickness = mx.get_thickness(depth);
        if (thickness <= 0.0f)
            break;

        const FLOAT outer_radius = inner_radius + thickness;
        if (outer_radius > mx.max_radius)
            break;

        for (const auto arc : m_rings[depth])
        {
            const D2D1_POINT_2F inner_start_point = MakePoint(m_center, inner_radius, arc.m_start);
            const D2D1_POINT_2F inner_end_point = MakePoint(m_center, inner_radius, arc.m_end);
// TODO: When inner_start_point == inner_end_point, D2D gets confused where to
// draw the arc, since it's a full circle.

            ID2D1PathGeometry* pGeometry = nullptr;
            if (SUCCEEDED(target.Factory()->CreatePathGeometry(&pGeometry)))
            {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pGeometry->Open(&pSink)))
                {
                    D2D1_ARC_SEGMENT inner;
                    inner.point = inner_end_point;
                    inner.size = D2D1::SizeF(inner_radius, inner_radius);
                    inner.rotationAngle = arc.m_start;
                    inner.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                    inner.arcSize = (arc.m_end - arc.m_start > 180) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;

                    D2D1_ARC_SEGMENT outer;
                    outer.point = MakePoint(m_center, outer_radius, arc.m_start);
                    outer.size = D2D1::SizeF(outer_radius, outer_radius);
                    outer.rotationAngle = arc.m_end;
                    outer.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                    outer.arcSize = (arc.m_end - arc.m_start > 180) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;

                    pSink->SetFillMode(D2D1_FILL_MODE_WINDING);
                    pSink->BeginFigure(inner_start_point, D2D1_FIGURE_BEGIN_FILLED);
                    pSink->AddArc(inner);
                    pSink->AddLine(MakePoint(m_center, outer_radius, arc.m_end));
                    pSink->AddArc(outer);
                    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

                    pSink->Close();
                    ReleaseI(pSink);

                    const bool isFile = !!arc.m_node->AsFile();
                    const bool isHighlight = (highlight == arc.m_node);

                    pFillBrush->SetColor(MakeColor(arc, depth, isHighlight));

                    if (isFile)
                        pTarget->PushLayer(fileLayerParams, pFileLayer);

                    pTarget->FillGeometry(pGeometry, pFillBrush);
                    pTarget->DrawGeometry(pGeometry, pLineBrush, mx.stroke);

                    if (isHighlight)
                    {
                        ReleaseI(pHighlight);
                        pHighlight = pGeometry;
                        pHighlight->AddRef();
                    }

                    if (isFile)
                        pTarget->PopLayer();
                }

// TODO: Cache geometries for faster repainting?
                ReleaseI(pGeometry);
            }
        }

        inner_radius = outer_radius;
    }

    ReleaseI(pFileLayer);

    // Hover highlight.

    if (pHighlight)
    {
        pTarget->DrawGeometry(pHighlight, pLineBrush, mx.stroke * 2.0f);
        ReleaseI(pHighlight);
    }
}

std::shared_ptr<Node> Sunburst::HitTest(POINT pt)
{
    const FLOAT xdelta = (pt.x - m_center.x);
    const FLOAT ydelta = (pt.y - m_center.y);
    FLOAT radius = sqrt((xdelta * xdelta) + (ydelta * ydelta));

    SunburstMetrics mx(m_dpi, m_bounds, m_rings.size());

    const bool use_parent = (radius <= mx.center_radius);
    if (use_parent)
    {
        if (m_roots.size() == 1)
            return m_roots[0];
// TODO: Need the actual start/end arcs for each root, otherwise there may be
// dead spots with no arc.
        radius = mx.center_radius + (mx.get_thickness(0) / 2);
    }

    radius -= mx.center_radius;

    for (size_t depth = 0; depth < m_rings.size(); ++depth)
    {
        radius -= mx.get_thickness(depth);
        if (radius < 0.0f)
        {
            const FLOAT angle = FindAngle(m_center, FLOAT(pt.x), FLOAT(pt.y));
            for (const auto arc : m_rings[depth])
            {
                if (arc.m_start <= angle && angle < arc.m_end)
                {
                    std::shared_ptr<Node> node(arc.m_node);
                    if (node && use_parent)
                        node = node->GetParent();
                    return node;
                }
            }
        }
    }

    return nullptr;
}

void Sunburst::OnDpiChanged(const DpiScaler& dpi)
{
    m_dpi.OnDpiChanged(dpi);

    m_rings.clear();
}

