// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "scan.h"
#include "sunburst.h"
#include "actions.h"
#include "ui.h"
#include "sunburst.h"
#include <assert.h>
#include <windowsx.h>
#include <iosfwd>

//----------------------------------------------------------------------------
// ScannerThread.

class ScannerThread
{
public:
                            ScannerThread(std::recursive_mutex& ui_mutex);
                            ~ScannerThread() { Stop(); }

    std::vector<std::shared_ptr<DirNode>> Start(int argc, const WCHAR** argv);
    void                    Stop();

    bool                    IsComplete();

protected:
    static void             ThreadProc(ScannerThread* pThis);

private:
    HANDLE                  m_hWake;
    HANDLE                  m_hStop;
    volatile LONG           m_generation = 0;
    size_t                  m_cursor = 0;
    std::vector<std::shared_ptr<DirNode>> m_roots;
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
            roots.emplace_back(MakeRoot(argv[ii]));
    }
    else
    {
        roots.emplace_back(MakeRoot(nullptr));
    }

    if (!m_thread)
        m_thread = std::make_unique<std::thread>(ThreadProc, this);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_roots = roots;
        m_cursor = 0;
        InterlockedIncrement(&m_generation);
    }

    SetEvent(m_hWake);

    return roots;
}

void ScannerThread::Stop()
{
    if (m_thread)
    {
        SetEvent(m_hStop);
        InterlockedIncrement(&m_generation);
        m_thread->join();
        m_roots.clear();
        m_cursor = 0;
        ResetEvent(m_hStop);
    }
}

bool ScannerThread::IsComplete()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_roots.empty();
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
                    pThis->m_roots.clear();
                    pThis->m_cursor = 0;
                    break;
                }

                root = pThis->m_roots[pThis->m_cursor++];
            }

            Scan(root, generation, &pThis->m_generation, pThis->m_ui_mutex);
        }
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
                            MainWindow();

    HWND                    Create(HINSTANCE hinst);
    void                    Scan(int argc, const WCHAR** argv, bool rescan);

protected:
                            ~MainWindow() {}

    LRESULT                 WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void                    OnCommand(WORD id, HWND hwndCtrl, WORD code);
    void                    OnDpiChanged(const DpiScaler& dpi);
    void                    OnLayout(RECT* prc);
    LRESULT                 OnDestroy();
    LRESULT                 OnNcDestroy();

    void                    DrawNodeInfo(HDC hdc, const RECT& rc, const std::shared_ptr<Node>& node);

    void                    Expand(const std::shared_ptr<Node>& node);
    void                    Rescan();

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND                    m_hwnd = 0;
    DpiScaler               m_dpi;
    bool                    m_inWmDpiChanged = false;

    std::recursive_mutex    m_ui_mutex; // Synchronize m_scanner vs m_sunburst.

    std::vector<std::shared_ptr<DirNode>> m_original_roots;
    std::vector<std::shared_ptr<DirNode>> m_roots;
    ScannerThread           m_scanner;

    DirectHwndRenderTarget  m_directRender;
    Sunburst                m_sunburst;

    std::shared_ptr<Node>   m_hover_node;

    MainWindow(const MainWindow&) = delete;
    const MainWindow& operator=(const MainWindow&) = delete;
};

MainWindow::MainWindow()
: m_scanner(m_ui_mutex)
{
}

HWND MainWindow::Create(HINSTANCE hinst)
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
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = HBRUSH(COLOR_WINDOW + 1 );
        wc.lpszClassName = c_class;
        RegisterClass(&wc);
        s_registered = true;
    }

    assert(!m_hwnd);
    const HWND hwnd = CreateWindowExW(0, c_class, c_caption, c_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hinst, this);
    assert(hwnd == m_hwnd);

    if (hwnd)
    {
        OnDpiChanged(DpiScaler(__GetDpiForWindow(hwnd)));

        RECT rcDesk;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesk, 0);

        const LONG cxWork = rcDesk.right - rcDesk.left;
        const LONG cyWork = rcDesk.bottom - rcDesk.top;

// TODO: Remember previous size.
        const LONG cx = m_dpi.Scale(std::max<int>(320, cxWork / 2));
        const LONG cy = m_dpi.Scale(std::max<int>(320, cyWork / 2));

        const LONG xx = rcDesk.left + (cxWork - cx) / 2;
        const LONG yy = rcDesk.top + (cyWork - cy) / 2;
        SetWindowPos(hwnd, 0, xx, yy, cx, cy, SWP_NOZORDER);

        ShowWindow(hwnd, SW_SHOW);
    }

    return hwnd;
}

void MainWindow::Scan(int argc, const WCHAR** argv, bool rescan)
{
    m_roots = m_scanner.Start(argc, argv);
    if (!rescan)
        m_original_roots = m_roots;
    SetTimer(m_hwnd, TIMER_PROGRESS, INTERVAL_PROGRESS, nullptr);
    InvalidateRect(m_hwnd, nullptr, false);
}

void MainWindow::Expand(const std::shared_ptr<Node>& node)
{
    if (!node || node->AsFile())
        return;

    const bool up = (m_roots.size() == 1 && node == m_roots[0]);
    if (up && !node->GetParent())
    {
        if (m_original_roots.size() == 1 && node == m_original_roots[0])
            return;

        m_roots = m_original_roots;
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

        m_roots.clear();
        m_roots.emplace_back(dir);
    }

    InvalidateRect(m_hwnd, nullptr, false);
}

void MainWindow::Rescan()
{
// TODO: Rescan current root(s) instead, clearing them first.  That gets
// tricky if a scan is already in progress.  Maybe convert the recursive Scan
// into a queue of DirNode's to be scanned, and insert the current roots at
// the head of the queue?  And what if any of those DirNode's are already in
// progress being scanned, or haven't been scanned yet?
    std::vector<const WCHAR*> paths;
    for (const auto root : m_original_roots)
        paths.emplace_back(root->GetName());

    Scan(int(paths.size()), paths.data(), true/*rescan*/);
}

void MainWindow::DrawNodeInfo(HDC hdc, const RECT& rc, const std::shared_ptr<Node>& node)
{
    TEXTMETRIC tm;
    RECT rcLine = rc;
    std::wstring text;

// TODO: SelectFont(hdc, ...);
    GetTextMetrics(hdc, &tm);

    rcLine.left += m_dpi.Scale(4);
    rcLine.bottom = rcLine.top + tm.tmHeight;

    node->GetFullPath(text);
    ExtTextOut(hdc, rcLine.left, rcLine.top, ETO_OPAQUE, &rcLine, text.c_str(), int(text.length()), nullptr);

    OffsetRect(&rcLine, 0, tm.tmHeight);
    rcLine.right = rcLine.left + m_dpi.Scale(100);

    WCHAR sz[100];

    sz[0] = 0;
    if (node)
    {
        if (node->AsDir())
            swprintf_s(sz, _countof(sz), TEXT("%.3f MB"), FLOAT(node->AsDir()->GetSize() / 1024 / 1024));
        else if (node->AsFile())
            swprintf_s(sz, _countof(sz), TEXT("%.3f MB"), FLOAT(node->AsFile()->GetSize() / 1024 / 1024));
        else if (node->AsFreeSpace())
            swprintf_s(sz, _countof(sz), TEXT("%.3f MB"), FLOAT(node->AsFreeSpace()->GetFreeSize() / 1024 / 1024));
    }
    ExtTextOut(hdc, rcLine.left, rcLine.top, ETO_OPAQUE, &rcLine, sz, int(wcslen(sz)), nullptr);

    OffsetRect(&rcLine, 0, tm.tmHeight);

    sz[0] = 0;
    if (node && node->AsDir())
    {
        swprintf_s(sz, _countof(sz), TEXT("%llu Files"), node->AsDir()->CountFiles());
    }
    ExtTextOut(hdc, rcLine.left, rcLine.top, ETO_OPAQUE, &rcLine, sz, int(wcslen(sz)), nullptr);
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

            TEXTMETRIC tm;
// TODO: SelectFont(ps.hdc, ...);
            GetTextMetrics(ps.hdc, &tm);

            const LONG top_reserve = tm.tmHeight; // Space for full path.
            const LONG margin_reserve = m_dpi.Scale(3);

            // D2D rendering.

            if (SUCCEEDED(m_directRender.CreateDeviceResources(m_hwnd)))
            {
                ID2D1RenderTarget* const pTarget = m_directRender.Target();
                pTarget->AddRef();

                pTarget->BeginDraw();

                pTarget->SetTransform(D2D1::Matrix3x2F::Identity());
                pTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

                D2D1_SIZE_F rtSize = pTarget->GetSize();
                rtSize.height -= margin_reserve + top_reserve;

                const FLOAT extent = std::min<FLOAT>(rtSize.width, rtSize.height);
                const FLOAT xx = (rtSize.width - extent) / 2;
                const FLOAT yy = margin_reserve + top_reserve + (rtSize.height - extent) / 2;
                const D2D1_RECT_F bounds = D2D1::RectF(xx, yy, xx + extent, yy + extent);

                static size_t s_gen = 0;
                const size_t gen = ++s_gen;

                Sunburst sunburst;
                {
                    std::lock_guard<std::recursive_mutex> lock(m_ui_mutex);

// TODO: Only rebuild rings when something has changed.
                    sunburst.Init(m_sunburst); // Solve chicken and egg problem: HitTest depends on RenderRings.
                    sunburst.BuildRings(m_roots);
                    m_hover_node = sunburst.HitTest(pt);
                    sunburst.RenderRings(m_directRender, bounds, m_hover_node);
                }

                if (gen == s_gen)
                    m_sunburst = std::move(sunburst);
                else
                    m_hover_node.reset();

                if (FAILED(pTarget->EndDraw()))
                    m_directRender.ReleaseDeviceResources();

                pTarget->Release();
            }

            // GDI painting.

            {
                DrawNodeInfo(ps.hdc, rcClient, m_hover_node);

                static int s_counter = 0;
                WCHAR sz[100];
                s_counter++;
                wsprintf(sz, TEXT("Hello - %u"), s_counter);

                SIZE size;
                GetTextExtentPoint32(ps.hdc, sz, int(wcslen(sz)), &size);

                rcClient.top += margin_reserve + top_reserve;
                const LONG xx = rcClient.left + ((rcClient.right - rcClient.left) - size.cx) / 2;
                const LONG yy = rcClient.top + ((rcClient.bottom - rcClient.top) - size.cy) / 2;
                SetBkMode(ps.hdc, TRANSPARENT);
                ExtTextOut(ps.hdc, xx, yy, 0, &rcClient, sz, int(wcslen(sz)), 0);
            }

            RestoreDC(ps.hdc, -1);
            EndPaint(m_hwnd, &ps);
        }
        break;

    case WM_MOUSEMOVE:
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);

            std::shared_ptr<Node> hover(m_hover_node);
            m_hover_node = m_sunburst.HitTest(pt);

// TODO: Sometimes no highlight is painted even though it should be.
            if (hover != m_hover_node)
                InvalidateRect(m_hwnd, nullptr, false);
        }
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
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            std::shared_ptr<Node> node = m_sunburst.HitTest(pt);
            Expand(node);
        }
        break;

// TODO: Context menu.
    case WM_RBUTTONDOWN:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            std::shared_ptr<Node> node = m_sunburst.HitTest(pt);
            if (node)
            {
                DirNode* dir = node->AsDir();
                if (dir)
                {
                    dir->Hide(!dir->Hidden());
                    InvalidateRect(m_hwnd, nullptr, false);
                }
            }
        }
        break;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO* const p = reinterpret_cast<MINMAXINFO*>(lParam);
            p->ptMinTrackSize.x = m_dpi.Scale(320);
            p->ptMinTrackSize.y = m_dpi.Scale(320);
        }
        break;

    case WM_CREATE:
// TODO: SendMessage(WM_SETICON, true, LPARAM(LoadImage(IDI_MAINICON, IMAGE_ICON)));
// TODO: SendMessage(WM_SETICON, false, LPARAM(LoadImage(IDI_MAINICON, IMAGE_ICON, 16, 16)));
        m_directRender.CreateDeviceResources(m_hwnd);
        goto LDefault;

    case WM_SIZE:
        m_directRender.ResizeDeviceResources();
        if (m_hover_node)
        {
            m_hover_node.reset();
            InvalidateRect(m_hwnd, nullptr, false);
        }
        goto LDefault;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_F5:
            Rescan();
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

    default:
LDefault:
        return DefWindowProc(m_hwnd, msg, wParam, lParam);
    }

    return 0;
}

void MainWindow::OnCommand(WORD id, HWND hwndCtrl, WORD code)
{
// TODO: Handle command IDs.
}

void MainWindow::OnDpiChanged(const DpiScaler& dpi)
{
    m_dpi = dpi;

#if 0
    const WORD realDpi = __GetDpiForWindow(m_hwnd);
// TODO: Create font.
#endif

    m_sunburst.OnDpiChanged(dpi);
}

LRESULT MainWindow::OnDestroy()
{
    m_directRender.ReleaseDeviceResources();
    return 0;
}

LRESULT MainWindow::OnNcDestroy()
{
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0);
    delete this;
    PostQuitMessage(0);
    return 0;
}

//----------------------------------------------------------------------------
// MakeUi.

HWND MakeUi(HINSTANCE hinst, int argc, const WCHAR** argv)
{
    MainWindow* p = new MainWindow();

    HWND hwnd = p->Create(hinst);

    if (hwnd)
    {
        SetFocus(hwnd);
        p->Scan(argc, argv, false/*rescan*/);
    }

    return hwnd;
}

