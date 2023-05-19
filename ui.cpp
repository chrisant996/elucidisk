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

//----------------------------------------------------------------------------
// MainWindow.

class MainWindow
{
public:
                            MainWindow() {}

    HWND                    Create(HINSTANCE hinst);
    void                    Scan(int argc, const WCHAR** argv);

protected:
                            ~MainWindow() {}

    LRESULT                 WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void                    OnCommand(WORD id, HWND hwndCtrl, WORD code);
    void                    OnDpiChanged(WORD dpi);
    void                    OnLayout(RECT* prc);
    LRESULT                 OnDestroy();
    LRESULT                 OnNcDestroy();

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND                    m_hwnd = 0;
    DpiScaler               m_dpi;

    std::vector<std::shared_ptr<DirNode>> m_roots;

    DirectHwndRenderTarget  m_directRender;

    MainWindow(const MainWindow&) = delete;
    const MainWindow& operator=(const MainWindow&) = delete;
};

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
        OnDpiChanged(__GetDpiForWindow(hwnd));

// TODO: Real size.
        const int cx = m_dpi.Scale(480);
        const int cy = m_dpi.Scale(480);

        RECT rcDesk;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesk, 0);

        const LONG cxWork = rcDesk.right - rcDesk.left;
        const LONG cyWork = rcDesk.bottom - rcDesk.top;
        const LONG xx = rcDesk.left + (cxWork - cx) / 2;
        const LONG yy = rcDesk.top + (cyWork - cy) / 2;
        SetWindowPos(hwnd, 0, xx, yy, cx, cy, SWP_NOZORDER);

        ShowWindow(hwnd, SW_SHOW);
    }

    return hwnd;
}

void MainWindow::Scan(int argc, const WCHAR** argv)
{
    m_roots.clear();

// TODO: Asynchronous scanning.
// TODO: Progress feedback.
    if (argc)
    {
        for (int ii = 0; ii < argc; ++ii)
            m_roots.emplace_back(::Scan(argv[ii]));
    }
    else
    {
        m_roots.emplace_back(::Scan(nullptr));
    }

    InvalidateRect(m_hwnd, nullptr, false);
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

            // D2D rendering.

            if (SUCCEEDED(m_directRender.CreateDeviceResources(m_hwnd)))
            {
                ID2D1RenderTarget* const pTarget = m_directRender.Target();
                pTarget->BeginDraw();

                pTarget->SetTransform(D2D1::Matrix3x2F::Identity());
                pTarget->Clear(D2D1::ColorF(D2D1::ColorF::Gray));

                D2D1_SIZE_F rtSize = pTarget->GetSize();

                ID2D1Layer* pLayer = nullptr;
                pTarget->CreateLayer(&pLayer);

                pTarget->PushLayer(D2D1::LayerParameters(D2D1::RectF(0, 0, rtSize.width, rtSize.height),
                                                         0,
                                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                         D2D1::Matrix3x2F::Identity(),
                                                         0.6f),
                                   pLayer);

                for (LONG ii = 0; ii < LONG(rtSize.width); ii += 50)
                {
                    pTarget->DrawLine(D2D1::Point2F(0 + float(ii), 0),
                                      D2D1::Point2F(rtSize.width - float(ii), rtSize.height),
                                      m_directRender.LineBrush(),
                                      3.0);
                }

                pTarget->PopLayer();
                ReleaseI(pLayer);

                if (FAILED(pTarget->EndDraw()))
                    m_directRender.ReleaseDeviceResources();
            }

            // GDI painting.

            {
                TEXTMETRIC tm;
// TODO: SelectFont(ps.hdc, ...);
                GetTextMetrics(ps.hdc, &tm);

                std::wstring s(TEXT("Hello World"));

                SIZE size;
                GetTextExtentPoint32(ps.hdc, s.c_str(), int(s.length()), &size);

                RECT rc;
                GetClientRect(m_hwnd, &rc);

                const LONG xx = rc.left + ((rc.right - rc.left) - size.cx) / 2;
                const LONG yy = rc.top + ((rc.bottom - rc.top) - size.cy) / 2;
                ExtTextOut(ps.hdc, xx, yy, 0, &rc, s.c_str(), int(s.length()), 0);
            }

            RestoreDC(ps.hdc, -1);
            EndPaint(m_hwnd, &ps);
        }
        break;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO* const p = reinterpret_cast<MINMAXINFO*>(lParam);
            p->ptMinTrackSize.x = m_dpi.Scale(320);
            p->ptMinTrackSize.y = m_dpi.Scale(240);
        }
        break;

    case WM_CREATE:
// TODO: SendMessage(WM_SETICON, true, LPARAM(LoadImage(IDI_MAINICON, IMAGE_ICON)));
// TODO: SendMessage(WM_SETICON, false, LPARAM(LoadImage(IDI_MAINICON, IMAGE_ICON, 16, 16)));
        m_directRender.CreateDeviceResources(m_hwnd);
        goto LDefault;

    case WM_SIZE:
        m_directRender.ResizeDeviceResources();
        goto LDefault;

    case WM_COMMAND:
        {
            const WORD id = GET_WM_COMMAND_ID(wParam, lParam);
            const HWND hwndCtrl = GET_WM_COMMAND_HWND(wParam, lParam);
            const WORD code = GET_WM_COMMAND_CMD(wParam, lParam);
            OnCommand(id, hwndCtrl, code);
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

void MainWindow::OnDpiChanged(WORD dpi)
{
    m_dpi = dpi;

#if 0
    const WORD realDpi = __GetDpiForWindow(m_hwnd);
// TODO: Create font.
#endif
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

HWND MakeUi(HINSTANCE hinst, int argc, const WCHAR** argv)
{
    MainWindow* p = new MainWindow();

    HWND hwnd = p->Create(hinst);

    if (hwnd)
    {
        SetFocus(hwnd);
        p->Scan(argc, argv);
    }

    return hwnd;
}

