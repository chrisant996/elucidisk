// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include "data.h"
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>

class DirNode;

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
    ID2D1SolidColorBrush*   FileLineBrush() const { return m_pFileLineBrush; }
    ID2D1SolidColorBrush*   FillBrush() const { return m_pFillBrush; }

private:
    HWND                    m_hwnd = 0;
    ID2D1Factory*           m_pFactory = nullptr;
    ID2D1HwndRenderTarget*  m_pTarget = nullptr;
    ID2D1SolidColorBrush*   m_pLineBrush = nullptr;
    ID2D1SolidColorBrush*   m_pFileLineBrush = nullptr;
    ID2D1SolidColorBrush*   m_pFillBrush = nullptr;
};

class Sunburst
{
    struct Arc
    {
        float               m_start;
        float               m_end;
        std::shared_ptr<Node> m_node;
    };

    enum class UnitScale { KB, MB, GB };

public:
                            Sunburst();
                            ~Sunburst();

    void                    Init(const Sunburst& other);
    void                    BuildRings(const std::vector<std::shared_ptr<DirNode>>& roots);
    void                    RenderRings(DirectHwndRenderTarget& target, const D2D1_RECT_F& rect, const std::shared_ptr<Node>& highlight);
    void                    FormatSize(ULONGLONG size, std::wstring& text, std::wstring& units, int places);
    std::shared_ptr<Node>   HitTest(POINT pt);
    void                    OnDpiChanged(const DpiScaler& dpi);

protected:
    static D2D1_COLOR_F     MakeColor(const Arc& arc, size_t depth, bool highlight);
    static D2D1_COLOR_F     MakeRootColor(bool highlight, bool free);
#ifdef USE_MIN_ARC_LENGTH
    static void             MakeArc(std::vector<Arc>& arcs, FLOAT outer_radius, FLOAT min_arc, const std::shared_ptr<Node>& node, ULONGLONG size, double& sweep, double total, float start, float span, double convert=1.0f);
    std::vector<Arc>        NextRing(const std::vector<Arc>& parent_ring, FLOAT outer_radius, FLOAT min_arc);
#else
    static void             MakeArc(std::vector<Arc>& arcs, const std::shared_ptr<Node>& node, ULONGLONG size, double& sweep, double total, float start, float span, double convert=1.0f);
    std::vector<Arc>        NextRing(const std::vector<Arc>& parent_ring);
#endif
    bool                    MakeArcGeometry(DirectHwndRenderTarget& target, FLOAT start, FLOAT end, FLOAT inner_radius, FLOAT outer_radius, ID2D1Geometry** ppGeometry);

private:
    DpiScaler               m_dpi;
    D2D1_RECT_F             m_bounds;
    D2D1_POINT_2F           m_center;
    UnitScale               m_units = UnitScale::MB;

    std::vector<std::shared_ptr<DirNode>> m_roots;
    std::vector<std::vector<Arc>> m_rings;
    std::vector<FLOAT>      m_start_angles;
    std::vector<FLOAT>      m_free_angles;
};

