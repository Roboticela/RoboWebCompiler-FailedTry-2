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
                   display == DisplayKind::TableCell || display == DisplayKind::Flex;
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

        // Normal flow is finished, so every containing block is final. The
        // out-of-flow model runs here, between the flow and the extent, because
        // it needs finished geometry to resolve against and it can change how
        // far the document reaches.
        ApplyPositioning();

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

    // -----------------------------------------------------------------------
    // The out-of-flow model
    // -----------------------------------------------------------------------

    float LayoutEngine::ResolveInset(const LayoutValue& inset, float containingLength, bool& isAuto) const
    {
        isAuto = false;

        switch (inset.type)
        {
        case LayoutValueType::Fixed:
            // An inset may be negative, which is what pulls a box past the edge
            // of its containing block, so the value is returned as written.
            return inset.value;

        case LayoutValueType::ParentRelative:
            // A percentage inset is a fraction of the containing block, and it
            // is the *height* that a vertical inset resolves against.
            return inset.value * containingLength;

        case LayoutValueType::ViewportRelative:
            return inset.value * viewport.logicalWidth;

        case LayoutValueType::Auto:
        case LayoutValueType::Content:
            break;
        }

        // `auto`: the box stays at its static position on that axis, which is
        // not the same as an offset of zero.
        isAuto = true;
        return 0.0f;
    }

    Rect LayoutEngine::ContainingBlockFor(ElementIndex index) const
    {
        const CompiledStyle& style = stylesCache[index];

        // A fixed box is positioned against the viewport, never against an
        // ancestor: that is what makes scrolling not move it.
        if (style.position == PositionKind::Fixed)
        {
            return Rect{ 0.0f, 0.0f, viewport.logicalWidth, viewport.logicalHeight };
        }

        const CompiledElement* element = ElementAt(document, index);

        for (ElementIndex parent = element != nullptr ? element->parent : NoElement; parent != NoElement;)
        {
            const CompiledStyle& parentStyle = stylesCache[parent];

            if (IsPositioned(parentStyle.position))
            {
                // The containing block of an absolutely positioned box is the
                // *padding box* of its positioned ancestor, which is the content
                // box grown by the padding.
                const ElementLayout& box = result.elements[parent];
                return Rect{ box.content.x - parentStyle.padding.left,
                             box.content.y - parentStyle.padding.top,
                             box.content.width + parentStyle.padding.Horizontal(),
                             box.content.height + parentStyle.padding.Vertical() };
            }

            const CompiledElement* node = ElementAt(document, parent);
            parent = node != nullptr ? node->parent : NoElement;
        }

        // No positioned ancestor: the initial containing block is the viewport.
        return Rect{ 0.0f, 0.0f, viewport.logicalWidth, viewport.logicalHeight };
    }

    bool LayoutEngine::IsWithinSubtree(ElementIndex element, ElementIndex ancestor) const
    {
        for (ElementIndex index = element; index != NoElement;)
        {
            if (index == ancestor)
            {
                return true;
            }

            const CompiledElement* node = ElementAt(document, index);
            index = node != nullptr ? node->parent : NoElement;
        }

        return false;
    }

    void LayoutEngine::ShiftSubtree(ElementIndex index, float deltaX, float deltaY)
    {
        if ((deltaX == 0.0f && deltaY == 0.0f) || index >= result.elements.size())
        {
            return;
        }

        ElementLayout& box = result.elements[index];

        if ((box.flags & LayoutFlagDisplayNone) != 0)
        {
            return;
        }

        box.border = box.border.Translated(deltaX, deltaY);
        box.content = box.content.Translated(deltaX, deltaY);
        box.clip = box.clip.Translated(deltaX, deltaY);

        // The line fragments this element owns travel with it. A fragment is
        // geometry the paint engine draws from directly, so moving the box
        // without moving them would move everything except the text.
        for (std::uint32_t slot = 0; slot < box.lineCount; ++slot)
        {
            const std::uint32_t lineSlot = box.firstLine + slot;
            if (lineSlot >= result.elementLines.size())
            {
                break;
            }

            LineLayout& line = result.lines[result.elementLines[lineSlot]];
            line.x += deltaX;
            line.lineTop += deltaY;
            line.baseline += deltaY;
        }

        const CompiledElement* element = ElementAt(document, index);
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

            ShiftSubtree(child, deltaX, deltaY);
            child = node->nextSibling;
        }
    }

    bool LayoutEngine::OrderedChildren(ElementIndex index, std::vector<ElementIndex>& ordered) const
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return false;
        }

        // Most boxes hold nothing that takes its own stacking level, so the
        // scan decides whether the sort is worth doing at all.
        bool anyLevelled = false;
        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            if (StackingLevel(stylesCache[child]) != 0)
            {
                anyLevelled = true;
                break;
            }

            child = node->nextSibling;
        }

        if (!anyLevelled)
        {
            return false;
        }

        ordered.clear();
        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            ordered.push_back(child);
            child = node->nextSibling;
        }

        // A stable sort by level keeps tree order inside each level: negatives
        // rise to the front, positives sink to the back, level zero holds the
        // position the flow gave it.
        std::stable_sort(ordered.begin(), ordered.end(), [this](ElementIndex a, ElementIndex b) {
            return StackingLevel(stylesCache[a]) < StackingLevel(stylesCache[b]);
        });

        return true;
    }

    void LayoutEngine::ApplyPositioning()
    {
        // Document order is parent before child, so an ancestor is moved before
        // a descendant asks where its containing block ended up.
        for (ElementIndex index = 0; index < document.elementCount; ++index)
        {
            const CompiledStyle& style = stylesCache[index];

            if (!IsPositioned(style.position))
            {
                continue;
            }

            const ElementLayout& box = result.elements[index];
            if ((box.flags & LayoutFlagDisplayNone) != 0)
            {
                continue;
            }

            const Rect containingBlock = ContainingBlockFor(index);

            bool topIsAuto = false;
            bool rightIsAuto = false;
            bool bottomIsAuto = false;
            bool leftIsAuto = false;

            const float top = ResolveInset(style.insetTop, containingBlock.height, topIsAuto);
            const float right = ResolveInset(style.insetRight, containingBlock.width, rightIsAuto);
            const float bottom = ResolveInset(style.insetBottom, containingBlock.height, bottomIsAuto);
            const float left = ResolveInset(style.insetLeft, containingBlock.width, leftIsAuto);

            float deltaX = 0.0f;
            float deltaY = 0.0f;

            if (IsOutOfFlow(style.position))
            {
                // An out-of-flow box is placed against the containing block. A
                // missing offset leaves it at its static position on that axis,
                // which is what the flow already computed; a pair of offsets
                // takes the first that was given, which is the start/end rule the
                // specification states for a box whose size is fixed.
                float targetX = box.border.x;
                float targetY = box.border.y;

                if (!leftIsAuto)
                {
                    targetX = containingBlock.x + left;
                }
                else if (!rightIsAuto)
                {
                    targetX = containingBlock.Right() - right - box.border.width;
                }

                if (!topIsAuto)
                {
                    targetY = containingBlock.y + top;
                }
                else if (!bottomIsAuto)
                {
                    targetY = containingBlock.Bottom() - bottom - box.border.height;
                }

                deltaX = targetX - box.border.x;
                deltaY = targetY - box.border.y;
            }
            else
            {
                // `relative` and `sticky` move the box, and everything inside
                // it, without moving anything else: the space it occupied in the
                // flow stays where it was.
                if (!leftIsAuto)
                {
                    deltaX = left;
                }
                else if (!rightIsAuto)
                {
                    deltaX = -right;
                }

                if (!topIsAuto)
                {
                    deltaY = top;
                }
                else if (!bottomIsAuto)
                {
                    deltaY = -bottom;
                }
            }

            ShiftSubtree(index, deltaX, deltaY);
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

            // An out-of-flow child is handled by the block path whatever its
            // display is: it has no inline box in the flow, so a container whose
            // only child is absolutely positioned still has to be arranged as a
            // block to place it.
            if (IsOutOfFlow(stylesCache[child].position))
            {
                return true;
            }

            // A float is the same: it is placed by the block path against a side
            // and never becomes part of a line, so a container holding one is a
            // block container even when everything else in it is inline.
            if (stylesCache[child].floatKind != FloatKind::None)
            {
                return true;
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
        // A line box is as tall as the face's own line height, which is what
        // `line-height: normal` means: the ascent, the descent *and* the line
        // gap. Leaving the gap out stacks lines tighter than the font asks;
        // that only shows on a family that uses one (Inter does, Segoe UI does
        // not), so the gap belongs to the descent here rather than nowhere.
        item.ascent = face.Ascent() * scale;
        item.descent = (face.Descent() + face.LineGap()) * scale;
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
        item.decorationStyle = IsSpecified(style.specified, StyleFlagTextDecorationStyle)
                                  ? style.textDecorationStyle
                                  : TextDecorationStyleKind::Solid;
        item.decorationThicknessKind = IsSpecified(style.specified, StyleFlagTextDecorationThickness)
                                           ? style.textDecorationThicknessKind
                                           : TextDecorationThicknessKind::Auto;
        item.decorationThickness = style.textDecorationThickness;
        item.underlineOffsetKind = IsSpecified(style.specified, StyleFlagTextUnderlineOffset)
                                       ? style.textUnderlineOffsetKind
                                       : TextUnderlineOffsetKind::Auto;
        item.underlineOffset = style.textUnderlineOffset;
        item.kerning = kerning;
        item.fontFeatures = fontFeatures;
        item.hyphens = IsSpecified(style.specified, StyleFlagHyphens) ? style.hyphens : HyphensKind::Manual;
        item.overflowWrap = IsSpecified(style.specified, StyleFlagOverflowWrap) ? style.overflowWrap
                                                                                : OverflowWrapKind::Normal;
        item.wordBreak = IsSpecified(style.specified, StyleFlagWordBreak) ? style.wordBreak
                                                                           : WordBreakKind::Normal;
        item.lineBreak = IsSpecified(style.specified, StyleFlagLineBreak) ? style.lineBreak
                                                                           : LineBreakKind::Auto;
        item.tabSize = style.tabSize;
        item.tabSizeLength = style.tabSizeLength;
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

            if (IsOutOfFlow(style.position))
            {
                // The block path places it; it never becomes part of a line.
                child = node->nextSibling;
                continue;
            }

            if (style.floatKind != FloatKind::None)
            {
                // A float is placed by the block path against a side; it never
                // becomes part of a line.
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
                                         std::uint32_t depth,                               const std::vector<ElementIndex>* restriction,
                               bool measureOnly,
                               const std::vector<FloatBox>* floats)
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

        // A vertical writing mode reads down a column rather than along a
        // baseline, which is a different flow entirely rather than a flag on
        // this one, so it takes its own path over the same items.
        if (IsVerticalWritingMode(style.writingMode))
        {
            return ArrangeVerticalInlineRun(container, style, items, x, y, availableWidth, depth, measureOnly);
        }

        const bool wrap = availableWidth > 0.0f;

        // A float shortens the lines whose vertical band it crosses. The insets
        // are recomputed for every line, because a float only reaches as far
        // down as its own box: the lines below it get the full width back.
        float lineLeftInset = 0.0f;
        float lineRightInset = 0.0f;
        float lineAvailable = availableWidth;
        float lineX = x;

        const auto refreshLineInsets = [&](float top) {
            if (floats == nullptr)
            {
                lineLeftInset = 0.0f;
                lineRightInset = 0.0f;
            }
            else
            {
                FloatInsetsAt(*floats, top, x, availableWidth, lineLeftInset, lineRightInset);
            }

            lineAvailable = std::max(1.0f, availableWidth - lineLeftInset - lineRightInset);
            lineX = x + lineLeftInset;
        };

        refreshLineInsets(y);

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
        float penX = lineX + indent;
        float maxAscent = 0.0f;
        float maxDescent = 0.0f;
        float specifiedLineHeight = 0.0f;
        float usedHeight = 0.0f;
        float widestLine = 0.0f;

        // The final flush is the last line of the run, which is the one
        // justification leaves alone. `flushLine` reads this flag rather than
        // taking a parameter because it is called from six places and only the
        // last one is the tail.
        bool flushingLastLine = false;

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
            const float used = penX - lineX - currentIndent;
            widestLine = std::max(widestLine, used);

            // Justification grows the gaps a line already has: the spaces inside
            // each fragment and the seam between two fragments. The extra is
            // handed to the paint engine as word spacing, so a fragment's glyphs
            // stay in step with the measurement that positioned the line.
            const auto spaceCount = [](const Fragment& fragment) -> std::size_t {
                const char* text = fragment.item->text;
                std::size_t count = 0;

                for (std::uint32_t byte = fragment.startByte; byte < fragment.endByte; ++byte)
                {
                    if (text[byte] == ' ')
                    {
                        ++count;
                    }
                }

                return count;
            };

            TextAlignKind effectiveAlign = style.textAlign;
            float justifyExtra = 0.0f;

            if (style.textAlign == TextAlignKind::Justify)
            {
                // The last line of a block is not justified unless
                // `text-align-last` asks for it.
                if (flushingLastLine)
                {
                    effectiveAlign = style.hasTextAlignLast ? style.textAlignLast : TextAlignKind::Start;
                }

                if (effectiveAlign == TextAlignKind::Justify && !measureOnly &&
                    style.textJustify != TextJustifyKind::None)
                {
                    std::size_t gaps = fragments.empty() ? 0 : fragments.size() - 1;

                    for (const Fragment& fragment : fragments)
                    {
                        gaps += spaceCount(fragment);
                    }

                    const float slack = lineAvailable - used;

                    if (gaps > 0 && slack > 0.0f)
                    {
                        justifyExtra = slack / static_cast<float>(gaps);
                    }
                }
            }

            const float offset = AlignmentOffset(effectiveAlign, lineAvailable, used);

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
                std::size_t gapsBefore = 0;

                for (std::size_t fragmentIndex = 0; fragmentIndex < fragments.size(); ++fragmentIndex)
                {
                    const Fragment& fragment = fragments[fragmentIndex];
                    const std::size_t spaces = spaceCount(fragment);

                    LineLayout line;
                    line.element = fragment.item->element;
                    line.container = container;
                    line.x = lineX + offset + currentIndent + fragment.x +
                             static_cast<float>(gapsBefore) * justifyExtra;
                    line.width = fragment.width + static_cast<float>(spaces) * justifyExtra;
                    line.baseline = baseline;
                    line.lineTop = lineTop;
                    line.lineHeight = lineHeight;
                    line.lineWidth = used;
                    line.fontSize = fragment.item->fontSize;
                    line.weight = fragment.item->weight;
                    line.letterSpacing = fragment.item->letterSpacing;
                    line.wordSpacing = fragment.item->wordSpacing + justifyExtra;
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
                    line.decorationStyle = fragment.item->decorationStyle;
                    line.decorationThicknessKind = fragment.item->decorationThicknessKind;
                    line.decorationThickness = fragment.item->decorationThickness;
                    line.underlineOffsetKind = fragment.item->underlineOffsetKind;
                    line.underlineOffset = fragment.item->underlineOffset;
                    line.kerning = fragment.item->kerning;
                    line.fontFeatures = fragment.item->fontFeatures;
                    line.yOffset = fragment.item->yOffset;
                    result.lines.push_back(line);

                    gapsBefore += spaces;

                    if (fragmentIndex + 1 < fragments.size())
                    {
                        ++gapsBefore;
                    }
                }
            }

            lineTop += lineHeight;
            usedHeight = lineTop - y;

            // The next line may sit below the float that shortened this one, so
            // its insets are worked out from its own top.
            refreshLineInsets(lineTop);
            penX = lineX;
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
                refreshLineInsets(lineTop);
                continue;
            }

            if (!item.isText)
            {
                if (wrap && penX > lineX && (penX - lineX) + item.width > lineAvailable)
                {
                    flushLine();
                }

                atomics.push_back(AtomicBox{ &item, penX - lineX - currentIndent });
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
                                        ? (wrap ? std::max(1.0f, lineAvailable - (penX - lineX)) : 0.0f)
                                        : (wrap ? lineAvailable : 0.0f);

                TextRunStyle runStyle;
                runStyle.letterSpacing = item.letterSpacing;
                runStyle.wordSpacing = item.wordSpacing;
                runStyle.fontFamily = item.fontFamily;
                runStyle.slant = item.fontStyle;
                runStyle.direction = item.direction;
                runStyle.kerning = item.kerning;
                runStyle.fontFeatures = item.fontFeatures;
                runStyle.tabSize = item.tabSize;
                runStyle.tabSizeLength = item.tabSizeLength;
                runStyle.breakPolicy.overflowWrap = item.overflowWrap;
                runStyle.breakPolicy.wordBreak = item.wordBreak;
                runStyle.breakPolicy.lineBreak = item.lineBreak;
                runStyle.breakPolicy.hyphens = item.hyphens;

                wrappedLines.clear();
                textLayout.Wrap(item.text + offset, item.fontSize, item.weight,
                                item.noWrap ? 0.0f : limit, item.preserveWhitespace, wrappedLines, &runStyle);

                if (wrappedLines.empty())
                {
                    break;
                }

                const TextLine& head = wrappedLines[0];

                if (firstFragment && wrap && penX > lineX && head.width > limit)
                {
                    // Not even the first word fits in the remaining space.
                    flushLine();
                    firstFragment = false;
                    continue;
                }

                fragments.push_back(Fragment{ &item, penX - lineX - currentIndent, head.width,
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

        flushingLastLine = true;
        flushLine();

        lastInlineWidth = widestLine;
        return usedHeight;
    }

    float LayoutEngine::ArrangeVerticalInlineRun(ElementIndex container,
                                                 const CompiledStyle& style,
                                                 const std::vector<InlineItem>& items,
                                                 float x,
                                                 float y,
                                                 float availableWidth,
                                                 std::uint32_t depth,
                                                 bool measureOnly)
    {
        const bool rightToLeftColumns = IsRightToLeftColumnWritingMode(style.writingMode);
        const float columnHeight = textLayout.LineHeight(style.fontSize, style.fontWeight);

        // `vertical-rl` starts at the right edge of the container and walks
        // left; `vertical-lr` starts at the left. A container with no width puts
        // the column at the origin.
        const float columnX = rightToLeftColumns
                                  ? x + std::max(availableWidth, columnHeight) - columnHeight
                                  : x;

        float penY = y;
        float widestColumn = 0.0f;

        for (const InlineItem& item : items)
        {
            if (item.hardBreak)
            {
                penY += textLayout.LineHeight(item.fontSize, item.weight);
                continue;
            }

            if (!item.isText)
            {
                if (!measureOnly)
                {
                    ArrangeElement(item.element, columnX, penY, item.width, Rect{}, depth, false);
                }

                penY += std::max(item.height, 0.0f);
                widestColumn = std::max(widestColumn, item.width);
                continue;
            }

            std::uint32_t textLength = 0;
            while (item.text[textLength] != '\0')
            {
                ++textLength;
            }

            TextRunStyle runStyle;
            runStyle.letterSpacing = item.letterSpacing;
            runStyle.wordSpacing = item.wordSpacing;
            runStyle.fontFamily = item.fontFamily;
            runStyle.slant = item.fontStyle;
            runStyle.direction = item.direction;
            runStyle.kerning = item.kerning;
            runStyle.fontFeatures = item.fontFeatures;
            runStyle.tabSize = item.tabSize;
            runStyle.tabSizeLength = item.tabSizeLength;
            runStyle.breakPolicy.overflowWrap = item.overflowWrap;
            runStyle.breakPolicy.wordBreak = item.wordBreak;
            runStyle.breakPolicy.lineBreak = item.lineBreak;
            runStyle.breakPolicy.hyphens = item.hyphens;

            // The extent down the column is the run's own measured advance: the
            // same metric the paint engine will walk, so the two cannot drift.
            const float extent = textLayout.Measure(item.text, item.fontSize, item.weight, &runStyle);
            const float lineHeight = textLayout.LineHeight(item.fontSize, item.weight);

            if (!measureOnly && textLength > 0)
            {
                LineLayout line;
                line.element = item.element;
                line.container = container;
                line.vertical = true;
                line.textOrientation = style.textOrientation;
                line.x = columnX;
                line.width = lineHeight;
                line.baseline = penY;
                line.lineTop = penY;
                line.lineHeight = extent;
                line.lineWidth = extent;
                line.fontSize = item.fontSize;
                line.weight = item.weight;
                line.letterSpacing = item.letterSpacing;
                line.wordSpacing = item.wordSpacing;
                line.fontFamily = item.fontFamily;
                line.fontStyle = item.fontStyle;
                line.direction = item.direction;
                line.startByte = 0;
                line.endByte = textLength;
                line.placeholder = item.placeholder;
                line.controlValue = item.controlValue;
                line.underline = item.underline;
                line.decoration = item.decoration;
                line.decorationColor = item.decorationColor;
                line.hasDecorationColor = item.hasDecorationColor;
                line.decorationStyle = item.decorationStyle;
                line.decorationThicknessKind = item.decorationThicknessKind;
                line.decorationThickness = item.decorationThickness;
                line.underlineOffsetKind = item.underlineOffsetKind;
                line.underlineOffset = item.underlineOffset;
                line.kerning = item.kerning;
                line.fontFeatures = item.fontFeatures;
                line.yOffset = item.yOffset;
                result.lines.push_back(line);
            }

            penY += extent;
            widestColumn = std::max(widestColumn, lineHeight);
        }

        lastInlineWidth = widestColumn;
        return penY - y;
    }

    // -----------------------------------------------------------------------
    // Block layout
    // -----------------------------------------------------------------------

    Rect LayoutEngine::PlaceFloat(ElementIndex index,
                                 FloatKind side,
                                 float x,
                                 float y,
                                 float availableWidth,
                                 const Rect& clip,
                                 std::uint32_t depth,
                                 const std::vector<FloatBox>& floats)
    {
        // A float's width is its content's max-content width unless a length is
        // declared, so it is laid out shrink-to-fit at the container's left edge
        // first; where it ends up is then a translation of that measurement.
        ArrangeElement(index, x, y, availableWidth, clip, depth, false, true);

        const ElementLayout& measured = result.elements[index];
        const float floatWidth = measured.border.width;

        float targetX = x;
        if (side == FloatKind::Right)
        {
            targetX = x + availableWidth - floatWidth;
        }

        // Floats on the same side stack vertically, and a float that cannot
        // share its row with an opposite-side float drops below it: the top is
        // the lowest bottom of the floats that block this one.
        float targetY = y;
        for (const FloatBox& placed : floats)
        {
            if (placed.box.Bottom() <= targetY)
            {
                continue;
            }

            const bool sameSide = placed.side == side;
            const bool noRoom = placed.side != side && floatWidth + placed.box.width > availableWidth;

            if (sameSide || noRoom)
            {
                targetY = placed.box.Bottom();
            }
        }

        if (targetX != measured.border.x || targetY != measured.border.y)
        {
            ShiftSubtree(index, targetX - measured.border.x, targetY - measured.border.y);
        }

        return result.elements[index].border;
    }

    float LayoutEngine::ClearBelowFloats(ClearKind clear, float y, const std::vector<FloatBox>& floats) const
    {
        float cleared = y;

        for (const FloatBox& placed : floats)
        {
            const bool left = placed.side == FloatKind::Left;
            const bool right = placed.side == FloatKind::Right;
            const bool applies = clear == ClearKind::Both ||
                                 (clear == ClearKind::Left && left) ||
                                 (clear == ClearKind::Right && right);

            if (applies && placed.box.Bottom() > cleared)
            {
                cleared = placed.box.Bottom();
            }
        }

        return cleared;
    }

    void LayoutEngine::FloatInsetsAt(const std::vector<FloatBox>& floats,
                                     float lineTop,
                                     float x,
                                     float availableWidth,
                                     float& leftInset,
                                     float& rightInset) const
    {
        leftInset = 0.0f;
        rightInset = 0.0f;

        const float rightEdge = x + availableWidth;

        for (const FloatBox& placed : floats)
        {
            // A float only reaches the lines its own band crosses; below it the
            // full width comes back.
            if (placed.box.y > lineTop || placed.box.Bottom() <= lineTop)
            {
                continue;
            }

            if (placed.side == FloatKind::Left)
            {
                const float inset = placed.box.Right() - x;
                if (inset > leftInset)
                {
                    leftInset = inset;
                }
            }
            else if (placed.side == FloatKind::Right)
            {
                const float inset = rightEdge - placed.box.x;
                if (inset > rightInset)
                {
                    rightInset = inset;
                }
            }
        }
    }

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

        // The floats placed in this block formatting context, and the lowest
        // point any of them reaches: the container grows to contain its floats
        // even though they take no room in the flow.
        std::vector<FloatBox> floats;
        float floatBottom = y;

        const auto flushRun = [&]() {
            if (inlineRun.empty())
            {
                return;
            }

            cursorY += ArrangeInlineRun(index, x, cursorY, availableWidth, clip, depth, &inlineRun,
                                        measureOnly, &floats);
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

            if (IsOutOfFlow(childStyle.position))
            {
                // An out-of-flow box reserves no space: the cursor does not
                // advance and its margins do not apply, because the page flows
                // as if it were not there. It is still arranged, at the position
                // it would have had, so that `top: auto; left: auto` has a
                // static position to fall back to and so its own content is
                // measured. ApplyPositioning moves it afterwards.
                flushRun();
                ArrangeElement(child, x, cursorY, availableWidth, clip, depth, measureOnly);
            }
            else if (childStyle.floatKind != FloatKind::None)
            {
                // A float is placed against a side of the block and reserves no
                // room in the flow: the cursor does not advance, but the
                // container grows to contain it. During intrinsic measurement it
                // is measured in place, because only painted geometry needs the
                // exclusions.
                flushRun();

                if (measureOnly)
                {
                    ArrangeElement(child, x, cursorY, availableWidth, clip, depth, true, true);
                }
                else
                {
                    cursorY = ClearBelowFloats(childStyle.clearKind, cursorY, floats);
                    const Rect placed = PlaceFloat(child, childStyle.floatKind, x, cursorY, availableWidth,
                                                   clip, depth, floats);
                    floats.push_back(FloatBox{ child, placed, childStyle.floatKind });
                    floatBottom = std::max(floatBottom, placed.Bottom());
                }
            }
            else if (IsBlockDisplay(childStyle.display))
            {
                flushRun();

                // `clear` moves the block below the floats it names before its
                // margins and its own box are placed.
                cursorY = ClearBelowFloats(childStyle.clearKind, cursorY, floats);
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

        // The container's height is the flow's extent or the bottom of its
        // lowest float, whichever is further down.
        return std::max(cursorY - y, floatBottom - y);
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

    float LayoutEngine::ArrangeFlex(ElementIndex index,
                                    float x,
                                    float y,
                                    float availableWidth,
                                    float availableHeight,
                                    const CompiledStyle& style,
                                    const Rect& clip,
                                    std::uint32_t depth,
                                    bool measureOnly)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            lastInlineWidth = 0.0f;
            return 0.0f;
        }

        const FlexDirectionKind direction = style.flexDirection;
        const bool row = direction == FlexDirectionKind::Row || direction == FlexDirectionKind::RowReverse;
        const bool reverse =
            direction == FlexDirectionKind::RowReverse || direction == FlexDirectionKind::ColumnReverse;

        // The main axis is the one items run along; the cross axis is the other.
        const float mainAvailable = row ? availableWidth : availableHeight;
        const float crossAvailable = row ? availableHeight : availableWidth;

        struct FlexItem
        {
            ElementIndex element = NoElement;
            std::int32_t order = 0;
            float baseMain = 0.0f;
            float main = 0.0f;
            float cross = 0.0f;
            Edges margin;
            float grow = 0.0f;
            float shrink = 1.0f;
            bool hasCrossSize = false;
            AlignItemsKind align = AlignItemsKind::Stretch;
        };

        std::vector<FlexItem> items;
        std::vector<ElementIndex> outOfFlow;

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

            if (IsOutOfFlow(childStyle.position) || childStyle.floatKind != FloatKind::None)
            {
                // Neither an absolutely positioned box nor a float is a flex
                // item; both are arranged after the line at the content origin
                // so they have geometry for positioning to build on.
                outOfFlow.push_back(child);
                child = node->nextSibling;
                continue;
            }

            FlexItem item;
            item.element = child;
            item.order = childStyle.flexOrder;
            item.margin = childStyle.margin;
            item.grow = childStyle.flexGrow;
            item.shrink = childStyle.flexShrink;
            item.align = childStyle.hasAlignSelf != 0 ? childStyle.alignSelf : style.alignItems;

            const float declaredMain =
                row ? ResolveDeclaredLength(childStyle.width, availableWidth)
                    : ResolveDeclaredLength(childStyle.height, 0.0f);
            const float declaredCross =
                row ? ResolveDeclaredLength(childStyle.height, 0.0f)
                    : ResolveDeclaredLength(childStyle.width, availableWidth);

            // The intrinsic size is a border box plus its margins, so the
            // margins are taken back out here and added again where they belong.
            const IntrinsicSize intrinsic = MeasureElement(child, 0.0f);
            const float intrinsicMain =
                row ? ClampPositive(intrinsic.width - item.margin.Horizontal())
                    : ClampPositive(intrinsic.height - item.margin.Vertical());
            const float intrinsicCross =
                row ? ClampPositive(intrinsic.height - item.margin.Vertical())
                    : ClampPositive(intrinsic.width - item.margin.Horizontal());

            // A declared `flex-basis` of zero is a real basis, not an absent
            // one: only `auto` (or nothing) falls back to the item's own size.
            bool hasBasis = false;
            float basis = 0.0f;

            if (childStyle.hasFlexBasis != 0 && childStyle.flexBasis.type != LayoutValueType::Auto)
            {
                basis = ResolveDeclaredLength(childStyle.flexBasis, mainAvailable > 0.0f ? mainAvailable : 0.0f);
                hasBasis = true;
            }

            if (hasBasis)
            {
                item.baseMain = basis;
            }
            else if (declaredMain > 0.0f)
            {
                item.baseMain = declaredMain;
            }
            else
            {
                item.baseMain = intrinsicMain;
            }

            if (declaredCross > 0.0f)
            {
                item.cross = declaredCross;
                item.hasCrossSize = true;
            }
            else
            {
                item.cross = intrinsicCross;
            }

            items.push_back(item);
            child = node->nextSibling;
        }

        if (items.empty())
        {
            for (const ElementIndex flow : outOfFlow)
            {
                ArrangeElement(flow, x, y, availableWidth, clip, depth, measureOnly);
            }

            lastInlineWidth = 0.0f;
            return 0.0f;
        }

        // `order` sorts the items; a stable sort keeps tree order for ties.
        std::stable_sort(items.begin(), items.end(), [](const FlexItem& left, const FlexItem& right) {
            return left.order < right.order;
        });

        const auto outerMain = [row](const FlexItem& item) {
            return item.main + (row ? item.margin.Horizontal() : item.margin.Vertical());
        };

        float sumBase = 0.0f;
        float sumGrow = 0.0f;
        float sumShrinkWeight = 0.0f;

        for (const FlexItem& item : items)
        {
            sumBase += item.baseMain + (row ? item.margin.Horizontal() : item.margin.Vertical());
            sumGrow += item.grow;
            sumShrinkWeight += item.shrink * item.baseMain;
        }

        const float free = mainAvailable > 0.0f ? mainAvailable - sumBase : 0.0f;

        for (FlexItem& item : items)
        {
            if (free > 0.0f && sumGrow > 0.0f)
            {
                item.main = item.baseMain + free * (item.grow / sumGrow);
            }
            else if (free < 0.0f && sumShrinkWeight > 0.0f)
            {
                const float scaled = free * (item.shrink * item.baseMain) / sumShrinkWeight;
                item.main = std::max(0.0f, item.baseMain + scaled);
            }
            else
            {
                item.main = item.baseMain;
            }
        }

        float usedMain = 0.0f;
        for (const FlexItem& item : items)
        {
            usedMain += outerMain(item);
        }

        const float leftover = mainAvailable > 0.0f ? std::max(0.0f, mainAvailable - usedMain) : 0.0f;
        const float count = static_cast<float>(items.size());

        float leading = 0.0f;
        float between = 0.0f;

        switch (style.justifyContent)
        {
        case JustifyContentKind::FlexEnd:
            leading = leftover;
            break;
        case JustifyContentKind::Center:
            leading = leftover * 0.5f;
            break;
        case JustifyContentKind::SpaceBetween:
            between = items.size() > 1 ? leftover / (count - 1.0f) : 0.0f;
            break;
        case JustifyContentKind::SpaceAround:
            between = leftover / count;
            leading = between * 0.5f;
            break;
        case JustifyContentKind::SpaceEvenly:
            between = leftover / (count + 1.0f);
            leading = between;
            break;
        case JustifyContentKind::FlexStart:
        default:
            break;
        }

        // The single line's cross size, and whether any item stretches to it.
        float lineCross = 0.0f;
        bool stretches = false;

        for (const FlexItem& item : items)
        {
            lineCross = std::max(lineCross, item.cross + (row ? item.margin.Vertical() : item.margin.Horizontal()));

            if (item.align == AlignItemsKind::Stretch && !item.hasCrossSize)
            {
                stretches = true;
            }
        }

        if (crossAvailable > 0.0f && stretches)
        {
            lineCross = std::max(lineCross, crossAvailable);
        }

        float mainCursor = leading;

        for (const FlexItem& item : items)
        {
            const float marginMainStart = row ? item.margin.left : item.margin.top;
            const float marginMainEnd = row ? item.margin.right : item.margin.bottom;

            float mainOffset = mainCursor + marginMainStart;

            // `*-reverse` lays the line out from the other end: the forward
            // positions are mirrored inside the container's main size.
            if (reverse && mainAvailable > 0.0f)
            {
                mainOffset = mainAvailable - (mainOffset + item.main);
            }

            float crossSize = item.cross;
            if (item.align == AlignItemsKind::Stretch && !item.hasCrossSize && crossAvailable > 0.0f)
            {
                crossSize = std::max(item.cross,
                                     lineCross - (row ? item.margin.Vertical() : item.margin.Horizontal()));
            }

            const float crossOuter = crossSize + (row ? item.margin.Vertical() : item.margin.Horizontal());
            float crossOffset = 0.0f;

            switch (item.align)
            {
            case AlignItemsKind::FlexEnd:
                crossOffset = lineCross - crossOuter;
                break;
            case AlignItemsKind::Center:
                crossOffset = (lineCross - crossOuter) * 0.5f;
                break;
            default:
                crossOffset = 0.0f;
                break;
            }

            float boxX = x;
            float boxY = y;
            float forcedWidth = 0.0f;
            float forcedHeight = 0.0f;

            if (row)
            {
                boxX = x + mainOffset;
                boxY = y + crossOffset + item.margin.top;
                forcedWidth = item.main;

                if (item.align == AlignItemsKind::Stretch && !item.hasCrossSize && crossAvailable > 0.0f)
                {
                    forcedHeight = crossSize;
                }
            }
            else
            {
                boxY = y + mainOffset;
                boxX = x + crossOffset + item.margin.left;
                forcedHeight = item.main;

                if (item.align == AlignItemsKind::Stretch && !item.hasCrossSize && crossAvailable > 0.0f)
                {
                    forcedWidth = crossSize;
                }
            }

            ArrangeElement(item.element, boxX, boxY, row ? item.main : availableWidth, clip, depth,
                           measureOnly, false, forcedWidth, forcedHeight);

            mainCursor += item.main + marginMainStart + marginMainEnd + between;
        }

        for (const ElementIndex flow : outOfFlow)
        {
            ArrangeElement(flow, x, y, availableWidth, clip, depth, measureOnly);
        }

        lastInlineWidth = row ? usedMain : lineCross;

        // The content height is the line's cross size for a row, and the main
        // extent for a column.
        return row ? lineCross : usedMain;
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
                                       bool measureOnly,
                                       bool shrinkToFit,
                                       float forcedWidth,
                                       float forcedHeight)
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
        // A float never fills its container: its width is its content's
        // max-content width unless it declares one.
        const bool fillsWidth = blockLevel && availableWidth > 0.0f && !shrinkToFit;

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

        // A flex item's main or stretched cross size is decided by the flex
        // arranger, which has the surrounding items to distribute space between.
        if (forcedWidth > 0.0f)
        {
            boxWidth = forcedWidth;
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
        else if (style.display == DisplayKind::Flex)
        {
            // A flex container arranges its children along an axis rather than
            // stacking them; the declared inner height, when there is one, is
            // what the cross axis stretches to.
            const float declaredContentHeight =
                declaredHeight > 0.0f ? ClampPositive(declaredHeight - extraHeight) : 0.0f;
            contentHeight = ArrangeFlex(index, contentX, contentY, contentWidth, declaredContentHeight,
                                        style, clip, depth + 1, measureOnly);
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

        if (forcedHeight > 0.0f)
        {
            boxHeight = forcedHeight;
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
