// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/NinePieceImagePainter.h"

#include "core/frame/Deprecation.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/NinePieceImageGrid.h"
#include "core/style/ComputedStyle.h"
#include "core/style/NinePieceImage.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

NinePieceImagePainter::NinePieceImagePainter(const LayoutBoxModelObject& layoutObject)
    : m_layoutObject(layoutObject)
{
}

bool NinePieceImagePainter::paint(GraphicsContext& graphicsContext, const LayoutRect& rect, const ComputedStyle& style,
    const NinePieceImage& ninePieceImage, SkXfermode::Mode op) const
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender())
        return false;

    // Find out if the hasImage() check in ComputedStyle::border*Width had any affect, i.e. if a border is non-zero while border-style is
    // none or hidden.
    if ((style.borderLeftWidth() && (style.borderLeft().style() == BNONE || style.borderLeft().style() == BHIDDEN))
        || (style.borderRightWidth() && (style.borderRight().style() == BNONE || style.borderRight().style() == BHIDDEN))
        || (style.borderTopWidth() && (style.borderTop().style() == BNONE || style.borderTop().style() == BHIDDEN))
        || (style.borderBottomWidth() && (style.borderBottom().style() == BNONE || style.borderBottom().style() == BHIDDEN)))
        Deprecation::countDeprecation(m_layoutObject.document(), UseCounter::BorderImageWithBorderStyleNone);

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    LayoutRect borderImageRect = rectWithOutsets;

    IntSize imageSize = roundedIntSize(m_layoutObject.calculateImageIntrinsicDimensions(styleImage, borderImageRect.size(),
        LayoutBoxModelObject::DoNotScaleByEffectiveZoom));

    IntRectOutsets borderWidths(style.borderTopWidth(), style.borderRightWidth(),
        style.borderBottomWidth(), style.borderLeftWidth());
    NinePieceImageGrid grid(ninePieceImage, imageSize, pixelSnappedIntRect(borderImageRect), borderWidths);

    RefPtr<Image> image = styleImage->image(&m_layoutObject, imageSize, style.effectiveZoom());

    InterpolationQuality interpolationQuality = BoxPainter::chooseInterpolationQuality(m_layoutObject, image.get(), 0, rectWithOutsets.size());
    InterpolationQuality previousInterpolationQuality = graphicsContext.imageInterpolationQuality();
    graphicsContext.setImageInterpolationQuality(interpolationQuality);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data",
        InspectorPaintImageEvent::data(m_layoutObject, *styleImage));

    for (NinePiece piece = MinPiece; piece < MaxPiece; ++piece) {
        NinePieceImageGrid::NinePieceDrawInfo drawInfo = grid.getNinePieceDrawInfo(piece);

        if (drawInfo.isDrawable) {
            if (drawInfo.isCornerPiece) {
                graphicsContext.drawImage(image.get(), drawInfo.destination, drawInfo.source, op);
            } else {
                graphicsContext.drawTiledImage(image.get(), drawInfo.destination,
                    drawInfo.source, drawInfo.tileScale, drawInfo.tileRule.horizontal,
                    drawInfo.tileRule.vertical, op);
            }
        }
    }

    graphicsContext.setImageInterpolationQuality(previousInterpolationQuality);
    return true;
}

} // namespace blink
