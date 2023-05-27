//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

// MICROSOFT LIMITED PUBLIC LICENSE version 1.1

// From sample source from Win81App for animated text on a path.
// https://github.com/uri247/Win81App/tree/master/Direct2D%20and%20DirectWrite%20animated%20text%20on%20a%20path%20sample

// BEGIN_CHANGE
#if 0
#include "pch.h"
#else
#include "../main.h"
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#endif
// END_CHANGE
#include "PathTextRenderer.h"

// An identity matrix for use by IDWritePixelSnapping::GetCurrentTransform.
const DWRITE_MATRIX identityTransform =
{
    1, 0,
    0, 1,
    0, 0
};

PathTextRenderer::PathTextRenderer(FLOAT pixelsPerDip) :
    m_pixelsPerDip(pixelsPerDip),
// BEGIN_CHANGE
#if 0
    m_ref(0)
#else
    m_ref(1)
#endif
// END_CHANGE
{
}

// BEGIN_CHANGE
bool PathTextRenderer::TestFit(void* clientDrawingContext, IDWriteTextLayout* pLayout)
{
    m_measure = true;
    m_fits = true;
    pLayout->Draw(clientDrawingContext, this, 0, 0);
    m_measure = false;
    return m_fits;
}
// END_CHANGE

//
// Draws a given glyph run along the geometry specified
// in the given clientDrawingEffect.
//
// This method calculates the horizontal displacement
// of each glyph cluster in the run, then calculates the
// tangent vector of the geometry at each of those distances.
// It then renders the glyph cluster using the offset and angle
// defined by that tangent, thereby placing each cluster on
// the path and also rotated to the path.
//
HRESULT PathTextRenderer::DrawGlyphRun(
    _In_opt_ void* clientDrawingContext,
    FLOAT baselineOriginX,
    FLOAT baselineOriginY,
    DWRITE_MEASURING_MODE measuringMode,
    _In_ DWRITE_GLYPH_RUN const* glyphRun,
    _In_ DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
    _In_opt_ IUnknown* clientDrawingEffect
)
{
    if (clientDrawingContext == nullptr)
    {
        return S_OK;
    }

    // Since we use our own custom renderer and explicitly set the effect
    // on the layout, we know exactly what the parameter is and can
    // safely cast it directly.
    PathTextDrawingContext* dc = static_cast<PathTextDrawingContext*>(clientDrawingContext);

    // Store any existing transformation in the render target.
    D2D1_MATRIX_3X2_F originalTransform;
    dc->d2DContext->GetTransform(&originalTransform);

    // Compute the length of the geometry.
    FLOAT maxLength;
// BEGIN_CHANGE
#if 0
    DX::ThrowIfFailed(
#else
    {
        HRESULT hr = (
#endif
// END_CHANGE
        dc->geometry->ComputeLength(
            originalTransform, &maxLength)
        );
// BEGIN_CHANGE
        if (FAILED(hr)) return hr;
        // Allow a little padding for readability.
        maxLength -= 2.0f * m_pixelsPerDip / 96.0f;
    }
// END_CHANGE

    // Set up a partial glyph run that we can modify.
    DWRITE_GLYPH_RUN partialGlyphRun = *glyphRun;

    // Determine whether the text is LTR or RTL.
    BOOL leftToRight = (glyphRun->bidiLevel % 2 == 0);

    // Set the initial length along the path.
    FLOAT length = baselineOriginX;

    // Set the index of the first glyph in the current glyph cluster.
    UINT firstGlyphIdx = 0;

    while (firstGlyphIdx < glyphRun->glyphCount)
    {
        // Compute the number of glyphs in this cluster and the total cluster width.
        UINT numGlyphsInCluster = 0;
        UINT i = firstGlyphIdx;
        FLOAT clusterWidth = 0;
        while (glyphRunDescription->clusterMap[i] == glyphRunDescription->clusterMap[firstGlyphIdx] &&
            i < glyphRun->glyphCount)
        {
            clusterWidth += glyphRun->glyphAdvances[i];
            i++;
            numGlyphsInCluster++;
        }

        // Compute the cluster's midpoint along the path.
        FLOAT midpoint = leftToRight ?
            (length + (clusterWidth / 2)) :
            (length - (clusterWidth / 2));

        // Only render this cluster if it's within the path.
// BEGIN_CHANGE
#if 0
        if (midpoint < maxLength)
#else
        FLOAT endpoint = leftToRight ?
            (length + clusterWidth) :
            (length - clusterWidth);
        if (endpoint > maxLength)
        {
            m_fits = false;
            break;
        }
#endif
// END_CHANGE
        {
            // Compute the offset and tangent at the cluster's midpoint.
            D2D1_POINT_2F offset;
            D2D1_POINT_2F tangent;
// BEGIN_CHANGE
#if 0
            DX::ThrowIfFailed(
#else
            {
                HRESULT hr = (
#endif
// END_CHANGE
                dc->geometry->ComputePointAtLength(
                    midpoint,
                    D2D1::IdentityMatrix(),
                    &offset,
                    &tangent
                    )
                );
// BEGIN_CHANGE
                if (FAILED(hr)) return hr;
            }
// END_CHANGE

            // Create a rotation matrix to align the cluster to the path.
            // Alternatively, we could use the D2D1::Matrix3x2F::Rotation()
            // helper, but this is more efficient since we already have cos(t)
            // and sin(t).
            D2D1_MATRIX_3X2_F rotation = D2D1::Matrix3x2F(
                tangent.x,
                tangent.y,
                -tangent.y,
                tangent.x,
                (offset.x * (1.0f - tangent.x) + offset.y * tangent.y),
                (offset.y * (1.0f - tangent.x) - offset.x * tangent.y)
                );

            // Create a translation matrix to center the cluster on the tangent point.
            D2D1_MATRIX_3X2_F translation = leftToRight ?
                D2D1::Matrix3x2F::Translation(-clusterWidth/2, 0) : // LTR --> nudge it left
                D2D1::Matrix3x2F::Translation(clusterWidth/2, 0); // RTL --> nudge it right

            // Apply the transformations (in the proper order).
            dc->d2DContext->SetTransform(translation * rotation * originalTransform);

            // Draw the transformed glyph cluster.
            partialGlyphRun.glyphCount = numGlyphsInCluster;
// BEGIN_CHANGE
            if (!m_measure)
// END_CHANGE
            dc->d2DContext->DrawGlyphRun(
                D2D1::Point2F(offset.x, offset.y),
                &partialGlyphRun,
                dc->brush
                );
        }

        // Advance to the next cluster.
        length = leftToRight ? (length + clusterWidth) : (length - clusterWidth);
        partialGlyphRun.glyphIndices += numGlyphsInCluster;
        partialGlyphRun.glyphAdvances += numGlyphsInCluster;

        if (partialGlyphRun.glyphOffsets != nullptr)
        {
            partialGlyphRun.glyphOffsets += numGlyphsInCluster;
        }

        firstGlyphIdx += numGlyphsInCluster;
    }

    // Restore the original transformation.
    dc->d2DContext->SetTransform(originalTransform);

    return S_OK;
}

HRESULT PathTextRenderer::DrawUnderline(
    _In_opt_ void* clientDrawingContext,
    FLOAT baselineOriginX,
    FLOAT baselineOriginY,
    _In_ DWRITE_UNDERLINE const* underline,
    _In_opt_ IUnknown* clientDrawingEffect
    )
{
    // We don't use underline in this application.
    return E_NOTIMPL;
}

HRESULT PathTextRenderer::DrawStrikethrough(
    _In_opt_ void* clientDrawingContext,
    FLOAT baselineOriginX,
    FLOAT baselineOriginY,
    _In_ DWRITE_STRIKETHROUGH const* strikethrough,
    _In_opt_ IUnknown* clientDrawingEffect
    )
{
    // We don't use strikethrough in this application.
    return E_NOTIMPL;
}

HRESULT PathTextRenderer::DrawInlineObject(
    _In_opt_ void* clientDrawingContext,
    FLOAT originX,
    FLOAT originY,
    IDWriteInlineObject* inlineObject,
    BOOL isSideways,
    BOOL isRightToLeft,
    _In_opt_ IUnknown* clientDrawingEffect
    )
{
    // We don't use inline objects in this application.
    return E_NOTIMPL;
}

//
// IDWritePixelSnapping methods
//
HRESULT PathTextRenderer::IsPixelSnappingDisabled(
    _In_opt_ void* clientDrawingContext,
    _Out_ BOOL* isDisabled
    )
{
    *isDisabled = FALSE;
    return S_OK;
}

HRESULT PathTextRenderer::GetCurrentTransform(
    _In_opt_ void* clientDrawingContext,
    _Out_ DWRITE_MATRIX* transform
    )
{
    *transform = identityTransform;
    return S_OK;
}

HRESULT PathTextRenderer::GetPixelsPerDip(
    _In_opt_ void* clientDrawingContext,
    _Out_ FLOAT* pixelsPerDip
    )
{
    *pixelsPerDip = m_pixelsPerDip;
    return S_OK;
}

//
// IUnknown methods
//
// These use a basic, non-thread-safe implementation of the
// standard reference-counting logic.
//
HRESULT PathTextRenderer::QueryInterface(
    REFIID riid,
    _Outptr_ void** object
    )
{
    *object = nullptr;
    return E_NOTIMPL;
}

ULONG PathTextRenderer::AddRef()
{
    m_ref++;

    return m_ref;
}

ULONG PathTextRenderer::Release()
{
    m_ref--;

    if (m_ref == 0)
    {
        delete this;
        return 0;
    }

    return m_ref;
}
