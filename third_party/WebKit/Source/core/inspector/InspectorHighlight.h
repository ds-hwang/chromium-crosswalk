// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorHighlight_h
#define InspectorHighlight_h

#include "core/CoreExport.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/TypeBuilder.h"

namespace blink {

class Color;

namespace protocol {
class Value;
}

struct CORE_EXPORT InspectorHighlightConfig {
    USING_FAST_MALLOC(InspectorHighlightConfig);
public:
    InspectorHighlightConfig();

    Color content;
    Color contentOutline;
    Color padding;
    Color border;
    Color margin;
    Color eventTarget;
    Color shape;
    Color shapeMargin;

    bool showInfo;
    bool showRulers;
    bool showExtensionLines;
    bool displayAsMaterial;

    String selectorList;
};

class CORE_EXPORT InspectorHighlight {
    STACK_ALLOCATED();
public:
    InspectorHighlight(Node*, const InspectorHighlightConfig&, bool appendElementInfo);
    InspectorHighlight();
    ~InspectorHighlight();

    static bool getBoxModel(Node*, OwnPtr<protocol::DOM::BoxModel>*);
    static InspectorHighlightConfig defaultConfig();
    static bool buildNodeQuads(Node*, FloatQuad* content, FloatQuad* padding, FloatQuad* border, FloatQuad* margin);

    void appendPath(PassRefPtr<protocol::ListValue> path, const Color& fillColor, const Color& outlineColor, const String& name = String());
    void appendQuad(const FloatQuad&, const Color& fillColor, const Color& outlineColor = Color::transparent, const String& name = String());
    void appendEventTargetQuads(Node* eventTargetNode, const InspectorHighlightConfig&);
    PassRefPtr<protocol::DictionaryValue> asProtocolValue() const;

private:
    void appendNodeHighlight(Node*, const InspectorHighlightConfig&);
    void appendPathsForShapeOutside(Node*, const InspectorHighlightConfig&);

    RefPtr<protocol::DictionaryValue> m_elementInfo;
    RefPtr<protocol::ListValue> m_highlightPaths;
    bool m_showRulers;
    bool m_showExtensionLines;
    bool m_displayAsMaterial;
};

} // namespace blink

#endif // InspectorHighlight_h
