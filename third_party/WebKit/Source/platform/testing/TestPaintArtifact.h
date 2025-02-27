// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TestPaintArtifact_h
#define TestPaintArtifact_h

#include "platform/graphics/Color.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "wtf/Allocator.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class FloatRect;
class PaintArtifact;
class TransformPaintPropertyNode;

// Useful for quickly making a paint artifact in unit tests.
// Must remain in scope while the paint artifact is used, because it owns the
// display item clients.
//
// Usage:
//   TestPaintArtifact artifact;
//   artifact.chunk(paintProperties)
//       .rectDrawing(bounds, color)
//       .rectDrawing(bounds2, color2);
//   artifact.chunk(otherPaintProperties)
//       .rectDrawing(bounds3, color3);
//   doSomethingWithArtifact(artifact);
class TestPaintArtifact {
    STACK_ALLOCATED();
public:
    TestPaintArtifact();
    ~TestPaintArtifact();

    // Add to the artifact.
    TestPaintArtifact& chunk(PassRefPtr<TransformPaintPropertyNode>, PassRefPtr<ClipPaintPropertyNode>, PassRefPtr<EffectPaintPropertyNode>);
    TestPaintArtifact& chunk(const PaintChunkProperties&);
    TestPaintArtifact& rectDrawing(const FloatRect& bounds, Color);

    // Can't add more things once this is called.
    const PaintArtifact& build();

private:
    class DummyRectClient;
    Vector<OwnPtr<DummyRectClient>> m_dummyClients;

    // Exists if m_built is false.
    DisplayItemList m_displayItemList;
    Vector<PaintChunk> m_paintChunks;

    // Exists if m_built is true.
    PaintArtifact m_paintArtifact;

    bool m_built;

    // To make MSVC happy.
    friend struct WTF::OwnedPtrDeleter<DummyRectClient>;
};

} // namespace blink

#endif // TestPaintArtifact_h
