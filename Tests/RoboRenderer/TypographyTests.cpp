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
#include "RoboRenderer/FontFile.h"
#include "RoboRenderer/Text.h"

#include <cmath>
#include <cstring>
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

ROBO_TEST(Typography, ColourGlyphsCarryTheirOwnPixels)
{
    FontRegistry fonts;
    const std::uint32_t emoji = 0x1F600;
    const FontFace& face = fonts.FaceFor(emoji, FontWeightKind::Normal);

    // A host with no colour font has nothing to assert: the emoji resolve to a
    // monochrome face and are rasterized as outlines like any other glyph.
    if (!face.HasColorGlyphs())
    {
        return;
    }

    // The covering face must be the colour one, not an earlier monochrome
    // symbol face: an emoji code point asks to be coloured.
    ROBO_CHECK(face.Covers(emoji));

    const Glyph* glyph = face.FindGlyph(emoji);
    ROBO_CHECK(glyph != nullptr);
    if (glyph == nullptr)
    {
        return;
    }

    std::vector<FontFace::ColorLayer> layers;
    const bool hasLayers = face.ColorLayers(glyph->glyphId, layers) && !layers.empty();

    GlyphCache cache(fonts);
    const RasterizedGlyph rasterized = cache.Get(emoji, 32.0f, FontWeightKind::Normal, 1.0f);
    ROBO_CHECK(rasterized.bitmap != nullptr);

    if (rasterized.bitmap == nullptr)
    {
        return;
    }

    // A glyph the face reports colour layers for rasterizes to colour pixels, and
    // at least one of them is actually painted.
    if (hasLayers)
    {
        ROBO_CHECK(rasterized.bitmap->color);
        ROBO_CHECK(rasterized.color != nullptr);

        std::size_t opaque = 0;
        if (rasterized.color != nullptr)
        {
            const std::size_t pixels =
                static_cast<std::size_t>(rasterized.bitmap->width) * rasterized.bitmap->height;
            for (std::size_t pixel = 0; pixel < pixels; ++pixel)
            {
                if (rasterized.color[pixel * 4 + 3] > 8)
                {
                    ++opaque;
                }
            }
        }
        ROBO_CHECK(opaque > 0);
    }
}

ROBO_TEST(Typography, EmojiPaintInColourEndToEnd)
{
    FontRegistry fonts;
    if (!fonts.FaceFor(0x1F600, FontWeightKind::Normal).HasColorGlyphs())
    {
        return;
    }

    RoboTools::CompiledApplication application;
    std::string error;
    std::vector<std::pair<std::string, std::string>> sheets = {
        { "app.css", ".big { font-size: 64px; color: #000000; }\n.page { background-color: #ffffff; }\n" }
    };

    // The emoji is written as its UTF-8 bytes, which is what a document really
    // carries.
    ROBO_CHECK(CompileBodyWithSources("<p class=\"big\" id=\"e\">\xF0\x9F\x98\x80</p>", application,
                                      sheets, error));

    TestRenderer window(application.Document(), 200.0f, 200.0f);
    ROBO_CHECK(window.Frame());

    const Image& framebuffer = window.backend.Framebuffer();
    std::size_t ink = 0;
    std::size_t colored = 0;

    for (std::uint32_t y = 0; y < framebuffer.height; ++y)
    {
        for (std::uint32_t x = 0; x < framebuffer.width; ++x)
        {
            const Color pixel = window.Pixel(x, y);
            if (pixel.red < 250 || pixel.green < 250 || pixel.blue < 250)
            {
                ++ink;
                const int red = pixel.red;
                const int green = pixel.green;
                const int blue = pixel.blue;
                const int high = std::max(red, std::max(green, blue));
                const int low = std::min(red, std::min(green, blue));
                if (high - low >= 8)
                {
                    ++colored;
                }
            }
        }
    }

    // The emoji painted ink, and that ink carried colour. A monochrome raster
    // would fail the second check, which is the whole point of the colour path.
    ROBO_CHECK(ink > 0);
    ROBO_CHECK(colored > 0);
}

ROBO_TEST(Typography, TheBundledFaceCarriesRealLayoutTables)
{
    const std::string path = std::string(ROBO_TEST_SOURCE_DIR) + "/Fonts/Inter/InterVariable.ttf";
    std::unique_ptr<SfntFont> font = SfntFont::LoadFile(path);

    // A checkout without the bundled OFL face still builds and runs; it simply
    // has nothing to assert about Inter's tables.
    if (!font)
    {
        return;
    }

    ROBO_CHECK(font->HasLayoutTables());

    const OtLayout* layout = font->Layout();
    ROBO_CHECK(layout != nullptr);

    if (layout != nullptr)
    {
        // A real text face carries both substitution and positioning.
        ROBO_CHECK(layout->HasSubstitution());
        ROBO_CHECK(layout->HasPositioning());
        ROBO_CHECK(layout->HasScript(OtLayout::ScriptTagForName("Latin")));
    }

    // A Latin script tag resolves, and a name with no OpenType system does not.
    ROBO_CHECK_EQ(OtLayout::ScriptTagForName("latin"), 0x6C61746Eu); // 'latn'
    ROBO_CHECK_EQ(OtLayout::ScriptTagForName("not a script"), 0u);
}

ROBO_TEST(Typography, ShapingIsTheSingleSourceOfAdvances)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    const char* text = "Hello, typography";
    std::vector<PositionedGlyph> glyphs;
    layout.ShapeRun(text, 0, static_cast<std::uint32_t>(std::strlen(text)), 16.0f,
                    FontWeightKind::Normal, glyphs);

    ROBO_CHECK(!glyphs.empty());

    float sum = 0.0f;
    for (const PositionedGlyph& glyph : glyphs)
    {
        ROBO_CHECK(glyph.face != nullptr);
        ROBO_CHECK(glyph.advance > 0.0f);
        ROBO_CHECK(glyph.byteStart < std::strlen(text));
        sum += glyph.advance;
    }

    // Measurement is exactly the sum of the shaped advances: line breaking and
    // painting cannot disagree about where a glyph lands.
    ROBO_CHECK(std::fabs(sum - layout.Measure(text, 16.0f, FontWeightKind::Normal)) < 0.001f);
}

ROBO_TEST(Typography, KerningNeverWidensAndUsuallyTightens)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    const char* pairs[] = { "AV", "AW", "To", "Yo", "WA", "Fi", "Pa" };
    bool tightened = false;

    for (const char* pair : pairs)
    {
        const std::string left(1, pair[0]);
        const std::string right(1, pair[1]);

        const float together = layout.Measure(pair, 48.0f, FontWeightKind::Normal);
        const float apart = layout.Measure(left.c_str(), 48.0f, FontWeightKind::Normal) +
                            layout.Measure(right.c_str(), 48.0f, FontWeightKind::Normal);

        // A kern pair can only bring glyphs closer, never push them apart.
        ROBO_CHECK(together <= apart + 0.01f);
        if (together < apart - 0.05f)
        {
            tightened = true;
        }
    }

    // At least one pair in the set is kerned in every real text face; a face
    // with no `GPOS` would leave them all apart, which is the signal that
    // shaping never ran.
    ROBO_CHECK(tightened);
}

ROBO_TEST(Typography, AVariableFaceInstantiatesItsAxes)
{
    const std::string path = std::string(ROBO_TEST_SOURCE_DIR) + "/Fonts/Inter/InterVariable.ttf";
    std::unique_ptr<SfntFont> font = SfntFont::LoadFile(path);
    if (!font)
    {
        return;
    }

    ROBO_CHECK(font->IsVariable());
    ROBO_CHECK(!font->Axes().empty());

    // Inter's variable file exposes a weight axis.
    bool hasWeight = false;
    for (const SfntFont::VariationAxis& axis : font->Axes())
    {
        if (axis.tag == 0x77676874u) // 'wght'
        {
            hasWeight = true;
        }
    }
    ROBO_CHECK(hasWeight);

    const std::uint16_t glyph = font->GlyphIndex('A');
    ROBO_CHECK(glyph != 0);

    const float regular = font->Advance(glyph);
    font->SetInstance({ 900.0f });
    ROBO_CHECK(font->HasInstance());
    const float black = font->Advance(glyph);

    // A heavier instance never has a narrower 'A'.
    ROBO_CHECK(black >= regular);
}

