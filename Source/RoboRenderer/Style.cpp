#include "RoboRenderer/Style.h"

namespace RoboRenderer
{
    namespace
    {
        /// The HTML formatting defaults, expressed as native values rather than as
        /// a stylesheet. Colours are the neutral palette the compiled examples
        /// use, so a document without styling still reads as a native control
        /// surface instead of a wireframe.
        constexpr Color TextColor{ 15, 23, 42, 255 };
        constexpr Color MutedTextColor{ 100, 116, 139, 255 };
        constexpr Color PlaceholderColor{ 148, 163, 184, 255 };
        constexpr Color BorderColor{ 203, 213, 225, 255 };
        constexpr Color SurfaceColor{ 255, 255, 255, 255 };
        constexpr Color RaisedColor{ 226, 232, 240, 255 };
        constexpr Color HeaderColor{ 241, 245, 249, 255 };
        constexpr Color LinkColor{ 37, 99, 235, 255 };
        constexpr Color DisabledColor{ 241, 245, 249, 255 };

        Color Shade(Color color, float factor)
        {
            const auto scale = [factor](std::uint8_t channel) {
                const float value = static_cast<float>(channel) * factor;
                return static_cast<std::uint8_t>(value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value));
            };

            return Color{ scale(color.red), scale(color.green), scale(color.blue), color.alpha };
        }

        /// Small builder so the default table reads as a table. Only the fields a
        /// category actually sets are touched; everything else keeps the neutral
        /// default from MakeBase().
        struct StyleBuilder
        {
            CompiledStyle style;

            StyleBuilder(DisplayKind display, float fontSize, FontWeightKind weight)
            {
                // A category default is complete: it specifies nothing because it
                // *is* the answer, and the authored table decides what replaces
                // it.
                style.specified = 0u;
                style.width = LayoutValue::Auto();
                style.height = LayoutValue::Auto();
                style.minimumWidth = LayoutValue::Auto();
                style.minimumHeight = LayoutValue::Auto();
                style.maximumWidth = LayoutValue::Auto();
                style.maximumHeight = LayoutValue::Auto();
                style.margin = Edges::Zero();
                style.padding = Edges::Zero();
                style.borderWidth = Edges::Zero();
                style.borderRadius = 0.0f;
                style.background = Color{ 0, 0, 0, 0 };
                style.foreground = TextColor;
                style.borderColor = BorderColor;
                style.fontSize = fontSize;
                style.lineHeight = 0.0f;
                style.letterSpacing = 0.0f;
                style.wordSpacing = 0.0f;
                style.textIndent = 0.0f;
                style.display = display;
                style.overflowX = OverflowKind::Visible;
                style.overflowY = OverflowKind::Visible;
                style.textAlign = TextAlignKind::Start;
                style.fontWeight = weight;
                style.fontStyle = FontSlantKind::Upright;
                style.whiteSpace = WhiteSpaceKind::Normal;
                style.textTransform = TextTransformKind::None;
                style.direction = TextDirectionKind::Auto;
                style.fontFamily = 0;
                style.hasBackground = 0;
                style.hasForeground = 0;
                style.hasBorder = 0;
                style.hasFontSize = 1;
                style.sourceStyleId = 0;
            }

            StyleBuilder& Size(LayoutValue width, LayoutValue height)
            {
                style.width = width;
                style.height = height;
                return *this;
            }

            StyleBuilder& Minimum(float width, float height)
            {
                style.minimumWidth = LayoutValue::Fixed(width);
                style.minimumHeight = LayoutValue::Fixed(height);
                return *this;
            }

            StyleBuilder& Margin(Edges edges)
            {
                style.margin = edges;
                return *this;
            }

            StyleBuilder& Padding(Edges edges)
            {
                style.padding = edges;
                return *this;
            }

            StyleBuilder& Border(float width)
            {
                style.borderWidth = Edges::Uniform(width);
                style.hasBorder = width > 0.0f ? 1 : 0;
                return *this;
            }

            StyleBuilder& Border(float width, Color color)
            {
                Border(width);
                style.borderColor = color;
                return *this;
            }

            StyleBuilder& Radius(float radius)
            {
                style.borderRadius = radius;
                return *this;
            }

            StyleBuilder& Background(Color color)
            {
                style.background = color;
                style.hasBackground = 1;
                return *this;
            }

            StyleBuilder& Foreground(Color color)
            {
                style.foreground = color;
                style.hasForeground = 1;
                return *this;
            }

            StyleBuilder& Align(TextAlignKind align)
            {
                style.textAlign = align;
                return *this;
            }

            StyleBuilder& Overflow(OverflowKind x, OverflowKind y)
            {
                style.overflowX = x;
                style.overflowY = y;
                return *this;
            }

            StyleBuilder& LineHeight(float value)
            {
                style.lineHeight = value;
                return *this;
            }

            CompiledStyle Build() const { return style; }
        };
    }

    bool IsBlockCategory(RenderCategory category)
    {
        switch (category)
        {
        case RenderCategory::Document:
        case RenderCategory::Container:
        case RenderCategory::Text:
        case RenderCategory::PreformattedText:
        case RenderCategory::Heading:
        case RenderCategory::Separator:
        case RenderCategory::List:
        case RenderCategory::ListItem:
        case RenderCategory::DescriptionTerm:
        case RenderCategory::DescriptionDescription:
        case RenderCategory::Figure:
        case RenderCategory::FigureCaption:
        case RenderCategory::Media:
        case RenderCategory::Form:
        case RenderCategory::FieldSet:
        case RenderCategory::TextArea:
        case RenderCategory::Table:
        case RenderCategory::TableCaption:
        case RenderCategory::TableSection:
        case RenderCategory::TableRow:
        case RenderCategory::Disclosure:
        case RenderCategory::Dialog:
        case RenderCategory::Frame:
        case RenderCategory::EmbeddedObject:
        case RenderCategory::CustomElement:
        case RenderCategory::UnsupportedNamespace:
            return true;
        default:
            return false;
        }
    }

    bool IsInlineCategory(RenderCategory category)
    {
        switch (category)
        {
        case RenderCategory::Link:
        case RenderCategory::Label:
        case RenderCategory::Button:
        case RenderCategory::Input:
        case RenderCategory::Checkbox:
        case RenderCategory::Radio:
        case RenderCategory::Range:
        case RenderCategory::ColorInput:
        case RenderCategory::FileInput:
        case RenderCategory::Select:
        case RenderCategory::Output:
        case RenderCategory::Progress:
        case RenderCategory::Meter:
        case RenderCategory::Ruby:
        case RenderCategory::RubyText:
        case RenderCategory::Image:
        case RenderCategory::Canvas:
            return true;
        default:
            return false;
        }
    }

    bool IsReplacedCategory(RenderCategory category)
    {
        switch (category)
        {
        case RenderCategory::Image:
        case RenderCategory::Canvas:
        case RenderCategory::Media:
        case RenderCategory::Frame:
        case RenderCategory::EmbeddedObject:
        case RenderCategory::Input:
        case RenderCategory::TextArea:
        case RenderCategory::Select:
        case RenderCategory::Checkbox:
        case RenderCategory::Radio:
        case RenderCategory::Range:
        case RenderCategory::ColorInput:
        case RenderCategory::FileInput:
        case RenderCategory::Progress:
        case RenderCategory::Meter:
            return true;
        default:
            return false;
        }
    }

    bool IsTextCategory(RenderCategory category)
    {
        switch (category)
        {
        case RenderCategory::Text:
        case RenderCategory::PreformattedText:
        case RenderCategory::Heading:
        case RenderCategory::Link:
        case RenderCategory::Label:
        case RenderCategory::Output:
        case RenderCategory::Button:
        case RenderCategory::DisclosureSummary:
        case RenderCategory::FigureCaption:
        case RenderCategory::TableCaption:
        case RenderCategory::DescriptionTerm:
        case RenderCategory::DescriptionDescription:
        case RenderCategory::ListItem:
        case RenderCategory::TableCell:
        case RenderCategory::Option:
        case RenderCategory::Ruby:
        case RenderCategory::RubyText:
        case RenderCategory::CustomElement:
            return true;
        default:
            return false;
        }
    }

    void CategoryIntrinsicSize(RenderCategory category, float& width, float& height)
    {
        // Logical units. These are the sizes a browser-like default gives a
        // control before content or an author size is applied; they are applied
        // after DPI scaling because layout works in logical units.
        width = 0.0f;
        height = 0.0f;

        switch (category)
        {
        case RenderCategory::Button: width = 72.0f; height = 30.0f; break;
        case RenderCategory::Input: width = 200.0f; height = 26.0f; break;
        case RenderCategory::TextArea: width = 220.0f; height = 60.0f; break;
        case RenderCategory::Select: width = 160.0f; height = 28.0f; break;
        case RenderCategory::Checkbox: width = 14.0f; height = 14.0f; break;
        case RenderCategory::Radio: width = 14.0f; height = 14.0f; break;
        case RenderCategory::Range: width = 160.0f; height = 20.0f; break;
        case RenderCategory::ColorInput: width = 48.0f; height = 26.0f; break;
        case RenderCategory::FileInput: width = 220.0f; height = 28.0f; break;
        case RenderCategory::Progress: width = 160.0f; height = 16.0f; break;
        case RenderCategory::Meter: width = 160.0f; height = 16.0f; break;
        case RenderCategory::Canvas: width = 300.0f; height = 150.0f; break;
        case RenderCategory::Media: width = 320.0f; height = 180.0f; break;
        case RenderCategory::Frame: width = 300.0f; height = 150.0f; break;
        case RenderCategory::EmbeddedObject: width = 300.0f; height = 150.0f; break;
        default: break;
        }
    }

    /// The text properties a run of text takes from the element it flows inside.
    ///
    /// This is the `inherited` set of the property registry expressed as native
    /// flags: the font, the colour, the spacing, the alignment, the decorations
    /// and the writing direction. Nothing that describes a box is here, because
    /// a box's own edges, background and borders are never inherited - a
    /// sentence inside a bordered card is set in the card's type, not in its
    /// padding.
    constexpr std::uint64_t InheritedTextFlags =
        StyleFlagForeground | StyleFlagFontFamily | StyleFlagFontSize | StyleFlagFontWeight |
        StyleFlagFontStyle | StyleFlagLineHeight | StyleFlagLetterSpacing | StyleFlagWordSpacing |
        StyleFlagTextAlign | StyleFlagTextAlignLast | StyleFlagTextJustify | StyleFlagTextIndent |
        StyleFlagTextTransform | StyleFlagWhiteSpace | StyleFlagWordBreak | StyleFlagOverflowWrap |
        StyleFlagLineBreak | StyleFlagHyphens | StyleFlagTabSize | StyleFlagFontKerning |
        StyleFlagFontFeatures | StyleFlagDirection | StyleFlagWritingMode | StyleFlagTextOrientation |
        StyleFlagTextDecoration | StyleFlagTextDecorationColor | StyleFlagTextDecorationStyle |
        StyleFlagTextDecorationThickness | StyleFlagTextUnderlineOffset;

    /// Carries `source`'s decoration onto `target` when `target` states none.
    ///
    /// The specification does not inherit `text-decoration`: a decoration
    /// propagates to the descendants painted inside it. Layout draws a decoration
    /// per run, so the words either side of a nested element and the nested
    /// element's own text all have to report the line - otherwise a phrase inside
    /// an underlined paragraph is the one phrase without a line under it.
    void PropagateTextDecoration(CompiledStyle& target, const CompiledStyle& source)
    {
        if (!IsSpecified(source.specified, StyleFlagTextDecoration) ||
            IsSpecified(target.specified, StyleFlagTextDecoration))
        {
            return;
        }

        target.textDecoration = source.textDecoration;
        target.specified |= StyleFlagTextDecoration;

        // What describes the decoration travels with it, and only where the
        // element painted inside has not described its own.
        if (IsSpecified(source.specified, StyleFlagTextDecorationColor) &&
            !IsSpecified(target.specified, StyleFlagTextDecorationColor))
        {
            target.textDecorationColor = source.textDecorationColor;
            target.specified |= StyleFlagTextDecorationColor;
        }

        if (IsSpecified(source.specified, StyleFlagTextDecorationStyle) &&
            !IsSpecified(target.specified, StyleFlagTextDecorationStyle))
        {
            target.textDecorationStyle = source.textDecorationStyle;
            target.specified |= StyleFlagTextDecorationStyle;
        }

        if (IsSpecified(source.specified, StyleFlagTextDecorationThickness) &&
            !IsSpecified(target.specified, StyleFlagTextDecorationThickness))
        {
            target.textDecorationThicknessKind = source.textDecorationThicknessKind;
            target.textDecorationThickness = source.textDecorationThickness;
            target.specified |= StyleFlagTextDecorationThickness;
        }

        if (IsSpecified(source.specified, StyleFlagTextUnderlineOffset) &&
            !IsSpecified(target.specified, StyleFlagTextUnderlineOffset))
        {
            target.textUnderlineOffsetKind = source.textUnderlineOffsetKind;
            target.textUnderlineOffset = source.textUnderlineOffset;
            target.specified |= StyleFlagTextUnderlineOffset;
        }
    }

    /// Copies the text properties of `source` onto `target`, leaving the target's
    /// own box - its display, edges, background and borders - alone.
    ///
    /// The specification does not inherit the decorations; they propagate to a
    /// descendant's painting instead. Here a run is the only thing that draws its
    /// own text, so it carries the decoration of the element it sits in, which is
    /// the underline a reader sees under a word inside a decorated paragraph.
    void InheritTextProperties(CompiledStyle& target, const CompiledStyle& source)
    {
        target.foreground = source.foreground;
        target.hasForeground = source.hasForeground;
        target.fontFamily = source.fontFamily;
        target.fontSize = source.fontSize;
        target.fontWeight = source.fontWeight;
        target.fontStyle = source.fontStyle;
        target.lineHeight = source.lineHeight;
        target.letterSpacing = source.letterSpacing;
        target.wordSpacing = source.wordSpacing;
        target.textAlign = source.textAlign;
        target.textAlignLast = source.textAlignLast;
        target.hasTextAlignLast = source.hasTextAlignLast;
        target.textJustify = source.textJustify;
        target.textIndent = source.textIndent;
        target.textTransform = source.textTransform;
        target.whiteSpace = source.whiteSpace;
        target.wordBreak = source.wordBreak;
        target.overflowWrap = source.overflowWrap;
        target.lineBreak = source.lineBreak;
        target.hyphens = source.hyphens;
        target.tabSize = source.tabSize;
        target.tabSizeLength = source.tabSizeLength;
        target.fontKerning = source.fontKerning;
        target.fontFeatures = source.fontFeatures;
        target.direction = source.direction;
        target.writingMode = source.writingMode;
        target.textOrientation = source.textOrientation;
        target.textDecoration = source.textDecoration;
        target.textDecorationColor = source.textDecorationColor;
        target.textDecorationStyle = source.textDecorationStyle;
        target.textDecorationThicknessKind = source.textDecorationThicknessKind;
        target.textDecorationThickness = source.textDecorationThickness;
        target.textUnderlineOffsetKind = source.textUnderlineOffsetKind;
        target.textUnderlineOffset = source.textUnderlineOffset;

        // The properties just carried in are decided values now, which is what
        // the mask says: nothing downstream reads a field the element itself
        // never set.
        target.specified |= InheritedTextFlags;
    }

    DefaultStyleProvider::DefaultStyleProvider()
    {
        for (CompiledStyle& style : categoryStyles)
        {
            style = StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal).Build();
        }

        const auto set = [this](RenderCategory category, const CompiledStyle& style) {
            categoryStyles[static_cast<std::size_t>(category)] = style;
        };

        // The document frame is the page itself: it fills the viewport and keeps
        // the body's default margin.
        set(RenderCategory::Document,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(0.0f), LayoutValue::Auto())
                .Padding(Edges::Uniform(8.0f))
                .Background(SurfaceColor)
                .Foreground(TextColor)
                .Overflow(OverflowKind::Visible, OverflowKind::Visible)
                .Build());

        set(RenderCategory::Container,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal).Build());

        // Flow content: a block whose children flow inline, with the default
        // vertical margins of a paragraph.
        set(RenderCategory::Text,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 0.0f, 12.0f, 0.0f })
                .Build());

        set(RenderCategory::PreformattedText,
            StyleBuilder(DisplayKind::Block, 15.0f, FontWeightKind::Normal)
                .Margin(Edges{ 12.0f, 0.0f, 12.0f, 0.0f })
                .Padding(Edges::Uniform(8.0f))
                .Background(HeaderColor)
                .Border(1.0f)
                .Overflow(OverflowKind::Visible, OverflowKind::Visible)
                .Build());

        set(RenderCategory::Heading,
            StyleBuilder(DisplayKind::Block, 32.0f, FontWeightKind::Bold)
                .Margin(Edges{ 12.0f, 0.0f, 8.0f, 0.0f })
                .Build());

        set(RenderCategory::LineBreak,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal).Build());

        set(RenderCategory::Separator,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Auto(), LayoutValue::Fixed(1.0f))
                .Margin(Edges{ 12.0f, 0.0f, 12.0f, 0.0f })
                .Background(BorderColor)
                .Build());

        set(RenderCategory::Link,
            StyleBuilder(DisplayKind::Inline, BaseFontSize, FontWeightKind::Normal)
                .Foreground(LinkColor)
                .Build());

        set(RenderCategory::List,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 8.0f, 0.0f, 12.0f, 0.0f })
                .Padding(Edges{ 0.0f, 0.0f, 0.0f, 24.0f })
                .Build());

        set(RenderCategory::ListItem,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 0.0f, 4.0f, 0.0f })
                .Build());

        set(RenderCategory::DescriptionTerm,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Bold)
                .Margin(Edges{ 8.0f, 0.0f, 2.0f, 0.0f })
                .Build());

        set(RenderCategory::DescriptionDescription,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 0.0f, 6.0f, 0.0f })
                .Padding(Edges{ 0.0f, 0.0f, 0.0f, 24.0f })
                .Build());

        set(RenderCategory::Figure,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 16.0f, 0.0f, 16.0f, 0.0f })
                .Build());

        set(RenderCategory::FigureCaption,
            StyleBuilder(DisplayKind::Block, 13.0f, FontWeightKind::Normal)
                .Margin(Edges{ 6.0f, 0.0f, 0.0f, 0.0f })
                .Foreground(MutedTextColor)
                .Build());

        set(RenderCategory::Image,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal).Build());

        set(RenderCategory::Media,
            StyleBuilder(DisplayKind::Block, 13.0f, FontWeightKind::Normal)
                .Margin(Edges{ 12.0f, 0.0f, 12.0f, 0.0f })
                .Foreground(MutedTextColor)
                .Build());

        set(RenderCategory::Canvas,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Border(1.0f)
                .Build());

        set(RenderCategory::Form,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 0.0f, 12.0f, 0.0f })
                .Build());

        set(RenderCategory::FieldSet,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 2.0f, 12.0f, 2.0f })
                .Padding(Edges{ 8.0f, 10.0f, 10.0f, 10.0f })
                .Border(1.0f)
                .Radius(4.0f)
                .Build());

        set(RenderCategory::Legend,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Bold)
                .Padding(Edges{ 0.0f, 4.0f, 0.0f, 4.0f })
                .Build());

        set(RenderCategory::Label,
            StyleBuilder(DisplayKind::Inline, BaseFontSize, FontWeightKind::Normal).Build());

        set(RenderCategory::Button,
            StyleBuilder(DisplayKind::InlineBlock, 14.0f, FontWeightKind::Normal)
                .Minimum(48.0f, 30.0f)
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Padding(Edges{ 5.0f, 14.0f, 5.0f, 14.0f })
                .Border(1.0f, Color{ 189, 200, 214, 255 })
                .Radius(4.0f)
                .Background(RaisedColor)
                .Foreground(TextColor)
                .Align(TextAlignKind::Center)
                .Build());

        set(RenderCategory::Input,
            StyleBuilder(DisplayKind::InlineBlock, 14.0f, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(200.0f), LayoutValue::Fixed(26.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Padding(Edges{ 3.0f, 6.0f, 3.0f, 6.0f })
                .Border(1.0f)
                .Radius(3.0f)
                .Background(SurfaceColor)
                .Foreground(TextColor)
                .Overflow(OverflowKind::Clip, OverflowKind::Clip)
                .Build());

        set(RenderCategory::Checkbox,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(14.0f), LayoutValue::Fixed(14.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Build());

        set(RenderCategory::Radio,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(14.0f), LayoutValue::Fixed(14.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Build());

        set(RenderCategory::Range,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(160.0f), LayoutValue::Fixed(20.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Build());

        set(RenderCategory::ColorInput,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(48.0f), LayoutValue::Fixed(26.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Border(1.0f)
                .Build());

        set(RenderCategory::FileInput,
            StyleBuilder(DisplayKind::InlineBlock, 14.0f, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(220.0f), LayoutValue::Fixed(28.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Padding(Edges{ 4.0f, 6.0f, 4.0f, 6.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Foreground(MutedTextColor)
                .Overflow(OverflowKind::Clip, OverflowKind::Clip)
                .Build());

        set(RenderCategory::TextArea,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(220.0f), LayoutValue::Auto())
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Padding(Edges{ 4.0f, 6.0f, 4.0f, 6.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Foreground(TextColor)
                .Overflow(OverflowKind::Clip, OverflowKind::Scroll)
                .Build());

        set(RenderCategory::Select,
            StyleBuilder(DisplayKind::InlineBlock, 14.0f, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(160.0f), LayoutValue::Fixed(28.0f))
                .Margin(Edges{ 0.0f, 4.0f, 4.0f, 0.0f })
                .Padding(Edges{ 4.0f, 24.0f, 4.0f, 6.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Foreground(TextColor)
                .Overflow(OverflowKind::Clip, OverflowKind::Clip)
                .Build());

        set(RenderCategory::Option,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Normal)
                .Padding(Edges{ 3.0f, 6.0f, 3.0f, 6.0f })
                .Build());

        set(RenderCategory::OptionGroup,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Bold).Build());

        set(RenderCategory::Output,
            StyleBuilder(DisplayKind::Inline, 14.0f, FontWeightKind::Normal).Build());

        set(RenderCategory::Progress,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(160.0f), LayoutValue::Fixed(16.0f))
                .Margin(Edges{ 4.0f, 4.0f, 4.0f, 4.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Build());

        set(RenderCategory::Meter,
            StyleBuilder(DisplayKind::InlineBlock, BaseFontSize, FontWeightKind::Normal)
                .Size(LayoutValue::Fixed(160.0f), LayoutValue::Fixed(16.0f))
                .Margin(Edges{ 4.0f, 4.0f, 4.0f, 4.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Build());

        set(RenderCategory::Table,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Normal)
                .Margin(Edges{ 12.0f, 0.0f, 12.0f, 0.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Build());

        set(RenderCategory::TableCaption,
            StyleBuilder(DisplayKind::Block, 13.0f, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 0.0f, 6.0f, 0.0f })
                .Padding(Edges{ 4.0f, 6.0f, 4.0f, 6.0f })
                .Foreground(MutedTextColor)
                .Align(TextAlignKind::Center)
                .Build());

        set(RenderCategory::TableColumn,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Normal).Build());

        set(RenderCategory::TableSection,
            StyleBuilder(DisplayKind::Block, 14.0f, FontWeightKind::Normal).Build());

        set(RenderCategory::TableRow,
            StyleBuilder(DisplayKind::TableRow, 14.0f, FontWeightKind::Normal).Build());

        set(RenderCategory::TableCell,
            StyleBuilder(DisplayKind::TableCell, 14.0f, FontWeightKind::Normal)
                .Padding(Edges{ 5.0f, 8.0f, 5.0f, 8.0f })
                .Border(1.0f)
                .Background(SurfaceColor)
                .Build());

        set(RenderCategory::Disclosure,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Margin(Edges{ 0.0f, 0.0f, 12.0f, 0.0f })
                .Build());

        set(RenderCategory::DisclosureSummary,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Bold)
                .Padding(Edges{ 2.0f, 0.0f, 2.0f, 18.0f })
                .Build());

        set(RenderCategory::Dialog,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal)
                .Padding(Edges::Uniform(12.0f))
                .Border(1.0f)
                .Radius(6.0f)
                .Background(SurfaceColor)
                .Foreground(TextColor)
                .Build());

        set(RenderCategory::Frame,
            StyleBuilder(DisplayKind::Block, 13.0f, FontWeightKind::Normal)
                .Margin(Edges{ 12.0f, 0.0f, 12.0f, 0.0f })
                .Border(1.0f)
                .Foreground(MutedTextColor)
                .Build());

        set(RenderCategory::EmbeddedObject,
            StyleBuilder(DisplayKind::Block, 13.0f, FontWeightKind::Normal)
                .Margin(Edges{ 12.0f, 0.0f, 12.0f, 0.0f })
                .Border(1.0f)
                .Foreground(MutedTextColor)
                .Build());

        set(RenderCategory::Ruby,
            StyleBuilder(DisplayKind::Inline, BaseFontSize, FontWeightKind::Normal).Build());

        set(RenderCategory::RubyText,
            StyleBuilder(DisplayKind::Inline, 11.0f, FontWeightKind::Normal)
                .Foreground(MutedTextColor)
                .Build());

        set(RenderCategory::CustomElement,
            StyleBuilder(DisplayKind::Block, BaseFontSize, FontWeightKind::Normal).Build());

        set(RenderCategory::UnsupportedNamespace,
            StyleBuilder(DisplayKind::Block, 13.0f, FontWeightKind::Normal)
                .Margin(Edges{ 8.0f, 0.0f, 8.0f, 0.0f })
                .Padding(Edges::Uniform(6.0f))
                .Border(1.0f)
                .Foreground(MutedTextColor)
                .Build());
    }

    // -----------------------------------------------------------------------
    // StyleTableProvider
    // -----------------------------------------------------------------------

    StyleTableProvider::StyleTableProvider() = default;

    const CompiledStyle* StyleTableProvider::AuthoredStyleFor(const CompiledDocument& document, ElementIndex index)
    {
        const CompiledElement* element = ElementAt(document, index);

        if (element == nullptr || element->styleIndex == 0 || document.styles == nullptr ||
            element->styleIndex >= document.styleCount)
        {
            return nullptr;
        }

        const CompiledStyle& authored = document.styles[element->styleIndex];
        return authored.specified != 0u ? &authored : nullptr;
    }

    void StyleTableProvider::ApplyAuthored(CompiledStyle& target, const CompiledStyle& authored)
    {
        const std::uint64_t specified = authored.specified;

        if (IsSpecified(specified, StyleFlagWidth))
        {
            target.width = authored.width;
        }
        if (IsSpecified(specified, StyleFlagHeight))
        {
            target.height = authored.height;
        }
        if (IsSpecified(specified, StyleFlagMinimumWidth))
        {
            target.minimumWidth = authored.minimumWidth;
        }
        if (IsSpecified(specified, StyleFlagMinimumHeight))
        {
            target.minimumHeight = authored.minimumHeight;
        }
        if (IsSpecified(specified, StyleFlagMaximumWidth))
        {
            target.maximumWidth = authored.maximumWidth;
        }
        if (IsSpecified(specified, StyleFlagMaximumHeight))
        {
            target.maximumHeight = authored.maximumHeight;
        }

        const auto applyEdge = [&specified](StyleFlags flag, float authored, float& target) {
            if (IsSpecified(specified, flag))
            {
                target = authored;
            }
        };

        applyEdge(StyleFlagMarginTop, authored.margin.top, target.margin.top);
        applyEdge(StyleFlagMarginRight, authored.margin.right, target.margin.right);
        applyEdge(StyleFlagMarginBottom, authored.margin.bottom, target.margin.bottom);
        applyEdge(StyleFlagMarginLeft, authored.margin.left, target.margin.left);

        applyEdge(StyleFlagPaddingTop, authored.padding.top, target.padding.top);
        applyEdge(StyleFlagPaddingRight, authored.padding.right, target.padding.right);
        applyEdge(StyleFlagPaddingBottom, authored.padding.bottom, target.padding.bottom);
        applyEdge(StyleFlagPaddingLeft, authored.padding.left, target.padding.left);

        applyEdge(StyleFlagBorderTopWidth, authored.borderWidth.top, target.borderWidth.top);
        applyEdge(StyleFlagBorderRightWidth, authored.borderWidth.right, target.borderWidth.right);
        applyEdge(StyleFlagBorderBottomWidth, authored.borderWidth.bottom, target.borderWidth.bottom);
        applyEdge(StyleFlagBorderLeftWidth, authored.borderWidth.left, target.borderWidth.left);

        if (IsSpecified(specified, StyleFlagBorderRadius))
        {
            target.borderRadius = authored.borderRadius;
        }

        if (IsSpecified(specified, StyleFlagBackground))
        {
            target.background = authored.background;
            target.hasBackground = 1;
        }
        if (IsSpecified(specified, StyleFlagForeground))
        {
            target.foreground = authored.foreground;
            target.hasForeground = 1;
        }
        if (IsSpecified(specified, StyleFlagBorderColor))
        {
            target.borderColor = authored.borderColor;
            target.hasBorder = 1;
        }

        if (IsSpecified(specified, StyleFlagFontSize))
        {
            target.fontSize = authored.fontSize;
            target.hasFontSize = 1;
        }
        if (IsSpecified(specified, StyleFlagFontWeight))
        {
            target.fontWeight = authored.fontWeight;
        }
        if (IsSpecified(specified, StyleFlagLetterSpacing))
        {
            target.letterSpacing = authored.letterSpacing;
        }
        if (IsSpecified(specified, StyleFlagLineHeight))
        {
            target.lineHeight = authored.lineHeight;
        }
        if (IsSpecified(specified, StyleFlagWordSpacing))
        {
            target.wordSpacing = authored.wordSpacing;
        }
        if (IsSpecified(specified, StyleFlagTextIndent))
        {
            target.textIndent = authored.textIndent;
        }
        if (IsSpecified(specified, StyleFlagWhiteSpace))
        {
            target.whiteSpace = authored.whiteSpace;
        }
        if (IsSpecified(specified, StyleFlagTextTransform))
        {
            target.textTransform = authored.textTransform;
        }
        if (IsSpecified(specified, StyleFlagDirection))
        {
            target.direction = authored.direction;
        }
        if (IsSpecified(specified, StyleFlagFontStyle))
        {
            target.fontStyle = authored.fontStyle;
        }
        if (IsSpecified(specified, StyleFlagFontFamily))
        {
            target.fontFamily = authored.fontFamily;
        }
        if (IsSpecified(specified, StyleFlagFontKerning))
        {
            target.fontKerning = authored.fontKerning;
        }
        if (IsSpecified(specified, StyleFlagFontFeatures))
        {
            target.fontFeatures = authored.fontFeatures;
        }
        if (IsSpecified(specified, StyleFlagTextDecoration))
        {
            target.textDecoration = authored.textDecoration;
        }
        if (IsSpecified(specified, StyleFlagTextDecorationColor))
        {
            target.textDecorationColor = authored.textDecorationColor;
        }
        if (IsSpecified(specified, StyleFlagTextDecorationStyle))
        {
            target.textDecorationStyle = authored.textDecorationStyle;
        }
        if (IsSpecified(specified, StyleFlagTextDecorationThickness))
        {
            target.textDecorationThicknessKind = authored.textDecorationThicknessKind;
            target.textDecorationThickness = authored.textDecorationThickness;
        }
        if (IsSpecified(specified, StyleFlagTextUnderlineOffset))
        {
            target.textUnderlineOffsetKind = authored.textUnderlineOffsetKind;
            target.textUnderlineOffset = authored.textUnderlineOffset;
        }
        if (IsSpecified(specified, StyleFlagHyphens))
        {
            target.hyphens = authored.hyphens;
        }
        if (IsSpecified(specified, StyleFlagOverflowWrap))
        {
            target.overflowWrap = authored.overflowWrap;
        }
        if (IsSpecified(specified, StyleFlagWordBreak))
        {
            target.wordBreak = authored.wordBreak;
        }
        if (IsSpecified(specified, StyleFlagLineBreak))
        {
            target.lineBreak = authored.lineBreak;
        }
        if (IsSpecified(specified, StyleFlagTabSize))
        {
            target.tabSize = authored.tabSize;
            target.tabSizeLength = authored.tabSizeLength;
        }
        if (IsSpecified(specified, StyleFlagTextAlignLast))
        {
            target.textAlignLast = authored.textAlignLast;
            target.hasTextAlignLast = authored.hasTextAlignLast;
        }
        if (IsSpecified(specified, StyleFlagTextJustify))
        {
            target.textJustify = authored.textJustify;
        }
        if (IsSpecified(specified, StyleFlagWritingMode))
        {
            target.writingMode = authored.writingMode;
        }
        if (IsSpecified(specified, StyleFlagTextOrientation))
        {
            target.textOrientation = authored.textOrientation;
        }
        if (IsSpecified(specified, StyleFlagTextAlign))
        {
            target.textAlign = authored.textAlign;
        }
        if (IsSpecified(specified, StyleFlagDisplay))
        {
            target.display = authored.display;
        }

        // The out-of-flow model. Each offset is copied only when its own flag is
        // set, so an element that declares `position` and one offset keeps the
        // default `auto` on the other three, which is what puts it at its static
        // position on those axes.
        if (IsSpecified(specified, StyleFlagPosition))
        {
            target.position = authored.position;
        }
        if (IsSpecified(specified, StyleFlagTop))
        {
            target.insetTop = authored.insetTop;
        }
        if (IsSpecified(specified, StyleFlagRight))
        {
            target.insetRight = authored.insetRight;
        }
        if (IsSpecified(specified, StyleFlagBottom))
        {
            target.insetBottom = authored.insetBottom;
        }
        if (IsSpecified(specified, StyleFlagLeft))
        {
            target.insetLeft = authored.insetLeft;
        }
        if (IsSpecified(specified, StyleFlagZIndex))
        {
            target.zIndex = authored.zIndex;
            target.hasZIndex = authored.hasZIndex;
        }

        // The float model. `none` is the initial value for both, so an unstyled
        // box stays in the line flow and clears nothing.
        if (IsSpecified(specified, StyleFlagFloat))
        {
            target.floatKind = authored.floatKind;
        }
        if (IsSpecified(specified, StyleFlagClear))
        {
            target.clearKind = authored.clearKind;
        }

        // The flex model. None of these properties is inherited, so a box that
        // declares one of them gets the initial value for the rest whether or
        // not the cascade wrote it. Copying the group together is therefore the
        // same as copying each field under its own flag: the authored defaults
        // for the unwritten fields are the initial values.
        if (IsSpecified(specified, StyleFlagFlex))
        {
            target.flexDirection = authored.flexDirection;
            target.flexWrap = authored.flexWrap;
            target.justifyContent = authored.justifyContent;
            target.alignItems = authored.alignItems;
            target.alignSelf = authored.alignSelf;
            target.hasAlignSelf = authored.hasAlignSelf;
            target.alignContent = authored.alignContent;
            target.flexGrow = authored.flexGrow;
            target.flexShrink = authored.flexShrink;
            target.flexBasis = authored.flexBasis;
            target.hasFlexBasis = authored.hasFlexBasis;
            target.flexOrder = authored.flexOrder;
            target.rowGap = authored.rowGap;
            target.columnGap = authored.columnGap;
        }
        if (IsSpecified(specified, StyleFlagOverflowX))
        {
            target.overflowX = authored.overflowX;
        }
        if (IsSpecified(specified, StyleFlagOverflowY))
        {
            target.overflowY = authored.overflowY;
        }

        // A border needs a width to be visible: the native default decides when
        // `border-style` alone would have made a zero-width border paint nothing.
        const bool hasSideBorder = authored.borderWidth.top > 0.0f || authored.borderWidth.right > 0.0f ||
                                   authored.borderWidth.bottom > 0.0f || authored.borderWidth.left > 0.0f;

        if (hasSideBorder && authored.borderColor.alpha != 0)
        {
            target.hasBorder = 1;
        }

        // The composed style carries the author's mask, so a consumer can tell a
        // field the document set from one the category default supplied. Without
        // this the flags a text property is gated on would always read false and
        // the value would sit in the struct unread.
        target.specified = specified;
        target.sourceStyleId = authored.sourceStyleId;
    }

    std::size_t StyleTableProvider::AuthoredStyleCount(const CompiledDocument& document) const
    {
        return document.styles != nullptr ? document.styleCount : 0;
    }

    const CompiledStyle& StyleTableProvider::StyleFor(const CompiledDocument& document,
                                                     ElementIndex index,
                                                     RenderCategory category)
    {
        const CompiledStyle* authored = AuthoredStyleFor(document, index);
        const CompiledElement* element = ElementAt(document, index);

        // A run of text is set by the element it flowed out of. It has no element
        // of its own for a selector to match and no box of its own either, so it
        // takes the text properties of that element's resolved style: `text
        // <span>more</span>` is one paragraph's worth of type rather than two
        // sizes and, in a link, the words around the bold one are the link's
        // colour rather than the document's.
        const bool inheritsFromContainer =
            element != nullptr && element->HasFlag(ElementFlagTextRun) &&
            element->parent != NoElement && ElementAt(document, element->parent) != nullptr;

        if (authored == nullptr && !inheritsFromContainer)
        {
            return defaults.StyleFor(document, index, category);
        }

        if (merged.size() <= index)
        {
            merged.resize(static_cast<std::size_t>(index) + 1);
            mergedValid.resize(static_cast<std::size_t>(index) + 1, 0);
        }

        if (mergedValid[index] == 0)
        {
            CompiledStyle composed = defaults.StyleFor(document, index, category);

            if (inheritsFromContainer)
            {
                const CompiledElement* container = ElementAt(document, element->parent);
                InheritTextProperties(composed, StyleFor(document, element->parent,
                                                          CategoryFor(container->kind)));
            }
            else
            {
                if (authored != nullptr)
                {
                    ApplyAuthored(composed, *authored);
                }

                // A decoration reaches the elements painted inside the one that
                // states it, which covers a nested element's own text the way
                // the runs around it are already covered.
                const CompiledElement* container =
                    element != nullptr && element->parent != NoElement
                        ? ElementAt(document, element->parent)
                        : nullptr;

                if (container != nullptr)
                {
                    PropagateTextDecoration(composed, StyleFor(document, element->parent,
                                                              CategoryFor(container->kind)));
                }
            }

            merged[index] = composed;
            mergedValid[index] = 1;
            ++authoredElementCount;
        }

        return merged[index];
    }

    bool StyleTableProvider::HasOverride(const CompiledDocument& document, ElementIndex index) const
    {
        return AuthoredStyleFor(document, index) != nullptr;
    }

    const CompiledStyle& DefaultStyleProvider::CategoryStyle(RenderCategory category) const
    {
        const std::size_t index = static_cast<std::size_t>(category);
        if (index >= categoryStyles.size())
        {
            return categoryStyles[static_cast<std::size_t>(RenderCategory::Container)];
        }

        return categoryStyles[index];
    }

    bool DefaultStyleProvider::HasOverride(const CompiledDocument& document, ElementIndex index) const
    {
        (void)document;
        (void)index;

        // The category defaults are the whole answer here; the table that can
        // override them is StyleTableProvider's.
        return false;
    }

    const CompiledStyle& DefaultStyleProvider::StyleFor(const CompiledDocument& document,
                                                        ElementIndex index,
                                                        RenderCategory category)
    {
        const CompiledStyle& base = CategoryStyle(category);

        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return base;
        }

        const bool inlineContent = element->HasFlag(ElementFlagInline);
        const bool hidden = element->HasFlag(ElementFlagHidden);
        const bool rightToLeft = element->HasFlag(ElementFlagRightToLeft);

        float fontSize = base.fontSize;
        FontWeightKind weight = base.fontWeight;

        // A heading's size is a property of its level, which the compiler
        // normalised from the source element.
        if (category == RenderCategory::Heading)
        {
            static const float headingSizes[7] = { 24.0f, 32.0f, 24.0f, 18.5f, 16.0f, 13.5f, 11.0f };
            const std::uint16_t level = element->headingLevel <= 6 ? element->headingLevel : 1;
            fontSize = headingSizes[level];

            // Nested headings are laid out at the size of the level, not the tag.
            if (level >= 4)
            {
                weight = FontWeightKind::Bold;
            }
        }

        const bool needsAdjustment = inlineContent || hidden || rightToLeft ||
                                     fontSize != base.fontSize || weight != base.fontWeight ||
                                     element->HasFlag(ElementFlagDisabled);

        if (!needsAdjustment)
        {
            return base;
        }

        elementStyle = base;
        elementStyle.fontSize = fontSize;
        elementStyle.fontWeight = weight;

        if (inlineContent && IsBlockCategory(category))
        {
            // <span> and <p> share one native category; the compiler recorded
            // which of them flows inline.
            elementStyle.display = DisplayKind::Inline;
            elementStyle.margin = Edges::Zero();
        }

        if (hidden || element->HasFlag(ElementFlagInert))
        {
            elementStyle.display = DisplayKind::Hidden;
        }

        if (rightToLeft)
        {
            if (elementStyle.textAlign == TextAlignKind::Start)
            {
                elementStyle.textAlign = TextAlignKind::End;
            }
            else if (elementStyle.textAlign == TextAlignKind::End)
            {
                elementStyle.textAlign = TextAlignKind::Start;
            }
        }

        if (element->HasFlag(ElementFlagDisabled) && elementStyle.hasBackground != 0)
        {
            elementStyle.background = DisabledColor;
            elementStyle.foreground = PlaceholderColor;
            elementStyle.hasForeground = 1;
        }

        return elementStyle;
    }

    // Shade is used by the painters for hover and pressed states; it lives here so
    // the whole default appearance is in one translation unit.
    Color ShadeColor(Color color, float factor)
    {
        return Shade(color, factor);
    }
}
