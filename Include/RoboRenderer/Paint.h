// ---------------------------------------------------------------------------
// RoboRenderer - paint.
//
//     Layout geometry + runtime state + resolved style  ->  PaintEngine
//                                                              |
//                                                              v
//                                                    typed paint commands
//
// A paint command is a number and a rectangle, never a string. The paint engine
// knows how a native category looks; it does not know a single HTML tag name and
// it cannot parse anything. It walks the tree the layout engine already produced
// and turns each element into the commands its category and its state call for.
//
// The list is a flat, contiguous stream. The backend consumes it in order, so it
// needs no tree, no recursion, and no allocation of its own.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"
#include "RoboRenderer/Layout.h"
#include "RoboRenderer/RuntimeState.h"
#include "RoboRenderer/Style.h"

#include <cstdint>
#include <vector>

namespace RoboRenderer
{
    /// Every operation the runtime can hand to a graphics backend. The set is
    /// deliberately small: a backend implements these and nothing else.
    enum class PaintCommandType : std::uint8_t
    {
        FillRect = 0,
        FillRoundedRect = 1,
        StrokeRect = 2,
        StrokeRoundedRect = 3,
        DrawImage = 4,
        DrawText = 5,
        /// Sets the clip the following commands are drawn through.
        PushClip = 6,
        PopClip = 7,
        /// Applies a translation (and scale) to the following commands. Used for
        /// scrolled subtrees and for DPI, never for HTML semantics.
        PushTransform = 8,
        PopTransform = 9
    };

    const char* ToString(PaintCommandType type);

    /// One typed paint operation.
    ///
    /// `bounds` is in logical units. Fields that do not apply to a command type
    /// stay at their default, so the struct is uniform and the backend can read
    /// it without a variant dispatch table.
    struct PaintCommand
    {
        PaintCommandType type = PaintCommandType::FillRect;

        /// Rectangle the command paints into, in logical units.
        Rect bounds;

        Color color{ 0, 0, 0, 255 };

        /// DrawImage: which compiled resource to sample.
        ResourceIndex resource = 0;

        /// Corner radius for the rounded variants.
        float radius = 0.0f;

        /// Per-edge widths for StrokeRect, clockwise from the top. A zero edge
        /// draws nothing, which is how a table cell draws only the sides its
        /// resolved border asks for.
        Edges edges{ 0.0f, 0.0f, 0.0f, 0.0f };

        /// DrawText: the run's font size and weight, in logical units.
        float fontSize = 0.0f;
        FontWeightKind weight = FontWeightKind::Normal;

        /// DrawText: range inside PaintList::Glyphs().
        std::uint32_t firstGlyph = 0;
        std::uint32_t glyphCount = 0;

        /// Element the command came from. The debug renderer uses it to attribute
        /// pixels to elements; a release build still carries it because it is one
        /// word and it makes diagnostics possible.
        ElementIndex element = NoElement;
    };

    /// One positioned glyph. The backend looks up the coverage bitmap by code
    /// point, so the command stream stays small: no per-glyph bitmap pointer, no
    /// duplicated font data. The glyph's own bearing is applied by the backend,
    /// which is the only layer that knows the device rasterization scale.
    struct PaintGlyph
    {
        /// Pen position on the baseline, in logical units.
        float x = 0.0f;
        float y = 0.0f;
        std::uint32_t codepoint = 0;
        /// The face's chain index and the font's glyph id, for a shaped glyph.
        /// A glyph id of zero means the backend resolves the code point instead,
        /// which is what the built-in stroke face and unshaped runs use.
        std::uint32_t faceId = 0;
        std::uint16_t glyphId = 0;
    };

    /// How a control paints itself in its current state. The painters below
    /// resolve element state to one of these once, instead of testing five flags
    /// per command.
    enum class ControlVisualState : std::uint8_t
    {
        Normal = 0,
        Hovered = 1,
        Pressed = 2,
        Focused = 3,
        Disabled = 4
    };

    /// The display list of one frame.
    class PaintList
    {
    public:
        void Clear();
        void Reserve(std::size_t commandCount, std::size_t glyphCount);

        PaintCommand& Add(PaintCommandType type, const Rect& bounds, ElementIndex element);

        /// Appends a glyph run to the glyph array and returns its first index.
        std::uint32_t AddGlyph(const PaintGlyph& glyph);

        const std::vector<PaintCommand>& Commands() const { return commands; }
        const std::vector<PaintGlyph>& Glyphs() const { return glyphs; }

        std::size_t CommandCount() const { return commands.size(); }
        std::size_t GlyphCount() const { return glyphs.size(); }

        /// Union of every painted rectangle: the area a full repaint covers.
        const Rect& Bounds() const { return bounds; }

        bool IsEmpty() const { return commands.empty(); }

    private:
        std::vector<PaintCommand> commands;
        std::vector<PaintGlyph> glyphs;
        Rect bounds;
    };

    /// What the last paint pass cost, for benchmarks and the debug overlay.
    struct PaintStats
    {
        double buildTimeMs = 0.0;
        std::uint32_t commandCount = 0;
        std::uint32_t glyphCount = 0;
        std::uint32_t elementCount = 0;
        std::uint32_t textRunCount = 0;
        std::uint32_t imageCount = 0;
    };

    /// Turns laid out elements into paint commands.
    ///
    /// The engine is a pure function of layout, state and style: give it the same
    /// three and it produces the same command stream. That is what lets the
    /// visual regression tests compare command streams as well as pixels.
    class PaintEngine
    {
    public:
        PaintEngine(const CompiledDocument& document,
                    const LayoutEngine& layout,
                    UiState& state,
                    StyleProvider& styles);

        /// Caret blink is a renderer decision; painting only obeys it.
        void SetCaretVisible(bool visible) { caretVisible = visible; }

        void Invalidate(ElementIndex index);
        void InvalidateAll();

        /// True when a command stream is needed: something was invalidated, or no
        /// stream has been built yet.
        bool IsDirty() const { return dirty || list.IsEmpty(); }

        /// Rebuilds the command stream and reports which region of the window has
        /// to be redrawn.
        void Build();

        const PaintList& List() const { return list; }

        /// The display list as it is being built. The debug overlay and the
        /// canvas pass append to the same stream, so a backend executes one list
        /// in one order rather than a chain of passes.
        PaintList& CommandList() { return list; }

        const PaintStats& Stats() const { return stats; }

        /// Region the last Build invalidated, in logical units.
        const Rect& DirtyRegion() const { return dirtyRegion; }
        bool IsFullRepaint() const { return fullRepaint; }

    private:
        /// Walks one element and its subtree. `originX/originY` carry the
        /// translation the subtree is painted with, which is how a scroll offset
        /// reaches painting without touching layout.
        void PaintElement(ElementIndex index, float originX, float originY, const Rect& clip);

        void PaintBackground(ElementIndex index,
                             const ElementLayout& layout,
                             const CompiledStyle& style,
                             const CompiledElement& element,
                             float originX,
                             float originY);

        void PaintBorder(ElementIndex index,
                         const ElementLayout& layout,
                         const CompiledStyle& style,
                         float originX,
                         float originY);

        /// Paints the line fragments of an element.
        void PaintText(ElementIndex index, float originX, float originY);

        /// Category painters. Every element goes through exactly one of these.
        void PaintListMarker(ElementIndex index,
                             const ElementLayout& layout,
                             const CompiledStyle& style,
                             const CompiledElement& element,
                             float originX,
                             float originY);

        void PaintImage(ElementIndex index,
                        const ElementLayout& layout,
                        const CompiledStyle& style,
                        const CompiledElement& element,
                        float originX,
                        float originY);

        void PaintControl(ElementIndex index,
                          const ElementLayout& layout,
                          const CompiledStyle& style,
                          const CompiledElement& element,
                          RenderCategory category,
                          float originX,
                          float originY);

        void PaintTableParts(ElementIndex index,
                             const ElementLayout& layout,
                             const CompiledStyle& style,
                             const CompiledElement& element,
                             RenderCategory category,
                             float originX,
                             float originY);

        void PaintSeparator(ElementIndex index,
                            const ElementLayout& layout,
                            const CompiledStyle& style,
                            const CompiledElement& element,
                            float originX,
                            float originY);

        void PaintDisclosureMarker(ElementIndex index,
                                   const ElementLayout& layout,
                                   const CompiledStyle& style,
                                   const CompiledElement& element,
                                   float originX,
                                   float originY);

        void PaintMedia(ElementIndex index,
                        const ElementLayout& layout,
                        const CompiledStyle& style,
                        const CompiledElement& element,
                        RenderCategory category,
                        float originX,
                        float originY);

        void PaintCanvas(ElementIndex index,
                         const ElementLayout& layout,
                         const CompiledStyle& style,
                         const CompiledElement& element,
                         float originX,
                         float originY);

        /// Resolves the appearance of a control from its runtime state.
        ControlVisualState VisualStateOf(ElementIndex index) const;

        /// Scroll translation of an element, if it scrolls.
        void ScrollOffsetOf(ElementIndex index, float& offsetX, float& offsetY) const;

        /// The area an element and its subtree occupy, for dirty tracking.
        Rect SubtreeBounds(ElementIndex index) const;

        /// Records a glyph run for one line fragment.
        void EmitTextRun(ElementIndex index,
                         float x,
                         float y,
                         const char* text,
                         std::uint32_t startByte,
                         std::uint32_t endByte,
                         float fontSize,
                         FontWeightKind weight,
                         const Color& color,
                         const TextRunStyle* runStyle = nullptr);

        const CompiledDocument& document;
        const LayoutEngine& layout;
        UiState& state;
        StyleProvider& styles;
        bool caretVisible = true;

        PaintList list;
        PaintStats stats;

        Rect dirtyRegion;
        std::vector<std::uint8_t> elementDirty;
        bool dirty = true;
        bool fullRepaint = true;
    };
}
