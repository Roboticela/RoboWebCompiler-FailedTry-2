// ---------------------------------------------------------------------------
// RoboRenderer - native layout.
//
//     CompiledDocument + RuntimeState + viewport  ->  LayoutEngine  ->  geometry
//
// Layout is runtime work and it is expected: it depends on the window size, the
// DPI, the measured text, the current values of controls and the state of a
// disclosure. What it never does is read HTML: it walks compiled elements whose
// category, text, resources and state the compiler already resolved.
//
// Layout is separate from painting. This engine produces geometry, line boxes and
// clipping; the paint engine consumes them and knows nothing about layout.
//
// Geometry is in absolute document coordinates. Scrolling is a translation that
// painting and hit testing apply on their way down the tree, which is why a scroll
// never invalidates layout.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"
#include "RoboRenderer/RuntimeState.h"
#include "RoboRenderer/Style.h"
#include "RoboRenderer/Text.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace RoboRenderer
{
    /// A rectangle in logical units.
    struct Rect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        float Right() const { return x + width; }
        float Bottom() const { return y + height; }

        bool IsEmpty() const { return width <= 0.0f || height <= 0.0f; }

        bool Contains(float pointX, float pointY) const
        {
            return pointX >= x && pointX < Right() && pointY >= y && pointY < Bottom();
        }

        bool Intersects(const Rect& other) const
        {
            return !(other.x >= Right() || other.Right() <= x || other.y >= Bottom() ||
                     other.Bottom() <= y);
        }

        Rect Intersection(const Rect& other) const
        {
            const float left = x > other.x ? x : other.x;
            const float top = y > other.y ? y : other.y;
            const float right = Right() < other.Right() ? Right() : other.Right();
            const float bottom = Bottom() < other.Bottom() ? Bottom() : other.Bottom();

            if (right <= left || bottom <= top)
            {
                return Rect{ left, top, 0.0f, 0.0f };
            }

            return Rect{ left, top, right - left, bottom - top };
        }

        Rect Union(const Rect& other) const
        {
            if (IsEmpty())
            {
                return other;
            }
            if (other.IsEmpty())
            {
                return *this;
            }

            const float left = x < other.x ? x : other.x;
            const float top = y < other.y ? y : other.y;
            const float right = Right() > other.Right() ? Right() : other.Right();
            const float bottom = Bottom() > other.Bottom() ? Bottom() : other.Bottom();
            return Rect{ left, top, right - left, bottom - top };
        }

        Rect Expanded(float amount) const
        {
            return Rect{ x - amount, y - amount, width + amount * 2.0f, height + amount * 2.0f };
        }

        Rect Translated(float offsetX, float offsetY) const
        {
            return Rect{ x + offsetX, y + offsetY, width, height };
        }

        /// Integer aligned, which is what a framebuffer wants.
        Rect Rounded() const
        {
            const float left = static_cast<float>(static_cast<int>(x));
            const float top = static_cast<float>(static_cast<int>(y));
            const float right = static_cast<float>(static_cast<int>(Right() + 0.5f));
            const float bottom = static_cast<float>(static_cast<int>(Bottom() + 0.5f));
            return Rect{ left, top, right - left, bottom - top };
        }
    };

    /// A float that has been placed in a block formatting context. The arranger
    /// keeps the placed boxes so that following line boxes can be shortened
    /// around them and a `clear` can be pushed below them.
    struct FloatBox
    {
        ElementIndex element = NoElement;
        /// The float's border box, in the coordinates of its block container.
        Rect box;
        FloatKind side = FloatKind::None;
    };

    /// Flags on a laid out element.
    enum LayoutFlags : std::uint16_t
    {
        LayoutFlagNone = 0,
        /// The element takes no space and paints nothing.
        LayoutFlagDisplayNone = 1u << 0,
        /// The element establishes a clip region for its subtree.
        LayoutFlagClips = 1u << 1,
        /// The element scrolls its content.
        LayoutFlagScrolls = 1u << 2,
        /// Inline level: it participates in an inline flow as an atom.
        LayoutFlagAtomic = 1u << 3,
        /// The element owns at least one line fragment.
        LayoutFlagHasLines = 1u << 4,
        /// The element is inside a table.
        LayoutFlagInTable = 1u << 5
    };

    /// One laid out element.
    struct ElementLayout
    {
        /// Border box: the element's own rectangle, including border and padding.
        Rect border;
        /// Content box: where children and text live.
        Rect content;
        /// Clip region that applies to this element and its subtree.
        Rect clip;

        /// Extent of the content when it is larger than the box.
        float scrollWidth = 0.0f;
        float scrollHeight = 0.0f;

        std::uint32_t firstLine = 0;
        std::uint32_t lineCount = 0;
        std::uint32_t depth = 0;
        std::uint16_t flags = LayoutFlagNone;
    };

    /// One line box fragment: the part of a text run placed on one line.
    struct LineLayout
    {
        /// The element the text belongs to.
        ElementIndex element = NoElement;
        /// The block that established the inline context.
        ElementIndex container = NoElement;

        float x = 0.0f;
        float width = 0.0f;
        float baseline = 0.0f;

        /// The line box this fragment sits on.
        float lineTop = 0.0f;
        float lineHeight = 0.0f;
        float lineWidth = 0.0f;

        float fontSize = 0.0f;
        FontWeightKind weight = FontWeightKind::Normal;

        /// The CSS text options the fragment was measured with, so painting
        /// measures the same run the same way. See `TextRunStyle`.
        float letterSpacing = 0.0f;
        float wordSpacing = 0.0f;
        const char* fontFamily = nullptr;
        FontSlantKind fontStyle = FontSlantKind::Upright;
        TextDirectionKind direction = TextDirectionKind::Auto;
        /// `font-kerning` and `font-feature-settings`, so painting measures and
        /// shapes the fragment exactly as layout did.
        bool kerning = true;
        const char* fontFeatures = nullptr;

        std::uint32_t startByte = 0;
        std::uint32_t endByte = 0;

        /// True when the fragment shows a placeholder rather than a value.
        bool placeholder = false;
        /// True when the fragment is the value of a text control.
        bool controlValue = false;
        /// True when the fragment should be underlined (links).
        bool underline = false;
        /// `text-decoration-line`, as `TextDecorationFlags` bits. A link's
        /// underline sets the same bit, so one field decides every decoration.
        std::uint8_t decoration = TextDecorationNone;
        /// `text-decoration-color`; the text colour paints it when the style
        /// leaves the colour at `currentColor`.
        Color decorationColor = Color::Rgba(0, 0, 0, 255);
        bool hasDecorationColor = false;
        /// `text-decoration-style`, and the thickness the line is drawn at.
        TextDecorationStyleKind decorationStyle = TextDecorationStyleKind::Solid;
        TextDecorationThicknessKind decorationThicknessKind = TextDecorationThicknessKind::Auto;
        float decorationThickness = 0.0f;
        /// `text-underline-offset`, and where the underline sits relative to the
        /// baseline: positive moves it down, negative up.
        TextUnderlineOffsetKind underlineOffsetKind = TextUnderlineOffsetKind::Auto;
        float underlineOffset = 0.0f;
        /// Vertical offset for ruby annotations.
        float yOffset = 0.0f;

        /// True when the line runs down a column rather than along a baseline,
        /// which is what `writing-mode: vertical-*` asks for. The run then starts
        /// at `lineTop` and walks down `lineHeight`, with `x` as the column's
        /// cross-axis position and `width` its cross-axis size.
        bool vertical = false;

        /// `text-orientation`: which glyphs of a vertical run stand sideways.
        TextOrientationKind textOrientation = TextOrientationKind::Mixed;
    };

    /// The geometry of one layout pass.
    struct LayoutResult
    {
        std::vector<ElementLayout> elements;
        std::vector<LineLayout> lines;
        /// Line fragment indices grouped by element, so an element can name its
        /// own fragments even though a line may mix several elements.
        std::vector<std::uint32_t> elementLines;

        /// Height of the laid out document, which is what the viewport scrolls.
        float documentHeight = 0.0f;
        float documentWidth = 0.0f;

        std::size_t laidOutCount = 0;
        std::size_t lineCount = 0;

        const ElementLayout* Element(ElementIndex index) const
        {
            return index < elements.size() ? &elements[index] : nullptr;
        }

        /// Iterates the line fragments of one element, in flow order.
        void ForEachLine(ElementIndex index,
                         const std::function<void(const LineLayout&)>& visitor) const;
    };

    /// Intrinsic size of a loaded resource, in logical units.
    struct ResourceMetrics
    {
        float width = 0.0f;
        float height = 0.0f;
        bool loaded = false;
    };

    /// Lays out a compiled document.
    class LayoutEngine
    {
    public:
        LayoutEngine(const CompiledDocument& document, StyleProvider& styles);

        /// The viewport changed: everything that depends on it is invalidated.
        void SetViewport(const RenderTargetInfo& target);
        const RenderTargetInfo& Viewport() const { return viewport; }

        /// Intrinsic sizes of the loaded resources, so an image can be laid out
        /// at its real size. Owned by the ResourceManager.
        void SetResourceMetrics(const ResourceMetrics* metrics, std::size_t count);

        /// Marks one element, or the whole document, for layout. Any geometry
        /// change relayouts the document; the per-element flags drive which
        /// region has to be repainted.
        void Invalidate(ElementIndex index);
        void InvalidateAll();

        bool NeedsLayout() const { return layoutInvalid; }
        bool IsElementDirty(ElementIndex index) const;

        /// Recomputes geometry when something is invalid, otherwise does nothing.
        void UpdateLayout(const UiState& state);

        /// Lays out unconditionally: the first frame, a viewport change, or a
        /// benchmark that wants to time layout rather than the invalid check.
        void ForceLayout(const UiState& state);

        const LayoutResult& Result() const { return result; }

        /// The style that applied to an element during the last layout pass.
        const CompiledStyle& StyleOf(ElementIndex index) const { return stylesCache[index]; }

        /// The children of `index` in paint order, written into `ordered`.
        ///
        /// The normal flow keeps tree order, but a positioned box that declares
        /// `z-index` leaves that order for the stacking level it names: negative
        /// levels paint behind the flow, positive levels in front of it, each
        /// group in ascending level and ties in tree order. Returning false
        /// means no child takes its own level, so the caller can walk the tree
        /// order it already has instead of paying for a sort.
        bool OrderedChildren(ElementIndex index, std::vector<ElementIndex>& ordered) const;

        /// Turns a declared size into a used length, or -1 when the element did
        /// not declare one. Percentages resolve against the containing block the
        /// parent handed down, viewport units against the viewport: both are
        /// runtime facts, which is why the compiler stores them unresolved.
        float ResolveDeclaredLength(const LayoutValue& value, float containingWidth) const;

        const FontRegistry& Fonts() const { return fonts; }
        const TextLayout& Text() const { return textLayout; }

        /// Total scrollable extent of the document.
        float DocumentHeight() const { return result.documentHeight; }

        /// Largest scroll offset the current geometry allows.
        float MaxScrollY() const;

        /// Number of elements laid out, including those inside a closed subtree.
        std::size_t LaidOutCount() const { return result.laidOutCount; }

    private:
        struct IntrinsicSize
        {
            float width = 0.0f;
            float height = 0.0f;
        };

        /// One item of an inline formatting context: a text fragment or an
        /// atomic box (a control, an image, a line break).
        struct InlineItem
        {
            ElementIndex element = NoElement;
            const char* text = nullptr;
            float fontSize = 0.0f;
            FontWeightKind weight = FontWeightKind::Normal;
            float width = 0.0f;
            float height = 0.0f;
            float ascent = 0.0f;
            float descent = 0.0f;
            bool isText = false;
            bool hardBreak = false;
            bool placeholder = false;
            bool controlValue = false;
            bool underline = false;
            bool preserveWhitespace = false;
            float yOffset = 0.0f;

            /// `text-decoration-line` bits and the colour they are drawn in,
            /// carried to the line the item lands on.
            std::uint8_t decoration = TextDecorationNone;
            Color decorationColor = Color::Rgba(0, 0, 0, 255);
            bool hasDecorationColor = false;
            TextDecorationStyleKind decorationStyle = TextDecorationStyleKind::Solid;
            TextDecorationThicknessKind decorationThicknessKind = TextDecorationThicknessKind::Auto;
            float decorationThickness = 0.0f;
            TextUnderlineOffsetKind underlineOffsetKind = TextUnderlineOffsetKind::Auto;
            float underlineOffset = 0.0f;

            /// The CSS text options this item is measured and painted with.
            float letterSpacing = 0.0f;
            float wordSpacing = 0.0f;
            const char* fontFamily = nullptr;
            FontSlantKind fontStyle = FontSlantKind::Upright;
            TextDirectionKind direction = TextDirectionKind::Auto;
            /// `font-kerning` and `font-feature-settings`, passed to the shaper.
            bool kerning = true;
            const char* fontFeatures = nullptr;

            /// The line-breaking family, passed to the wrapper with the run.
            HyphensKind hyphens = HyphensKind::Manual;
            OverflowWrapKind overflowWrap = OverflowWrapKind::Normal;
            WordBreakKind wordBreak = WordBreakKind::Normal;
            LineBreakKind lineBreak = LineBreakKind::Auto;
            /// `tab-size`: a count of space advances, or a length in logical
            /// pixels when `tabSizeLength` is non-zero.
            float tabSize = 0.0f;
            float tabSizeLength = 0.0f;
            /// `line-height` as a multiple of the font size, 0 for "normal".
            float lineHeightScale = 0.0f;
            /// `white-space: nowrap`: the run never wraps, however wide it is.
            bool noWrap = false;
        };

        void ResolveStyles();
        void BuildLineIndex();

        /// Applies the out-of-flow model once normal flow has placed every box.
        ///
        /// It runs after the flow and before the extent is measured, because an
        /// out-of-flow box is positioned against the *finished* geometry of its
        /// containing block, not the geometry it had while its ancestors were
        /// still being arranged. Elements are visited in document order, so an
        /// ancestor is moved before a descendant asks for its containing block.
        void ApplyPositioning();

        /// The rectangle an out-of-flow box resolves its offsets against: the
        /// padding box of the nearest positioned ancestor, or the initial
        /// containing block (the viewport) when there is none. `fixed` is always
        /// the viewport, which is what makes it ignore scrolling.
        Rect ContainingBlockFor(ElementIndex index) const;

        /// Resolves one inset against the containing block's size. `isAuto` is
        /// set when the offset is `auto`, which leaves the box at its static
        /// position on that axis.
        float ResolveInset(const LayoutValue& inset, float containingLength, bool& isAuto) const;

        /// Moves a box and everything inside it: its own geometry, the line
        /// fragments it owns and every descendant, recursively. A positioned box
        /// is moved as a unit, so a translation is the only primitive needed.
        void ShiftSubtree(ElementIndex index, float deltaX, float deltaY);

        /// True when `element` is `ancestor` or inside it.
        bool IsWithinSubtree(ElementIndex element, ElementIndex ancestor) const;

        /// Inline elements flow into line boxes rather than owning a block box,
        /// so their border box is the union of the fragments they own. Hit
        /// testing and ElementBounds need that box; painting reads the fragments
        /// directly.
        void ResolveInlineBoxes();

        bool IsDisplayed(ElementIndex index) const;

        /// True when a closed disclosure ancestor hides this element. A closed
        /// <details> still shows its summary and hides everything else.
        bool IsCollapsedByDisclosure(ElementIndex index) const;
        bool HasBlockChildren(ElementIndex index) const;
        bool StacksChildren(ElementIndex index) const;

        const char* TextFor(ElementIndex index, bool& isPlaceholder, bool& isControlValue) const;

        /// Intrinsic size of an element's content. An available width of zero
        /// means unbounded, which yields the max-content width.
        IntrinsicSize MeasureElement(ElementIndex index, float availableWidth);

        /// Arranges an element and returns the used height of its border box.
        /// In measure-only mode nothing is written to the result. `shrinkToFit`
        /// is set for a float, whose width is its content's max-content width
        /// rather than the width of its container.
        float ArrangeElement(ElementIndex index,
                             float x,
                             float y,
                             float availableWidth,
                             const Rect& clip,
                             std::uint32_t depth,
                             bool measureOnly,
                             bool shrinkToFit = false);

        float ArrangeBlockChildren(ElementIndex index,
                                   float x,
                                   float y,
                                   float availableWidth,
                                   const Rect& clip,
                                   std::uint32_t depth,
                                   bool measureOnly);

        /// Flows inline content into line boxes. `restriction` limits the flow to
        /// a specific run of children, which is how an anonymous inline row is
        /// laid out inside a block container that also holds blocks.
        float ArrangeInlineRun(ElementIndex container,
                               float x,
                               float y,
                               float availableWidth,
                               const Rect& clip,
                               std::uint32_t depth,
                               const std::vector<ElementIndex>* restriction,
                               bool measureOnly,
                               const std::vector<FloatBox>* floats = nullptr);

        /// Flows inline content down a column instead of along a baseline: the
        /// vertical-writing-mode sibling of `ArrangeInlineRun`. The block has one
        /// column, so the cross axis is as wide as the tallest line box.
        float ArrangeVerticalInlineRun(ElementIndex container,
                                       const CompiledStyle& style,
                                       const std::vector<InlineItem>& items,
                                       float x,
                                       float y,
                                       float availableWidth,
                                       std::uint32_t depth,
                                       bool measureOnly);

        void GatherInlineItems(ElementIndex parent, std::vector<InlineItem>& items);

        /// Appends the text of one element as an inline item. Returns false when
        /// the element has no text.
        bool AppendTextItem(ElementIndex index, std::vector<InlineItem>& items);

        float ArrangeTable(ElementIndex index,
                           float x,
                           float y,
                           float availableWidth,
                           const Rect& clip,
                           std::uint32_t depth,
                           bool measureOnly);

        void CollectTableRows(ElementIndex table,
                              std::vector<ElementIndex>& rows,
                              std::vector<ElementIndex>& captions) const;

        /// Places one float and returns its border box in the container's
        /// coordinates. The float is laid out shrink-to-fit at the container's
        /// left edge to learn its size, then moved to the named side and below
        /// any float it cannot sit beside.
        Rect PlaceFloat(ElementIndex index,
                        FloatKind side,
                        float x,
                        float y,
                        float availableWidth,
                        const Rect& clip,
                        std::uint32_t depth,
                        const std::vector<FloatBox>& floats);

        /// The top a cleared box must drop to so it sits below the floats the
        /// `clear` value names. Returns `y` unchanged when nothing is cleared.
        float ClearBelowFloats(ClearKind clear, float y, const std::vector<FloatBox>& floats) const;

        /// The left and right insets a float imposes on a line box whose top is
        /// `lineTop`, as offsets into a container starting at `x` and `availableWidth`
        /// wide. Both are zero when no float overlaps the line's band.
        void FloatInsetsAt(const std::vector<FloatBox>& floats,
                           float lineTop,
                           float x,
                           float availableWidth,
                           float& leftInset,
                           float& rightInset) const;

        const CompiledDocument& document;
        StyleProvider& styleProvider;
        FontRegistry fonts;
        TextLayout textLayout;

        std::vector<CompiledStyle> stylesCache;
        std::vector<std::uint8_t> elementDirty;

        const ResourceMetrics* resourceMetrics = nullptr;
        std::size_t resourceMetricCount = 0;

        const UiState* state = nullptr;

        RenderTargetInfo viewport;
        LayoutResult result;
        bool layoutInvalid = true;

        /// Measurement scratch, following one rule: a value written by a nested
        /// call is read by its caller before anything else runs. That is what
        /// lets intrinsic measurement reuse one buffer instead of allocating per
        /// element.
        ElementLayout measureBox;
        float lastInlineWidth = 0.0f;
        std::vector<float> columnScratch;

        /// Owns the strings a `text-transform` produced. An inline item's text
        /// points at document storage today, so a transformed run needs somewhere
        /// to live for as long as the item does; the pass that built it does.
        std::vector<std::string> transformedText;
    };
}
