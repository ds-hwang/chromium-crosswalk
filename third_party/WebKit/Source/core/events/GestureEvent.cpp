/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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

#include "core/events/GestureEvent.h"

#include "core/dom/Element.h"
#include "wtf/text/AtomicString.h"

namespace blink {

PassRefPtrWillBeRawPtr<GestureEvent> GestureEvent::create(PassRefPtrWillBeRawPtr<AbstractView> view, const PlatformGestureEvent& event)
{
    AtomicString eventType;
    float deltaX = 0;
    float deltaY = 0;
    float velocityX = 0;
    float velocityY = 0;
    bool inertial = false;

    GestureSource source = GestureSourceUninitialized;
    switch (event.source()) {
    case PlatformGestureSourceTouchpad:
        source = GestureSourceTouchpad;
        break;
    case PlatformGestureSourceTouchscreen:
        source = GestureSourceTouchscreen;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (event.type()) {
    case PlatformEvent::GestureScrollBegin:
        eventType = EventTypeNames::gesturescrollstart; break;
    case PlatformEvent::GestureScrollEnd:
        eventType = EventTypeNames::gesturescrollend; break;
    case PlatformEvent::GestureScrollUpdate:
        // Only deltaX/Y are used when converting this
        // back to a PlatformGestureEvent.
        eventType = EventTypeNames::gesturescrollupdate;
        deltaX = event.deltaX();
        deltaY = event.deltaY();
        inertial = event.inertial();
        break;
    case PlatformEvent::GestureTap:
        eventType = EventTypeNames::gesturetap; break;
    case PlatformEvent::GestureTapUnconfirmed:
        eventType = EventTypeNames::gesturetapunconfirmed; break;
    case PlatformEvent::GestureTapDown:
        eventType = EventTypeNames::gesturetapdown; break;
    case PlatformEvent::GestureShowPress:
        eventType = EventTypeNames::gestureshowpress; break;
    case PlatformEvent::GestureLongPress:
        eventType = EventTypeNames::gesturelongpress; break;
    case PlatformEvent::GestureFlingStart:
        eventType = EventTypeNames::gestureflingstart;
        velocityX = event.velocityX();
        velocityY = event.velocityY();
        break;
    case PlatformEvent::GestureTwoFingerTap:
    case PlatformEvent::GesturePinchBegin:
    case PlatformEvent::GesturePinchEnd:
    case PlatformEvent::GesturePinchUpdate:
    case PlatformEvent::GestureTapDownCancel:
    default:
        return nullptr;
    }
    return adoptRefWillBeNoop(new GestureEvent(eventType, view, event.globalPosition().x(), event.globalPosition().y(), event.position().x(), event.position().y(), event.modifiers(), deltaX, deltaY, velocityX, velocityY, inertial, event.timestamp(), event.resendingPluginId(), source));
}

const AtomicString& GestureEvent::interfaceName() const
{
    // FIXME: when a GestureEvent.idl interface is defined, return the string "GestureEvent".
    // Until that happens, do not advertise an interface that does not exist, since it will
    // trip up the bindings integrity checks.
    return UIEvent::interfaceName();
}

bool GestureEvent::isGestureEvent() const
{
    return true;
}

GestureEvent::GestureEvent()
    : m_deltaX(0)
    , m_deltaY(0)
    , m_velocityX(0)
    , m_velocityY(0)
    , m_inertial(false)
    , m_source(GestureSourceUninitialized)
    , m_resendingPluginId(-1)
{
}

GestureEvent::GestureEvent(const AtomicString& type, PassRefPtrWillBeRawPtr<AbstractView> view, int screenX, int screenY, int clientX, int clientY, PlatformEvent::Modifiers modifiers, float deltaX, float deltaY, float velocityX, float velocityY, bool inertial, double platformTimeStamp, int resendingPluginId, GestureSource source)
    : MouseRelatedEvent(type, true, true, view, 0, IntPoint(screenX, screenY), IntPoint(clientX, clientY), IntPoint(0, 0), modifiers, platformTimeStamp, PositionType::Position)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_velocityX(velocityX)
    , m_velocityY(velocityY)
    , m_inertial(inertial)
    , m_source(source)
    , m_resendingPluginId(resendingPluginId)
{
}

DEFINE_TRACE(GestureEvent)
{
    MouseRelatedEvent::trace(visitor);
}

} // namespace blink
