// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

// static
Worklet* Worklet::create(ExecutionContext* executionContext)
{
    Worklet* worklet = new Worklet(executionContext);
    worklet->suspendIfNeeded();
    return worklet;
}

Worklet::Worklet(ExecutionContext* executionContext)
    : ActiveDOMObject(executionContext)
    , m_workletGlobalScope(WorkletGlobalScope::create(executionContext->url(), executionContext->userAgent(), executionContext->securityOrigin(), toIsolate(executionContext)))
{
}

ScriptPromise Worklet::import(ScriptState* scriptState, const String& url)
{
    KURL scriptURL = executionContext()->completeURL(url);
    if (!scriptURL.isValid()) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SyntaxError, "'" + url + "' is not a valid URL."));
    }

    // TODO(ikilpatrick): Perform upfront CSP checks once we decide on a
    // CSP-policy for worklets.

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    m_resolvers.append(resolver);

    ScriptPromise promise = resolver->promise();

    // TODO(ikilpatrick): WorkerScriptLoader will need to be extended to allow
    // module loading support. For now just fetch a 'classic' script.

    // NOTE: WorkerScriptLoader may synchronously invoke its callbacks
    // (resolving the promise) before we return it.
    m_scriptLoaders.append(WorkerScriptLoader::create());
    m_scriptLoaders.last()->loadAsynchronously(*executionContext(), scriptURL, DenyCrossOriginRequests,
        bind(&Worklet::onResponse, this),
        bind(&Worklet::onFinished, this, m_scriptLoaders.last().get(), resolver));

    return promise;
}

void Worklet::onResponse()
{
    // TODO(ikilpatrick): Add devtools instrumentation on worklet script
    // resource loading.
}

void Worklet::onFinished(WorkerScriptLoader* scriptLoader, ScriptPromiseResolver* resolver)
{
    if (scriptLoader->failed()) {
        resolver->reject(DOMException::create(NetworkError));
    } else {
        // TODO(ikilpatrick): Worklets don't have the same error behaviour
        // as workers, etc. For a SyntaxError we should reject, however if
        // the script throws a normal error, resolve. For now just resolve.
        m_workletGlobalScope->scriptController()->evaluate(ScriptSourceCode(scriptLoader->script(), scriptLoader->url()));
        resolver->resolve();
    }

    size_t index = m_scriptLoaders.find(scriptLoader);

    ASSERT(index != kNotFound);
    ASSERT(m_resolvers[index] == resolver);

    m_scriptLoaders.remove(index);
    m_resolvers.remove(index);
}

void Worklet::stop()
{
    m_workletGlobalScope->scriptController()->willScheduleExecutionTermination();

    for (auto scriptLoader : m_scriptLoaders) {
        scriptLoader->cancel();
    }
}

DEFINE_TRACE(Worklet)
{
    visitor->trace(m_resolvers);
    visitor->trace(m_workletGlobalScope);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
