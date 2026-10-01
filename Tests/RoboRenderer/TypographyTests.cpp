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
#include "RoboRenderer/SvgGlyph.h"
#include "RoboRenderer/Text.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace RoboRenderer;

namespace
{
    /// The Type 2 number encoding: one byte below 108, two above, and the
    /// explicit 16-bit form only when a value cannot be written shorter.
    void PushCffInt(std::vector<std::uint8_t>& out, int value)
    {
        if (value >= -107 && value <= 107)
        {
            out.push_back(static_cast<std::uint8_t>(value + 139));
        }
        else if (value >= 108 && value <= 1131)
        {
            const int reduced = value - 108;
            out.push_back(static_cast<std::uint8_t>(247 + (reduced >> 8)));
            out.push_back(static_cast<std::uint8_t>(reduced & 0xFF));
        }
        else if (value >= -1131 && value <= -108)
        {
            const int reduced = -value - 108;
            out.push_back(static_cast<std::uint8_t>(251 + (reduced >> 8)));
            out.push_back(static_cast<std::uint8_t>(reduced & 0xFF));
        }
        else
        {
            out.push_back(28);
            out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(value & 0xFF));
        }
    }

    /// Builds the smallest CFF table that holds the given charstrings. The
    /// layout is fixed, so the Top DICT can name the CharStrings offset (28)
    /// before it is written.
    std::vector<std::uint8_t> BuildCff(const std::vector<std::vector<std::uint8_t>>& glyphs)
    {
        std::vector<std::uint8_t> out;

        const auto writeIndex = [&out](const std::vector<std::vector<std::uint8_t>>& items) {
            const std::uint32_t count = static_cast<std::uint32_t>(items.size());
            out.push_back(static_cast<std::uint8_t>(count >> 8));
            out.push_back(static_cast<std::uint8_t>(count & 0xFF));
            if (count == 0)
            {
                return;
            }

            std::uint32_t total = 0;
            for (const std::vector<std::uint8_t>& item : items)
            {
                total += static_cast<std::uint32_t>(item.size());
            }

            const int offSize = (total + 1) < 256 ? 1 : 2;
            out.push_back(static_cast<std::uint8_t>(offSize));

            const auto writeOffset = [&out, offSize](std::uint32_t value) {
                if (offSize == 1)
                {
                    out.push_back(static_cast<std::uint8_t>(value));
                }
                else
                {
                    out.push_back(static_cast<std::uint8_t>(value >> 8));
                    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
                }
            };

            std::uint32_t offset = 1;
            writeOffset(offset);
            for (const std::vector<std::uint8_t>& item : items)
            {
                offset += static_cast<std::uint32_t>(item.size());
                writeOffset(offset);
            }
            for (const std::vector<std::uint8_t>& item : items)
            {
                out.insert(out.end(), item.begin(), item.end());
            }
        };

        // Header
        out.push_back(1);
        out.push_back(0);
        out.push_back(4);
        out.push_back(4);

        // Name INDEX
        {
            std::vector<std::uint8_t> name = { 'T', 'e', 's', 't' };
            writeIndex({ name });
        }

        // Top DICT INDEX: CharStrings at 28 (the layout below fixes that).
        {
            std::vector<std::uint8_t> dict = { 0x1D, 0x00, 0x00, 0x00, 28, 0x11 };
            writeIndex({ dict });
        }

        const std::vector<std::vector<std::uint8_t>> empty;
        writeIndex(empty); // String INDEX
        writeIndex(empty); // Global Subr INDEX

        writeIndex(glyphs); // CharStrings INDEX
        return out;
    }
}

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

ROBO_TEST(Typography, CffCharstringsProduceOutlines)
{
    const std::vector<std::uint8_t> notdef = { 0x0E };

    std::vector<std::uint8_t> square;
    PushCffInt(square, 100);
    PushCffInt(square, 100);
    square.push_back(21); // rmoveto
    PushCffInt(square, 0);
    PushCffInt(square, 200);
    PushCffInt(square, 200);
    PushCffInt(square, 0);
    PushCffInt(square, 0);
    PushCffInt(square, -200);
    PushCffInt(square, -200);
    PushCffInt(square, 0);
    square.push_back(5);    // rlineto
    square.push_back(0x0E); // endchar

    const std::vector<std::uint8_t> bytes = BuildCff({ notdef, square });
    const std::unique_ptr<CffFont> font =
        CffFont::Load(bytes.data(), static_cast<std::uint32_t>(bytes.size()), 2);

    ROBO_CHECK(font != nullptr);
    if (font == nullptr)
    {
        return;
    }

    ROBO_CHECK(font->GlyphCount() == 2);

    GlyphPath path;
    ROBO_CHECK(font->Outline(1, path));
    ROBO_CHECK(path.ends.size() == 1);
    ROBO_CHECK(path.points.size() == 5);

    float x0 = 1.0e9f;
    float y0 = 1.0e9f;
    float x1 = -1.0e9f;
    float y1 = -1.0e9f;
    for (const ContourPoint& point : path.points)
    {
        x0 = std::min(x0, point.x);
        y0 = std::min(y0, point.y);
        x1 = std::max(x1, point.x);
        y1 = std::max(y1, point.y);
    }
    ROBO_CHECK(std::fabs(x0 - 100.0f) < 0.01f);
    ROBO_CHECK(std::fabs(y0 - 100.0f) < 0.01f);
    ROBO_CHECK(std::fabs(x1 - 300.0f) < 0.01f);
    ROBO_CHECK(std::fabs(y1 - 300.0f) < 0.01f);

    // The blank `.notdef` charstring yields no contour.
    ROBO_CHECK(!font->Outline(0, path));
    ROBO_CHECK(!font->Outline(7, path));
}

ROBO_TEST(Typography, CffCubicsAreFlattenedToContours)
{
    const std::vector<std::uint8_t> notdef = { 0x0E };

    std::vector<std::uint8_t> curve;
    PushCffInt(curve, 0);
    PushCffInt(curve, 0);
    curve.push_back(21); // rmoveto
    PushCffInt(curve, 0);
    PushCffInt(curve, 200);
    PushCffInt(curve, 200);
    PushCffInt(curve, 200);
    PushCffInt(curve, 200);
    PushCffInt(curve, 0);
    curve.push_back(8);    // rrcurveto
    curve.push_back(0x0E); // endchar

    const std::vector<std::uint8_t> bytes = BuildCff({ notdef, curve });
    const std::unique_ptr<CffFont> font =
        CffFont::Load(bytes.data(), static_cast<std::uint32_t>(bytes.size()), 2);

    ROBO_CHECK(font != nullptr);
    if (font == nullptr)
    {
        return;
    }

    GlyphPath path;
    ROBO_CHECK(font->Outline(1, path));
    ROBO_CHECK(path.ends.size() == 1);
    // A cubic is not a straight line: it flattens to several points.
    ROBO_CHECK(path.points.size() > 3);
}

ROBO_TEST(Typography, ItalicIsSelectedOrSynthesized)
{
    // Only the bundled upright Inter: the host has no Inter italic to borrow, so
    // `font-style: italic` must come back as a synthesized oblique.
    const std::string directory = std::string(ROBO_TEST_SOURCE_DIR) + "/Fonts/Inter";
    FontRegistry fonts({ directory });

    if (fonts.UsesStrokeFallback())
    {
        return;
    }

    const FontFace& italic = fonts.Face(FontWeightKind::Normal, nullptr, FontSlantKind::Italic);
    ROBO_CHECK(italic.IsItalic());
    ROBO_CHECK(fonts.HasSyntheticItalic());

    if (fonts.HasSyntheticItalic())
    {
        const FontFace& upright = fonts.Face(FontWeightKind::Normal);
        const Glyph* slanted = italic.FindGlyph('A');
        const Glyph* plain = upright.FindGlyph('A');

        ROBO_CHECK(slanted != nullptr && plain != nullptr);

        if (slanted != nullptr && plain != nullptr)
        {
            ROBO_CHECK(slanted->slant != 0.0f);
            // A shear moves no advance, so measurement is untouched.
            ROBO_CHECK(slanted->advance == plain->advance);
        }
    }
}

ROBO_TEST(Typography, DirectionOverridesTheFirstStrongCharacter)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<p class=\"rtl\">abc</p>", application,
                                      { { "app.css", ".rtl { direction: rtl }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 200.0f);
    ROBO_CHECK(window.Frame());

    float aX = 0.0f;
    float cX = 0.0f;
    bool sawA = false;
    bool sawC = false;

    for (const PaintGlyph& glyph : window.renderer.Paint().List().Glyphs())
    {
        if (glyph.codepoint == 'a')
        {
            aX = glyph.x;
            sawA = true;
        }
        else if (glyph.codepoint == 'c')
        {
            cX = glyph.x;
            sawC = true;
        }
    }

    ROBO_CHECK(sawA && sawC);

    // A right-to-left paragraph paints its first character at the right edge,
    // so 'a' sits to the right of 'c' even though the run is Latin.
    if (sawA && sawC)
    {
        ROBO_CHECK(aX > cX);
    }
}

ROBO_TEST(Typography, CssTextPropertiesReachTheRuntime)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // `letter-spacing` and `text-transform` are the two properties whose effect
    // is visible in the painted glyphs, so a document that compiles them into
    // the authored table but never applies them at layout is caught here.
    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">abc</p>", application,
        { { "app.css", ".styled { text-transform: uppercase; letter-spacing: 6px }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 200.0f);
    ROBO_CHECK(window.Frame());

    std::size_t uppercaseCount = 0;
    std::size_t lowercaseCount = 0;

    for (const PaintGlyph& glyph : window.renderer.Paint().List().Glyphs())
    {
        if (glyph.codepoint >= 'A' && glyph.codepoint <= 'Z')
        {
            ++uppercaseCount;
        }
        else if (glyph.codepoint >= 'a' && glyph.codepoint <= 'z')
        {
            ++lowercaseCount;
        }
    }

    ROBO_CHECK_EQ(lowercaseCount, static_cast<std::size_t>(0));
    ROBO_CHECK_EQ(uppercaseCount, static_cast<std::size_t>(3));
}

ROBO_TEST(Typography, SvgFillParsesColorsAndNone)
{
    SvgPaint paint;

    ROBO_CHECK(ParseSvgFill("#ff0000", paint));
    ROBO_CHECK(paint.kind == SvgPaint::Kind::Solid);
    ROBO_CHECK(paint.red > 0.99f && paint.green == 0.0f && paint.blue == 0.0f);

    ROBO_CHECK(ParseSvgFill("rgb(0, 128, 255)", paint));
    ROBO_CHECK(paint.green > 0.49f && paint.green < 0.51f);
    ROBO_CHECK(paint.blue > 0.99f);

    // `none` paints nothing, and a gradient reference is the caller's to resolve.
    ROBO_CHECK(ParseSvgFill("none", paint) == false);
    ROBO_CHECK(ParseSvgFill("url(#g)", paint) == false);
}

ROBO_TEST(Typography, SvgGlyphDocumentFlattensToFilledShapes)
{
    const std::string document = "<svg xmlns=\"http://www.w3.org/2000/svg\">"
                                 "<path d=\"M0 0 L100 0 L100 100 Z\" fill=\"#00ff00\"/>"
                                 "</svg>";

    std::vector<SvgShape> shapes;
    const bool rendered =
        RenderSvgGlyph(reinterpret_cast<const std::uint8_t*>(document.data()), document.size(), shapes);

    ROBO_CHECK(rendered);
    ROBO_CHECK_EQ(shapes.size(), static_cast<std::size_t>(1));

    if (!shapes.empty())
    {
        ROBO_CHECK_EQ(shapes[0].contours.size(), static_cast<std::size_t>(1));
        ROBO_CHECK(shapes[0].contours[0].size() >= 3);
        ROBO_CHECK(shapes[0].paint.green > 0.99f);

        // SVG y grows downwards; the font-space contour is flipped.
        bool flipped = false;
        for (const SvgPoint& point : shapes[0].contours[0])
        {
            if (point.y < -50.0f)
            {
                flipped = true;
            }
        }
        ROBO_CHECK(flipped);
    }
}

ROBO_TEST(Typography, SvgGlyphGradientResolvesAgainstTheShape)
{
    const std::string document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\">"
        "<defs><linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">"
        "<stop offset=\"0\" stop-color=\"#ff0000\"/>"
        "<stop offset=\"1\" stop-color=\"#0000ff\"/>"
        "</linearGradient></defs>"
        "<path d=\"M0 0 L100 0 L100 100 L0 100 Z\" fill=\"url(#g)\"/>"
        "</svg>";

    std::vector<SvgShape> shapes;
    ROBO_CHECK(
        RenderSvgGlyph(reinterpret_cast<const std::uint8_t*>(document.data()), document.size(), shapes));
    ROBO_CHECK_EQ(shapes.size(), static_cast<std::size_t>(1));

    if (!shapes.empty())
    {
        ROBO_CHECK(shapes[0].paint.kind == SvgPaint::Kind::Linear);
        ROBO_CHECK_EQ(shapes[0].paint.stops.size(), static_cast<std::size_t>(2));
        // The objectBoundingBox coordinates resolve to the shape's height.
        ROBO_CHECK(std::abs(shapes[0].paint.y1 - shapes[0].paint.y0) > 1.0f);
    }
}

ROBO_TEST(Typography, FontKerningCanBeTurnedOff)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    TextRunStyle kerned;
    TextRunStyle unkerned;
    unkerned.kerning = false;

    const char* pairs[] = { "AV", "AW", "To", "Yo", "WA", "Pa" };
    bool observed = false;

    for (const char* pair : pairs)
    {
        const float tight = layout.Measure(pair, 48.0f, FontWeightKind::Normal, &kerned);
        const float loose = layout.Measure(pair, 48.0f, FontWeightKind::Normal, &unkerned);

        // Removing the `kern` feature can only give the pair its own advances
        // back, never tighten it further.
        ROBO_CHECK(loose >= tight - 0.01f);

        if (loose > tight + 0.05f)
        {
            observed = true;
        }
    }

    ROBO_CHECK(observed);
}

ROBO_TEST(Typography, FontFeatureSettingsReachTheShaper)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    TextRunStyle defaults;
    TextRunStyle withoutLigatures;
    withoutLigatures.fontFeatures = "\"liga\" 0";

    // A ligature replaces several glyphs with one, so turning the feature off
    // can only make the run no narrower.
    const char* text = "fi fl ffi";
    const float withLigatures = layout.Measure(text, 48.0f, FontWeightKind::Normal, &defaults);
    const float plain = layout.Measure(text, 48.0f, FontWeightKind::Normal, &withoutLigatures);

    ROBO_CHECK(plain >= withLigatures - 0.01f);

    // The declaration is parsed the way CSS writes it, and `normal` parses to
    // nothing at all rather than to a feature named normal.
    FontFeatureSetting settings[8];
    ROBO_CHECK_EQ(TextLayout::ParseFontFeatures("\"liga\" 0, \"kern\" 1", settings, 8),
                  static_cast<std::size_t>(2));
    ROBO_CHECK_EQ(TextLayout::ParseFontFeatures("normal", settings, 8), static_cast<std::size_t>(0));
    ROBO_CHECK_EQ(TextLayout::ParseFontFeatures("", settings, 8), static_cast<std::size_t>(0));
    ROBO_CHECK_EQ(TextLayout::ParseFontFeatures("\"dlig\"", settings, 8), static_cast<std::size_t>(1));
    ROBO_CHECK_EQ(settings[0].value, static_cast<std::uint16_t>(1));
}

ROBO_TEST(Typography, CssKerningFeaturesAndDecorationReachTheRuntime)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">AV text</p>", application,
        { { "app.css", ".styled { font-kerning: none; font-feature-settings: \"liga\" 0; "
                        "text-decoration: underline }" } },
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    bool sawKerning = false;
    bool sawFeatures = false;
    bool sawDecoration = false;
    std::string featureText;

    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];

        if (IsSpecified(style.specified, StyleFlagFontKerning) && style.fontKerning == FontKerningKind::None)
        {
            sawKerning = true;
        }

        if (IsSpecified(style.specified, StyleFlagFontFeatures) && style.fontFeatures != 0)
        {
            sawFeatures = true;
            featureText = StringAt(document, style.fontFeatures);
        }

        if (IsSpecified(style.specified, StyleFlagTextDecoration) &&
            style.textDecoration == TextDecorationUnderline)
        {
            sawDecoration = true;
        }
    }

    ROBO_CHECK(sawKerning);
    ROBO_CHECK(sawFeatures);
    ROBO_CHECK(sawDecoration);
    ROBO_CHECK(featureText == "\"liga\" 0");

    // And the decoration is painted: the underline is a filled rectangle the
    // undecorated document does not have.
    const auto filledRects = [](::RoboRenderer::RoboRenderer& renderer) {
        std::size_t count = 0;

        for (const PaintCommand& command : renderer.Paint().List().Commands())
        {
            if (command.type == PaintCommandType::FillRect)
            {
                ++count;
            }
        }

        return count;
    };

    TestRenderer decorated(document, 400.0f, 200.0f);
    ROBO_CHECK(decorated.Frame());
    const std::size_t decoratedRects = filledRects(decorated.renderer);

    RoboTools::CompiledApplication plainApplication;
    ROBO_CHECK(CompileBodyWithSources("<p class=\"styled\">AV text</p>", plainApplication,
                                      { { "app.css", ".styled { font-kerning: none }" } }, error));

    TestRenderer undecorated(plainApplication.Document(), 400.0f, 200.0f);
    ROBO_CHECK(undecorated.Frame());

    ROBO_CHECK(decoratedRects > filledRects(undecorated.renderer));
}

ROBO_TEST(Typography, FontVariantLonghandsBecomeOpenTypeFeatures)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"caps\">Caps</p><p class=\"nums\">123</p><p class=\"none\">ffi</p>"
        "<p class=\"short\">Short</p>",
        application,
        { { "app.css",
            ".caps { font-variant-caps: all-small-caps }\n"
            ".nums { font-variant-numeric: tabular-nums slashed-zero }\n"
            ".none { font-variant-ligatures: none }\n"
            ".short { font-variant: small-caps tabular-nums }\n" } },
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    const auto featuresForText = [&](const char* text) {
        for (std::uint32_t index = 0; index < document.styleCount; ++index)
        {
            const CompiledStyle& style = document.styles[index];
            if (!IsSpecified(style.specified, StyleFlagFontFeatures) || style.fontFeatures == 0)
            {
                continue;
            }
            const std::string written = StringAt(document, style.fontFeatures);
            if (written.find(text) != std::string::npos)
            {
                return written;
            }
        }
        return std::string();
    };

    // `all-small-caps` needs both the lower-case and the upper-case small caps.
    const std::string caps = featuresForText("smcp");
    ROBO_CHECK(caps.find("\"smcp\" 1") != std::string::npos);
    ROBO_CHECK(caps.find("\"c2sc\" 1") != std::string::npos);

    const std::string nums = featuresForText("tnum");
    ROBO_CHECK(nums.find("\"tnum\" 1") != std::string::npos);
    ROBO_CHECK(nums.find("\"zero\" 1") != std::string::npos);

    // `font-variant-ligatures: none` is the shaper's `liga` off, and it reaches
    // the same channel `font-feature-settings` does.
    const std::string off = featuresForText("liga");
    ROBO_CHECK(off.find("\"liga\" 0") != std::string::npos);
    ROBO_CHECK(off.find("\"calt\" 0") != std::string::npos);

    // The `font-variant` shorthand routes each keyword to the longhand that
    // owns it, so one declaration reaches two different feature families. Note
    // it is `small-caps`, not `all-small-caps`, so no `c2sc` is written.
    bool sawShorthand = false;
    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];
        if (!IsSpecified(style.specified, StyleFlagFontFeatures) || style.fontFeatures == 0)
        {
            continue;
        }
        const std::string written = StringAt(document, style.fontFeatures);
        if (written.find("\"smcp\" 1") != std::string::npos &&
            written.find("\"tnum\" 1") != std::string::npos &&
            written.find("c2sc") == std::string::npos)
        {
            sawShorthand = true;
        }
    }
    ROBO_CHECK(sawShorthand);
}

ROBO_TEST(Typography, AnExplicitFeatureSettingBeatsAFontVariantLonghand)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">AV</p>", application,
        { { "app.css", ".styled { font-variant-ligatures: none; font-feature-settings: \"liga\" 1 }" } },
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];
        if (!IsSpecified(style.specified, StyleFlagFontFeatures) || style.fontFeatures == 0)
        {
            continue;
        }

        const std::string written = StringAt(document, style.fontFeatures);

        // The explicit declaration owns `liga`, so the longhand's `liga 0` is not
        // written a second time; the longhand still contributes its other tags.
        ROBO_CHECK(written.find("\"liga\" 1") != std::string::npos);
        ROBO_CHECK(written.find("\"liga\" 0") == std::string::npos);
        ROBO_CHECK(written.find("\"calt\" 0") != std::string::npos);
        return;
    }

    ROBO_CHECK(false);
}

ROBO_TEST(Typography, TextDecorationDetailReachesTheRuntime)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">underlined</p>", application,
        { { "app.css", ".styled { text-decoration: underline dotted 3px #123456; "
                        "text-underline-offset: 5px }" } },
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    bool sawStyle = false;
    bool sawThickness = false;
    bool sawOffset = false;

    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];

        if (IsSpecified(style.specified, StyleFlagTextDecorationStyle) &&
            style.textDecorationStyle == TextDecorationStyleKind::Dotted)
        {
            sawStyle = true;
        }

        if (IsSpecified(style.specified, StyleFlagTextDecorationThickness) &&
            style.textDecorationThicknessKind == TextDecorationThicknessKind::Length &&
            style.textDecorationThickness == 3.0f)
        {
            sawThickness = true;
        }

        if (IsSpecified(style.specified, StyleFlagTextUnderlineOffset) &&
            style.textUnderlineOffsetKind == TextUnderlineOffsetKind::Length &&
            style.textUnderlineOffset == 5.0f)
        {
            sawOffset = true;
        }
    }

    ROBO_CHECK(sawStyle);
    ROBO_CHECK(sawThickness);
    ROBO_CHECK(sawOffset);
}

ROBO_TEST(Typography, TheDecorationShorthandRoutesEachPartToItsLonghand)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">text</p>", application,
        { { "app.css", ".styled { text-decoration: overline wavy from-font }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    bool sawLine = false;
    bool sawStyle = false;
    bool sawThickness = false;

    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];

        if (IsSpecified(style.specified, StyleFlagTextDecoration) &&
            style.textDecoration == TextDecorationOverline)
        {
            sawLine = true;
        }

        if (IsSpecified(style.specified, StyleFlagTextDecorationStyle) &&
            style.textDecorationStyle == TextDecorationStyleKind::Wavy)
        {
            sawStyle = true;
        }

        if (IsSpecified(style.specified, StyleFlagTextDecorationThickness) &&
            style.textDecorationThicknessKind == TextDecorationThicknessKind::FromFont)
        {
            sawThickness = true;
        }
    }

    ROBO_CHECK(sawLine);
    ROBO_CHECK(sawStyle);
    ROBO_CHECK(sawThickness);
}

ROBO_TEST(Typography, ADottedDecorationPaintsMorePiecesThanASolidOne)
{
    const auto rectangleCount = [](RoboTools::CompiledApplication& application, float x, float y) {
        TestRenderer renderer(application.Document(), x, y);
        if (!renderer.Frame())
        {
            return std::size_t(0);
        }

        std::size_t count = 0;
        for (const PaintCommand& command : renderer.renderer.Paint().List().Commands())
        {
            if (command.type == PaintCommandType::FillRect)
            {
                ++count;
            }
        }
        return count;
    };

    std::string error;

    RoboTools::CompiledApplication solid;
    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">underlined text</p>", solid,
        { { "app.css", ".styled { text-decoration: underline solid }" } }, error));
    const std::size_t solidRects = rectangleCount(solid, 400.0f, 200.0f);

    RoboTools::CompiledApplication dotted;
    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">underlined text</p>", dotted,
        { { "app.css", ".styled { text-decoration: underline dotted }" } }, error));
    const std::size_t dottedRects = rectangleCount(dotted, 400.0f, 200.0f);

    // A dotted underline is a run of dots, so it takes more rectangles than the
    // single rectangle a solid underline draws.
    ROBO_CHECK(dottedRects > solidRects);
}

ROBO_TEST(Typography, LineBreakingAndJustificationPropertiesReachTheRuntime)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">alpha beta gamma</p>", application,
        { { "app.css", ".styled { hyphens: none; overflow-wrap: anywhere; word-break: break-all; "
                        "line-break: strict; tab-size: 4; text-align: justify; "
                        "text-justify: inter-word; text-align-last: center }" } },
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    bool sawHyphens = false;
    bool sawOverflowWrap = false;
    bool sawWordBreak = false;
    bool sawLineBreak = false;
    bool sawTabSize = false;
    bool sawJustify = false;
    bool sawJustifyKind = false;
    bool sawAlignLast = false;

    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];

        if (IsSpecified(style.specified, StyleFlagHyphens) && style.hyphens == HyphensKind::None)
        {
            sawHyphens = true;
        }
        if (IsSpecified(style.specified, StyleFlagOverflowWrap) &&
            style.overflowWrap == OverflowWrapKind::Anywhere)
        {
            sawOverflowWrap = true;
        }
        if (IsSpecified(style.specified, StyleFlagWordBreak) && style.wordBreak == WordBreakKind::BreakAll)
        {
            sawWordBreak = true;
        }
        if (IsSpecified(style.specified, StyleFlagLineBreak) && style.lineBreak == LineBreakKind::Strict)
        {
            sawLineBreak = true;
        }
        if (IsSpecified(style.specified, StyleFlagTabSize) && style.tabSize == 4.0f)
        {
            sawTabSize = true;
        }
        if (IsSpecified(style.specified, StyleFlagTextAlign) && style.textAlign == TextAlignKind::Justify)
        {
            sawJustify = true;
        }
        if (IsSpecified(style.specified, StyleFlagTextJustify) &&
            style.textJustify == TextJustifyKind::InterWord)
        {
            sawJustifyKind = true;
        }
        if (IsSpecified(style.specified, StyleFlagTextAlignLast) && style.hasTextAlignLast &&
            style.textAlignLast == TextAlignKind::Center)
        {
            sawAlignLast = true;
        }
    }

    ROBO_CHECK(sawHyphens);
    ROBO_CHECK(sawOverflowWrap);
    ROBO_CHECK(sawWordBreak);
    ROBO_CHECK(sawLineBreak);
    ROBO_CHECK(sawTabSize);
    ROBO_CHECK(sawJustify);
    ROBO_CHECK(sawJustifyKind);
    ROBO_CHECK(sawAlignLast);
}

ROBO_TEST(Typography, JustifyStretchesEveryLineButTheLast)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p id=\"para\">alpha beta gamma delta epsilon zeta eta theta</p>", application,
        { { "app.css", "#para { width: 120px; text-align: justify }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 200.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex paragraph = FindElementById(document, "para");
    ROBO_CHECK(paragraph != NoElement);

    float widestWordSpacing = 0.0f;
    float lastLineWordSpacing = -1.0f;
    std::size_t lineCount = 0;

    window.renderer.Layout().Result().ForEachLine(paragraph, [&](const LineLayout& line) {
        widestWordSpacing = std::max(widestWordSpacing, line.wordSpacing);
        lastLineWordSpacing = line.wordSpacing;
        ++lineCount;
    });

    // The paragraph must actually wrap, or there is no non-final line to justify.
    ROBO_CHECK(lineCount >= 2);
    ROBO_CHECK(widestWordSpacing > 0.0f);

    // The last line keeps its spaces at their natural width.
    ROBO_CHECK(lastLineWordSpacing == 0.0f);
}

ROBO_TEST(Typography, TabSizeScalesTheTabStop)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    TextRunStyle eight;
    eight.tabSize = 8.0f;

    TextRunStyle length;
    length.tabSizeLength = 40.0f;

    const float fourSpace = layout.TabAdvance(16.0f, FontWeightKind::Normal);
    const float eightSpace = layout.TabAdvance(16.0f, FontWeightKind::Normal, &eight);
    const float lengthStop = layout.TabAdvance(16.0f, FontWeightKind::Normal, &length);

    ROBO_CHECK(std::abs(eightSpace - fourSpace * 2.0f) < 0.001f);
    ROBO_CHECK(std::abs(lengthStop - 40.0f) < 0.001f);
}

ROBO_TEST(Typography, ASoftHyphenIsABreakOpportunity)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    // "alpha" + U+00AD + "beta". The concatenation stops the hex escape from
    // swallowing the 'b', which is a hex digit.
    const char* text = "alpha\xC2\xAD" "beta";

    TextRunStyle manual;
    manual.breakPolicy.hyphens = HyphensKind::Manual;

    const float narrowWidth = layout.Measure("alpha", 16.0f, FontWeightKind::Normal) + 1.0f;

    std::vector<TextLine> lines;
    layout.Wrap(text, 16.0f, FontWeightKind::Normal, narrowWidth, false, lines, &manual);

    ROBO_CHECK(lines.size() >= 2);

    if (lines.size() >= 2)
    {
        // The break lands at the soft hyphen, which is the sixth byte.
        ROBO_CHECK_EQ(lines[0].endByte, static_cast<std::uint32_t>(5));
    }

    // `hyphens: none` ignores the soft hyphen, so the word is one line when it
    // fits on one.
    TextRunStyle none;
    none.breakPolicy.hyphens = HyphensKind::None;

    std::vector<TextLine> unbroken;
    layout.Wrap(text, 16.0f, FontWeightKind::Normal, 1000.0f, false, unbroken, &none);
    ROBO_CHECK_EQ(unbroken.size(), static_cast<std::size_t>(1));

    // And the soft hyphen paints nothing: shaping skips it.
    std::vector<PositionedGlyph> glyphs;
    layout.ShapeRun(text, 0, static_cast<std::uint32_t>(std::char_traits<char>::length(text)), 16.0f,
                    FontWeightKind::Normal, glyphs, &manual);

    for (const PositionedGlyph& glyph : glyphs)
    {
        ROBO_CHECK(glyph.codepoint != 0x00AD);
    }
}

ROBO_TEST(Typography, WritingModeAndOrientationReachTheRuntime)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"styled\">vertical</p>", application,
        { { "app.css", ".styled { writing-mode: vertical-rl; text-orientation: upright }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    bool sawWritingMode = false;
    bool sawOrientation = false;

    for (std::uint32_t index = 0; index < document.styleCount; ++index)
    {
        const CompiledStyle& style = document.styles[index];

        if (IsSpecified(style.specified, StyleFlagWritingMode) &&
            style.writingMode == WritingModeKind::VerticalRl)
        {
            sawWritingMode = true;
        }

        if (IsSpecified(style.specified, StyleFlagTextOrientation) &&
            style.textOrientation == TextOrientationKind::Upright)
        {
            sawOrientation = true;
        }
    }

    ROBO_CHECK(sawWritingMode);
    ROBO_CHECK(sawOrientation);

    ROBO_CHECK(IsVerticalWritingMode(WritingModeKind::VerticalRl));
    ROBO_CHECK(IsRightToLeftColumnWritingMode(WritingModeKind::VerticalRl));
    ROBO_CHECK(!IsVerticalWritingMode(WritingModeKind::HorizontalTb));
}

ROBO_TEST(Typography, VerticalWritingModeLaysTextDownAColumn)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p id=\"vertical\">abcde</p>", application,
        { { "app.css", "#vertical { writing-mode: vertical-rl }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 200.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex paragraph = FindElementById(document, "vertical");
    ROBO_CHECK(paragraph != NoElement);

    bool sawVerticalLine = false;

    window.renderer.Layout().Result().ForEachLine(paragraph, [&](const LineLayout& line) {
        // The run walks down its column, so its extent is taller than the
        // column is wide.
        if (line.vertical && line.lineHeight > line.width)
        {
            sawVerticalLine = true;
        }
    });

    ROBO_CHECK(sawVerticalLine);

    const PaintCommand* textCommand = FindCommand(window.renderer, PaintCommandType::DrawText, paragraph);
    ROBO_CHECK(textCommand != nullptr);

    if (textCommand != nullptr)
    {
        ROBO_CHECK(textCommand->bounds.height > textCommand->bounds.width);
    }

    // `text-orientation` defaults to `mixed`, so the Latin letters are turned a
    // quarter turn and the backend has to rotate them.
    bool sawTurnedGlyph = false;

    for (const PaintGlyph& glyph : window.renderer.Paint().List().Glyphs())
    {
        if (glyph.rotation != 0)
        {
            sawTurnedGlyph = true;
        }
    }

    ROBO_CHECK(sawTurnedGlyph);
}

ROBO_TEST(Typography, UprightOrientationLeavesGlyphsStanding)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p id=\"vertical\">abcde</p>", application,
        { { "app.css", "#vertical { writing-mode: vertical-rl; text-orientation: upright }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 200.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex paragraph = FindElementById(document, "vertical");
    ROBO_CHECK(paragraph != NoElement);

    std::size_t glyphCount = 0;

    for (const PaintGlyph& glyph : window.renderer.Paint().List().Glyphs())
    {
        ++glyphCount;
        ROBO_CHECK_EQ(static_cast<int>(glyph.rotation), 0);
    }

    ROBO_CHECK(glyphCount > 0);
}

ROBO_TEST(Typography, AVerticalUnderlineRunsDownTheColumn)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<p id=\"vertical\">abcde</p>", application,
        { { "app.css", "#vertical { writing-mode: vertical-rl; text-decoration: underline }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 200.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex paragraph = FindElementById(document, "vertical");
    ROBO_CHECK(paragraph != NoElement);

    // The underline is a bar along the column, so it is taller than it is wide.
    bool sawVerticalBar = false;

    for (const PaintCommand& command : window.renderer.Paint().List().Commands())
    {
        if (command.type == PaintCommandType::FillRect && command.element == paragraph &&
            command.bounds.height > command.bounds.width)
        {
            sawVerticalBar = true;
        }
    }

    ROBO_CHECK(sawVerticalBar);
}

ROBO_TEST(Typography, UnderlineMetricsComeFromTheFace)
{
    FontRegistry fonts;
    TextLayout layout(fonts);

    const float small = layout.UnderlineThickness(100.0f, FontWeightKind::Normal);
    const float large = layout.UnderlineThickness(200.0f, FontWeightKind::Normal);

    ROBO_CHECK(small > 0.0f);

    // The thickness is a design-unit value scaled by the font size, so doubling
    // the size doubles it; the only exception is both hitting the one-pixel
    // floor, which a face with an unusually thin underline would.
    const bool doubles = std::abs(large - small * 2.0f) < 0.05f;
    ROBO_CHECK(doubles || (small == 1.0f && large == 1.0f));

    // An underline sits below the baseline.
    ROBO_CHECK(layout.UnderlinePosition(100.0f, FontWeightKind::Normal) < 0.0f);
}

ROBO_TEST(Typography, AVerticalFieldPaintsAHorizontalCaret)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<input id=\"field\" type=\"text\" value=\"hi\">", application,
        { { "app.css", "#field { writing-mode: vertical-rl }" } }, error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 320.0f, 240.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex field = FindElementById(document, "field");
    ROBO_CHECK(field != NoElement);

    ROBO_CHECK(window.ClickElement(field).paintChanged);
    ROBO_CHECK_EQ(window.renderer.State().Focused(), field);

    // Repaint with the field focused, so the frame carries the caret.
    ROBO_CHECK(window.Frame());

    // The caret is a thin bar crossing the column, so it is wide and one pixel
    // tall - the horizontal bar a vertical run needs, not the vertical bar a
    // horizontal run draws.
    bool sawHorizontalCaret = false;

    for (const PaintCommand& command : window.renderer.Paint().List().Commands())
    {
        if (command.type != PaintCommandType::FillRect || command.element != field)
        {
            continue;
        }

        if (command.bounds.height <= 1.5f && command.bounds.width >= 4.0f && command.bounds.width <= 40.0f)
        {
            sawHorizontalCaret = true;
        }
    }

    ROBO_CHECK(sawHorizontalCaret);
}




