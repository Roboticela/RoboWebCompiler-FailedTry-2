// ---------------------------------------------------------------------------
// RoboRenderer - Unicode.
//
// The tables are generated from the Unicode Character Database and compiled in;
// nothing here reads a UCD file at run time. On top of them sit the algorithms
// text actually needs:
//
//     properties      general category, script, bidi class, combining class,
//                     line/grapheme/word/sentence break, East Asian width,
//                     emoji, brackets, mirroring
//     normalization   NFD / NFC / NFKD / NFKC
//     case            simple and full case mapping
//     segmentation    extended grapheme clusters, words, sentences (UAX #29)
//     line breaking   break opportunities (UAX #14)
//     bidi            paragraph direction, embedding levels, visual order (UAX #9)
//
// Everything is code point based. UTF-8 is decoded at the boundary and never
// leaks into the algorithms, because a "character" is a code point and a
// "visible character" is a grapheme cluster, and neither is a byte.
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RoboRenderer
{
    // --- property vocabularies --------------------------------------------
    //
    // The numeric values are the generator's indices into its tables, so the
    // order is fixed. `UnicodeUnknown*` is never a table value: it is what a
    // lookup returns for an unassigned code point.

    enum class GeneralCategory : std::uint8_t
    {
        Cc, Cf, Cn, Co, Cs, Ll, Lm, Lo, Lt, Lu,
        Mc, Me, Mn, Nd, Nl, No, Pc, Pd, Pe, Pf,
        Pi, Po, Ps, Sc, Sk, Sm, So, Zl, Zp, Zs,
        Unknown = 0xFF,
    };

    enum class BidiClass : std::uint8_t
    {
        L, R, AL, EN, ES, ET, AN, CS, NSM, BN,
        B, S, WS, ON, LRE, RLE, PDF, LRO, RLO,
        LRI, RLI, FSI, PDI,
        Unknown = 0xFF,
    };

    enum class LineBreakClass : std::uint8_t
    {
        XX, BK, CR, LF, CM, NL, SG, WJ, ZW, GL,
        SP, ZWJ, B2, BA, BB, HY, CB, CL, CP, EX,
        IN, NS, OP, QU, IS, NU, PO, PR, SY, AI,
        AL, CJ, EB, EM, H2, H3, HL, ID, JL, JV,
        JT, RI, SA, VF, VI, AP, AK, AS,
        Unknown = 0xFF,
    };

    enum class EastAsianWidth : std::uint8_t
    {
        N, A, H, W, F, Na,
        Unknown = 0xFF,
    };

    enum class GraphemeBreakClass : std::uint8_t
    {
        Other, CR, LF, Control, Extend, ZWJ, RegionalIndicator,
        Prepend, SpacingMark, L, V, T, LV, LVT,
        Unknown = 0xFF,
    };

    enum class WordBreakClass : std::uint8_t
    {
        Other, CR, LF, Newline, Extend, ZWJ, RegionalIndicator,
        Format, Katakana, HebrewLetter, ALetter, SingleQuote,
        DoubleQuote, MidNumLet, MidLetter, MidNum, Numeric,
        ExtendNumLet, WSegSpace,
        Unknown = 0xFF,
    };

    enum class SentenceBreakClass : std::uint8_t
    {
        Other, CR, LF, Sep, Format, Space, Lower, Upper, OLetter,
        Numeric, ATerm, SContinue, STerm, Close, Extend,
        Unknown = 0xFF,
    };

    // --- UTF-8 -------------------------------------------------------------

    /// Decodes one code point and advances `offset` past it. An invalid lead
    /// byte yields U+FFFD and consumes one byte, so a caller never loses its
    /// place in a malformed string.
    std::uint32_t DecodeUtf8(const char* text, std::uint32_t& offset);

    /// Decodes a whole UTF-8 string into code points.
    std::vector<std::uint32_t> Utf8ToCodepoints(const char* text);

    /// Appends a code point to a UTF-8 string.
    void AppendUtf8(std::string& out, std::uint32_t codepoint);

    /// Encodes a code point.
    std::string Utf8FromCodepoint(std::uint32_t codepoint);

    /// Code point count of a UTF-8 string.
    std::uint32_t CodepointCount(const char* text);

    /// Byte offset of a code point index (clamped to the end).
    std::uint32_t ByteOffsetForCodepointIndex(const char* text, std::uint32_t index);

    /// Code point index of a byte offset.
    std::uint32_t CodepointIndexForByteOffset(const char* text, std::uint32_t byteOffset);

    // --- properties --------------------------------------------------------

    GeneralCategory GeneralCategoryOf(std::uint32_t codepoint);
    BidiClass BidiClassOf(std::uint32_t codepoint);
    LineBreakClass LineBreakClassOf(std::uint32_t codepoint);
    GraphemeBreakClass GraphemeBreakOf(std::uint32_t codepoint);
    WordBreakClass WordBreakOf(std::uint32_t codepoint);
    SentenceBreakClass SentenceBreakOf(std::uint32_t codepoint);
    EastAsianWidth EastAsianWidthOf(std::uint32_t codepoint);

    /// Canonical combining class (0 for a starter).
    std::uint8_t CombiningClassOf(std::uint32_t codepoint);

    /// Script index, and its name (for diagnostics and selection hints).
    std::uint16_t ScriptOf(std::uint32_t codepoint);
    const char* ScriptName(std::uint16_t script);
    std::uint16_t ScriptIndexByName(const char* name);

    const char* GeneralCategoryName(GeneralCategory category);
    const char* BidiClassName(BidiClass bidiClass);

    bool IsEmoji(std::uint32_t codepoint);
    bool IsEmojiPresentation(std::uint32_t codepoint);
    bool IsEmojiModifier(std::uint32_t codepoint);
    bool IsEmojiModifierBase(std::uint32_t codepoint);
    bool IsEmojiComponent(std::uint32_t codepoint);
    bool IsExtendedPictographic(std::uint32_t codepoint);

    /// True for the characters Unicode calls White_Space.
    bool IsWhiteSpace(std::uint32_t codepoint);

    /// True for a mark (combining, spacing or enclosing).
    bool IsMark(std::uint32_t codepoint);

    /// True for a letter of any kind.
    bool IsLetter(std::uint32_t codepoint);

    /// True for a decimal digit.
    bool IsDigit(std::uint32_t codepoint);

    /// True for a format character or a variation selector: something that
    /// changes how text is presented but is not itself drawn.
    bool IsFormatOrVariationSelector(std::uint32_t codepoint);

    /// The mirror of a code point in the other direction (also its own value).
    std::uint32_t MirrorOf(std::uint32_t codepoint);

    /// Bracket pairing: true when the code point opens or closes a bracket.
    /// `pair` receives the code point it pairs with.
    bool IsOpenBracket(std::uint32_t codepoint, std::uint32_t& pair);
    bool IsCloseBracket(std::uint32_t codepoint, std::uint32_t& pair);

    // --- normalization (UAX #15) -------------------------------------------

    enum class NormalForm
    {
        None,
        NFD,
        NFC,
        NFKD,
        NFKC,
    };

    /// Canonical (or compatibility) decomposition of one code point. Hangul
    /// syllables decompose algorithmically, exactly as the standard requires.
    std::vector<std::uint32_t> DecomposeCodePoint(std::uint32_t codepoint, bool compatibility);

    /// Applies a normalization form to a code point sequence.
    std::vector<std::uint32_t> NormalizeCodePoints(const std::vector<std::uint32_t>& input, NormalForm form);

    /// Applies a normalization form to a UTF-8 string.
    std::string NormalizeUtf8(const char* text, NormalForm form);

    /// True when the sequence is already in the given form.
    bool IsNormalized(const std::vector<std::uint32_t>& input, NormalForm form);

    // --- case (UAX #21 / SpecialCasing) ------------------------------------

    std::uint32_t ToLowerCodePoint(std::uint32_t codepoint);
    std::uint32_t ToUpperCodePoint(std::uint32_t codepoint);
    std::uint32_t ToTitleCodePoint(std::uint32_t codepoint);

    std::string ToUpperUtf8(const char* text);
    std::string ToLowerUtf8(const char* text);
    std::string ToTitleUtf8(const char* text);

    /// Title-cases a string the way `text-transform: capitalize` means it: the
    /// first letter of every word is upper, the rest lower.
    std::string CapitalizeUtf8(const char* text);

    // --- segmentation (UAX #29) --------------------------------------------

    /// Byte offsets where extended grapheme clusters begin, starting with 0.
    std::vector<std::uint32_t> GraphemeClusterBoundaries(const char* text);

    /// Byte offsets where words begin. The first cluster is separate when it is
    /// not a word character, which is what a double-click selection wants.
    std::vector<std::uint32_t> WordBoundaries(const char* text);

    /// Byte offsets where sentences begin.
    std::vector<std::uint32_t> SentenceBoundaries(const char* text);

    // --- line breaking (UAX #14) -------------------------------------------

    /// Byte offsets at which a line may break. 0 is included; the end of the
    /// string is not, because breaking there is always allowed.
    std::vector<std::uint32_t> LineBreakOpportunities(const char* text);

    // --- bidi (UAX #9) -----------------------------------------------------

    /// Paragraph direction request.
    enum class ParagraphDirection
    {
        Auto,
        LeftToRight,
        RightToLeft,
    };

    /// One analyzed paragraph.
    struct BidiParagraph
    {
        std::uint8_t baseLevel = 0;
        /// Resolved embedding level per code point.
        std::vector<std::uint8_t> levels;
        /// Logical index of each code point in visual order, left to right.
        std::vector<std::uint32_t> visualToLogical;
        /// True when a code point is not itself displayed (explicit codes, BN).
        std::vector<std::uint8_t> removed;
    };

    /// The full Unicode Bidirectional Algorithm for one paragraph.
    BidiParagraph ResolveParagraph(const std::vector<std::uint32_t>& codepoints,
                                   ParagraphDirection direction);
}
