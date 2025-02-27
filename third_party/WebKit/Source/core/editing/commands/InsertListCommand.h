/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef InsertListCommand_h
#define InsertListCommand_h

#include "core/editing/commands/CompositeEditCommand.h"

namespace blink {

class HTMLElement;
class HTMLUListElement;

class InsertListCommand final : public CompositeEditCommand {
public:
    enum Type { OrderedList, UnorderedList };

    static PassRefPtrWillBeRawPtr<InsertListCommand> create(Document& document, Type listType)
    {
        return adoptRefWillBeNoop(new InsertListCommand(document, listType));
    }

    bool preservesTypingStyle() const override { return true; }

    DECLARE_VIRTUAL_TRACE();

private:
    InsertListCommand(Document&, Type);

    void doApply(EditingState*) override;
    EditAction editingAction() const override { return EditActionInsertList; }

    HTMLUListElement* fixOrphanedListChild(Node*, EditingState*);
    bool selectionHasListOfType(const VisibleSelection&, const HTMLQualifiedName&);
    PassRefPtrWillBeRawPtr<HTMLElement> mergeWithNeighboringLists(PassRefPtrWillBeRawPtr<HTMLElement>, EditingState*);
    bool doApplyForSingleParagraph(bool forceCreateList, const HTMLQualifiedName&, Range& currentSelection, EditingState*);
    void unlistifyParagraph(const VisiblePosition& originalStart, HTMLElement* listNode, Node* listChildNode, EditingState*);
    void listifyParagraph(const VisiblePosition& originalStart, const HTMLQualifiedName& listTag, EditingState*);
    void moveParagraphOverPositionIntoEmptyListItem(const VisiblePosition&, PassRefPtrWillBeRawPtr<HTMLLIElement>, EditingState*);

    Type m_type;
};

} // namespace blink

#endif // InsertListCommand_h
