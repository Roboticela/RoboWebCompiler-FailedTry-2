#include "RoboRenderer/RuntimeState.h"

#include "RoboRenderer/Text.h"

namespace RoboRenderer
{
    UiState::UiState(const CompiledDocument& compiledDocument)
        : document(&compiledDocument)
    {
        Reset();
    }

    void UiState::Reset()
    {
        const std::uint32_t count = document != nullptr ? document->elementCount : 0;
        elements.assign(count, ElementState{});
        inputIndex.assign(count, NoSlot);
        inputs.clear();

        hovered = NoElement;
        focused = NoElement;
        pressed = NoElement;
        documentScrollX = 0.0f;
        documentScrollY = 0.0f;
        numericOverrides.clear();

        SeedFromCompiled();
    }

    void UiState::SeedFromCompiled()
    {
        if (document == nullptr)
        {
            return;
        }

        for (ElementIndex index = 0; index < document->elementCount; ++index)
        {
            const CompiledElement& element = document->elements[index];
            ElementState& state = elements[index];

            if (element.HasFlag(ElementFlagDisabled)) state.SetFlag(RuntimeFlagDisabled);
            if (element.HasFlag(ElementFlagChecked)) state.SetFlag(RuntimeFlagChecked);
            if (element.HasFlag(ElementFlagSelected)) state.SetFlag(RuntimeFlagSelected);
            if (element.HasFlag(ElementFlagOpen)) state.SetFlag(RuntimeFlagOpen);

            const RenderCategory category = CategoryFor(element.kind);
            const bool isTextControl = category == RenderCategory::Input ||
                                       category == RenderCategory::TextArea ||
                                       category == RenderCategory::FileInput;
            const bool isChoice = category == RenderCategory::Checkbox ||
                                  category == RenderCategory::Radio;

            if (!isTextControl && !isChoice)
            {
                continue;
            }

            inputIndex[index] = static_cast<std::uint32_t>(inputs.size());
            InputFieldState field;

            // A textarea's initial value is its text content, not a value
            // attribute; a text field's is the value attribute. Both are
            // compiled data the runtime only reads to seed its own state.
            if (category == RenderCategory::TextArea)
            {
                field.value = StringAt(*document, element.text);
            }
            else
            {
                field.value = StringAt(*document, element.value);
            }

            field.caret = TextLayout::CodepointCount(field.value.c_str());
            field.selectionAnchor = field.caret;
            inputs.push_back(std::move(field));
        }
    }

    InputFieldState* UiState::Input(ElementIndex index)
    {
        if (index >= inputIndex.size() || inputIndex[index] == NoSlot)
        {
            return nullptr;
        }

        return &inputs[inputIndex[index]];
    }

    const InputFieldState* UiState::Input(ElementIndex index) const
    {
        if (index >= inputIndex.size() || inputIndex[index] == NoSlot)
        {
            return nullptr;
        }

        return &inputs[inputIndex[index]];
    }

    void UiState::SetNumericValue(ElementIndex index, float value)
    {
        if (index >= elements.size())
        {
            return;
        }

        for (NumericOverride& entry : numericOverrides)
        {
            if (entry.element == index)
            {
                entry.value = value;
                elements[index].SetFlag(RuntimeFlagValueChanged);
                elements[index].SetFlag(RuntimeFlagPaintDirty);
                return;
            }
        }

        NumericOverride entry;
        entry.element = index;
        entry.value = value;
        numericOverrides.push_back(entry);

        elements[index].SetFlag(RuntimeFlagValueChanged);
        elements[index].SetFlag(RuntimeFlagPaintDirty);
    }

    void UiState::ClearNumericValue(ElementIndex index)
    {
        for (std::size_t position = 0; position < numericOverrides.size(); ++position)
        {
            if (numericOverrides[position].element != index)
            {
                continue;
            }

            numericOverrides.erase(numericOverrides.begin() + static_cast<std::ptrdiff_t>(position));
            if (index < elements.size())
            {
                elements[index].ClearFlag(RuntimeFlagValueChanged);
                elements[index].SetFlag(RuntimeFlagPaintDirty);
            }
            return;
        }
    }

    bool UiState::HasNumericValue(ElementIndex index) const
    {
        for (const NumericOverride& entry : numericOverrides)
        {
            if (entry.element == index)
            {
                return true;
            }
        }

        return false;
    }

    float UiState::NumericValue(ElementIndex index) const
    {
        for (const NumericOverride& entry : numericOverrides)
        {
            if (entry.element == index)
            {
                return entry.value;
            }
        }

        if (document == nullptr || index >= document->elementCount)
        {
            return 0.0f;
        }

        return document->elements[index].numericValue;
    }

    const char* UiState::DisplayValue(ElementIndex index) const
    {
        if (document == nullptr || index >= elements.size())
        {
            return "";
        }

        const InputFieldState* field = Input(index);
        if (field != nullptr && !field->value.empty())
        {
            return field->value.c_str();
        }

        const CompiledElement& element = document->elements[index];
        const RenderCategory category = CategoryFor(element.kind);

        if (field != nullptr)
        {
            // An empty text control shows its placeholder.
            const char* placeholder = StringAt(*document, element.auxiliaryText);
            if (element.HasFlag(ElementFlagHasPlaceholder))
            {
                return placeholder;
            }
        }

        if (element.HasFlag(ElementFlagHasValue))
        {
            return StringAt(*document, element.value);
        }

        if (category == RenderCategory::Select || category == RenderCategory::Option)
        {
            // The selected option's label is what a closed select shows. The
            // renderer resolves that during layout, which has the tree at hand.
            return StringAt(*document, element.text);
        }

        return StringAt(*document, element.text);
    }

    void UiState::SetHovered(ElementIndex index)
    {
        if (hovered == index)
        {
            return;
        }

        // Clear the previous chain, then mark the new one. The chain is what
        // gives a parent container its hover appearance and what a future
        // :hover style resolution will consume.
        for (ElementIndex current = hovered; current != NoElement;)
        {
            ElementState& state = elements[current];
            state.ClearFlag(RuntimeFlagHovered);
            state.ClearFlag(RuntimeFlagHoveredSubtree);
            state.SetFlag(RuntimeFlagPaintDirty);

            const CompiledElement* element = ElementAt(*document, current);
            current = element != nullptr ? element->parent : NoElement;
        }

        hovered = index;

        for (ElementIndex current = index; current != NoElement;)
        {
            ElementState& state = elements[current];
            if (current == index)
            {
                state.SetFlag(RuntimeFlagHovered);
            }
            else
            {
                state.SetFlag(RuntimeFlagHoveredSubtree);
            }
            state.SetFlag(RuntimeFlagPaintDirty);

            const CompiledElement* element = ElementAt(*document, current);
            current = element != nullptr ? element->parent : NoElement;
        }
    }

    void UiState::SetPressed(ElementIndex index)
    {
        if (pressed == index)
        {
            return;
        }

        if (pressed != NoElement)
        {
            elements[pressed].ClearFlag(RuntimeFlagPressed);
            elements[pressed].SetFlag(RuntimeFlagPaintDirty);
        }

        pressed = index;

        if (pressed != NoElement)
        {
            elements[pressed].SetFlag(RuntimeFlagPressed);
            elements[pressed].SetFlag(RuntimeFlagPaintDirty);
        }
    }

    void UiState::SetFocused(ElementIndex index, bool focusVisible)
    {
        if (focused == index)
        {
            if (index != NoElement)
            {
                const bool wasVisible = elements[index].HasFlag(RuntimeFlagFocusVisible);
                if (wasVisible != focusVisible)
                {
                    if (focusVisible)
                    {
                        elements[index].SetFlag(RuntimeFlagFocusVisible);
                    }
                    else
                    {
                        elements[index].ClearFlag(RuntimeFlagFocusVisible);
                    }
                    elements[index].SetFlag(RuntimeFlagPaintDirty);
                }
            }
            return;
        }

        for (ElementIndex current = focused; current != NoElement;)
        {
            ElementState& state = elements[current];
            state.ClearFlag(RuntimeFlagFocused);
            state.ClearFlag(RuntimeFlagFocusVisible);
            state.ClearFlag(RuntimeFlagFocusSubtree);
            state.SetFlag(RuntimeFlagPaintDirty);

            const CompiledElement* element = ElementAt(*document, current);
            current = element != nullptr ? element->parent : NoElement;
        }

        focused = index;

        for (ElementIndex current = index; current != NoElement;)
        {
            ElementState& state = elements[current];
            if (current == index)
            {
                state.SetFlag(RuntimeFlagFocused);
                if (focusVisible)
                {
                    state.SetFlag(RuntimeFlagFocusVisible);
                }
            }
            else
            {
                state.SetFlag(RuntimeFlagFocusSubtree);
            }
            state.SetFlag(RuntimeFlagPaintDirty);

            const CompiledElement* element = ElementAt(*document, current);
            current = element != nullptr ? element->parent : NoElement;
        }
    }

    ElementIndex FindSelectedOption(const CompiledDocument& document,
                                    const UiState& state,
                                    ElementIndex selectIndex)
    {
        const CompiledElement* select = ElementAt(document, selectIndex);
        if (select == nullptr)
        {
            return NoElement;
        }

        ElementIndex firstOption = NoElement;

        for (ElementIndex child = select->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            const RenderCategory category = CategoryFor(node->kind);
            if (category == RenderCategory::Option)
            {
                if (firstOption == NoElement)
                {
                    firstOption = child;
                }

                const bool selected = state.Element(child).HasFlag(RuntimeFlagSelected) ||
                                      node->HasFlag(ElementFlagSelected);
                if (selected)
                {
                    return child;
                }
            }
            else if (category == RenderCategory::OptionGroup)
            {
                for (ElementIndex option = node->firstChild; option != NoElement;)
                {
                    const CompiledElement* optionNode = ElementAt(document, option);
                    if (optionNode == nullptr)
                    {
                        break;
                    }

                    if (CategoryFor(optionNode->kind) == RenderCategory::Option)
                    {
                        if (firstOption == NoElement)
                        {
                            firstOption = option;
                        }

                        const bool selected = state.Element(option).HasFlag(RuntimeFlagSelected) ||
                                              optionNode->HasFlag(ElementFlagSelected);
                        if (selected)
                        {
                            return option;
                        }
                    }

                    option = optionNode->nextSibling;
                }
            }

            child = node->nextSibling;
        }

        return firstOption;
    }

    const char* SelectedOptionLabel(const CompiledDocument& document,
                                    const UiState& state,
                                    ElementIndex selectIndex)
    {
        const ElementIndex option = FindSelectedOption(document, state, selectIndex);
        if (option == NoElement)
        {
            return "";
        }

        const CompiledElement* optionElement = ElementAt(document, option);
        if (optionElement == nullptr)
        {
            return "";
        }

        if (optionElement->auxiliaryText != 0)
        {
            return StringAt(document, optionElement->auxiliaryText);
        }

        return StringAt(document, optionElement->text);
    }

    ElementIndex UiState::FocusParent(ElementIndex index) const
    {
        if (document == nullptr)
        {
            return NoElement;
        }

        const CompiledElement* element = ElementAt(*document, index);
        while (element != nullptr && element->parent != NoElement)
        {
            if (element->parent != NoElement && document->elements[element->parent].HasFlag(ElementFlagFocusable))
            {
                return element->parent;
            }

            element = ElementAt(*document, element->parent);
        }

        return NoElement;
    }
}
