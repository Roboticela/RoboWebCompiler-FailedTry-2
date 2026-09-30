// ---------------------------------------------------------------------------
// RoboRenderer - runtime state.
//
//     Compiled data (immutable, shared, in read-only memory)
//              +
//     Runtime state (mutable, per window)
//              =
//     The UI that is drawn and interacted with
//
// Nothing here writes to the compiled document. Hovering a button, typing in an
// input, opening a disclosure or scrolling a panel changes this state only; the
// compiled tables stay exactly as the compiler emitted them, so several windows
// can render the same document at once.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"

#include <string>
#include <vector>

namespace RoboRenderer
{
    /// Per element runtime flags.
    enum RuntimeElementFlags : std::uint32_t
    {
        RuntimeFlagNone = 0u,

        /// Pointer is over the element.
        RuntimeFlagHovered = 1u << 0,
        /// Pointer is held down on the element.
        RuntimeFlagPressed = 1u << 1,
        /// Keyboard focus.
        RuntimeFlagFocused = 1u << 2,
        /// Focus was reached by keyboard, so the focus ring is drawn.
        RuntimeFlagFocusVisible = 1u << 3,
        /// Pointer is over the element's subtree (for parents of the hovered one).
        RuntimeFlagHoveredSubtree = 1u << 4,
        /// The element's subtree contains the focused element.
        RuntimeFlagFocusSubtree = 1u << 5,

        /// Runtime state the document also declares initially.
        RuntimeFlagChecked = 1u << 6,
        RuntimeFlagSelected = 1u << 7,
        RuntimeFlagOpen = 1u << 8,
        RuntimeFlagDisabled = 1u << 9,

        /// The element's own value changed since it was compiled.
        RuntimeFlagValueChanged = 1u << 10,

        /// Invalidation: geometry must be recomputed, or the element repainted.
        RuntimeFlagLayoutDirty = 1u << 11,
        RuntimeFlagPaintDirty = 1u << 12
    };

    struct ElementState
    {
        std::uint32_t flags = 0;

        /// Scroll offset inside the element, when its style allows scrolling.
        float scrollX = 0.0f;
        float scrollY = 0.0f;

        bool HasFlag(RuntimeElementFlags flag) const { return (flags & flag) != 0u; }
        void SetFlag(RuntimeElementFlags flag) { flags |= flag; }
        void ClearFlag(RuntimeElementFlags flag) { flags &= ~flag; }
    };

    /// Text editing state of a text-like control. Empty for every other element,
    /// so a document with 100,000 elements stores none of this.
    struct InputFieldState
    {
        /// Current value, in UTF-8. The compiled value is only the initial value.
        std::string value;
        /// Caret position as a code point index.
        std::uint32_t caret = 0;
        /// Selection anchor; equal to the caret when there is no selection.
        std::uint32_t selectionAnchor = 0;
        /// Scroll offset inside the field, in logical units.
        float scrollX = 0.0f;

        /// Byte range of the selection, ordered.
        std::uint32_t SelectionStart() const
        {
            return caret < selectionAnchor ? caret : selectionAnchor;
        }

        std::uint32_t SelectionEnd() const
        {
            return caret < selectionAnchor ? selectionAnchor : caret;
        }

        bool HasSelection() const { return caret != selectionAnchor; }
    };

    /// All mutable UI state for one document in one window.
    class UiState
    {
    public:
        explicit UiState(const CompiledDocument& document);

        /// Rebuilds the arrays for the document and returns every element to the
        /// state its compiled data describes.
        void Reset();

        std::size_t ElementCount() const { return elements.size(); }

        ElementState& Element(ElementIndex index) { return elements[index]; }
        const ElementState& Element(ElementIndex index) const { return elements[index]; }

        bool IsValid(ElementIndex index) const { return index < elements.size(); }

        /// The text editing state of a control, or nullptr when the element is
        /// not a text control.
        InputFieldState* Input(ElementIndex index);
        const InputFieldState* Input(ElementIndex index) const;

        /// The text a control displays: its runtime value when it has one, its
        /// compiled initial value otherwise, and the placeholder for an empty
        /// text field.
        const char* DisplayValue(ElementIndex index) const;

        // --- numeric values ----------------------------------------------
        /// Range, progress and meter values the runtime changed. The compiled
        /// value stays untouched; an override lives here, sparsely, so a
        /// document of 100,000 elements stores overrides only where one exists.
        void SetNumericValue(ElementIndex index, float value);
        void ClearNumericValue(ElementIndex index);
        bool HasNumericValue(ElementIndex index) const;
        /// The current value of a numeric control: the override when there is
        /// one, the compiled value otherwise.
        float NumericValue(ElementIndex index) const;

        // --- pointer, focus and activation -------------------------------
        ElementIndex Hovered() const { return hovered; }
        ElementIndex Focused() const { return focused; }
        ElementIndex Pressed() const { return pressed; }

        void SetHovered(ElementIndex index);
        void SetPressed(ElementIndex index);
        void SetFocused(ElementIndex index, bool focusVisible);

        /// Nearest focusable ancestor, or NoElement.
        ElementIndex FocusParent(ElementIndex index) const;

        // --- document scroll ---------------------------------------------
        float ScrollX() const { return documentScrollX; }
        float ScrollY() const { return documentScrollY; }
        void SetScroll(float x, float y)
        {
            documentScrollX = x;
            documentScrollY = y;
        }

        /// Every element whose compiled state should seed its runtime state.
        /// Called by the renderer when the document is attached.
        void SeedFromCompiled();

    private:
        /// Text controls only, so a document of 100,000 elements stores no
        /// editing state it does not need. `inputIndex` holds the slot of each
        /// element, or NoSlot.
        static constexpr std::uint32_t NoSlot = 0xFFFFFFFFu;

        const CompiledDocument* document;
        std::vector<ElementState> elements;
        std::vector<std::uint32_t> inputIndex;
        std::vector<InputFieldState> inputs;

        /// Sparse overrides, searched linearly: a document has a handful of
        /// numeric controls at most, and an array indexed by element would cost
        /// every element in the document.
        struct NumericOverride
        {
            ElementIndex element = NoElement;
            float value = 0.0f;
        };

        std::vector<NumericOverride> numericOverrides;

        ElementIndex hovered = NoElement;
        ElementIndex focused = NoElement;
        ElementIndex pressed = NoElement;

        float documentScrollX = 0.0f;
        float documentScrollY = 0.0f;
    };

    /// The option a closed <select> shows: the selected one, or the first option
    /// when the document selects none.
    ElementIndex FindSelectedOption(const CompiledDocument& document,
                                    const UiState& state,
                                    ElementIndex selectIndex);

    /// The label a closed <select> shows.
    const char* SelectedOptionLabel(const CompiledDocument& document,
                                    const UiState& state,
                                    ElementIndex selectIndex);
}
