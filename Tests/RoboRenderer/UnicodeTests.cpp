// ---------------------------------------------------------------------------
// Unicode: properties, normalization, case, segmentation, line breaking, bidi.
//
// The tables are generated, so these tests are what stops a regenerated table
// from silently changing what the runtime believes about a code point.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

#include "RoboRenderer/Unicode.h"

#include <string>
#include <vector>

using namespace RoboRenderer;

namespace
{
    std::string CodepointsToUtf8(const std::vector<std::uint32_t>& codepoints)
    {
        std::string result;
        for (const std::uint32_t codepoint : codepoints)
        {
            AppendUtf8(result, codepoint);
        }
        return result;
    }

    std::vector<std::uint32_t> Utf8(const char* text)
    {
        return Utf8ToCodepoints(text);
    }
}

ROBO_TEST(Unicode, PropertiesAreWhatTheUcdSays)
{
    ROBO_CHECK(GeneralCategoryOf('A') == GeneralCategory::Lu);
    ROBO_CHECK(GeneralCategoryOf('a') == GeneralCategory::Ll);
    ROBO_CHECK(GeneralCategoryOf('7') == GeneralCategory::Nd);
    ROBO_CHECK(GeneralCategoryOf(' ') == GeneralCategory::Zs);
    ROBO_CHECK(GeneralCategoryOf(0x0301) == GeneralCategory::Mn); // combining acute
    ROBO_CHECK(GeneralCategoryOf(0x4E2D) == GeneralCategory::Lo); // han
    ROBO_CHECK(GeneralCategoryOf(0x10FFFD) == GeneralCategory::Co); // private use

    ROBO_CHECK(BidiClassOf('A') == BidiClass::L);
    ROBO_CHECK(BidiClassOf(0x05D0) == BidiClass::R);   // Hebrew alef
    ROBO_CHECK(BidiClassOf(0x0627) == BidiClass::AL);  // Arabic alef
    ROBO_CHECK(BidiClassOf('7') == BidiClass::EN);

    ROBO_CHECK(std::string(ScriptName(ScriptOf(0x05D0))) == "Hebrew");
    ROBO_CHECK(std::string(ScriptName(ScriptOf(0x0627))) == "Arabic");

    ROBO_CHECK(IsLetter(0x0416));    // Cyrillic zhe
    ROBO_CHECK(IsDigit(0x0661));     // Arabic-Indic one
    ROBO_CHECK(IsWhiteSpace(0x3000));
    ROBO_CHECK(IsMark(0x0902));      // Devanagari anusvara

    ROBO_CHECK(IsEmoji(0x1F600));
    ROBO_CHECK(IsEmojiPresentation(0x1F600));
    ROBO_CHECK(IsEmojiModifier(0x1F3FD));
    ROBO_CHECK(IsEmojiModifierBase(0x1F44D));
    ROBO_CHECK(IsExtendedPictographic(0x1F600));

    ROBO_CHECK(CombiningClassOf(0x0301) == 230);
    ROBO_CHECK(CombiningClassOf('a') == 0);

    std::uint32_t pair = 0;
    ROBO_CHECK(IsOpenBracket('(', pair) && pair == ')');
    ROBO_CHECK(IsCloseBracket(']', pair) && pair == '[');
    ROBO_CHECK(MirrorOf('(') == ')');
    ROBO_CHECK(MirrorOf('A') == 'A');

    ROBO_CHECK(EastAsianWidthOf(0x4E2D) == EastAsianWidth::W);
}

ROBO_TEST(Unicode, NormalizationComposesAndDecomposes)
{
    // U+00E9 is e plus a combining acute.
    const std::string decomposed = CodepointsToUtf8({ 0x0065, 0x0301 });
    const std::string composed = "\xC3\xA9";

    ROBO_CHECK(NormalizeUtf8(decomposed.c_str(), NormalForm::NFC) == composed);
    ROBO_CHECK(NormalizeUtf8(composed.c_str(), NormalForm::NFD) == decomposed);
    ROBO_CHECK(IsNormalized(Utf8(composed.c_str()), NormalForm::NFC));
    ROBO_CHECK(!IsNormalized(Utf8(decomposed.c_str()), NormalForm::NFC));

    // Hangul composes algorithmically: L + V is a syllable.
    ROBO_CHECK(NormalizeUtf8("\xE1\x84\x80\xE1\x85\xA1", NormalForm::NFC) == "\xEA\xB0\x80");

    // A compatibility decomposition folds the ligature to its letters.
    ROBO_CHECK(NormalizeUtf8("\xEF\xAC\x83", NormalForm::NFKC) == "ffi");
    ROBO_CHECK(NormalizeUtf8("\xEF\xAC\x83", NormalForm::NFC) == "\xEF\xAC\x83");
}

ROBO_TEST(Unicode, CaseMapsTheHardCases)
{
    ROBO_CHECK(ToUpperUtf8("hello") == "HELLO");
    ROBO_CHECK(ToLowerUtf8("HELLO") == "hello");
    ROBO_CHECK(ToUpperUtf8("stra\xC3\x9F" "e") == "STRASSE"); // sharp s is two letters upper
    ROBO_CHECK(CapitalizeUtf8("hello world") == "Hello World");
    ROBO_CHECK(ToUpperCodePoint('a') == 'A');
    ROBO_CHECK(ToLowerCodePoint(0x0416) == 0x0436); // Cyrillic zhe
}

ROBO_TEST(Unicode, GraphemeClustersGroupWhatBelongsTogether)
{
    // A base and its combining mark are one cluster.
    const std::vector<std::uint32_t> combining = GraphemeClusterBoundaries("a\xCC\x81" "b");
    ROBO_CHECK_EQ(combining.size(), static_cast<std::size_t>(2));

    // A flag is one cluster: two regional indicators.
    const std::vector<std::uint32_t> flag = GraphemeClusterBoundaries("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8");
    ROBO_CHECK_EQ(flag.size(), static_cast<std::size_t>(1));

    // A family is one cluster: a zero-width-joiner sequence of pictographs.
    const std::vector<std::uint32_t> family = GraphemeClusterBoundaries(
        "\xF0\x9F\x91\xA8" "\xE2\x80\x8D" "\xF0\x9F\x91\xA9" "\xE2\x80\x8D"
        "\xF0\x9F\x91\xA7" "\xE2\x80\x8D" "\xF0\x9F\x91\xA6");
    ROBO_CHECK_EQ(family.size(), static_cast<std::size_t>(1));

    // A skin-tone modifier stays with its base.
    const std::vector<std::uint32_t> tone = GraphemeClusterBoundaries(
        "\xF0\x9F\x91\x8D" "\xF0\x9F\x8F\xBD");
    ROBO_CHECK_EQ(tone.size(), static_cast<std::size_t>(1));

    // Two separate emoji are two clusters.
    const std::vector<std::uint32_t> pair = GraphemeClusterBoundaries(
        "\xF0\x9F\x98\x80\xF0\x9F\x98\x81");
    ROBO_CHECK_EQ(pair.size(), static_cast<std::size_t>(2));
}

ROBO_TEST(Unicode, LineBreakingFindsTheRealOpportunities)
{
    // A space is an opportunity; the word before it is not.
    const std::vector<std::uint32_t> spaced = LineBreakOpportunities("one two");
    ROBO_CHECK(spaced.size() >= 2);
    ROBO_CHECK_EQ(spaced.front(), static_cast<std::uint32_t>(0));

    bool afterSpace = false;
    for (const std::uint32_t offset : spaced)
    {
        if (offset == 4)
        {
            afterSpace = true;
        }
    }
    ROBO_CHECK(afterSpace);

    // CJK breaks between ideographs even with no space.
    const std::vector<std::uint32_t> cjk = LineBreakOpportunities(
        "\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97");
    ROBO_CHECK(cjk.size() >= 2);

    // A word with no spaces offers no opportunity before its end.
    const std::vector<std::uint32_t> word = LineBreakOpportunities("supercalifragilistic");
    ROBO_CHECK_EQ(word.size(), static_cast<std::size_t>(1));
}

ROBO_TEST(Unicode, BidiResolvesDirectionAndOrder)
{
    // Pure Latin: level 0, unchanged order.
    const BidiParagraph latin = ResolveParagraph(Utf8("abc"), ParagraphDirection::Auto);
    ROBO_CHECK_EQ(latin.baseLevel, static_cast<std::uint8_t>(0));

    for (const std::uint32_t level : latin.levels)
    {
        ROBO_CHECK_EQ(level, static_cast<std::uint8_t>(0));
    }

    // Pure Hebrew: base level 1 and visual order reversed.
    const std::vector<std::uint32_t> hebrewCodepoints = { 0x05D0, 0x05D1, 0x05D2 };
    const BidiParagraph hebrew = ResolveParagraph(hebrewCodepoints, ParagraphDirection::Auto);
    ROBO_CHECK_EQ(hebrew.baseLevel, static_cast<std::uint8_t>(1));
    ROBO_CHECK_EQ(hebrew.visualToLogical.size(), static_cast<std::size_t>(3));
    ROBO_CHECK_EQ(hebrew.visualToLogical[0], static_cast<std::uint32_t>(2));
    ROBO_CHECK_EQ(hebrew.visualToLogical[2], static_cast<std::uint32_t>(0));

    // An explicit direction overrides the first strong character.
    const BidiParagraph forced = ResolveParagraph(hebrewCodepoints, ParagraphDirection::LeftToRight);
    ROBO_CHECK_EQ(forced.baseLevel, static_cast<std::uint8_t>(0));

    // Latin inside Hebrew keeps its own level, which is what makes a mixed run
    // render in the right place rather than merely reversed.
    const std::vector<std::uint32_t> mixed = { 0x05D0, 'a', 'b', 0x05D1 };
    const BidiParagraph resolved = ResolveParagraph(mixed, ParagraphDirection::Auto);
    ROBO_CHECK_EQ(resolved.baseLevel, static_cast<std::uint8_t>(1));

    bool sawEven = false;
    for (const std::uint32_t level : resolved.levels)
    {
        if ((level & 1) == 0)
        {
            sawEven = true;
        }
    }
    ROBO_CHECK(sawEven);
}
