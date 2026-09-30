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

        std::uint32_t startByte = 0;
        std::uint32_t endByte = 0;

        /// True when the fragment shows a placeholder rather than a value.
        bool placeholder = false;
        /// True when the fragment is the value of a text control.
        bool controlValue = false;
        /// True when the fragment should be underlined (links).
        bool underline = false;
        /// Vertical offset for ruby annotations.
        float yOffset = 0.0f;
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
        };

        void ResolveStyles();
        void BuildLineIndex();

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
        /// In measure-only mode nothing is written to the result.
        float ArrangeElement(ElementIndex index,
                             float x,
                             float y,
                             float availableWidth,
                             const Rect& clip,
                             std::uint32_t depth,
                             bool measureOnly);

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
    };
}
