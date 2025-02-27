/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSCrossfadeValue.h"

#include "core/css/CSSImageValue.h"
#include "core/layout/LayoutObject.h"
#include "core/style/StyleFetchedImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/graphics/CrossfadeGeneratedImage.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static bool subimageIsPending(CSSValue* value)
{
    if (value->isImageValue())
        return toCSSImageValue(value)->isCachePending();

    if (value->isImageGeneratorValue())
        return toCSSImageGeneratorValue(value)->isPending();

    ASSERT_NOT_REACHED();

    return false;
}

static bool subimageKnownToBeOpaque(CSSValue* value, const LayoutObject* layoutObject)
{
    if (value->isImageValue())
        return toCSSImageValue(value)->knownToBeOpaque(layoutObject);

    if (value->isImageGeneratorValue())
        return toCSSImageGeneratorValue(value)->knownToBeOpaque(layoutObject);

    ASSERT_NOT_REACHED();

    return false;
}

static ImageResource* cachedImageForCSSValue(CSSValue* value, Document* document)
{
    if (!value)
        return nullptr;

    if (value->isImageValue()) {
        StyleFetchedImage* styleImageResource = toCSSImageValue(value)->cacheImage(document);
        if (!styleImageResource)
            return nullptr;

        return styleImageResource->cachedImage();
    }

    if (value->isImageGeneratorValue()) {
        toCSSImageGeneratorValue(value)->loadSubimages(document);
        // FIXME: Handle CSSImageGeneratorValue (and thus cross-fades with gradients and canvas).
        return nullptr;
    }

    ASSERT_NOT_REACHED();

    return nullptr;
}

static Image* renderableImageForCSSValue(CSSValue* value, const LayoutObject* layoutObject)
{
    ImageResource* cachedImage = cachedImageForCSSValue(value, &layoutObject->document());

    if (!cachedImage || !cachedImage->canRender())
        return nullptr;

    return cachedImage->image();
}

static KURL urlForCSSValue(const CSSValue* value)
{
    if (!value->isImageValue())
        return KURL();

    return KURL(ParsedURLString, toCSSImageValue(*value).url());
}

CSSCrossfadeValue::CSSCrossfadeValue(PassRefPtrWillBeRawPtr<CSSValue> fromValue, PassRefPtrWillBeRawPtr<CSSValue> toValue, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> percentageValue)
    : CSSImageGeneratorValue(CrossfadeClass)
    , m_fromValue(fromValue)
    , m_toValue(toValue)
    , m_percentageValue(percentageValue)
    , m_cachedFromImage(nullptr)
    , m_cachedToImage(nullptr)
    , m_crossfadeSubimageObserver(this)
{
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(this);
#endif
}

CSSCrossfadeValue::~CSSCrossfadeValue()
{
#if !ENABLE(OILPAN)
    dispose();
#endif
}

void CSSCrossfadeValue::dispose()
{
    if (m_cachedFromImage) {
        m_cachedFromImage->removeClient(&m_crossfadeSubimageObserver);
        m_cachedFromImage = nullptr;
    }
    if (m_cachedToImage) {
        m_cachedToImage->removeClient(&m_crossfadeSubimageObserver);
        m_cachedToImage = nullptr;
    }
}

String CSSCrossfadeValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("-webkit-cross-fade(");
    result.append(m_fromValue->cssText());
    result.appendLiteral(", ");
    result.append(m_toValue->cssText());
    result.appendLiteral(", ");
    result.append(m_percentageValue->cssText());
    result.append(')');
    return result.toString();
}

PassRefPtrWillBeRawPtr<CSSCrossfadeValue> CSSCrossfadeValue::valueWithURLsMadeAbsolute()
{
    RefPtrWillBeRawPtr<CSSValue> fromValue = m_fromValue;
    if (m_fromValue->isImageValue())
        fromValue = toCSSImageValue(*m_fromValue).valueWithURLMadeAbsolute();
    RefPtrWillBeRawPtr<CSSValue> toValue = m_toValue;
    if (m_toValue->isImageValue())
        toValue = toCSSImageValue(*m_toValue).valueWithURLMadeAbsolute();
    return CSSCrossfadeValue::create(fromValue.release(), toValue.release(), m_percentageValue);
}

IntSize CSSCrossfadeValue::fixedSize(const LayoutObject* layoutObject)
{
    Image* fromImage = renderableImageForCSSValue(m_fromValue.get(), layoutObject);
    Image* toImage = renderableImageForCSSValue(m_toValue.get(), layoutObject);

    if (!fromImage || !toImage)
        return IntSize();

    IntSize fromImageSize = fromImage->size();
    IntSize toImageSize = toImage->size();

    // Rounding issues can cause transitions between images of equal size to return
    // a different fixed size; avoid performing the interpolation if the images are the same size.
    if (fromImageSize == toImageSize)
        return fromImageSize;

    float percentage = m_percentageValue->getFloatValue();
    float inversePercentage = 1 - percentage;

    return IntSize(fromImageSize.width() * inversePercentage + toImageSize.width() * percentage,
        fromImageSize.height() * inversePercentage + toImageSize.height() * percentage);
}

bool CSSCrossfadeValue::isPending() const
{
    return subimageIsPending(m_fromValue.get()) || subimageIsPending(m_toValue.get());
}

bool CSSCrossfadeValue::knownToBeOpaque(const LayoutObject* layoutObject) const
{
    return subimageKnownToBeOpaque(m_fromValue.get(), layoutObject) && subimageKnownToBeOpaque(m_toValue.get(), layoutObject);
}

void CSSCrossfadeValue::loadSubimages(Document* document)
{
    RefPtrWillBeRawPtr<ImageResource> oldCachedFromImage = m_cachedFromImage;
    RefPtrWillBeRawPtr<ImageResource> oldCachedToImage = m_cachedToImage;

    m_cachedFromImage = cachedImageForCSSValue(m_fromValue.get(), document);
    m_cachedToImage = cachedImageForCSSValue(m_toValue.get(), document);

    if (m_cachedFromImage != oldCachedFromImage) {
        if (oldCachedFromImage)
            oldCachedFromImage->removeClient(&m_crossfadeSubimageObserver);
        if (m_cachedFromImage)
            m_cachedFromImage->addClient(&m_crossfadeSubimageObserver);
    }

    if (m_cachedToImage != oldCachedToImage) {
        if (oldCachedToImage)
            oldCachedToImage->removeClient(&m_crossfadeSubimageObserver);
        if (m_cachedToImage)
            m_cachedToImage->addClient(&m_crossfadeSubimageObserver);
    }

    m_crossfadeSubimageObserver.setReady(true);
}

PassRefPtr<Image> CSSCrossfadeValue::image(const LayoutObject* layoutObject, const IntSize& size)
{
    if (size.isEmpty())
        return nullptr;

    Image* fromImage = renderableImageForCSSValue(m_fromValue.get(), layoutObject);
    Image* toImage = renderableImageForCSSValue(m_toValue.get(), layoutObject);

    if (!fromImage || !toImage)
        return Image::nullImage();

    RefPtr<Image> fromImageRef(fromImage);
    RefPtr<Image> toImageRef(toImage);

    if (fromImage->isSVGImage())
        fromImageRef = SVGImageForContainer::create(toSVGImage(fromImage), size, 1, urlForCSSValue(m_fromValue.get()));

    if (toImage->isSVGImage())
        toImageRef = SVGImageForContainer::create(toSVGImage(toImage), size, 1, urlForCSSValue(m_toValue.get()));

    m_generatedImage = CrossfadeGeneratedImage::create(fromImageRef, toImageRef, m_percentageValue->getFloatValue(), fixedSize(layoutObject), size);

    return m_generatedImage.release();
}

void CSSCrossfadeValue::crossfadeChanged(const IntRect&)
{
    for (const auto& curr : clients()) {
        LayoutObject* client = const_cast<LayoutObject*>(curr.key);
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

void CSSCrossfadeValue::CrossfadeSubimageObserverProxy::imageChanged(ImageResource*, const IntRect* rect)
{
    if (m_ready)
        m_ownerValue->crossfadeChanged(*rect);
}

bool CSSCrossfadeValue::hasFailedOrCanceledSubresources() const
{
    if (m_cachedFromImage && m_cachedFromImage->loadFailedOrCanceled())
        return true;
    if (m_cachedToImage && m_cachedToImage->loadFailedOrCanceled())
        return true;
    return false;
}

bool CSSCrossfadeValue::equals(const CSSCrossfadeValue& other) const
{
    return compareCSSValuePtr(m_fromValue, other.m_fromValue)
        && compareCSSValuePtr(m_toValue, other.m_toValue)
        && compareCSSValuePtr(m_percentageValue, other.m_percentageValue);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSCrossfadeValue)
{
    visitor->trace(m_fromValue);
    visitor->trace(m_toValue);
    visitor->trace(m_percentageValue);
    visitor->trace(m_cachedFromImage);
    visitor->trace(m_cachedToImage);
    visitor->trace(m_crossfadeSubimageObserver);
    CSSImageGeneratorValue::traceAfterDispatch(visitor);
}

} // namespace blink
