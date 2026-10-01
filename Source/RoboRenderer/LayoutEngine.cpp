#include "RoboRenderer/Layout.h"

#include <algorithm>
#include <cmath>

namespace RoboRenderer
{
    void LayoutResult::ForEachLine(ElementIndex index,
                                  const std::function<void(const LineLayout&)>& visitor) const
    {
        if (index >= elements.size())
        {
            return;
        }

        const ElementLayout& layout = elements[index];
        for (std::uint32_t slot = 0; slot < layout.lineCount; ++slot)
        {
            const std::uint32_t lineIndex = layout.firstLine + slot;
            if (lineIndex < elementLines.size())
            {
                visitor(lines[elementLines[lineIndex]]);
            }
        }
    }

    namespace
    {
        bool IsBlockDisplay(DisplayKind display)
        {
            return display == DisplayKind::Block || display == DisplayKind::TableRow ||
                   display == DisplayKind::TableCell;
        }

        float ClampPositive(float value)
        {
            return value > 0.0f ? value : 0.0f;
        }

        /// A category whose children stack even though HTML would let them flow
        /// inline. Form controls in a form are the native case: a framework lays a
        /// form out as a column of fields, and that is what the default form style
        /// says.
        bool IsStackingCategory(RenderCategory category)
        {
            return category == RenderCategory::Form || category == RenderCategory::FieldSet;
        }

        float AlignmentOffset(TextAlignKind align, float available, float used)
        {
            const float slack = available - used;
            if (slack <= 0.0f)
            {
                return 0.0f;
            }

            switch (align)
            {
            case TextAlignKind::Center: return slack * 0.5f;
            case TextAlignKind::End: return slack;
            case TextAlignKind::Start: return 0.0f;
            }

            return 0.0f;
        }
    }

    // -----------------------------------------------------------------------
    // Construction, viewport and invalidation
    // -----------------------------------------------------------------------

    LayoutEngine::LayoutEngine(const CompiledDocument& compiledDocument, StyleProvider& styles)
        : document(compiledDocument)
        , styleProvider(styles)
        , textLayout(fonts)
    {
        stylesCache.resize(document.elementCount);
        elementDirty.assign(document.elementCount, 1);
        result.elements.resize(document.elementCount);
        ResolveStyles();
    }

    void LayoutEngine::SetViewport(const RenderTargetInfo& target)
    {
        if (target.logicalWidth == viewport.logicalWidth &&
            target.logicalHeight == viewport.logicalHeight &&
            target.deviceScale == viewport.deviceScale)
        {
            return;
        }

        viewport = target;
        InvalidateAll();
    }

    void LayoutEngine::SetResourceMetrics(const ResourceMetrics* metrics, std::size_t count)
    {
        resourceMetrics = metrics;
        resourceMetricCount = count;
        InvalidateAll();
    }

    void LayoutEngine::InvalidateAll()
    {
        layoutInvalid = true;
        std::fill(elementDirty.begin(), elementDirty.end(), static_cast<std::uint8_t>(1));
    }

    void LayoutEngine::Invalidate(ElementIndex index)
    {
        if (index >= elementDirty.size())
        {
            InvalidateAll();
            return;
        }

        elementDirty[index] = 1;
        layoutInvalid = true;
    }

    bool LayoutEngine::IsElementDirty(ElementIndex index) const
    {
        return index < elementDirty.size() && elementDirty[index] != 0;
    }

    float LayoutEngine::ResolveDeclaredLength(const LayoutValue& value, float containingWidth) const
    {
        switch (value.type)
        {
        case LayoutValueType::Fixed:
            return value.value;

        case LayoutValueType::ParentRelative:
            // `width: 50%` is a fraction of the containing block, which is what
            // the parent handed down as the available width.
            return containingWidth > 0.0f ? value.value * containingWidth : -1.0f;

        case LayoutValueType::ViewportRelative:
            // `50vw`: the viewport is the one thing layout may not substitute a
            // containing block for.
            return viewport.logicalWidth > 0.0f ? value.value * viewport.logicalWidth : -1.0f;

        case LayoutValueType::Auto:
        case LayoutValueType::Content:
            break;
        }

        // Not a declared size: the caller falls back to the content size.
        return -1.0f;
    }

    void LayoutEngine::ResolveStyles()
    {
        for (ElementIndex index = 0; index < document.elementCount; ++index)
        {
            const CompiledElement& element = document.elements[index];
            stylesCache[index] = styleProvider.StyleFor(document, index, CategoryFor(element.kind));
        }
    }

    void LayoutEngine::UpdateLayout(const UiState& runtimeState)
    {
        if (!layoutInvalid)
        {
            return;
        }

        ForceLayout(runtimeState);
    }

    void LayoutEngine::ForceLayout(const UiState& runtimeState)
    {
        state = &runtimeState;

        if (stylesCache.size() != document.elementCount)
        {
            stylesCache.resize(document.elementCount);
            elementDirty.assign(document.elementCount, 1);
            result.elements.resize(document.elementCount);
        }

        ResolveStyles();

        result.lines.clear();
        result.elementLines.clear();
        result.laidOutCount = 0;
        transformedText.clear();

        const float viewportWidth = viewport.logicalWidth > 0.0f ? viewport.logicalWidth : 800.0f;
        const float viewportHeight = viewport.logicalHeight > 0.0f ? viewport.logicalHeight : 600.0f;
        const Rect viewportRect{ 0.0f, 0.0f, viewportWidth, viewportHeight };

        ArrangeElement(document.root, 0.0f, 0.0f, viewportWidth, viewportRect, 0, false);

        BuildLineIndex();
        ResolveInlineBoxes();

        const ElementLayout& rootLayout = result.elements[document.root];
        result.documentWidth = std::max(viewportWidth, rootLayout.border.width);
        result.documentHeight = std::max(viewportHeight, rootLayout.border.height);

        result.lineCount = result.lines.size();

        std::fill(elementDirty.begin(), elementDirty.end(), static_cast<std::uint8_t>(0));
        layoutInvalid = false;
        state = nullptr;
    }

    void LayoutEngine::BuildLineIndex()
    {
        const std::size_t elementCount = result.elements.size();
        if (elementCount == 0 || result.lines.empty())
        {
            return;
        }

        // A counting sort of line fragments by element. Fragments of one element
        // are not contiguous in flow order (a bold word interrupts a sentence),
        // but every consumer wants "the lines of this element".
        std::vector<std::uint32_t> counts(elementCount + 1, 0);
        for (const LineLayout& line : result.lines)
        {
            if (line.element < elementCount)
            {
                ++counts[line.element + 1];
            }
        }

        for (std::size_t index = 1; index <= elementCount; ++index)
        {
            counts[index] += counts[index - 1];
        }

        result.elementLines.resize(result.lines.size());
        std::vector<std::uint32_t> cursor(counts.begin(), counts.end());

        for (std::uint32_t index = 0; index < result.lines.size(); ++index)
        {
            const ElementIndex element = result.lines[index].element;
            if (element >= elementCount)
            {
                continue;
            }

            result.elementLines[cursor[element]++] = index;
        }

        for (std::size_t index = 0; index < elementCount; ++index)
        {
            result.elements[index].firstLine = counts[index];
            result.elements[index].lineCount = counts[index + 1] - counts[index];
            if (result.elements[index].lineCount != 0)
            {
                result.elements[index].flags |= LayoutFlagHasLines;
            }
        }
    }

    void LayoutEngine::ResolveInlineBoxes()
    {
        // An inline container is placed by the inline formatting context, which
        // owns its geometry through line fragments rather than through a block
        // box. Giving it the union of its own fragments makes it addressable by
        // hit testing and by ElementBounds without changing what is painted: the
        // fragments themselves are still what draws the text.
        const std::size_t elementCount = result.elements.size();

        for (std::size_t index = 0; index < elementCount; ++index)
        {
            ElementLayout& box = result.elements[index];

            if ((box.flags & LayoutFlagDisplayNone) != 0 || box.lineCount == 0)
            {
                continue;
            }

            // A block that already produced a box keeps it; only elements that
            // were never given one take their box from the fragments.
            if (!box.border.IsEmpty())
            {
                continue;
            }

            Rect area;
            bool haveArea = false;

            for (std::uint32_t slot = 0; slot < box.lineCount; ++slot)
            {
                const std::uint32_t lineSlot = box.firstLine + slot;
                if (lineSlot >= result.elementLines.size())
                {
                    break;
                }

                const LineLayout& line = result.lines[result.elementLines[lineSlot]];
                const Rect fragment{ line.x, line.lineTop, line.width, line.lineHeight };
                area = haveArea ? area.Union(fragment) : fragment;
                haveArea = true;
            }

            if (haveArea)
            {
                box.border = area;
                box.content = area;
                box.scrollWidth = area.width;
                box.scrollHeight = area.height;
            }
        }
    }

    float LayoutEngine::MaxScrollY() const
    {
        return std::max(0.0f, result.documentHeight - viewport.logicalHeight);
    }

    // -----------------------------------------------------------------------
    // Predicates
    // -----------------------------------------------------------------------

    bool LayoutEngine::IsDisplayed(ElementIndex index) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || state == nullptr)
        {
            return false;
        }

        if (stylesCache[index].display == DisplayKind::Hidden)
        {
            return false;
        }

        const RenderCategory category = CategoryFor(element->kind);
        if (category == RenderCategory::None)
        {
            return false;
        }

        // A dialog shows while it is open: runtime state, not compiled state. A
        // disclosure is always displayed - a closed <details> still shows its
        // summary, and IsCollapsedByDisclosure hides the rest of its content.
        const bool open = state->Element(index).HasFlag(RuntimeFlagOpen) ||
                          element->HasFlag(ElementFlagOpen);

        if (category == RenderCategory::Dialog && !open)
        {
            return false;
        }

        return true;
    }

    bool LayoutEngine::IsCollapsedByDisclosure(ElementIndex index) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || element->parent == NoElement || state == nullptr)
        {
            return false;
        }

        const CompiledElement* parent = ElementAt(document, element->parent);
        if (parent == nullptr || CategoryFor(parent->kind) != RenderCategory::Disclosure)
        {
            return false;
        }

        const bool open = state->Element(element->parent).HasFlag(RuntimeFlagOpen) ||
                          parent->HasFlag(ElementFlagOpen);
        if (open)
        {
            return false;
        }

        // The summary is the only content a closed disclosure shows. The
        // compiler keeps it as the disclosure's first child.
        return CategoryFor(element->kind) != RenderCategory::DisclosureSummary;
    }

    bool LayoutEngine::HasBlockChildren(ElementIndex index) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return false;
        }

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            if (IsDisplayed(child) && !IsCollapsedByDisclosure(child) &&
                IsBlockDisplay(stylesCache[child].display))
            {
                return true;
            }

            child = node->nextSibling;
        }

        return false;
    }

    bool LayoutEngine::StacksChildren(ElementIndex index) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return false;
        }

        return HasBlockChildren(index) || IsStackingCategory(CategoryFor(element->kind));
    }

    // -----------------------------------------------------------------------
    // Intrinsic measurement
    // -----------------------------------------------------------------------

    LayoutEngine::IntrinsicSize LayoutEngine::MeasureElement(ElementIndex index, float availableWidth)
    {
        IntrinsicSize size;

        if (!IsDisplayed(index))
        {
            return size;
        }

        // Measurement never writes to the result: it arranges the element in
        // measure-only mode, which leaves its geometry in the scratch box, and
        // reads that before anything else can run. An available width of zero
        // means unbounded, which is the max-content case an inline atom needs.
        const Rect clip{ 0.0f, 0.0f, viewport.logicalWidth, viewport.logicalHeight };

        ArrangeElement(index, 0.0f, 0.0f, availableWidth, clip, 0, true);

        const CompiledStyle& style = stylesCache[index];

        size.width = measureBox.border.width + style.margin.Horizontal();
        size.height = measureBox.border.height + style.margin.Vertical();
        return size;
    }

    // -----------------------------------------------------------------------
    // Text of an element
    // -----------------------------------------------------------------------

    const char* LayoutEngine::TextFor(ElementIndex index, bool& isPlaceholder, bool& isControlValue) const
    {
        isPlaceholder = false;
        isControlValue = false;

        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || state == nullptr)
        {
            return "";
        }

        const RenderCategory category = CategoryFor(element->kind);

        if (category == RenderCategory::Input || category == RenderCategory::TextArea)
        {
            isControlValue = true;

            const InputFieldState* field = state->Input(index);
            if (field != nullptr && !field->value.empty())
            {
                return field->value.c_str();
            }

            if (element->HasFlag(ElementFlagHasPlaceholder))
            {
                isPlaceholder = true;
                return StringAt(document, element->auxiliaryText);
            }

            return "";
        }

        if (category == RenderCategory::Select)
        {
            isControlValue = true;
            return SelectedOptionLabel(document, *state, index);
        }

        if (category == RenderCategory::FileInput)
        {
            isControlValue = true;
            const InputFieldState* field = state->Input(index);
            return field != nullptr && !field->value.empty() ? field->value.c_str() : "";
        }

        if (category == RenderCategory::Option && element->auxiliaryText != 0)
        {
            return StringAt(document, element->auxiliaryText);
        }

        return StringAt(document, element->text);
    }

    // -----------------------------------------------------------------------
    // Inline formatting context
    // -----------------------------------------------------------------------

    bool LayoutEngine::AppendTextItem(ElementIndex index, std::vector<InlineItem>& items)
    {
        const CompiledElement* node = ElementAt(document, index);
        if (node == nullptr || state == nullptr)
        {
            return false;
        }

        bool isPlaceholder = false;
        bool isControlValue = false;
        const char* text = TextFor(index, isPlaceholder, isControlValue);
        if (text == nullptr || text[0] == '\0')
        {
            return false;
        }

        const CompiledStyle& style = stylesCache[index];
        const RenderCategory category = CategoryFor(node->kind);

        // `text-transform` rewrites the run before it is measured, so the width
        // and the painted glyphs describe the same text.
        const TextTransformKind transform = IsSpecified(style.specified, StyleFlagTextTransform)
                                                ? style.textTransform
                                                : TextTransformKind::None;

        if (transform != TextTransformKind::None)
        {
            transformedText.push_back(ApplyTextTransform(text, transform));
            text = transformedText.back().c_str();
        }

        const char* family = nullptr;
        if (IsSpecified(style.specified, StyleFlagFontFamily) && style.fontFamily != 0)
        {
            family = StringAt(document, style.fontFamily);
        }

        const FontSlantKind slant = IsSpecified(style.specified, StyleFlagFontStyle) ? style.fontStyle
                                                                                    : FontSlantKind::Upright;
        const TextDirectionKind direction =
            IsSpecified(style.specified, StyleFlagDirection) ? style.direction : TextDirectionKind::Auto;

        // `font-kerning` and `font-feature-settings` decide how the run shapes,
        // so they are measured with the same settings painting will use.
        const bool kerning = !(IsSpecified(style.specified, StyleFlagFontKerning) &&
                               style.fontKerning == FontKerningKind::None);
        const char* fontFeatures =
            IsSpecified(style.specified, StyleFlagFontFeatures) && style.fontFeatures != 0
                ? StringAt(document, style.fontFeatures)
                : nullptr;

        TextRunStyle runStyle;
        runStyle.letterSpacing = IsSpecified(style.specified, StyleFlagLetterSpacing) ? style.letterSpacing : 0.0f;
        runStyle.wordSpacing = IsSpecified(style.specified, StyleFlagWordSpacing) ? style.wordSpacing : 0.0f;
        runStyle.fontFamily = family;
        runStyle.slant = slant;
        runStyle.direction = direction;
        runStyle.kerning = kerning;
        runStyle.fontFeatures = fontFeatures;

        const FontFace& face = fonts.Face(style.fontWeight, family, slant);
        const float scale = style.fontSize / face.UnitsPerEm();

        const bool preserved = node->HasFlag(ElementFlagPreserveWhitespace) ||
                               (IsSpecified(style.specified, StyleFlagWhiteSpace) &&
                                (style.whiteSpace == WhiteSpaceKind::Pre ||
                                 style.whiteSpace == WhiteSpaceKind::PreWrap ||
                                 style.whiteSpace == WhiteSpaceKind::PreLine));

        InlineItem item;
        item.element = index;
        item.text = text;
        item.isText = true;
        item.fontSize = style.fontSize;
        item.weight = style.fontWeight;
        item.width = textLayout.Measure(text, style.fontSize, style.fontWeight, &runStyle);
        item.ascent = face.Ascent() * scale;
        item.descent = face.Descent() * scale;
        item.height = item.ascent + item.descent;
        item.placeholder = isPlaceholder;
        item.controlValue = isControlValue;
        item.underline = category == RenderCategory::Link;
        item.decoration = IsSpecified(style.specified, StyleFlagTextDecoration)
                              ? style.textDecoration
                              : TextDecorationNone;
        if (item.underline)
        {
            item.decoration |= TextDecorationUnderline;
        }
        item.hasDecorationColor = IsSpecified(style.specified, StyleFlagTextDecorationColor) != 0;
        item.decorationColor = style.textDecorationColor;
        item.kerning = kerning;
        item.fontFeatures = fontFeatures;
        item.preserveWhitespace = preserved;
        item.letterSpacing = runStyle.letterSpacing;
        item.wordSpacing = runStyle.wordSpacing;
        item.fontFamily = family;
        item.fontStyle = slant;
        item.direction = direction;
        item.lineHeightScale = IsSpecified(style.specified, StyleFlagLineHeight) ? style.lineHeight : 0.0f;
        item.noWrap = IsSpecified(style.specified, StyleFlagWhiteSpace) &&
                      style.whiteSpace == WhiteSpaceKind::NoWrap;
        item.yOffset = category == RenderCategory::RubyText ? -style.fontSize * 0.55f : 0.0f;
        items.push_back(item);
        return true;
    }

    void LayoutEngine::GatherInlineItems(ElementIndex parent, std::vector<InlineItem>& items)
    {
        const CompiledElement* element = ElementAt(document, parent);
        if (element == nullptr)
        {
            return;
        }

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            const RenderCategory category = CategoryFor(node->kind);
            const CompiledStyle& style = stylesCache[child];

            if (!IsDisplayed(child) || style.display == DisplayKind::Hidden)
            {
                child = node->nextSibling;
                continue;
            }

            if (IsBlockDisplay(style.display))
            {
                // A block inside an inline context. The caller's run splitting
                // normally places it; reaching it here means the run was entered
                // through an inline parent, so it becomes an atomic box.
                InlineItem item;
                item.element = child;
                item.isText = false;
                const IntrinsicSize size = MeasureElement(child, 0.0f);
                item.width = size.width;
                item.height = size.height;
                item.ascent = size.height;
                items.push_back(item);

                child = node->nextSibling;
                continue;
            }

            if (category == RenderCategory::LineBreak)
            {
                InlineItem item;
                item.element = child;
                item.isText = false;
                item.hardBreak = true;
                item.height = textLayout.LineHeight(style.fontSize, style.fontWeight);
                items.push_back(item);

                child = node->nextSibling;
                continue;
            }

            if (IsReplacedCategory(category) || category == RenderCategory::Button ||
                category == RenderCategory::DisclosureSummary)
            {
                // A control or an image is an atom in the flow: it is placed as a
                // whole box and its own content is laid out inside it.
                InlineItem item;
                item.element = child;
                item.isText = false;
                const IntrinsicSize size = MeasureElement(child, 0.0f);
                item.width = size.width;
                item.height = size.height;
                item.ascent = size.height;
                items.push_back(item);

                child = node->nextSibling;
                continue;
            }

            AppendTextItem(child, items);

            if (node->firstChild != NoElement && !StacksChildren(child))
            {
                // An inline container continues the same line, so its children are
                // gathered into this flow rather than laid out separately.
                GatherInlineItems(child, items);
            }

            child = node->nextSibling;
        }
    }

    float LayoutEngine::ArrangeInlineRun(ElementIndex container,
                                         float x,
                                         float y,
                                         float availableWidth,
                                         const Rect& clip,
                                         std::uint32_t depth,
                                         const std::vector<ElementIndex>* restriction,
                                         bool measureOnly)
    {
        const CompiledElement* element = ElementAt(document, container);
        if (element == nullptr)
        {
            lastInlineWidth = 0.0f;
            return 0.0f;
        }

        const CompiledStyle& style = stylesCache[container];

        std::vector<InlineItem> items;
        items.reserve(16);

        if (restriction == nullptr)
        {
            if (!StacksChildren(container))
            {
                AppendTextItem(container, items);
            }

            GatherInlineItems(container, items);
        }
        else
        {
            for (const ElementIndex child : *restriction)
            {
                const CompiledElement* node = ElementAt(document, child);
                if (node == nullptr || !IsDisplayed(child))
                {
                    continue;
                }

                const RenderCategory category = CategoryFor(node->kind);
                const CompiledStyle& childStyle = stylesCache[child];

                if (IsBlockDisplay(childStyle.display))
                {
                    continue;
                }

                if (category == RenderCategory::LineBreak)
                {
                    InlineItem item;
                    item.element = child;
                    item.isText = false;
                    item.hardBreak = true;
                    item.height = textLayout.LineHeight(childStyle.fontSize, childStyle.fontWeight);
                    items.push_back(item);
                    continue;
                }

                if (IsReplacedCategory(category) || category == RenderCategory::Button ||
                    category == RenderCategory::DisclosureSummary)
                {
                    InlineItem item;
                    item.element = child;
                    item.isText = false;
                    const IntrinsicSize size = MeasureElement(child, 0.0f);
                    item.width = size.width;
                    item.height = size.height;
                    item.ascent = size.height;
                    items.push_back(item);
                    continue;
                }

                AppendTextItem(child, items);

                if (node->firstChild != NoElement && !StacksChildren(child))
                {
                    GatherInlineItems(child, items);
                }
            }
        }

        const bool wrap = availableWidth > 0.0f;

        struct Fragment
        {
            const InlineItem* item = nullptr;
            float x = 0.0f;
            float width = 0.0f;
            std::uint32_t startByte = 0;
            std::uint32_t endByte = 0;
        };

        struct AtomicBox
        {
            const InlineItem* item = nullptr;
            float x = 0.0f;
        };

        std::vector<Fragment> fragments;
        std::vector<AtomicBox> atomics;
        std::vector<TextLine> wrappedLines;

        // `text-indent` shifts the first line of the block only.
        const float indent = IsSpecified(style.specified, StyleFlagTextIndent) ? style.textIndent : 0.0f;

        float lineTop = y;
        float currentIndent = indent;
        float penX = x + indent;
        float maxAscent = 0.0f;
        float maxDescent = 0.0f;
        float specifiedLineHeight = 0.0f;
        float usedHeight = 0.0f;
        float widestLine = 0.0f;

        const auto flushLine = [&]() {
            if (fragments.empty() && atomics.empty())
            {
                return;
            }

            // A specified `line-height` can widen a line box beyond what the
            // font's own metrics ask for; it never narrows it, so a tall glyph
            // still fits and the line boxes do not overlap.
            const float lineHeight = std::max(maxAscent + maxDescent, specifiedLineHeight);
            const float baseline = lineTop + maxAscent;
            const float used = penX - x - currentIndent;
            widestLine = std::max(widestLine, used);

            const float offset = AlignmentOffset(style.textAlign, availableWidth, used);

            for (const AtomicBox& atomic : atomics)
            {
                if (measureOnly)
                {
                    continue;
                }

                ArrangeElement(atomic.item->element, x + offset + atomic.x,
                               baseline - atomic.item->ascent, atomic.item->width, clip, depth, false);
            }

            if (!measureOnly)
            {
                for (const Fragment& fragment : fragments)
                {
                    LineLayout line;
                    line.element = fragment.item->element;
                    line.container = container;
                    line.x = x + offset + currentIndent + fragment.x;
                    line.width = fragment.width;
                    line.baseline = baseline;
                    line.lineTop = lineTop;
                    line.lineHeight = lineHeight;
                    line.lineWidth = used;
                    line.fontSize = fragment.item->fontSize;
                    line.weight = fragment.item->weight;
                    line.letterSpacing = fragment.item->letterSpacing;
                    line.wordSpacing = fragment.item->wordSpacing;
                    line.fontFamily = fragment.item->fontFamily;
                    line.fontStyle = fragment.item->fontStyle;
                    line.direction = fragment.item->direction;
                    line.startByte = fragment.startByte;
                    line.endByte = fragment.endByte;
                    line.placeholder = fragment.item->placeholder;
                    line.controlValue = fragment.item->controlValue;
                    line.underline = fragment.item->underline;
                    line.decoration = fragment.item->decoration;
                    line.decorationColor = fragment.item->decorationColor;
                    line.hasDecorationColor = fragment.item->hasDecorationColor;
                    line.kerning = fragment.item->kerning;
                    line.fontFeatures = fragment.item->fontFeatures;
                    line.yOffset = fragment.item->yOffset;
                    result.lines.push_back(line);
                }
            }

            lineTop += lineHeight;
            usedHeight = lineTop - y;
            penX = x;
            currentIndent = 0.0f;
            maxAscent = 0.0f;
            maxDescent = 0.0f;
            specifiedLineHeight = 0.0f;
            fragments.clear();
            atomics.clear();
        };

        for (const InlineItem& item : items)
        {
            if (item.hardBreak)
            {
                flushLine();
                const float breakHeight = item.height > 0.0f
                                              ? item.height
                                              : textLayout.LineHeight(style.fontSize, style.fontWeight);
                lineTop += breakHeight;
                usedHeight = lineTop - y;
                continue;
            }

            if (!item.isText)
            {
                if (wrap && penX > x && (penX - x) + item.width > availableWidth)
                {
                    flushLine();
                }

                atomics.push_back(AtomicBox{ &item, penX - x - currentIndent });
                penX += item.width;
                maxAscent = std::max(maxAscent, item.ascent);
                maxDescent = std::max(maxDescent, item.descent);
                continue;
            }

            std::uint32_t offset = 0;
            bool firstFragment = true;

            for (;;)
            {
                const float limit = firstFragment
                                        ? (wrap ? std::max(1.0f, availableWidth - (penX - x)) : 0.0f)
                                        : (wrap ? availableWidth : 0.0f);

                TextRunStyle runStyle;
                runStyle.letterSpacing = item.letterSpacing;
                runStyle.wordSpacing = item.wordSpacing;
                runStyle.fontFamily = item.fontFamily;
                runStyle.slant = item.fontStyle;
                runStyle.direction = item.direction;
                runStyle.kerning = item.kerning;
                runStyle.fontFeatures = item.fontFeatures;

                wrappedLines.clear();
                textLayout.Wrap(item.text + offset, item.fontSize, item.weight,
                                item.noWrap ? 0.0f : limit, item.preserveWhitespace, wrappedLines, &runStyle);

                if (wrappedLines.empty())
                {
                    break;
                }

                const TextLine& head = wrappedLines[0];

                if (firstFragment && wrap && penX > x && head.width > limit)
                {
                    // Not even the first word fits in the remaining space.
                    flushLine();
                    firstFragment = false;
                    continue;
                }

                fragments.push_back(Fragment{ &item, penX - x - currentIndent, head.width,
                                              offset + head.startByte, offset + head.endByte });
                penX += head.width;
                maxAscent = std::max(maxAscent, item.ascent);
                maxDescent = std::max(maxDescent, item.descent);
                if (item.lineHeightScale > 0.0f)
                {
                    specifiedLineHeight =
                        std::max(specifiedLineHeight, item.lineHeightScale * item.fontSize);
                }

                if (wrappedLines.size() == 1)
                {
                    break;
                }

                // The next slice starts after the break. A hard break consumed a
                // newline, which is one byte the line's end offset does not
                // count, so it has to be stepped over or the following line would
                // be seen as an empty one at the start of the next slice.
                offset += head.endByte + (head.hardBreak ? 1u : 0u);
                flushLine();
                firstFragment = false;
            }
        }

        flushLine();

        lastInlineWidth = widestLine;
        return usedHeight;
    }

    // -----------------------------------------------------------------------
    // Block layout
    // -----------------------------------------------------------------------

    float LayoutEngine::ArrangeBlockChildren(ElementIndex index,
                                             float x,
                                             float y,
                                             float availableWidth,
                                             const Rect& clip,
                                             std::uint32_t depth,
                                             bool measureOnly)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return 0.0f;
        }

        float cursorY = y;
        std::vector<ElementIndex> inlineRun;

        const auto flushRun = [&]() {
            if (inlineRun.empty())
            {
                return;
            }

            cursorY += ArrangeInlineRun(index, x, cursorY, availableWidth, clip, depth, &inlineRun,
                                        measureOnly);
            inlineRun.clear();
        };

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            if (!IsDisplayed(child) || IsCollapsedByDisclosure(child))
            {
                if (!measureOnly)
                {
                    result.elements[child].flags |= LayoutFlagDisplayNone;
                }

                child = node->nextSibling;
                continue;
            }

            const CompiledStyle& childStyle = stylesCache[child];

            if (IsBlockDisplay(childStyle.display))
            {
                flushRun();

                cursorY += childStyle.margin.top;
                cursorY += ArrangeElement(child, x, cursorY, availableWidth, clip, depth, measureOnly);
                cursorY += childStyle.margin.bottom;
            }
            else
            {
                inlineRun.push_back(child);
            }

            child = node->nextSibling;
        }

        flushRun();
        return cursorY - y;
    }

    // -----------------------------------------------------------------------
    // Tables
    // -----------------------------------------------------------------------

    void LayoutEngine::CollectTableRows(ElementIndex table,
                                        std::vector<ElementIndex>& rows,
                                        std::vector<ElementIndex>& captions) const
    {
        const CompiledElement* element = ElementAt(document, table);
        if (element == nullptr)
        {
            return;
        }

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            const RenderCategory category = CategoryFor(node->kind);

            if (category == RenderCategory::TableRow)
            {
                rows.push_back(child);
            }
            else if (category == RenderCategory::TableCaption)
            {
                captions.push_back(child);
            }
            else if (category == RenderCategory::TableSection)
            {
                for (ElementIndex grandChild = node->firstChild; grandChild != NoElement;)
                {
                    const CompiledElement* grandNode = ElementAt(document, grandChild);
                    if (grandNode == nullptr)
                    {
                        break;
                    }

                    if (CategoryFor(grandNode->kind) == RenderCategory::TableRow)
                    {
                        rows.push_back(grandChild);
                    }

                    grandChild = grandNode->nextSibling;
                }
            }

            child = node->nextSibling;
        }
    }

    float LayoutEngine::ArrangeTable(ElementIndex index,
                                     float x,
                                     float y,
                                     float availableWidth,
                                     const Rect& clip,
                                     std::uint32_t depth,
                                     bool measureOnly)
    {
        std::vector<ElementIndex> rows;
        std::vector<ElementIndex> captions;
        CollectTableRows(index, rows, captions);

        float cursorY = y;

        for (const ElementIndex caption : captions)
        {
            cursorY += ArrangeElement(caption, x, cursorY, availableWidth, clip, depth, measureOnly);
        }

        if (rows.empty())
        {
            lastInlineWidth = 0.0f;
            return cursorY - y;
        }

        struct CellRef
        {
            ElementIndex element = NoElement;
            std::uint32_t column = 0;
            std::uint32_t span = 1;
            float contentWidth = 0.0f;
            float boxWidth = 0.0f;
        };

        std::vector<CellRef> cells;
        std::vector<std::uint32_t> rowStart;
        rowStart.push_back(0);

        // Column count and the intrinsic width each cell needs.
        std::uint32_t columnCount = 0;
        for (const ElementIndex row : rows)
        {
            const CompiledElement* rowElement = ElementAt(document, row);
            if (rowElement == nullptr)
            {
                continue;
            }

            std::uint32_t column = 0;
            for (ElementIndex cell = rowElement->firstChild; cell != NoElement;)
            {
                const CompiledElement* cellElement = ElementAt(document, cell);
                if (cellElement == nullptr)
                {
                    break;
                }

                if (CategoryFor(cellElement->kind) == RenderCategory::TableCell)
                {
                    CellRef reference;
                    reference.element = cell;
                    reference.column = column;
                    reference.span = cellElement->columnSpan != 0 ? cellElement->columnSpan : 1u;

                    const IntrinsicSize size = MeasureElement(cell, 0.0f);
                    reference.contentWidth = size.width;
                    reference.boxWidth = size.width;
                    cells.push_back(reference);
                    column += reference.span;
                }

                cell = cellElement->nextSibling;
            }

            columnCount = std::max(columnCount, column);
            rowStart.push_back(static_cast<std::uint32_t>(cells.size()));
        }

        if (columnCount == 0)
        {
            columnCount = 1;
        }

        columnScratch.assign(columnCount, 0.0f);

        for (const CellRef& cell : cells)
        {
            if (cell.span == 1 && cell.column < columnCount)
            {
                columnScratch[cell.column] = std::max(columnScratch[cell.column], cell.contentWidth);
            }
        }

        for (const CellRef& cell : cells)
        {
            if (cell.span <= 1)
            {
                continue;
            }

            float spanned = 0.0f;
            for (std::uint32_t offset = 0; offset < cell.span && cell.column + offset < columnCount; ++offset)
            {
                spanned += columnScratch[cell.column + offset];
            }

            if (cell.contentWidth > spanned)
            {
                const std::uint32_t covered =
                    std::min(cell.span, columnCount > cell.column ? columnCount - cell.column : 1u);
                const float extra = (cell.contentWidth - spanned) / static_cast<float>(covered);
                for (std::uint32_t offset = 0; offset < covered; ++offset)
                {
                    columnScratch[cell.column + offset] += extra;
                }
            }
        }

        float totalWidth = 0.0f;
        for (const float width : columnScratch)
        {
            totalWidth += width;
        }

        // An auto table shrinks to fit its container rather than overflowing.
        if (availableWidth > 0.0f && totalWidth > availableWidth && totalWidth > 0.0f)
        {
            const float factor = availableWidth / totalWidth;
            totalWidth = 0.0f;
            for (float& width : columnScratch)
            {
                width = std::max(16.0f, width * factor);
                totalWidth += width;
            }
        }

        // Rows and cells.
        for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            const std::size_t begin = rowStart[rowIndex];
            const std::size_t stop = rowStart[rowIndex + 1];

            float rowHeight = 0.0f;
            float cellX = x;

            for (std::size_t index2 = begin; index2 < stop; ++index2)
            {
                CellRef& cell = cells[index2];

                float cellWidth = 0.0f;
                for (std::uint32_t offset = 0; offset < cell.span && cell.column + offset < columnCount; ++offset)
                {
                    cellWidth += columnScratch[cell.column + offset];
                }

                if (cellWidth <= 0.0f)
                {
                    cellWidth = columnScratch[std::min(cell.column, columnCount - 1)];
                }

                cell.boxWidth = cellWidth;
                rowHeight = std::max(rowHeight,
                                     ArrangeElement(cell.element, cellX, cursorY, cellWidth, clip,
                                                    depth + 1, measureOnly));
                cellX += cellWidth;
            }

            if (!measureOnly)
            {
                ElementLayout& rowLayout = result.elements[rows[rowIndex]];
                rowLayout.border = Rect{ x, cursorY, totalWidth, rowHeight };
                rowLayout.content = rowLayout.border;
                rowLayout.clip = clip;
                rowLayout.flags |= LayoutFlagInTable;
                rowLayout.depth = depth;
            }

            cursorY += rowHeight;
        }

        lastInlineWidth = totalWidth;

        if (!measureOnly)
        {
            result.elements[index].scrollWidth = totalWidth;
        }
        else
        {
            measureBox.scrollWidth = totalWidth;
        }

        return cursorY - y;
    }

    // -----------------------------------------------------------------------
    // Element arrangement
    // -----------------------------------------------------------------------

    float LayoutEngine::ArrangeElement(ElementIndex index,
                                       float x,
                                       float y,
                                       float availableWidth,
                                       const Rect& clip,
                                       std::uint32_t depth,
                                       bool measureOnly)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return 0.0f;
        }

        const RenderCategory category = CategoryFor(element->kind);
        const CompiledStyle& style = stylesCache[index];

        if (!IsDisplayed(index))
        {
            ElementLayout hidden;
            hidden.flags |= LayoutFlagDisplayNone;
            hidden.depth = depth;

            measureBox = hidden;
            if (!measureOnly)
            {
                result.elements[index] = hidden;
            }

            return 0.0f;
        }

        const Edges margin = style.margin;
        const Edges border = style.borderWidth;
        const Edges padding = style.padding;

        const float boxX = x + margin.left;
        const float boxY = y + margin.top;
        const float extraWidth = border.Horizontal() + padding.Horizontal();
        const float extraHeight = border.Vertical() + padding.Vertical();

        // --- declared size -------------------------------------------------
        float declaredWidth = element->HasFlag(ElementFlagHasWidth) ? element->attributeWidth : 0.0f;
        float declaredHeight = element->HasFlag(ElementFlagHasHeight) ? element->attributeHeight : 0.0f;

        if (declaredWidth <= 0.0f)
        {
            const float resolved = ResolveDeclaredLength(style.width, availableWidth);
            if (resolved > 0.0f)
            {
                declaredWidth = resolved;
            }
        }

        if (declaredHeight <= 0.0f)
        {
            const float resolved = ResolveDeclaredLength(style.height, availableWidth);
            if (resolved > 0.0f)
            {
                declaredHeight = resolved;
            }
        }

        if (element->resource != 0 && resourceMetrics != nullptr && element->resource < resourceMetricCount &&
            category == RenderCategory::Image)
        {
            const ResourceMetrics& metrics = resourceMetrics[element->resource];
            if (metrics.loaded)
            {
                if (declaredWidth <= 0.0f && !element->HasFlag(ElementFlagHasWidth))
                {
                    declaredWidth = metrics.width;
                }
                if (declaredHeight <= 0.0f && !element->HasFlag(ElementFlagHasHeight))
                {
                    declaredHeight = metrics.height;
                }
            }
        }

        // Textarea rows/columns and input size are the HTML sizing model.
        if (category == RenderCategory::TextArea)
        {
            const float lineHeight = textLayout.LineHeight(style.fontSize, style.fontWeight);
            if (declaredHeight <= 0.0f && element->HasFlag(ElementFlagHasRows) && element->rows != 0)
            {
                declaredHeight = static_cast<float>(element->rows) * lineHeight + extraHeight;
            }
            if (declaredWidth <= 0.0f && element->HasFlag(ElementFlagHasColumns) && element->columns != 0)
            {
                declaredWidth = static_cast<float>(element->columns) * style.fontSize * 0.55f + extraWidth;
            }
        }

        if (category == RenderCategory::Input && declaredWidth <= 0.0f &&
            element->HasFlag(ElementFlagHasColumns) && element->columns != 0)
        {
            // <input size="…"> is a character count, converted with the real
            // advance of the face rather than with a fixed character width.
            declaredWidth = static_cast<float>(element->columns) * style.fontSize * 0.55f + extraWidth;
        }

        // --- placeholder for a category outside the HTML-only phase --------
        if (category == RenderCategory::UnsupportedNamespace)
        {
            if (declaredWidth <= 0.0f) declaredWidth = 160.0f;
            if (declaredHeight <= 0.0f) declaredHeight = 48.0f;

            ElementLayout placeholder;
            placeholder.border = Rect{ boxX, boxY, declaredWidth, declaredHeight };
            placeholder.content = placeholder.border;
            placeholder.clip = clip;
            placeholder.scrollWidth = declaredWidth;
            placeholder.scrollHeight = declaredHeight;
            placeholder.depth = depth;

            measureBox = placeholder;
            if (!measureOnly)
            {
                result.elements[index] = placeholder;
                ++result.laidOutCount;
            }

            return declaredHeight + margin.Vertical();
        }

        // A control whose content is drawn from its own value (a text field, a
        // textarea, a select) lays its text out like any other text container; a
        // control sized by the category itself (a checkbox, a range, a progress
        // bar) is a fixed box.
        const bool replaced = IsReplacedCategory(category) && category != RenderCategory::TextArea &&
                              category != RenderCategory::Input && category != RenderCategory::Select;
        const bool blockLevel = IsBlockCategory(category) && !IsInlineCategory(category);
        const bool fillsWidth = blockLevel && availableWidth > 0.0f;

        // --- intrinsic width when the box is not sized yet ------------------
        float maxContentWidth = 0.0f;

        const bool needsIntrinsicWidth = declaredWidth <= 0.0f && !fillsWidth && !replaced &&
                                         category != RenderCategory::Table;

        if (needsIntrinsicWidth)
        {
            if (StacksChildren(index))
            {
                // A container of blocks sizes itself to its widest child, which
                // is measured on demand below.
                maxContentWidth = extraWidth;
            }
            else
            {
                ArrangeInlineRun(index, 0.0f, 0.0f, 0.0f, clip, depth + 1, nullptr, true);
                maxContentWidth = lastInlineWidth + extraWidth;
            }
        }

        float boxWidth = declaredWidth;
        if (boxWidth <= 0.0f)
        {
            boxWidth = fillsWidth ? ClampPositive(availableWidth - margin.Horizontal()) : maxContentWidth;
        }

        const float minimumWidth = ResolveDeclaredLength(style.minimumWidth, availableWidth);
        if (minimumWidth > 0.0f && boxWidth < minimumWidth)
        {
            boxWidth = minimumWidth;
        }

        if (boxWidth <= 0.0f)
        {
            // An empty inline-block still has its padding, and a block with an
            // unbounded container fills nothing: both end up at their padding.
            boxWidth = extraWidth;
        }

        const float contentWidth = ClampPositive(boxWidth - extraWidth);
        const float contentX = boxX + border.left + padding.left;
        const float contentY = boxY + border.top + padding.top;

        // --- content ------------------------------------------------------
        float contentHeight = 0.0f;

        if (replaced)
        {
            if (category == RenderCategory::Image)
            {
                if (declaredHeight <= 0.0f)
                {
                    const float lineHeight = textLayout.LineHeight(style.fontSize, style.fontWeight);
                    const bool hasAlternativeText = element->HasFlag(ElementFlagHasAlternativeText);
                    contentHeight = hasAlternativeText ? lineHeight + 8.0f : 24.0f;
                }
                else
                {
                    contentHeight = declaredHeight - extraHeight;
                }
            }
            else
            {
                float intrinsicCategoryWidth = 0.0f;
                float intrinsicCategoryHeight = 0.0f;
                CategoryIntrinsicSize(category, intrinsicCategoryWidth, intrinsicCategoryHeight);

                if (declaredWidth <= 0.0f && !fillsWidth)
                {
                    boxWidth = intrinsicCategoryWidth + extraWidth;
                }

                contentHeight = declaredHeight > 0.0f ? declaredHeight - extraHeight : intrinsicCategoryHeight;
            }
        }
        else if (category == RenderCategory::Table)
        {
            contentHeight = ArrangeTable(index, contentX, contentY, contentWidth, clip, depth + 1,
                                         measureOnly);

            if (declaredWidth <= 0.0f && !fillsWidth)
            {
                const float tableWidth = measureOnly ? measureBox.scrollWidth
                                                     : result.elements[index].scrollWidth;
                boxWidth = tableWidth + extraWidth;
            }
        }
        else if (StacksChildren(index))
        {
            contentHeight = ArrangeBlockChildren(index, contentX, contentY, contentWidth, clip, depth + 1,
                                                 measureOnly);
        }
        else
        {
            // Everything else -- text, inline containers, controls that carry
            // their own value -- flows its content into line boxes.
            const std::size_t lineMark = measureOnly ? 0 : result.lines.size();
            contentHeight = ArrangeInlineRun(index, contentX, contentY, contentWidth, clip, depth + 1,
                                             nullptr, measureOnly);

            // A text control scrolls its own content horizontally to keep the
            // caret in view; the caret position is runtime state, the shift is
            // geometry applied to this field's fragments only.
            if (category == RenderCategory::Input && !measureOnly && state != nullptr)
            {
                const InputFieldState* field = state->Input(index);
                if (field != nullptr && field->scrollX > 0.0f)
                {
                    for (std::size_t lineIndex = lineMark; lineIndex < result.lines.size(); ++lineIndex)
                    {
                        if (result.lines[lineIndex].element == index)
                        {
                            result.lines[lineIndex].x -= field->scrollX;
                        }
                    }
                }
            }
        }

        // --- height -------------------------------------------------------
        float boxHeight = declaredHeight;
        if (boxHeight <= 0.0f)
        {
            boxHeight = contentHeight + extraHeight;
        }

        if (category == RenderCategory::Input)
        {
            // A text field is one line tall even when its value is empty.
            const float lineHeight = textLayout.LineHeight(style.fontSize, style.fontWeight);
            boxHeight = std::max(boxHeight, lineHeight + extraHeight);
        }

        const float minimumHeight = ResolveDeclaredLength(style.minimumHeight, availableWidth);
        if (minimumHeight > 0.0f && boxHeight < minimumHeight)
        {
            boxHeight = minimumHeight;
        }

        // The document frame fills the viewport even when its content is short,
        // which is what a native window does and what makes it scroll only when
        // the content is actually taller.
        if (index == document.root && viewport.logicalHeight > 0.0f && boxHeight < viewport.logicalHeight)
        {
            boxHeight = viewport.logicalHeight;
        }

        ElementLayout layout;
        layout.border = Rect{ boxX, boxY, boxWidth, boxHeight };
        layout.content = Rect{ contentX, contentY, ClampPositive(boxWidth - extraWidth),
                               ClampPositive(boxHeight - extraHeight) };
        layout.depth = depth;

        if (IsInlineCategory(category))
        {
            layout.flags |= LayoutFlagAtomic;
        }

        if (style.overflowX != OverflowKind::Visible || style.overflowY != OverflowKind::Visible)
        {
            layout.flags |= LayoutFlagClips;
            layout.clip = layout.content.Intersection(clip);
        }
        else
        {
            layout.clip = clip;
        }

        if (style.overflowX == OverflowKind::Scroll || style.overflowY == OverflowKind::Scroll)
        {
            layout.flags |= LayoutFlagScrolls;
        }

        layout.scrollWidth = std::max(layout.content.width, contentWidth);
        layout.scrollHeight = std::max(layout.content.height, contentHeight);

        measureBox = layout;

        if (!measureOnly)
        {
            result.elements[index] = layout;
            ++result.laidOutCount;
        }

        return boxHeight + margin.Vertical();
    }
}
