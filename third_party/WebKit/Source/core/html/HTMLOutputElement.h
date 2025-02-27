/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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

#ifndef HTMLOutputElement_h
#define HTMLOutputElement_h

#include "core/dom/DOMTokenList.h"
#include "core/html/HTMLFormControlElement.h"

namespace blink {

class CORE_EXPORT HTMLOutputElement final : public HTMLFormControlElement, private DOMTokenListObserver {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLOutputElement);
public:
    static PassRefPtrWillBeRawPtr<HTMLOutputElement> create(Document&, HTMLFormElement*);
    ~HTMLOutputElement() override;

    bool willValidate() const override { return false; }

    String value() const;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    void setFor(const AtomicString&);
    DOMTokenList* htmlFor() const;

    bool canContainRangeEndPoint() const override { return false; }

    DECLARE_VIRTUAL_TRACE();

private:
    HTMLOutputElement(Document&, HTMLFormElement*);

    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    const AtomicString& formControlType() const override;
    bool isEnumeratable() const override { return true; }
    bool supportLabels() const override { return true; }
    bool supportsFocus() const override;
    void childrenChanged(const ChildrenChange&) override;
    void resetImpl() override;

    void valueWasSet() final;

    bool m_isDefaultValueMode;
    String m_defaultValue;
    RefPtrWillBeMember<DOMTokenList> m_tokens;
};

} // namespace blink

#endif // HTMLOutputElement_h
