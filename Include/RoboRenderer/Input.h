// ---------------------------------------------------------------------------
// RoboRenderer - input, hit testing and focus.
//
//     Pointer position  ->  HitTester  ->  ElementIndex  ->  InputRouter
//                                                                 |
//                                                                 v
//                                                        runtime state
//
// Hit testing reads the geometry the layout engine produced. It never looks up
// an element by name, never walks a string, and never asks what an HTML tag is:
// a point maps to an element index through rectangles and the compiled tree.
//
// The router owns behaviour, not presentation: it changes runtime state, tells
// the layout engine what it invalidated, and reports what the application may
// want to know (a form was submitted, a link was activated).
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"
#include "RoboRenderer/Layout.h"
#include "RoboRenderer/RuntimeState.h"

#include <cstdint>
#include <string>

namespace RoboRenderer
{
    enum class EventType : std::uint8_t
    {
        None = 0,
        PointerMove,
        PointerDown,
        PointerUp,
        Click,
        DoubleClick,
        Wheel,
        KeyDown,
        KeyUp,
        TextInput,
        Focus,
        Blur,
        Change,
        Submit,
        ResetEvent
    };

    const char* ToString(EventType type);

    /// Keys the runtime models. A key it does not know stays Key::None and is
    /// still delivered, so an application can act on it.
    enum class Key : std::uint16_t
    {
        None = 0,
        Enter,
        Escape,
        Tab,
        Backspace,
        Delete,
        Left,
        Right,
        Up,
        Down,
        Home,
        End,
        PageUp,
        PageDown,
        Space
    };

    const char* ToString(Key key);

    enum InputModifier : std::uint8_t
    {
        ModifierNone = 0,
        ModifierShift = 1u << 0,
        ModifierControl = 1u << 1,
        ModifierAlt = 1u << 2,
        ModifierMeta = 1u << 3
    };

    struct InputEvent
    {
        EventType type = EventType::None;

        /// Pointer position in logical window units.
        float x = 0.0f;
        float y = 0.0f;

        /// Wheel or drag deltas, in logical units.
        float deltaX = 0.0f;
        float deltaY = 0.0f;

        /// 0 left, 1 middle, 2 right.
        int button = 0;

        Key key = Key::None;
        std::uint32_t modifiers = ModifierNone;

        /// UTF-8 text a TextInput event carries.
        std::string text;

        bool HasModifier(InputModifier modifier) const { return (modifiers & modifier) != 0u; }
    };

    /// What one dispatch did, so the renderer knows what to recompute and the
    /// application knows what happened.
    struct EventResult
    {
        ElementIndex target = NoElement;
        bool handled = false;

        /// Geometry changed: the renderer runs layout before painting.
        bool layoutChanged = false;
        /// Pixels changed: the renderer paints.
        bool paintChanged = false;

        /// The document was scrolled, which never invalidates layout.
        bool scrolled = false;

        /// The focused element changed.
        ElementIndex focusChanged = NoElement;

        /// The application may want to act on these.
        bool submitted = false;
        bool reset = false;
        bool activated = false;
        /// The link that was activated, as a resource-independent target string.
        StringIndex activatedHref = 0;
    };

    /// Maps a point to an element.
    class HitTester
    {
    public:
        HitTester(const CompiledDocument& document, const LayoutEngine& layout, const UiState& state);

        /// Deepest element whose border box contains the point and which is not
        /// clipped away, or NoElement when the point misses the document.
        ElementIndex HitTest(float x, float y) const;

        /// Nearest interactive element at or above the hit element: what a click
        /// actually targets.
        ElementIndex HitTestInteractive(float x, float y) const;

        /// Innermost scrollable element at the point, or NoElement.
        ElementIndex ScrollableAt(float x, float y) const;

        /// The element's border box in window coordinates, with the scroll
        /// translation of its ancestors applied.
        bool ElementBounds(ElementIndex index, Rect& out) const;

        /// Position inside an element's content box, in content coordinates.
        bool LocalPosition(ElementIndex index, float x, float y, float& localX, float& localY) const;

        /// True when the element, or an ancestor, hides the point.
        bool IsVisibleAt(ElementIndex index, float x, float y) const;

    private:
        /// Downward walk: the point is in window coordinates, `origin` is the
        /// translation the subtree is drawn with, and the last child that hits
        /// wins because it was painted last.
        ElementIndex HitTestRecursive(ElementIndex index,
                                      float pointX,
                                      float pointY,
                                      float originX,
                                      float originY,
                                      const Rect& clip) const;

        /// Scroll translation that applies to an element's children.
        void ScrollOf(ElementIndex index, float& offsetX, float& offsetY) const;

        /// Translation that applies to an element itself, accumulated from its
        /// ancestors' scroll offsets.
        void OriginOf(ElementIndex index, float& offsetX, float& offsetY) const;

        const CompiledDocument& document;
        const LayoutEngine& layout;
        const UiState& state;
    };

    /// Behaviour the runtime performs itself, with no document script involved.
    class InputRouter
    {
    public:
        InputRouter(const CompiledDocument& document, LayoutEngine& layout, UiState& state, HitTester& hitTester);

        EventResult Dispatch(const InputEvent& event);

        /// Moves focus, drawing the ring only when the platform says the focus
        /// came from the keyboard.
        EventResult SetFocus(ElementIndex index, bool focusVisible);

        /// Next focusable element in document order. `backwards` walks back.
        ElementIndex NextFocusable(ElementIndex from, bool backwards) const;

        /// Activates an element the way a click would. Exposed because tests and
        /// automation need to activate without a pointer.
        EventResult Activate(ElementIndex index);

        /// True when the element can take keyboard focus.
        bool IsFocusable(ElementIndex index) const;

        /// True when the element is a text editing control.
        bool IsTextControl(ElementIndex index) const;

        /// Programmatic editing, used by tests and by an application's own
        /// keyboard handling.
        bool InsertText(ElementIndex index, const std::string& text);
        bool DeleteBackward(ElementIndex index);
        bool DeleteForward(ElementIndex index);
        bool SelectAll(ElementIndex index);
        bool MoveCaret(ElementIndex index, std::int32_t delta, bool select);

        /// The element a click would target: interactive ancestor, or the hit
        /// element itself when nothing above it is interactive.
        ElementIndex ClickTarget(float x, float y) const;

    private:
        void UpdateHover(ElementIndex index, EventResult& result);
        bool ScrollBy(ElementIndex index, float deltaX, float deltaY, EventResult& result);

        void SetChecked(ElementIndex index, bool checked, EventResult& result);
        void SelectRadioSiblings(ElementIndex index, EventResult& result);
        void CycleSelectOption(ElementIndex index, EventResult& result);
        void ToggleDisclosure(ElementIndex index, EventResult& result);
        bool UpdateRangeFromPointer(ElementIndex index, float x, EventResult& result);

        const CompiledDocument& document;
        LayoutEngine& layout;
        UiState& state;
        HitTester& hitTester;
    };
}
