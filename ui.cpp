// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "scan.h"
#include "sunburst.h"
#include "actions.h"
#include "ui.h"
#include "sunburst.h"
#include "res.h"
#include "version.h"
#include <windowsx.h>
#include <iosfwd>

void inset_rect_for_stroke(D2D1_RECT_F& rect, FLOAT stroke)
{
    rect.left += 0.5f;
    rect.top += 0.5f;
    rect.right -= 0.5f;
    rect.bottom -= 0.5f;

    if (stroke > 1.0f)
    {
        stroke -= 1.0f;
        rect.left += stroke / 2;
        rect.top += stroke / 2;
        rect.right -= stroke / 2;
        rect.bottom -= stroke / 2;
    }
}

FLOAT MakeUpIcon(DirectHwndRenderTarget& target, RECT rc, const DpiScaler& dpi, ID2D1Geometry** out)
{
    SPI<ID2D1PathGeometry> spGeometry;
    if (target.Factory() && SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        const LONG dim = std::min<LONG>(rc.right - rc.left, rc.bottom - rc.top) * 3 / 4;
        const LONG cx = dim * 3 / 5;
        const LONG cy = dim * 4 / 5;
        const FLOAT thickness = FLOAT(std::max<LONG>(3, dim / 8));

        const FLOAT left = FLOAT(rc.left + ((rc.right - rc.left) - cx) / 2);
        const FLOAT top = FLOAT(rc.top + ((rc.bottom - rc.top) - cy) / 2);
        D2D1_RECT_F rect = D2D1::RectF(left, top, left + cx, top + cy);

        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
            spSink->BeginFigure(D2D1::Point2F(rect.right, rect.bottom), D2D1_FIGURE_BEGIN_FILLED);

            D2D1_POINT_2F points[] =
            {
                D2D1::Point2F(rect.left + thickness, rect.bottom),
                D2D1::Point2F(rect.left + thickness, rect.top + thickness*2),
                D2D1::Point2F(rect.left, rect.top + thickness*2),
                D2D1::Point2F(rect.left + thickness*3/2, rect.top + (LONG(thickness) & 1)),
                D2D1::Point2F(rect.left + thickness*3, rect.top + thickness*2),
                D2D1::Point2F(rect.left + thickness*2, rect.top + thickness*2),
                D2D1::Point2F(rect.left + thickness*2, rect.bottom - thickness),
                D2D1::Point2F(rect.right, rect.bottom - thickness),
            };
            spSink->AddLines(points, _countof(points));

            spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            spSink->Close();

            *out = spGeometry.Transfer();
        }
    }

    return 0.0f; // FillGeometry.
}

FLOAT MakeBackIcon(DirectHwndRenderTarget& target, RECT rc, const DpiScaler& dpi, ID2D1Geometry** out)
{
    SPI<ID2D1PathGeometry> spGeometry;
    if (target.Factory() && SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        const LONG dim = std::min<LONG>(rc.right - rc.left, rc.bottom - rc.top) * 3 / 4;
        const LONG cx = dim * 4 / 5;
        const FLOAT thickness = FLOAT(std::max<LONG>(3, dim / 8));
        const LONG cy = LONG(thickness) * 3;

        const FLOAT left = FLOAT(rc.left + ((rc.right - rc.left) - cx) / 2);
        const FLOAT top = FLOAT(rc.top + ((rc.bottom - rc.top) - cy) / 2);
        D2D1_RECT_F rect = D2D1::RectF(left, top, left + cx, top + cy);

        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
            spSink->BeginFigure(D2D1::Point2F(rect.right, rect.bottom - thickness), D2D1_FIGURE_BEGIN_FILLED);

            D2D1_POINT_2F points[] =
            {
                D2D1::Point2F(rect.left + thickness*2, rect.bottom - thickness),
                D2D1::Point2F(rect.left + thickness*2, rect.bottom),
                D2D1::Point2F(rect.left + (LONG(thickness) & 1), rect.top + thickness*3/2),
                D2D1::Point2F(rect.left + thickness*2, rect.top),
                D2D1::Point2F(rect.left + thickness*2, rect.top + thickness),
                D2D1::Point2F(rect.right, rect.top + thickness),
            };
            spSink->AddLines(points, _countof(points));

            spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            spSink->Close();

            *out = spGeometry.Transfer();
        }
    }

    return 0.0f; // FillGeometry.
}

FLOAT MakeRefreshIcon(DirectHwndRenderTarget& target, RECT rc, const DpiScaler& dpi, ID2D1Geometry** out)
{
    SPI<ID2D1PathGeometry> spGeometry;
    if (target.Factory() && SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        const LONG dim = std::min<LONG>(rc.right - rc.left, rc.bottom - rc.top) * 3 / 4;
        const LONG cx = dim & ~1;
        const LONG cy = dim & ~1;
        const FLOAT thickness = FLOAT(std::max<LONG>(3, dim / 8));

        const FLOAT left = FLOAT(rc.left + ((rc.right - rc.left) - cx) / 2);
        const FLOAT top = FLOAT(rc.top + ((rc.bottom - rc.top) - cy) / 2);
        D2D1_RECT_F rect = D2D1::RectF(left, top, left + cx, top + cy);

        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            const D2D1_POINT_2F center = D2D1::Point2F((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);

            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
            spSink->BeginFigure(D2D1::Point2F(center.x, rect.top + thickness), D2D1_FIGURE_BEGIN_FILLED);

            const FLOAT outer_radius = (rect.right - rect.left) / 2 - thickness;

            D2D1_ARC_SEGMENT outer;
            outer.size = D2D1::SizeF(outer_radius, outer_radius);
            outer.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
            outer.point = D2D1::Point2F(rect.left + thickness, center.y);
            outer.rotationAngle = -90.0f;
            outer.arcSize = D2D1_ARC_SIZE_LARGE;
            spSink->AddArc(outer);

            D2D1_POINT_2F points[] =
            {
                D2D1::Point2F(rect.left, center.y),
                D2D1::Point2F(rect.left + thickness*3/2, center.y - thickness*2 + (LONG(thickness) & 1)),
                D2D1::Point2F(rect.left + thickness*3, center.y),
                D2D1::Point2F(rect.left + thickness*2, center.y),
            };
            spSink->AddLines(points, _countof(points));

            D2D1_ARC_SEGMENT inner;
            inner.size = D2D1::SizeF(outer_radius - thickness, outer_radius - thickness);
            inner.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
            inner.point = D2D1::Point2F(center.x, rect.top + thickness*2);
            inner.rotationAngle = 270.0f;
            inner.arcSize = D2D1_ARC_SIZE_LARGE;
            spSink->AddArc(inner);

            spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            spSink->Close();

            *out = spGeometry.Transfer();
        }
    }

    return 0.0f; // FillGeometry.
}

FLOAT MakeAppsIcon(DirectHwndRenderTarget& target, RECT rc, const DpiScaler& dpi, ID2D1Geometry** out)
{
    FLOAT stroke = 0.0f;

    SPI<ID2D1PathGeometry> spGeometry;
    if (target.Factory() && SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        const LONG dim = std::min<LONG>(rc.right - rc.left, rc.bottom - rc.top) * 7 / 10;
        const LONG margin = std::max<LONG>(2, dim / 10);
        const LONG extent = dim - margin*2;
        stroke = FLOAT(std::max<LONG>(2, extent / 10));
        const FLOAT halfstroke = stroke/2;
        const FLOAT leg = FLOAT(extent/2 - std::max<LONG>(1, LONG(halfstroke)));
        const FLOAT effectiveleg = leg - halfstroke;

        const FLOAT left = FLOAT(rc.left + ((rc.right - rc.left) - extent) / 2);
        const FLOAT top = FLOAT(rc.top + ((rc.bottom - rc.top) - extent) / 2);
        D2D1_RECT_F rect = D2D1::RectF(left, top, left + extent, top + extent);

        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);

            {
                spSink->BeginFigure(D2D1::Point2F(rect.right - halfstroke, rect.bottom - halfstroke), D2D1_FIGURE_BEGIN_FILLED);
                D2D1_POINT_2F points[] =
                {
                    D2D1::Point2F(rect.right - effectiveleg, rect.bottom - halfstroke),
                    D2D1::Point2F(rect.right - effectiveleg, rect.bottom - effectiveleg),
                    D2D1::Point2F(rect.right - halfstroke, rect.bottom - effectiveleg),
                };
                spSink->AddLines(points, _countof(points));
                spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }

            {
                spSink->BeginFigure(D2D1::Point2F(rect.left + halfstroke, rect.bottom - halfstroke), D2D1_FIGURE_BEGIN_FILLED);
                D2D1_POINT_2F points[] =
                {
                    D2D1::Point2F(rect.left + effectiveleg, rect.bottom - halfstroke),
                    D2D1::Point2F(rect.left + effectiveleg, rect.bottom - effectiveleg),
                    D2D1::Point2F(rect.left + halfstroke, rect.bottom - effectiveleg),
                };
                spSink->AddLines(points, _countof(points));
                spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }

            {
                spSink->BeginFigure(D2D1::Point2F(rect.left + halfstroke, rect.top + halfstroke), D2D1_FIGURE_BEGIN_FILLED);
                D2D1_POINT_2F points[] =
                {
                    D2D1::Point2F(rect.left + effectiveleg, rect.top + halfstroke),
                    D2D1::Point2F(rect.left + effectiveleg, rect.top + effectiveleg),
                    D2D1::Point2F(rect.left + halfstroke, rect.top + effectiveleg),
                };
                spSink->AddLines(points, _countof(points));
                spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }

            {
                const FLOAT tiltedoffset = sqrtf((leg-stroke)*(leg-stroke)/2);
                spSink->BeginFigure(D2D1::Point2F(rect.right - effectiveleg, rect.top + effectiveleg - tiltedoffset), D2D1_FIGURE_BEGIN_FILLED);
                D2D1_POINT_2F points[] =
                {
                    D2D1::Point2F(rect.right - effectiveleg + tiltedoffset, rect.top + effectiveleg - tiltedoffset*2),
                    D2D1::Point2F(rect.right - effectiveleg + tiltedoffset*2, rect.top + effectiveleg - tiltedoffset),
                    D2D1::Point2F(rect.right - effectiveleg + tiltedoffset, rect.top + effectiveleg),
                };
                spSink->AddLines(points, _countof(points));
                spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }

            spSink->Close();

            *out = spGeometry.Transfer();
        }
    }

    return stroke; // DrawGeometry when > 0.0f.
}

FLOAT MakeFolderIcon(DirectHwndRenderTarget& target, RECT rc, const DpiScaler& dpi, ID2D1Geometry** out)
{
    FLOAT stroke = 0.0f;

    SPI<ID2D1PathGeometry> spGeometry;
    if (target.Factory() && SUCCEEDED(target.Factory()->CreatePathGeometry(&spGeometry)))
    {
        const LONG dim = std::min<LONG>(rc.right - rc.left, rc.bottom - rc.top) * 2 / 3;
        const LONG cx = (dim & ~1) | ((rc.right - rc.left) & 1);
        const LONG cy = ((dim*4/5) & ~1) | ((rc.bottom - rc.top) & 1);
        stroke = FLOAT(std::max<LONG>(2, dim / 10));
        const FLOAT halfstroke = stroke/2;
        const FLOAT tab = FLOAT(dpi.Scale(3));

        const FLOAT left = FLOAT(rc.left + ((rc.right - rc.left) - cx) / 2);
        const FLOAT top = FLOAT(rc.top + ((rc.bottom - rc.top) - cy) / 2);
        D2D1_RECT_F rect = D2D1::RectF(left, top, left + cx, top + cy);

        SPI<ID2D1GeometrySink> spSink;
        if (SUCCEEDED(spGeometry->Open(&spSink)))
        {
            spSink->SetFillMode(D2D1_FILL_MODE_WINDING);
            spSink->BeginFigure(D2D1::Point2F(rect.right - halfstroke, rect.bottom - halfstroke), D2D1_FIGURE_BEGIN_FILLED);

            D2D1_POINT_2F points[] =
            {
                D2D1::Point2F(rect.left + halfstroke, rect.bottom - halfstroke),
                D2D1::Point2F(rect.left + halfstroke, rect.top + halfstroke),
                D2D1::Point2F(rect.left + halfstroke + cx/3, rect.top + halfstroke),
                D2D1::Point2F(rect.left + halfstroke + cx/3 + tab, rect.top + halfstroke + tab),
                D2D1::Point2F(rect.right - halfstroke, rect.top + halfstroke + tab),
            };
            spSink->AddLines(points, _countof(points));

            spSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            spSink->Close();

            *out = spGeometry.Transfer();
        }
    }

    return stroke; // DrawGeometry when > 0.0f.
}

//----------------------------------------------------------------------------
// ScannerThread.

class ScannerThread
{
public:
                            ScannerThread(std::recursive_mutex& ui_mutex);
                            ~ScannerThread() { Stop(); }

    std::vector<std::shared_ptr<DirNode>> Start(int argc, const WCHAR** argv);
    void                    Start(const std::shared_ptr<DirNode>& dir);
    void                    Stop();

    bool                    IsComplete();
    void                    GetScanningPath(std::wstring& out);

protected:
    void                    StartInternal(const std::vector<std::shared_ptr<DirNode>>& roots, bool fullscan);
    static void             ThreadProc(ScannerThread* pThis);

private:
    HANDLE                  m_hWake;
    HANDLE                  m_hStop;
    volatile LONG           m_generation = 0;
    size_t                  m_cursor = 0;
    std::shared_ptr<Node>   m_current;
    std::vector<std::shared_ptr<DirNode>> m_roots;
    bool                    m_fullscan = false;
    std::unique_ptr<std::thread> m_thread;
    std::mutex              m_mutex;
    std::recursive_mutex&   m_ui_mutex;
};

ScannerThread::ScannerThread(std::recursive_mutex& ui_mutex)
: m_ui_mutex(ui_mutex)
{
    m_hWake = CreateEvent(nullptr, false, false, nullptr);
    m_hStop = CreateEvent(nullptr, true, false, nullptr);
}

std::vector<std::shared_ptr<DirNode>> ScannerThread::Start(int argc, const WCHAR** argv)
{
    std::vector<std::shared_ptr<DirNode>> roots;

    if (argc)
    {
        for (int ii = 0; ii < argc; ++ii)
        {
            const auto root = MakeRoot(argv[ii]);
            if (root)
                roots.emplace_back(root);
        }
    }

    if (roots.empty())
        roots.emplace_back(MakeRoot(nullptr));

    m_fullscan = true;
    StartInternal(roots, true);

    return roots;
}

void ScannerThread::Start(const std::shared_ptr<DirNode>& dir)
{
    std::vector<std::shared_ptr<DirNode>> dirs;
    dirs.emplace_back(dir);
    StartInternal(dirs, false);
}

void ScannerThread::StartInternal(const std::vector<std::shared_ptr<DirNode>>& roots, bool fullscan)
{
    assert(fullscan || !m_fullscan); // Can't Rescan while a Scan is in progress.

    if (!m_thread)
        m_thread = std::make_unique<std::thread>(ThreadProc, this);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (fullscan)
        {
            m_current.reset();
            m_roots = roots;
            m_cursor = 0;
        }
        else
        {
            m_roots.insert(m_roots.begin() + m_cursor, roots.begin(), roots.end());
        }

        InterlockedIncrement(&m_generation);
    }

    SetEvent(m_hWake);
}

void ScannerThread::Stop()
{
    if (m_thread)
    {
        SetEvent(m_hStop);
        InterlockedIncrement(&m_generation);
        m_thread->join();
        m_current.reset();
        m_roots.clear();
        m_cursor = 0;
        m_fullscan = false;
        ResetEvent(m_hStop);
    }
}

bool ScannerThread::IsComplete()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_roots.empty();
}

void ScannerThread::GetScanningPath(std::wstring& out)
{
    std::lock_guard<std::recursive_mutex> lock(m_ui_mutex);

    if (m_current)
        m_current->GetFullPath(out);
    else
        out.clear();
}

void ScannerThread::ThreadProc(ScannerThread* pThis)
{
    while (true)
    {
        DWORD dw;

        const HANDLE handles[2] = { pThis->m_hWake, pThis->m_hStop };
        dw = WaitForMultipleObjects(_countof(handles), handles, false, INFINITE);

        if (dw != WAIT_OBJECT_0)
            break;

        const LONG generation = pThis->m_generation;

        while (generation == pThis->m_generation)
        {
            std::shared_ptr<DirNode> root;

            {
                std::lock_guard<std::mutex> lock(pThis->m_mutex);

                if (pThis->m_cursor >= pThis->m_roots.size())
                {
                    // This is important for the Rescan case.
                    for (const auto& top : pThis->m_roots)
                    {
                        std::shared_ptr<DirNode> parent = top;
                        while (parent)
                        {
                            std::shared_ptr<DirNode> up = parent->GetParent();
                            if (!up)
                                parent->Finish();
                            parent = up;
                        }
                    }

                    pThis->m_current.reset();
                    pThis->m_roots.clear();
                    pThis->m_cursor = 0;
                    pThis->m_fullscan = false;
                    break;
                }

                root = pThis->m_roots[pThis->m_cursor++];
            }

            ScanFeedback feedback = { pThis->m_ui_mutex, pThis->m_current, g_use_compressed_size };
            Scan(root, generation, &pThis->m_generation, feedback);
        }
    }
}

//----------------------------------------------------------------------------
// SizeTracker.

class SizeTracker
{
public:
                            SizeTracker(LONG default_cx, LONG default_cy);
    void                    OnCreate(HWND hwnd);
    void                    OnSize();
    void                    OnDestroy();

protected:
    void                    ReadPosition();
    void                    WritePosition();

private:
    HWND                    m_hwnd = 0;
    bool                    m_resized = false;
    bool                    m_maximized = false;
    RECT                    m_rcRestore = {};
    DpiScaler               m_dpi;
    const SIZE              m_default_size;
};

SizeTracker::SizeTracker(LONG default_cx, LONG default_cy)
: m_default_size({ default_cx, default_cy })
{
}

void SizeTracker::OnCreate(HWND hwnd)
{
    m_hwnd = hwnd;
    m_dpi = __GetDpiForWindow(hwnd);

    ReadPosition();
}

void SizeTracker::OnSize()
{
    if (!m_hwnd || IsIconic(m_hwnd))
        return;

    bool const maximized = !!IsMaximized(m_hwnd);
    DpiScaler dpi(__GetDpiForWindow(m_hwnd));

    RECT rc;
    GetWindowRect(m_hwnd, &rc);

    if (!maximized &&
        (memcmp(&m_rcRestore, &rc, sizeof(m_rcRestore)) || !dpi.IsDpiEqual(m_dpi)))
    {
        m_resized = true;
        m_rcRestore = rc;
        m_dpi = dpi;
    }

    if (maximized != m_maximized)
    {
        m_resized = true;
        m_maximized = maximized;
    }
}

void SizeTracker::OnDestroy()
{
    if (m_resized)
    {
        WritePosition();
        m_resized = false;
    }
    m_hwnd = 0;
}

void SizeTracker::ReadPosition()
{
    assert(m_hwnd);

    LONG cx = ReadRegLong(TEXT("WindowWidth"), 0);
    LONG cy = ReadRegLong(TEXT("WindowHeight"), 0);
    const bool maximized = !!ReadRegLong(TEXT("Maximized"), false);

    MONITORINFO info = { sizeof(info) };
    HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfo(hmon, &info);

    cx = m_dpi.Scale(cx ? cx : m_default_size.cx);
    cy = m_dpi.Scale(cy ? cy : m_default_size.cy);

    cx = std::min<LONG>(cx, info.rcWork.right - info.rcWork.left);
    cy = std::min<LONG>(cy, info.rcWork.bottom - info.rcWork.top);
    cx = std::max<LONG>(cx, m_dpi.Scale(480));
    cy = std::max<LONG>(cy, m_dpi.Scale(360));

    const LONG xx = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - cx) / 2;
    const LONG yy = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - cy) / 2;

    SetWindowPos(m_hwnd, 0, xx, yy, cx, cy, SWP_NOZORDER);
    GetWindowRect(m_hwnd, &m_rcRestore);

    ShowWindow(m_hwnd, maximized ? SW_MAXIMIZE : SW_NORMAL);

    m_resized = false;
}

void SizeTracker::WritePosition()
{
    assert(m_hwnd);

    const LONG cx = m_dpi.ScaleTo(m_rcRestore.right - m_rcRestore.left, 96);
    const LONG cy = m_dpi.ScaleTo(m_rcRestore.bottom - m_rcRestore.top, 96);

    WriteRegLong(TEXT("WindowWidth"), cx);
    WriteRegLong(TEXT("WindowHeight"), cy);
    WriteRegLong(TEXT("Maximized"), m_maximized);

    m_resized = false;
}

//----------------------------------------------------------------------------
// Buttons.

class Buttons
{
    typedef FLOAT (MakeButtonIconFn)(DirectHwndRenderTarget& target, RECT rc, const DpiScaler& dpi, ID2D1Geometry** out);

    struct Button
    {
        UINT m_id = 0;
        RECT m_rect = {};
        std::wstring m_caption;
        std::wstring m_desc;
        SPI<ID2D1Geometry> m_spGeometry;
        FLOAT m_stroke = 0.0f;
        MakeButtonIconFn* m_make_icon = nullptr;
        bool m_hidden = false;
    };

public:
    void                    Attach(HWND hwnd);
    void                    OnDpiChanged(const DpiScaler& dpi);
    void                    AddButton(UINT id, const RECT& rect, const WCHAR* caption=nullptr, const WCHAR* desc=nullptr, MakeButtonIconFn* make_icon=nullptr);
    void                    ShowButton(UINT id, bool show);
    void                    RenderButtons(DirectHwndRenderTarget& target);
    const WCHAR*            GetHoverDescription();
    void                    OnMouseMessage(UINT msg, const POINT* pt);
    void                    OnCancelMode();

protected:
    int                     HitTest(const POINT* pt) const;
    void                    InvalidateButton(int index) const;
    void                    SetHover(int hover, int pressed);

private:
    HWND                    m_hwnd = 0;
    std::vector<Button>     m_buttons;
    int                     m_hover = -1;
    int                     m_pressed = -1;
    DpiScaler               m_dpi;
};

void Buttons::Attach(HWND hwnd)
{
    OnCancelMode();
    m_buttons.clear();
    m_hwnd = hwnd;
}

void Buttons::OnDpiChanged(const DpiScaler& dpi)
{
    m_dpi.OnDpiChanged(dpi);
}

void Buttons::AddButton(const UINT id, const RECT& rect, const WCHAR* caption, const WCHAR* desc, MakeButtonIconFn* make_icon)
{
    Button button;
    button.m_id = id;
    button.m_rect = rect;
    if (caption)
        button.m_caption = caption;
    if (desc)
    {
        button.m_desc = TEXT("\u2192 ");
        button.m_desc.append(desc);
    }
    button.m_make_icon = make_icon;
    m_buttons.emplace_back(std::move(button));
}

void Buttons::ShowButton(const UINT id, const bool show)
{
    bool changed = false;

    for (auto& button : m_buttons)
    {
        if (id == button.m_id)
        {
            changed = (changed || button.m_hidden == show);
            button.m_hidden = !show;
        }
    }

    if (changed)
        InvalidateRect(m_hwnd, nullptr, false);
}

void Buttons::RenderButtons(DirectHwndRenderTarget& target)
{
    for (size_t ii = 0; ii < m_buttons.size(); ++ii)
    {
        auto& button = m_buttons[ii];
        if (button.m_hidden)
            continue;

        const RECT& rect = button.m_rect;
        D2D1_RECT_F rectF;
        rectF.left = FLOAT(rect.left);
        rectF.top = FLOAT(rect.top);
        rectF.right = FLOAT(rect.right);
        rectF.bottom = FLOAT(rect.bottom);

        const bool pressed = (m_hover == ii && m_pressed == ii);

        target.FillBrush()->SetColor(D2D1::ColorF(pressed ? D2D1::ColorF::LightSteelBlue : D2D1::ColorF::White));
        target.Target()->FillRectangle(rectF, target.FillBrush());

        const FLOAT stroke = FLOAT(m_dpi.Scale(1));
        inset_rect_for_stroke(rectF, stroke);
        target.FillBrush()->SetColor(D2D1::ColorF((pressed || m_hover == ii) ? D2D1::ColorF::Black : 0xD0D0D0));
        target.Target()->DrawRectangle(rectF, target.FillBrush(), stroke);

        if (!button.m_spGeometry && button.m_make_icon)
            button.m_stroke = (*button.m_make_icon)(target, button.m_rect, m_dpi, button.m_spGeometry.UnsafeAddress());
        if (button.m_spGeometry)
        {
            if (button.m_stroke > 0.0f)
                target.Target()->DrawGeometry(button.m_spGeometry, target.LineBrush(), button.m_stroke, target.RoundedStrokeStyle());
            else
                target.Target()->FillGeometry(button.m_spGeometry, target.LineBrush());
        }

        if (!button.m_caption.empty())
            target.WriteText(target.TextFormat(), 0.0f, 0.0f, rectF, button.m_caption, WTO_HCENTER|WTO_VCENTER);
    }
}

const WCHAR* Buttons::GetHoverDescription()
{
    return (m_hover >= 0 && m_hover < m_buttons.size()) ? m_buttons[m_hover].m_desc.c_str() : nullptr;
}

void Buttons::OnMouseMessage(UINT msg, const POINT* pt)
{
    const int hover = HitTest(pt);

    switch (msg)
    {
    case WM_MOUSEMOVE:
        SetHover(hover, m_pressed);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetHover(hover, hover);
        break;
    case WM_LBUTTONUP:
        if (m_hover >= 0 && m_hover < m_buttons.size() && m_hover == m_pressed)
            SendMessage(m_hwnd, WM_COMMAND, GET_WM_COMMAND_MPS(m_buttons[m_hover].m_id, m_hwnd, 0));
        m_pressed = -1;
        InvalidateButton(m_hover);
        break;
    }
}

void Buttons::OnCancelMode()
{
    SetHover(-1, -1);
}

int Buttons::HitTest(const POINT* pt) const
{
    if (pt)
    {
        for (size_t ii = 0; ii < m_buttons.size(); ++ii)
        {
            if (PtInRect(&m_buttons[ii].m_rect, *pt))
                return int(ii);
        }
    }
    return -1;
}

void Buttons::InvalidateButton(int index) const
{
    assert(m_hwnd);

    if (index >= 0 && index < m_buttons.size())
    {
        // Hybrid D2D+GDI painting doesn't mix well with partial invalidation.
        //InvalidateRect(m_hwnd, &m_buttons[index].m_rect, false);
        InvalidateRect(m_hwnd, nullptr, false);
    }
}

void Buttons::SetHover(int hover, int pressed)
{
    if (m_hover != hover || m_pressed != pressed)
    {
        InvalidateButton(m_hover);
        m_hover = hover;
        m_pressed = pressed;
        InvalidateButton(m_hover);
    }

    if (m_hover >= 0)
    {
        TRACKMOUSEEVENT track = { sizeof(track) };
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = m_hwnd;
        track.dwHoverTime = HOVER_DEFAULT;
        _TrackMouseEvent(&track);
    }
}

//----------------------------------------------------------------------------
// MainWindow.

class MainWindow
{
    enum
    {
        TIMER_PROGRESS          = 1,
        INTERVAL_PROGRESS               = 100,
    };

public:
                            MainWindow(HINSTANCE hinst);

    HWND                    Create();
    void                    Scan(int argc, const WCHAR** argv, bool rescan);

protected:
                            ~MainWindow() {}

    LRESULT                 WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void                    OnCommand(WORD id, HWND hwndCtrl, WORD code);
    void                    OnDpiChanged(const DpiScaler& dpi);
    void                    OnLayout(RECT* prc);
    LRESULT                 OnDestroy();
    LRESULT                 OnNcDestroy();

    void                    DrawNodeInfo(DirectHwndRenderTarget& target, D2D1_RECT_F rect, const std::shared_ptr<Node>& node, bool free_space);
    void                    DrawAppInfo(DirectHwndRenderTarget& target, D2D1_RECT_F rect);

    void                    Expand(const std::shared_ptr<Node>& node);
    void                    SetRoot(const std::shared_ptr<DirNode>& root);
    void                    SetRoots(const std::vector<std::shared_ptr<DirNode>>& roots);
    void                    Up();
    void                    Back();
    void                    Forward();
    void                    Summary();
    void                    DeleteNode(const std::shared_ptr<Node>& node);
    void                    UpdateRecycleBin(const std::shared_ptr<RecycleBinNode>& recycle);
    void                    EnumDrives();
    void                    Refresh(bool all=false);
    void                    Rescan(const std::shared_ptr<DirNode>& dir);

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND                    m_hwnd = 0;
    const HINSTANCE         m_hinst;
    HFONT                   m_hfont = 0;
    DpiScaler               m_dpi;
    LONG                    m_top_reserve = 0;
    LONG                    m_margin_reserve = 0;
    RECT                    m_web_link_rect = {};
    SizeTracker             m_sizeTracker;
    LONG                    m_cxNumberArea = 0;
    bool                    m_inWmDpiChanged = false;

    std::recursive_mutex    m_ui_mutex; // Synchronize m_scanner vs m_sunburst.

    std::vector<std::wstring> m_drives;

    std::vector<std::shared_ptr<DirNode>> m_original_roots;
    std::vector<std::shared_ptr<DirNode>> m_roots;
    std::vector<std::shared_ptr<DirNode>> m_back_stack; // (nullptr means use m_original_roots)
    size_t                  m_back_current = 0;
    ScannerThread           m_scanner;

    DirectHwndRenderTarget  m_directRender;
    Sunburst                m_sunburst;
    Buttons                 m_buttons;

    std::shared_ptr<Node>   m_hover_node;
    bool                    m_hover_free = false;

    MainWindow(const MainWindow&) = delete;
    const MainWindow& operator=(const MainWindow&) = delete;
};

static HFONT MakeFont(const DpiScaler& dpi, LONG points=0, LONG weight=0, const WCHAR* facename=nullptr)
{
    LOGFONT lf = { 0 };
    lstrcpyn(lf.lfFaceName, facename ? facename : TEXT("Segoe UI"), _countof(lf.lfFaceName));
    lf.lfHeight = dpi.PointSizeToHeight(points ? points : 10);
    lf.lfWeight = weight ? weight : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    return CreateFontIndirect(&lf);
}

MainWindow::MainWindow(HINSTANCE hinst)
: m_hinst(hinst)
, m_sizeTracker(800, 600)
, m_scanner(m_ui_mutex)
{
}

HWND MainWindow::Create()
{
    static const WCHAR* const c_class = TEXT("Elucidisk_MainWindow");
    static const WCHAR* const c_caption = TEXT("Elucidisk");
    static const DWORD c_style = WS_OVERLAPPEDWINDOW;

    static bool s_registered = false;
    if (!s_registered)
    {
        WNDCLASS wc = { 0 };
        wc.style = CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS;
        wc.lpfnWndProc = StaticWndProc;
        wc.hInstance = m_hinst;
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = HBRUSH(COLOR_WINDOW + 1 );
        wc.lpszClassName = c_class;
        RegisterClass(&wc);
        s_registered = true;
    }

    EnumDrives();

    assert(!m_hwnd);
    const HWND hwnd = CreateWindowExW(0, c_class, c_caption, c_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, m_hinst, this);
    assert(hwnd == m_hwnd);

    if (hwnd)
    {
        OnDpiChanged(DpiScaler(__GetDpiForWindow(hwnd)));
        m_sizeTracker.OnCreate(hwnd);
    }

    return hwnd;
}

void MainWindow::Scan(int argc, const WCHAR** argv, bool rescan)
{
    SetRoots(m_scanner.Start(argc, argv));
    if (!rescan)
        m_original_roots = m_roots;

    m_back_stack.clear();
    m_back_stack.emplace_back(nullptr);
    m_back_current = 0;

    SetTimer(m_hwnd, TIMER_PROGRESS, INTERVAL_PROGRESS, nullptr);
}

void MainWindow::SetRoot(const std::shared_ptr<DirNode>& root)
{
    if (root)
    {
        std::vector<std::shared_ptr<DirNode>> roots;
        roots.emplace_back(root);
        SetRoots(roots);
    }
    else
    {
        SetRoots(m_original_roots);
    }
}

void MainWindow::SetRoots(const std::vector<std::shared_ptr<DirNode>>& roots)
{
    m_roots.clear();
    m_roots = roots;

    std::wstring title = TEXT("Elucidisk");
    for (size_t ii = 0; ii < m_roots.size(); ++ii)
    {
        title.append(ii ? TEXT(" , ") : TEXT(" - "));
        title.append(m_roots[ii]->GetName());
    }
    SetWindowText(m_hwnd, title.c_str());

    InvalidateRect(m_hwnd, nullptr, false);
}

void MainWindow::Expand(const std::shared_ptr<Node>& node)
{
    if (!node || node->AsFile() || node->AsRecycleBin() || node->AsFreeSpace() || !is_root_finished(node))
        return;

    std::shared_ptr<DirNode> back;

    const bool up = (m_roots.size() == 1 && node == m_roots[0]);
    if (up && !node->GetParent())
    {
        if (m_original_roots.size() == 1 && node == m_original_roots[0])
            return;

        SetRoots(m_original_roots);
        assert(back == nullptr);
    }
    else
    {
        std::shared_ptr<DirNode> dir;
        if (up)
            dir = node->GetParent();
        else if (node->AsDir())
            dir = std::static_pointer_cast<DirNode>(node->AsDir()->shared_from_this());

        if (!dir)
            return;

        SetRoot(dir);
        back = dir;
    }

    assert(m_back_stack.size() > m_back_current);
    m_back_stack.resize(++m_back_current);
    m_back_stack.emplace_back(back);

    InvalidateRect(m_hwnd, nullptr, false);
}

void MainWindow::Up()
{
    if (m_roots.size() == 1)
        Expand(m_roots[0]);
}

void MainWindow::Back()
{
    if (m_back_current == 0)
        return;

    SetRoot(m_back_stack[--m_back_current]);
}

void MainWindow::Forward()
{
    if (m_back_current + 1 == m_back_stack.size())
        return;

    SetRoot(m_back_stack[++m_back_current]);
}

void MainWindow::Summary()
{
    EnumDrives();

    std::vector<const WCHAR*> args;
    for (const auto& drive : m_drives)
    {
        if (!is_subst(drive.c_str()))
            args.emplace_back(drive.c_str());
    }

    if (!args.empty())
        Scan(int(args.size()), args.data(), false);
}

void MainWindow::Refresh(bool all)
{
    if (!all && m_roots.size() == 1 && m_roots[0]->GetParent() != nullptr)
    {
        Rescan(m_roots[0]);
    }
    else
    {
        EnumDrives();

        std::vector<const WCHAR*> paths;
        for (const auto root : m_original_roots)
            paths.emplace_back(root->GetName());

        Scan(int(paths.size()), paths.data(), false/*rescan*/);
    }
}

void MainWindow::Rescan(const std::shared_ptr<DirNode>& dir)
{
#ifdef DEBUG
    if (dir->IsFake())
        return;
#endif

    if (!m_scanner.IsComplete())
    {
        MessageBeep(0xffffffff);
        return;
    }

    bool compressed = false;
    {
        std::wstring path;
        dir->GetFullPath(path);
        strip_separator(path);

        if (!is_drive(path.c_str()))
        {
            WIN32_FIND_DATA fd;
            HANDLE hFind = FindFirstFile(path.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
                    compressed = true;
                FindClose(hFind);
            }
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_ui_mutex);

        dir->Clear();
        dir->SetCompressed(compressed);
    }

    m_scanner.Start(dir);

    SetTimer(m_hwnd, TIMER_PROGRESS, INTERVAL_PROGRESS, nullptr);
    InvalidateRect(m_hwnd, nullptr, false);
}

void MainWindow::EnumDrives()
{
    m_drives.clear();

    WCHAR buffer[1024];
    const DWORD dw = GetLogicalDriveStrings(_countof(buffer), buffer);
    if (dw > 0 && dw <= _countof(buffer))
    {
        for (WCHAR* p = buffer; *p; ++p)
        {
            WCHAR* drive = p;

            while (*p)
                ++p;

            for (WCHAR* q = drive + wcslen(drive); q-- > drive;)
            {
                if (!is_separator(*q))
                    break;
                *q = 0;
            }

            const UINT type = GetDriveType(drive);
            if (type == DRIVE_FIXED || type == DRIVE_RAMDISK)
                m_drives.emplace_back(drive);
        }
    }

    if (m_drives.empty())
        m_drives.emplace_back(TEXT("."));

    if (m_hwnd)
    {
        RECT rcClient;
        GetClientRect(m_hwnd, &rcClient);
        OnLayout(&rcClient);
        InvalidateRect(m_hwnd, nullptr, false);
    }
}

void MainWindow::DrawNodeInfo(DirectHwndRenderTarget& t, D2D1_RECT_F rect, const std::shared_ptr<Node>& node, const bool is_free)
{
    // Determine top line text.

    bool bold = false;
    bool show_free = false;
    const WCHAR* desc = m_buttons.GetHoverDescription();
    std::wstring text;

    if (desc)
    {
        text = desc;
    }
    else if (node)
    {
        std::wstring path;
        node->GetFullPath(path);
        if (node->AsDir() && node->AsDir()->GetFreeSpace())
        {
            text.append(is_free ? TEXT("Free on ") : TEXT("Used on "));
            show_free = is_free;
        }
        text.append(path);
    }
    else if (m_scanner.IsComplete())
    {
        if (m_roots.size() > 0)
        {
            std::wstring path;
            text.clear();
            for (size_t ii = 0; ii < m_roots.size(); ++ii)
            {
                text.append(ii ? TEXT(" , ") : TEXT("Scan of "));
                m_roots[ii]->GetFullPath(path);
                text.append(path);
                bold = true;
            }
        }
        else
        {
            text = TEXT("There's nothing to scan.");
        }
    }
    else
    {
        std::wstring path;
        m_scanner.GetScanningPath(path);
        if (!path.empty())
        {
            text.clear();
            text.append(TEXT("Scanning "));
            text.append(path);
        }
    }

    // Write top line text.

    const int padding = m_dpi.Scale(4);

    rect.left += padding;
    rect.right -= padding;
    rect.top += padding;
    rect.bottom -= padding;

    D2D1_SIZE_F size;
    D2D1_RECT_F rectLine = rect;

    SPI<IDWriteTextLayout> spTextLayout;
    IDWriteTextFormat* pTextFormat = bold ? t.CenterTextFormat() : t.TextFormat();
    if (t.MeasureText(pTextFormat, rectLine, text.c_str(), text.length(), size, &spTextLayout))
    {
        WriteTextOptions options = bold ? WTO_HCENTER|WTO_PATH_ELLIPSIS : WTO_NONE|WTO_PATH_ELLIPSIS;
        t.WriteText(pTextFormat, rectLine.left, rectLine.top, rectLine, text, options);
    }

    rectLine.top += (bold ? t.CenterFontSize() : t.FontSize()) + padding;

    // Write node details.

    if (!desc && node)
    {
        std::wstring units;
        ULONGLONG bytes = 0;
        bool has_bytes = true;
        D2D1_RECT_F rectNumber;

        if (node->AsDir())
        {
            std::shared_ptr<FreeSpaceNode> free = node->AsDir()->GetFreeSpace();
            bytes = free && show_free ? free->GetFreeSize() : node->AsDir()->GetEffectiveSize();
        }
        else if (node->AsFile())
            bytes = node->AsFile()->GetSize();
        else if (node->AsFreeSpace())
            bytes = node->AsFreeSpace()->GetFreeSize();
        else
            has_bytes = false;

        if (has_bytes)
        {
            D2D1_COLOR_F oldColor = t.TextBrush()->GetColor();
            m_sunburst.FormatSize(bytes, text, units);

            if (node->IsCompressed())
                t.TextBrush()->SetColor(D2D1::ColorF(0x0033ff));

            rectNumber = rectLine;
            rectNumber.right = FLOAT(m_cxNumberArea);
            t.WriteText(t.TextFormat(), 0.0f, rectNumber.top, rectNumber, text, WTO_RIGHT_ALIGN);
            if (node->IsCompressed())
                units.append(TEXT(" compressed"));
            t.WriteText(t.TextFormat(), rectLine.left + m_cxNumberArea + padding, rectLine.top, rectLine, units);

            if (node->IsCompressed())
                t.TextBrush()->SetColor(oldColor);
        }

        rectLine.top += t.FontSize();

        if (node->AsDir() && !show_free && !node->AsDir()->IsRecycleBin())
        {
            FormatCount(node->AsDir()->CountFiles(), text);
            rectNumber = rectLine;
            rectNumber.right = FLOAT(m_cxNumberArea);
            t.WriteText(t.TextFormat(), 0.0f, rectNumber.top, rectNumber, text, WTO_RIGHT_ALIGN);
            text = TEXT("Files");
            t.WriteText(t.TextFormat(), rectLine.left + m_cxNumberArea + padding, rectLine.top, rectLine, text);
            rectLine.top += t.FontSize();

            FormatCount(node->AsDir()->CountDirs(true/*include_recycle*/), text);
            rectNumber = rectLine;
            rectNumber.right = FLOAT(m_cxNumberArea);
            t.WriteText(t.TextFormat(), 0.0f, rectNumber.top, rectNumber, text, WTO_RIGHT_ALIGN);
            text = TEXT("Dirs");
            t.WriteText(t.TextFormat(), rectLine.left + m_cxNumberArea + padding, rectLine.top, rectLine, text);
            rectLine.top += t.FontSize();
        }
    }
}

void MainWindow::DrawAppInfo(DirectHwndRenderTarget& t, D2D1_RECT_F rect)
{
    WCHAR sz[100];
    std::wstring text;

#ifdef DEBUG
    {
        static int s_counter = 0;
        s_counter++;

        auto rectDbgInfo = rect;
        rectDbgInfo.bottom -= m_margin_reserve;

        swprintf_s(sz, _countof(sz), TEXT("%u nodes / %u paints"), CountNodes(), s_counter);
        t.WriteText(t.TextFormat(), 0.0f, 0.0f, rectDbgInfo, sz, wcslen(sz), WTO_HCENTER|WTO_BOTTOM_ALIGN);
    }
#endif

    const LONG padding = m_dpi.Scale(4);
    rect.right -= padding;
    rect.bottom -= padding;

    text = TEXT("Elucidisk github repo");
    auto oldColor = t.TextBrush()->GetColor();
    t.TextBrush()->SetColor(D2D1::ColorF(0x3333ff));
    t.WriteText(t.TextFormat(), 0.0f, 0.0f, rect, text, WTO_RIGHT_ALIGN|WTO_BOTTOM_ALIGN|WTO_REMEMBER_METRICS|WTO_UNDERLINE);
    m_web_link_rect.left = LONG(t.LastTextPosition().x);
    m_web_link_rect.top = LONG(t.LastTextPosition().y);
    m_web_link_rect.right = LONG(rect.right);
    m_web_link_rect.bottom = LONG(rect.bottom);
    t.TextBrush()->SetColor(oldColor);
    rect.bottom -= t.LastTextSize().height;

    text = TEXT("by Christopher Antos");
    t.WriteText(t.TextFormat(), 0.0f, 0.0f, rect, text, WTO_RIGHT_ALIGN|WTO_BOTTOM_ALIGN|WTO_REMEMBER_METRICS);
    rect.bottom -= t.LastTextSize().height;

    text = TEXT(COPYRIGHT_STR);
    const WCHAR* end = wcschr(wcschr(wcschr(text.c_str(), ' ') + 1, ' ') + 1, ' ');
    text.resize(end - text.c_str());
    t.WriteText(t.TextFormat(), 0.0f, 0.0f, rect, text, WTO_RIGHT_ALIGN|WTO_BOTTOM_ALIGN|WTO_REMEMBER_METRICS);
    rect.bottom -= t.LastTextSize().height;

    swprintf_s(sz, _countof(sz), TEXT("Version %u.%u"), VERSION_MAJOR, VERSION_MINOR);
    t.WriteText(t.TextFormat(), 0.0f, 0.0f, rect, sz, wcslen(sz), WTO_RIGHT_ALIGN|WTO_BOTTOM_ALIGN|WTO_REMEMBER_METRICS);
    rect.bottom -= t.LastTextSize().height;
}

LRESULT CALLBACK MainWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        MainWindow* p = static_cast<MainWindow*>(LPCREATESTRUCT(lParam)->lpCreateParams);
        assert(p);

        // set the "this" pointer
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, DWORD_PTR(p));
        p->m_hwnd = hwnd;
    }

    MainWindow* pThis = reinterpret_cast<MainWindow*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (!pThis)
        return ::DefWindowProc(hwnd, msg, wParam, lParam);

    assert(pThis->m_hwnd == hwnd);

    if (msg == WM_DESTROY)
        return pThis->OnDestroy();
    if (msg == WM_NCDESTROY)
        return pThis->OnNcDestroy();

    return pThis->WndProc(msg, wParam, lParam);
}

LRESULT MainWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return true;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(m_hwnd, &ps);
            SaveDC(ps.hdc);

            RECT rcClient;
            GetClientRect(m_hwnd, &rcClient);

            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);

            m_buttons.ShowButton(IDM_UP, m_roots.size() == 1 && (m_roots[0]->GetParent() || m_original_roots.size() > 1));
            m_buttons.ShowButton(IDM_BACK, m_back_current > 0);

            if (SUCCEEDED(m_directRender.CreateDeviceResources(m_hwnd, m_dpi)))
            {
                ID2D1RenderTarget* const pTarget = m_directRender.Target();
                pTarget->AddRef();

                pTarget->BeginDraw();

                pTarget->SetTransform(D2D1::Matrix3x2F::Identity());
                pTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

                const D2D1_SIZE_F rtSize = pTarget->GetSize();
                const D2D1_RECT_F rectClient = D2D1::RectF(FLOAT(rcClient.left), FLOAT(rcClient.top), FLOAT(rcClient.right), FLOAT(rcClient.bottom));

                const FLOAT width = rtSize.width - (m_margin_reserve + m_dpi.Scale(32) + m_margin_reserve) * 2;
                const FLOAT height = rtSize.height - (m_margin_reserve + m_top_reserve);

                const FLOAT extent = std::min<FLOAT>(width, height);
                FLOAT xx = (rtSize.width - extent) / 2;
                FLOAT yy = m_margin_reserve + m_top_reserve + (height - extent) / 2;
                const D2D1_RECT_F bounds = D2D1::RectF(xx, yy, xx + extent, yy + extent);

                static size_t s_gen = 0;
                const size_t gen = ++s_gen;

                Sunburst sunburst;
                {
                    std::lock_guard<std::recursive_mutex> lock(m_ui_mutex);

                    sunburst.OnDpiChanged(m_dpi);
                    sunburst.SetBounds(bounds);

                    // FUTURE: Only rebuild rings when something has changed?
                    sunburst.BuildRings(m_roots);
                    m_hover_node = sunburst.HitTest(pt, &m_hover_free);
                    sunburst.RenderRings(m_directRender, m_hover_node);
                }

                if (gen == s_gen)
                {
                    m_sunburst = std::move(sunburst);
                }
                else
                {
                    m_hover_node.reset();
                    m_hover_free = false;
                }

                m_buttons.RenderButtons(m_directRender);

                {
                    ULONGLONG used = 0;
                    for (const auto root : m_roots)
                        used += root->GetEffectiveSize();

                    std::wstring text;
                    std::wstring units;
                    m_sunburst.FormatSize(used, text, units);
                    text.append(TEXT(" "));
                    text.append(units);

                    std::wstring text2;
                    if (m_roots.size() > 1)
                    {
                        text2 = TEXT("Total");
                    }
                    else if (m_roots.size() == 1)
                    {
                        for (const auto& drive : m_drives)
                        {
                            if (wcsnicmp(drive.c_str(), m_roots[0]->GetName(), drive.length()) == 0)
                            {
                                text2 = drive;
                                break;
                            }
                        }
                    }

                    m_directRender.WriteText(m_directRender.CenterTextFormat(), 0.0f, 0.0f, bounds, text, WTO_HCENTER|WTO_VCENTER|WTO_REMEMBER_METRICS);
                    if (!text2.empty())
                    {
                        D2D1_RECT_F rectLabel = bounds;
                        rectLabel.bottom = m_directRender.LastTextPosition().y;
                        m_directRender.WriteText(m_directRender.TextFormat(), 0.0f, 0.0f, rectLabel, text2, WTO_HCENTER|WTO_BOTTOM_ALIGN);
                    }
                }

                DrawNodeInfo(m_directRender, rectClient, m_hover_node, m_hover_free);
                DrawAppInfo(m_directRender, rectClient);

                if (FAILED(pTarget->EndDraw()))
                    m_directRender.ReleaseDeviceResources();

                pTarget->Release();
            }

            RestoreDC(ps.hdc, -1);
            EndPaint(m_hwnd, &ps);
        }
        break;

    case WM_SETCURSOR:
        {
            UINT xy = GetMessagePos();
            POINT pt = { GET_X_LPARAM(xy), GET_Y_LPARAM(xy) };
            ScreenToClient(m_hwnd, &pt);
            if (PtInRect(&m_web_link_rect, pt))
            {
                SetCursor(LoadCursor(0, IDC_HAND));
                return true;
            }
        }
        goto LDefault;

    case WM_MOUSEMOVE:
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);

            const std::shared_ptr<Node> hover(m_hover_node);
            const bool hover_free = m_hover_free;
            m_hover_node = m_sunburst.HitTest(pt, &m_hover_free);

            if (hover != m_hover_node || hover_free != m_hover_free)
                InvalidateRect(m_hwnd, nullptr, false);

            if (hover)
            {
                TRACKMOUSEEVENT track = { sizeof(track) };
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = m_hwnd;
                track.dwHoverTime = HOVER_DEFAULT;
                _TrackMouseEvent(&track);
            }

            m_buttons.OnMouseMessage(msg, &pt);
        }
        break;
    case WM_MOUSELEAVE:
        m_hover_node.reset();
        m_hover_free = false;
        m_buttons.OnCancelMode();
        InvalidateRect(m_hwnd, nullptr, false);
        break;

    case WM_TIMER:
        if (wParam == TIMER_PROGRESS)
        {
            if (m_scanner.IsComplete())
                KillTimer(m_hwnd, wParam);
            InvalidateRect(m_hwnd, nullptr, false);
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            std::shared_ptr<Node> node = m_sunburst.HitTest(pt);
            Expand(node);

            m_buttons.OnMouseMessage(msg, &pt);

            if (PtInRect(&m_web_link_rect, pt))
                ShellOpen(m_hwnd, TEXT("https://github.com/chrisant996/elucidisk"));
        }
        break;

    case WM_LBUTTONUP:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            m_buttons.OnMouseMessage(msg, &pt);
        }
        break;

    case WM_RBUTTONDOWN:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            std::shared_ptr<Node> node = m_sunburst.HitTest(pt);
            DirNode* const dir = node ? node->AsDir() : nullptr;
            FileNode* const file = node ? node->AsFile() : nullptr;
            DirNode* const parent = (node && node->GetParent() ? node->GetParent()->AsDir() : nullptr);
            RecycleBinNode* const recycle = (dir && dir->AsRecycleBin() ? dir->AsRecycleBin() :
                                             dir && dir->GetRecycleBin() ? dir->GetRecycleBin()->AsRecycleBin() :
                                             nullptr);
            std::wstring path;

            if (node)
            {
                if (is_root_finished(node) && !file && !dir)
                    break;

                node->GetFullPath(path);
                if (path.empty())
                    break;
            }

            POINT ptScreen = pt;
            ClientToScreen(m_hwnd, &ptScreen);

            const int nPos = node ? 0 : 1;
            HMENU hmenu = LoadMenu(m_hinst, MAKEINTRESOURCE(IDR_CONTEXT_MENU));
            HMENU hmenuSub = GetSubMenu(hmenu, nPos);

            const bool root_finished(node);
            if (!root_finished || !m_scanner.IsComplete())
            {
                EnableMenuItem(hmenuSub, IDM_RESCAN, MF_BYCOMMAND|MF_GRAYED);
                EnableMenuItem(hmenuSub, IDM_RECYCLE_ENTRY, MF_BYCOMMAND|MF_GRAYED);
                EnableMenuItem(hmenuSub, IDM_DELETE_ENTRY, MF_BYCOMMAND|MF_GRAYED);
            }

            if (file)
            {
                DeleteMenu(hmenuSub, IDM_RESCAN, MF_BYCOMMAND);
                DeleteMenu(hmenuSub, IDM_OPEN_DIRECTORY, MF_BYCOMMAND);
            }
            if (file || !parent)
            {
                DeleteMenu(hmenuSub, IDM_HIDE_DIRECTORY, MF_BYCOMMAND);
                DeleteMenu(hmenuSub, IDM_SHOW_DIRECTORY, MF_BYCOMMAND);
            }
            if (dir)
            {
                if (dir->IsRecycleBin() || !parent)
                {
                    DeleteMenu(hmenuSub, IDM_RECYCLE_ENTRY, MF_BYCOMMAND);
                    DeleteMenu(hmenuSub, IDM_DELETE_ENTRY, MF_BYCOMMAND);
                }
                DeleteMenu(hmenuSub, IDM_OPEN_FILE, MF_BYCOMMAND);
                DeleteMenu(hmenuSub, dir->IsHidden() ? IDM_HIDE_DIRECTORY : IDM_SHOW_DIRECTORY, MF_BYCOMMAND);
            }
            if (!recycle)
                DeleteMenu(hmenuSub, IDM_EMPTY_RECYCLEBIN, MF_BYCOMMAND);

            if (g_use_compressed_size)
                CheckMenuItem(hmenuSub, IDM_OPTION_COMPRESSED, MF_BYCOMMAND|MF_CHECKED);
            if (g_show_free_space)
                CheckMenuItem(hmenuSub, IDM_OPTION_FREESPACE, MF_BYCOMMAND|MF_CHECKED);
            if (g_show_names)
                CheckMenuItem(hmenuSub, IDM_OPTION_NAMES, MF_BYCOMMAND|MF_CHECKED);
            if (g_rainbow)
                CheckMenuItem(hmenuSub, IDM_OPTION_RAINBOW, MF_BYCOMMAND|MF_CHECKED);
#ifdef DEBUG
            CheckMenuRadioItem(hmenuSub, IDM_OPTION_REALDATA, IDM_OPTION_ONLYDIRS, IDM_OPTION_REALDATA + g_fake_data, MF_BYCOMMAND|MF_CHECKED);
            if (g_fake_data)
            {
                EnableMenuItem(hmenuSub, IDM_OPEN_FILE, MF_BYCOMMAND|MF_GRAYED);
                EnableMenuItem(hmenuSub, IDM_OPEN_DIRECTORY, MF_BYCOMMAND|MF_GRAYED);
                EnableMenuItem(hmenuSub, IDM_RECYCLE_ENTRY, MF_BYCOMMAND|MF_GRAYED);
                EnableMenuItem(hmenuSub, IDM_DELETE_ENTRY, MF_BYCOMMAND|MF_GRAYED);
                EnableMenuItem(hmenuSub, IDM_EMPTY_RECYCLEBIN, MF_BYCOMMAND|MF_GRAYED);
            }
#endif

            MakeMenuPretty(hmenuSub);

            if (recycle)
            {
                WCHAR sz[100];
                MENUITEMINFO mii = { sizeof(mii) };
                mii.fMask = MIIM_STRING;
                mii.dwTypeData = sz;
                mii.cch = _countof(sz);
                if (GetMenuItemInfo(hmenuSub, IDM_EMPTY_RECYCLEBIN, false, &mii))
                {
                    WCHAR sz2[100];
                    std::wstring size;
                    std::wstring units;
                    FormatSize(recycle->GetSize(), size, units);
                    swprintf_s(sz2, _countof(sz2), TEXT("%s (%s %s)"), sz, size.c_str(), units.c_str());

                    mii.fMask = MIIM_FTYPE|MIIM_STRING;
                    mii.dwTypeData = sz2;
                    mii.cch = UINT(wcslen(sz2));
                    mii.fType = MFT_STRING;
                    SetMenuItemInfo(hmenuSub, IDM_EMPTY_RECYCLEBIN, false, &mii);
                }
            }

            const UINT idm = TrackPopupMenu(hmenuSub, TPM_RIGHTBUTTON|TPM_RETURNCMD, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
            switch (idm)
            {
            case IDM_RESCAN:
                if (dir)
                    Rescan(std::static_pointer_cast<DirNode>(node));
                break;
            case IDM_OPEN_DIRECTORY:
            case IDM_OPEN_FILE:
                if (node)
                {
                    if (dir && dir->IsRecycleBin())
                        ShellOpenRecycleBin(m_hwnd);
                    else
                        ShellOpen(m_hwnd, path.c_str());
                }
                break;

            case IDM_RECYCLE_ENTRY:
                if (node && ShellRecycle(m_hwnd, path.c_str()))
                    DeleteNode(node);
                break;
            case IDM_DELETE_ENTRY:
                if (node && ShellDelete(m_hwnd, path.c_str()))
                    DeleteNode(node);
                break;
            case IDM_EMPTY_RECYCLEBIN:
                if (recycle)
                {
                    path = recycle->GetParent()->GetName();
                    if (ShellEmptyRecycleBin(m_hwnd, path.c_str()))
                        recycle->UpdateRecycleBin();
                }
                break;

            case IDM_HIDE_DIRECTORY:
            case IDM_SHOW_DIRECTORY:
                if (dir)
                {
                    dir->Hide(!dir->IsHidden());
                    InvalidateRect(m_hwnd, nullptr, false);
                }
                break;

            case IDM_OPTION_COMPRESSED:
                g_use_compressed_size = !g_use_compressed_size;
                WriteRegLong(TEXT("UseCompressedSize"), g_use_compressed_size);
                if (IDYES == MessageBox(m_hwnd, TEXT("The setting will take effect in the next scan.\n\nRescan now?"), TEXT("Confirm Rescan"), MB_YESNOCANCEL|MB_ICONQUESTION))
                    Refresh(true/*all*/);
                break;
            case IDM_OPTION_FREESPACE:
                g_show_free_space = !g_show_free_space;
                WriteRegLong(TEXT("ShowFreeSpace"), g_show_free_space);
                InvalidateRect(m_hwnd, nullptr, false);
                break;
            case IDM_OPTION_NAMES:
                g_show_names = !g_show_names;
                WriteRegLong(TEXT("ShowNames"), g_show_names);
                InvalidateRect(m_hwnd, nullptr, false);
                break;
            case IDM_OPTION_RAINBOW:
                g_rainbow = !g_rainbow;
                WriteRegLong(TEXT("Rainbow"), g_rainbow);
                InvalidateRect(m_hwnd, nullptr, false);
                break;

#ifdef DEBUG
            case IDM_OPTION_REALDATA:
            case IDM_OPTION_SIMULATED:
            case IDM_OPTION_COLORWHEEL:
            case IDM_OPTION_EMPTYDRIVE:
            case IDM_OPTION_ONLYDIRS:
                {
                    const long fake_data = idm - IDM_OPTION_REALDATA;
                    if (fake_data != g_fake_data)
                    {
                        g_fake_data = fake_data;
                        WriteRegLong(TEXT("DbgFakeData"), g_fake_data);
                        Refresh(true/*all*/);
                    }
                }
                break;
#endif
            }

            DestroyMenu(hmenu);
        }
        break;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO* const p = reinterpret_cast<MINMAXINFO*>(lParam);
            p->ptMinTrackSize.x = m_dpi.Scale(480);
            p->ptMinTrackSize.y = m_dpi.Scale(360);
        }
        break;

    case WM_WINDOWPOSCHANGED:
        m_sizeTracker.OnSize();
        goto LDefault;

    case WM_SIZE:
        {
            m_directRender.ResizeDeviceResources();

            RECT rcClient;
            GetClientRect(m_hwnd, &rcClient);
            OnLayout(&rcClient);

            if (m_hover_node)
            {
                m_hover_node.reset();
                m_hover_free = false;
                InvalidateRect(m_hwnd, nullptr, false);
            }
        }
        goto LDefault;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_F5:
            Refresh();
            break;
        case VK_UP:
            Up();
            break;
        case VK_LEFT:
        case VK_BACK:
            Back();
            break;
        case VK_RIGHT:
            Forward();
            break;
        default:
            goto LDefault;
        }
        break;

    case WM_COMMAND:
        {
            const WORD id = GET_WM_COMMAND_ID(wParam, lParam);
            const HWND hwndCtrl = GET_WM_COMMAND_HWND(wParam, lParam);
            const WORD code = GET_WM_COMMAND_CMD(wParam, lParam);
            OnCommand(id, hwndCtrl, code);
        }
        break;

    case WM_DPICHANGED:
    case WMU_DPICHANGED:
        {
            assert(!m_inWmDpiChanged);
            const bool wasIn = m_inWmDpiChanged;
            m_inWmDpiChanged = true;

            OnDpiChanged(DpiScaler(wParam));

            RECT rc;
            const RECT* prc = LPCRECT(lParam);
            if (!prc)
            {
                GetWindowRect(m_hwnd, &rc);
                prc = &rc;
            }

            SetWindowPos(m_hwnd, 0, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_DRAWFRAME|SWP_NOACTIVATE|SWP_NOZORDER);

            m_inWmDpiChanged = wasIn;
        }
        break;

    case WM_CREATE:
        SendMessage(m_hwnd, WM_SETICON, true, LPARAM(LoadImage(m_hinst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 0, 0, 0)));
        SendMessage(m_hwnd, WM_SETICON, false, LPARAM(LoadImage(m_hinst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0)));
        goto LDefault;

    default:
LDefault:
        return DefWindowProc(m_hwnd, msg, wParam, lParam);
    }

    return 0;
}

void MainWindow::OnCommand(WORD id, HWND hwndCtrl, WORD code)
{
    switch (id)
    {
    case IDM_REFRESH:
        Refresh();
        break;
    case IDM_UP:
        Up();
        break;
    case IDM_BACK:
        Back();
        break;
    case IDM_SUMMARY:
        Summary();
        break;
    case IDM_APPWIZ:
        ShellOpen(m_hwnd, TEXT("appwiz.cpl"));
        break;
    case IDM_FOLDER:
        {
            std::wstring path;
            if (ShellBrowseForFolder(m_hwnd, TEXT("Choose Folder to Scan"), path))
            {
                const WCHAR* paths[] = { path.c_str(), nullptr };
                Scan(_countof(paths) - 1, paths, false/*rescan*/);
            }
        }
        break;
    default:
        if (id >= IDM_DRIVE_FIRST && id <= IDM_DRIVE_LAST)
        {
            const size_t index = id - IDM_DRIVE_FIRST;
            if (index < m_drives.size())
            {
                const WCHAR* drive = m_drives[index].c_str();
                Scan(1, &drive, false);
            }
        }
        break;
    }
}

void MainWindow::OnDpiChanged(const DpiScaler& dpi)
{
    m_dpi = dpi;

    if (m_hfont)
        DeleteFont(m_hfont);

    m_hfont = MakeFont(dpi, 10);

    {
        SIZE size;
        HDC hdc = GetDC(m_hwnd);
        SaveDC(hdc);
        SelectFont(hdc, m_hfont);

        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        m_top_reserve = tm.tmHeight; // Space for full path.
        m_margin_reserve = dpi.Scale(3);

        LONG cxMax = 0;
        for (WCHAR ch = '0'; ch <= '9'; ++ch)
        {
            GetTextExtentPoint32(hdc, &ch, 1, &size);
            if (cxMax < size.cx)
                cxMax = size.cx;
        }
        m_cxNumberArea = 9 * cxMax;

        GetTextExtentPoint32(hdc, TEXT(",,."), 3, &size);
        m_cxNumberArea += size.cx;

        RestoreDC(hdc, -1);
        ReleaseDC(m_hwnd, hdc);
    }

    m_sunburst.OnDpiChanged(dpi);
    m_buttons.OnDpiChanged(dpi);
}

void MainWindow::OnLayout(RECT* prc)
{
    RECT rc;
    const LONG dim = m_dpi.Scale(32);
    const LONG margin = m_dpi.Scale(8);

    prc->top += m_top_reserve;

    m_buttons.Attach(m_hwnd);

    rc.right = prc->right - margin;
    rc.top = prc->top + m_top_reserve + m_margin_reserve * 2;
    rc.left = rc.right - dim;
    rc.bottom = rc.top + dim;
    const WCHAR* rescan_desc = (m_roots.size() > 1) ? TEXT("Rescan Folders") : TEXT("Rescan Folder");
    m_buttons.AddButton(IDM_REFRESH, rc, nullptr, rescan_desc, MakeRefreshIcon);

    OffsetRect(&rc, 0, (dim + margin));
    m_buttons.AddButton(IDM_BACK, rc, nullptr, TEXT("Back"), MakeBackIcon);

    OffsetRect(&rc, 0, (dim + margin));
    m_buttons.AddButton(IDM_UP, rc, nullptr, TEXT("Parent Folder"), MakeUpIcon);

    rc.right = prc->right - margin;
    rc.bottom = prc->bottom - m_margin_reserve * 6 - m_top_reserve * 4;
    rc.left = rc.right - dim;
    rc.top = rc.bottom - dim;
    m_buttons.AddButton(IDM_APPWIZ, rc, nullptr, TEXT("Programs and Features"), MakeAppsIcon);

    rc.left = prc->left + margin;
    rc.bottom = prc->bottom - margin;
    rc.right = rc.left + (dim * 5 / 2);
    rc.top = rc.bottom - dim;
    m_buttons.AddButton(IDM_SUMMARY, rc, TEXT("Summary"), TEXT("Summary of Local Drives"));

    rc.left = prc->left + margin;
    rc.top = prc->top + m_top_reserve + m_margin_reserve + m_top_reserve + m_top_reserve + margin;
    rc.right = rc.left + dim;
    rc.bottom = rc.top + dim;
    m_buttons.AddButton(IDM_FOLDER, rc, nullptr, TEXT("Scan Folder"), MakeFolderIcon);
    OffsetRect(&rc, 0, dim + margin);

    for (UINT ii = 0; ii < m_drives.size(); ++ii)
    {
        if (rc.bottom > prc->bottom - margin - dim - margin)
            break;
        std::wstring desc = TEXT("Scan ");
        desc.append(m_drives[ii].c_str());
        m_buttons.AddButton(IDM_DRIVE_FIRST + ii, rc, m_drives[ii].c_str(), desc.c_str());
        OffsetRect(&rc, 0, dim + margin);
    }
}

LRESULT MainWindow::OnDestroy()
{
    m_sizeTracker.OnDestroy();
    m_directRender.ReleaseDeviceResources();
    if (m_hfont)
    {
        DeleteFont(m_hfont);
        m_hfont = 0;
    }
    return 0;
}

LRESULT MainWindow::OnNcDestroy()
{
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0);
    delete this;
    PostQuitMessage(0);
    return 0;
}

void MainWindow::DeleteNode(const std::shared_ptr<Node>& node)
{
    assert(node);
    if (!node)
        return;

    const std::shared_ptr<Node> parent = node->GetParent();
    assert(parent);
    if (!parent || !parent->AsDir())
        return;

    {
        std::lock_guard<std::recursive_mutex> lock(m_ui_mutex);
        parent->AsDir()->DeleteChild(node);
    }

    InvalidateRect(m_hwnd, nullptr, false);
}

void MainWindow::UpdateRecycleBin(const std::shared_ptr<RecycleBinNode>& recycle)
{
    assert(recycle);
    if (!recycle)
        return;

    assert(recycle->IsRecycleBin());
    if (!recycle->IsRecycleBin())
        return;

    {
        std::lock_guard<std::recursive_mutex> lock(m_ui_mutex);
        recycle->UpdateRecycleBin();
    }

    InvalidateRect(m_hwnd, nullptr, false);
}

//----------------------------------------------------------------------------
// MakeUi.

HWND MakeUi(HINSTANCE hinst, int argc, const WCHAR** argv)
{
    MainWindow* p = new MainWindow(hinst);

    HWND hwnd = p->Create();

    if (hwnd)
    {
        SetFocus(hwnd);
        p->Scan(argc, argv, false/*rescan*/);
    }

    return hwnd;
}

