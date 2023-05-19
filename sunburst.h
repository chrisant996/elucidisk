// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>

HRESULT InitializeD2D();

class DirectHwndRenderTarget
{
public:
                            DirectHwndRenderTarget();
                            ~DirectHwndRenderTarget();

    HRESULT                 CreateDeviceResources(HWND hwnd);
    HRESULT                 ResizeDeviceResources();
    void                    ReleaseDeviceResources();

    ID2D1Factory*           Factory() const { return m_pFactory; }
    ID2D1RenderTarget*      Target() const { return m_pTarget; }

    ID2D1SolidColorBrush*   LineBrush() const { return m_pLineBrush; }

private:
    HWND                    m_hwnd = 0;
    ID2D1Factory*           m_pFactory = nullptr;
    ID2D1HwndRenderTarget*  m_pTarget = nullptr;
    ID2D1SolidColorBrush*   m_pLineBrush = nullptr;
};

