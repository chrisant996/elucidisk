// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include <windowsx.h>
#include <wincodec.h>

#ifndef ILC_COLORMASK
#define ILC_COLORMASK   0x00FE
#endif

#ifndef BORDERX_PEN
#define BORDERX_PEN     32
#endif

WORD __GetHdcDpi(HDC hdc)
{
    const WORD dxLogPixels = static_cast<WORD>(GetDeviceCaps(hdc, LOGPIXELSX));
#ifdef DEBUG
    const WORD dyLogPixels = static_cast<WORD>(GetDeviceCaps(hdc, LOGPIXELSY));
    assert(dxLogPixels == dyLogPixels);
#endif
    return dxLogPixels;
}

class User32
{
public:
                            User32();

    WORD                    GetDpiForSystem();
    WORD                    GetDpiForWindow(HWND hwnd);
    int                     GetSystemMetricsForDpi(int nIndex, UINT dpi);
    bool                    SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi);
    bool                    IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT context);
    bool                    AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT contextA, DPI_AWARENESS_CONTEXT contextB);
    DPI_AWARENESS_CONTEXT   SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context);
    DPI_AWARENESS_CONTEXT   GetWindowDpiAwarenessContext(HWND hwnd);
    bool                    EnableNonClientDpiScaling(HWND hwnd);
    bool                    EnablePerMonitorMenuScaling();

private:
    bool                    Initialize();

private:
    DWORD                   m_dwErr = 0;
    HMODULE                 m_hLib = 0;
    bool                    m_fInitialized = false;
    union
    {
        FARPROC	proc[10];
        struct
        {
            UINT (WINAPI* GetDpiForSystem)();
            UINT (WINAPI* GetDpiForWindow)(HWND hwnd);
            int (WINAPI* GetSystemMetricsForDpi)(int nIndex, UINT dpi);
            BOOL (WINAPI* SystemParametersInfoForDpi)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi);
            BOOL (WINAPI* IsValidDpiAwarenessContext)(DPI_AWARENESS_CONTEXT context);
            BOOL (WINAPI* AreDpiAwarenessContextsEqual)(DPI_AWARENESS_CONTEXT contextA, DPI_AWARENESS_CONTEXT contextB);
            DPI_AWARENESS_CONTEXT (WINAPI* SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT context);
            DPI_AWARENESS_CONTEXT (WINAPI* GetWindowDpiAwarenessContext)(HWND hwnd);
            BOOL (WINAPI* EnableNonClientDpiScaling)(HWND hwnd);
            BOOL (WINAPI* EnablePerMonitorMenuScaling)();
        };
    } m_user32;
};

User32 g_user32;

User32::User32()
{
    ZeroMemory(&m_user32, sizeof(m_user32));

    Initialize();

    static_assert(_countof(m_user32.proc) == sizeof(m_user32) / sizeof(FARPROC), "mismatched FARPROC struct");
}

bool User32::Initialize()
{
    if (!m_fInitialized)
    {
        m_fInitialized = true;
        m_hLib = LoadLibrary(TEXT("user32.dll"));
        if (!m_hLib)
        {
            m_dwErr = GetLastError();
        }
        else
        {
            size_t cProcs = 0;

            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "GetDpiForSystem")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "GetDpiForWindow")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "GetSystemMetricsForDpi")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "SystemParametersInfoForDpi")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "IsValidDpiAwarenessContext")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "AreDpiAwarenessContextsEqual")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "SetThreadDpiAwarenessContext")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "GetWindowDpiAwarenessContext")))
                m_dwErr = GetLastError();
            if (!(m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "EnableNonClientDpiScaling")))
                m_dwErr = GetLastError();
            m_user32.proc[cProcs++] = GetProcAddress(m_hLib, "EnablePerMonitorMenuScaling"); // Optional: not an error if it's missing.
            assert(_countof(m_user32.proc) == cProcs);
        }
    }

    return !m_dwErr;
}

WORD User32::GetDpiForSystem()
{
    if (m_user32.GetDpiForSystem)
        return m_user32.GetDpiForSystem();

    const HDC hdc = GetDC(0);
    const WORD dpi = __GetHdcDpi(hdc);
    ReleaseDC(0, hdc);
    return dpi;
}

WORD User32::GetDpiForWindow(HWND hwnd)
{
    if (m_user32.GetDpiForWindow)
        return m_user32.GetDpiForWindow(hwnd);

    const HDC hdc = GetDC(hwnd);
    const WORD dpi = __GetHdcDpi(hdc);
    ReleaseDC(hwnd, hdc);
    return dpi;
}

int User32::GetSystemMetricsForDpi(int nIndex, UINT dpi)
{
    if (m_user32.GetSystemMetricsForDpi)
    {
        // Scale these ourselves because the OS doesn't seem to return them scaled.  ?!
        if (nIndex == SM_CXFOCUSBORDER || nIndex == SM_CYFOCUSBORDER)
            return HIDPIMulDiv(GetSystemMetrics(nIndex), dpi, 96);

        return m_user32.GetSystemMetricsForDpi(nIndex, dpi);
    }

    return GetSystemMetrics(nIndex);
}

bool User32::SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT _dpi)
{
    DpiScaler dpi(static_cast<WORD>(_dpi));
    DpiScaler dpiSystem(__GetDpiForSystem());

    switch (uiAction)
    {
    case SPI_GETICONTITLELOGFONT:
        if (SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni))
        {
            const LPLOGFONT plf = LPLOGFONT(pvParam);
            plf->lfHeight = dpi.ScaleFrom(plf->lfHeight, dpiSystem);
            return true;
        }
        break;

    case SPI_GETICONMETRICS:
        if (SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni))
        {
            const LPICONMETRICS pim = LPICONMETRICS(pvParam);
            pim->lfFont.lfHeight = dpi.ScaleFrom(pim->lfFont.lfHeight, dpiSystem);
            return true;
        }
        break;

    case SPI_GETNONCLIENTMETRICS:
        if (SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni))
        {
            const LPNONCLIENTMETRICS pncm = LPNONCLIENTMETRICS(pvParam);
            pncm->lfCaptionFont.lfHeight = dpi.ScaleFrom(pncm->lfCaptionFont.lfHeight, dpiSystem);
            pncm->lfMenuFont.lfHeight = dpi.ScaleFrom(pncm->lfMenuFont.lfHeight, dpiSystem);
            pncm->lfMessageFont.lfHeight = dpi.ScaleFrom(pncm->lfMessageFont.lfHeight, dpiSystem);
            pncm->lfSmCaptionFont.lfHeight = dpi.ScaleFrom(pncm->lfSmCaptionFont.lfHeight, dpiSystem);
            pncm->lfStatusFont.lfHeight = dpi.ScaleFrom(pncm->lfStatusFont.lfHeight, dpiSystem);
            return true;
        }
        break;
    }

    return false;
}

bool User32::IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT context)
{
    if (m_user32.IsValidDpiAwarenessContext)
        return m_user32.IsValidDpiAwarenessContext(context);

    return false;
}

bool User32::AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT contextA, DPI_AWARENESS_CONTEXT contextB)
{
    if (m_user32.AreDpiAwarenessContextsEqual)
        return m_user32.AreDpiAwarenessContextsEqual(contextA, contextB);

    return (contextA == contextB);
}

DPI_AWARENESS_CONTEXT User32::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context)
{
    if (m_user32.SetThreadDpiAwarenessContext)
        return m_user32.SetThreadDpiAwarenessContext(context);

    return DPI_AWARENESS_CONTEXT_UNAWARE;
}

DPI_AWARENESS_CONTEXT User32::GetWindowDpiAwarenessContext(HWND hwnd)
{
    if (m_user32.GetWindowDpiAwarenessContext)
        return m_user32.GetWindowDpiAwarenessContext(hwnd);

    return DPI_AWARENESS_CONTEXT_UNAWARE;
}

bool User32::EnableNonClientDpiScaling(HWND hwnd)
{
    if (m_user32.EnableNonClientDpiScaling)
        return m_user32.EnableNonClientDpiScaling(hwnd);

    return true;
}

bool User32::EnablePerMonitorMenuScaling()
{
    if (m_user32.EnablePerMonitorMenuScaling)
        return m_user32.EnablePerMonitorMenuScaling();

    return false;
}

WORD __GetDpiForSystem()
{
    return WORD(g_user32.GetDpiForSystem());
}

WORD __GetDpiForWindow(HWND hwnd)
{
    return WORD(g_user32.GetDpiForWindow(hwnd));
}

int __GetSystemMetricsForDpi(int nIndex, UINT dpi)
{
    return g_user32.GetSystemMetricsForDpi(nIndex, dpi);
}

bool __SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi)
{
    return g_user32.SystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, dpi);
}

bool __IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT context)
{
    return g_user32.IsValidDpiAwarenessContext(context);
}

bool __AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT contextA, DPI_AWARENESS_CONTEXT contextB)
{
    return g_user32.AreDpiAwarenessContextsEqual(contextA, contextB);
}

DPI_AWARENESS_CONTEXT __SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context)
{
    return g_user32.SetThreadDpiAwarenessContext(context);
}

DPI_AWARENESS_CONTEXT __GetWindowDpiAwarenessContext(HWND hwnd)
{
    return g_user32.GetWindowDpiAwarenessContext(hwnd);
}

bool __EnableNonClientDpiScaling(HWND hwnd)
{
    return g_user32.EnableNonClientDpiScaling(hwnd);
}

bool __EnablePerMonitorMenuScaling()
{
    return g_user32.EnablePerMonitorMenuScaling();
}

bool __IsHwndPerMonitorAware(HWND hwnd)
{
    const DPI_AWARENESS_CONTEXT context = __GetWindowDpiAwarenessContext(hwnd);

    return (__AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) ||
            __AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
}

ThreadDpiAwarenessContext::ThreadDpiAwarenessContext(const bool fUsePerMonitorAwareness)
{
    DPI_AWARENESS_CONTEXT const context = fUsePerMonitorAwareness ? DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE : DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;

    m_fRestore = true;
    m_context = __SetThreadDpiAwarenessContext(context);
}

ThreadDpiAwarenessContext::ThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context)
{
    if (context == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 && !__IsValidDpiAwarenessContext(context))
        context = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;

    m_fRestore = true;
    m_context = __SetThreadDpiAwarenessContext(context);
}

void ThreadDpiAwarenessContext::Restore()
{
    if (m_fRestore)
    {
        __SetThreadDpiAwarenessContext(m_context);
        m_fRestore = false;
    }
}

// HIDPISIGN and HIDPIABS ensure correct rounding for negative numbers passed
// into HIDPIMulDiv as x (round -1.5 to -1, 2.5 to round to 2, etc).  This is
// done by taking the absolute value of x and multiplying the result by the
// sign of x.  Y and z should never be negative, as y is the dpi of the
// device, and z is always 96 (100%).
inline int HIDPISIGN(int x)
{
    return (x < 0) ? -1 : 1;
}
inline int HIDPIABS(int x)
{
    return (x < 0) ? -x : x;
}

int HIDPIMulDiv(int x, int y, int z)
{
    assert(y);
    assert(z);
    // >>1 rounds up at 0.5, >>2 rounds up at 0.75, >>3 rounds up at 0.875
    //return (((HIDPIABS(x) * y) + (z >> 1)) / z) * HIDPISIGN(x);
    //return (((HIDPIABS(x) * y) + (z >> 2)) / z) * HIDPISIGN(x);
    return (((HIDPIABS(x) * y) + (z >> 3)) / z) * HIDPISIGN(x);
}

static float s_textScaleFactor = 0.0;

static float GetTextScaleFactor()
{
    if (s_textScaleFactor == 0.0)
    {
        s_textScaleFactor = 1.0;

        // I can't find any API that exposes the Text Scale Factor.
        // It's stored in the registry as the factor times 100 here:
        // Software\\Microsoft\\Accessibility\\TextScaleFactor
        SHKEY shkey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Accessibility"), 0, KEY_READ, &shkey) == ERROR_SUCCESS)
        {
            DWORD dwType;
            DWORD dwValue;
            DWORD dwSize = sizeof(dwValue);
            if (RegQueryValueEx(shkey, TEXT("TextScaleFactor"), 0, &dwType, LPBYTE(&dwValue), &dwSize) == ERROR_SUCCESS &&
                REG_DWORD == dwType &&
                sizeof(dwValue) == dwSize)
            {
                s_textScaleFactor = float(dwValue) / 100;
            }
        }
    }

    return s_textScaleFactor;
}

bool HIDPI_OnWmSettingChange()
{
    const float oldTextScaleFactor = s_textScaleFactor;
    s_textScaleFactor = 0.0;
    return (GetTextScaleFactor() != oldTextScaleFactor);
}

DpiScaler::DpiScaler()
{
    m_logPixels = 96;
#ifdef DEBUG
    m_fTextScaling = false;
#endif
}

DpiScaler::DpiScaler(WORD dpi)
{
    assert(dpi);
    m_logPixels = dpi;
#ifdef DEBUG
    m_fTextScaling = false;
#endif
}

DpiScaler::DpiScaler(WPARAM wParam)
{
    assert(wParam);
    assert(LOWORD(wParam));
    m_logPixels = LOWORD(wParam);
#ifdef DEBUG
    m_fTextScaling = false;
#endif
}

DpiScaler::DpiScaler(const DpiScaler& dpi)
{
    assert(!dpi.IsTextScaling());
    m_logPixels = dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = dpi.m_fTextScaling;
#endif
}

DpiScaler::DpiScaler(const DpiScaler& dpi, bool fTextScaling)
{
    assert(!dpi.IsTextScaling());
    m_logPixels = fTextScaling ? WORD(GetTextScaleFactor() * dpi.m_logPixels) : dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = dpi.m_fTextScaling;
#endif
}

DpiScaler::DpiScaler(DpiScaler&& dpi)
{
    assert(!dpi.IsTextScaling());
    m_logPixels = dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = dpi.m_fTextScaling;
#endif
}

bool DpiScaler::IsDpiEqual(UINT dpi) const
{
    assert(dpi);
    return dpi == m_logPixels;
}

bool DpiScaler::IsDpiEqual(const DpiScaler& dpi) const
{
    return (dpi.m_logPixels == m_logPixels);
}

DpiScaler& DpiScaler::operator=(WORD dpi)
{
    assert(dpi);
    m_logPixels = dpi;
#ifdef DEBUG
    m_fTextScaling = false;
#endif
    return *this;
}

DpiScaler& DpiScaler::operator=(const DpiScaler& dpi)
{
    m_logPixels = dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = dpi.m_fTextScaling;
#endif
    return *this;
}

DpiScaler& DpiScaler::operator=(DpiScaler&& dpi)
{
    m_logPixels = dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = dpi.m_fTextScaling;
#endif
    return *this;
}

void DpiScaler::OnDpiChanged(const DpiScaler& dpi)
{
    m_logPixels = dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = dpi.m_fTextScaling;
#endif
}

void DpiScaler::OnDpiChanged(const DpiScaler& dpi, bool fTextScaling)
{
    m_logPixels = fTextScaling ? WORD(GetTextScaleFactor() * dpi.m_logPixels) : dpi.m_logPixels;
#ifdef DEBUG
    m_fTextScaling = fTextScaling;
#endif
}

int DpiScaler::Scale(int n) const
{
    return HIDPIMulDiv(n, m_logPixels, 96);
}

float DpiScaler::ScaleF(float n) const
{
    return n * float(m_logPixels) / 96.0f;
}

int DpiScaler::ScaleTo(int n, DWORD dpi) const
{
    assert(dpi);
    return HIDPIMulDiv(n, dpi, m_logPixels);
}

int DpiScaler::ScaleTo(int n, const DpiScaler& dpi) const
{
    return HIDPIMulDiv(n, dpi.m_logPixels, m_logPixels);
}

int DpiScaler::ScaleFrom(int n, DWORD dpi) const
{
    assert(dpi);
    return HIDPIMulDiv(n, m_logPixels, dpi);
}

int DpiScaler::ScaleFrom(int n, const DpiScaler& dpi ) const
{
    return HIDPIMulDiv(n, m_logPixels, dpi.m_logPixels);
}

int DpiScaler::PointSizeToHeight(int nPointSize) const
{
    assert(nPointSize >= 1);
    return -MulDiv(nPointSize, m_logPixels, 72);
}

int DpiScaler::PointSizeToHeight(float pointSize) const
{
    assert(pointSize >= 1);
    return -MulDiv(int(pointSize * 10), m_logPixels, 720);
}

int DpiScaler::GetSystemMetrics(int nIndex) const
{
    return __GetSystemMetricsForDpi(nIndex, m_logPixels);
}

bool DpiScaler::SystemParametersInfo(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) const
{
    return __SystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, m_logPixels);
}

WPARAM DpiScaler::MakeWParam() const
{
    return MAKELONG(m_logPixels, m_logPixels);
}

static void InitBmiForRgbaDibSection(BITMAPINFO& bmi, LONG cx, LONG cy)
{
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = 0 - cy;    // negative = top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
}

static HBITMAP ScaleWicBitmapToBitmap(IWICImagingFactory* pFactory, IWICBitmap* pBmp, int cx, int cy)
{
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = 0 - cy;    // negative = top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* pBits;
    HBITMAP hBmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&pBits, nullptr, 0);
    SPI<IWICBitmapScaler> spScaler;
    bool ok = false;

    do
    {
        if (!hBmp)
            break;

        if (FAILED(pFactory->CreateBitmapScaler(&spScaler)))
            break;

        WICBitmapInterpolationMode const mode = WICBitmapInterpolationModeFant;
        if (FAILED(spScaler->Initialize(pBmp, cx, cy, mode)))
            break;

        WICRect rect = { 0, 0, cx, cy };
        if (FAILED(spScaler->CopyPixels(&rect, cx * 4, cx * cy * 4, pBits)))
            break;

        ok = true;
    }
    while (false);

    if (!ok)
    {
        DeleteBitmap(hBmp);
        hBmp = 0;
    }

    return hBmp;
}

bool HIDPI_StretchBitmap(HBITMAP* phbm, int cxDstImg, int cyDstImg, int cColumns, int cRows, COLORREF& crTransparent)
{
    // Check for caller errors.
    if (!phbm || !*phbm)
        return false;
    if (!cxDstImg && !cyDstImg)
        return false;
    if (!cColumns || !cRows)
        return false;

    // Get the bitmap object attributes.
    BITMAP bm;
    if ((sizeof(bm) != GetObject(*phbm, sizeof(bm), &bm)))
        return false;

    // The bitmap dimensions must be a multiple of the columns and rows.
    assert(!(bm.bmWidth % cColumns) && !(bm.bmHeight % cRows));

    const int cxSrcImg = bm.bmWidth / cColumns;
    const int cySrcImg = bm.bmHeight / cRows;

    // If one dimension was 0, infer it based on the other values.
    if (!cxDstImg)
        cxDstImg = HIDPIMulDiv(cyDstImg, cxSrcImg, cySrcImg);
    else if(!cyDstImg)
        cyDstImg = HIDPIMulDiv(cxDstImg, cySrcImg, cxSrcImg);

    // If the dimensions don't match, the bitmap needs to be scaled.
#ifndef DEBUG // Debug builds always scale, to ensure this code path is well exercised.
    if (cxSrcImg != cxDstImg || cySrcImg != cyDstImg)
#endif
    {
        // Create GDI and WIC objects to perform the scaling.
        const HDC hdcSrc = CreateCompatibleDC(0);
        const HDC hdcDst = CreateCompatibleDC(0);
        const HDC hdcResize = CreateCompatibleDC(0);

        SPI<IWICImagingFactory> spFactory;
        CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), (void**)&spFactory);

        // Ensure the input bitmap uses an alpha channel for transparency.
        HBITMAP hbmpInput = *phbm;
        HBITMAP hbmpConverted = 0;
        if (crTransparent != CLR_NONE)
        {
            BYTE* pbBits;
            BITMAPINFO bmi;
            InitBmiForRgbaDibSection(bmi, bm.bmWidth, bm.bmHeight);
            hbmpConverted = CreateDIBSection(hdcSrc, &bmi, DIB_RGB_COLORS, (void**)&pbBits, 0, 0);

            if (hbmpConverted &&
                GetDIBits(hdcSrc, hbmpInput, 0, bm.bmHeight, pbBits, &bmi, DIB_RGB_COLORS))
            {
                const BYTE bR = GetRValue(crTransparent);
                const BYTE bG = GetGValue(crTransparent);
                const BYTE bB = GetBValue(crTransparent);
                for (size_t c = bm.bmWidth * bm.bmHeight; c--;)
                {
                    if (pbBits[0] == bB && pbBits[1] == bG && pbBits[2] == bR)
                        pbBits[3] = 0;
                    else
                        pbBits[3] = 255;
                    pbBits += 4;
                }

                hbmpInput = hbmpConverted;
                crTransparent = CLR_NONE; // Clue in the caller what's just happened!
            }
            else
            {
                // Couldn't get the device independent bits, so we can't
                // massage the bitmap into the format required by WIC.  So
                // give up on using WIC and fall back to using StretchBlt,
                // which uses an ugly pixel-doubling algorithm.
                assert(false);
                spFactory.Release();
            }
        }

        // Create the WIC factory and bitmap.
        SPI<IWICBitmap> spBmpFull;
        if (spFactory)
        {
            if (FAILED(spFactory->CreateBitmapFromHBITMAP(hbmpInput, 0, WICBitmapUseAlpha, &spBmpFull)))
                spFactory.Release();
        }

        // Create GDI objects to perform the scaling.
        BITMAPINFO bmi;
        InitBmiForRgbaDibSection(bmi, cxDstImg * cColumns, cyDstImg * cRows);
        const HBITMAP hbmOldSrc = SelectBitmap(hdcSrc, hbmpInput);
        const HBITMAP hbmNew = CreateDIBSection(hdcSrc, &bmi, DIB_RGB_COLORS, 0, 0, 0);
        const HBITMAP hbmOldDst = SelectBitmap(hdcDst, hbmNew);

        // Iterate through the tiles in the grid, stretching each one
        // individually to avoid rounding errors from 'bleeding' between
        // tiles.
        for (int jj = 0, yDest = 0, yBmp = 0; jj < cRows; jj++, yDest += cyDstImg, yBmp += cySrcImg)
            for (int ii = 0, xDest = 0, xBmp = 0; ii < cColumns; ii++, xDest += cxDstImg, xBmp += cxSrcImg)
                if (spFactory)
                {
                    SPI<IWICBitmap> spBmp;
                    if (SUCCEEDED(spFactory->CreateBitmapFromSourceRect(spBmpFull, xBmp, yBmp, cxSrcImg, cySrcImg, &spBmp)))
                    {
                        HBITMAP hBmp = ScaleWicBitmapToBitmap(spFactory, spBmp, cxDstImg, cyDstImg);
                        if (hBmp)
                        {
                            const HBITMAP hbmOldResize = SelectBitmap(hdcResize, hBmp);
                            BitBlt(hdcDst, xDest, yDest, cxDstImg, cyDstImg, hdcResize, 0, 0, SRCCOPY);
                            SelectBitmap(hdcResize, hbmOldResize);
                            DeleteBitmap(hBmp);
                        }
                    }
                }
                else
                {
                    StretchBlt(hdcDst, xDest, yDest, cxDstImg, cyDstImg, hdcSrc, xBmp, yBmp, cxSrcImg, cySrcImg, SRCCOPY);
                }

        //spBmpFull.Release();
        //spFactory.Release();

        // Free the GDI objects we created.
        SelectBitmap(hdcSrc, hbmOldSrc);
        SelectBitmap(hdcDst, hbmOldDst);
        DeleteDC(hdcSrc);
        DeleteDC(hdcDst);
        DeleteDC(hdcResize);
        if (hbmpConverted)
            DeleteBitmap(hbmpConverted);

        // Delete the passed in bitmap and return the bitmap we created.
        DeleteBitmap(*phbm);
        *phbm = hbmNew;
    }

    return true;
}

static bool HIDPI_StretchIcon_Internal(HICON hiconIn, HICON* phiconOut, int cxIcon, int cyIcon)
{
    *phiconOut = 0;

    const HDC hdc = CreateCompatibleDC(0);

    const HBITMAP hbmMask = CreateCompatibleBitmap(hdc, cxIcon, cyIcon);
    const HBITMAP hbmOldMask = HBITMAP(SelectObject(hdc, hbmMask));
    const bool fMaskOk = DrawIconEx(hdc, 0, 0, hiconIn, cxIcon, cyIcon, 0, 0, DI_MASK);
    SelectObject(hdc, hbmOldMask);

    const HBITMAP hbmImage = CreateBitmap(cxIcon, cyIcon, 1, GetDeviceCaps(hdc, BITSPIXEL), 0);
    const HBITMAP hbmOldImage = HBITMAP(SelectObject(hdc, hbmImage));
    const bool fImageOk = DrawIconEx(hdc, 0, 0, hiconIn, cxIcon, cyIcon, 0, 0, DI_IMAGE);
    SelectObject(hdc, hbmOldImage);

    if (fMaskOk && fImageOk)
    {
        ICONINFO iconinfo;

        iconinfo.fIcon = true;
        iconinfo.hbmColor = hbmImage;
        iconinfo.hbmMask = hbmMask;

        *phiconOut = CreateIconIndirect(&iconinfo);
    }

    DeleteBitmap(hbmImage);
    DeleteBitmap(hbmMask);
    DeleteDC(hdc);

    return (fMaskOk && fImageOk && *phiconOut);
}

bool HIDPI_StretchIcon(const DpiScaler& dpi, HICON* phic, int cxIcon, int cyIcon)
{
    HICON hiconOut;

    if (HIDPI_StretchIcon_Internal(*phic, &hiconOut, cxIcon, cyIcon))
    {
        DestroyIcon( *phic );
        *phic = hiconOut;
        return true;
    }

    return false;
}

bool HIDPI_GetBitmapLogPixels(HINSTANCE hinst, UINT idb, int* pdxLogPixels, int* pdyLogPixels, int* pnBitsPerPixel)
{
    *pdxLogPixels = 0;
    *pdyLogPixels = 0;
    *pnBitsPerPixel = 0;

    // Note that MSDN says there is no cleanup needed after FindResource,
    // LoadResource, or LockResource.
    const HRSRC hResource = FindResource(hinst, MAKEINTRESOURCE(idb), RT_BITMAP);
    if (!hResource)
        return false;

    const HGLOBAL hResourceBitmap = LoadResource(hinst, hResource);
    if (!hResourceBitmap)
        return false;

    const BITMAPINFO* const pBitmapInfo = static_cast<const BITMAPINFO*>(LockResource(hResourceBitmap));
    if (!pBitmapInfo)
        return false;

    // There are at least three common values of PslsPerMeter that occur for
    // 96 DPI bitmaps:
    //
    //      0               = The bitmap doesn't set this value.
    //      2834            = 72 DPI.
    //      3780            = 96 DPI.
    //
    // For simplicity, treat all values under 3780 as 96 DPI bitmaps.
    const int cxPelsPerMeter = (pBitmapInfo->bmiHeader.biXPelsPerMeter < 3780) ? 3780 : pBitmapInfo->bmiHeader.biXPelsPerMeter;
    const int cyPelsPerMeter = (pBitmapInfo->bmiHeader.biYPelsPerMeter < 3780) ? 3780 : pBitmapInfo->bmiHeader.biYPelsPerMeter;

    // The formula for converting PelsPerMeter to LogPixels(DPI) is:
    //
    //      LogPixels = PelsPerMeter / 39.37
    //
    // Where:
    //
    //      PelsPerMeter = Pixels per meter.
    //      LogPixels = Pixels per inch.
    //
    // Round up, otherwise things can get cut off.
    *pdxLogPixels = int((cxPelsPerMeter * 100 + 1968) / 3937);
    *pdyLogPixels = int((cyPelsPerMeter * 100 + 1968) / 3937);

    *pnBitsPerPixel = int(pBitmapInfo->bmiHeader.biPlanes * pBitmapInfo->bmiHeader.biBitCount);

    return true;
}

HIMAGELIST HIDPI_ImageList_LoadImage(HINSTANCE hinst, const int cxTarget, const int cyTarget, UINT idb, const int cxNative, int cGrow, COLORREF crMask, const UINT uType, const UINT uFlags)
{
    // If the image type is not IMAGE_BITMAP, or if the caller doesn't care
    // about the dimensions of the image, then just use the ones in the file.
    if ((uType != IMAGE_BITMAP) || !cxNative || !cxTarget)
        return ImageList_LoadImage(hinst, MAKEINTRESOURCE(idb), cxNative, cGrow, crMask, uType, uFlags);

    // Get the bitmap dimensions.
    int cxBmpLogPixels;
    int cyBmpLogPixels;
    int nBitsPerPixel;
    if (!HIDPI_GetBitmapLogPixels(hinst, idb, &cxBmpLogPixels, &cyBmpLogPixels, &nBitsPerPixel))
        return 0;

    // Load the bitmap image.
    BITMAP bm;
    HIMAGELIST himl = 0;
    HBITMAP hbmImage = HBITMAP(LoadImage(hinst, MAKEINTRESOURCE(idb), uType, 0, 0, uFlags));

    if (hbmImage && sizeof(bm) == GetObject(hbmImage, sizeof(bm), &bm))
    {
        // Windows says DDPI currently will always be square pixels.
        assert(cxBmpLogPixels == cyBmpLogPixels);

        // Bitmap width should be an integral multiple of image width.  If not,
        // then either the bitmap is wrong or the passed in cxNative is wrong.
        assert(!(bm.bmWidth % cxNative));

        // If the input bitmap has an alpha channel already, then ignore the
        // crMask since the OS implementation of ImageList_LoadImage ignores
        // it in that case.
        if (nBitsPerPixel >= 32)
            crMask = CLR_NONE;

        const int cImages = bm.bmWidth / cxNative;
        const int cxImage = cxTarget;
        const int cy = cxTarget;
        const COLORREF crOldMask = crMask;

        // Stretching the bitmap in one action can cause individual images to
        // be stretched to wrong place/dimensions.  Even when the target DPI
        // is an integral multiple of 96 there's still the potential for
        // interpolation to "bleed" pixels between adjacent images (a real
        // problem, not just hypothetical).  Therefore we stretch individual
        // images separately to make sure each one is stretched properly.
        HIDPI_StretchBitmap(&hbmImage, cxImage, cy, cImages, 1, crMask);

        if (crOldMask != crMask)
            GetObject(hbmImage, sizeof(bm), &bm);

        UINT flags = 0;

        if (crMask != CLR_NONE)
        {
            // ILC_MASK is important for supporting CLR_DEFAULT.
            flags |= ILC_MASK;
        }

        if (bm.bmBits)
        {
            // ILC_COLORMASK bits are important when merging ImageLists.
            flags |= ( bm.bmBitsPixel & ILC_COLORMASK );
        }

        // The bitmap MUST be de-selected from the DC in order to create the
        // image list of the size asked for.
        himl = ImageList_Create(cxImage, cy, flags, cImages, cGrow);

        if (himl)
        {
            int cAdded;

            if (CLR_NONE == crMask)
                cAdded = ImageList_Add(himl, hbmImage, 0);
            else
                cAdded = ImageList_AddMasked(himl, hbmImage, crMask);

            if (cAdded < 0)
            {
                ImageList_Destroy(himl);
                himl = 0;
            }
        }
    }

    DeleteBitmap(hbmImage);
    return himl;
}

int __MessageBox(__in_opt HWND hWnd, __in_opt LPCTSTR lpText, __in_opt LPCTSTR lpCaption, __in UINT uType)
{
    ThreadDpiAwarenessContext dpiContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    return MessageBox(hWnd, lpText, lpCaption, uType);
}

