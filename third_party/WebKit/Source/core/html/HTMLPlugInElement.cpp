/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "core/html/HTMLPlugInElement.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/npruntime_impl.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/PluginDocument.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutEmbeddedObject.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutPart.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/plugins/PluginView.h"
#include "platform/Logging.h"
#include "platform/MIMETypeFromURL.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/Widget.h"
#include "platform/plugins/PluginData.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& doc, bool createdByParser, PreferPlugInsForImagesOption preferPlugInsForImagesOption)
    : HTMLFrameOwnerElement(tagName, doc)
    , m_isDelayingLoadEvent(false)
    , m_NPObject(0)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed. For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
    , m_shouldPreferPlugInsForImages(preferPlugInsForImagesOption == ShouldPreferPlugInsForImages)
{
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_pluginWrapper); // cleared in detach()
    ASSERT(!m_isDelayingLoadEvent);

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }
}

DEFINE_TRACE(HTMLPlugInElement)
{
    visitor->trace(m_imageLoader);
    visitor->trace(m_persistedPluginWidget);
    HTMLFrameOwnerElement::trace(visitor);
}

void HTMLPlugInElement::setPersistedPluginWidget(Widget* widget)
{
    if (m_persistedPluginWidget == widget)
        return;
#if ENABLE(OILPAN)
    if (m_persistedPluginWidget) {
        if (m_persistedPluginWidget->isPluginView()) {
            m_persistedPluginWidget->hide();
            m_persistedPluginWidget->dispose();
        } else {
            ASSERT(m_persistedPluginWidget->isFrameView());
        }
    }
#endif
    m_persistedPluginWidget = widget;
}

bool HTMLPlugInElement::canProcessDrag() const
{
    return pluginWidget() && pluginWidget()->isPluginView() && toPluginView(pluginWidget())->canProcessDrag();
}

bool HTMLPlugInElement::willRespondToMouseClickEvents()
{
    if (isDisabledFormControl())
        return false;
    LayoutObject* r = layoutObject();
    return r && (r->isEmbeddedObject() || r->isLayoutPart());
}

void HTMLPlugInElement::removeAllEventListeners()
{
    HTMLFrameOwnerElement::removeAllEventListeners();
    if (LayoutPart* layoutObject = existingLayoutPart()) {
        if (Widget* widget = layoutObject->widget())
            widget->eventListenersRemoved();
    }
}

void HTMLPlugInElement::didMoveToNewDocument(Document& oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLFrameOwnerElement::didMoveToNewDocument(oldDocument);
}

void HTMLPlugInElement::attach(const AttachContext& context)
{
    HTMLFrameOwnerElement::attach(context);

    if (!layoutObject() || useFallbackContent()) {
        // If we don't have a layoutObject we have to dispose of any plugins
        // which we persisted over a reattach.
        if (m_persistedPluginWidget) {
            HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
            setPersistedPluginWidget(nullptr);
        }
        return;
    }

    if (isImageType()) {
        if (!m_imageLoader)
            m_imageLoader = HTMLImageLoader::create(this);
        m_imageLoader->updateFromElement();
    } else if (needsWidgetUpdate()
        && layoutEmbeddedObject()
        && !layoutEmbeddedObject()->showsUnavailablePluginIndicator()
        && !wouldLoadAsNetscapePlugin(m_url, m_serviceType)
        && !m_isDelayingLoadEvent) {
        m_isDelayingLoadEvent = true;
        document().incrementLoadEventDelayCount();
        document().loadPluginsSoon();
    }
}

void HTMLPlugInElement::updateWidget()
{
    RefPtrWillBeRawPtr<HTMLPlugInElement> protector(this);
    updateWidgetInternal();
    if (m_isDelayingLoadEvent) {
        m_isDelayingLoadEvent = false;
        document().decrementLoadEventDelayCount();
    }
}

void HTMLPlugInElement::removedFrom(ContainerNode* insertionPoint)
{
    // If we've persisted the plugin and we're removed from the tree then
    // make sure we cleanup the persistance pointer.
    if (m_persistedPluginWidget) {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
        setPersistedPluginWidget(nullptr);
    }
    HTMLFrameOwnerElement::removedFrom(insertionPoint);
}

void HTMLPlugInElement::requestPluginCreationWithoutLayoutObjectIfPossible()
{
    if (m_serviceType.isEmpty())
        return;

    if (!document().frame()
        || !document().frame()->loader().client()->canCreatePluginWithoutRenderer(m_serviceType))
        return;

    if (layoutObject() && layoutObject()->isLayoutPart())
        return;

    createPluginWithoutLayoutObject();
}

void HTMLPlugInElement::createPluginWithoutLayoutObject()
{
    ASSERT(document().frame()->loader().client()->canCreatePluginWithoutRenderer(m_serviceType));

    KURL url;
    // CSP can block src-less objects.
    if (!allowedToLoadObject(url, m_serviceType))
        return;

    Vector<String> paramNames;
    Vector<String> paramValues;

    paramNames.append("type");
    paramValues.append(m_serviceType);

    bool useFallback = false;
    loadPlugin(url, m_serviceType, paramNames, paramValues, useFallback, false);
}

bool HTMLPlugInElement::shouldAccelerate() const
{
    if (Widget* widget = ownedWidget())
        return widget->isPluginView() && toPluginView(widget)->platformLayer();
    return false;
}

void HTMLPlugInElement::detach(const AttachContext& context)
{
    // Update the widget the next time we attach (detaching destroys the plugin).
    // FIXME: None of this "needsWidgetUpdate" related code looks right.
    if (layoutObject() && !useFallbackContent())
        setNeedsWidgetUpdate(true);
    if (m_isDelayingLoadEvent) {
        m_isDelayingLoadEvent = false;
        document().decrementLoadEventDelayCount();
    }

    // Only try to persist a plugin widget we actually own.
    Widget* plugin = ownedWidget();
    if (plugin && context.performingReattach) {
        setPersistedPluginWidget(releaseWidget().get());
    } else {
        // Clear the widget; will trigger disposal of it with Oilpan.
        setWidget(nullptr);
    }

    resetInstance();

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }

    HTMLFrameOwnerElement::detach(context);
}

LayoutObject* HTMLPlugInElement::createLayoutObject(const ComputedStyle& style)
{
    // Fallback content breaks the DOM->layoutObject class relationship of this
    // class and all superclasses because createObject won't necessarily return
    // a LayoutEmbeddedObject or LayoutPart.
    if (useFallbackContent())
        return LayoutObject::createObject(this, style);

    if (isImageType()) {
        LayoutImage* image = new LayoutImage(this);
        image->setImageResource(LayoutImageResource::create());
        return image;
    }


    return new LayoutEmbeddedObject(this);
}

void HTMLPlugInElement::finishParsingChildren()
{
    HTMLFrameOwnerElement::finishParsingChildren();
    if (useFallbackContent())
        return;

    setNeedsWidgetUpdate(true);
    if (inDocument())
        lazyReattachIfNeeded();
}

void HTMLPlugInElement::resetInstance()
{
    m_pluginWrapper.clear();
}

SharedPersistent<v8::Object>* HTMLPlugInElement::pluginWrapper()
{
    LocalFrame* frame = document().frame();
    if (!frame)
        return nullptr;

    // If the host dynamically turns off JavaScript (or Java) we will still
    // return the cached allocated Bindings::Instance. Not supporting this
    // edge-case is OK.
    if (!m_pluginWrapper) {
        Widget* plugin;

        if (m_persistedPluginWidget)
            plugin = m_persistedPluginWidget.get();
        else
            plugin = pluginWidget();

        if (plugin)
            m_pluginWrapper = frame->script().createPluginWrapper(plugin);
    }
    return m_pluginWrapper.get();
}

Widget* HTMLPlugInElement::pluginWidget() const
{
    if (LayoutPart* layoutPart = layoutPartForJSBindings())
        return layoutPart->widget();
    return nullptr;
}

bool HTMLPlugInElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr)
        return true;
    return HTMLFrameOwnerElement::isPresentationAttribute(name);
}

void HTMLPlugInElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr) {
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == heightAttr) {
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr) {
        applyAlignmentAttributeToStyle(value, style);
    } else {
        HTMLFrameOwnerElement::collectStyleForPresentationAttribute(name, value, style);
    }
}

void HTMLPlugInElement::defaultEventHandler(Event* event)
{
    // Firefox seems to use a fake event listener to dispatch events to plugin
    // (tested with mouse events only). This is observable via different order
    // of events - in Firefox, event listeners specified in HTML attributes
    // fires first, then an event gets dispatched to plugin, and only then
    // other event listeners fire. Hopefully, this difference does not matter in
    // practice.

    // FIXME: Mouse down and scroll events are passed down to plugin via custom
    // code in EventHandler; these code paths should be united.

    LayoutObject* r = layoutObject();
    if (!r || !r->isLayoutPart())
        return;
    if (r->isEmbeddedObject()) {
        if (toLayoutEmbeddedObject(r)->showsUnavailablePluginIndicator())
            return;
    }
    RefPtrWillBeRawPtr<Widget> widget = toLayoutPart(r)->widget();
    if (!widget)
        return;
    widget->handleEvent(event);
    if (event->defaultHandled())
        return;
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

LayoutPart* HTMLPlugInElement::layoutPartForJSBindings() const
{
    // Needs to load the plugin immediatedly because this function is called
    // when JavaScript code accesses the plugin.
    // FIXME: Check if dispatching events here is safe.
    document().updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasksSynchronously);
    return existingLayoutPart();
}

bool HTMLPlugInElement::isKeyboardFocusable() const
{
    if (!document().isActive())
        return false;
    return pluginWidget() && pluginWidget()->isPluginView() && toPluginView(pluginWidget())->supportsKeyboardFocus();
}

bool HTMLPlugInElement::hasCustomFocusLogic() const
{
    return !useFallbackContent();
}

bool HTMLPlugInElement::isPluginElement() const
{
    return true;
}

bool HTMLPlugInElement::layoutObjectIsFocusable() const
{
    if (HTMLFrameOwnerElement::supportsFocus() && HTMLFrameOwnerElement::layoutObjectIsFocusable())
        return true;

    if (useFallbackContent() || !HTMLFrameOwnerElement::layoutObjectIsFocusable())
        return false;
    return layoutObject() && layoutObject()->isEmbeddedObject() && !toLayoutEmbeddedObject(layoutObject())->showsUnavailablePluginIndicator();
}

NPObject* HTMLPlugInElement::getNPObject()
{
    ASSERT(document().frame());
    if (!m_NPObject)
        m_NPObject = document().frame()->script().createScriptObjectForPluginElement(this);
    return m_NPObject;
}

void HTMLPlugInElement::setPluginFocus(bool focused)
{
    // NPAPI flash requires to receive messages when web contents focus changes.
    if (getNPObject() && pluginWidget() && pluginWidget()->isPluginView())
        toPluginView(pluginWidget())->setFocus(focused, WebFocusTypeNone);
}

bool HTMLPlugInElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (LocalFrame* frame = document().frame()) {
        KURL completedURL = document().completeURL(m_url);
        return frame->loader().client()->objectContentType(completedURL, m_serviceType, shouldPreferPlugInsForImages()) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

LayoutEmbeddedObject* HTMLPlugInElement::layoutEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
    // when using fallback content.
    if (!layoutObject() || !layoutObject()->isEmbeddedObject())
        return nullptr;
    return toLayoutEmbeddedObject(layoutObject());
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInElement::allowedToLoadFrameURL(const String& url)
{
    KURL completeURL = document().completeURL(url);
    if (contentFrame() && protocolIsJavaScript(completeURL)
        && !document().securityOrigin()->canAccess(contentFrame()->securityContext()->securityOrigin()))
        return false;
    return document().frame()->isURLAllowed(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInElement::wouldLoadAsNetscapePlugin(const String& url, const String& serviceType)
{
    ASSERT(document().frame());
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = document().completeURL(url);
    return document().frame()->loader().client()->objectContentType(completedURL, serviceType, shouldPreferPlugInsForImages()) == ObjectContentNetscapePlugin;
}

bool HTMLPlugInElement::requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    if (protocolIsJavaScript(url))
        return false;

    KURL completedURL = url.isEmpty() ? KURL() : document().completeURL(url);
    if (!allowedToLoadObject(completedURL, mimeType))
        return false;

    bool useFallback;
    if (!shouldUsePlugin(completedURL, mimeType, hasFallbackContent(), useFallback)) {
        // If the plugin element already contains a subframe,
        // loadOrRedirectSubframe will re-use it. Otherwise, it will create a
        // new frame and set it as the LayoutPart's widget, causing what was
        // previously in the widget to be torn down.
        return loadOrRedirectSubframe(completedURL, getNameAttribute(), true);
    }

    return loadPlugin(completedURL, mimeType, paramNames, paramValues, useFallback, true);
}

bool HTMLPlugInElement::loadPlugin(const KURL& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback, bool requireLayoutObject)
{
    if (!allowedToLoadPlugin(url, mimeType))
        return false;

    LocalFrame* frame = document().frame();
    if (!frame->loader().allowPlugins(AboutToInstantiatePlugin))
        return false;

    LayoutEmbeddedObject* layoutObject = layoutEmbeddedObject();
    // FIXME: This code should not depend on layoutObject!
    if ((!layoutObject && requireLayoutObject) || useFallback)
        return false;

    WTF_LOG(Plugins, "%p Plugin URL: %s", this, m_url.utf8().data());
    WTF_LOG(Plugins, "   Loaded URL: %s", url.string().utf8().data());
    m_loadedUrl = url;

    if (m_persistedPluginWidget) {
        setWidget(m_persistedPluginWidget.release());
    } else {
        bool loadManually = document().isPluginDocument() && !document().containsPlugins();
        FrameLoaderClient::DetachedPluginPolicy policy = requireLayoutObject ? FrameLoaderClient::FailOnDetachedPlugin : FrameLoaderClient::AllowDetachedPlugin;
        RefPtrWillBeRawPtr<Widget> widget = frame->loader().client()->createPlugin(this, url, paramNames, paramValues, mimeType, loadManually, policy);
        if (!widget) {
            if (layoutObject && !layoutObject->showsUnavailablePluginIndicator())
                layoutObject->setPluginUnavailabilityReason(LayoutEmbeddedObject::PluginMissing);
            return false;
        }

        if (layoutObject)
            setWidget(widget);
        else
            setPersistedPluginWidget(widget.get());
    }

    document().setContainsPlugins();
    // TODO(esprehn): WebPluginContainerImpl::setWebLayer also schedules a compositing update, do we need both?
    setNeedsCompositingUpdate();
    // Make sure any input event handlers introduced by the plugin are taken into account.
    if (Page* page = document().frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->notifyGeometryChanged();
    }
    return true;
}

bool HTMLPlugInElement::shouldUsePlugin(const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
    // Allow other plugins to win over QuickTime because if the user has
    // installed a plugin that can handle TIFF (which QuickTime can also
    // handle) they probably intended to override QT.
    if (document().frame()->page() && (mimeType == "image/tiff" || mimeType == "image/tif" || mimeType == "image/x-tiff")) {
        const PluginData* pluginData = document().frame()->page()->pluginData();
        String pluginName = pluginData ? pluginData->pluginNameForMimeType(mimeType) : String();
        if (!pluginName.isEmpty() && !pluginName.contains("QuickTime", TextCaseInsensitive)) {
            useFallback = false;
            return true;
        }
    }

    ObjectContentType objectType = document().frame()->loader().client()->objectContentType(url, mimeType, shouldPreferPlugInsForImages());
    // If an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon.
    useFallback = objectType == ObjectContentNone && hasFallback;
    return objectType == ObjectContentNone || objectType == ObjectContentNetscapePlugin || objectType == ObjectContentOtherPlugin;

}

void HTMLPlugInElement::dispatchErrorEvent()
{
    if (document().isPluginDocument() && document().ownerElement())
        document().ownerElement()->dispatchEvent(Event::create(EventTypeNames::error));
    else
        dispatchEvent(Event::create(EventTypeNames::error));
}

bool HTMLPlugInElement::allowedToLoadObject(const KURL& url, const String& mimeType)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    LocalFrame* frame = document().frame();
    Settings* settings = frame->settings();
    if (!settings)
        return false;

    if (MIMETypeRegistry::isJavaAppletMIMEType(mimeType))
        return false;

    if (!document().securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url.string());
        return false;
    }

    AtomicString declaredMimeType = document().isPluginDocument() && document().ownerElement() ?
        document().ownerElement()->fastGetAttribute(HTMLNames::typeAttr) :
        fastGetAttribute(HTMLNames::typeAttr);
    if (!document().contentSecurityPolicy()->allowObjectFromSource(url)
        || !document().contentSecurityPolicy()->allowPluginTypeForDocument(document(), mimeType, declaredMimeType, url)) {
        layoutEmbeddedObject()->setPluginUnavailabilityReason(LayoutEmbeddedObject::PluginBlockedByContentSecurityPolicy);
        return false;
    }
    // If the URL is empty, a plugin could still be instantiated if a MIME-type
    // is specified.
    return (!mimeType.isEmpty() && url.isEmpty()) || !MixedContentChecker::shouldBlockFetch(frame, WebURLRequest::RequestContextObject, WebURLRequest::FrameTypeNone, url);
}

bool HTMLPlugInElement::allowedToLoadPlugin(const KURL& url, const String& mimeType)
{
    if (document().isSandboxed(SandboxPlugins)) {
        document().addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel,
            "Failed to load '" + url.elidedString() + "' as a plugin, because the frame into which the plugin is loading is sandboxed."));
        return false;
    }
    return true;
}

void HTMLPlugInElement::didAddUserAgentShadowRoot(ShadowRoot&)
{
    userAgentShadowRoot()->appendChild(HTMLContentElement::create(document()));
}

void HTMLPlugInElement::willAddFirstAuthorShadowRoot()
{
    lazyReattachIfAttached();
}

bool HTMLPlugInElement::hasFallbackContent() const
{
    return false;
}

bool HTMLPlugInElement::useFallbackContent() const
{
    return openShadowRoot();
}

void HTMLPlugInElement::lazyReattachIfNeeded()
{
    if (!useFallbackContent() && needsWidgetUpdate() && layoutObject() && !isImageType())
        lazyReattachIfAttached();
}

} // namespace blink
