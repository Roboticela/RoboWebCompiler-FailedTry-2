#include "RoboRenderer/Input.h"

#include "RoboRenderer/Text.h"

#include <algorithm>
#include <cmath>

namespace RoboRenderer
{
    namespace
    {
        bool IsTextControlCategory(RenderCategory category)
        {
            return category == RenderCategory::Input || category == RenderCategory::TextArea ||
                   category == RenderCategory::FileInput;
        }

        bool IsInteractiveCategory(RenderCategory category)
        {
            switch (category)
            {
            case RenderCategory::Button:
            case RenderCategory::Input:
            case RenderCategory::TextArea:
            case RenderCategory::Select:
            case RenderCategory::Checkbox:
            case RenderCategory::Radio:
            case RenderCategory::Range:
            case RenderCategory::ColorInput:
            case RenderCategory::FileInput:
            case RenderCategory::Link:
            case RenderCategory::DisclosureSummary:
            case RenderCategory::Progress:
            case RenderCategory::Meter:
                return true;
            default:
                return false;
            }
        }

        float ClampComponent(float value, float minimum, float maximum)
        {
            return value < minimum ? minimum : (value > maximum ? maximum : value);
        }

        void MergeResult(EventResult& into, const EventResult& from)
        {
            into.layoutChanged = into.layoutChanged || from.layoutChanged;
            into.paintChanged = into.paintChanged || from.paintChanged;
            into.scrolled = into.scrolled || from.scrolled;
            into.submitted = into.submitted || from.submitted;
            into.reset = into.reset || from.reset;
            into.activated = into.activated || from.activated;

            if (from.focusChanged != NoElement)
            {
                into.focusChanged = from.focusChanged;
            }

            if (from.activatedHref != 0)
            {
                into.activatedHref = from.activatedHref;
            }

            into.handled = into.handled || from.handled;

            if (into.target == NoElement)
            {
                into.target = from.target;
            }
        }
    }

    const char* ToString(EventType type)
    {
        switch (type)
        {
        case EventType::None: return "None";
        case EventType::PointerMove: return "PointerMove";
        case EventType::PointerDown: return "PointerDown";
        case EventType::PointerUp: return "PointerUp";
        case EventType::Click: return "Click";
        case EventType::DoubleClick: return "DoubleClick";
        case EventType::Wheel: return "Wheel";
        case EventType::KeyDown: return "KeyDown";
        case EventType::KeyUp: return "KeyUp";
        case EventType::TextInput: return "TextInput";
        case EventType::Focus: return "Focus";
        case EventType::Blur: return "Blur";
        case EventType::Change: return "Change";
        case EventType::Submit: return "Submit";
        case EventType::ResetEvent: return "Reset";
        }

        return "Unknown";
    }

    const char* ToString(Key key)
    {
        switch (key)
        {
        case Key::None: return "None";
        case Key::Enter: return "Enter";
        case Key::Escape: return "Escape";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Delete: return "Delete";
        case Key::Left: return "Left";
        case Key::Right: return "Right";
        case Key::Up: return "Up";
        case Key::Down: return "Down";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::PageUp: return "PageUp";
        case Key::PageDown: return "PageDown";
        case Key::Space: return "Space";
        }

        return "Unknown";
    }

    // -----------------------------------------------------------------------
    // HitTester
    // -----------------------------------------------------------------------

    HitTester::HitTester(const CompiledDocument& compiledDocument,
                         const LayoutEngine& layoutEngine,
                         const UiState& uiState)
        : document(compiledDocument)
        , layout(layoutEngine)
        , state(uiState)
    {
    }

    void HitTester::ScrollOf(ElementIndex index, float& offsetX, float& offsetY) const
    {
        offsetX = 0.0f;
        offsetY = 0.0f;

        const ElementLayout* box = layout.Result().Element(index);
        if (box == nullptr)
        {
            return;
        }

        if ((box->flags & LayoutFlagScrolls) != 0)
        {
            const ElementState& elementState = state.Element(index);
            const float maxX = std::max(0.0f, box->scrollWidth - box->content.width);
            const float maxY = std::max(0.0f, box->scrollHeight - box->content.height);

            offsetX = ClampComponent(elementState.scrollX, 0.0f, maxX);
            offsetY = ClampComponent(elementState.scrollY, 0.0f, maxY);
        }

        // The page itself scrolls with the document offset, which is the same
        // translation painting applies to the root's children.
        if (index == document.root)
        {
            offsetX += state.ScrollX();
            offsetY += state.ScrollY();
        }
    }

    void HitTester::OriginOf(ElementIndex index, float& offsetX, float& offsetY) const
    {
        offsetX = 0.0f;
        offsetY = 0.0f;

        if (index == document.root || index == NoElement)
        {
            return;
        }

        // A fixed box is positioned against the viewport: its geometry is
        // already in viewport coordinates, so no ancestor's scroll applies to
        // it. That is the same rule the paint traversal follows, which keeps
        // bounds, hit testing and what is drawn in agreement.
        if (layout.StyleOf(index).position == PositionKind::Fixed)
        {
            return;
        }

        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || element->parent == NoElement)
        {
            return;
        }

        float parentX = 0.0f;
        float parentY = 0.0f;
        OriginOf(element->parent, parentX, parentY);

        float scrollX = 0.0f;
        float scrollY = 0.0f;
        ScrollOf(element->parent, scrollX, scrollY);

        offsetX = parentX - scrollX;
        offsetY = parentY - scrollY;
    }

    bool HitTester::ElementBounds(ElementIndex index, Rect& out) const
    {
        const ElementLayout* box = layout.Result().Element(index);
        if (box == nullptr)
        {
            return false;
        }

        float originX = 0.0f;
        float originY = 0.0f;
        OriginOf(index, originX, originY);

        out = box->border.Translated(originX, originY);
        return true;
    }

    bool HitTester::LocalPosition(ElementIndex index, float x, float y, float& localX, float& localY) const
    {
        Rect bounds;
        if (!ElementBounds(index, bounds))
        {
            return false;
        }

        localX = x - bounds.x;
        localY = y - bounds.y;
        return true;
    }

    ElementIndex HitTester::HitTestRecursive(ElementIndex index,
                                             float pointX,
                                             float pointY,
                                             float originX,
                                             float originY,
                                             const Rect& clip) const
    {
        const ElementLayout* box = layout.Result().Element(index);
        const CompiledElement* element = ElementAt(document, index);

        if (box == nullptr || element == nullptr || (box->flags & LayoutFlagDisplayNone) != 0)
        {
            return NoElement;
        }

        // The incoming clip is what an ancestor's overflow left visible. An
        // element's own overflow clips its content, never its own border, so the
        // element itself is tested against the incoming clip only.
        if (!clip.Contains(pointX, pointY))
        {
            return NoElement;
        }

        float scrollX = 0.0f;
        float scrollY = 0.0f;
        ScrollOf(index, scrollX, scrollY);

        const float childOriginX = originX - scrollX;
        const float childOriginY = originY - scrollY;

        Rect childClip = clip;

        if ((box->flags & (LayoutFlagClips | LayoutFlagScrolls)) != 0)
        {
            childClip = box->clip.Translated(originX, originY).Intersection(clip);
        }

        // Children are tested in tree order and the last match wins, which is the
        // same rule painting uses: whatever was painted last is on top.
        ElementIndex best = NoElement;

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* childElement = ElementAt(document, child);

            // A fixed box is positioned against the viewport, so it is hit
            // tested at its document coordinates with no ancestor scroll or clip
            // applied: the same rule painting uses, so what is seen on top is
            // what is clicked.
            const bool fixed = childElement != nullptr &&
                               layout.StyleOf(child).position == PositionKind::Fixed;

            const RenderTargetInfo& target = layout.Viewport();

            const ElementIndex hit =
                fixed ? HitTestRecursive(child, pointX, pointY, 0.0f, 0.0f,
                                         Rect{ 0.0f, 0.0f, target.logicalWidth, target.logicalHeight })
                      : HitTestRecursive(child, pointX, pointY, childOriginX, childOriginY, childClip);

            if (hit != NoElement)
            {
                best = hit;
            }

            child = childElement != nullptr ? childElement->nextSibling : NoElement;
        }

        if (best != NoElement)
        {
            return best;
        }

        return box->border.Translated(originX, originY).Contains(pointX, pointY) ? index : NoElement;
    }

    ElementIndex HitTester::HitTest(float x, float y) const
    {
        const RenderTargetInfo& viewport = layout.Viewport();
        const Rect window{ 0.0f, 0.0f, viewport.logicalWidth, viewport.logicalHeight };

        if (!window.Contains(x, y))
        {
            return NoElement;
        }

        float originX = 0.0f;
        float originY = 0.0f;
        ScrollOf(document.root, originX, originY);

        return HitTestRecursive(document.root, x, y, -originX, -originY, window);
    }

    ElementIndex HitTester::HitTestInteractive(float x, float y) const
    {
        for (ElementIndex index = HitTest(x, y); index != NoElement;)
        {
            const CompiledElement* element = ElementAt(document, index);
            if (element == nullptr)
            {
                break;
            }

            if (element->HasFlag(ElementFlagInteractive) || IsInteractiveCategory(CategoryFor(element->kind)))
            {
                return index;
            }

            index = element->parent;
        }

        // Nothing interactive is under the point. Returning the raw hit would
        // report the document frame as hovered or clicked, which it never is.
        return NoElement;
    }

    ElementIndex HitTester::ScrollableAt(float x, float y) const
    {
        for (ElementIndex index = HitTest(x, y); index != NoElement;)
        {
            const CompiledElement* element = ElementAt(document, index);
            if (element == nullptr)
            {
                break;
            }

            const ElementLayout* box = layout.Result().Element(index);
            if (box != nullptr && (box->flags & LayoutFlagScrolls) != 0)
            {
                return index;
            }

            index = element->parent;
        }

        return NoElement;
    }

    bool HitTester::IsVisibleAt(ElementIndex index, float x, float y) const
    {
        const ElementLayout* box = layout.Result().Element(index);
        return box != nullptr && box->clip.Contains(x, y);
    }

    // -----------------------------------------------------------------------
    // InputRouter
    // -----------------------------------------------------------------------

    InputRouter::InputRouter(const CompiledDocument& compiledDocument,
                             LayoutEngine& layoutEngine,
                             UiState& uiState,
                             HitTester& hitTesterRef)
        : document(compiledDocument)
        , layout(layoutEngine)
        , state(uiState)
        , hitTester(hitTesterRef)
    {
    }

    bool InputRouter::IsFocusable(ElementIndex index) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return false;
        }

        if (element->HasFlag(ElementFlagDisabled))
        {
            return false;
        }

        if (element->HasFlag(ElementFlagFocusable))
        {
            return true;
        }

        return IsInteractiveCategory(CategoryFor(element->kind));
    }

    bool InputRouter::IsTextControl(ElementIndex index) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || element->HasFlag(ElementFlagReadOnly) || element->HasFlag(ElementFlagDisabled))
        {
            return false;
        }

        // The state table carries a field for every typed control, including a
        // checkbox, so the category - not the presence of state - decides
        // whether the keyboard edits text or activates the control.
        if (!IsTextControlCategory(CategoryFor(element->kind)))
        {
            return false;
        }

        return state.Input(index) != nullptr;
    }

    ElementIndex InputRouter::NextFocusable(ElementIndex from, bool backwards) const
    {
        const std::uint32_t count = document.elementCount;
        if (count == 0)
        {
            return NoElement;
        }

        ElementIndex index = from == NoElement ? document.root : from;

        // A single sweep of document order, wrapping once, is what a tab order
        // is: the compiled tree already lists elements in source order.
        for (std::uint32_t step = 0; step < count + 1; ++step)
        {
            index = backwards ? PreviousInDocumentOrder(document, index)
                              : (step == 0 && from != NoElement ? NextInDocumentOrder(document, from)
                                                                : NextInDocumentOrder(document, index));

            if (index == NoElement)
            {
                index = backwards ? LastInDocumentOrder(document) : document.root;
                continue;
            }

            if (index != from && IsFocusable(index))
            {
                return index;
            }
        }

        return NoElement;
    }

    EventResult InputRouter::SetFocus(ElementIndex index, bool focusVisible)
    {
        EventResult result;

        if (index != NoElement && !IsFocusable(index))
        {
            return result;
        }

        if (state.Focused() == index)
        {
            return result;
        }

        const ElementIndex previous = state.Focused();
        state.SetFocused(index, focusVisible);

        result.focusChanged = index;
        result.paintChanged = true;
        result.handled = true;

        // A control that gains focus scrolls itself into view, which is why a
        // tab through a long page keeps the focused field on screen.
        if (index != NoElement)
        {
            Rect bounds;
            if (hitTester.ElementBounds(index, bounds))
            {
                const RenderTargetInfo& viewport = layout.Viewport();
                const float bottom = bounds.Bottom();
                float scrollY = state.ScrollY();

                if (bounds.y - scrollY < 0.0f)
                {
                    state.SetScroll(state.ScrollX(), std::max(0.0f, bounds.y - 8.0f));
                    result.scrolled = true;
                }
                else if (bottom - scrollY > viewport.logicalHeight)
                {
                    state.SetScroll(state.ScrollX(),
                                    std::min(layout.MaxScrollY(),
                                             bottom - viewport.logicalHeight + 8.0f));
                    result.scrolled = true;
                }
            }
        }

        (void)previous;
        return result;
    }

    void InputRouter::SetChecked(ElementIndex index, bool checked, EventResult& result)
    {
        ElementState& elementState = state.Element(index);

        const bool current = elementState.HasFlag(RuntimeFlagChecked);
        if (current == checked)
        {
            return;
        }

        if (checked)
        {
            elementState.SetFlag(RuntimeFlagChecked);
        }
        else
        {
            elementState.ClearFlag(RuntimeFlagChecked);
        }

        elementState.SetFlag(RuntimeFlagPaintDirty);
        elementState.SetFlag(RuntimeFlagValueChanged);
        result.paintChanged = true;
        result.handled = true;
    }

    void InputRouter::SelectRadioSiblings(ElementIndex index, EventResult& result)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return;
        }

        // A radio group is the set of radios that share a parent and, when the
        // document gives names, the same name. Both are compiled values, so the
        // runtime never reads an attribute to decide.
        const CompiledElement* parent = ElementAt(document, element->parent);

        for (ElementIndex sibling = parent != nullptr ? parent->firstChild : NoElement; sibling != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, sibling);
            if (node == nullptr)
            {
                break;
            }

            if (sibling != index && CategoryFor(node->kind) == RenderCategory::Radio &&
                (element->name == 0 || node->name == element->name))
            {
                SetChecked(sibling, false, result);
            }

            sibling = node->nextSibling;
        }
    }

    void InputRouter::CycleSelectOption(ElementIndex index, EventResult& result)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return;
        }

        ElementIndex current = FindSelectedOption(document, state, index);
        ElementIndex next = NoElement;
        ElementIndex first = NoElement;

        // The HTML-only phase models a select without a popup layer: activating
        // it advances to the next option, which keeps the control usable and
        // keyboard driven until a popup layer exists.
        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            if (CategoryFor(node->kind) == RenderCategory::Option)
            {
                if (first == NoElement)
                {
                    first = child;
                }

                if (current != NoElement && child != current && next == NoElement)
                {
                    const CompiledElement* currentElement = ElementAt(document, current);
                    if (currentElement != nullptr && child > current)
                    {
                        next = child;
                    }
                }
            }

            child = node->nextSibling;
        }

        // A select with nothing selected starts at the first option; one at the
        // end wraps to the beginning.
        const ElementIndex target = current == NoElement ? first : (next != NoElement ? next : first);

        if (target == NoElement || target == current)
        {
            return;
        }

        for (ElementIndex option = element->firstChild; option != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, option);
            if (node == nullptr)
            {
                break;
            }

            if (CategoryFor(node->kind) == RenderCategory::Option)
            {
                ElementState& optionState = state.Element(option);
                if (option == target)
                {
                    optionState.SetFlag(RuntimeFlagSelected);
                }
                else
                {
                    optionState.ClearFlag(RuntimeFlagSelected);
                }

                optionState.SetFlag(RuntimeFlagPaintDirty);
            }

            option = node->nextSibling;
        }

        state.Element(index).SetFlag(RuntimeFlagValueChanged);
        state.Element(index).SetFlag(RuntimeFlagPaintDirty);

        result.paintChanged = true;
        result.handled = true;
        result.activated = true;
    }

    void InputRouter::ToggleDisclosure(ElementIndex index, EventResult& result)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return;
        }

        // A summary toggles the disclosure it belongs to.
        ElementIndex target = index;

        if (CategoryFor(element->kind) == RenderCategory::DisclosureSummary && element->parent != NoElement)
        {
            target = element->parent;
        }

        const CompiledElement* targetElement = ElementAt(document, target);
        if (targetElement == nullptr)
        {
            return;
        }

        ElementState& elementState = state.Element(target);
        const bool open = elementState.HasFlag(RuntimeFlagOpen);

        if (open)
        {
            elementState.ClearFlag(RuntimeFlagOpen);
        }
        else
        {
            elementState.SetFlag(RuntimeFlagOpen);
        }

        elementState.SetFlag(RuntimeFlagLayoutDirty);
        elementState.SetFlag(RuntimeFlagPaintDirty);

        layout.Invalidate(target);

        result.layoutChanged = true;
        result.paintChanged = true;
        result.handled = true;
        result.activated = true;
    }

    bool InputRouter::UpdateRangeFromPointer(ElementIndex index, float x, EventResult& result)
    {
        const CompiledElement* element = ElementAt(document, index);
        const ElementLayout* box = layout.Result().Element(index);

        if (element == nullptr || box == nullptr)
        {
            return false;
        }

        Rect border;
        if (!hitTester.ElementBounds(index, border))
        {
            return false;
        }

        const float contentLeft = border.x + (box->content.x - box->border.x);
        const float contentWidth = std::max(1.0f, box->content.width);

        const float fraction = ClampComponent((x - contentLeft) / contentWidth, 0.0f, 1.0f);
        const float value = element->minimum + (element->maximum - element->minimum) * fraction;

        state.SetNumericValue(index, value);

        result.paintChanged = true;
        result.handled = true;
        return true;
    }

    EventResult InputRouter::Activate(ElementIndex index)
    {
        EventResult result;
        result.target = index;

        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || element->HasFlag(ElementFlagDisabled))
        {
            return result;
        }

        const RenderCategory category = CategoryFor(element->kind);

        switch (category)
        {
        case RenderCategory::Button:
        {
            MergeResult(result, SetFocus(index, false));

            switch (element->buttonBehavior)
            {
            case ButtonBehavior::Submit: result.submitted = true; break;
            case ButtonBehavior::Reset: result.reset = true; break;
            case ButtonBehavior::None: break;
            }

            result.activated = true;
            result.handled = true;
            break;
        }

        case RenderCategory::Checkbox:
        {
            MergeResult(result, SetFocus(index, false));

            const bool checked = state.Element(index).HasFlag(RuntimeFlagChecked);
            SetChecked(index, !checked, result);
            result.activated = true;
            break;
        }

        case RenderCategory::Radio:
        {
            MergeResult(result, SetFocus(index, false));

            SetChecked(index, true, result);
            SelectRadioSiblings(index, result);
            result.activated = true;
            break;
        }

        case RenderCategory::Select:
        {
            MergeResult(result, SetFocus(index, false));
            CycleSelectOption(index, result);
            break;
        }

        case RenderCategory::DisclosureSummary:
        {
            MergeResult(result, SetFocus(index, false));
            ToggleDisclosure(index, result);
            break;
        }

        case RenderCategory::Link:
        {
            result.activatedHref = element->href;
            result.activated = true;
            result.handled = true;
            break;
        }

        case RenderCategory::Input:
        case RenderCategory::TextArea:
        {
            MergeResult(result, SetFocus(index, false));

            if (InputFieldState* field = state.Input(index))
            {
                field->caret = TextLayout::CodepointCount(field->value.c_str());
                field->selectionAnchor = field->caret;
                state.Element(index).SetFlag(RuntimeFlagPaintDirty);
                result.paintChanged = true;
            }

            result.handled = true;
            break;
        }

        case RenderCategory::FileInput:
        case RenderCategory::ColorInput:
        case RenderCategory::Range:
        case RenderCategory::Progress:
        case RenderCategory::Meter:
        {
            MergeResult(result, SetFocus(index, false));
            result.handled = true;
            break;
        }

        default:
            break;
        }

        return result;
    }

    bool InputRouter::InsertText(ElementIndex index, const std::string& text)
    {
        InputFieldState* field = state.Input(index);
        if (field == nullptr || text.empty())
        {
            return false;
        }

        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || element->HasFlag(ElementFlagReadOnly) || element->HasFlag(ElementFlagDisabled))
        {
            return false;
        }

        std::string& value = field->value;

        if (field->HasSelection())
        {
            const std::uint32_t start = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->SelectionStart());
            const std::uint32_t end = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->SelectionEnd());
            value.erase(start, end - start);
            field->caret = field->SelectionStart();
            field->selectionAnchor = field->caret;
        }

        const std::uint32_t byteOffset = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->caret);
        value.insert(byteOffset, text);
        field->caret += TextLayout::CodepointCount(text.c_str());
        field->selectionAnchor = field->caret;

        state.Element(index).SetFlag(RuntimeFlagValueChanged);
        state.Element(index).SetFlag(RuntimeFlagPaintDirty);
        layout.Invalidate(index);
        return true;
    }

    bool InputRouter::DeleteBackward(ElementIndex index)
    {
        InputFieldState* field = state.Input(index);
        if (field == nullptr)
        {
            return false;
        }

        std::string& value = field->value;

        if (field->HasSelection())
        {
            const std::uint32_t start = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->SelectionStart());
            const std::uint32_t end = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->SelectionEnd());
            value.erase(start, end - start);
            field->caret = field->SelectionStart();
            field->selectionAnchor = field->caret;
        }
        else
        {
            if (field->caret == 0)
            {
                return false;
            }

            const std::uint32_t caretByte = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->caret);
            const std::uint32_t previousByte =
                TextLayout::ByteOffsetForCodepoint(value.c_str(), field->caret - 1);
            value.erase(previousByte, caretByte - previousByte);
            field->caret = field->caret - 1;
            field->selectionAnchor = field->caret;
        }

        state.Element(index).SetFlag(RuntimeFlagValueChanged);
        state.Element(index).SetFlag(RuntimeFlagPaintDirty);
        layout.Invalidate(index);
        return true;
    }

    bool InputRouter::DeleteForward(ElementIndex index)
    {
        InputFieldState* field = state.Input(index);
        if (field == nullptr)
        {
            return false;
        }

        std::string& value = field->value;

        if (field->HasSelection())
        {
            return DeleteBackward(index);
        }

        const std::uint32_t count = TextLayout::CodepointCount(value.c_str());
        if (field->caret >= count)
        {
            return false;
        }

        const std::uint32_t caretByte = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->caret);
        const std::uint32_t nextByte = TextLayout::ByteOffsetForCodepoint(value.c_str(), field->caret + 1);
        value.erase(caretByte, nextByte - caretByte);

        state.Element(index).SetFlag(RuntimeFlagValueChanged);
        state.Element(index).SetFlag(RuntimeFlagPaintDirty);
        layout.Invalidate(index);
        return true;
    }

    bool InputRouter::SelectAll(ElementIndex index)
    {
        InputFieldState* field = state.Input(index);
        if (field == nullptr)
        {
            return false;
        }

        field->selectionAnchor = 0;
        field->caret = TextLayout::CodepointCount(field->value.c_str());
        state.Element(index).SetFlag(RuntimeFlagPaintDirty);
        return true;
    }

    bool InputRouter::MoveCaret(ElementIndex index, std::int32_t delta, bool select)
    {
        InputFieldState* field = state.Input(index);
        if (field == nullptr)
        {
            return false;
        }

        const std::int32_t count = static_cast<std::int32_t>(TextLayout::CodepointCount(field->value.c_str()));
        std::int32_t caret = static_cast<std::int32_t>(field->caret) + delta;
        caret = caret < 0 ? 0 : (caret > count ? count : caret);

        field->caret = static_cast<std::uint32_t>(caret);

        if (!select)
        {
            field->selectionAnchor = field->caret;
        }

        state.Element(index).SetFlag(RuntimeFlagPaintDirty);
        return true;
    }

    EventResult InputRouter::Dispatch(const InputEvent& event)
    {
        EventResult result;

        switch (event.type)
        {
        case EventType::PointerMove:
        {
            const ElementIndex hit = hitTester.HitTestInteractive(event.x, event.y);
            UpdateHover(hit, result);

            // Dragging a range keeps updating it while the button is held.
            if (state.Pressed() != NoElement)
            {
                const CompiledElement* pressed = ElementAt(document, state.Pressed());
                if (pressed != nullptr && CategoryFor(pressed->kind) == RenderCategory::Range)
                {
                    UpdateRangeFromPointer(state.Pressed(), event.x, result);
                }
            }

            break;
        }

        case EventType::PointerDown:
        {
            const ElementIndex hit = hitTester.HitTestInteractive(event.x, event.y);
            result.target = hit;

            if (hit == NoElement)
            {
                MergeResult(result, SetFocus(NoElement, false));
                state.SetPressed(NoElement);
                break;
            }

            const CompiledElement* element = ElementAt(document, hit);
            const RenderCategory category = element != nullptr ? CategoryFor(element->kind) : RenderCategory::None;

            if (IsFocusable(hit))
            {
                MergeResult(result, SetFocus(hit, false));
            }
            else if (!IsTextControlCategory(category) && state.Focused() != NoElement)
            {
                MergeResult(result, SetFocus(NoElement, false));
            }

            state.SetPressed(hit);
            result.paintChanged = true;
            result.handled = true;

            if (category == RenderCategory::Range)
            {
                UpdateRangeFromPointer(hit, event.x, result);
            }

            break;
        }

        case EventType::PointerUp:
        {
            const ElementIndex hit = hitTester.HitTestInteractive(event.x, event.y);
            const ElementIndex pressed = state.Pressed();

            result.target = hit;
            state.SetPressed(NoElement);
            result.paintChanged = true;

            // A click is a press and a release on the same element, which is what
            // makes dragging off a button cancel it.
            if (pressed != NoElement && pressed == hit)
            {
                MergeResult(result, Activate(hit));
            }

            break;
        }

        case EventType::Click:
        case EventType::DoubleClick:
        {
            const ElementIndex hit = hitTester.HitTestInteractive(event.x, event.y);
            result.target = hit;

            if (hit == NoElement)
            {
                break;
            }

            if (event.type == EventType::DoubleClick && IsTextControl(hit))
            {
                SelectAll(hit);
                result.handled = true;
                result.paintChanged = true;
                break;
            }

            MergeResult(result, Activate(hit));
            break;
        }

        case EventType::Wheel:
        {
            const ElementIndex scrollTarget = hitTester.ScrollableAt(event.x, event.y);

            if (scrollTarget != NoElement && ScrollBy(scrollTarget, event.deltaX, event.deltaY, result))
            {
                break;
            }

            // The innermost scroller is exhausted (or there is none), so the page
            // scrolls instead. A positive wheel delta scrolls the content up, so
            // the document offset grows.
            const float maxScrollY = layout.MaxScrollY();
            if (maxScrollY > 0.0f)
            {
                const float next = ClampComponent(state.ScrollY() + event.deltaY, 0.0f, maxScrollY);
                if (next != state.ScrollY())
                {
                    state.SetScroll(state.ScrollX(), next);
                    result.scrolled = true;
                    result.handled = true;
                    result.paintChanged = true;
                }
            }

            break;
        }

        case EventType::KeyDown:
        {
            const ElementIndex focused = state.Focused();
            result.target = focused;

            // Tab moves focus whether or not a control has it.
            if (event.key == Key::Tab)
            {
                const ElementIndex next = NextFocusable(focused, event.HasModifier(ModifierShift));
                if (next != NoElement)
                {
                    MergeResult(result, SetFocus(next, true));
                }
                else if (focused != NoElement)
                {
                    MergeResult(result, SetFocus(NoElement, false));
                }

                result.handled = true;
                break;
            }

            if (focused == NoElement)
            {
                break;
            }

            if (event.key == Key::Escape)
            {
                MergeResult(result, SetFocus(NoElement, false));
                result.handled = true;
                break;
            }

            if (IsTextControl(focused))
            {
                if (event.HasModifier(ModifierControl))
                {
                    if (event.key == Key::Left)
                    {
                        result.handled = MoveCaret(focused, -1, true);
                        result.paintChanged = result.paintChanged || result.handled;
                        break;
                    }

                    if (event.key == Key::Right)
                    {
                        result.handled = MoveCaret(focused, 1, true);
                        result.paintChanged = result.paintChanged || result.handled;
                        break;
                    }
                }

                switch (event.key)
                {
                case Key::Backspace:
                    result.handled = DeleteBackward(focused);
                    result.paintChanged = result.paintChanged || result.handled;
                    result.layoutChanged = result.layoutChanged || result.handled;
                    break;

                case Key::Delete:
                    result.handled = DeleteForward(focused);
                    result.paintChanged = result.paintChanged || result.handled;
                    result.layoutChanged = result.layoutChanged || result.handled;
                    break;

                case Key::Left:
                    result.handled = MoveCaret(focused, -1, event.HasModifier(ModifierShift));
                    result.paintChanged = result.paintChanged || result.handled;
                    break;

                case Key::Right:
                    result.handled = MoveCaret(focused, 1, event.HasModifier(ModifierShift));
                    result.paintChanged = result.paintChanged || result.handled;
                    break;

                case Key::Home:
                    result.handled = MoveCaret(focused, -static_cast<std::int32_t>(
                                                           TextLayout::CodepointCount(
                                                               state.Input(focused)->value.c_str())),
                                               event.HasModifier(ModifierShift));
                    result.paintChanged = result.paintChanged || result.handled;
                    break;

                case Key::End:
                    {
                        const std::int32_t count = static_cast<std::int32_t>(
                            TextLayout::CodepointCount(state.Input(focused)->value.c_str()));
                        result.handled = MoveCaret(focused, count, event.HasModifier(ModifierShift));
                        result.paintChanged = result.paintChanged || result.handled;
                    }
                    break;

                case Key::Enter:
                {
                    const CompiledElement* element = ElementAt(document, focused);
                    const bool multiline = element != nullptr &&
                                           CategoryFor(element->kind) == RenderCategory::TextArea;

                    if (multiline)
                    {
                        InsertText(focused, "\n");
                        result.handled = true;
                        result.paintChanged = true;
                        result.layoutChanged = true;
                    }
                    else
                    {
                        result.submitted = true;
                        result.handled = true;
                    }
                    break;
                }

                default:
                    break;
                }

                break;
            }

            // A focused control that is not a text field responds to Enter,
            // Space and, for a range, the arrow keys.
            if (event.key == Key::Enter || event.key == Key::Space)
            {
                MergeResult(result, Activate(focused));
                break;
            }

            const CompiledElement* element = ElementAt(document, focused);
            if (element != nullptr && CategoryFor(element->kind) == RenderCategory::Range &&
                (event.key == Key::Left || event.key == Key::Right || event.key == Key::Up ||
                 event.key == Key::Down))
            {
                const float step = event.HasModifier(ModifierShift) ? 10.0f : 1.0f;
                const float direction = (event.key == Key::Left || event.key == Key::Down) ? -1.0f : 1.0f;
                const float current = state.HasNumericValue(focused) ? state.NumericValue(focused)
                                                                     : element->numericValue;
                const float next = ClampComponent(current + direction * step, element->minimum, element->maximum);

                state.SetNumericValue(focused, next);

                result.handled = true;
                result.paintChanged = true;
            }

            break;
        }

        case EventType::TextInput:
        {
            const ElementIndex focused = state.Focused();
            result.target = focused;

            if (focused != NoElement && IsTextControl(focused) &&
                InsertText(focused, event.text))
            {
                result.handled = true;
                result.paintChanged = true;
                result.layoutChanged = true;
            }

            break;
        }

        case EventType::Focus:
        {
            MergeResult(result, SetFocus(state.Focused(), true));
            break;
        }

        case EventType::Blur:
        {
            MergeResult(result, SetFocus(NoElement, false));
            break;
        }

        default:
            break;
        }

        return result;
    }

    void InputRouter::UpdateHover(ElementIndex index, EventResult& result)
    {
        if (state.Hovered() == index)
        {
            return;
        }

        state.SetHovered(index);
        result.target = index;
        result.paintChanged = true;
        result.handled = index != NoElement;
    }

    bool InputRouter::ScrollBy(ElementIndex index, float deltaX, float deltaY, EventResult& result)
    {
        const ElementLayout* box = layout.Result().Element(index);
        if (box == nullptr || (box->flags & LayoutFlagScrolls) == 0)
        {
            return false;
        }

        ElementState& elementState = state.Element(index);

        const float maxX = std::max(0.0f, box->scrollWidth - box->content.width);
        const float maxY = std::max(0.0f, box->scrollHeight - box->content.height);

        const float nextX = ClampComponent(elementState.scrollX + deltaX, 0.0f, maxX);
        const float nextY = ClampComponent(elementState.scrollY + deltaY, 0.0f, maxY);

        if (nextX == elementState.scrollX && nextY == elementState.scrollY)
        {
            return false;
        }

        elementState.scrollX = nextX;
        elementState.scrollY = nextY;
        elementState.SetFlag(RuntimeFlagPaintDirty);

        result.scrolled = true;
        result.handled = true;
        result.paintChanged = true;
        return true;
    }
}
