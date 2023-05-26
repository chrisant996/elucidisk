//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

// MICROSOFT LIMITED PUBLIC LICENSE version 1.1
// From sample source from Win81App for animated text on a path.
// https://github.com/uri247/Win81App/tree/master/Direct2D%20and%20DirectWrite%20animated%20text%20on%20a%20path%20sample

#pragma once

struct PathTextDrawingContext
{
    SPI<ID2D1DeviceContext> d2DContext;
    SPI<ID2D1Geometry> geometry;
    SPI<ID2D1Brush> brush;
};

class PathTextRenderer : public IDWriteTextRenderer
{
public:
    PathTextRenderer(
        FLOAT pixelsPerDip
        );

// BEGIN_CHANGE
    STDMETHOD_(bool, TestFit)(
        void* clientDrawingContext,
        IDWriteTextLayout* pLayout
        );
// END_CHANGE

    STDMETHOD(DrawGlyphRun)(
        _In_opt_ void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE measuringMode,
        _In_ DWRITE_GLYPH_RUN const* glyphRun,
        _In_ DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
        _In_opt_ IUnknown* clientDrawingEffect
        ) override;

    STDMETHOD(DrawUnderline)(
        _In_opt_ void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        _In_ DWRITE_UNDERLINE const* underline,
        _In_opt_ IUnknown* clientDrawingEffect
        ) override;

    STDMETHOD(DrawStrikethrough)(
        _In_opt_ void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        _In_ DWRITE_STRIKETHROUGH const* strikethrough,
        _In_opt_ IUnknown* clientDrawingEffect
        ) override;

    STDMETHOD(DrawInlineObject)(
        _In_opt_ void* clientDrawingContext,
        FLOAT originX,
        FLOAT originY,
        IDWriteInlineObject* inlineObject,
        BOOL isSideways,
        BOOL isRightToLeft,
        _In_opt_ IUnknown* clientDrawingEffect
        ) override;

    STDMETHOD(IsPixelSnappingDisabled)(
        _In_opt_ void* clientDrawingContext,
        _Out_ BOOL* isDisabled
        ) override;

    STDMETHOD(GetCurrentTransform)(
        _In_opt_ void* clientDrawingContext,
        _Out_ DWRITE_MATRIX* transform
        ) override;

    STDMETHOD(GetPixelsPerDip)(
        _In_opt_ void* clientDrawingContext,
        _Out_ FLOAT* pixelsPerDip
        ) override;

    STDMETHOD(QueryInterface)(
        REFIID riid,
        _Outptr_ void** object
        ) override;

    STDMETHOD_(ULONG, AddRef)() override;

    STDMETHOD_(ULONG, Release)() override;

private:
    FLOAT m_pixelsPerDip;   // Number of pixels per DIP.
    UINT m_ref;             // Reference count for AddRef and Release.
// BEGIN_CHANGE
    bool m_measure = false; // Measure instead of drawing.
    bool m_fits = false;    // Whether the text fits.
// END_CHANGE
};
