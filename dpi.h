// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#ifndef WM_DPICHANGED
#define WM_DPICHANGED           0x02E0
#endif

#define WMU_DPICHANGED          (WM_USER + 9997)    // Specialized internal use.
#define WMU_REFRESHDPI          (WM_USER + 9998)    // Specialized internal use.

#ifndef DPI_AWARENESS_CONTEXT_UNAWARE
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_UNAWARE               (DPI_AWARENESS_CONTEXT(-1))
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE          (DPI_AWARENESS_CONTEXT(-2))
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE     (DPI_AWARENESS_CONTEXT(-3))
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2  (DPI_AWARENESS_CONTEXT(-4))
#endif // !DPI_AWARENESS_CONTEXT_UNAWARE

int HIDPIMulDiv(int x, int y, int z);

WORD __GetHdcDpi(HDC hdc);
WORD __GetDpiForSystem();
WORD __GetDpiForWindow(HWND hwnd);
bool __IsHwndPerMonitorAware(HWND hwnd);

int __GetSystemMetricsForDpi(int nIndex, UINT dpi);
bool __SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi);

bool __EnableNonClientDpiScaling(HWND hwnd);
bool __EnablePerMonitorMenuScaling();

class ThreadDpiAwarenessContext
{
public:
    ThreadDpiAwarenessContext(bool fUsePerMonitorAwareness);
    ThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context);
    ~ThreadDpiAwarenessContext() { Restore(); }

    void                Restore();

private:
    DPI_AWARENESS_CONTEXT m_context;
    bool                m_fRestore;
};

class DpiScaler
{
public:
                DpiScaler();
    explicit    DpiScaler(WORD dpi);
    explicit    DpiScaler(WPARAM wParam);
    explicit    DpiScaler(const DpiScaler& dpi);

    bool        IsDpiEqual(UINT dpi) const;
    bool        IsDpiEqual(const DpiScaler& dpi) const;
    bool        operator==(UINT dpi) const { return IsDpiEqual( dpi ); }
    bool        operator==(const DpiScaler& dpi ) const { return IsDpiEqual( dpi ); }

    DpiScaler&  operator=(WORD dpi);
    DpiScaler&  operator=(const DpiScaler& dpi);
    void        OnDpiChanged(const DpiScaler& dpi);

    int         Scale(int n) const;

    int         ScaleTo(int n, DWORD dpi) const;
    int         ScaleTo(int n, const DpiScaler& dpi) const;
    int         ScaleFrom(int n, DWORD dpi) const;
    int         ScaleFrom(int n, const DpiScaler& dpi) const;

    int         PointSizeToHeight(int nPointSize) const;
    int         PointSizeToHeight(float pointSize) const;

    int         GetSystemMetrics(int nIndex) const;
    bool        SystemParametersInfo(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) const;

    WPARAM      MakeWParam() const;

private:
    WORD        m_logPixels;
};

/*
 * HIDPI_StretchBitmap
 *
 *      Stretches a bitmap containing a grid of images.  There are cColumns
 *      images per row and cRows rows per bitmap.  Each image is scaled
 *      individually, so that there are no artifacts with non-integral scaling
 *      factors.  If the bitmap contains only one image, set cColumns and
 *      cRows to 1.
 *
 * Args:
 *
 *      phbm            - A pointer to the bitmap to be scaled.
 *      cxDstImg        - The width of each image after scaling.
 *      cyDstImg        - The height of each image after scaling.
 *      cColumns        - The number of images per row.  This value should
 *                        evenly divide the width of the bitmap.
 *      cRows           - The number of rows in the bitmap.  This value should
 *                        evenly divide the height of the bitmap.
 *      crTransparent   - [in/out] The transparent color if no alpha channel.
 *                        CLR_NONE on input or output indicates alpha channel
 *                        is used for transparency (or that it's opaque).
 *
 * Rets:
 *
 *      Returns true on success, false on failure.
 *
 *      If any scaling has occured, the bitmap pointed to by phbm is deleted
 *      and is replaced by a new bitmap handle.
 */
bool HIDPI_StretchBitmap(HBITMAP* phbm, int cxDstImg, int cyDstImg, int cColumns, int cRows, COLORREF& crTransparent);

/*
 * HIDPI_GetBitmapLogPixels
 *
 *      Retrieves the DPI fields of the specified bitmap.
 *
 * Args:
 *
 *      hinst           - The HINSTANCE of the bitmap resource.
 *      idb             - The ID of the bitmap resource.
 *      pdxLogPixels    - The returned value for the horizontal DPI field of
 *                        the bitmap.  This value is never less than 96.
 *      pdyLogPixels    - The returned value for the vertical DPI field of
 *                        the bitmap.  This value is never less than 96.
 *
 * Rets:
 *
 *      Returns true on success, false on failure.
 */
bool HIDPI_GetBitmapLogPixels(HINSTANCE hinst, UINT idb, int* pdxLogPixels, int* pdyLogPixels);

/*
 * HIDPI_StretchIcon
 *
 *      Stretches an icon to the specified size.
 *
 * Args:
 *
 *      dpi             - The encapsulated DPI.
 *      phic            - The icon to stretch.
 *      cxIcon          - The desired width of the icon.
 *      cyIcon          - The desired height of the icon.
 *
 * Rets:
 *
 *      Returns true on success, false on failure.
 *
 *      If any stretching occurred, the icon pointed to by phic is deleted and
 *      is replaced by a new icon handle.
 */
bool HIDPI_StretchIcon(const DpiScaler& dpi, HICON* phic, int cxIcon, int cyIcon);

/*
 * HIDPI_ImageList_LoadImage
 *
 *      This function operates identically to ImageList_LoadImage, except it
 *      also performs scaling if needed.
 *
 * Args/Rets:   See the MSDN documentation for ImageList_LoadImage.
 */
HIMAGELIST HIDPI_ImageList_LoadImage(HINSTANCE hinst, int cxTarget, int cyTarget, UINT idb, int cxNative, int cGrow, COLORREF crMask, UINT uType, UINT uFlags);

