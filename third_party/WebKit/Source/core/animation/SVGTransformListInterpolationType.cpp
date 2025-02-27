// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGTransformListInterpolationType.h"

#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/SVGTransformList.h"

namespace blink {

class SVGTransformNonInterpolableValue : public NonInterpolableValue {
public:
    virtual ~SVGTransformNonInterpolableValue() {}

    static PassRefPtr<SVGTransformNonInterpolableValue> create(Vector<SVGTransformType>& transformTypes)
    {
        return adoptRef(new SVGTransformNonInterpolableValue(transformTypes));
    }

    const Vector<SVGTransformType>& transformTypes() const { return m_transformTypes; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    SVGTransformNonInterpolableValue(Vector<SVGTransformType>& transformTypes)
    {
        m_transformTypes.swap(transformTypes);
    }

    Vector<SVGTransformType> m_transformTypes;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGTransformNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGTransformNonInterpolableValue);

namespace {

PassOwnPtr<InterpolableValue> translateToInterpolableValue(SVGTransform* transform)
{
    FloatPoint translate = transform->translate();
    OwnPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, InterpolableNumber::create(translate.x()));
    result->set(1, InterpolableNumber::create(translate.y()));
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGTransform> translateFromInterpolableValue(const InterpolableValue& value)
{
    const InterpolableList& list = toInterpolableList(value);

    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_TRANSLATE);
    transform->setTranslate(
        toInterpolableNumber(list.get(0))->value(),
        toInterpolableNumber(list.get(1))->value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> scaleToInterpolableValue(SVGTransform* transform)
{
    FloatSize scale = transform->scale();
    OwnPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, InterpolableNumber::create(scale.width()));
    result->set(1, InterpolableNumber::create(scale.height()));
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGTransform> scaleFromInterpolableValue(const InterpolableValue& value)
{
    const InterpolableList& list = toInterpolableList(value);

    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_SCALE);
    transform->setScale(
        toInterpolableNumber(list.get(0))->value(),
        toInterpolableNumber(list.get(1))->value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> rotateToInterpolableValue(SVGTransform* transform)
{
    FloatPoint rotationCenter = transform->rotationCenter();
    OwnPtr<InterpolableList> result = InterpolableList::create(3);
    result->set(0, InterpolableNumber::create(transform->angle()));
    result->set(1, InterpolableNumber::create(rotationCenter.x()));
    result->set(2, InterpolableNumber::create(rotationCenter.y()));
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGTransform> rotateFromInterpolableValue(const InterpolableValue& value)
{
    const InterpolableList& list = toInterpolableList(value);

    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_ROTATE);
    transform->setRotate(
        toInterpolableNumber(list.get(0))->value(),
        toInterpolableNumber(list.get(1))->value(),
        toInterpolableNumber(list.get(2))->value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> skewXToInterpolableValue(SVGTransform* transform)
{
    return InterpolableNumber::create(transform->angle());
}

PassRefPtrWillBeRawPtr<SVGTransform> skewXFromInterpolableValue(const InterpolableValue& value)
{
    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_SKEWX);
    transform->setSkewX(toInterpolableNumber(value).value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> skewYToInterpolableValue(SVGTransform* transform)
{
    return InterpolableNumber::create(transform->angle());
}

PassRefPtrWillBeRawPtr<SVGTransform> skewYFromInterpolableValue(const InterpolableValue& value)
{
    RefPtrWillBeRawPtr<SVGTransform> transform = SVGTransform::create(SVG_TRANSFORM_SKEWY);
    transform->setSkewY(toInterpolableNumber(value).value());
    return transform.release();
}

PassOwnPtr<InterpolableValue> toInterpolableValue(SVGTransform* transform, SVGTransformType transformType)
{
    switch (transformType) {
    case SVG_TRANSFORM_TRANSLATE:
        return translateToInterpolableValue(transform);
    case SVG_TRANSFORM_SCALE:
        return scaleToInterpolableValue(transform);
    case SVG_TRANSFORM_ROTATE:
        return rotateToInterpolableValue(transform);
    case SVG_TRANSFORM_SKEWX:
        return skewXToInterpolableValue(transform);
    case SVG_TRANSFORM_SKEWY:
        return skewYToInterpolableValue(transform);
    case SVG_TRANSFORM_MATRIX:
    case SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtrWillBeRawPtr<SVGTransform> fromInterpolableValue(const InterpolableValue& value, SVGTransformType transformType)
{
    switch (transformType) {
    case SVG_TRANSFORM_TRANSLATE:
        return translateFromInterpolableValue(value);
    case SVG_TRANSFORM_SCALE:
        return scaleFromInterpolableValue(value);
    case SVG_TRANSFORM_ROTATE:
        return rotateFromInterpolableValue(value);
    case SVG_TRANSFORM_SKEWX:
        return skewXFromInterpolableValue(value);
    case SVG_TRANSFORM_SKEWY:
        return skewYFromInterpolableValue(value);
    case SVG_TRANSFORM_MATRIX:
    case SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

const Vector<SVGTransformType>& getTransformTypes(const InterpolationValue& value)
{
    return toSVGTransformNonInterpolableValue(*value.nonInterpolableValue).transformTypes();
}

bool transformTypesMatch(const InterpolationValue& first, const InterpolationValue& second)
{
    const Vector<SVGTransformType>& firstTransformTypes = getTransformTypes(first);
    const Vector<SVGTransformType>& secondTransformTypes = getTransformTypes(second);
    return firstTransformTypes == secondTransformTypes;
}

class SVGTransformListChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<SVGTransformListChecker> create(const InterpolationValue& underlying)
    {
        return adoptPtr(new SVGTransformListChecker(underlying));
    }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        // TODO(suzyh): change maybeConvertSingle so we don't have to recalculate for changes to the interpolable values
        if (!underlying && !m_underlying)
            return true;
        if (!underlying || !m_underlying)
            return false;
        return m_underlying.interpolableValue->equals(*underlying.interpolableValue)
            && getTransformTypes(m_underlying) == getTransformTypes(underlying);
    }

private:
    SVGTransformListChecker(const InterpolationValue& underlying)
        : m_underlying(underlying.clone())
    { }

    const InterpolationValue m_underlying;
};

} // namespace

InterpolationValue SVGTransformListInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedTransformList)
        return nullptr;

    const SVGTransformList& svgList = toSVGTransformList(svgValue);
    OwnPtr<InterpolableList> result = InterpolableList::create(svgList.length());

    Vector<SVGTransformType> transformTypes;
    for (size_t i = 0; i < svgList.length(); i++) {
        const SVGTransform* transform = svgList.at(i);
        SVGTransformType transformType(transform->transformType());
        if (transformType == SVG_TRANSFORM_MATRIX) {
            // TODO(ericwilligers): Support matrix interpolation.
            return nullptr;
        }
        result->set(i, toInterpolableValue(transform->clone().get(), transformType));
        transformTypes.append(transformType);
    }
    return InterpolationValue(result.release(), SVGTransformNonInterpolableValue::create(transformTypes));
}

InterpolationValue SVGTransformListInterpolationType::maybeConvertSingle(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    Vector<SVGTransformType> types;
    Vector<OwnPtr<InterpolableValue>> interpolableParts;

    if (keyframe.composite() == EffectModel::CompositeAdd) {
        if (underlying) {
            types.appendVector(getTransformTypes(underlying));
            interpolableParts.append(underlying.interpolableValue->clone());
        }
        conversionCheckers.append(SVGTransformListChecker::create(underlying));
    } else {
        ASSERT(!keyframe.isNeutral());
    }

    if (!keyframe.isNeutral()) {
        RefPtrWillBeRawPtr<SVGPropertyBase> svgValue = environment.svgBaseValue().cloneForAnimation(toSVGPropertySpecificKeyframe(keyframe).value());
        InterpolationValue value = maybeConvertSVGValue(*svgValue);
        if (!value)
            return nullptr;
        types.appendVector(getTransformTypes(value));
        interpolableParts.append(value.interpolableValue.release());
    }

    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(types.size());
    size_t interpolableListIndex = 0;
    for (auto& part : interpolableParts) {
        InterpolableList& list = toInterpolableList(*part);
        for (size_t i = 0; i < list.length(); ++i) {
            interpolableList->set(interpolableListIndex, list.getMutable(i).release());
            ++interpolableListIndex;
        }
    }

    return InterpolationValue(interpolableList.release(), SVGTransformNonInterpolableValue::create(types));
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGTransformListInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue) const
{
    RefPtrWillBeRawPtr<SVGTransformList> result = SVGTransformList::create();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    const Vector<SVGTransformType>& transformTypes = toSVGTransformNonInterpolableValue(nonInterpolableValue)->transformTypes();
    for (size_t i = 0; i < list.length(); ++i)
        result->append(fromInterpolableValue(*list.get(i), transformTypes.at(i)));
    return result.release();
}

PairwiseInterpolationValue SVGTransformListInterpolationType::mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const
{
    if (!transformTypesMatch(start, end))
        return nullptr;

    return PairwiseInterpolationValue(start.interpolableValue.release(), end.interpolableValue.release(), end.nonInterpolableValue.release());
}

void SVGTransformListInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value) const
{
    underlyingValueOwner.set(*this, value);
}

} // namespace blink
