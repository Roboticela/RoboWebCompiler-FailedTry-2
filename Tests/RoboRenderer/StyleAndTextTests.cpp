// ---------------------------------------------------------------------------
// Native style defaults and the text subsystem.
//
// Style is where a renderer normally depends on a stylesheet. These tests pin
// the resolved native defaults that layout and paint consume, and the text
// measurement, line breaking and glyph rasterization the runtime performs itself.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

#include <cmath>
#include <cstring>

using namespace RoboRenderer;

ROBO_TEST(Style, CategoryDefaultsDescribeTheHtmlFormattingModel)
{
    DefaultStyleProvider styles;

    // The document fills the viewport; a heading is bold and larger than body
    // text; a link is inline; a list indents its content.
    const CompiledStyle& documentStyle = styles.CategoryStyle(RenderCategory::Document);
    ROBO_CHECK_EQ(static_cast<int>(documentStyle.display), static_cast<int>(DisplayKind::Block));
    ROBO_CHECK(documentStyle.padding.left > 0.0f);

    const CompiledStyle& heading = styles.CategoryStyle(RenderCategory::Heading);
    ROBO_CHECK(heading.fontSize > DefaultStyleProvider::BaseFontSize);
    ROBO_CHECK_EQ(static_cast<int>(heading.fontWeight), static_cast<int>(FontWeightKind::Bold));

    const CompiledStyle& link = styles.CategoryStyle(RenderCategory::Link);
    ROBO_CHECK_EQ(static_cast<int>(link.display), static_cast<int>(DisplayKind::Inline));

    const CompiledStyle& list = styles.CategoryStyle(RenderCategory::List);
    ROBO_CHECK(list.padding.left > 0.0f);

    const CompiledStyle& button = styles.CategoryStyle(RenderCategory::Button);
    ROBO_CHECK_EQ(static_cast<int>(button.display), static_cast<int>(DisplayKind::InlineBlock));
    ROBO_CHECK(button.hasBorder == 1);
    ROBO_CHECK(button.borderRadius > 0.0f);

    const CompiledStyle& checkbox = styles.CategoryStyle(RenderCategory::Checkbox);
    ROBO_CHECK_EQ(static_cast<int>(checkbox.width.type), static_cast<int>(LayoutValueType::Fixed));
    ROBO_CHECK_EQ(static_cast<int>(checkbox.height.type), static_cast<int>(LayoutValueType::Fixed));
    ROBO_CHECK_EQ(checkbox.width.value, 14.0f);
}

ROBO_TEST(Style, CategoryPredicatesAreConsistent)
{
    // Every category is either block level or inline level, and nothing is both.
    for (int value = 0; value < static_cast<int>(RenderCategoryCount); ++value)
    {
        const RenderCategory category = static_cast<RenderCategory>(value);

        const bool block = IsBlockCategory(category);
        const bool inlineLevel = IsInlineCategory(category);

        ROBO_CHECK(!(block && inlineLevel));

        if (category == RenderCategory::Text || category == RenderCategory::Heading ||
            category == RenderCategory::PreformattedText)
        {
            ROBO_CHECK(IsTextCategory(category));
        }

        if (category == RenderCategory::Image || category == RenderCategory::Canvas)
        {
            ROBO_CHECK(IsReplacedCategory(category));
        }
    }
}

ROBO_TEST(Style, ControlStatesAreDerivedFromNativeAppearance)
{
    // Shading is how hover and press are expressed without a stylesheet.
    const Color base{ 200, 200, 200, 255 };

    const Color darker = ShadeColor(base, 0.5f);
    const Color lighter = ShadeColor(base, 1.2f);

    ROBO_CHECK(darker.red < base.red);
    ROBO_CHECK(lighter.red > base.red);
    ROBO_CHECK_EQ(static_cast<int>(darker.alpha), static_cast<int>(base.alpha));

    // The scale saturates rather than wrapping around.
    const Color saturated = ShadeColor(Color{ 250, 250, 250, 255 }, 4.0f);
    ROBO_CHECK_EQ(static_cast<int>(saturated.red), 255);

    const Color floored = ShadeColor(Color{ 5, 5, 5, 255 }, 0.0f);
    ROBO_CHECK_EQ(static_cast<int>(floored.red), 0);
}

ROBO_TEST(Text, MeasurementIsRealMetricsNotFixedWidth)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    const float narrow = layout.Measure("i", 16.0f, FontWeightKind::Normal);
    const float wide = layout.Measure("W", 16.0f, FontWeightKind::Normal);

    ROBO_CHECK(narrow > 0.0f);
    ROBO_CHECK(wide > 0.0f);

    // A fixed width font would make these equal; real advances do not.
    ROBO_CHECK(std::fabs(wide - narrow) > 0.5f);

    // Bold is wider than regular at the same size.
    ROBO_CHECK(layout.Measure("Bold text here", 16.0f, FontWeightKind::Bold) >
               layout.Measure("Bold text here", 16.0f, FontWeightKind::Normal));

    // Measurement scales with font size, and an empty run measures zero.
    ROBO_CHECK(layout.Measure("Measure me", 32.0f, FontWeightKind::Normal) >
               layout.Measure("Measure me", 16.0f, FontWeightKind::Normal));
    ROBO_CHECK_EQ(layout.Measure("", 16.0f, FontWeightKind::Normal), 0.0f);

    // Line height comes from the face's ascent, descent and line gap.
    ROBO_CHECK(layout.LineHeight(16.0f, FontWeightKind::Normal) > 16.0f);

    // A prefix measures exactly what the prefix is worth.
    const char* text = "abcdef";
    float prefix = 0.0f;
    for (int index = 1; index <= 6; ++index)
    {
        const float measured = layout.MeasurePrefix(text, static_cast<std::uint32_t>(index), 16.0f,
                                                    FontWeightKind::Normal);
        ROBO_CHECK(measured > prefix);
        prefix = measured;
    }

    ROBO_CHECK_EQ(prefix, layout.Measure(text, 16.0f, FontWeightKind::Normal));

    // A tab is four spaces wide, which is the white-space model the HTML-only
    // phase uses and what line breaking and painting must agree on.
    ROBO_CHECK_EQ(layout.TabAdvance(16.0f, FontWeightKind::Normal),
                  layout.Advance(' ', 16.0f, FontWeightKind::Normal) * 4.0f);
}

ROBO_TEST(Text, Utf8IsDecodedByCodePoint)
{
    std::uint32_t offset = 0;
    ROBO_CHECK_EQ(TextLayout::DecodeUtf8("A", offset), static_cast<std::uint32_t>('A'));
    ROBO_CHECK_EQ(offset, static_cast<std::uint32_t>(1));

    offset = 0;
    ROBO_CHECK_EQ(TextLayout::DecodeUtf8("\xC3\xA9", offset), static_cast<std::uint32_t>(0xE9));
    ROBO_CHECK_EQ(offset, static_cast<std::uint32_t>(2));

    offset = 0;
    ROBO_CHECK_EQ(TextLayout::DecodeUtf8("\xE2\x82\xAC", offset), static_cast<std::uint32_t>(0x20AC));
    ROBO_CHECK_EQ(offset, static_cast<std::uint32_t>(3));

    offset = 0;
    ROBO_CHECK_EQ(TextLayout::DecodeUtf8("\xF0\x9F\x99\x82", offset), static_cast<std::uint32_t>(0x1F642));
    ROBO_CHECK_EQ(offset, static_cast<std::uint32_t>(4));

    // An invalid lead byte becomes the replacement character instead of losing
    // the offset and desynchronising the rest of the string.
    offset = 0;
    ROBO_CHECK_EQ(TextLayout::DecodeUtf8("\xFFZ", offset), static_cast<std::uint32_t>(0xFFFD));
    ROBO_CHECK_EQ(offset, static_cast<std::uint32_t>(1));

    const char* mixed = "a\xC3\xA9\xE2\x82\xAC";
    ROBO_CHECK_EQ(TextLayout::CodepointCount(mixed), static_cast<std::uint32_t>(3));
    ROBO_CHECK_EQ(TextLayout::ByteOffsetForCodepoint(mixed, 2), static_cast<std::uint32_t>(3));
    ROBO_CHECK_EQ(TextLayout::CodepointIndexForByte(mixed, 3), static_cast<std::uint32_t>(2));
    ROBO_CHECK_EQ(TextLayout::ByteOffsetForCodepoint(mixed, 99),
                  static_cast<std::uint32_t>(std::strlen(mixed)));
}

ROBO_TEST(Text, LineBreakingHonoursBreaksWordsAndWhitespace)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    std::vector<TextLine> lines;

    // A hard break always starts a new line.
    layout.Wrap("first\nsecond", 16.0f, FontWeightKind::Normal, 1000.0f, false, lines);
    ROBO_CHECK_EQ(lines.size(), static_cast<std::size_t>(2));
    ROBO_CHECK(lines[0].hardBreak);

    // Narrow lines wrap at a space rather than overflowing.
    layout.Wrap("one two three four five six seven eight", 16.0f, FontWeightKind::Normal, 80.0f, false, lines);
    ROBO_CHECK(lines.size() > 1);

    for (const TextLine& line : lines)
    {
        ROBO_CHECK(line.width <= 80.0f + 0.001f);
        ROBO_CHECK(line.endByte > line.startByte);
    }

    // A word longer than the line breaks inside the word rather than overflowing.
    layout.Wrap("supercalifragilisticexpialidocious", 16.0f, FontWeightKind::Normal, 40.0f, false, lines);
    ROBO_CHECK(lines.size() > 1);

    for (const TextLine& line : lines)
    {
        ROBO_CHECK(line.width <= 40.0f + 0.001f);
    }

    // Preserved whitespace never wraps: <pre> and <textarea> need that.
    layout.Wrap("one two three four five six", 16.0f, FontWeightKind::Normal, 20.0f, true, lines);
    ROBO_CHECK_EQ(lines.size(), static_cast<std::size_t>(1));

    // An unbounded width wraps only on explicit breaks.
    layout.Wrap("one two three", 16.0f, FontWeightKind::Normal, 0.0f, false, lines);
    ROBO_CHECK_EQ(lines.size(), static_cast<std::size_t>(1));

    // An empty run still occupies one line box.
    layout.Wrap("", 16.0f, FontWeightKind::Normal, 100.0f, false, lines);
    ROBO_CHECK_EQ(lines.size(), static_cast<std::size_t>(1));
    ROBO_CHECK_EQ(lines[0].width, 0.0f);
}

ROBO_TEST(Text, GlyphCacheRasterizesOncePerGlyphAndSize)
{
    FontRegistry fonts;
    GlyphCache cache(fonts);

    const RasterizedGlyph first = cache.Get('A', 16.0f, FontWeightKind::Normal, 1.0f);
    ROBO_CHECK(first.bitmap != nullptr);
    ROBO_CHECK(!first.bitmap->blank);
    ROBO_CHECK(first.coverage != nullptr);
    ROBO_CHECK(first.bitmap->width > 0);
    ROBO_CHECK(first.bitmap->height > 0);
    ROBO_CHECK(first.bitmap->advance > 0.0f);

    const std::size_t afterFirst = cache.RasterizedGlyphCount();

    // Asking again is a lookup, not another rasterization.
    const RasterizedGlyph again = cache.Get('A', 16.0f, FontWeightKind::Normal, 1.0f);
    ROBO_CHECK_EQ(cache.RasterizedGlyphCount(), afterFirst);
    ROBO_CHECK(again.bitmap == first.bitmap);

    // A different size is a different bucket.
    const RasterizedGlyph larger = cache.Get('A', 32.0f, FontWeightKind::Normal, 1.0f);
    ROBO_CHECK(larger.bitmap != first.bitmap);
    ROBO_CHECK(larger.bitmap->advance > first.bitmap->advance);
    ROBO_CHECK(cache.SizeBucketCount() >= 2);

    // Bold is rasterized separately from regular.
    const RasterizedGlyph bold = cache.Get('A', 16.0f, FontWeightKind::Bold, 1.0f);
    ROBO_CHECK(bold.bitmap != first.bitmap);

    // A space has an advance but no ink.
    const RasterizedGlyph space = cache.Get(' ', 16.0f, FontWeightKind::Normal, 1.0f);
    ROBO_CHECK(space.bitmap != nullptr);
    ROBO_CHECK(space.bitmap->blank);
    ROBO_CHECK(space.bitmap->advance > 0.0f);
    ROBO_CHECK(space.coverage == nullptr);

    // Emoji resolve through the fallback chain to a face that covers them, so
    // they rasterize like any other glyph.
    const RasterizedGlyph emoji = cache.Get(0x1F642, 16.0f, FontWeightKind::Normal, 1.0f);

    if (!fonts.UsesStrokeFallback())
    {
        ROBO_CHECK(emoji.bitmap != nullptr);
        ROBO_CHECK(emoji.bitmap->advance > 0.0f);
    }

    // A code point no face in the chain maps still has no coverage: the caller
    // paints nothing rather than an invented glyph.
    const RasterizedGlyph unmapped = cache.Get(0x10FFFD, 16.0f, FontWeightKind::Normal, 1.0f);
    ROBO_CHECK(unmapped.bitmap == nullptr);
    ROBO_CHECK(unmapped.coverage == nullptr);

    const std::size_t coverageBytes = cache.CoverageBytes();
    ROBO_CHECK(coverageBytes > 0);

    cache.Clear();
    ROBO_CHECK_EQ(cache.SizeBucketCount(), static_cast<std::size_t>(0));
    ROBO_CHECK_EQ(cache.CoverageBytes(), static_cast<std::size_t>(0));
}

ROBO_TEST(Text, HitTestingMapsAPositionToACodePoint)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    const char* text = "hello";

    // Before the first glyph, at the start of the string.
    ROBO_CHECK_EQ(layout.HitTestOffset(text, -20.0f, 16.0f, FontWeightKind::Normal),
                  static_cast<std::uint32_t>(0));

    // Well past the end, at the end of the string.
    ROBO_CHECK_EQ(layout.HitTestOffset(text, 500.0f, 16.0f, FontWeightKind::Normal),
                  static_cast<std::uint32_t>(std::strlen(text)));

    // The middle of the string lands in the middle, not at an edge.
    const float middle = layout.Measure(text, 16.0f, FontWeightKind::Normal) * 0.5f;
    const std::uint32_t offset = layout.HitTestOffset(text, middle, 16.0f, FontWeightKind::Normal);
    ROBO_CHECK(offset > 0);
    ROBO_CHECK(offset < std::strlen(text));
}
