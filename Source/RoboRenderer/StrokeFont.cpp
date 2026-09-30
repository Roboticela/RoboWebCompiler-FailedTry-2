#include "RoboRenderer/Text.h"

#include <algorithm>
#include <cstddef>

namespace RoboRenderer
{
    namespace
    {
        /// The built-in face is described on a coarse design grid so the outlines
        /// stay readable and compact:
        ///
        ///     y = 7   cap height          |     x = 0 .. 4   typical letter box
        ///     y = 5   x-height            |     x = 5 .. 6   wide letters (m, w, M, W)
        ///     y = 0   baseline            |
        ///     y = -3  descender           |
        ///
        /// One grid unit is 100 font units, so the face has a 700 unit cap height
        /// on a 1000 unit em: a 16 logical unit font size gives the 11.2 unit cap
        /// height a real text face would.
        constexpr float GridUnit = 100.0f;

        /// How much wider the synthesized bold weight is than the regular one.
        constexpr float BoldAdvanceScale = 1.07f;

        /// A stroke outline: (x, y) grid pairs. A negative x lifts the pen and
        /// starts the next stroke, which is how letters with several strokes
        /// (A, E, i, …) are described. Point counts are derived from the array
        /// length, so the table cannot disagree with the data.
        struct GlyphSource
        {
            std::uint8_t advance;
            const std::int8_t* points;
            std::size_t pointCount;
        };

        template <std::size_t N>
        constexpr GlyphSource MakeGlyphSource(std::uint8_t advance, const std::int8_t (&glyph)[N])
        {
            return GlyphSource{ advance, glyph, N / 2 };
        }

        // --- ASCII control and punctuation --------------------------------
        constexpr std::int8_t gSpace[] = { -1,-1 };
        constexpr std::int8_t gExclam[] = { 0,2, 0,7, -1,-1, 0,0, 0,0 };
        constexpr std::int8_t gQuote[] = { 0,5, 0,7, -1,-1, 2,5, 2,7 };
        constexpr std::int8_t gHash[] = { 0,2, 4,2, -1,-1, 0,5, 4,5, -1,-1, 1,0, 1,7, -1,-1, 3,0, 3,7 };
        constexpr std::int8_t gDollar[] = { 4,6, 3,7, 1,7, 0,6, 0,5, 1,4, 3,4, 4,3, 4,2, 3,1, 1,1, 0,2,
                                            -1,-1, 2,8, 2,0 };
        constexpr std::int8_t gPercent[] = { 0,0, 4,7, -1,-1, 0,5, 1,5, 1,7, 0,7, 0,5,
                                             -1,-1, 3,0, 4,0, 4,2, 3,2, 3,0 };
        constexpr std::int8_t gAmpersand[] = { 3,0, 0,4, 0,6, 1,7, 2,6, 2,5, 0,2, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gApostrophe[] = { 0,7, 0,5 };
        constexpr std::int8_t gParenOpen[] = { 2,7, 1,6, 1,1, 2,0 };
        constexpr std::int8_t gParenClose[] = { 0,7, 1,6, 1,1, 0,0 };
        constexpr std::int8_t gAsterisk[] = { 2,3, 2,7, -1,-1, 0,4, 4,6, -1,-1, 0,6, 4,4 };
        constexpr std::int8_t gPlus[] = { 2,1, 2,5, -1,-1, 0,3, 4,3 };
        constexpr std::int8_t gComma[] = { 1,1, 0,-1 };
        constexpr std::int8_t gMinus[] = { 0,3, 4,3 };
        constexpr std::int8_t gPeriod[] = { 0,0, 0,0 };
        constexpr std::int8_t gSlash[] = { 0,0, 4,7 };

        // --- digits -------------------------------------------------------
        constexpr std::int8_t gDigitZero[] = { 1,0, 0,1, 0,6, 1,7, 3,7, 4,6, 4,1, 3,0, 1,0,
                                               -1,-1, 0,1, 4,6 };
        constexpr std::int8_t gDigitOne[] = { 1,6, 2,7, 2,0, -1,-1, 1,0, 3,0 };
        constexpr std::int8_t gDigitTwo[] = { 0,6, 1,7, 3,7, 4,6, 4,5, 0,0, 4,0 };
        constexpr std::int8_t gDigitThree[] = { 0,7, 4,7, 2,4, -1,-1, 2,4, 3,4, 4,3, 4,1, 3,0, 1,0, 0,1 };
        constexpr std::int8_t gDigitFour[] = { 3,0, 3,7, 0,2, 4,2 };
        constexpr std::int8_t gDigitFive[] = { 4,7, 0,7, 0,4, 3,4, 4,3, 4,1, 3,0, 1,0, 0,1 };
        constexpr std::int8_t gDigitSix[] = { 4,6, 3,7, 1,7, 0,6, 0,1, 1,0, 3,0, 4,1, 4,3, 3,4, 0,4 };
        constexpr std::int8_t gDigitSeven[] = { 0,7, 4,7, 1,0 };
        constexpr std::int8_t gDigitEight[] = { 1,4, 0,5, 0,6, 1,7, 3,7, 4,6, 4,5, 3,4, 1,4,
                                                0,3, 0,1, 1,0, 3,0, 4,1, 4,3, 3,4 };
        constexpr std::int8_t gDigitNine[] = { 0,1, 1,0, 3,0, 4,1, 4,6, 3,7, 1,7, 0,6, 0,4, 1,3, 4,3 };

        constexpr std::int8_t gColon[] = { 0,0, 0,0, -1,-1, 0,3, 0,3 };
        constexpr std::int8_t gSemicolon[] = { 0,0, 0,0, -1,-1, 1,3, 0,1 };
        constexpr std::int8_t gLess[] = { 4,4, 0,2, 4,0 };
        constexpr std::int8_t gEquals[] = { 0,2, 4,2, -1,-1, 0,4, 4,4 };
        constexpr std::int8_t gGreater[] = { 0,4, 4,2, 0,0 };
        constexpr std::int8_t gQuestion[] = { 0,6, 1,7, 3,7, 4,6, 4,5, 2,3, 2,2, -1,-1, 2,0, 2,0 };
        constexpr std::int8_t gAt[] = { 4,2, 4,5, 3,6, 1,6, 0,5, 0,1, 1,0, 3,0, 4,1,
                                        -1,-1, 4,2, 3,2, 2,2, 2,4, 3,4, 4,4 };

        // --- uppercase ----------------------------------------------------
        constexpr std::int8_t gUpperA[] = { 0,0, 2,7, 4,0, -1,-1, 1,3, 3,3 };
        constexpr std::int8_t gUpperB[] = { 0,0, 0,7, 3,7, 4,6, 4,5, 3,4, 0,4,
                                            -1,-1, 0,4, 3,4, 4,3, 4,1, 3,0, 0,0 };
        constexpr std::int8_t gUpperC[] = { 4,6, 3,7, 1,7, 0,6, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gUpperD[] = { 0,0, 0,7, -1,-1, 0,7, 3,7, 4,6, 4,1, 3,0, 0,0 };
        constexpr std::int8_t gUpperE[] = { 4,7, 0,7, 0,0, 4,0, -1,-1, 0,4, 3,4 };
        constexpr std::int8_t gUpperF[] = { 4,7, 0,7, 0,0, -1,-1, 0,4, 3,4 };
        constexpr std::int8_t gUpperG[] = { 4,6, 3,7, 1,7, 0,6, 0,1, 1,0, 3,0, 4,1, 4,3, 2,3 };
        constexpr std::int8_t gUpperH[] = { 0,0, 0,7, -1,-1, 4,0, 4,7, -1,-1, 0,4, 4,4 };
        constexpr std::int8_t gUpperI[] = { 0,0, 0,7 };
        constexpr std::int8_t gUpperJ[] = { 3,7, 3,1, 2,0, 1,0, 0,1 };
        constexpr std::int8_t gUpperK[] = { 0,0, 0,7, -1,-1, 4,7, 0,3, -1,-1, 1,4, 4,0 };
        constexpr std::int8_t gUpperL[] = { 0,7, 0,0, 4,0 };
        constexpr std::int8_t gUpperM[] = { 0,0, 0,7, 2,4, 4,7, 4,0 };
        constexpr std::int8_t gUpperN[] = { 0,0, 0,7, 4,0, 4,7 };
        constexpr std::int8_t gUpperO[] = { 1,0, 0,1, 0,6, 1,7, 3,7, 4,6, 4,1, 3,0, 1,0 };
        constexpr std::int8_t gUpperP[] = { 0,0, 0,7, 3,7, 4,6, 4,4, 3,3, 0,3 };
        constexpr std::int8_t gUpperQ[] = { 1,0, 0,1, 0,6, 1,7, 3,7, 4,6, 4,1, 3,0, 1,0,
                                            -1,-1, 2,2, 4,-1 };
        constexpr std::int8_t gUpperR[] = { 0,0, 0,7, 3,7, 4,6, 4,4, 3,3, 0,3, -1,-1, 2,3, 4,0 };
        constexpr std::int8_t gUpperS[] = { 4,6, 3,7, 1,7, 0,6, 0,5, 1,4, 3,4, 4,3, 4,1, 3,0, 1,0, 0,1 };
        constexpr std::int8_t gUpperT[] = { 0,7, 4,7, -1,-1, 2,7, 2,0 };
        constexpr std::int8_t gUpperU[] = { 0,7, 0,1, 1,0, 3,0, 4,1, 4,7 };
        constexpr std::int8_t gUpperV[] = { 0,7, 2,0, 4,7 };
        constexpr std::int8_t gUpperW[] = { 0,7, 1,0, 3,5, 5,0, 6,7 };
        constexpr std::int8_t gUpperX[] = { 0,0, 4,7, -1,-1, 0,7, 4,0 };
        constexpr std::int8_t gUpperY[] = { 0,7, 2,4, 4,7, -1,-1, 2,4, 2,0 };
        constexpr std::int8_t gUpperZ[] = { 0,7, 4,7, 0,0, 4,0 };

        constexpr std::int8_t gBracketOpen[] = { 2,7, 1,7, 1,0, 2,0 };
        constexpr std::int8_t gBackslash[] = { 0,7, 4,0 };
        constexpr std::int8_t gBracketClose[] = { 0,7, 1,7, 1,0, 0,0 };
        constexpr std::int8_t gCaret[] = { 0,5, 2,7, 4,5 };
        constexpr std::int8_t gUnderscore[] = { 0,-1, 4,-1 };
        constexpr std::int8_t gGrave[] = { 0,7, 1,5 };

        // --- lowercase ----------------------------------------------------
        constexpr std::int8_t gLowerA[] = { 4,5, 4,0, -1,-1, 4,4, 3,5, 1,5, 0,4, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gLowerB[] = { 0,0, 0,7, -1,-1, 0,4, 1,5, 3,5, 4,4, 4,1, 3,0, 1,0, 0,1 };
        constexpr std::int8_t gLowerC[] = { 4,4, 3,5, 1,5, 0,4, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gLowerD[] = { 4,0, 4,7, -1,-1, 4,4, 3,5, 1,5, 0,4, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gLowerE[] = { 0,3, 4,3, 4,4, 3,5, 1,5, 0,4, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gLowerF[] = { 3,7, 2,7, 1,6, 1,0, -1,-1, 0,5, 3,5 };
        constexpr std::int8_t gLowerG[] = { 4,5, 4,-2, 3,-3, 1,-3, 0,-2,
                                            -1,-1, 4,4, 3,5, 1,5, 0,4, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gLowerH[] = { 0,0, 0,7, -1,-1, 0,4, 1,5, 3,5, 4,4, 4,0 };
        constexpr std::int8_t gLowerI[] = { 0,0, 0,5, -1,-1, 0,7, 0,7 };
        constexpr std::int8_t gLowerJ[] = { 1,5, 1,-2, 0,-3, -1,-1, 1,7, 1,7 };
        constexpr std::int8_t gLowerK[] = { 0,0, 0,7, -1,-1, 3,5, 0,2, -1,-1, 1,2, 3,0 };
        constexpr std::int8_t gLowerL[] = { 0,0, 0,7 };
        constexpr std::int8_t gLowerM[] = { 0,0, 0,5, -1,-1, 0,4, 1,5, 2,4, 2,0,
                                            -1,-1, 2,4, 3,5, 4,4, 4,0 };
        constexpr std::int8_t gLowerN[] = { 0,0, 0,5, -1,-1, 0,4, 1,5, 3,5, 4,4, 4,0 };
        constexpr std::int8_t gLowerO[] = { 1,0, 0,1, 0,4, 1,5, 3,5, 4,4, 4,1, 3,0, 1,0 };
        constexpr std::int8_t gLowerP[] = { 0,-3, 0,5, -1,-1, 0,4, 1,5, 3,5, 4,4, 4,1, 3,0, 1,0, 0,1 };
        constexpr std::int8_t gLowerQ[] = { 4,-3, 4,5, -1,-1, 4,4, 3,5, 1,5, 0,4, 0,1, 1,0, 3,0, 4,1 };
        constexpr std::int8_t gLowerR[] = { 0,0, 0,5, -1,-1, 0,3, 1,5, 3,5, 4,4 };
        constexpr std::int8_t gLowerS[] = { 4,4, 3,5, 1,5, 0,4, 1,3, 3,3, 4,2, 4,1, 3,0, 1,0, 0,1 };
        constexpr std::int8_t gLowerT[] = { 1,6, 1,1, 2,0, 3,0, -1,-1, 0,5, 3,5 };
        constexpr std::int8_t gLowerU[] = { 0,5, 0,1, 1,0, 3,0, 4,1, -1,-1, 4,5, 4,0 };
        constexpr std::int8_t gLowerV[] = { 0,5, 2,0, 4,5 };
        constexpr std::int8_t gLowerW[] = { 0,5, 1,0, 2,4, 3,0, 4,5 };
        constexpr std::int8_t gLowerX[] = { 0,0, 4,5, -1,-1, 0,5, 4,0 };
        constexpr std::int8_t gLowerY[] = { 0,5, 2,0, -1,-1, 4,5, 1,-3 };
        constexpr std::int8_t gLowerZ[] = { 0,5, 4,5, 0,0, 4,0 };

        constexpr std::int8_t gBraceOpen[] = { 3,7, 2,7, 2,4, 1,3, 2,2, 2,0, 3,0 };
        constexpr std::int8_t gBar[] = { 0,-1, 0,8 };
        constexpr std::int8_t gBraceClose[] = { 0,7, 1,7, 1,4, 2,3, 1,2, 1,0, 0,0 };
        constexpr std::int8_t gTilde[] = { 0,3, 1,4, 3,2, 4,3 };

        /// ASCII 32 .. 126, in code point order.
        constexpr GlyphSource gSources[] = {
            MakeGlyphSource(4, gSpace),
            MakeGlyphSource(3, gExclam),
            MakeGlyphSource(4, gQuote),
            MakeGlyphSource(6, gHash),
            MakeGlyphSource(6, gDollar),
            MakeGlyphSource(6, gPercent),
            MakeGlyphSource(6, gAmpersand),
            MakeGlyphSource(3, gApostrophe),
            MakeGlyphSource(4, gParenOpen),
            MakeGlyphSource(4, gParenClose),
            MakeGlyphSource(6, gAsterisk),
            MakeGlyphSource(6, gPlus),
            MakeGlyphSource(3, gComma),
            MakeGlyphSource(5, gMinus),
            MakeGlyphSource(3, gPeriod),
            MakeGlyphSource(5, gSlash),
            MakeGlyphSource(6, gDigitZero),
            MakeGlyphSource(6, gDigitOne),
            MakeGlyphSource(6, gDigitTwo),
            MakeGlyphSource(6, gDigitThree),
            MakeGlyphSource(6, gDigitFour),
            MakeGlyphSource(6, gDigitFive),
            MakeGlyphSource(6, gDigitSix),
            MakeGlyphSource(6, gDigitSeven),
            MakeGlyphSource(6, gDigitEight),
            MakeGlyphSource(6, gDigitNine),
            MakeGlyphSource(3, gColon),
            MakeGlyphSource(3, gSemicolon),
            MakeGlyphSource(5, gLess),
            MakeGlyphSource(6, gEquals),
            MakeGlyphSource(5, gGreater),
            MakeGlyphSource(5, gQuestion),
            MakeGlyphSource(7, gAt),
            MakeGlyphSource(6, gUpperA),
            MakeGlyphSource(6, gUpperB),
            MakeGlyphSource(6, gUpperC),
            MakeGlyphSource(6, gUpperD),
            MakeGlyphSource(6, gUpperE),
            MakeGlyphSource(5, gUpperF),
            MakeGlyphSource(6, gUpperG),
            MakeGlyphSource(6, gUpperH),
            MakeGlyphSource(3, gUpperI),
            MakeGlyphSource(5, gUpperJ),
            MakeGlyphSource(6, gUpperK),
            MakeGlyphSource(5, gUpperL),
            MakeGlyphSource(7, gUpperM),
            MakeGlyphSource(6, gUpperN),
            MakeGlyphSource(6, gUpperO),
            MakeGlyphSource(6, gUpperP),
            MakeGlyphSource(6, gUpperQ),
            MakeGlyphSource(6, gUpperR),
            MakeGlyphSource(6, gUpperS),
            MakeGlyphSource(6, gUpperT),
            MakeGlyphSource(6, gUpperU),
            MakeGlyphSource(6, gUpperV),
            MakeGlyphSource(8, gUpperW),
            MakeGlyphSource(6, gUpperX),
            MakeGlyphSource(6, gUpperY),
            MakeGlyphSource(6, gUpperZ),
            MakeGlyphSource(4, gBracketOpen),
            MakeGlyphSource(5, gBackslash),
            MakeGlyphSource(4, gBracketClose),
            MakeGlyphSource(5, gCaret),
            MakeGlyphSource(5, gUnderscore),
            MakeGlyphSource(3, gGrave),
            MakeGlyphSource(6, gLowerA),
            MakeGlyphSource(6, gLowerB),
            MakeGlyphSource(6, gLowerC),
            MakeGlyphSource(6, gLowerD),
            MakeGlyphSource(6, gLowerE),
            MakeGlyphSource(4, gLowerF),
            MakeGlyphSource(6, gLowerG),
            MakeGlyphSource(6, gLowerH),
            MakeGlyphSource(3, gLowerI),
            MakeGlyphSource(4, gLowerJ),
            MakeGlyphSource(5, gLowerK),
            MakeGlyphSource(3, gLowerL),
            MakeGlyphSource(7, gLowerM),
            MakeGlyphSource(6, gLowerN),
            MakeGlyphSource(6, gLowerO),
            MakeGlyphSource(6, gLowerP),
            MakeGlyphSource(6, gLowerQ),
            MakeGlyphSource(5, gLowerR),
            MakeGlyphSource(6, gLowerS),
            MakeGlyphSource(5, gLowerT),
            MakeGlyphSource(6, gLowerU),
            MakeGlyphSource(6, gLowerV),
            MakeGlyphSource(6, gLowerW),
            MakeGlyphSource(6, gLowerX),
            MakeGlyphSource(6, gLowerY),
            MakeGlyphSource(6, gLowerZ),
            MakeGlyphSource(4, gBraceOpen),
            MakeGlyphSource(3, gBar),
            MakeGlyphSource(4, gBraceClose),
            MakeGlyphSource(6, gTilde)
        };

        constexpr std::uint32_t FirstCodepoint = 32;
        constexpr std::uint32_t LastCodepoint = 126;

        static_assert(sizeof(gSources) / sizeof(gSources[0]) == LastCodepoint - FirstCodepoint + 1,
                      "the built-in face must cover ASCII 32..126 exactly");
    }

    std::uint32_t StrokeFontFace::GlyphCount()
    {
        return static_cast<std::uint32_t>(LastCodepoint - FirstCodepoint + 1);
    }

    StrokeFontFace::StrokeFontFace(FontWeightKind weight)
    {
        weightValue = weight == FontWeightKind::Bold ? 700.0f : 400.0f;

        glyphs.resize(GlyphCount());

        for (std::size_t index = 0; index < glyphs.size(); ++index)
        {
            const GlyphSource& source = gSources[index];

            Glyph glyph;
            glyph.codepoint = static_cast<std::uint32_t>(FirstCodepoint + index);
            glyph.advance = static_cast<float>(source.advance) * GridUnit;

            // The face is a single stroke design, so the bold weight is
            // synthesized: the strokes are drawn thicker and the advances are
            // widened, which is what makes bold measure wider than regular.
            if (weight == FontWeightKind::Bold)
            {
                glyph.advance *= BoldAdvanceScale;
            }
            glyph.firstPoint = static_cast<std::uint32_t>(points.size());
            glyph.pointCount = static_cast<std::uint32_t>(source.pointCount);
            glyph.hasInk = false;

            bool firstPoint = true;
            for (std::size_t pointIndex = 0; pointIndex < source.pointCount; ++pointIndex)
            {
                const float x = static_cast<float>(source.points[pointIndex * 2]) * GridUnit;
                const float y = static_cast<float>(source.points[pointIndex * 2 + 1]) * GridUnit;

                points.push_back(StrokePoint{ x, y });

                if (x < 0.0f)
                {
                    // Pen up: the stroke ended here. The rasterizer reads it
                    // straight from the same array, so no second table is needed.
                    continue;
                }

                glyph.hasInk = true;

                if (firstPoint)
                {
                    glyph.x0 = glyph.x1 = x;
                    glyph.y0 = glyph.y1 = y;
                    firstPoint = false;
                }
                else
                {
                    glyph.x0 = std::min(glyph.x0, x);
                    glyph.x1 = std::max(glyph.x1, x);
                    glyph.y0 = std::min(glyph.y0, y);
                    glyph.y1 = std::max(glyph.y1, y);
                }
            }

            glyphs[index] = glyph;
        }

        // The replacement glyph for an unmapped code point: an empty box, so
        // missing text is visible rather than silently absent.
        missing = Glyph{};
        missing.codepoint = 0;
        missing.advance = 6.0f * GridUnit * (weight == FontWeightKind::Bold ? BoldAdvanceScale : 1.0f);
        missing.hasInk = true;
        missing.firstPoint = static_cast<std::uint32_t>(points.size());
        missing.pointCount = 5;
        missing.x0 = 0.0f;
        missing.y0 = 0.0f;
        missing.x1 = 4.0f * GridUnit;
        missing.y1 = 7.0f * GridUnit;

        const std::int8_t box[] = { 0,0, 4,0, 4,7, 0,7, 0,0 };
        for (std::uint32_t index = 0; index < missing.pointCount; ++index)
        {
            points.push_back(StrokePoint{ static_cast<float>(box[index * 2]) * GridUnit,
                                          static_cast<float>(box[index * 2 + 1]) * GridUnit });
        }
    }

    float StrokeFontFace::Weight() const
    {
        return weightValue;
    }

    const Glyph* StrokeFontFace::FindGlyph(std::uint32_t codepoint) const
    {
        if (codepoint < FirstCodepoint || codepoint > LastCodepoint)
        {
            return nullptr;
        }

        return &glyphs[codepoint - FirstCodepoint];
    }
}
