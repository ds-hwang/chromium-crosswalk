/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/Location.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/DOMURLUtilsReadOnly.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

Location::Location(Frame* frame)
    : m_frame(frame)
{
}

DEFINE_TRACE(Location)
{
    visitor->trace(m_frame);
}

inline const KURL& Location::url() const
{
    const KURL& url = toLocalFrame(m_frame)->document()->url();
    if (!url.isValid())
        return blankURL(); // Use "about:blank" while the page is still loading (before we have a frame).

    return url;
}

String Location::href() const
{
    if (!m_frame)
        return String();

    return url().strippedForUseAsHref();
}

String Location::protocol() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::protocol(url());
}

String Location::host() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::host(url());
}

String Location::hostname() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::hostname(url());
}

String Location::port() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::port(url());
}

String Location::pathname() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::pathname(url());
}

String Location::search() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::search(url());
}

String Location::origin() const
{
    if (!m_frame)
        return String();
    return DOMURLUtilsReadOnly::origin(url());
}

PassRefPtrWillBeRawPtr<DOMStringList> Location::ancestorOrigins() const
{
    RefPtrWillBeRawPtr<DOMStringList> origins = DOMStringList::create(DOMStringList::Location);
    if (!m_frame)
        return origins.release();
    for (Frame* frame = m_frame->tree().parent(); frame; frame = frame->tree().parent())
        origins->append(frame->securityContext()->securityOrigin()->toString());
    return origins.release();
}

String Location::hash() const
{
    if (!m_frame)
        return String();

    return DOMURLUtilsReadOnly::hash(url());
}

void Location::setHref(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& url)
{
    if (!m_frame)
        return;
    setLocation(url, currentWindow, enteredWindow);
}

void Location::setProtocol(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& protocol, ExceptionState& exceptionState)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    if (!url.setProtocol(protocol)) {
        exceptionState.throwDOMException(SyntaxError, "'" + protocol + "' is an invalid protocol.");
        return;
    }
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::setHost(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& host)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    url.setHostAndPort(host);
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::setHostname(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& hostname)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    url.setHost(hostname);
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::setPort(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& portString)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    url.setPort(portString);
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::setPathname(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& pathname)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    url.setPath(pathname);
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::setSearch(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& search)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    url.setQuery(search);
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::setHash(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& hash)
{
    if (!m_frame)
        return;
    KURL url = toLocalFrame(m_frame)->document()->url();
    String oldFragmentIdentifier = url.fragmentIdentifier();
    String newFragmentIdentifier = hash;
    if (hash[0] == '#')
        newFragmentIdentifier = hash.substring(1);
    url.setFragmentIdentifier(newFragmentIdentifier);
    // Note that by parsing the URL and *then* comparing fragments, we are
    // comparing fragments post-canonicalization, and so this handles the
    // cases where fragment identifiers are ignored or invalid.
    if (equalIgnoringNullity(oldFragmentIdentifier, url.fragmentIdentifier()))
        return;
    setLocation(url.string(), currentWindow, enteredWindow);
}

void Location::assign(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& url)
{
    if (!m_frame)
        return;
    setLocation(url, currentWindow, enteredWindow);
}

void Location::replace(LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, const String& url)
{
    if (!m_frame)
        return;
    setLocation(url, currentWindow, enteredWindow, SetLocation::ReplaceThisFrame);
}

void Location::reload(LocalDOMWindow* currentWindow)
{
    if (!m_frame)
        return;
    if (protocolIsJavaScript(toLocalFrame(m_frame)->document()->url()))
        return;
    m_frame->reload(FrameLoadTypeReload, ClientRedirect);
}

void Location::setLocation(const String& url, LocalDOMWindow* currentWindow, LocalDOMWindow* enteredWindow, SetLocation locationPolicy)
{
    ASSERT(m_frame);
    if (!m_frame || !m_frame->host())
        return;

    if (!currentWindow->frame() || !currentWindow->frame()->canNavigate(*m_frame))
        return;

    Document* enteredDocument = enteredWindow->document();
    if (!enteredDocument)
        return;

    KURL completedURL = enteredDocument->completeURL(url);
    if (completedURL.isNull())
        return;

    if (m_frame->domWindow()->isInsecureScriptAccess(*currentWindow, completedURL))
        return;

    V8DOMActivityLogger* activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    if (activityLogger) {
        Vector<String> argv;
        argv.append("LocalDOMWindow");
        argv.append("url");
        argv.append(enteredDocument->url());
        argv.append(completedURL);
        activityLogger->logEvent("blinkSetAttribute", argv.size(), argv.data());
    }
    m_frame->navigate(*currentWindow->document(), completedURL, locationPolicy == SetLocation::ReplaceThisFrame, UserGestureStatus::None);
}

} // namespace blink
