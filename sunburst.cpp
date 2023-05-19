// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "sunburst.h"
#include <assert.h>

static ID2D1Factory* s_pD2DFactory = nullptr;

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
    ReleaseI(m_pTarget);
    ReleaseI(m_pFactory);
    m_hwnd = 0;
}

