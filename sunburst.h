// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include "data.h"
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include "TextOnPath/PathTextRenderer.h"
#include <string>

#define USE_MIN_ARC_LENGTH              // Likely permanent.

//#define USE_CHART_OUTLINE               // Experimenting with this off.

class DirNode;
struct SunburstMetrics;

HRESULT InitializeD2D();
HRESULT InitializeDWrite();

bool GetD2DFactory(ID2D1Factory** ppFactory);
bool GetDWriteFactory(IDWriteFactory2** ppFactory);

enum WriteTextOptions
{
    WTO_NONE                = 0x0000,
    WTO_REMEMBER_METRICS    = 0x0001,
    WTO_CLIP                = 0x0002,
    WTO_HCENTER             = 0x0004,
    WTO_VCENTER             = 0x0008,
    WTO_RIGHT_ALIGN         = 0x0010,
    WTO_BOTTOM_ALIGN        = 0x0020,

    WTO_UNDERLINE           = 0x8000,
};
DEFINE_ENUM_FLAG_OPERATORS(WriteTextOptions);

struct Shortened
{
    std::wstring m_text;
    FLOAT m_extent = 0.0f;
    size_t m_orig_offset = 0;
};

class DirectHwndRenderTarget
{
    struct Resources
    {
        HRESULT                     Init(HWND hwnd, const D2D1_SIZE_U& size, const DpiScaler& dpi);

        SPI<ID2D1Factory>           m_spFactory;
        SPI<IDWriteFactory2>        m_spDWriteFactory;

        SPI<ID2D1HwndRenderTarget>  m_spTarget;
        SPQI<ID2D1DeviceContext>    m_spContext;

        SPI<ID2D1SolidColorBrush>   m_spLineBrush;
        SPI<ID2D1SolidColorBrush>   m_spFileLineBrush;
        SPI<ID2D1SolidColorBrush>   m_spFillBrush;
        SPI<ID2D1SolidColorBrush>   m_spTextBrush;

        SPI<ID2D1StrokeStyle>       m_spRoundedStroke;
        SPI<ID2D1StrokeStyle>       m_spBevelStroke;

        SPI<IDWriteTextFormat>      m_spTextFormat;
        SPI<IDWriteTextFormat>      m_spCenterTextFormat;
        FLOAT                       m_fontSize = 0.0f;
        FLOAT                       m_centerFontSize = 0.0f;

        SPI<IDWriteTextFormat>      m_spArcTextFormat;
        SPI<IDWriteRenderingParams> m_spRenderingParams;
        SPI<PathTextRenderer>       m_spPathTextRenderer;
        FLOAT                       m_arcFontSize = 0.0f;

        D2D1_POINT_2F               m_lastTextPosition = D2D1::Point2F();
        D2D1_SIZE_F                 m_lastTextSize = D2D1::SizeF();
    };

public:
                            DirectHwndRenderTarget();
                            ~DirectHwndRenderTarget();

    HRESULT                 CreateDeviceResources(HWND hwnd, const DpiScaler& dpi);
    HRESULT                 ResizeDeviceResources();
    void                    ReleaseDeviceResources();

    ID2D1Factory*           Factory() const { return m_resources->m_spFactory; }
    IDWriteFactory*         DWriteFactory() const { return m_resources->m_spDWriteFactory; }
    ID2D1RenderTarget*      Target() const { return m_resources->m_spTarget; }

    ID2D1SolidColorBrush*   LineBrush() const { return m_resources->m_spLineBrush; }
    ID2D1SolidColorBrush*   FileLineBrush() const { return m_resources->m_spFileLineBrush; }
    ID2D1SolidColorBrush*   FillBrush() const { return m_resources->m_spFillBrush; }
    ID2D1SolidColorBrush*   TextBrush() const { return m_resources->m_spTextBrush; }

    ID2D1StrokeStyle*       RoundedStrokeStyle() const { return m_resources->m_spRoundedStroke; }
    ID2D1StrokeStyle*       BevelStrokeStyle() const { return m_resources->m_spBevelStroke; }

    IDWriteTextFormat*      TextFormat() const { return m_resources->m_spTextFormat; }
    FLOAT                   FontSize() const { return m_resources->m_fontSize; }
    IDWriteTextFormat*      CenterTextFormat() const { return m_resources->m_spCenterTextFormat; }
    FLOAT                   CenterFontSize() const { return m_resources->m_centerFontSize; }

    ID2D1DeviceContext*     Context() const { return m_resources->m_spContext; }
    IDWriteTextFormat*      ArcTextFormat() const { return m_resources->m_spArcTextFormat; }
    PathTextRenderer*       ArcTextRenderer() const { return m_resources->m_spPathTextRenderer; }
    FLOAT                   ArcFontSize() const { return m_resources->m_arcFontSize; }

    bool                    ShortenText(IDWriteTextFormat* format, const D2D1_RECT_F& rect, const WCHAR* text, size_t len, FLOAT target, Shortened& out, int ellipsis=1);
    bool                    MeasureText(IDWriteTextFormat* format, const D2D1_RECT_F& rect, const WCHAR* text, size_t len, D2D1_SIZE_F& size, IDWriteTextLayout** ppLayout=nullptr);
    bool                    MeasureText(IDWriteTextFormat* format, const D2D1_RECT_F& rect, const std::wstring& text, D2D1_SIZE_F& size, IDWriteTextLayout** ppLayout=nullptr);
    bool                    WriteText(IDWriteTextFormat* format, FLOAT x, FLOAT y, const D2D1_RECT_F& rect, const WCHAR* text, size_t len, WriteTextOptions options=WTO_NONE, IDWriteTextLayout* pLayout=nullptr);
    bool                    WriteText(IDWriteTextFormat* format, FLOAT x, FLOAT y, const D2D1_RECT_F& rect, const std::wstring& text, WriteTextOptions options=WTO_NONE, IDWriteTextLayout* pLayout=nullptr);
    D2D1_POINT_2F           LastTextPosition() const { return m_resources->m_lastTextPosition; }
    D2D1_SIZE_F             LastTextSize() const { return m_resources->m_lastTextSize; }

private:
    HWND                        m_hwnd = 0;
    std::unique_ptr<Resources>  m_resources;
};

class Sunburst
{
    struct Arc
    {
        float               m_start;
        float               m_end;
        std::shared_ptr<Node> m_node;
    };

    struct HighlightInfo
    {
        Arc                 m_arc;
        SPI<ID2D1Geometry>  m_geometry;
    };

public:
                            Sunburst();
                            ~Sunburst();

    bool                    OnDpiChanged(const DpiScaler& dpi);
    bool                    SetBounds(const D2D1_RECT_F& rect);
    void                    BuildRings(const std::vector<std::shared_ptr<DirNode>>& roots);
    void                    RenderRings(DirectHwndRenderTarget& target, const std::shared_ptr<Node>& highlight);
    void                    FormatSize(ULONGLONG size, std::wstring& text, std::wstring& units, int places=-1);
    std::shared_ptr<Node>   HitTest(POINT pt, bool* is_free=nullptr);

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
    void                    AddArcToSink(ID2D1GeometrySink* pSink, bool counter_clockwise, FLOAT start, FLOAT end, const D2D1_POINT_2F& end_point, FLOAT radius);
    bool                    MakeArcGeometry(DirectHwndRenderTarget& target, FLOAT start, FLOAT end, FLOAT inner_radius, FLOAT outer_radius, ID2D1Geometry** ppGeometry);
    void                    DrawArcText(DirectHwndRenderTarget& target, const Arc& arc, FLOAT radius);
    void                    RenderRingsInternal(DirectHwndRenderTarget& target, const SunburstMetrics& mx, const std::shared_ptr<Node>& highlight, bool files, HighlightInfo& highlightInfo);

private:
    DpiScaler               m_dpi;
    FLOAT                   m_min_arc_text_len = 0;
    D2D1_RECT_F             m_bounds = D2D1::RectF();
    D2D1_POINT_2F           m_center = D2D1::Point2F();
    UnitScale               m_units = UnitScale::MB;

    std::vector<std::shared_ptr<DirNode>> m_roots;
    std::vector<std::vector<Arc>> m_rings;
    std::vector<FLOAT>      m_start_angles;
    std::vector<FLOAT>      m_free_angles;
};

