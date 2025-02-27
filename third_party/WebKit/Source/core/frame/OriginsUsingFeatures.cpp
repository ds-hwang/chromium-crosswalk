// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/OriginsUsingFeatures.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/page/Page.h"
#include "public/platform/Platform.h"

namespace blink {

OriginsUsingFeatures::~OriginsUsingFeatures()
{
    updateMeasurementsAndClear();
}

OriginsUsingFeatures::Value::Value()
    : m_countBits(0)
{
}

void OriginsUsingFeatures::countAnyWorld(Document& document, Feature feature)
{
    document.originsUsingFeaturesValue().count(feature);
}

void OriginsUsingFeatures::countMainWorldOnly(const ScriptState* scriptState, Document& document, Feature feature)
{
    if (!scriptState || !scriptState->world().isMainWorld())
        return;
    countAnyWorld(document, feature);
}

static Document* documentFromEventTarget(EventTarget& target)
{
    ExecutionContext* executionContext = target.executionContext();
    if (!executionContext)
        return nullptr;
    if (executionContext->isDocument())
        return toDocument(executionContext);
    if (LocalDOMWindow* executingWindow = executionContext->executingWindow())
        return executingWindow->document();
    return nullptr;
}

void OriginsUsingFeatures::countOriginOrIsolatedWorldHumanReadableName(const ScriptState* scriptState, EventTarget& target, Feature feature)
{
    if (!scriptState)
        return;
    Document* document = documentFromEventTarget(target);
    if (!document)
        return;
    if (scriptState->world().isMainWorld()) {
        document->originsUsingFeaturesValue().count(feature);
        return;
    }
    if (Page* page = document->page())
        page->originsUsingFeatures().countName(feature, scriptState->world().isolatedWorldHumanReadableName());
}

void OriginsUsingFeatures::Value::count(Feature feature)
{
    ASSERT(feature < Feature::NumberOfFeatures);
    m_countBits |= 1 << static_cast<unsigned>(feature);
}

void OriginsUsingFeatures::countName(Feature feature, const String& name)
{
    auto result = m_valueByName.add(name, Value());
    result.storedValue->value.count(feature);
}

void OriginsUsingFeatures::clear()
{
    m_originAndValues.clear();
    m_valueByName.clear();
}

void OriginsUsingFeatures::documentDetached(Document& document)
{
    OriginsUsingFeatures::Value counter = document.originsUsingFeaturesValue();
    if (counter.isEmpty())
        return;

    const KURL& url = document.url();
    if (!url.protocolIsInHTTPFamily())
        return;

    m_originAndValues.append(std::make_pair(url.host(), counter));
    document.originsUsingFeaturesValue().clear();
    ASSERT(document.originsUsingFeaturesValue().isEmpty());
}

void OriginsUsingFeatures::updateMeasurementsAndClear()
{
    if (!m_originAndValues.isEmpty())
        recordOriginsToRappor();
    if (!m_valueByName.isEmpty())
        recordNamesToRappor();
}

void OriginsUsingFeatures::recordOriginsToRappor()
{
    ASSERT(!m_originAndValues.isEmpty());

    // Aggregate values by origins.
    HashMap<String, OriginsUsingFeatures::Value> aggregatedByOrigin;
    for (const auto& originAndValue : m_originAndValues) {
        ASSERT(!originAndValue.first.isEmpty());
        auto result = aggregatedByOrigin.add(originAndValue.first, originAndValue.second);
        if (!result.isNewEntry)
            result.storedValue->value.aggregate(originAndValue.second);
    }

    // Report to RAPPOR.
    for (auto& originAndValue : aggregatedByOrigin)
        originAndValue.value.recordOriginToRappor(originAndValue.key);

    m_originAndValues.clear();
}

void OriginsUsingFeatures::recordNamesToRappor()
{
    ASSERT(!m_valueByName.isEmpty());

    for (auto& nameAndValue : m_valueByName)
        nameAndValue.value.recordNameToRappor(nameAndValue.key);

    m_valueByName.clear();
}

void OriginsUsingFeatures::Value::aggregate(OriginsUsingFeatures::Value other)
{
    m_countBits |= other.m_countBits;
}

void OriginsUsingFeatures::Value::recordOriginToRappor(const String& origin)
{
    if (get(Feature::ElementCreateShadowRoot))
        Platform::current()->recordRappor("WebComponents.ElementCreateShadowRoot", origin);
    if (get(Feature::ElementAttachShadow))
        Platform::current()->recordRappor("WebComponents.ElementAttachShadow", origin);
    if (get(Feature::DocumentRegisterElement))
        Platform::current()->recordRappor("WebComponents.DocumentRegisterElement", origin);
    if (get(Feature::EventPath))
        Platform::current()->recordRappor("WebComponents.EventPath", origin);
    if (get(Feature::DeviceMotionInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.DeviceMotion.Insecure", origin);
    if (get(Feature::DeviceOrientationInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.DeviceOrientation.Insecure", origin);
    if (get(Feature::FullscreenInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.Fullscreen.Insecure", origin);
    if (get(Feature::GeolocationInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.Geolocation.Insecure", origin);
    if (get(Feature::GetUserMediaInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.GetUserMedia.Insecure", origin);
    if (get(Feature::GetUserMediaSecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.GetUserMedia.Secure", origin);
    if (get(Feature::ApplicationCacheManifestSelectInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.ApplicationCacheManifestSelect.Insecure", origin);
    if (get(Feature::ApplicationCacheAPIInsecureOrigin))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.ApplicationCacheAPI.Insecure", origin);
}

void OriginsUsingFeatures::Value::recordNameToRappor(const String& name)
{
    if (get(Feature::EventPath))
        Platform::current()->recordRappor("WebComponents.EventPath.Extensions", name);
}

} // namespace blink
