/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/scroll/ScrollbarThemeMacOverlayAPI.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/mac/LocalCurrentGraphicsContext.h"
#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "wtf/RetainPtr.h"

@interface BlinkScrollbarObserver : NSObject {
    blink::ScrollbarThemeClient* _scrollbar;
    RetainPtr<ScrollbarPainter> _scrollbarPainter;
    BOOL _visible;
}
- (id)initWithScrollbar:(blink::ScrollbarThemeClient*)scrollbar painter:(ScrollbarPainter)painter;
@end

@implementation BlinkScrollbarObserver

- (id)initWithScrollbar:(blink::ScrollbarThemeClient*)scrollbar painter:(ScrollbarPainter)painter
{
    if (!(self = [super init]))
        return nil;
    _scrollbar = scrollbar;
    _scrollbarPainter = painter;

    [_scrollbarPainter.get() addObserver:self forKeyPath:@"knobAlpha" options:0 context:nil];
    return self;
}

- (id)painter
{
    return _scrollbarPainter.get();
}

- (void)dealloc
{

    [_scrollbarPainter.get() removeObserver:self forKeyPath:@"knobAlpha"];
    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context
{
    if ([keyPath isEqualToString:@"knobAlpha"]) {
        BOOL visible = [_scrollbarPainter.get() knobAlpha] > 0;
        if (_visible != visible) {
            _visible = visible;
            _scrollbar->visibilityChanged();
        }
    }
}

@end

namespace blink {

typedef HashMap<ScrollbarThemeClient*, RetainPtr<BlinkScrollbarObserver>> ScrollbarPainterMap;

static ScrollbarPainterMap& scrollbarPainterMap()
{
    static ScrollbarPainterMap* map = new ScrollbarPainterMap;
    return *map;
}

static bool supportsExpandedScrollbars()
{
    // FIXME: This is temporary until all platforms that support ScrollbarPainter support this part of the API.
    static bool globalSupportsExpandedScrollbars = [NSClassFromString(@"NSScrollerImp") instancesRespondToSelector:@selector(setExpanded:)];
    return globalSupportsExpandedScrollbars;
}

ScrollbarPart ScrollbarThemeMacOverlayAPI::invalidateOnThumbPositionChange(
    const ScrollbarThemeClient& scrollbar, float oldPosition, float newPosition) const
{
    // ScrollAnimatorMac will invalidate scrollbar parts if necessary.
    return NoPart;
}

void ScrollbarThemeMacOverlayAPI::registerScrollbar(ScrollbarThemeClient& scrollbar)
{
    ScrollbarThemeMacCommon::registerScrollbar(scrollbar);

    bool isHorizontal = scrollbar.orientation() == HorizontalScrollbar;
    ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:recommendedScrollerStyle() controlSize:(NSControlSize)scrollbar.controlSize() horizontal:isHorizontal replacingScrollerImp:nil];
    RetainPtr<BlinkScrollbarObserver> observer = [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar painter:scrollbarPainter];

    scrollbarPainterMap().add(&scrollbar, observer);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

void ScrollbarThemeMacOverlayAPI::unregisterScrollbar(ScrollbarThemeClient& scrollbar)
{
    scrollbarPainterMap().remove(&scrollbar);

    ScrollbarThemeMacCommon::unregisterScrollbar(scrollbar);
}

void ScrollbarThemeMacOverlayAPI::setNewPainterForScrollbar(ScrollbarThemeClient& scrollbar, ScrollbarPainter newPainter)
{
    RetainPtr<BlinkScrollbarObserver> observer = [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar painter:newPainter];
    scrollbarPainterMap().set(&scrollbar, observer);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

ScrollbarPainter ScrollbarThemeMacOverlayAPI::painterForScrollbar(const ScrollbarThemeClient& scrollbar) const
{
    return [scrollbarPainterMap().get(const_cast<ScrollbarThemeClient*>(&scrollbar)).get() painter];
}

void ScrollbarThemeMacOverlayAPI::paintTrackBackground(GraphicsContext& context, const ScrollbarThemeClient& scrollbar, const IntRect& rect) {
    if (DrawingRecorder::useCachedDrawingIfPossible(context, scrollbar, DisplayItem::ScrollbarTrackBackground))
        return;

    DrawingRecorder recorder(context, scrollbar, DisplayItem::ScrollbarTrackBackground, rect);

    ASSERT(isOverlayAPIAvailable());

    GraphicsContextStateSaver stateSaver(context);
    context.translate(rect.x(), rect.y());
    LocalCurrentGraphicsContext localContext(context, IntRect(IntPoint(), rect.size()));

    CGRect frameRect = scrollbar.frameRect();
    ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
    [scrollbarPainter setEnabled:scrollbar.enabled()];
    [scrollbarPainter setBoundsSize: NSSizeFromCGSize(frameRect.size)];
    NSRect trackRect = NSMakeRect(0, 0, frameRect.size.width, frameRect.size.height);
    [scrollbarPainter drawKnobSlotInRect:trackRect highlight:NO];
}

void ScrollbarThemeMacOverlayAPI::paintThumb(GraphicsContext& context, const ScrollbarThemeClient& scrollbar, const IntRect& rect) {
    if (DrawingRecorder::useCachedDrawingIfPossible(context, scrollbar, DisplayItem::ScrollbarThumb))
        return;

    // Expand dirty rect to allow for scroll thumb anti-aliasing in minimum thumb size case.
    IntRect dirtyRect = IntRect(rect);
    dirtyRect.inflate(1);
    DrawingRecorder recorder(context, scrollbar, DisplayItem::ScrollbarThumb, dirtyRect);

    ASSERT(isOverlayAPIAvailable());

    GraphicsContextStateSaver stateSaver(context);
    context.translate(rect.x(), rect.y());
    LocalCurrentGraphicsContext localContext(context, IntRect(IntPoint(), rect.size()));

    ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
    [scrollbarPainter setEnabled:scrollbar.enabled()];
    [scrollbarPainter setBoundsSize:NSSizeFromCGSize(rect.size())];
    [scrollbarPainter setDoubleValue:0];
    [scrollbarPainter setKnobProportion:1];

    CGFloat oldKnobAlpha = [scrollbarPainter knobAlpha];
    [scrollbarPainter setKnobAlpha:1];

    if (scrollbar.enabled())
        [scrollbarPainter drawKnob];

    // If this state is not set, then moving the cursor over the scrollbar area will only cause the
    // scrollbar to engorge when moved over the top of the scrollbar area.
    [scrollbarPainter setBoundsSize: NSSizeFromCGSize(scrollbar.frameRect().size())];
    [scrollbarPainter setKnobAlpha:oldKnobAlpha];
}

int ScrollbarThemeMacOverlayAPI::scrollbarThickness(ScrollbarControlSize controlSize)
{
    NSControlSize nsControlSize = static_cast<NSControlSize>(controlSize);
    ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:recommendedScrollerStyle() controlSize:nsControlSize horizontal:NO replacingScrollerImp:nil];
    BOOL wasExpanded = NO;
    if (supportsExpandedScrollbars()) {
        wasExpanded = [scrollbarPainter isExpanded];
        [scrollbarPainter setExpanded:YES];
    }
    int thickness = [scrollbarPainter trackBoxWidth];
    if (supportsExpandedScrollbars())
        [scrollbarPainter setExpanded:wasExpanded];
    return thickness;
}

bool ScrollbarThemeMacOverlayAPI::usesOverlayScrollbars() const
{
    return recommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMacOverlayAPI::updateScrollbarOverlayStyle(const ScrollbarThemeClient& scrollbar)
{
    ScrollbarPainter painter = painterForScrollbar(scrollbar);
    switch (scrollbar.scrollbarOverlayStyle()) {
    case ScrollbarOverlayStyleDefault:
        [painter setKnobStyle:NSScrollerKnobStyleDefault];
        break;
    case ScrollbarOverlayStyleDark:
        [painter setKnobStyle:NSScrollerKnobStyleDark];
        break;
    case ScrollbarOverlayStyleLight:
        [painter setKnobStyle:NSScrollerKnobStyleLight];
        break;
    }
}

WebScrollbarButtonsPlacement ScrollbarThemeMacOverlayAPI::buttonsPlacement() const
{
    return WebScrollbarButtonsPlacementNone;
}

bool ScrollbarThemeMacOverlayAPI::hasThumb(const ScrollbarThemeClient& scrollbar)
{
    ScrollbarPainter painter = painterForScrollbar(scrollbar);
    int minLengthForThumb = [painter knobMinLength] + [painter trackOverlapEndInset] + [painter knobOverlapEndInset]
        + 2 * ([painter trackEndInset] + [painter knobEndInset]);
    return scrollbar.enabled() && (scrollbar.orientation() == HorizontalScrollbar ?
             scrollbar.width() :
             scrollbar.height()) >= minLengthForThumb;
}

IntRect ScrollbarThemeMacOverlayAPI::backButtonRect(const ScrollbarThemeClient& scrollbar, ScrollbarPart part, bool painting)
{
    ASSERT(buttonsPlacement() == WebScrollbarButtonsPlacementNone);
    return IntRect();
}

IntRect ScrollbarThemeMacOverlayAPI::forwardButtonRect(const ScrollbarThemeClient& scrollbar, ScrollbarPart part, bool painting)
{
    ASSERT(buttonsPlacement() == WebScrollbarButtonsPlacementNone);
    return IntRect();
}

IntRect ScrollbarThemeMacOverlayAPI::trackRect(const ScrollbarThemeClient& scrollbar, bool painting)
{
    ASSERT(!hasButtons(scrollbar));
    return scrollbar.frameRect();
}

int ScrollbarThemeMacOverlayAPI::minimumThumbLength(const ScrollbarThemeClient& scrollbar)
{
    return [painterForScrollbar(scrollbar) knobMinLength];
}

void ScrollbarThemeMacOverlayAPI::updateEnabledState(const ScrollbarThemeClient& scrollbar)
{
    [painterForScrollbar(scrollbar) setEnabled:scrollbar.enabled()];
}

float ScrollbarThemeMacOverlayAPI::thumbOpacity(const ScrollbarThemeClient& scrollbar) const {
    ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
    return [scrollbarPainter knobAlpha];
}

} // namespace blink

