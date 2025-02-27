/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/resolver/CSSToStyleMap.h"

#include "core/CSSValueKeywords.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/BorderImageLengthBox.h"
#include "core/style/FillLayer.h"

namespace blink {

void CSSToStyleMap::mapFillAttachment(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setAttachment(FillLayer::initialFillAttachment(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue.getValueID()) {
    case CSSValueFixed:
        layer->setAttachment(FixedBackgroundAttachment);
        break;
    case CSSValueScroll:
        layer->setAttachment(ScrollBackgroundAttachment);
        break;
    case CSSValueLocal:
        layer->setAttachment(LocalBackgroundAttachment);
        break;
    default:
        return;
    }
}

void CSSToStyleMap::mapFillClip(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setClip(FillLayer::initialFillClip(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    layer->setClip(primitiveValue.convertTo<EFillBox>());
}

void CSSToStyleMap::mapFillComposite(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setComposite(FillLayer::initialFillComposite(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    layer->setComposite(primitiveValue.convertTo<CompositeOperator>());
}

void CSSToStyleMap::mapFillBlendMode(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setBlendMode(FillLayer::initialFillBlendMode(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    layer->setBlendMode(primitiveValue.convertTo<WebBlendMode>());
}

void CSSToStyleMap::mapFillOrigin(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setOrigin(FillLayer::initialFillOrigin(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    layer->setOrigin(primitiveValue.convertTo<EFillBox>());
}


void CSSToStyleMap::mapFillImage(StyleResolverState& state, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setImage(FillLayer::initialFillImage(layer->type()));
        return;
    }

    CSSPropertyID property = layer->type() == BackgroundFillLayer ? CSSPropertyBackgroundImage : CSSPropertyWebkitMaskImage;
    layer->setImage(state.styleImage(property, value));
}

void CSSToStyleMap::mapFillRepeatX(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setRepeatX(FillLayer::initialFillRepeatX(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    layer->setRepeatX(primitiveValue.convertTo<EFillRepeat>());
}

void CSSToStyleMap::mapFillRepeatY(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setRepeatY(FillLayer::initialFillRepeatY(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    layer->setRepeatY(primitiveValue.convertTo<EFillRepeat>());
}

void CSSToStyleMap::mapFillSize(StyleResolverState& state, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setSizeType(FillLayer::initialFillSizeType(layer->type()));
        layer->setSizeLength(FillLayer::initialFillSizeLength(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue() && !value.isValuePair())
        return;

    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueContain)
        layer->setSizeType(Contain);
    else if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueCover)
        layer->setSizeType(Cover);
    else
        layer->setSizeType(SizeLength);

    LengthSize b = FillLayer::initialFillSizeLength(layer->type());

    if (value.isPrimitiveValue() && (toCSSPrimitiveValue(value).getValueID() == CSSValueContain || toCSSPrimitiveValue(value).getValueID() == CSSValueCover)) {
        layer->setSizeLength(b);
        return;
    }

    Length firstLength;
    Length secondLength;

    if (value.isValuePair()) {
        const CSSValuePair& pair = toCSSValuePair(value);
        firstLength = StyleBuilderConverter::convertLengthOrAuto(state, pair.first());
        secondLength = StyleBuilderConverter::convertLengthOrAuto(state, pair.second());
    } else {
        ASSERT(value.isPrimitiveValue());
        firstLength = StyleBuilderConverter::convertLengthOrAuto(state, value);
        secondLength = Length();
    }

    b.setWidth(firstLength);
    b.setHeight(secondLength);
    layer->setSizeLength(b);
}

void CSSToStyleMap::mapFillXPosition(StyleResolverState& state, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setXPosition(FillLayer::initialFillXPosition(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue() && !value.isValuePair())
        return;

    Length length;
    if (value.isValuePair())
        length = toCSSPrimitiveValue(toCSSValuePair(value).second()).convertToLength(state.cssToLengthConversionData());
    else
        length = toCSSPrimitiveValue(value).convertToLength(state.cssToLengthConversionData());

    layer->setXPosition(length);
    if (value.isValuePair())
        layer->setBackgroundXOrigin(toCSSPrimitiveValue(toCSSValuePair(value).first()).convertTo<BackgroundEdgeOrigin>());
}

void CSSToStyleMap::mapFillYPosition(StyleResolverState& state, FillLayer* layer, const CSSValue& value)
{
    if (value.isInitialValue()) {
        layer->setYPosition(FillLayer::initialFillYPosition(layer->type()));
        return;
    }

    if (!value.isPrimitiveValue() && !value.isValuePair())
        return;

    Length length;
    if (value.isValuePair())
        length = toCSSPrimitiveValue(toCSSValuePair(value).second()).convertToLength(state.cssToLengthConversionData());
    else
        length = toCSSPrimitiveValue(value).convertToLength(state.cssToLengthConversionData());

    layer->setYPosition(length);
    if (value.isValuePair())
        layer->setBackgroundYOrigin(toCSSPrimitiveValue(toCSSValuePair(value).first()).convertTo<BackgroundEdgeOrigin>());
}

void CSSToStyleMap::mapFillMaskSourceType(StyleResolverState&, FillLayer* layer, const CSSValue& value)
{
    EMaskSourceType type = FillLayer::initialFillMaskSourceType(layer->type());
    if (value.isInitialValue()) {
        layer->setMaskSourceType(type);
        return;
    }

    if (!value.isPrimitiveValue())
        return;

    switch (toCSSPrimitiveValue(value).getValueID()) {
    case CSSValueAlpha:
        type = MaskAlpha;
        break;
    case CSSValueLuminance:
        type = MaskLuminance;
        break;
    case CSSValueAuto:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    layer->setMaskSourceType(type);
}

double CSSToStyleMap::mapAnimationDelay(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSTimingData::initialDelay();
    return toCSSPrimitiveValue(value).computeSeconds();
}

Timing::PlaybackDirection CSSToStyleMap::mapAnimationDirection(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSAnimationData::initialDirection();

    switch (toCSSPrimitiveValue(value).getValueID()) {
    case CSSValueNormal:
        return Timing::PlaybackDirectionNormal;
    case CSSValueAlternate:
        return Timing::PlaybackDirectionAlternate;
    case CSSValueReverse:
        return Timing::PlaybackDirectionReverse;
    case CSSValueAlternateReverse:
        return Timing::PlaybackDirectionAlternateReverse;
    default:
        ASSERT_NOT_REACHED();
        return CSSAnimationData::initialDirection();
    }
}

double CSSToStyleMap::mapAnimationDuration(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSTimingData::initialDuration();
    return toCSSPrimitiveValue(value).computeSeconds();
}

Timing::FillMode CSSToStyleMap::mapAnimationFillMode(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSAnimationData::initialFillMode();

    switch (toCSSPrimitiveValue(value).getValueID()) {
    case CSSValueNone:
        return Timing::FillModeNone;
    case CSSValueForwards:
        return Timing::FillModeForwards;
    case CSSValueBackwards:
        return Timing::FillModeBackwards;
    case CSSValueBoth:
        return Timing::FillModeBoth;
    default:
        ASSERT_NOT_REACHED();
        return CSSAnimationData::initialFillMode();
    }
}

double CSSToStyleMap::mapAnimationIterationCount(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSAnimationData::initialIterationCount();
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID() == CSSValueInfinite)
        return std::numeric_limits<double>::infinity();
    return primitiveValue.getFloatValue();
}

AtomicString CSSToStyleMap::mapAnimationName(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSAnimationData::initialName();
    if (value.isCustomIdentValue())
        return AtomicString(toCSSCustomIdentValue(value).value());
    ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
    return CSSAnimationData::initialName();
}

EAnimPlayState CSSToStyleMap::mapAnimationPlayState(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSAnimationData::initialPlayState();
    if (toCSSPrimitiveValue(value).getValueID() == CSSValuePaused)
        return AnimPlayStatePaused;
    ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueRunning);
    return AnimPlayStatePlaying;
}

CSSTransitionData::TransitionProperty CSSToStyleMap::mapAnimationProperty(const CSSValue& value)
{
    if (value.isInitialValue())
        return CSSTransitionData::initialProperty();
    if (value.isCustomIdentValue()) {
        const CSSCustomIdentValue& customIdentValue = toCSSCustomIdentValue(value);
        if (customIdentValue.isKnownPropertyID())
            return CSSTransitionData::TransitionProperty(customIdentValue.valueAsPropertyID());
        return CSSTransitionData::TransitionProperty(customIdentValue.value());
    }
    ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
    return CSSTransitionData::TransitionProperty(CSSTransitionData::TransitionNone);
}

PassRefPtr<TimingFunction> CSSToStyleMap::mapAnimationTimingFunction(const CSSValue& value, bool allowStepMiddle)
{
    // FIXME: We should probably only call into this function with a valid
    // single timing function value which isn't initial or inherit. We can
    // currently get into here with initial since the parser expands unset
    // properties in shorthands to initial.

    if (value.isPrimitiveValue()) {
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
        switch (primitiveValue.getValueID()) {
        case CSSValueLinear:
            return LinearTimingFunction::shared();
        case CSSValueEase:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease);
        case CSSValueEaseIn:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
        case CSSValueEaseOut:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
        case CSSValueEaseInOut:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut);
        case CSSValueStepStart:
            return StepsTimingFunction::preset(StepsTimingFunction::Start);
        case CSSValueStepMiddle:
            if (allowStepMiddle)
                return StepsTimingFunction::preset(StepsTimingFunction::Middle);
            return CSSTimingData::initialTimingFunction();
        case CSSValueStepEnd:
            return StepsTimingFunction::preset(StepsTimingFunction::End);
        default:
            ASSERT_NOT_REACHED();
            return CSSTimingData::initialTimingFunction();
        }
    }

    if (value.isCubicBezierTimingFunctionValue()) {
        const CSSCubicBezierTimingFunctionValue& cubicTimingFunction = toCSSCubicBezierTimingFunctionValue(value);
        return CubicBezierTimingFunction::create(cubicTimingFunction.x1(), cubicTimingFunction.y1(), cubicTimingFunction.x2(), cubicTimingFunction.y2());
    }

    if (value.isInitialValue())
        return CSSTimingData::initialTimingFunction();

    const CSSStepsTimingFunctionValue& stepsTimingFunction = toCSSStepsTimingFunctionValue(value);
    if (stepsTimingFunction.stepAtPosition() == StepsTimingFunction::Middle && !allowStepMiddle)
        return CSSTimingData::initialTimingFunction();
    return StepsTimingFunction::create(stepsTimingFunction.numberOfSteps(), stepsTimingFunction.stepAtPosition());
}

void CSSToStyleMap::mapNinePieceImage(StyleResolverState& state, CSSPropertyID property, const CSSValue& value, NinePieceImage& image)
{
    // If we're not a value list, then we are "none" and don't need to alter the empty image at all.
    if (!value.isValueList())
        return;

    // Retrieve the border image value.
    const CSSValueList& borderImage = toCSSValueList(value);

    // Set the image (this kicks off the load).
    CSSPropertyID imageProperty;
    if (property == CSSPropertyWebkitBorderImage)
        imageProperty = CSSPropertyBorderImageSource;
    else if (property == CSSPropertyWebkitMaskBoxImage)
        imageProperty = CSSPropertyWebkitMaskBoxImageSource;
    else
        imageProperty = property;

    for (unsigned i = 0 ; i < borderImage.length() ; ++i) {
        const CSSValue& current = *borderImage.item(i);

        if (current.isImageValue() || current.isImageGeneratorValue() || current.isImageSetValue()) {
            image.setImage(state.styleImage(imageProperty, current));
        } else if (current.isBorderImageSliceValue()) {
            mapNinePieceImageSlice(state, current, image);
        } else if (current.isValueList()) {
            const CSSValueList& slashList = toCSSValueList(current);
            size_t length = slashList.length();
            // Map in the image slices.
            if (length && slashList.item(0)->isBorderImageSliceValue())
                mapNinePieceImageSlice(state, *slashList.item(0), image);

            // Map in the border slices.
            if (length > 1)
                image.setBorderSlices(mapNinePieceImageQuad(state, *slashList.item(1)));

            // Map in the outset.
            if (length > 2)
                image.setOutset(mapNinePieceImageQuad(state, *slashList.item(2)));
        } else if (current.isPrimitiveValue() || current.isValuePair()) {
            // Set the appropriate rules for stretch/round/repeat of the slices.
            mapNinePieceImageRepeat(state, current, image);
        }
    }

    if (property == CSSPropertyWebkitBorderImage) {
        // We have to preserve the legacy behavior of -webkit-border-image and make the border slices
        // also set the border widths. We don't need to worry about percentages, since we don't even support
        // those on real borders yet.
        if (image.borderSlices().top().isLength() && image.borderSlices().top().length().isFixed())
            state.style()->setBorderTopWidth(image.borderSlices().top().length().value());
        if (image.borderSlices().right().isLength() && image.borderSlices().right().length().isFixed())
            state.style()->setBorderRightWidth(image.borderSlices().right().length().value());
        if (image.borderSlices().bottom().isLength() && image.borderSlices().bottom().length().isFixed())
            state.style()->setBorderBottomWidth(image.borderSlices().bottom().length().value());
        if (image.borderSlices().left().isLength() && image.borderSlices().left().length().isFixed())
            state.style()->setBorderLeftWidth(image.borderSlices().left().length().value());
    }
}

static Length convertBorderImageSliceSide(const CSSPrimitiveValue& value)
{
    if (value.isPercentage())
        return Length(value.getDoubleValue(), Percent);
    return Length(round(value.getDoubleValue()), Fixed);
}

void CSSToStyleMap::mapNinePieceImageSlice(StyleResolverState&, const CSSValue& value, NinePieceImage& image)
{
    if (!value.isBorderImageSliceValue())
        return;

    // Retrieve the border image value.
    const CSSBorderImageSliceValue& borderImageSlice = toCSSBorderImageSliceValue(value);

    // Set up a length box to represent our image slices.
    LengthBox box;
    CSSQuadValue* slices = borderImageSlice.slices();
    box.m_top = convertBorderImageSliceSide(*slices->top());
    box.m_bottom = convertBorderImageSliceSide(*slices->bottom());
    box.m_left = convertBorderImageSliceSide(*slices->left());
    box.m_right = convertBorderImageSliceSide(*slices->right());
    image.setImageSlices(box);

    // Set our fill mode.
    image.setFill(borderImageSlice.m_fill);
}

static BorderImageLength toBorderImageLength(CSSPrimitiveValue& value, const CSSToLengthConversionData& conversionData)
{
    if (value.isNumber())
        return value.getDoubleValue();
    if (value.isPercentage())
        return Length(value.getDoubleValue(), Percent);
    if (value.getValueID() != CSSValueAuto)
        return value.computeLength<Length>(conversionData);
    return Length(Auto);
}

BorderImageLengthBox CSSToStyleMap::mapNinePieceImageQuad(StyleResolverState& state, const CSSValue& value)
{
    if (!value.isQuadValue())
        return BorderImageLengthBox(Length(Auto));

    const CSSQuadValue& slices = toCSSQuadValue(value);

    // Set up a border image length box to represent our image slices.
    return BorderImageLengthBox(
        toBorderImageLength(*slices.top(), state.cssToLengthConversionData()),
        toBorderImageLength(*slices.right(), state.cssToLengthConversionData()),
        toBorderImageLength(*slices.bottom(), state.cssToLengthConversionData()),
        toBorderImageLength(*slices.left(), state.cssToLengthConversionData()));
}

void CSSToStyleMap::mapNinePieceImageRepeat(StyleResolverState&, const CSSValue& value, NinePieceImage& image)
{
    if (!value.isValuePair())
        return;

    const CSSValuePair& pair = toCSSValuePair(value);
    CSSValueID firstIdentifier = toCSSPrimitiveValue(pair.first()).getValueID();
    CSSValueID secondIdentifier = toCSSPrimitiveValue(pair.second()).getValueID();

    ENinePieceImageRule horizontalRule;
    switch (firstIdentifier) {
    case CSSValueStretch:
        horizontalRule = StretchImageRule;
        break;
    case CSSValueRound:
        horizontalRule = RoundImageRule;
        break;
    case CSSValueSpace:
        horizontalRule = SpaceImageRule;
        break;
    default: // CSSValueRepeat
        horizontalRule = RepeatImageRule;
        break;
    }
    image.setHorizontalRule(horizontalRule);

    ENinePieceImageRule verticalRule;
    switch (secondIdentifier) {
    case CSSValueStretch:
        verticalRule = StretchImageRule;
        break;
    case CSSValueRound:
        verticalRule = RoundImageRule;
        break;
    case CSSValueSpace:
        verticalRule = SpaceImageRule;
        break;
    default: // CSSValueRepeat
        verticalRule = RepeatImageRule;
        break;
    }
    image.setVerticalRule(verticalRule);
}

} // namespace blink
