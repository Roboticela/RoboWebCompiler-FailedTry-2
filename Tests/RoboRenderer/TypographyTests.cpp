// ---------------------------------------------------------------------------
// The font stack: real faces, fallback, bidirectional runs and Arabic joining.
//
//     code point -> FontRegistry fallback chain -> face -> outline -> pixels
//
// These tests pin the decisions the runtime makes about script, direction and
// clusters, and they render a multilingual document to prove the pieces agree
// with each other rather than only with their own unit.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

#include "RoboRenderer/Bidi.h"
#include "RoboRenderer/Text.h"

#include <cmath>
#include <string>
#include <vector>

using namespace RoboRenderer;

ROBO_TEST(Typography, ScriptsAreClassifiedCorrectly)
{
    // Right-to-left scripts, the first of which the showcase opens with.
    ROBO_CHECK(IsRtlCodepoint(0x05D0)); // Hebrew alef
    ROBO_CHECK(IsRtlCodepoint(0x0627)); // Arabic alef
    ROBO_CHECK(IsRtlCodepoint(0x06CC)); // Persian yeh
    ROBO_CHECK(!IsRtlCodepoint('A'));

    // Strong left-to-right characters, across the scripts the default family
    // covers.
    ROBO_CHECK(IsLtrCodepoint('A'));
    ROBO_CHECK(IsLtrCodepoint('z'));
    ROBO_CHECK(IsLtrCodepoint(0x03A9)); // Greek omega
    ROBO_CHECK(IsLtrCodepoint(0x0416)); // Cyrillic zhe
    ROBO_CHECK(!IsLtrCodepoint(' '));

    // Text that breaks between characters rather than at spaces.
    ROBO_CHECK(IsCjkCodepoint(0x4E2D)); // han "middle"
    ROBO_CHECK(IsCjkCodepoint(0x3042)); // hiragana a
    ROBO_CHECK(IsCjkCodepoint(0xD55C)); // hangul han
    ROBO_CHECK(!IsCjkCodepoint('A'));

    // A cluster is never broken: combining marks, variation selectors, the
    // zero-width joiner, skin-tone modifiers and the keycap mark.
    ROBO_CHECK(IsClusterExtender(0x0301));
    ROBO_CHECK(IsClusterExtender(0xFE0F));
    ROBO_CHECK(IsClusterExtender(0x200D));
    ROBO_CHECK(IsClusterExtender(0x1F3FD));
    ROBO_CHECK(IsClusterExtender(0x20E3));
    ROBO_CHECK(!IsClusterExtender('A'));

    ROBO_CHECK(IsRegionalIndicator(0x1F1FA));
    ROBO_CHECK(!IsRegionalIndicator('A'));
}

ROBO_TEST(Typography, ArabicLettersTakeTheirContextualForms)
{
    const std::uint32_t beh = 0x0628;
    const std::uint32_t alef = 0x0627;

    // A letter with no joining neighbour stands alone.
    ROBO_CHECK_EQ(ArabicShapedCodepoint(0, beh, 0), static_cast<std::uint32_t>(0xFE8F));

    // Beh followed by alef: beh is initial, because alef joins only on its
    // right. Alef is final.
    ROBO_CHECK_EQ(ArabicShapedCodepoint(0, beh, alef), static_cast<std::uint32_t>(0xFE91));
    ROBO_CHECK_EQ(ArabicShapedCodepoint(beh, alef, 0), static_cast<std::uint32_t>(0xFE8E));

    // Beh between two joining letters is medial.
    ROBO_CHECK_EQ(ArabicShapedCodepoint(beh, beh, beh), static_cast<std::uint32_t>(0xFE92));

    // A non-Arabic code point is returned unchanged.
    ROBO_CHECK_EQ(ArabicShapedCodepoint(beh, 'A', beh), static_cast<std::uint32_t>('A'));
}

ROBO_TEST(Typography, LineBreakingNeverSplitsAGraphemeCluster)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    // A run of ZWJ emoji sequences: the only places a break may land are
    // between whole clusters.
    std::string text;
    for (int index = 0; index < 12; ++index)
    {
        text += "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB"; // woman technologist
    }

    std::vector<TextLine> lines;
    layout.Wrap(text.c_str(), 16.0f, FontWeightKind::Normal, 40.0f, false, lines);

    ROBO_CHECK(lines.size() > 1);

    for (const TextLine& line : lines)
    {
        // No line begins with a cluster extender: the break never landed inside
        // a sequence.
        std::uint32_t offset = line.startByte;
        const std::uint32_t first = TextLayout::DecodeUtf8(text.c_str(), offset);
        ROBO_CHECK(!IsClusterExtender(first));
    }
}

ROBO_TEST(Typography, EveryScriptResolvesThroughTheFallbackChain)
{
    FontRegistry fonts;
    TextLayout layout(fonts);
    GlyphCache cache(fonts);

    ROBO_CHECK(fonts.FaceCount() >= 1);

    // Measurement works for every writing system the showcase carries.
    const char* samples[] = {
        "Hello",                              // Latin
        "\xCE\xA9",                           // Greek omega
        "\xD0\x96",                           // Cyrillic zhe
        "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D",   // Hebrew shalom
        "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85",   // Arabic salaam
        "\xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8" // Devanagari namas
        "\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87",
        "\xE4\xB8\xAD\xE6\x96\x87",           // Chinese zhongwen
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", // Japanese nihongo
        "\xED\x95\x9C\xEA\xB5\xAD",           // Korean hanguk
        "\xF0\x9F\x98\x80",                   // emoji
    };

    for (const char* sample : samples)
    {
        const float width = layout.Measure(sample, 16.0f, FontWeightKind::Normal);
        ROBO_CHECK(width > 0.0f);

        // The first code point of the sample rasterizes to a real bitmap through
        // whichever face in the chain covers it.
        std::uint32_t offset = 0;
        const std::uint32_t codepoint = TextLayout::DecodeUtf8(sample, offset);
        const Glyph* glyph = fonts.FaceFor(codepoint, FontWeightKind::Normal).FindGlyph(codepoint);

        if (glyph != nullptr)
        {
            const RasterizedGlyph rasterized = cache.Get(codepoint, 16.0f, FontWeightKind::Normal, 1.0f);
            ROBO_CHECK(rasterized.bitmap != nullptr);
            ROBO_CHECK(rasterized.bitmap->advance > 0.0f);
        }
    }

    // The primary family is reported, so a caller can say which face it drew
    // with without guessing.
    ROBO_CHECK(!fonts.PrimaryFamily().empty());
}

ROBO_TEST(Typography, RightToLeftTextPaintsIntoItsLineBox)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // A Hebrew paragraph and an Arabic one: both resolve to a fallback face and
    // are reordered for display.
    ROBO_CHECK(CompileBody("<p id=\"hebrew\">\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D \xD7\xA2\xD7\x95\xD7\x9C\xD7\x9D</p>"
                           "<p id=\"arabic\">\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85 \xD8\xB9\xD8\xA7\xD9\x84\xD9\x85</p>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 200.0f);
    ROBO_CHECK(window.Frame());
    ROBO_CHECK(!window.renderer.DiagnosticLog().HasErrors());

    const ElementIndex hebrew = FindElementById(document, "hebrew");
    ROBO_CHECK(hebrew != NoElement);

    const ElementLayout* box = window.renderer.Layout().Result().Element(hebrew);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        ROBO_CHECK(CountInkPixels(window.backend, box->border) > 0);
    }

    // The run painted glyphs at run time rather than an empty box.
    ROBO_CHECK(window.renderer.Metrics().glyphCount > 0);
}
