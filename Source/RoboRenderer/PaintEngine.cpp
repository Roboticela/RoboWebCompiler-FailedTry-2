#include "RoboRenderer/Paint.h"
#include "RoboRenderer/Bidi.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

namespace RoboRenderer
{
    namespace
    {
        // The same neutral palette the native default styles use, so a control
        // painted from runtime state and a control painted from style agree.
        constexpr Color TextColor{ 15, 23, 42, 255 };
        constexpr Color PlaceholderColor{ 148, 163, 184, 255 };
        constexpr Color BorderColor{ 203, 213, 225, 255 };
        constexpr Color SurfaceColor{ 255, 255, 255, 255 };
        constexpr Color RaisedColor{ 226, 232, 240, 255 };
        constexpr Color HeaderColor{ 241, 245, 249, 255 };
        constexpr Color AccentColor{ 37, 99, 235, 255 };
        constexpr Color AccentMutedColor{ 147, 179, 240, 255 };
        constexpr Color TrackColor{ 226, 232, 240, 255 };
        constexpr Color LinkColor{ 37, 99, 235, 255 };
        constexpr Color DisabledColor{ 241, 245, 249, 255 };
        constexpr Color DisabledTextColor{ 148, 163, 184, 255 };
        constexpr Color SelectionColor{ 191, 219, 254, 255 };
        constexpr Color FocusRingColor{ 59, 130, 246, 255 };
        constexpr Color SuccessColor{ 22, 163, 74, 255 };
        constexpr Color WarningColor{ 217, 119, 6, 255 };
        constexpr Color DangerColor{ 220, 38, 38, 255 };

        Rect Translated(const Rect& rect, float offsetX, float offsetY)
        {
            return rect.Translated(offsetX, offsetY);
        }

        float Clamp(float value, float minimum, float maximum)
        {
            return value < minimum ? minimum : (value > maximum ? maximum : value);
        }

        /// A rectangle covering one thickened segment. Segments are how the paint
        /// engine draws the few shapes that are not rectangles - an arrow, a tick -
        /// without needing a path object: at control sizes a short stepped
        /// diagonal reads as a diagonal.
        void EmitSegment(PaintList& list,
                         float x0,
                         float y0,
                         float x1,
                         float y1,
                         float thickness,
                         const Color& color,
                         ElementIndex element)
        {
            const float left = std::min(x0, x1) - thickness * 0.5f;
            const float top = std::min(y0, y1) - thickness * 0.5f;
            const float right = std::max(x0, x1) + thickness * 0.5f;
            const float bottom = std::max(y0, y1) + thickness * 0.5f;

            list.Add(PaintCommandType::FillRect, Rect{ left, top, right - left, bottom - top }, element).color =
                color;
        }

        void EmitSteppedLine(PaintList& list,
                             float x0,
                             float y0,
                             float x1,
                             float y1,
                             float thickness,
                             int steps,
                             const Color& color,
                             ElementIndex element)
        {
            steps = steps < 2 ? 2 : steps;

            for (int step = 0; step < steps; ++step)
            {
                const float t0 = static_cast<float>(step) / static_cast<float>(steps);
                const float t1 = static_cast<float>(step + 1) / static_cast<float>(steps);

                EmitSegment(list,
                            x0 + (x1 - x0) * t0,
                            y0 + (y1 - y0) * t0,
                            x0 + (x1 - x0) * t1,
                            y0 + (y1 - y0) * t1,
                            thickness,
                            color,
                            element);
            }
        }

        /// A filled triangle pointing down: the select control's arrow, and the
        /// marker of an open disclosure.
        void EmitDownTriangle(PaintList& list,
                              float centerX,
                              float top,
                              float width,
                              float height,
                              const Color& color,
                              ElementIndex element)
        {
            const float rowHeight = height <= 2.0f ? 1.0f : std::max(1.0f, std::floor(height / 2.0f));
            const float rows = height <= 2.0f ? 2.0f : std::floor(height / rowHeight);

            for (float row = 0.0f; row < rows; row += 1.0f)
            {
                const float rowWidth = width * (rows - row) / rows;
                list.Add(PaintCommandType::FillRect,
                         Rect{ centerX - rowWidth * 0.5f, top + row * rowHeight, rowWidth, rowHeight },
                         element)
                    .color = color;
            }
        }

        /// A filled triangle pointing right: the marker of a closed disclosure.
        void EmitRightTriangle(PaintList& list,
                               float left,
                               float centerY,
                               float width,
                               float height,
                               const Color& color,
                               ElementIndex element)
        {
            const float columnWidth = width <= 2.0f ? 1.0f : std::max(1.0f, std::floor(width / 2.0f));
            const float columns = width <= 2.0f ? 2.0f : std::floor(width / columnWidth);

            for (float column = 0.0f; column < columns; column += 1.0f)
            {
                const float columnHeight = height * (columns - column) / columns;
                list.Add(PaintCommandType::FillRect,
                         Rect{ left + column * columnWidth, centerY - columnHeight * 0.5f, columnWidth,
                               columnHeight },
                         element)
                    .color = color;
            }
        }

        /// The tick inside a checked box, drawn in the box's own white. The two
        /// arms run through the lower half of the box so the accent fill still
        /// reads at the centre: a control this small has no room for a tick that
        /// covers its middle and leaves no colour visible at all.
        void EmitCheckMark(PaintList& list, const Rect& box, float inset, ElementIndex element)
        {
            const Rect inner{ box.x + inset, box.y + inset, box.width - inset * 2.0f, box.height - inset * 2.0f };
            if (inner.IsEmpty())
            {
                return;
            }

            const float thickness = std::max(1.0f, inner.height * 0.16f);

            const float vertexX = inner.x + inner.width * 0.36f;
            const float vertexY = inner.y + inner.height * 0.94f;

            EmitSteppedLine(list,
                            inner.x + inner.width * 0.04f,
                            inner.y + inner.height * 0.50f,
                            vertexX,
                            vertexY,
                            thickness,
                            4,
                            SurfaceColor,
                            element);
            EmitSteppedLine(list,
                            vertexX,
                            vertexY,
                            inner.x + inner.width * 0.92f,
                            inner.y + inner.height * 0.42f,
                            thickness,
                            6,
                            SurfaceColor,
                            element);
        }

        const char* LowerAlpha(unsigned value, char* buffer)
        {
            unsigned remaining = value == 0 ? 1u : value;
            char digits[16];
            int count = 0;

            while (remaining > 0 && count < 16)
            {
                digits[count++] = static_cast<char>('a' + (remaining - 1) % 26);
                remaining = (remaining - 1) / 26;
            }

            int index = 0;
            for (int position = count - 1; position >= 0; --position)
            {
                buffer[index++] = digits[position];
            }

            buffer[index] = '\0';
            return buffer;
        }

        const char* UpperAlpha(unsigned value, char* buffer)
        {
            LowerAlpha(value, buffer);

            for (char* cursor = buffer; *cursor != '\0'; ++cursor)
            {
                if (*cursor >= 'a' && *cursor <= 'z')
                {
                    *cursor = static_cast<char>(*cursor - 'a' + 'A');
                }
            }

            return buffer;
        }

        const char* RomanNumerals(unsigned value, char* buffer, bool upper)
        {
            static const char* const Table[13] = { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV",
                                                   "I" };
            static const unsigned Values[13] = { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 };

            unsigned remaining = value == 0 ? 1u : value;
            int written = 0;

            for (int index = 0; index < 13 && remaining > 0; ++index)
            {
                while (remaining >= Values[index] && written < 20)
                {
                    for (const char* letter = Table[index]; *letter != '\0' && written < 20; ++letter)
                    {
                        buffer[written++] =
                            upper ? *letter : static_cast<char>(*letter - 'A' + 'a');
                    }

                    remaining -= Values[index];
                }
            }

            if (written == 0)
            {
                buffer[written++] = '0';
            }

            buffer[written] = '\0';
            return buffer;
        }

        /// Parses the "#rgb" / "#rrggbb" a color input carries. Anything else
        /// stays white, which is the value an unspecified color input has.
        Color ParseColorValue(const char* text)
        {
            if (text == nullptr || text[0] != '#')
            {
                return SurfaceColor;
            }

            const auto hex = [](char value) -> int {
                if (value >= '0' && value <= '9')
                {
                    return value - '0';
                }
                if (value >= 'a' && value <= 'f')
                {
                    return value - 'a' + 10;
                }
                if (value >= 'A' && value <= 'F')
                {
                    return value - 'A' + 10;
                }
                return -1;
            };

            const std::size_t length = std::char_traits<char>::length(text);

            if (length == 4)
            {
                const int red = hex(text[1]);
                const int green = hex(text[2]);
                const int blue = hex(text[3]);

                if (red < 0 || green < 0 || blue < 0)
                {
                    return SurfaceColor;
                }

                return Color::Rgba(static_cast<std::uint8_t>(red * 17),
                                   static_cast<std::uint8_t>(green * 17),
                                   static_cast<std::uint8_t>(blue * 17));
            }

            if (length >= 7)
            {
                const int channels[3][2] = { { hex(text[1]), hex(text[2]) },
                                             { hex(text[3]), hex(text[4]) },
                                             { hex(text[5]), hex(text[6]) } };
                std::uint8_t values[3];

                for (int index = 0; index < 3; ++index)
                {
                    if (channels[index][0] < 0 || channels[index][1] < 0)
                    {
                        return SurfaceColor;
                    }

                    values[index] = static_cast<std::uint8_t>(channels[index][0] * 16 + channels[index][1]);
                }

                return Color::Rgba(values[0], values[1], values[2]);
            }

            return SurfaceColor;
        }

        /// The string an element paints. It resolves exactly the way the layout
        /// engine resolved it, so the byte ranges the line fragments carry still
        /// address the same characters.
        const char* PaintTextFor(const CompiledDocument& document,
                                 const UiState& state,
                                 ElementIndex index,
                                 bool& isPlaceholder,
                                 bool& isControlValue)
        {
            isPlaceholder = false;
            isControlValue = false;

            const CompiledElement* element = ElementAt(document, index);
            if (element == nullptr)
            {
                return "";
            }

            const RenderCategory category = CategoryFor(element->kind);

            if (category == RenderCategory::Input || category == RenderCategory::TextArea)
            {
                isControlValue = true;

                const InputFieldState* field = state.Input(index);
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
                return SelectedOptionLabel(document, state, index);
            }

            if (category == RenderCategory::FileInput)
            {
                isControlValue = true;
                const InputFieldState* field = state.Input(index);
                return field != nullptr && !field->value.empty() ? field->value.c_str() : "";
            }

            if (category == RenderCategory::Option && element->auxiliaryText != 0)
            {
                return StringAt(document, element->auxiliaryText);
            }

            return StringAt(document, element->text);
        }
    }

    const char* ToString(PaintCommandType type)
    {
        switch (type)
        {
        case PaintCommandType::FillRect: return "FillRect";
        case PaintCommandType::FillRoundedRect: return "FillRoundedRect";
        case PaintCommandType::StrokeRect: return "StrokeRect";
        case PaintCommandType::StrokeRoundedRect: return "StrokeRoundedRect";
        case PaintCommandType::DrawImage: return "DrawImage";
        case PaintCommandType::DrawText: return "DrawText";
        case PaintCommandType::PushClip: return "PushClip";
        case PaintCommandType::PopClip: return "PopClip";
        case PaintCommandType::PushTransform: return "PushTransform";
        case PaintCommandType::PopTransform: return "PopTransform";
        }

        return "Unknown";
    }

    // -----------------------------------------------------------------------
    // PaintList
    // -----------------------------------------------------------------------

    void PaintList::Clear()
    {
        commands.clear();
        glyphs.clear();
        bounds = Rect{};
    }

    void PaintList::Reserve(std::size_t commandCount, std::size_t glyphCount)
    {
        commands.reserve(commandCount);
        glyphs.reserve(glyphCount);
    }

    PaintCommand& PaintList::Add(PaintCommandType type, const Rect& commandBounds, ElementIndex element)
    {
        PaintCommand command;
        command.type = type;
        command.bounds = commandBounds;
        command.element = element;

        commands.push_back(command);

        // Only painted commands extend the repaint area: a clip or a transform
        // covers no pixels by itself.
        if (type != PaintCommandType::PushClip && type != PaintCommandType::PopClip &&
            type != PaintCommandType::PushTransform && type != PaintCommandType::PopTransform)
        {
            bounds = bounds.Union(commandBounds);
        }

        return commands.back();
    }

    std::uint32_t PaintList::AddGlyph(const PaintGlyph& glyph)
    {
        const std::uint32_t index = static_cast<std::uint32_t>(glyphs.size());
        glyphs.push_back(glyph);
        return index;
    }

    // -----------------------------------------------------------------------
    // PaintEngine
    // -----------------------------------------------------------------------

    PaintEngine::PaintEngine(const CompiledDocument& compiledDocument,
                             const LayoutEngine& layoutEngine,
                             UiState& uiState,
                             StyleProvider& styleProvider)
        : document(compiledDocument)
        , layout(layoutEngine)
        , state(uiState)
        , styles(styleProvider)
    {
        elementDirty.assign(document.elementCount, 0);
        dirtyRegion = Rect{ 0.0f, 0.0f, layout.Viewport().logicalWidth, layout.Viewport().logicalHeight };
    }

    void PaintEngine::Invalidate(ElementIndex index)
    {
        if (index >= elementDirty.size())
        {
            InvalidateAll();
            return;
        }

        elementDirty[index] = 1;
        dirty = true;
    }

    void PaintEngine::InvalidateAll()
    {
        std::fill(elementDirty.begin(), elementDirty.end(), static_cast<std::uint8_t>(0));
        dirty = true;
        fullRepaint = true;
    }

    ControlVisualState PaintEngine::VisualStateOf(ElementIndex index) const
    {
        const ElementState& elementState = state.Element(index);

        if (elementState.HasFlag(RuntimeFlagDisabled))
        {
            return ControlVisualState::Disabled;
        }

        if (elementState.HasFlag(RuntimeFlagPressed))
        {
            return ControlVisualState::Pressed;
        }

        if (elementState.HasFlag(RuntimeFlagHovered))
        {
            return ControlVisualState::Hovered;
        }

        if (elementState.HasFlag(RuntimeFlagFocused))
        {
            return ControlVisualState::Focused;
        }

        return ControlVisualState::Normal;
    }

    void PaintEngine::ScrollOffsetOf(ElementIndex index, float& offsetX, float& offsetY) const
    {
        offsetX = 0.0f;
        offsetY = 0.0f;

        const ElementLayout* box = layout.Result().Element(index);
        if (box == nullptr || (box->flags & LayoutFlagScrolls) == 0)
        {
            return;
        }

        const ElementState& elementState = state.Element(index);
        const float maxX = std::max(0.0f, box->scrollWidth - box->content.width);
        const float maxY = std::max(0.0f, box->scrollHeight - box->content.height);

        offsetX = Clamp(elementState.scrollX, 0.0f, maxX);
        offsetY = Clamp(elementState.scrollY, 0.0f, maxY);
    }

    Rect PaintEngine::SubtreeBounds(ElementIndex index) const
    {
        const ElementLayout* box = layout.Result().Element(index);
        if (box == nullptr)
        {
            return Rect{};
        }

        Rect area = box->border;

        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return area;
        }

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            area = area.Union(SubtreeBounds(child));

            const CompiledElement* childElement = ElementAt(document, child);
            child = childElement != nullptr ? childElement->nextSibling : NoElement;
        }

        return area;
    }

    void PaintEngine::Build()
    {
        const auto start = std::chrono::steady_clock::now();

        const RenderTargetInfo& viewport = layout.Viewport();
        const Rect viewportRect{ 0.0f, 0.0f, viewport.logicalWidth, viewport.logicalHeight };

        list.Clear();
        list.Reserve(document.elementCount * 3 + 16, 128);

        // The region that has to be redrawn is the union of the subtrees that
        // were explicitly invalidated. The input layer's per-element dirty flag
        // is consulted for nothing here: the renderer turns an event that
        // changed pixels into one Invalidate call on the element it targeted,
        // which keeps a focus or hover change from repainting its ancestors - or
        // the whole window - for no reason.
        Rect region;
        bool regionKnown = false;

        if (fullRepaint)
        {
            region = viewportRect;
            regionKnown = true;
        }
        else
        {
            for (ElementIndex index = 0; index < document.elementCount; ++index)
            {
                ElementState& elementState = state.Element(index);

                if (elementState.HasFlag(RuntimeFlagPaintDirty))
                {
                    elementState.ClearFlag(RuntimeFlagPaintDirty);
                }

                if (elementDirty[index] == 0)
                {
                    continue;
                }

                const Rect area = SubtreeBounds(index);
                region = regionKnown ? region.Union(area) : area;
                regionKnown = true;

                elementDirty[index] = 0;
            }

            if (!regionKnown)
            {
                region = viewportRect;
            }
        }

        dirtyRegion = region.Intersection(viewportRect);

        stats.elementCount = 0;
        stats.textRunCount = 0;
        stats.imageCount = 0;

        PaintElement(document.root, 0.0f, 0.0f, viewportRect);

        stats.buildTimeMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        stats.commandCount = static_cast<std::uint32_t>(list.CommandCount());
        stats.glyphCount = static_cast<std::uint32_t>(list.GlyphCount());

        dirty = false;
        fullRepaint = false;
    }

    void PaintEngine::PaintElement(ElementIndex index, float originX, float originY, const Rect& clip)
    {
        const CompiledElement* element = ElementAt(document, index);
        const ElementLayout* box = layout.Result().Element(index);

        if (element == nullptr || box == nullptr || (box->flags & LayoutFlagDisplayNone) != 0)
        {
            return;
        }

        const RenderCategory category = CategoryFor(element->kind);
        if (category == RenderCategory::None || category == RenderCategory::UnsupportedNamespace)
        {
            // A category the runtime does not paint is a diagnostic, not a crash:
            // the compiler reports the same element under its own error code.
            return;
        }

        ++stats.elementCount;

        const CompiledStyle& style = styles.StyleFor(document, index, category);

        PaintBackground(index, *box, style, *element, originX, originY);

        switch (category)
        {
        case RenderCategory::Image:
            PaintImage(index, *box, style, *element, originX, originY);
            break;
        case RenderCategory::Canvas:
            PaintCanvas(index, *box, style, *element, originX, originY);
            break;
        case RenderCategory::Media:
        case RenderCategory::Frame:
        case RenderCategory::EmbeddedObject:
            PaintMedia(index, *box, style, *element, category, originX, originY);
            break;
        case RenderCategory::Separator:
            PaintSeparator(index, *box, style, *element, originX, originY);
            break;
        case RenderCategory::ListItem:
            PaintListMarker(index, *box, style, *element, originX, originY);
            break;
        case RenderCategory::DisclosureSummary:
            PaintDisclosureMarker(index, *box, style, *element, originX, originY);
            break;
        case RenderCategory::Table:
        case RenderCategory::TableSection:
        case RenderCategory::TableRow:
        case RenderCategory::TableCell:
            PaintTableParts(index, *box, style, *element, category, originX, originY);
            break;
        case RenderCategory::Button:
        case RenderCategory::Input:
        case RenderCategory::TextArea:
        case RenderCategory::Select:
        case RenderCategory::Checkbox:
        case RenderCategory::Radio:
        case RenderCategory::Range:
        case RenderCategory::Progress:
        case RenderCategory::Meter:
        case RenderCategory::ColorInput:
        case RenderCategory::FileInput:
            PaintControl(index, *box, style, *element, category, originX, originY);
            break;
        default:
            break;
        }

        PaintBorder(index, *box, style, originX, originY);
        PaintText(index, originX, originY);

        // A focus ring sits outside the border so it can never hide it.
        const ElementState& elementState = state.Element(index);
        if (elementState.HasFlag(RuntimeFlagFocused) && elementState.HasFlag(RuntimeFlagFocusVisible))
        {
            const Rect ring = Translated(box->border, originX, originY).Expanded(2.0f);
            PaintCommand& command = list.Add(PaintCommandType::StrokeRoundedRect, ring, index);
            command.color = FocusRingColor;
            command.radius = std::max(style.borderRadius, 2.0f) + 2.0f;
            command.edges = Edges::Uniform(2.0f);
        }

        // Children are painted through the element's scroll translation, which is
        // the only thing a scroll changes: no geometry is recomputed for it.
        float scrollX = 0.0f;
        float scrollY = 0.0f;
        ScrollOffsetOf(index, scrollX, scrollY);

        if (index == document.root)
        {
            scrollX += state.ScrollX();
            scrollY += state.ScrollY();
        }

        const float childOriginX = originX - scrollX;
        const float childOriginY = originY - scrollY;

        Rect childClip = clip;
        bool clipping = false;

        if ((box->flags & (LayoutFlagClips | LayoutFlagScrolls)) != 0)
        {
            // The clip travels with the subtree, so the clip command carries the
            // same translation the children were given.
            list.Add(PaintCommandType::PushClip, box->clip.Translated(childOriginX, childOriginY), index);
            childClip = box->clip.Intersection(clip);
            clipping = true;
        }

        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* childElement = ElementAt(document, child);

            if (childElement != nullptr &&
                styles.StyleFor(document, child, CategoryFor(childElement->kind)).position ==
                    PositionKind::Fixed)
            {
                // A fixed box is positioned against the viewport, so neither a
                // scroll nor a clip from any ancestor applies to it. Its layout
                // geometry is already in viewport coordinates, which is what an
                // origin of zero means, and its clip is the viewport itself.
                const RenderTargetInfo& target = layout.Viewport();
                PaintElement(child, 0.0f, 0.0f,
                             Rect{ 0.0f, 0.0f, target.logicalWidth, target.logicalHeight });
            }
            else
            {
                PaintElement(child, childOriginX, childOriginY, childClip);
            }

            child = childElement != nullptr ? childElement->nextSibling : NoElement;
        }

        if (clipping)
        {
            list.Add(PaintCommandType::PopClip, box->clip, index);
        }
    }

    void PaintEngine::PaintBackground(ElementIndex index,
                                      const ElementLayout& box,
                                      const CompiledStyle& style,
                                      const CompiledElement& element,
                                      float originX,
                                      float originY)
    {
        if (style.hasBackground == 0 || style.background.alpha == 0)
        {
            return;
        }

        (void)element;

        const Rect area = Translated(box.border, originX, originY);
        Color color = style.background;

        if (style.borderRadius > 0.0f)
        {
            PaintCommand& command = list.Add(PaintCommandType::FillRoundedRect, area, index);
            command.color = color;
            command.radius = style.borderRadius;
        }
        else
        {
            list.Add(PaintCommandType::FillRect, area, index).color = color;
        }
    }

    void PaintEngine::PaintBorder(ElementIndex index,
                                  const ElementLayout& box,
                                  const CompiledStyle& style,
                                  float originX,
                                  float originY)
    {
        if (style.hasBorder == 0 || style.borderWidth.Horizontal() <= 0.0f)
        {
            return;
        }

        const Rect area = Translated(box.border, originX, originY);

        if (style.borderRadius > 0.0f)
        {
            PaintCommand& command = list.Add(PaintCommandType::StrokeRoundedRect, area, index);
            command.color = style.borderColor;
            command.radius = style.borderRadius;
            command.edges = style.borderWidth;
        }
        else
        {
            PaintCommand& command = list.Add(PaintCommandType::StrokeRect, area, index);
            command.color = style.borderColor;
            command.edges = style.borderWidth;
        }
    }

    void PaintEngine::EmitTextRun(ElementIndex index,
                                  float x,
                                  float baseline,
                                  const char* text,
                                  std::uint32_t startByte,
                                  std::uint32_t endByte,
                                  float fontSize,
                                  FontWeightKind weight,
                                  const Color& color,
                                  const TextRunStyle* runStyle,
                                  bool vertical,
                                  TextOrientationKind orientation)
    {
        if (text == nullptr || fontSize <= 0.0f || startByte >= endByte)
        {
            return;
        }

        const TextLayout& textLayout = layout.Text();

        // Shape the run in logical order. The face's own `GSUB`/`GPOS` decide
        // ligatures, kerning and contextual forms; a run whose face cannot shape
        // comes back one glyph per code point, and a fallback chain introduces a
        // new run every time the covering face changes.
        std::vector<PositionedGlyph> glyphs;
        textLayout.ShapeRun(text, startByte, endByte, fontSize, weight, glyphs, runStyle);

        if (glyphs.empty())
        {
            return;
        }

        // A stated `direction` wins; otherwise the first strong character
        // decides. A right-to-left run is reversed for display after shaping, so
        // the letters were still shaped in the order they were written.
        bool rightToLeft = false;

        if (runStyle != nullptr && runStyle->direction == TextDirectionKind::RightToLeft)
        {
            rightToLeft = true;
        }
        else if (runStyle == nullptr || runStyle->direction == TextDirectionKind::Auto)
        {
            for (const PositionedGlyph& glyph : glyphs)
            {
                if (IsRtlCodepoint(glyph.codepoint))
                {
                    rightToLeft = true;
                    break;
                }
                if (IsLtrCodepoint(glyph.codepoint))
                {
                    break;
                }
            }
        }

        if (rightToLeft)
        {
            std::reverse(glyphs.begin(), glyphs.end());
        }

        // A vertical line walks the pen down the column instead of along the
        // baseline; each glyph keeps its orientation, which is what
        // `text-orientation: upright` asks for.
        float pen = vertical ? baseline : x;
        const std::uint32_t firstGlyph = static_cast<std::uint32_t>(list.GlyphCount());
        std::uint32_t glyphCount = 0;

        for (const PositionedGlyph& positioned : glyphs)
        {
            if (positioned.codepoint != ' ' && positioned.codepoint != 0x00A0 &&
                positioned.codepoint != '\t')
            {
                PaintGlyph glyph;

                if (vertical)
                {
                    glyph.x = x + positioned.xOffset;
                    glyph.y = pen + positioned.yOffset + fontSize * 0.75f;

                    // `upright` stands every glyph up; `sideways` turns every
                    // one; `mixed` stands up the wide scripts and turns the
                    // rest, which is the rule the reference states.
                    const bool standsUpright =
                        orientation == TextOrientationKind::Upright ||
                        (orientation == TextOrientationKind::Mixed &&
                         IsCjkCodepoint(positioned.codepoint));
                    glyph.rotation = standsUpright ? 0 : 1;
                }
                else
                {
                    glyph.x = pen + positioned.xOffset;
                    glyph.y = baseline - positioned.yOffset;
                }

                glyph.codepoint = positioned.codepoint;
                glyph.faceId = positioned.faceIndex;
                glyph.glyphId = positioned.glyphId;
                list.AddGlyph(glyph);
                ++glyphCount;
            }

            pen += positioned.advance;
        }

        if (glyphCount == 0)
        {
            return;
        }

        const Rect commandBounds = vertical ? Rect{ x, baseline, fontSize, pen - baseline }
                                            : Rect{ x, baseline, pen - x, fontSize };
        PaintCommand& command = list.Add(PaintCommandType::DrawText, commandBounds, index);
        command.color = color;
        command.fontSize = fontSize;
        command.weight = weight;
        command.firstGlyph = firstGlyph;
        command.glyphCount = glyphCount;

        ++stats.textRunCount;
    }

    void PaintEngine::PaintText(ElementIndex index, float originX, float originY)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return;
        }

        const LayoutResult& result = layout.Result();
        const ElementLayout* box = result.Element(index);
        if (box == nullptr || box->lineCount == 0)
        {
            return;
        }

        const RenderCategory category = CategoryFor(element->kind);
        const CompiledStyle& style = styles.StyleFor(document, index, category);
        const ElementState& elementState = state.Element(index);

        bool isPlaceholder = false;
        bool isControlValue = false;
        const char* text = PaintTextFor(document, state, index, isPlaceholder, isControlValue);

        if (text == nullptr || text[0] == '\0')
        {
            return;
        }

        // The same transform the layout pass applied, so the byte offsets the
        // line recorded still name the same characters here.
        std::string transformed;
        if (IsSpecified(style.specified, StyleFlagTextTransform) &&
            style.textTransform != TextTransformKind::None)
        {
            transformed = ApplyTextTransform(text, style.textTransform);
            text = transformed.c_str();
        }

        Color color = TextColor;

        if (elementState.HasFlag(RuntimeFlagDisabled))
        {
            color = DisabledTextColor;
        }
        else if (isPlaceholder)
        {
            color = PlaceholderColor;
        }
        else if (style.hasForeground != 0)
        {
            color = style.foreground;
        }
        else if (category == RenderCategory::Link)
        {
            color = LinkColor;
        }

        const bool isTextControl = category == RenderCategory::Input || category == RenderCategory::TextArea;
        const InputFieldState* field = isTextControl ? state.Input(index) : nullptr;
        const bool focused = elementState.HasFlag(RuntimeFlagFocused);

        result.ForEachLine(index, [&](const LineLayout& line) {
            const float lineX = line.x + originX;
            const float baseline = line.baseline + originY + line.yOffset;
            const float top = line.lineTop + originY + line.yOffset;

            TextRunStyle runStyle;
            runStyle.letterSpacing = line.letterSpacing;
            runStyle.wordSpacing = line.wordSpacing;
            runStyle.fontFamily = line.fontFamily;
            runStyle.slant = line.fontStyle;
            runStyle.direction = line.direction;
            runStyle.kerning = line.kerning;
            runStyle.fontFeatures = line.fontFeatures;

            // A selection paints under the glyphs; a caret paints over them. The
            // band a selection covers runs along the line, so a vertical line
            // covers a band down its column instead.
            if (field != nullptr && focused && field->HasSelection() && line.controlValue)
            {
                const std::uint32_t startByte = TextLayout::ByteOffsetForCodepoint(text, field->SelectionStart());
                const std::uint32_t endByte = TextLayout::ByteOffsetForCodepoint(text, field->SelectionEnd());

                if (endByte > startByte)
                {
                    const float selectionStart =
                        layout.Text().MeasurePrefix(text, startByte, line.fontSize, line.weight, &runStyle);
                    const float selectionEnd =
                        layout.Text().MeasurePrefix(text, endByte, line.fontSize, line.weight, &runStyle);

                    if (line.vertical)
                    {
                        list.Add(PaintCommandType::FillRect,
                                 Rect{ lineX, top + selectionStart, line.width, selectionEnd - selectionStart },
                                 index)
                            .color = SelectionColor;
                    }
                    else
                    {
                        list.Add(PaintCommandType::FillRect,
                                 Rect{ lineX + selectionStart, top, selectionEnd - selectionStart,
                                       line.lineHeight },
                                 index)
                            .color = SelectionColor;
                    }
                }
            }

            EmitTextRun(index, lineX, line.vertical ? top : baseline, text, line.startByte, line.endByte,
                        line.fontSize, line.weight, color, &runStyle, line.vertical, line.textOrientation);

            // The decoration colour is the element's own colour unless the
            // document named one: that is what `currentColor` means, and the
            // link underline rides the same bits.
            std::uint8_t decoration = line.decoration;

            if (line.underline)
            {
                decoration |= TextDecorationUnderline;
            }

            if (decoration != 0u)
            {
                const Color decorationColor = line.hasDecorationColor ? line.decorationColor : color;

                float thickness = std::max(1.0f, line.fontSize * 0.07f);

                if (line.decorationThicknessKind == TextDecorationThicknessKind::FromFont)
                {
                    // `from-font` reads the face's own `post` metrics rather than
                    // a fraction of the font size.
                    thickness = layout.Text().UnderlineThickness(line.fontSize, line.weight, &runStyle);
                }
                else if (line.decorationThicknessKind == TextDecorationThicknessKind::Length &&
                         line.decorationThickness > 0.0f)
                {
                    thickness = line.decorationThickness;
                }

                const TextDecorationStyleKind decorationStyle = line.decorationStyle;
                const float decorationWidth = std::max(line.width, 1.0f);

                const auto drawDecoration = [&](float x, float y, float width, float strokeThickness) {
                    switch (decorationStyle)
                    {
                    case TextDecorationStyleKind::Solid:
                        list.Add(PaintCommandType::FillRect, Rect{ x, y, width, strokeThickness }, index).color =
                            decorationColor;
                        break;

                    case TextDecorationStyleKind::Double:
                    {
                        // Two thin lines with a gap between them.
                        const float line = std::max(1.0f, strokeThickness * 0.4f);
                        const float gap = std::max(1.0f, strokeThickness * 0.4f);
                        list.Add(PaintCommandType::FillRect, Rect{ x, y, width, line }, index).color =
                            decorationColor;
                        list.Add(PaintCommandType::FillRect, Rect{ x, y + line + gap, width, line }, index).color =
                            decorationColor;
                        break;
                    }

                    case TextDecorationStyleKind::Dotted:
                    {
                        const float dot = std::max(1.0f, strokeThickness);
                        const float step = dot * 2.0f;

                        for (float offset = 0.0f; offset < width; offset += step)
                        {
                            const float dotWidth = std::min(dot, width - offset);
                            list.Add(PaintCommandType::FillRect, Rect{ x + offset, y, dotWidth, dot }, index).color =
                                decorationColor;
                        }
                        break;
                    }

                    case TextDecorationStyleKind::Dashed:
                    {
                        const float dash = std::max(2.0f, strokeThickness * 3.0f);
                        const float step = dash * 2.0f;

                        for (float offset = 0.0f; offset < width; offset += step)
                        {
                            const float dashWidth = std::min(dash, width - offset);
                            list.Add(PaintCommandType::FillRect,
                                     Rect{ x + offset, y, dashWidth, strokeThickness }, index)
                                .color = decorationColor;
                        }
                        break;
                    }

                    case TextDecorationStyleKind::Wavy:
                    {
                        // A staircase stands in for the wave: each quarter
                        // wavelength steps up or down, which reads as a wave at
                        // the size a text decoration is drawn at.
                        const float amplitude = std::max(1.0f, strokeThickness);
                        const float quarter = std::max(2.0f, strokeThickness * 2.0f);
                        int phase = 0;

                        for (float offset = 0.0f; offset < width; offset += quarter, ++phase)
                        {
                            const float level = ((phase / 2) % 2 == 0) ? 0.0f : amplitude;
                            const float piece = std::min(quarter, width - offset);
                            list.Add(PaintCommandType::FillRect,
                                     Rect{ x + offset, y + level, piece, strokeThickness }, index)
                                .color = decorationColor;
                        }
                        break;
                    }
                    }
                };

                // A vertical line's decoration runs down the column: the
                // underline on the far side, the overline on the near side and a
                // strikethrough through the middle. Which physical side each
                // falls on is what the glyphs' quarter turn makes of the
                // baseline's above and below.
                const auto drawVerticalDecoration = [&](float barX, float barY, float length,
                                                        float strokeThickness) {
                    switch (decorationStyle)
                    {
                    case TextDecorationStyleKind::Solid:
                        list.Add(PaintCommandType::FillRect, Rect{ barX, barY, strokeThickness, length }, index)
                            .color = decorationColor;
                        break;

                    case TextDecorationStyleKind::Double:
                    {
                        const float bar = std::max(1.0f, strokeThickness * 0.4f);
                        const float gap = std::max(1.0f, strokeThickness * 0.4f);
                        list.Add(PaintCommandType::FillRect, Rect{ barX, barY, bar, length }, index).color =
                            decorationColor;
                        list.Add(PaintCommandType::FillRect, Rect{ barX + bar + gap, barY, bar, length }, index)
                            .color = decorationColor;
                        break;
                    }

                    case TextDecorationStyleKind::Dotted:
                    {
                        const float dot = std::max(1.0f, strokeThickness);
                        const float step = dot * 2.0f;

                        for (float offset = 0.0f; offset < length; offset += step)
                        {
                            const float piece = std::min(dot, length - offset);
                            list.Add(PaintCommandType::FillRect, Rect{ barX, barY + offset, dot, piece }, index)
                                .color = decorationColor;
                        }
                        break;
                    }

                    case TextDecorationStyleKind::Dashed:
                    {
                        const float dash = std::max(2.0f, strokeThickness * 3.0f);
                        const float step = dash * 2.0f;

                        for (float offset = 0.0f; offset < length; offset += step)
                        {
                            const float piece = std::min(dash, length - offset);
                            list.Add(PaintCommandType::FillRect,
                                     Rect{ barX, barY + offset, strokeThickness, piece }, index)
                                .color = decorationColor;
                        }
                        break;
                    }

                    case TextDecorationStyleKind::Wavy:
                    {
                        const float amplitude = std::max(1.0f, strokeThickness);
                        const float quarter = std::max(2.0f, strokeThickness * 2.0f);
                        int phase = 0;

                        for (float offset = 0.0f; offset < length; offset += quarter, ++phase)
                        {
                            const float level = ((phase / 2) % 2 == 0) ? 0.0f : amplitude;
                            const float piece = std::min(quarter, length - offset);
                            list.Add(PaintCommandType::FillRect,
                                     Rect{ barX + level, barY + offset, strokeThickness, piece }, index)
                                .color = decorationColor;
                        }
                        break;
                    }
                    }
                };

                const float columnLength = std::max(line.lineHeight, 1.0f);

                if ((decoration & TextDecorationUnderline) != 0u)
                {
                    // `auto` puts the underline where the face says it goes,
                    // which is the `post` table's position; a face without one
                    // falls back to the fraction of the font size.
                    const float fontOffset =
                        -layout.Text().UnderlinePosition(line.fontSize, line.weight, &runStyle);
                    const float defaultOffset =
                        fontOffset > 0.0f ? fontOffset : std::max(1.0f, line.fontSize * 0.12f);
                    const float underlineOffset =
                        line.underlineOffsetKind == TextUnderlineOffsetKind::Length ? line.underlineOffset
                                                                                    : defaultOffset;

                    if (line.vertical)
                    {
                        const float barX = lineX + std::max(line.width - thickness, 0.0f) + underlineOffset;
                        drawVerticalDecoration(barX, top, columnLength, thickness);
                    }
                    else
                    {
                        drawDecoration(lineX, baseline + underlineOffset, decorationWidth, thickness);
                    }
                }

                if ((decoration & TextDecorationOverline) != 0u)
                {
                    if (line.vertical)
                    {
                        drawVerticalDecoration(lineX, top, columnLength, thickness);
                    }
                    else
                    {
                        drawDecoration(lineX, top, decorationWidth, thickness);
                    }
                }

                if ((decoration & TextDecorationLineThrough) != 0u)
                {
                    if (line.vertical)
                    {
                        const float barX = lineX + std::max(line.width * 0.5f - thickness * 0.5f, 0.0f);
                        drawVerticalDecoration(barX, top, columnLength, thickness);
                    }
                    else
                    {
                        drawDecoration(lineX, baseline - line.fontSize * 0.28f, decorationWidth, thickness);
                    }
                }
            }

            if (field != nullptr && focused && caretVisible && !field->HasSelection())
            {
                const std::uint32_t caretByte = TextLayout::ByteOffsetForCodepoint(text, field->caret);
                const float caretOffset =
                    layout.Text().MeasurePrefix(text, caretByte, line.fontSize, line.weight, &runStyle);

                if (line.vertical)
                {
                    // The caret crosses the column at the offset the prefix
                    // reaches, which is a horizontal bar in a vertical run.
                    list.Add(PaintCommandType::FillRect,
                             Rect{ lineX, top + caretOffset, std::max(line.width, 1.0f), 1.0f }, index)
                        .color = TextColor;
                }
                else
                {
                    list.Add(PaintCommandType::FillRect,
                             Rect{ lineX + caretOffset, top, 1.0f, line.lineHeight }, index)
                        .color = TextColor;
                }
            }
        });
    }

    void PaintEngine::PaintListMarker(ElementIndex index,
                                      const ElementLayout& box,
                                      const CompiledStyle& style,
                                      const CompiledElement& element,
                                      float originX,
                                      float originY)
    {
        const CompiledElement* parent = ElementAt(document, element.parent);
        if (parent == nullptr || CategoryFor(parent->kind) != RenderCategory::List)
        {
            return;
        }

        const char* markerName = element.listStyle != 0 ? StringAt(document, element.listStyle)
                                                        : StringAt(document, parent->listStyle);

        if (markerName == nullptr || markerName[0] == '\0')
        {
            markerName = "disc";
        }

        if (std::char_traits<char>::compare(markerName, "none", 4) == 0)
        {
            return;
        }

        // The ordinal is the item's position among its list item siblings,
        // shifted by the list's start attribute when it declares one.
        std::uint32_t ordinal = 0;
        for (ElementIndex sibling = parent->firstChild; sibling != NoElement && sibling != index;)
        {
            const CompiledElement* node = ElementAt(document, sibling);
            if (node == nullptr)
            {
                break;
            }

            if (CategoryFor(node->kind) == RenderCategory::ListItem)
            {
                ++ordinal;
            }

            sibling = node->nextSibling;
        }

        const long long value =
            static_cast<long long>(parent->HasFlag(ElementFlagHasListStart) ? parent->listStart : 1) +
            static_cast<long long>(ordinal);

        const Rect content = Translated(box.content, originX, originY);
        const ElementState& elementState = state.Element(index);
        const Color markerColor = elementState.HasFlag(RuntimeFlagDisabled)
                                      ? DisabledTextColor
                                      : (style.hasForeground != 0 ? style.foreground : TextColor);

        const float fontSize = style.fontSize > 0.0f ? style.fontSize : 16.0f;
        const float lineHeight = layout.Text().LineHeight(fontSize, style.fontWeight);
        const float markerSize = std::max(4.0f, fontSize * 0.3f);
        const float centerY = content.y + lineHeight * 0.5f;
        const float markerRight = content.x - std::max(6.0f, fontSize * 0.4f);

        const Rect markerBox{ markerRight - markerSize, centerY - markerSize * 0.5f, markerSize, markerSize };

        if (std::char_traits<char>::compare(markerName, "disc", 4) == 0)
        {
            PaintCommand& command = list.Add(PaintCommandType::FillRoundedRect, markerBox, index);
            command.color = markerColor;
            command.radius = markerSize * 0.5f;
            return;
        }

        if (std::char_traits<char>::compare(markerName, "circle", 6) == 0)
        {
            PaintCommand& command = list.Add(PaintCommandType::StrokeRoundedRect, markerBox, index);
            command.color = markerColor;
            command.radius = markerSize * 0.5f;
            command.edges = Edges::Uniform(1.0f);
            return;
        }

        if (std::char_traits<char>::compare(markerName, "square", 6) == 0)
        {
            list.Add(PaintCommandType::FillRect, markerBox, index).color = markerColor;
            return;
        }

        char buffer[40];

        if (std::char_traits<char>::compare(markerName, "decimal", 7) == 0)
        {
            std::snprintf(buffer, sizeof(buffer), "%lld.", value);
        }
        else if (std::char_traits<char>::compare(markerName, "lower-alpha", 11) == 0 ||
                 std::char_traits<char>::compare(markerName, "lower-latin", 11) == 0)
        {
            char letters[16];
            LowerAlpha(static_cast<unsigned>(value < 1 ? 1 : value), letters);
            std::snprintf(buffer, sizeof(buffer), "%s.", letters);
        }
        else if (std::char_traits<char>::compare(markerName, "upper-alpha", 11) == 0 ||
                 std::char_traits<char>::compare(markerName, "upper-latin", 11) == 0)
        {
            char letters[16];
            UpperAlpha(static_cast<unsigned>(value < 1 ? 1 : value), letters);
            std::snprintf(buffer, sizeof(buffer), "%s.", letters);
        }
        else if (std::char_traits<char>::compare(markerName, "lower-roman", 11) == 0)
        {
            char letters[24];
            RomanNumerals(static_cast<unsigned>(value < 1 ? 1 : value), letters, false);
            std::snprintf(buffer, sizeof(buffer), "%s.", letters);
        }
        else if (std::char_traits<char>::compare(markerName, "upper-roman", 11) == 0)
        {
            char letters[24];
            RomanNumerals(static_cast<unsigned>(value < 1 ? 1 : value), letters, true);
            std::snprintf(buffer, sizeof(buffer), "%s.", letters);
        }
        else
        {
            return;
        }

        const std::size_t length = std::char_traits<char>::length(buffer);
        const float labelWidth = layout.Text().Measure(buffer, fontSize, style.fontWeight);
        const float baseline = centerY + fontSize * 0.36f;

        EmitTextRun(index, markerRight - labelWidth, baseline, buffer, 0, static_cast<std::uint32_t>(length),
                    fontSize, style.fontWeight, markerColor);
    }

    void PaintEngine::PaintImage(ElementIndex index,
                                 const ElementLayout& box,
                                 const CompiledStyle& style,
                                 const CompiledElement& element,
                                 float originX,
                                 float originY)
    {
        (void)style;

        if (element.resource == 0)
        {
            return;
        }

        ++stats.imageCount;

        PaintCommand& command =
            list.Add(PaintCommandType::DrawImage, Translated(box.content, originX, originY), index);
        command.resource = element.resource;
    }

    void PaintEngine::PaintCanvas(ElementIndex index,
                                  const ElementLayout& box,
                                  const CompiledStyle& style,
                                  const CompiledElement& element,
                                  float originX,
                                  float originY)
    {
        (void)style;
        (void)element;

        // Nothing has drawn into the canvas yet. An application paints into it
        // through the runtime's canvas pass; until then it shows as an empty
        // surface rather than as a hole in the window.
        list.Add(PaintCommandType::FillRect, Translated(box.content, originX, originY), index).color = HeaderColor;
    }

    void PaintEngine::PaintMedia(ElementIndex index,
                                 const ElementLayout& box,
                                 const CompiledStyle& style,
                                 const CompiledElement& element,
                                 RenderCategory category,
                                 float originX,
                                 float originY)
    {
        (void)style;

        const Rect content = Translated(box.content, originX, originY);
        list.Add(PaintCommandType::FillRect, content, index).color = Color::Rgba(17, 24, 39, 255);

        // A play triangle, sized from the smaller side so it stays balanced.
        const float size = std::min(std::min(content.width, content.height) * 0.24f, 36.0f);
        EmitRightTriangle(list, content.x + content.width * 0.5f - size * 0.5f,
                          content.y + content.height * 0.5f, size, size, SurfaceColor, index);

        if (category == RenderCategory::Media && element.HasFlag(ElementFlagControls))
        {
            const float barHeight = std::min(18.0f, content.height * 0.18f);
            list.Add(PaintCommandType::FillRect,
                     Rect{ content.x, content.Bottom() - barHeight, content.width * 0.32f, barHeight },
                     index)
                .color = Color::Rgba(37, 99, 235, 220);
        }
    }

    void PaintEngine::PaintSeparator(ElementIndex index,
                                     const ElementLayout& box,
                                     const CompiledStyle& style,
                                     const CompiledElement& element,
                                     float originX,
                                     float originY)
    {
        (void)element;

        if (style.hasBackground != 0)
        {
            return;
        }

        list.Add(PaintCommandType::FillRect, Translated(box.border, originX, originY), index).color = BorderColor;
    }

    void PaintEngine::PaintDisclosureMarker(ElementIndex index,
                                            const ElementLayout& box,
                                            const CompiledStyle& style,
                                            const CompiledElement& element,
                                            float originX,
                                            float originY)
    {
        (void)style;

        const bool open = state.Element(index).HasFlag(RuntimeFlagOpen) || element.HasFlag(ElementFlagOpen);
        const Rect content = Translated(box.content, originX, originY);

        const float size = 7.0f;
        const float centerY = content.y + std::max(10.0f, content.height * 0.5f);
        // The summary reserves room on its left through its padding, so the
        // marker sits in the padding rather than over the text.
        const float left = content.x - 16.0f;

        if (open)
        {
            EmitDownTriangle(list, left + size * 0.5f, centerY - size * 0.35f, size, size, TextColor, index);
        }
        else
        {
            EmitRightTriangle(list, left, centerY, size, size, TextColor, index);
        }
    }

    void PaintEngine::PaintTableParts(ElementIndex index,
                                      const ElementLayout& box,
                                      const CompiledStyle& style,
                                      const CompiledElement& element,
                                      RenderCategory category,
                                      float originX,
                                      float originY)
    {
        (void)style;
        (void)element;

        if (category != RenderCategory::TableRow)
        {
            return;
        }

        // A row highlights as a whole when the pointer is anywhere over it: a
        // native table behaviour, not a property of one cell.
        const ElementState& elementState = state.Element(index);
        if (!elementState.HasFlag(RuntimeFlagHovered) && !elementState.HasFlag(RuntimeFlagHoveredSubtree))
        {
            return;
        }

        list.Add(PaintCommandType::FillRect, Translated(box.border, originX, originY), index).color = HeaderColor;
    }

    void PaintEngine::PaintControl(ElementIndex index,
                                   const ElementLayout& box,
                                   const CompiledStyle& style,
                                   const CompiledElement& element,
                                   RenderCategory category,
                                   float originX,
                                   float originY)
    {
        const ControlVisualState visual = VisualStateOf(index);
        const ElementState& elementState = state.Element(index);
        const Rect border = Translated(box.border, originX, originY);
        const Rect content = Translated(box.content, originX, originY);

        switch (category)
        {
        case RenderCategory::Button:
        {
            // Hover and press are native appearance: the document declares none
            // of it, so the painter derives both from runtime state.
            Color face = style.hasBackground != 0 ? style.background : RaisedColor;

            if (visual == ControlVisualState::Pressed)
            {
                face = ShadeColor(face, 0.86f);
            }
            else if (visual == ControlVisualState::Hovered)
            {
                face = ShadeColor(face, 1.06f);
            }
            else if (visual == ControlVisualState::Disabled)
            {
                face = DisabledColor;
            }

            PaintCommand& command = list.Add(PaintCommandType::FillRoundedRect, border, index);
            command.color = face;
            command.radius = style.borderRadius > 0.0f ? style.borderRadius : 4.0f;
            break;
        }

        case RenderCategory::Checkbox:
        {
            const bool checked = elementState.HasFlag(RuntimeFlagChecked) || element.HasFlag(ElementFlagChecked);
            const bool disabled = visual == ControlVisualState::Disabled;

            // The face is a rounded box, not a plain rectangle: a native check
            // box has a radius even when the document style gives none.
            PaintCommand& face = list.Add(PaintCommandType::FillRoundedRect, border, index);
            face.color = disabled ? DisabledColor : SurfaceColor;
            face.radius = std::max(2.0f, border.height * 0.18f);

            if (checked)
            {
                const Rect inner = border.Expanded(-1.0f);
                PaintCommand& fill = list.Add(PaintCommandType::FillRoundedRect, inner, index);
                fill.color = disabled ? AccentMutedColor : AccentColor;
                fill.radius = std::max(2.0f, inner.height * 0.24f);

                EmitCheckMark(list, border, 2.0f, index);
            }
            break;
        }

        case RenderCategory::Radio:
        {
            const bool checked = elementState.HasFlag(RuntimeFlagChecked) || element.HasFlag(ElementFlagChecked);
            const bool disabled = visual == ControlVisualState::Disabled;
            const float radius = border.height * 0.5f;

            PaintCommand& face = list.Add(PaintCommandType::FillRoundedRect, border, index);
            face.color = disabled ? DisabledColor : SurfaceColor;
            face.radius = radius;

            PaintCommand& ring = list.Add(PaintCommandType::StrokeRoundedRect, border, index);
            ring.color = BorderColor;
            ring.radius = radius;
            ring.edges = Edges::Uniform(1.0f);

            if (checked)
            {
                const Rect dot = border.Expanded(-border.height * 0.3f);
                PaintCommand& fill = list.Add(PaintCommandType::FillRoundedRect, dot, index);
                fill.color = disabled ? AccentMutedColor : AccentColor;
                fill.radius = dot.height * 0.5f;
            }
            break;
        }

        case RenderCategory::Range:
        {
            const float trackHeight = std::max(2.0f, std::floor(std::max(content.height, 1.0f) * 0.2f));
            const float trackY = content.y + (content.height - trackHeight) * 0.5f;
            const Rect track{ content.x, trackY, content.width, trackHeight };

            const bool hasValue = state.HasNumericValue(index) || element.HasFlag(ElementFlagHasNumericValue);
            const float value = state.HasNumericValue(index) ? state.NumericValue(index) : element.numericValue;

            float fraction = 0.5f;
            if (hasValue)
            {
                const float span = element.maximum - element.minimum;
                fraction = span > 0.0f ? Clamp((value - element.minimum) / span, 0.0f, 1.0f) : 0.0f;
            }

            PaintCommand& groove = list.Add(PaintCommandType::FillRoundedRect, track, index);
            groove.color = TrackColor;
            groove.radius = trackHeight * 0.5f;

            const float filled = std::max(trackHeight, track.width * fraction);
            PaintCommand& progress =
                list.Add(PaintCommandType::FillRoundedRect, Rect{ track.x, track.y, filled, track.height }, index);
            progress.color = AccentColor;
            progress.radius = trackHeight * 0.5f;

            const float thumbRadius = std::max(4.0f, content.height * 0.4f);
            const Rect thumb{ track.x + track.width * fraction - thumbRadius,
                              content.y + content.height * 0.5f - thumbRadius,
                              thumbRadius * 2.0f,
                              thumbRadius * 2.0f };

            PaintCommand& knob = list.Add(PaintCommandType::FillRoundedRect, thumb, index);
            knob.color = SurfaceColor;
            knob.radius = thumbRadius;

            PaintCommand& edge = list.Add(PaintCommandType::StrokeRoundedRect, thumb, index);
            edge.color = AccentColor;
            edge.radius = thumbRadius;
            edge.edges = Edges::Uniform(2.0f);
            break;
        }

        case RenderCategory::Progress:
        {
            const float span = element.maximum - element.minimum;
            const bool known = state.HasNumericValue(index) || element.HasFlag(ElementFlagHasNumericValue);
            const float value = state.HasNumericValue(index) ? state.NumericValue(index) : element.numericValue;
            const float fraction =
                known ? (span > 0.0f ? Clamp((value - element.minimum) / span, 0.0f, 1.0f) : 0.0f) : -1.0f;

            if (fraction < 0.0f)
            {
                // An indeterminate bar shows a band, which reads as work in
                // progress without claiming a percentage the document never gave.
                PaintCommand& band = list.Add(PaintCommandType::FillRect,
                                              Rect{ content.x + content.width * 0.35f, content.y,
                                                    content.width * 0.3f, content.height },
                                              index);
                band.color = AccentMutedColor;
            }
            else
            {
                PaintCommand& fill = list.Add(PaintCommandType::FillRect,
                                              Rect{ content.x, content.y, content.width * fraction, content.height },
                                              index);
                fill.color = AccentColor;
            }
            break;
        }

        case RenderCategory::Meter:
        {
            const float span = element.maximum - element.minimum;
            const float value = state.HasNumericValue(index) ? state.NumericValue(index) : element.numericValue;
            const float fraction = span > 0.0f ? Clamp((value - element.minimum) / span, 0.0f, 1.0f) : 0.0f;

            Color barColor = SuccessColor;
            if (fraction < 0.34f)
            {
                barColor = DangerColor;
            }
            else if (fraction < 0.67f)
            {
                barColor = WarningColor;
            }

            PaintCommand& fill = list.Add(PaintCommandType::FillRect,
                                          Rect{ content.x, content.y, content.width * fraction, content.height },
                                          index);
            fill.color = barColor;
            break;
        }

        case RenderCategory::ColorInput:
        {
            list.Add(PaintCommandType::FillRect, content, index).color =
                ParseColorValue(StringAt(document, element.value));
            break;
        }

        case RenderCategory::Select:
        {
            const float size = 5.0f;
            const float centerX = border.Right() - std::max(10.0f, border.height * 0.4f);
            const float top = border.y + border.height * 0.5f - size * 0.5f;

            EmitDownTriangle(list, centerX, top, size * 1.6f, size, TextColor, index);
            break;
        }

        default:
            break;
        }
    }
}
