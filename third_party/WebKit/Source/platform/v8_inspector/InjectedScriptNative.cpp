// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/InjectedScriptNative.h"

#include "platform/inspector_protocol/Values.h"
#include "wtf/Vector.h"

namespace blink {

InjectedScriptNative::InjectedScriptNative(v8::Isolate* isolate)
    : m_lastBoundObjectId(1)
    , m_isolate(isolate)
{
}

static const char privateKeyName[] = "v8-inspector#injectedScript";

InjectedScriptNative::~InjectedScriptNative() { }

void InjectedScriptNative::setOnInjectedScriptHost(v8::Local<v8::Object> injectedScriptHost)
{
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::External> external = v8::External::New(m_isolate, this);
    v8::Local<v8::Private> privateKey = v8::Private::ForApi(m_isolate, v8::String::NewFromUtf8(m_isolate, privateKeyName, v8::NewStringType::kInternalized).ToLocalChecked());
    injectedScriptHost->SetPrivate(m_isolate->GetCurrentContext(), privateKey, external);
}

InjectedScriptNative* InjectedScriptNative::fromInjectedScriptHost(v8::Local<v8::Object> injectedScriptObject)
{
    v8::Isolate* isolate = injectedScriptObject->GetIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, v8::String::NewFromUtf8(isolate, privateKeyName, v8::NewStringType::kInternalized).ToLocalChecked());
    v8::Local<v8::Value> value = injectedScriptObject->GetPrivate(context, privateKey).ToLocalChecked();
    ASSERT(value->IsExternal());
    v8::Local<v8::External> external = value.As<v8::External>();
    return static_cast<InjectedScriptNative*>(external->Value());
}

int InjectedScriptNative::bind(v8::Local<v8::Value> value, const String& groupName)
{
    if (m_lastBoundObjectId <= 0)
        m_lastBoundObjectId = 1;
    int id = m_lastBoundObjectId++;
    m_idToWrappedObject.set(id, adoptPtr(new v8::Global<v8::Value>(m_isolate, value)));
    addObjectToGroup(id, groupName);
    return id;
}

void InjectedScriptNative::unbind(int id)
{
    m_idToWrappedObject.remove(id);
    m_idToObjectGroupName.remove(id);
}

v8::Local<v8::Value> InjectedScriptNative::objectForId(int id)
{
    return m_idToWrappedObject.contains(id) ? m_idToWrappedObject.get(id)->Get(m_isolate) : v8::Local<v8::Value>();
}

void InjectedScriptNative::addObjectToGroup(int objectId, const String& groupName)
{
    if (groupName.isEmpty())
        return;
    if (objectId <= 0)
        return;
    m_idToObjectGroupName.set(objectId, groupName);
    NameToObjectGroup::iterator groupIt = m_nameToObjectGroup.find(groupName);
    if (groupIt == m_nameToObjectGroup.end())
        m_nameToObjectGroup.set(groupName, Vector<int>()).storedValue->value.append(objectId);
    else
        groupIt->value.append(objectId);
}

void InjectedScriptNative::releaseObjectGroup(const String& groupName)
{
    if (groupName.isEmpty())
        return;
    NameToObjectGroup::iterator groupIt = m_nameToObjectGroup.find(groupName);
    if (groupIt == m_nameToObjectGroup.end())
        return;
    for (int id : groupIt->value)
        unbind(id);
    m_nameToObjectGroup.remove(groupIt);
}

String InjectedScriptNative::groupName(int objectId) const
{
    if (objectId <= 0)
        return String();
    return m_idToObjectGroupName.get(objectId);
}

} // namespace blink

