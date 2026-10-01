// ---------------------------------------------------------------------------
// RoboRenderer - Unicode algorithms.
//
// See Unicode.h for the contract. Everything below is code-point based: the
// generated tables answer "what is this code point", and the algorithms answer
// "what does this sequence mean". UTF-8 is decoded at the boundary.
// ---------------------------------------------------------------------------
#include "RoboRenderer/Unicode.h"

#include "Generated/UnicodeData.h"

#include <algorithm>
#include <cstring>

namespace RoboRenderer
{
    namespace
    {
        using namespace UnicodeData;

        // --- range lookups --------------------------------------------------

        std::uint8_t Lookup8(const Range8* ranges, std::uint32_t count, std::uint32_t codepoint,
                             std::uint8_t fallback)
        {
            std::uint32_t low = 0;
            std::uint32_t high = count;

            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (ranges[mid].start <= codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }

            if (low == 0)
            {
                return fallback;
            }

            const Range8& range = ranges[low - 1];
            return codepoint <= range.end ? range.value : fallback;
        }

        std::uint16_t Lookup16(const Range16* ranges, std::uint32_t count, std::uint32_t codepoint,
                               std::uint16_t fallback)
        {
            std::uint32_t low = 0;
            std::uint32_t high = count;

            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (ranges[mid].start <= codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }

            if (low == 0)
            {
                return fallback;
            }

            const Range16& range = ranges[low - 1];
            return codepoint <= range.end ? range.value : fallback;
        }

        const MirrorPair* FindMirror(std::uint32_t codepoint)
        {
            std::uint32_t low = 0;
            std::uint32_t high = MirrorCount;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (Mirrors[mid].codepoint < codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            if (low < MirrorCount && Mirrors[low].codepoint == codepoint)
            {
                return &Mirrors[low];
            }
            return nullptr;
        }

        const Bracket* FindBracket(std::uint32_t codepoint)
        {
            std::uint32_t low = 0;
            std::uint32_t high = BracketCount;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (Brackets[mid].codepoint < codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            if (low < BracketCount && Brackets[low].codepoint == codepoint)
            {
                return &Brackets[low];
            }
            return nullptr;
        }

        const Decomposition* FindDecomposition(std::uint32_t codepoint)
        {
            std::uint32_t low = 0;
            std::uint32_t high = DecompositionCount;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (Decompositions[mid].codepoint < codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            if (low < DecompositionCount && Decompositions[low].codepoint == codepoint)
            {
                return &Decompositions[low];
            }
            return nullptr;
        }

        const CaseMap* FindSimpleCase(std::uint32_t codepoint)
        {
            std::uint32_t low = 0;
            std::uint32_t high = SimpleCaseCount;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (SimpleCaseMaps[mid].codepoint < codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            if (low < SimpleCaseCount && SimpleCaseMaps[low].codepoint == codepoint)
            {
                return &SimpleCaseMaps[low];
            }
            return nullptr;
        }

        const FullCaseMap* FindFullCase(std::uint32_t codepoint)
        {
            std::uint32_t low = 0;
            std::uint32_t high = FullCaseCount;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (FullCaseMaps[mid].codepoint < codepoint)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            if (low < FullCaseCount && FullCaseMaps[low].codepoint == codepoint)
            {
                return &FullCaseMaps[low];
            }
            return nullptr;
        }

        std::uint32_t FindComposition(std::uint32_t first, std::uint32_t second)
        {
            std::uint32_t low = 0;
            std::uint32_t high = CompositionCount;

            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                const Composition& entry = Compositions[mid];

                if (entry.first < first || (entry.first == first && entry.second < second))
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }

            if (low < CompositionCount && Compositions[low].first == first &&
                Compositions[low].second == second)
            {
                return Compositions[low].composed;
            }

            return 0;
        }

        std::uint8_t IndicConjunctOf(std::uint32_t codepoint)
        {
            return Lookup8(IndicConjunctRanges, IndicConjunctRangesCount, codepoint, 0);
        }

        // --- Hangul (algorithmic composition, UAX #15 section 16) ----------

        constexpr std::uint32_t HangulSBase = 0xAC00;
        constexpr std::uint32_t HangulLBase = 0x1100;
        constexpr std::uint32_t HangulVBase = 0x1161;
        constexpr std::uint32_t HangulTBase = 0x11A7;
        constexpr std::uint32_t HangulLCount = 19;
        constexpr std::uint32_t HangulVCount = 21;
        constexpr std::uint32_t HangulTCount = 28;
        constexpr std::uint32_t HangulNCount = HangulVCount * HangulTCount;
        constexpr std::uint32_t HangulSCount = HangulLCount * HangulNCount;

        bool IsHangulSyllable(std::uint32_t codepoint)
        {
            return codepoint >= HangulSBase && codepoint < HangulSBase + HangulSCount;
        }

        bool IsHangulL(std::uint32_t codepoint)
        {
            return codepoint >= HangulLBase && codepoint < HangulLBase + HangulLCount;
        }

        bool IsHangulV(std::uint32_t codepoint)
        {
            return codepoint >= HangulVBase && codepoint < HangulVBase + HangulVCount;
        }

        bool IsHangulT(std::uint32_t codepoint)
        {
            return codepoint > HangulTBase && codepoint < HangulTBase + HangulTCount;
        }

        std::uint32_t ComposeHangul(std::uint32_t first, std::uint32_t second)
        {
            if (IsHangulL(first) && IsHangulV(second))
            {
                return HangulSBase + ((first - HangulLBase) * HangulVCount + (second - HangulVBase)) * HangulTCount;
            }

            if (IsHangulSyllable(first) && (first - HangulSBase) % HangulTCount == 0 && IsHangulT(second))
            {
                return first + (second - HangulTBase);
            }

            return 0;
        }

        // --- normalization helpers -----------------------------------------

        void DecomposeRecursive(std::uint32_t codepoint, bool compatibility, std::vector<std::uint32_t>& out)
        {
            if (IsHangulSyllable(codepoint))
            {
                const std::uint32_t index = codepoint - HangulSBase;
                out.push_back(HangulLBase + index / HangulNCount);
                out.push_back(HangulVBase + (index % HangulNCount) / HangulTCount);

                if (index % HangulTCount != 0)
                {
                    out.push_back(HangulTBase + index % HangulTCount);
                }
                return;
            }

            const Decomposition* entry = FindDecomposition(codepoint);
            if (entry == nullptr || (!compatibility && entry->compat != 0))
            {
                out.push_back(codepoint);
                return;
            }

            for (std::uint8_t index = 0; index < entry->length; ++index)
            {
                DecomposeRecursive(DecompositionData[entry->offset + index], compatibility, out);
            }
        }

        void CanonicalOrder(std::vector<std::uint32_t>& sequence)
        {
            // A stable insertion sort keyed on combining class: the sequences
            // are short and usually already ordered, which is why this is the
            // cheap correct answer rather than a merge sort.
            for (std::size_t index = 1; index < sequence.size(); ++index)
            {
                const std::uint32_t codepoint = sequence[index];
                const std::uint8_t order = CombiningClassOf(codepoint);
                if (order == 0)
                {
                    continue;
                }

                std::size_t position = index;
                while (position > 0)
                {
                    const std::uint8_t previousOrder = CombiningClassOf(sequence[position - 1]);
                    if (previousOrder == 0 || previousOrder <= order)
                    {
                        break;
                    }
                    sequence[position] = sequence[position - 1];
                    --position;
                }
                sequence[position] = codepoint;
            }
        }

        std::vector<std::uint32_t> DecomposeAll(const std::vector<std::uint32_t>& input, bool compatibility)
        {
            std::vector<std::uint32_t> result;
            result.reserve(input.size());

            for (const std::uint32_t codepoint : input)
            {
                DecomposeRecursive(codepoint, compatibility, result);
            }

            CanonicalOrder(result);
            return result;
        }

        std::vector<std::uint32_t> ComposeAll(const std::vector<std::uint32_t>& input)
        {
            if (input.empty())
            {
                return input;
            }

            std::vector<std::uint32_t> result;
            result.reserve(input.size());

            std::size_t starterIndex = 0;
            bool haveStarter = false;
            std::uint8_t lastClass = 0;

            for (std::size_t index = 0; index < input.size(); ++index)
            {
                const std::uint32_t codepoint = input[index];
                const std::uint8_t order = CombiningClassOf(codepoint);

                if (haveStarter && (lastClass < order || lastClass == 0))
                {
                    const std::uint32_t composed = FindComposition(result[starterIndex], codepoint);
                    const std::uint32_t hangul = composed == 0 ? ComposeHangul(result[starterIndex], codepoint) : 0;

                    if (composed != 0 || hangul != 0)
                    {
                        result[starterIndex] = composed != 0 ? composed : hangul;
                        continue;
                    }
                }

                if (order == 0)
                {
                    starterIndex = result.size();
                    haveStarter = true;
                }

                lastClass = order;
                result.push_back(codepoint);
            }

            return result;
        }

        // --- segmentation helpers ------------------------------------------

        struct Codepoints
        {
            std::vector<std::uint32_t> values;
            std::vector<std::uint32_t> byteOffsets; // byte offset of each codepoint
        };

        Codepoints SplitUtf8(const char* text)
        {
            Codepoints result;
            if (text == nullptr)
            {
                return result;
            }

            std::uint32_t offset = 0;
            while (text[offset] != '\0')
            {
                result.byteOffsets.push_back(offset);
                result.values.push_back(DecodeUtf8(text, offset));
            }

            return result;
        }

        bool IsControlOrBreak(std::uint32_t codepoint)
        {
            const GeneralCategory category = GeneralCategoryOf(codepoint);
            return category == GeneralCategory::Cc || category == GeneralCategory::Cf ||
                   category == GeneralCategory::Zl || category == GeneralCategory::Zp;
        }
    }

    // -----------------------------------------------------------------------
    // UTF-8
    // -----------------------------------------------------------------------

    std::uint32_t DecodeUtf8(const char* text, std::uint32_t& offset)
    {
        const auto byte = [text](std::uint32_t index) {
            return static_cast<unsigned char>(text[index]);
        };

        const unsigned char first = byte(offset);
        if (first < 0x80)
        {
            ++offset;
            return first;
        }

        std::uint32_t codepoint = 0;
        int length = 0;

        if ((first & 0xE0) == 0xC0)
        {
            codepoint = first & 0x1Fu;
            length = 1;
        }
        else if ((first & 0xF0) == 0xE0)
        {
            codepoint = first & 0x0Fu;
            length = 2;
        }
        else if ((first & 0xF8) == 0xF0)
        {
            codepoint = first & 0x07u;
            length = 3;
        }
        else
        {
            ++offset;
            return 0xFFFD;
        }

        for (int index = 0; index < length; ++index)
        {
            const unsigned char next = byte(offset + 1);
            if ((next & 0xC0) != 0x80)
            {
                break;
            }

            codepoint = (codepoint << 6) | (next & 0x3Fu);
            ++offset;
        }

        ++offset;
        return codepoint;
    }

    std::vector<std::uint32_t> Utf8ToCodepoints(const char* text)
    {
        return SplitUtf8(text).values;
    }

    void AppendUtf8(std::string& out, std::uint32_t codepoint)
    {
        if (codepoint < 0x80)
        {
            out.push_back(static_cast<char>(codepoint));
        }
        else if (codepoint < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::string Utf8FromCodepoint(std::uint32_t codepoint)
    {
        std::string result;
        AppendUtf8(result, codepoint);
        return result;
    }

    std::uint32_t CodepointCount(const char* text)
    {
        if (text == nullptr)
        {
            return 0;
        }

        std::uint32_t offset = 0;
        std::uint32_t count = 0;
        while (text[offset] != '\0')
        {
            DecodeUtf8(text, offset);
            ++count;
        }
        return count;
    }

    std::uint32_t ByteOffsetForCodepointIndex(const char* text, std::uint32_t index)
    {
        if (text == nullptr)
        {
            return 0;
        }

        std::uint32_t offset = 0;
        std::uint32_t current = 0;
        while (text[offset] != '\0' && current < index)
        {
            DecodeUtf8(text, offset);
            ++current;
        }
        return offset;
    }

    std::uint32_t CodepointIndexForByteOffset(const char* text, std::uint32_t byteOffset)
    {
        if (text == nullptr)
        {
            return 0;
        }

        std::uint32_t offset = 0;
        std::uint32_t index = 0;
        while (text[offset] != '\0' && offset < byteOffset)
        {
            DecodeUtf8(text, offset);
            ++index;
        }
        return index;
    }

    // -----------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------

    GeneralCategory GeneralCategoryOf(std::uint32_t codepoint)
    {
        // Unassigned code points are not listed in UnicodeData.txt, so Cn is the
        // fallback rather than a table entry.
        const std::uint8_t value =
            Lookup8(GeneralCategoryRanges, GeneralCategoryRangesCount, codepoint, 2);
        return static_cast<GeneralCategory>(value);
    }

    BidiClass BidiClassOf(std::uint32_t codepoint)
    {
        return static_cast<BidiClass>(Lookup8(BidiClassRanges, BidiClassRangesCount, codepoint, 0));
    }

    LineBreakClass LineBreakClassOf(std::uint32_t codepoint)
    {
        return static_cast<LineBreakClass>(Lookup8(LineBreakRanges, LineBreakRangesCount, codepoint, 0));
    }

    GraphemeBreakClass GraphemeBreakOf(std::uint32_t codepoint)
    {
        return static_cast<GraphemeBreakClass>(
            Lookup8(GraphemeBreakRanges, GraphemeBreakRangesCount, codepoint, 0));
    }

    WordBreakClass WordBreakOf(std::uint32_t codepoint)
    {
        return static_cast<WordBreakClass>(Lookup8(WordBreakRanges, WordBreakRangesCount, codepoint, 0));
    }

    SentenceBreakClass SentenceBreakOf(std::uint32_t codepoint)
    {
        return static_cast<SentenceBreakClass>(
            Lookup8(SentenceBreakRanges, SentenceBreakRangesCount, codepoint, 0));
    }

    EastAsianWidth EastAsianWidthOf(std::uint32_t codepoint)
    {
        return static_cast<EastAsianWidth>(
            Lookup8(EastAsianWidthRanges, EastAsianWidthRangesCount, codepoint, 0));
    }

    std::uint8_t CombiningClassOf(std::uint32_t codepoint)
    {
        return Lookup8(CombiningClassRanges, CombiningClassRangesCount, codepoint, 0);
    }

    std::uint16_t ScriptOf(std::uint32_t codepoint)
    {
        return Lookup16(ScriptRanges, ScriptRangesCount, codepoint, 0xFFFFu);
    }

    const char* ScriptName(std::uint16_t script)
    {
        if (script == 0xFFFFu || script >= ScriptNameCount)
        {
            return "Unknown";
        }
        return ScriptNames[script];
    }

    std::uint16_t ScriptIndexByName(const char* name)
    {
        if (name == nullptr)
        {
            return 0xFFFFu;
        }

        for (std::uint32_t index = 0; index < ScriptNameCount; ++index)
        {
            if (std::strcmp(ScriptNames[index], name) == 0)
            {
                return static_cast<std::uint16_t>(index);
            }
        }

        return 0xFFFFu;
    }

    const char* GeneralCategoryName(GeneralCategory category)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(category);
        if (value >= GeneralCategoryNameCount)
        {
            return "??";
        }
        return GeneralCategoryNames[value];
    }

    const char* BidiClassName(BidiClass bidiClass)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(bidiClass);
        if (value >= BidiClassNameCount)
        {
            return "??";
        }
        return BidiClassNames[value];
    }

    bool IsEmoji(std::uint32_t codepoint)
    {
        return (Lookup8(EmojiRanges, EmojiRangesCount, codepoint, 0) & 1u) != 0;
    }

    bool IsEmojiPresentation(std::uint32_t codepoint)
    {
        return (Lookup8(EmojiRanges, EmojiRangesCount, codepoint, 0) & 2u) != 0;
    }

    bool IsEmojiModifier(std::uint32_t codepoint)
    {
        return (Lookup8(EmojiRanges, EmojiRangesCount, codepoint, 0) & 4u) != 0;
    }

    bool IsEmojiModifierBase(std::uint32_t codepoint)
    {
        return (Lookup8(EmojiRanges, EmojiRangesCount, codepoint, 0) & 8u) != 0;
    }

    bool IsEmojiComponent(std::uint32_t codepoint)
    {
        return (Lookup8(EmojiRanges, EmojiRangesCount, codepoint, 0) & 16u) != 0;
    }

    bool IsExtendedPictographic(std::uint32_t codepoint)
    {
        return (Lookup8(EmojiRanges, EmojiRangesCount, codepoint, 0) & 32u) != 0;
    }

    bool IsWhiteSpace(std::uint32_t codepoint)
    {
        switch (codepoint)
        {
            case 0x0009:
            case 0x000A:
            case 0x000B:
            case 0x000C:
            case 0x000D:
            case 0x0020:
            case 0x0085:
            case 0x00A0:
            case 0x1680:
            case 0x2028:
            case 0x2029:
            case 0x202F:
            case 0x205F:
            case 0x3000:
                return true;
            default:
                break;
        }

        return codepoint >= 0x2000 && codepoint <= 0x200A;
    }

    bool IsMark(std::uint32_t codepoint)
    {
        const GeneralCategory category = GeneralCategoryOf(codepoint);
        return category == GeneralCategory::Mn || category == GeneralCategory::Mc ||
               category == GeneralCategory::Me;
    }

    bool IsLetter(std::uint32_t codepoint)
    {
        const GeneralCategory category = GeneralCategoryOf(codepoint);
        return category == GeneralCategory::Lu || category == GeneralCategory::Ll ||
               category == GeneralCategory::Lt || category == GeneralCategory::Lm ||
               category == GeneralCategory::Lo;
    }

    bool IsDigit(std::uint32_t codepoint)
    {
        return GeneralCategoryOf(codepoint) == GeneralCategory::Nd;
    }

    bool IsFormatOrVariationSelector(std::uint32_t codepoint)
    {
        if (GeneralCategoryOf(codepoint) == GeneralCategory::Cf)
        {
            return true;
        }

        return (codepoint >= 0xFE00 && codepoint <= 0xFE0F) || (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
    }

    std::uint32_t MirrorOf(std::uint32_t codepoint)
    {
        const MirrorPair* pair = FindMirror(codepoint);
        return pair == nullptr ? codepoint : pair->mirror;
    }

    bool IsOpenBracket(std::uint32_t codepoint, std::uint32_t& pair)
    {
        const Bracket* bracket = FindBracket(codepoint);
        if (bracket == nullptr || bracket->open == 0)
        {
            return false;
        }
        pair = bracket->pair;
        return true;
    }

    bool IsCloseBracket(std::uint32_t codepoint, std::uint32_t& pair)
    {
        const Bracket* bracket = FindBracket(codepoint);
        if (bracket == nullptr || bracket->open != 0)
        {
            return false;
        }
        pair = bracket->pair;
        return true;
    }

    // -----------------------------------------------------------------------
    // Normalization
    // -----------------------------------------------------------------------

    std::vector<std::uint32_t> DecomposeCodePoint(std::uint32_t codepoint, bool compatibility)
    {
        std::vector<std::uint32_t> result;
        DecomposeRecursive(codepoint, compatibility, result);
        return result;
    }

    std::vector<std::uint32_t> NormalizeCodePoints(const std::vector<std::uint32_t>& input, NormalForm form)
    {
        switch (form)
        {
            case NormalForm::None:
                return input;
            case NormalForm::NFD:
                return DecomposeAll(input, false);
            case NormalForm::NFKD:
                return DecomposeAll(input, true);
            case NormalForm::NFC:
                return ComposeAll(DecomposeAll(input, false));
            case NormalForm::NFKC:
                return ComposeAll(DecomposeAll(input, true));
        }

        return input;
    }

    std::string NormalizeUtf8(const char* text, NormalForm form)
    {
        const std::vector<std::uint32_t> normalized = NormalizeCodePoints(Utf8ToCodepoints(text), form);

        std::string result;
        result.reserve(normalized.size() * 2);
        for (const std::uint32_t codepoint : normalized)
        {
            AppendUtf8(result, codepoint);
        }
        return result;
    }

    bool IsNormalized(const std::vector<std::uint32_t>& input, NormalForm form)
    {
        if (form == NormalForm::None)
        {
            return true;
        }
        return NormalizeCodePoints(input, form) == input;
    }

    // -----------------------------------------------------------------------
    // Case
    // -----------------------------------------------------------------------

    std::uint32_t ToLowerCodePoint(std::uint32_t codepoint)
    {
        const FullCaseMap* full = FindFullCase(codepoint);
        if (full != nullptr && full->lowerLength == 1)
        {
            return FullCaseData[full->offset];
        }

        const CaseMap* simple = FindSimpleCase(codepoint);
        if (simple != nullptr && simple->lower != 0)
        {
            return simple->lower;
        }
        return codepoint;
    }

    std::uint32_t ToUpperCodePoint(std::uint32_t codepoint)
    {
        const FullCaseMap* full = FindFullCase(codepoint);
        if (full != nullptr && full->upperLength == 1)
        {
            return FullCaseData[full->offset + full->lowerLength + full->titleLength];
        }

        const CaseMap* simple = FindSimpleCase(codepoint);
        if (simple != nullptr && simple->upper != 0)
        {
            return simple->upper;
        }
        return codepoint;
    }

    std::uint32_t ToTitleCodePoint(std::uint32_t codepoint)
    {
        const FullCaseMap* full = FindFullCase(codepoint);
        if (full != nullptr && full->titleLength == 1)
        {
            return FullCaseData[full->offset + full->lowerLength];
        }

        const CaseMap* simple = FindSimpleCase(codepoint);
        if (simple != nullptr && simple->title != 0)
        {
            return simple->title;
        }
        return codepoint;
    }

    namespace
    {
        std::string MapCase(const char* text, int which)
        {
            const std::vector<std::uint32_t> codepoints = Utf8ToCodepoints(text);
            std::string result;
            result.reserve(codepoints.size() * 2);

            for (const std::uint32_t codepoint : codepoints)
            {
                const FullCaseMap* full = FindFullCase(codepoint);
                if (full != nullptr)
                {
                    std::uint8_t length = 0;
                    std::uint32_t offset = full->offset;

                    if (which == 0)
                    {
                        length = full->lowerLength;
                    }
                    else if (which == 1)
                    {
                        length = full->titleLength;
                        offset += full->lowerLength;
                    }
                    else
                    {
                        length = full->upperLength;
                        offset += full->lowerLength + full->titleLength;
                    }

                    if (length > 1)
                    {
                        for (std::uint8_t index = 0; index < length; ++index)
                        {
                            AppendUtf8(result, FullCaseData[offset + index]);
                        }
                        continue;
                    }
                }

                AppendUtf8(result, which == 0 ? ToLowerCodePoint(codepoint)
                                             : (which == 1 ? ToTitleCodePoint(codepoint)
                                                           : ToUpperCodePoint(codepoint)));
            }

            return result;
        }
    }

    std::string ToLowerUtf8(const char* text) { return MapCase(text, 0); }
    std::string ToTitleUtf8(const char* text) { return MapCase(text, 1); }
    std::string ToUpperUtf8(const char* text) { return MapCase(text, 2); }

    std::string CapitalizeUtf8(const char* text)
    {
        const std::vector<std::uint32_t> codepoints = Utf8ToCodepoints(text);

        std::string result;
        result.reserve(codepoints.size() * 2);

        bool atWordStart = true;

        for (std::size_t index = 0; index < codepoints.size(); ++index)
        {
            const std::uint32_t codepoint = codepoints[index];

            // The start of a word is the first letter after a non-letter.
            const bool isLetterNow = IsLetter(codepoint);

            if (isLetterNow && atWordStart)
            {
                AppendUtf8(result, ToTitleCodePoint(codepoint));
                atWordStart = false;
                continue;
            }

            AppendUtf8(result, ToLowerCodePoint(codepoint));

            if (!isLetterNow && GeneralCategoryOf(codepoint) != GeneralCategory::Mn &&
                GeneralCategoryOf(codepoint) != GeneralCategory::Mc)
            {
                atWordStart = true;
            }
        }

        return result;
    }

    // -----------------------------------------------------------------------
    // Segmentation: grapheme clusters (UAX #29)
    // -----------------------------------------------------------------------

    std::vector<std::uint32_t> GraphemeClusterBoundaries(const char* text)
    {
        const Codepoints input = SplitUtf8(text);
        std::vector<std::uint32_t> boundaries;

        const std::size_t count = input.values.size();
        if (count == 0)
        {
            return boundaries;
        }

        std::vector<GraphemeBreakClass> classes(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            classes[index] = GraphemeBreakOf(input.values[index]);
        }

        const auto isControl = [&](std::size_t index) {
            const GraphemeBreakClass value = classes[index];
            return value == GraphemeBreakClass::Control || value == GraphemeBreakClass::CR ||
                   value == GraphemeBreakClass::LF;
        };

        boundaries.push_back(input.byteOffsets[0]);

        for (std::size_t index = 1; index < count; ++index)
        {
            const GraphemeBreakClass previous = classes[index - 1];
            const GraphemeBreakClass current = classes[index];

            bool join = false;

            // GB3
            if (previous == GraphemeBreakClass::CR && current == GraphemeBreakClass::LF)
            {
                join = true;
            }
            // GB4 / GB5
            else if (isControl(index - 1) || isControl(index))
            {
                join = false;
            }
            // GB6
            else if (previous == GraphemeBreakClass::L &&
                     (current == GraphemeBreakClass::L || current == GraphemeBreakClass::V ||
                      current == GraphemeBreakClass::LV || current == GraphemeBreakClass::LVT))
            {
                join = true;
            }
            // GB7
            else if ((previous == GraphemeBreakClass::LV || previous == GraphemeBreakClass::V) &&
                     (current == GraphemeBreakClass::V || current == GraphemeBreakClass::T))
            {
                join = true;
            }
            // GB8
            else if ((previous == GraphemeBreakClass::LVT || previous == GraphemeBreakClass::T) &&
                     current == GraphemeBreakClass::T)
            {
                join = true;
            }
            // GB9
            else if (current == GraphemeBreakClass::Extend || current == GraphemeBreakClass::ZWJ)
            {
                join = true;
            }
            // GB9a
            else if (current == GraphemeBreakClass::SpacingMark)
            {
                join = true;
            }
            // GB9b
            else if (previous == GraphemeBreakClass::Prepend)
            {
                join = true;
            }
            // GB9c: an Indic consonant joined across linkers and extenders.
            else if (IndicConjunctOf(input.values[index]) == 1)
            {
                std::size_t back = index;
                bool sawLinker = false;

                while (back > 0)
                {
                    const std::uint8_t conjunct = IndicConjunctOf(input.values[back - 1]);
                    if (conjunct == 2)
                    {
                        --back;
                        continue;
                    }
                    if (conjunct == 3)
                    {
                        sawLinker = true;
                        --back;
                        continue;
                    }
                    if (conjunct == 1 && sawLinker)
                    {
                        join = true;
                    }
                    break;
                }
            }
            // GB11: pictograph, extenders, joiner, pictograph.
            else if (IsExtendedPictographic(input.values[index]) && previous == GraphemeBreakClass::ZWJ)
            {
                std::size_t back = index - 1;
                bool sawPictograph = false;

                while (back > 0)
                {
                    const GraphemeBreakClass value = classes[back - 1];
                    if (value == GraphemeBreakClass::Extend)
                    {
                        --back;
                        continue;
                    }

                    sawPictograph = IsExtendedPictographic(input.values[back - 1]);
                    break;
                }

                join = sawPictograph;
            }
            // GB12 / GB13: regional indicators pair up.
            else if (previous == GraphemeBreakClass::RegionalIndicator &&
                     current == GraphemeBreakClass::RegionalIndicator)
            {
                std::size_t runLength = 0;
                for (std::size_t back = index; back > 0; --back)
                {
                    if (classes[back - 1] != GraphemeBreakClass::RegionalIndicator)
                    {
                        break;
                    }
                    ++runLength;
                }
                join = (runLength % 2) == 1;
            }

            if (!join)
            {
                boundaries.push_back(input.byteOffsets[index]);
            }
        }

        return boundaries;
    }

    // -----------------------------------------------------------------------
    // Segmentation: words (UAX #29)
    // -----------------------------------------------------------------------

    std::vector<std::uint32_t> WordBoundaries(const char* text)
    {
        const Codepoints input = SplitUtf8(text);
        std::vector<std::uint32_t> boundaries;

        const std::size_t count = input.values.size();
        if (count == 0)
        {
            return boundaries;
        }

        std::vector<WordBreakClass> classes(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            classes[index] = WordBreakOf(input.values[index]);
        }

        const auto isAlphanumeric = [&](std::size_t index) {
            const WordBreakClass value = classes[index];
            return value == WordBreakClass::ALetter || value == WordBreakClass::HebrewLetter ||
                   value == WordBreakClass::Numeric || value == WordBreakClass::Katakana ||
                   value == WordBreakClass::ExtendNumLet;
        };

        const auto isMidLetter = [&](std::size_t index) {
            return classes[index] == WordBreakClass::MidLetter ||
                   classes[index] == WordBreakClass::MidNumLet ||
                   classes[index] == WordBreakClass::SingleQuote;
        };

        const auto isMidNumber = [&](std::size_t index) {
            return classes[index] == WordBreakClass::MidNum ||
                   classes[index] == WordBreakClass::MidNumLet ||
                   classes[index] == WordBreakClass::SingleQuote;
        };

        // A boundary at the start is only reported when the first character is
        // not alphanumeric, which is what a caller selecting a word wants.
        if (!isAlphanumeric(0))
        {
            boundaries.push_back(input.byteOffsets[0]);
        }

        for (std::size_t index = 1; index < count; ++index)
        {
            const WordBreakClass previous = classes[index - 1];
            const WordBreakClass current = classes[index];

            bool join = false;

            if (previous == WordBreakClass::CR && current == WordBreakClass::LF)
            {
                join = true;
            }
            else if (previous == WordBreakClass::Newline || previous == WordBreakClass::CR ||
                     previous == WordBreakClass::LF || current == WordBreakClass::Newline ||
                     current == WordBreakClass::CR || current == WordBreakClass::LF)
            {
                join = false;
            }
            else if (current == WordBreakClass::Extend || current == WordBreakClass::Format ||
                     current == WordBreakClass::ZWJ)
            {
                join = true;
            }
            else if (previous == WordBreakClass::ZWJ && IsExtendedPictographic(input.values[index]))
            {
                join = true;
            }
            else if (previous == WordBreakClass::RegionalIndicator &&
                     current == WordBreakClass::RegionalIndicator)
            {
                std::size_t runLength = 0;
                for (std::size_t back = index; back > 0; --back)
                {
                    if (classes[back - 1] != WordBreakClass::RegionalIndicator)
                    {
                        break;
                    }
                    ++runLength;
                }
                join = (runLength % 2) == 1;
            }
            else if (previous == WordBreakClass::HebrewLetter && current == WordBreakClass::SingleQuote)
            {
                join = true;
            }
            else if (isAlphanumeric(index - 1) && isAlphanumeric(index))
            {
                join = true;
            }
            else if (isAlphanumeric(index - 1) && isMidLetter(index) && index + 1 < count &&
                     isAlphanumeric(index + 1))
            {
                join = true;
            }
            else if (isMidLetter(index - 1) && isAlphanumeric(index) && index >= 2 &&
                     isAlphanumeric(index - 2))
            {
                join = true;
            }
            else if (previous == WordBreakClass::Numeric && isMidNumber(index) && index + 1 < count &&
                     classes[index + 1] == WordBreakClass::Numeric)
            {
                join = true;
            }
            else if (isMidNumber(index - 1) && current == WordBreakClass::Numeric && index >= 2 &&
                     classes[index - 2] == WordBreakClass::Numeric)
            {
                join = true;
            }
            else if (previous == WordBreakClass::Katakana && current == WordBreakClass::Katakana)
            {
                join = true;
            }
            else if (isAlphanumeric(index - 1) && current == WordBreakClass::ExtendNumLet)
            {
                join = true;
            }
            else if (previous == WordBreakClass::ExtendNumLet && isAlphanumeric(index))
            {
                join = true;
            }
            else if (previous == WordBreakClass::WSegSpace && current == WordBreakClass::WSegSpace)
            {
                join = true;
            }

            if (!join)
            {
                boundaries.push_back(input.byteOffsets[index]);
            }
        }

        return boundaries;
    }

    // -----------------------------------------------------------------------
    // Segmentation: sentences (UAX #29)
    // -----------------------------------------------------------------------

    std::vector<std::uint32_t> SentenceBoundaries(const char* text)
    {
        const Codepoints input = SplitUtf8(text);
        std::vector<std::uint32_t> boundaries;

        const std::size_t count = input.values.size();
        if (count == 0)
        {
            return boundaries;
        }

        boundaries.push_back(input.byteOffsets[0]);

        std::vector<SentenceBreakClass> classes(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            classes[index] = SentenceBreakOf(input.values[index]);
        }

        const auto isAny = [&](std::size_t index, SentenceBreakClass value) {
            return classes[index] == value;
        };

        for (std::size_t index = 1; index < count; ++index)
        {
            const SentenceBreakClass previous = classes[index - 1];
            const SentenceBreakClass current = classes[index];

            bool join = true;

            if (previous == SentenceBreakClass::CR && current == SentenceBreakClass::LF)
            {
                join = true;
            }
            else if (previous == SentenceBreakClass::Sep || previous == SentenceBreakClass::CR ||
                     previous == SentenceBreakClass::LF)
            {
                join = false;
            }
            else if (current == SentenceBreakClass::Extend || current == SentenceBreakClass::Format)
            {
                join = true;
            }
            else if (current == SentenceBreakClass::ATerm && index + 1 < count)
            {
                // SB6: ATerm × Numeric
                join = classes[index + 1] == SentenceBreakClass::Numeric;
            }
            else if (previous == SentenceBreakClass::ATerm && current == SentenceBreakClass::Numeric)
            {
                join = true;
            }
            else if (previous == SentenceBreakClass::ATerm || previous == SentenceBreakClass::STerm)
            {
                // SB8 / SB11: a terminator followed by closing punctuation or
                // spaces keeps the sentence together, otherwise it ends it.
                if (current == SentenceBreakClass::Close || current == SentenceBreakClass::Space ||
                    current == SentenceBreakClass::Sep || current == SentenceBreakClass::CR ||
                    current == SentenceBreakClass::LF || current == SentenceBreakClass::Extend ||
                    current == SentenceBreakClass::Format)
                {
                    join = true;
                }
                else
                {
                    // SB8a: STerm × (SContinue | STerm | ATerm)
                    join = current == SentenceBreakClass::SContinue ||
                           current == SentenceBreakClass::STerm ||
                           current == SentenceBreakClass::ATerm;
                }
            }
            else if (isAny(index - 1, SentenceBreakClass::Lower) &&
                     isAny(index, SentenceBreakClass::Upper))
            {
                join = true; // common abbreviation pattern
            }
            else
            {
                join = true;
            }

            if (!join)
            {
                boundaries.push_back(input.byteOffsets[index]);
            }
        }

        return boundaries;
    }

    // -----------------------------------------------------------------------
    // Line breaking (UAX #14)
    // -----------------------------------------------------------------------

    namespace
    {
        LineBreakClass ResolveLineBreak(LineBreakClass value, std::uint32_t codepoint)
        {
            switch (value)
            {
                case LineBreakClass::AI:
                case LineBreakClass::SG:
                case LineBreakClass::XX:
                    return LineBreakClass::AL;
                case LineBreakClass::SA:
                    return IsMark(codepoint) ? LineBreakClass::CM : LineBreakClass::AL;
                case LineBreakClass::CJ:
                    return LineBreakClass::NS;
                default:
                    return value;
            }
        }

        bool IsMandatoryBefore(LineBreakClass value)
        {
            return value == LineBreakClass::BK || value == LineBreakClass::CR ||
                   value == LineBreakClass::LF || value == LineBreakClass::NL;
        }
    }

    std::vector<std::uint32_t> LineBreakOpportunities(const char* text)
    {
        const Codepoints input = SplitUtf8(text);
        std::vector<std::uint32_t> opportunities;

        const std::size_t count = input.values.size();
        if (count == 0)
        {
            return opportunities;
        }

        std::vector<LineBreakClass> classes(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            classes[index] = ResolveLineBreak(LineBreakClassOf(input.values[index]), input.values[index]);
        }

        // LB10: a remaining combining mark behaves as AL.
        for (std::size_t index = 0; index < count; ++index)
        {
            if (classes[index] == LineBreakClass::CM || classes[index] == LineBreakClass::ZWJ)
            {
                classes[index] = LineBreakClass::AL;
            }
        }

        const auto classBefore = [&](std::size_t index) -> LineBreakClass {
            // Skips spaces to find the class a rule should look at.
            std::size_t position = index;
            while (position > 0 && classes[position - 1] == LineBreakClass::SP)
            {
                --position;
            }
            return position == 0 ? LineBreakClass::XX : classes[position - 1];
        };

        opportunities.push_back(input.byteOffsets[0]);

        for (std::size_t index = 1; index < count; ++index)
        {
            const LineBreakClass before = classes[index - 1];
            const LineBreakClass after = classes[index];

            bool allow = true;

            // LB4 / LB5: hard breaks.
            if (before == LineBreakClass::BK)
            {
                allow = true;
            }
            else if (before == LineBreakClass::CR && after == LineBreakClass::LF)
            {
                allow = false;
            }
            else if (before == LineBreakClass::CR || before == LineBreakClass::LF ||
                     before == LineBreakClass::NL)
            {
                allow = true;
            }
            // LB6: never break before a hard break.
            else if (IsMandatoryBefore(after))
            {
                allow = false;
            }
            // LB7: never break before a space or a zero-width space.
            else if (after == LineBreakClass::SP || after == LineBreakClass::ZW)
            {
                allow = false;
            }
            // LB8: break after a zero-width space.
            else if (before == LineBreakClass::ZW)
            {
                allow = true;
            }
            // LB8a: no break after a zero-width joiner.
            else if (before == LineBreakClass::ZWJ)
            {
                allow = false;
            }
            // LB9/LB10 already folded marks into AL.
            // LB11: word joiner.
            else if (after == LineBreakClass::WJ || before == LineBreakClass::WJ)
            {
                allow = false;
            }
            // LB12: glues to the following.
            else if (before == LineBreakClass::GL)
            {
                allow = false;
            }
            // LB13: no break before closing punctuation or separators.
            else if (after == LineBreakClass::CL || after == LineBreakClass::CP ||
                     after == LineBreakClass::EX || after == LineBreakClass::IS ||
                     after == LineBreakClass::SY)
            {
                allow = false;
            }
            // LB14: no break after an opening bracket.
            else if (classBefore(index) == LineBreakClass::OP)
            {
                allow = false;
            }
            // LB16: no break between closing punctuation and a non-starter.
            else if ((classBefore(index) == LineBreakClass::CL || classBefore(index) == LineBreakClass::CP) &&
                     after == LineBreakClass::NS)
            {
                allow = false;
            }
            // LB17: two ideographic spaces.
            else if (classBefore(index) == LineBreakClass::B2 && after == LineBreakClass::B2)
            {
                allow = false;
            }
            // LB18: a space is a break opportunity.
            else if (before == LineBreakClass::SP)
            {
                allow = true;
            }
            // LB19: no break around a quotation mark.
            else if (before == LineBreakClass::QU || after == LineBreakClass::QU)
            {
                allow = false;
            }
            // LB20: a contingent break always breaks.
            else if (before == LineBreakClass::CB || after == LineBreakClass::CB)
            {
                allow = true;
            }
            // LB21: no break before a break-after class, and none after BB.
            else if (after == LineBreakClass::BA || after == LineBreakClass::HY ||
                     after == LineBreakClass::NS)
            {
                allow = false;
            }
            else if (before == LineBreakClass::BB)
            {
                allow = false;
            }
            // LB21a: a Hebrew letter with a hyphen keeps the next letter.
            else if (classes[index > 1 ? index - 2 : 0] == LineBreakClass::HL &&
                     (before == LineBreakClass::HY || before == LineBreakClass::BA))
            {
                allow = false;
            }
            // LB21b: a solidus keeps a following Hebrew letter.
            else if (before == LineBreakClass::SY && after == LineBreakClass::HL)
            {
                allow = false;
            }
            // LB22: no break before an inseparable sequence.
            else if (after == LineBreakClass::IN)
            {
                allow = false;
            }
            // LB23: letters and numbers.
            else if ((before == LineBreakClass::AL || before == LineBreakClass::HL) &&
                     after == LineBreakClass::NU)
            {
                allow = false;
            }
            else if (before == LineBreakClass::NU &&
                     (after == LineBreakClass::AL || after == LineBreakClass::HL))
            {
                allow = false;
            }
            // LB23a: prefix and ideographs.
            else if (before == LineBreakClass::PR &&
                     (after == LineBreakClass::ID || after == LineBreakClass::EB ||
                      after == LineBreakClass::EM))
            {
                allow = false;
            }
            else if ((before == LineBreakClass::ID || before == LineBreakClass::EB ||
                      before == LineBreakClass::EM) && after == LineBreakClass::PO)
            {
                allow = false;
            }
            // LB24: numeric prefixes and letters.
            else if ((before == LineBreakClass::PR || before == LineBreakClass::PO) &&
                     (after == LineBreakClass::AL || after == LineBreakClass::HL))
            {
                allow = false;
            }
            else if ((before == LineBreakClass::AL || before == LineBreakClass::HL) &&
                     (after == LineBreakClass::PR || after == LineBreakClass::PO))
            {
                allow = false;
            }
            // LB25: numeric sequences.
            else if ((before == LineBreakClass::PR || before == LineBreakClass::PO) &&
                     after == LineBreakClass::NU)
            {
                allow = false;
            }
            else if (before == LineBreakClass::NU && after == LineBreakClass::NU)
            {
                allow = false;
            }
            else if (before == LineBreakClass::NU &&
                     (after == LineBreakClass::SY || after == LineBreakClass::IS))
            {
                allow = false;
            }
            // LB26: Hangul syllables.
            else if (before == LineBreakClass::JL &&
                     (after == LineBreakClass::JL || after == LineBreakClass::JV ||
                      after == LineBreakClass::H2 || after == LineBreakClass::H3))
            {
                allow = false;
            }
            else if ((before == LineBreakClass::JV || before == LineBreakClass::H2) &&
                     (after == LineBreakClass::JV || after == LineBreakClass::JT))
            {
                allow = false;
            }
            else if ((before == LineBreakClass::JT || before == LineBreakClass::H3) &&
                     after == LineBreakClass::JT)
            {
                allow = false;
            }
            // LB27: Hangul and numeric affixes.
            else if ((before == LineBreakClass::JL || before == LineBreakClass::JV ||
                      before == LineBreakClass::JT || before == LineBreakClass::H2 ||
                      before == LineBreakClass::H3) && after == LineBreakClass::PO)
            {
                allow = false;
            }
            else if (before == LineBreakClass::PR &&
                     (after == LineBreakClass::JL || after == LineBreakClass::JV ||
                      after == LineBreakClass::JT || after == LineBreakClass::H2 ||
                      after == LineBreakClass::H3))
            {
                allow = false;
            }
            // LB28: letters stay together.
            else if ((before == LineBreakClass::AL || before == LineBreakClass::HL) &&
                     (after == LineBreakClass::AL || after == LineBreakClass::HL))
            {
                allow = false;
            }
            // LB29: a full stop keeps a following letter.
            else if (before == LineBreakClass::IS &&
                     (after == LineBreakClass::AL || after == LineBreakClass::HL))
            {
                allow = false;
            }
            // LB30: brackets around letters and numbers.
            else if ((before == LineBreakClass::AL || before == LineBreakClass::HL ||
                      before == LineBreakClass::NU) && after == LineBreakClass::OP)
            {
                allow = false;
            }
            else if (before == LineBreakClass::CP &&
                     (after == LineBreakClass::AL || after == LineBreakClass::HL ||
                      after == LineBreakClass::NU))
            {
                allow = false;
            }
            // LB30a: regional indicators pair up.
            else if (before == LineBreakClass::RI && after == LineBreakClass::RI)
            {
                std::size_t runLength = 0;
                for (std::size_t back = index; back > 0; --back)
                {
                    if (classes[back - 1] != LineBreakClass::RI)
                    {
                        break;
                    }
                    ++runLength;
                }
                allow = (runLength % 2) == 0;
            }
            // LB30b: emoji modifier attaches to its base.
            else if (after == LineBreakClass::EM)
            {
                allow = false;
            }
            // LB31: everywhere else is a break opportunity.
            else
            {
                allow = true;
            }

            if (allow)
            {
                opportunities.push_back(input.byteOffsets[index]);
            }
        }

        return opportunities;
    }

    // -----------------------------------------------------------------------
    // Bidi (UAX #9)
    // -----------------------------------------------------------------------

    namespace
    {
        bool IsIsolateInitiator(BidiClass value)
        {
            return value == BidiClass::LRI || value == BidiClass::RLI || value == BidiClass::FSI;
        }

        bool IsRemovedByX9(BidiClass value)
        {
            return value == BidiClass::RLE || value == BidiClass::LRE || value == BidiClass::RLO ||
                   value == BidiClass::LRO || value == BidiClass::PDF || value == BidiClass::BN;
        }

        bool IsNeutral(BidiClass value)
        {
            return value == BidiClass::B || value == BidiClass::S || value == BidiClass::WS ||
                   value == BidiClass::ON || value == BidiClass::FSI || value == BidiClass::LRI ||
                   value == BidiClass::RLI || value == BidiClass::PDI;
        }
    }

    BidiParagraph ResolveParagraph(const std::vector<std::uint32_t>& codepoints,
                                   ParagraphDirection direction)
    {
        BidiParagraph result;
        const std::size_t count = codepoints.size();

        result.levels.assign(count, 0);
        result.removed.assign(count, 0);
        if (count == 0)
        {
            return result;
        }

        std::vector<BidiClass> classes(count);
        std::vector<BidiClass> original(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            classes[index] = BidiClassOf(codepoints[index]);
            original[index] = classes[index];
        }

        // P2 / P3: the paragraph level comes from the first strong character,
        // skipping isolate content.
        std::uint8_t paragraphLevel = 0;
        bool paragraphResolved = false;

        if (direction == ParagraphDirection::RightToLeft)
        {
            paragraphLevel = 1;
            paragraphResolved = true;
        }
        else if (direction == ParagraphDirection::LeftToRight)
        {
            paragraphLevel = 0;
            paragraphResolved = true;
        }

        if (!paragraphResolved)
        {
            int isolateDepth = 0;
            for (std::size_t index = 0; index < count; ++index)
            {
                const BidiClass value = classes[index];

                if (IsIsolateInitiator(value))
                {
                    ++isolateDepth;
                    continue;
                }
                if (value == BidiClass::PDI)
                {
                    if (isolateDepth > 0)
                    {
                        --isolateDepth;
                    }
                    continue;
                }

                if (isolateDepth != 0)
                {
                    continue;
                }

                if (value == BidiClass::L)
                {
                    paragraphLevel = 0;
                    paragraphResolved = true;
                    break;
                }
                if (value == BidiClass::R || value == BidiClass::AL)
                {
                    paragraphLevel = 1;
                    paragraphResolved = true;
                    break;
                }
            }
        }

        result.baseLevel = paragraphLevel;

        // X1..X8: explicit embeddings, overrides and isolates.
        struct Status
        {
            std::uint8_t level = 0;
            BidiClass override = BidiClass::ON; // ON means "no override"
            bool isolate = false;
        };

        std::vector<Status> stack;
        Status current;
        current.level = paragraphLevel;
        current.override = BidiClass::ON;
        current.isolate = false;
        stack.push_back(current);

        const std::uint8_t maxDepth = 125;

        // Matching PDI for each isolate initiator (BD9), needed by X10.
        std::vector<std::int32_t> matchingPdi(count, -1);
        {
            std::vector<std::size_t> pending;
            for (std::size_t index = 0; index < count; ++index)
            {
                if (IsIsolateInitiator(original[index]))
                {
                    pending.push_back(index);
                }
                else if (original[index] == BidiClass::PDI && !pending.empty())
                {
                    matchingPdi[pending.back()] = static_cast<std::int32_t>(index);
                    pending.pop_back();
                }
            }
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            const BidiClass value = classes[index];

            if (IsRemovedByX9(value))
            {
                // X9: these are removed from further consideration. They keep
                // the current level so reordering can leave them in place.
                result.levels[index] = stack.back().level;
                result.removed[index] = 1;
                continue;
            }

            if (IsIsolateInitiator(value))
            {
                result.levels[index] = stack.back().level;
                if (stack.back().override != BidiClass::ON)
                {
                    classes[index] = stack.back().override;
                }

                std::uint8_t nextLevel = stack.back().level;

                BidiClass resolved = value;
                if (value == BidiClass::FSI)
                {
                    // X5c: an FSI takes the direction of its content.
                    const std::int32_t end = matchingPdi[index];
                    resolved = BidiClass::LRI;
                    for (std::size_t scan = index + 1; scan < count; ++scan)
                    {
                        if (end >= 0 && scan >= static_cast<std::size_t>(end))
                        {
                            break;
                        }
                        const BidiClass inner = original[scan];
                        if (inner == BidiClass::L)
                        {
                            resolved = BidiClass::LRI;
                            break;
                        }
                        if (inner == BidiClass::R || inner == BidiClass::AL)
                        {
                            resolved = BidiClass::RLI;
                            break;
                        }
                    }
                }

                if (resolved == BidiClass::RLI)
                {
                    nextLevel = static_cast<std::uint8_t>((stack.back().level + 1) | 1);
                }
                else
                {
                    nextLevel = static_cast<std::uint8_t>((stack.back().level + 2) & ~1);
                }

                if (nextLevel <= maxDepth && stack.size() < maxDepth)
                {
                    Status status;
                    status.level = nextLevel;
                    status.override = BidiClass::ON;
                    status.isolate = true;
                    stack.push_back(status);
                }
                continue;
            }

            if (value == BidiClass::PDI)
            {
                if (stack.size() > 1 && stack.back().isolate)
                {
                    stack.pop_back();
                }
                result.levels[index] = stack.back().level;
                if (stack.back().override != BidiClass::ON)
                {
                    classes[index] = stack.back().override;
                }
                continue;
            }

            if (value == BidiClass::RLE || value == BidiClass::LRE || value == BidiClass::RLO ||
                value == BidiClass::LRO)
            {
                const std::uint8_t level = stack.back().level;
                std::uint8_t nextLevel = 0;

                if (value == BidiClass::RLE || value == BidiClass::RLO)
                {
                    nextLevel = static_cast<std::uint8_t>((level + 1) | 1);
                }
                else
                {
                    nextLevel = static_cast<std::uint8_t>((level + 2) & ~1);
                }

                if (nextLevel <= maxDepth && stack.size() < maxDepth)
                {
                    Status status;
                    status.level = nextLevel;
                    status.override = (value == BidiClass::RLO)   ? BidiClass::R
                                      : (value == BidiClass::LRO) ? BidiClass::L
                                                                  : BidiClass::ON;
                    status.isolate = false;
                    stack.push_back(status);
                }

                result.levels[index] = level;
                result.removed[index] = 1;
                continue;
            }

            if (value == BidiClass::PDF)
            {
                if (stack.size() > 1 && !stack.back().isolate)
                {
                    stack.pop_back();
                }
                result.levels[index] = stack.back().level;
                result.removed[index] = 1;
                continue;
            }

            // X6: an ordinary character takes the current level, and an override
            // replaces its type.
            result.levels[index] = stack.back().level;
            if (stack.back().override != BidiClass::ON)
            {
                classes[index] = stack.back().override;
            }
        }

        // X10: isolating run sequences. A level run continues into the run that
        // starts at the matching PDI of an isolate initiator it ends with.
        std::vector<std::vector<std::size_t>> sequences;
        std::vector<std::uint8_t> visited(count, 0);

        std::vector<std::size_t> runStart(count, 0);
        for (std::size_t index = 0; index < count; ++index)
        {
            if (result.removed[index] != 0)
            {
                continue;
            }

            std::size_t start = index;
            while (start > 0 && (result.removed[start - 1] != 0 ||
                                 result.levels[start - 1] == result.levels[index]))
            {
                --start;
                if (result.removed[start] != 0)
                {
                    break;
                }
            }
            runStart[index] = start;
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            if (result.removed[index] != 0 || visited[index] != 0)
            {
                continue;
            }

            // Only start a sequence at the beginning of a level run.
            if (index > 0 && result.removed[index - 1] == 0 &&
                result.levels[index - 1] == result.levels[index])
            {
                continue;
            }

            std::vector<std::size_t> sequence;
            std::size_t cursor = index;

            while (true)
            {
                // Walk the level run.
                std::size_t step = cursor;
                while (step < count && result.removed[step] == 0 && result.levels[step] == result.levels[cursor])
                {
                    visited[step] = 1;
                    sequence.push_back(step);
                    ++step;
                }

                const std::size_t last = step - 1;
                const BidiClass lastClass = original[last];

                if (IsIsolateInitiator(lastClass) && matchingPdi[last] >= 0)
                {
                    cursor = static_cast<std::size_t>(matchingPdi[last]);
                    continue;
                }

                break;
            }

            sequences.push_back(std::move(sequence));
        }

        for (const std::vector<std::size_t>& sequence : sequences)
        {
            if (sequence.empty())
            {
                continue;
            }

            const std::size_t first = sequence.front();
            const std::size_t last = sequence.back();

            const std::uint8_t sosLevel = std::max(result.levels[first],
                                                   first > 0 ? result.levels[first - 1] : paragraphLevel);
            const BidiClass sos = (sosLevel & 1) == 1 ? BidiClass::R : BidiClass::L;

            const std::uint8_t eosLevel = std::max(result.levels[last],
                                                   last + 1 < count ? result.levels[last + 1] : paragraphLevel);
            const BidiClass eos = (eosLevel & 1) == 1 ? BidiClass::R : BidiClass::L;

            // The sequence's own classes, in sequence order.
            const std::size_t length = sequence.size();
            std::vector<BidiClass> local(length);
            for (std::size_t index = 0; index < length; ++index)
            {
                local[index] = classes[sequence[index]];
            }

            // W1: NSM takes the type of the previous character.
            for (std::size_t index = 0; index < length; ++index)
            {
                if (local[index] == BidiClass::NSM)
                {
                    local[index] = index == 0 ? sos : local[index - 1];
                    if (IsIsolateInitiator(local[index]) || local[index] == BidiClass::PDI)
                    {
                        local[index] = BidiClass::ON;
                    }
                }
            }

            // W2: EN after AL becomes AN.
            BidiClass lastStrong = sos;
            for (std::size_t index = 0; index < length; ++index)
            {
                if (local[index] == BidiClass::L || local[index] == BidiClass::R ||
                    local[index] == BidiClass::AL)
                {
                    lastStrong = local[index];
                }
                else if (local[index] == BidiClass::EN && lastStrong == BidiClass::AL)
                {
                    local[index] = BidiClass::AN;
                }
            }

            // W3: AL becomes R.
            for (std::size_t index = 0; index < length; ++index)
            {
                if (local[index] == BidiClass::AL)
                {
                    local[index] = BidiClass::R;
                }
            }

            // W4: a single separator between two numbers is numeric.
            for (std::size_t index = 1; index + 1 < length; ++index)
            {
                if ((local[index] == BidiClass::ES || local[index] == BidiClass::CS) &&
                    local[index - 1] == BidiClass::EN && local[index + 1] == BidiClass::EN)
                {
                    local[index] = BidiClass::EN;
                }
                else if (local[index] == BidiClass::CS && local[index - 1] == BidiClass::AN &&
                         local[index + 1] == BidiClass::AN)
                {
                    local[index] = BidiClass::AN;
                }
            }

            // W5: a sequence of terminators inside numbers is numeric.
            for (std::size_t index = 0; index < length; ++index)
            {
                if (local[index] != BidiClass::ET)
                {
                    continue;
                }

                std::size_t end = index;
                while (end < length && local[end] == BidiClass::ET)
                {
                    ++end;
                }

                const BidiClass beforeType = index == 0 ? sos : local[index - 1];
                const BidiClass afterType = end == length ? eos : local[end];

                if (beforeType == BidiClass::EN || afterType == BidiClass::EN)
                {
                    for (std::size_t fill = index; fill < end; ++fill)
                    {
                        local[fill] = BidiClass::EN;
                    }
                }

                index = end - 1;
            }

            // W6: remaining terminators and separators are neutral.
            for (std::size_t index = 0; index < length; ++index)
            {
                if (local[index] == BidiClass::ET || local[index] == BidiClass::ES ||
                    local[index] == BidiClass::CS)
                {
                    local[index] = BidiClass::ON;
                }
            }

            // W7: EN after a strong L is L.
            lastStrong = sos;
            for (std::size_t index = 0; index < length; ++index)
            {
                if (local[index] == BidiClass::L || local[index] == BidiClass::R)
                {
                    lastStrong = local[index];
                }
                else if (local[index] == BidiClass::EN && lastStrong == BidiClass::L)
                {
                    local[index] = BidiClass::L;
                }
            }

            // N0: paired brackets take the embedding direction, or the
            // surrounding strong direction when the pair is enclosed by one.
            {
                std::vector<std::pair<std::size_t, std::size_t>> pairs;
                std::vector<std::size_t> bracketStack;

                for (std::size_t index = 0; index < length; ++index)
                {
                    std::uint32_t pairValue = 0;
                    if (IsOpenBracket(codepoints[sequence[index]], pairValue))
                    {
                        bracketStack.push_back(index);
                    }
                    else if (IsCloseBracket(codepoints[sequence[index]], pairValue) &&
                             !bracketStack.empty())
                    {
                        pairs.emplace_back(bracketStack.back(), index);
                        bracketStack.pop_back();
                    }
                }

                std::sort(pairs.begin(), pairs.end());

                for (const auto& pair : pairs)
                {
                    const std::size_t open = pair.first;
                    const std::size_t close = pair.second;

                    BidiClass strongBefore = sos;
                    for (std::size_t index = 0; index < open; ++index)
                    {
                        if (local[index] == BidiClass::L || local[index] == BidiClass::R ||
                            local[index] == BidiClass::EN || local[index] == BidiClass::AN)
                        {
                            strongBefore = (local[index] == BidiClass::L) ? BidiClass::L : BidiClass::R;
                        }
                    }

                    BidiClass strongAfter = eos;
                    for (std::size_t index = close + 1; index < length; ++index)
                    {
                        if (local[index] == BidiClass::L || local[index] == BidiClass::R ||
                            local[index] == BidiClass::EN || local[index] == BidiClass::AN)
                        {
                            strongAfter = (local[index] == BidiClass::L) ? BidiClass::L : BidiClass::R;
                            break;
                        }
                    }

                    bool insideClear = true;
                    for (std::size_t index = open + 1; index < close; ++index)
                    {
                        if (local[index] == BidiClass::L || local[index] == BidiClass::R ||
                            local[index] == BidiClass::EN || local[index] == BidiClass::AN)
                        {
                            insideClear = false;
                            break;
                        }
                    }

                    BidiClass resolved = (result.levels[sequence[open]] & 1) == 1 ? BidiClass::R : BidiClass::L;
                    if (insideClear && strongBefore == strongAfter && strongBefore != sos)
                    {
                        resolved = strongBefore;
                    }

                    local[open] = resolved;
                    local[close] = resolved;
                }
            }

            // N1 / N2: neutrals.
            for (std::size_t index = 0; index < length; ++index)
            {
                if (!IsNeutral(local[index]))
                {
                    continue;
                }

                std::size_t end = index;
                while (end < length && IsNeutral(local[end]))
                {
                    ++end;
                }

                const BidiClass beforeType = index == 0 ? sos : local[index - 1];
                const BidiClass afterType = end == length ? eos : local[end];

                const BidiClass beforeStrong = (beforeType == BidiClass::L) ? BidiClass::L
                                            : (beforeType == BidiClass::R || beforeType == BidiClass::EN ||
                                               beforeType == BidiClass::AN)
                                                ? BidiClass::R
                                                : BidiClass::ON;
                const BidiClass afterStrong = (afterType == BidiClass::L) ? BidiClass::L
                                           : (afterType == BidiClass::R || afterType == BidiClass::EN ||
                                              afterType == BidiClass::AN)
                                               ? BidiClass::R
                                               : BidiClass::ON;

                BidiClass resolved = BidiClass::ON;
                if (beforeStrong != BidiClass::ON && beforeStrong == afterStrong)
                {
                    resolved = beforeStrong;
                }
                else
                {
                    resolved = (result.levels[sequence[index]] & 1) == 1 ? BidiClass::R : BidiClass::L;
                }

                for (std::size_t fill = index; fill < end; ++fill)
                {
                    local[fill] = resolved;
                }

                index = end - 1;
            }

            // I1 / I2: implicit levels.
            for (std::size_t index = 0; index < length; ++index)
            {
                const std::size_t target = sequence[index];
                const std::uint8_t level = result.levels[target];

                if ((level & 1) == 0)
                {
                    if (local[index] == BidiClass::R)
                    {
                        result.levels[target] = static_cast<std::uint8_t>(level + 1);
                    }
                    else if (local[index] == BidiClass::AN || local[index] == BidiClass::EN)
                    {
                        result.levels[target] = static_cast<std::uint8_t>(level + 2);
                    }
                }
                else if (local[index] != BidiClass::R)
                {
                    result.levels[target] = static_cast<std::uint8_t>(level + (local[index] == BidiClass::L ? 1 : 0));
                    if (local[index] == BidiClass::AN || local[index] == BidiClass::EN)
                    {
                        result.levels[target] = static_cast<std::uint8_t>(level + 1);
                    }
                }
            }
        }

        // L1: separators and trailing whitespace revert to the paragraph level.
        for (std::size_t index = 0; index < count; ++index)
        {
            const BidiClass value = original[index];
            if (value == BidiClass::B || value == BidiClass::S)
            {
                result.levels[index] = paragraphLevel;
                for (std::size_t back = index; back > 0; --back)
                {
                    const BidiClass previous = original[back - 1];
                    if (previous == BidiClass::WS || IsIsolateInitiator(previous) ||
                        previous == BidiClass::PDI || previous == BidiClass::BN)
                    {
                        result.levels[back - 1] = paragraphLevel;
                        continue;
                    }
                    break;
                }
            }
        }

        for (std::size_t back = count; back > 0; --back)
        {
            const BidiClass value = original[back - 1];
            if (value == BidiClass::WS || IsIsolateInitiator(value) || value == BidiClass::PDI ||
                value == BidiClass::BN)
            {
                result.levels[back - 1] = paragraphLevel;
                continue;
            }
            break;
        }

        // L2: reverse contiguous runs, from the highest level down to the
        // lowest odd level.
        std::vector<std::uint32_t> order;
        order.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            if (result.removed[index] == 0)
            {
                order.push_back(static_cast<std::uint32_t>(index));
            }
        }

        std::uint8_t highest = 0;
        std::uint8_t lowestOdd = 0xFF;
        for (const std::uint32_t index : order)
        {
            highest = std::max(highest, result.levels[index]);
            if ((result.levels[index] & 1) != 0)
            {
                lowestOdd = std::min(lowestOdd, result.levels[index]);
            }
        }

        if (lowestOdd != 0xFF)
        {
            for (int level = highest; level >= static_cast<int>(lowestOdd); --level)
            {
                std::size_t index = 0;
                while (index < order.size())
                {
                    if (result.levels[order[index]] >= level)
                    {
                        std::size_t end = index;
                        while (end < order.size() && result.levels[order[end]] >= level)
                        {
                            ++end;
                        }
                        std::reverse(order.begin() + static_cast<std::ptrdiff_t>(index),
                                     order.begin() + static_cast<std::ptrdiff_t>(end));
                        index = end;
                    }
                    else
                    {
                        ++index;
                    }
                }
            }
        }

        result.visualToLogical = std::move(order);
        return result;
    }
}
