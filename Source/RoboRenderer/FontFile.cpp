// ---------------------------------------------------------------------------
// RoboRenderer - font file parsing.
//
// See FontFile.h for what this is and, just as importantly, what it is not.
// The parsing here is defensive by construction: every read is bounds checked,
// a missing table is a face that reports less rather than a crash, and a
// malformed glyph yields "no outline" instead of memory the rasterizer cannot
// trust.
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace RoboRenderer
{
    namespace
    {
        std::uint16_t BytesToU16(const std::uint8_t* p)
        {
            return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
        }

        std::int16_t BytesToI16(const std::uint8_t* p)
        {
            return static_cast<std::int16_t>(BytesToU16(p));
        }

        std::uint32_t BytesToU32(const std::uint8_t* p)
        {
            return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
                   (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
        }

        bool TagEquals(const std::uint8_t* p, const char* tag)
        {
            return p[0] == static_cast<std::uint8_t>(tag[0]) && p[1] == static_cast<std::uint8_t>(tag[1]) &&
                   p[2] == static_cast<std::uint8_t>(tag[2]) && p[3] == static_cast<std::uint8_t>(tag[3]);
        }

        /// UTF-16BE (what the Windows name records use) to UTF-8.
        std::string Utf16BeToUtf8(const std::uint8_t* p, std::uint32_t length)
        {
            std::string result;
            std::uint32_t index = 0;

            while (index + 1 < length)
            {
                std::uint32_t code = BytesToU16(p + index);
                index += 2;

                if (code >= 0xD800 && code <= 0xDBFF && index + 1 < length)
                {
                    const std::uint32_t low = BytesToU16(p + index);
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        index += 2;
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    }
                }

                if (code < 0x80)
                {
                    result.push_back(static_cast<char>(code));
                }
                else if (code < 0x800)
                {
                    result.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else if (code < 0x10000)
                {
                    result.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else
                {
                    result.push_back(static_cast<char>(0xF0 | (code >> 18)));
                    result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                    result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
            }

            return result;
        }

        std::string MacRomanToUtf8(const std::uint8_t* p, std::uint32_t length)
        {
            // The Mac Roman high half matters for older fonts; ASCII is exact
            // for the rest, which is all the family names in practice need.
            std::string result;
            for (std::uint32_t index = 0; index < length; ++index)
            {
                result.push_back(static_cast<char>(p[index]));
            }
            return result;
        }
    }

    std::uint32_t SfntFont::FaceCount(const std::vector<std::uint8_t>& data)
    {
        if (data.size() < 12)
        {
            return data.empty() ? 0u : 1u;
        }

        if (TagEquals(data.data(), "ttcf"))
        {
            const std::uint32_t count = BytesToU32(data.data() + 8);
            return count == 0 ? 1u : count;
        }

        return 1u;
    }

    std::unique_ptr<SfntFont> SfntFont::Load(std::vector<std::uint8_t> data, std::uint32_t faceIndex)
    {
        if (data.size() < 12)
        {
            return nullptr;
        }

        std::unique_ptr<SfntFont> font(new SfntFont());
        font->bytes = std::move(data);

        if (!font->Parse(faceIndex))
        {
            return nullptr;
        }

        return font;
    }

    std::unique_ptr<SfntFont> SfntFont::LoadFile(const std::string& path, std::uint32_t faceIndex)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return nullptr;
        }

        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        stream.seekg(0, std::ios::beg);

        if (size <= 0)
        {
            return nullptr;
        }

        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char*>(data.data()), size);

        if (!stream)
        {
            return nullptr;
        }

        return Load(std::move(data), faceIndex);
    }

    std::uint16_t SfntFont::ReadU16(std::uint32_t offset) const
    {
        if (static_cast<std::size_t>(offset) + 2 > bytes.size())
        {
            return 0;
        }
        return BytesToU16(bytes.data() + offset);
    }

    std::int16_t SfntFont::ReadI16(std::uint32_t offset) const
    {
        return static_cast<std::int16_t>(ReadU16(offset));
    }

    std::uint32_t SfntFont::ReadU32(std::uint32_t offset) const
    {
        if (static_cast<std::size_t>(offset) + 4 > bytes.size())
        {
            return 0;
        }
        return BytesToU32(bytes.data() + offset);
    }

    const SfntFont::Table& SfntFont::Find(const char* tag) const
    {
        static const Table absent;

        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "head"))
        {
            return head;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "hhea"))
        {
            return hhea;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "hmtx"))
        {
            return hmtx;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "maxp"))
        {
            return maxp;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "cmap"))
        {
            return cmap;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "glyf"))
        {
            return glyf;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "loca"))
        {
            return loca;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "OS/2"))
        {
            return os2;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "name"))
        {
            return name;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "kern"))
        {
            return kern;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "COLR"))
        {
            return colr;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "CPAL"))
        {
            return cpal;
        }
        if (TagEquals(reinterpret_cast<const std::uint8_t*>(tag), "CBDT"))
        {
            return cbdt;
        }

        return absent;
    }

    bool SfntFont::Parse(std::uint32_t faceIndex)
    {
        const std::uint32_t version = ReadU32(0);

        std::uint32_t directory = 0;

        if (version == 0x74746366u) // 'ttcf'
        {
            const std::uint32_t count = ReadU32(8);
            if (faceIndex >= count || count == 0)
            {
                return false;
            }
            directory = ReadU32(12 + faceIndex * 4);
        }
        else
        {
            if (faceIndex != 0)
            {
                return false;
            }
            directory = 0;
        }

        const std::uint16_t tableCount = ReadU16(directory + 4);

        for (std::uint16_t index = 0; index < tableCount; ++index)
        {
            const std::uint32_t record = directory + 12 + static_cast<std::uint32_t>(index) * 16;
            if (static_cast<std::size_t>(record) + 16 > bytes.size())
            {
                break;
            }

            const std::uint8_t* tag = bytes.data() + record;
            const std::uint32_t offset = ReadU32(record + 8);
            const std::uint32_t length = ReadU32(record + 12);

            Table* target = nullptr;
            if (TagEquals(tag, "head"))
            {
                target = &head;
            }
            else if (TagEquals(tag, "hhea"))
            {
                target = &hhea;
            }
            else if (TagEquals(tag, "hmtx"))
            {
                target = &hmtx;
            }
            else if (TagEquals(tag, "maxp"))
            {
                target = &maxp;
            }
            else if (TagEquals(tag, "cmap"))
            {
                target = &cmap;
            }
            else if (TagEquals(tag, "glyf"))
            {
                target = &glyf;
            }
            else if (TagEquals(tag, "loca"))
            {
                target = &loca;
            }
            else if (TagEquals(tag, "OS/2"))
            {
                target = &os2;
            }
            else if (TagEquals(tag, "name"))
            {
                target = &name;
            }
            else if (TagEquals(tag, "kern"))
            {
                target = &kern;
            }
            else if (TagEquals(tag, "COLR"))
            {
                target = &colr;
            }
            else if (TagEquals(tag, "CPAL"))
            {
                target = &cpal;
            }
            else if (TagEquals(tag, "CBDT"))
            {
                target = &cbdt;
            }

            if (target != nullptr && !target->present)
            {
                target->present = true;
                target->offset = offset;
                target->length = length;
            }
        }

        if (!head.present || !maxp.present)
        {
            return false;
        }

        ReadHead();
        ReadHhea();
        ReadOs2();
        ReadMaxp();
        ReadHmtx();
        ReadName();
        ReadCmap();
        ReadKern();

        hasGlyf = glyf.present && loca.present;
        hasColor = colr.present || cbdt.present;

        return unitsPerEm != 0 && glyphCount != 0;
    }

    void SfntFont::ReadHead()
    {
        unitsPerEm = ReadU16(head.offset + 18);
        if (unitsPerEm == 0)
        {
            unitsPerEm = 1000;
        }
        indexToLocFormat = ReadI16(head.offset + 50);

        if (hasGlyf || (glyf.present && loca.present))
        {
            // `loca` is only meaningful once `head` and `maxp` are known, which
            // is why it is built here rather than in Parse.
        }

        // `maxp` may not have been read yet; the caller reads it right after.
    }

    void SfntFont::ReadHhea()
    {
        if (!hhea.present)
        {
            return;
        }

        ascent = static_cast<float>(ReadI16(hhea.offset + 4));
        descent = -static_cast<float>(ReadI16(hhea.offset + 6)); // stored negative
        if (descent < 0.0f)
        {
            descent = -descent;
        }
        lineGap = static_cast<float>(ReadI16(hhea.offset + 8));
        numberOfHMetrics = ReadU16(hhea.offset + 34);
    }

    void SfntFont::ReadOs2()
    {
        if (!os2.present)
        {
            return;
        }

        const std::uint16_t version = ReadU16(os2.offset);

        weightClass = static_cast<int>(ReadU16(os2.offset + 4));
        if (weightClass < 1 || weightClass > 1000)
        {
            weightClass = 400;
        }

        const std::uint16_t selection = ReadU16(os2.offset + 62);
        bold = (selection & 0x0020u) != 0;
        italic = (selection & 0x0001u) != 0;

        const bool useTypoMetrics = (selection & 0x0080u) != 0;

        const float typoAscent = static_cast<float>(ReadI16(os2.offset + 68));
        const float typoDescent = -static_cast<float>(ReadI16(os2.offset + 70));
        const float typoLineGap = static_cast<float>(ReadI16(os2.offset + 72));

        if (useTypoMetrics || ascent <= 0.0f)
        {
            if (typoAscent > 0.0f)
            {
                ascent = typoAscent;
                descent = typoDescent > 0.0f ? typoDescent : descent;
            }
        }

        if (lineGap <= 0.0f && typoLineGap > 0.0f)
        {
            lineGap = typoLineGap;
        }

        if (version >= 2)
        {
            xHeight = static_cast<float>(ReadI16(os2.offset + 86));
            capHeight = static_cast<float>(ReadI16(os2.offset + 88));
        }

        if (capHeight <= 0.0f)
        {
            capHeight = ascent > 0.0f ? ascent * 0.7f : unitsPerEm * 0.7f;
        }
        if (xHeight <= 0.0f)
        {
            xHeight = ascent > 0.0f ? ascent * 0.52f : unitsPerEm * 0.52f;
        }
    }

    void SfntFont::ReadMaxp()
    {
        glyphCount = ReadU16(maxp.offset + 4);

        if (!hasGlyf && glyf.present && loca.present && glyphCount > 0)
        {
            hasGlyf = true;
        }

        if (loca.present && glyphCount > 0)
        {
            locaOffsets.resize(static_cast<std::size_t>(glyphCount) + 1);

            if (indexToLocFormat == 0)
            {
                for (std::uint32_t index = 0; index <= glyphCount; ++index)
                {
                    locaOffsets[index] = static_cast<std::uint32_t>(ReadU16(loca.offset + index * 2)) * 2u;
                }
            }
            else
            {
                for (std::uint32_t index = 0; index <= glyphCount; ++index)
                {
                    locaOffsets[index] = ReadU32(loca.offset + index * 4);
                }
            }
        }
    }

    void SfntFont::ReadHmtx()
    {
        if (numberOfHMetrics == 0)
        {
            numberOfHMetrics = glyphCount;
        }
    }

    void SfntFont::ReadName()
    {
        if (!name.present)
        {
            return;
        }

        const std::uint16_t count = ReadU16(name.offset + 2);
        const std::uint32_t stringOffset = name.offset + ReadU16(name.offset + 4);

        const auto readNameId = [&](std::uint16_t wantedId) -> std::string {
            std::string bestWindows;
            std::string bestUnicode;
            std::string bestOther;

            for (std::uint16_t index = 0; index < count; ++index)
            {
                const std::uint32_t record = name.offset + 6 + static_cast<std::uint32_t>(index) * 12;
                const std::uint16_t platform = ReadU16(record);
                const std::uint16_t encoding = ReadU16(record + 2);
                const std::uint16_t language = ReadU16(record + 4);
                const std::uint16_t nameId = ReadU16(record + 6);
                const std::uint16_t length = ReadU16(record + 8);
                const std::uint16_t offset = ReadU16(record + 10);

                if (nameId != wantedId)
                {
                    continue;
                }

                const std::uint32_t start = stringOffset + offset;
                if (static_cast<std::size_t>(start) + length > bytes.size())
                {
                    continue;
                }

                if (platform == 3 && (encoding == 1 || encoding == 10))
                {
                    if (bestWindows.empty() || language == 0x0409)
                    {
                        bestWindows = Utf16BeToUtf8(bytes.data() + start, length);
                    }
                }
                else if (platform == 0)
                {
                    if (bestUnicode.empty())
                    {
                        bestUnicode = Utf16BeToUtf8(bytes.data() + start, length);
                    }
                }
                else if (platform == 1 && bestOther.empty())
                {
                    bestOther = MacRomanToUtf8(bytes.data() + start, length);
                }
            }

            if (!bestWindows.empty())
            {
                return bestWindows;
            }
            if (!bestUnicode.empty())
            {
                return bestUnicode;
            }
            return bestOther;
        };

        familyName = readNameId(16);
        if (familyName.empty())
        {
            familyName = readNameId(1);
        }

        subfamilyName = readNameId(17);
        if (subfamilyName.empty())
        {
            subfamilyName = readNameId(2);
        }

        fullName = readNameId(4);
        postScriptName = readNameId(6);

        if (familyName.empty())
        {
            familyName = "Unknown";
        }

        // A subfamily is the fallback signal when `fsSelection` says nothing.
        if (!bold && (subfamilyName.find("Bold") != std::string::npos ||
                      subfamilyName.find("bold") != std::string::npos))
        {
            bold = true;
        }
        if (!italic && (subfamilyName.find("Italic") != std::string::npos ||
                        subfamilyName.find("Oblique") != std::string::npos))
        {
            italic = true;
        }
    }

    void SfntFont::ReadCmap()
    {
        if (!cmap.present)
        {
            return;
        }

        const std::uint16_t count = ReadU16(cmap.offset + 2);

        std::uint32_t bestOffset = 0;
        std::uint16_t bestFormat = 0;
        int bestScore = -1;

        for (std::uint16_t index = 0; index < count; ++index)
        {
            const std::uint32_t record = cmap.offset + 4 + static_cast<std::uint32_t>(index) * 8;
            const std::uint16_t platform = ReadU16(record);
            const std::uint16_t encoding = ReadU16(record + 2);
            const std::uint32_t subtable = cmap.offset + ReadU32(record + 4);
            const std::uint16_t format = ReadU16(subtable);

            int score = -1;
            if (platform == 3 && encoding == 10 && format == 12)
            {
                score = 100;
            }
            else if (platform == 0 && format == 12)
            {
                score = 90;
            }
            else if (platform == 3 && encoding == 1 && format == 4)
            {
                score = 80;
            }
            else if (platform == 0 && format == 4)
            {
                score = 70;
            }
            else if (platform == 3 && encoding == 0 && format == 4)
            {
                score = 60;
            }
            else if (format == 4)
            {
                score = 40;
            }
            else if (format == 6)
            {
                score = 30;
            }
            else if (format == 0)
            {
                score = 20;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestOffset = subtable;
                bestFormat = format;
            }
        }

        if (bestScore < 0)
        {
            return;
        }

        if (bestFormat == 12)
        {
            const std::uint32_t groupCount = ReadU32(bestOffset + 12);
            for (std::uint32_t index = 0; index < groupCount; ++index)
            {
                const std::uint32_t group = bestOffset + 16 + index * 12;
                if (static_cast<std::size_t>(group) + 12 > bytes.size())
                {
                    break;
                }
                Range range;
                range.start = ReadU32(group);
                range.end = ReadU32(group + 4);
                range.startGlyph = ReadU32(group + 8);
                if (range.start <= range.end && range.end <= 0x10FFFFu)
                {
                    cmapRanges.push_back(range);
                }
            }
        }
        else if (bestFormat == 4)
        {
            const std::uint16_t segmentCount = ReadU16(bestOffset + 6) / 2;
            const std::uint32_t endCodes = bestOffset + 14;
            const std::uint32_t startCodes = endCodes + static_cast<std::uint32_t>(segmentCount) * 2 + 2;
            const std::uint32_t deltas = startCodes + static_cast<std::uint32_t>(segmentCount) * 2;
            const std::uint32_t rangeOffsets = deltas + static_cast<std::uint32_t>(segmentCount) * 2;

            for (std::uint16_t index = 0; index < segmentCount; ++index)
            {
                const std::uint32_t start = ReadU16(startCodes + index * 2);
                const std::uint32_t end = ReadU16(endCodes + index * 2);
                if (start > end)
                {
                    continue;
                }

                const std::uint16_t rangeOffset = ReadU16(rangeOffsets + index * 2);
                const std::uint16_t delta = ReadU16(deltas + index * 2);

                if (rangeOffset == 0)
                {
                    if (start == 0xFFFFu && end == 0xFFFFu)
                    {
                        continue;
                    }
                    Range range;
                    range.start = start;
                    range.end = end;
                    range.startGlyph = (start + delta) & 0xFFFFu;
                    cmapRanges.push_back(range);
                }
                else
                {
                    // Indirect segments are expanded; the cap keeps a malformed
                    // table from turning into a giant allocation.
                    const std::uint32_t length = end - start + 1;
                    if (length > 8192)
                    {
                        continue;
                    }
                    for (std::uint32_t code = start; code <= end; ++code)
                    {
                        const std::uint32_t glyphAddress =
                            rangeOffsets + index * 2 + rangeOffset + (code - start) * 2;
                        const std::uint16_t glyph = ReadU16(glyphAddress);
                        if (glyph != 0)
                        {
                            cmapSingles[code] = static_cast<std::uint16_t>((glyph + delta) & 0xFFFFu);
                        }
                    }
                }
            }
        }
        else if (bestFormat == 6)
        {
            const std::uint32_t firstCode = ReadU16(bestOffset + 6);
            const std::uint32_t entryCount = ReadU16(bestOffset + 8);
            for (std::uint32_t index = 0; index < entryCount; ++index)
            {
                const std::uint16_t glyph = ReadU16(bestOffset + 10 + index * 2);
                if (glyph != 0)
                {
                    cmapSingles[firstCode + index] = glyph;
                }
            }
        }
        else if (bestFormat == 0)
        {
            for (std::uint32_t code = 0; code < 256; ++code)
            {
                const std::uint16_t glyph = ReadU16(bestOffset + 6 + code * 2) & 0xFFu;
                if (glyph != 0)
                {
                    cmapSingles[code] = static_cast<std::uint16_t>(glyph);
                }
            }
        }

        std::sort(cmapRanges.begin(), cmapRanges.end(), [](const Range& a, const Range& b) {
            return a.start < b.start;
        });
    }

    void SfntFont::ReadKern()
    {
        if (!kern.present)
        {
            return;
        }

        const std::uint16_t tableCount = ReadU16(kern.offset + 2);

        for (std::uint16_t index = 0; index < tableCount; ++index)
        {
            const std::uint32_t subtable = kern.offset + 4 + static_cast<std::uint32_t>(index) * 8;
            const std::uint16_t length = ReadU16(subtable + 2);
            const std::uint16_t coverage = ReadU16(subtable + 4);
            const std::uint16_t format = ReadU16(subtable + 6);

            // Horizontal, non-cross-stream, non-minimum tables only: that is the
            // format 0 pair table a text layout wants.
            if (format != 0 || (coverage & 0x0001u) == 0)
            {
                continue;
            }

            const std::uint16_t pairCount = ReadU16(subtable + 8);
            for (std::uint16_t pair = 0; pair < pairCount; ++pair)
            {
                const std::uint32_t entry = subtable + 14 + static_cast<std::uint32_t>(pair) * 6;
                if (static_cast<std::size_t>(entry) + 6 > bytes.size())
                {
                    break;
                }
                const std::uint16_t left = ReadU16(entry);
                const std::uint16_t right = ReadU16(entry + 2);
                const std::int16_t value = ReadI16(entry + 4);
                kernPairs[(static_cast<std::uint32_t>(left) << 16) | right] = value;
            }

            if (length == 0)
            {
                break;
            }
            break;
        }
    }

    std::uint16_t SfntFont::GlyphIndex(std::uint32_t codepoint) const
    {
        const auto single = cmapSingles.find(codepoint);
        if (single != cmapSingles.end())
        {
            return single->second;
        }

        // Binary search the sorted ranges.
        std::size_t low = 0;
        std::size_t high = cmapRanges.size();

        while (low < high)
        {
            const std::size_t mid = (low + high) / 2;
            if (cmapRanges[mid].start <= codepoint)
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
            return 0;
        }

        const Range& range = cmapRanges[low - 1];
        if (codepoint > range.end)
        {
            return 0;
        }

        return static_cast<std::uint16_t>((range.startGlyph + (codepoint - range.start)) & 0xFFFFu);
    }

    float SfntFont::Advance(std::uint16_t glyph) const
    {
        if (!hmtx.present || numberOfHMetrics == 0)
        {
            return unitsPerEm * 0.5f;
        }

        const std::uint16_t metric = glyph < numberOfHMetrics ? glyph
                                                             : static_cast<std::uint16_t>(numberOfHMetrics - 1);
        return static_cast<float>(ReadU16(hmtx.offset + metric * 4));
    }

    void SfntFont::Bounds(std::uint16_t glyph,
                          float& x0,
                          float& y0,
                          float& x1,
                          float& y1) const
    {
        x0 = y0 = x1 = y1 = 0.0f;

        if (!hasGlyf || glyph >= glyphCount || locaOffsets.size() <= glyph + 1)
        {
            return;
        }

        const std::uint32_t start = locaOffsets[glyph];
        const std::uint32_t end = locaOffsets[glyph + 1];
        if (start >= end)
        {
            return;
        }

        const std::uint32_t base = glyf.offset + start;
        if (static_cast<std::size_t>(base) + 10 > bytes.size())
        {
            return;
        }

        x0 = static_cast<float>(ReadI16(base + 2));
        y0 = static_cast<float>(ReadI16(base + 4));
        x1 = static_cast<float>(ReadI16(base + 6));
        y1 = static_cast<float>(ReadI16(base + 8));
    }

    std::int32_t SfntFont::Kern(std::uint16_t left, std::uint16_t right) const
    {
        if (kernPairs.empty())
        {
            return 0;
        }

        const auto found = kernPairs.find((static_cast<std::uint32_t>(left) << 16) | right);
        return found == kernPairs.end() ? 0 : found->second;
    }

    bool SfntFont::Outline(std::uint16_t glyph, GlyphPath& out) const
    {
        out.points.clear();
        out.ends.clear();

        if (!hasGlyf || glyph >= glyphCount)
        {
            return false;
        }

        return ReadGlyf(glyph, out, 0);
    }

    bool SfntFont::ReadGlyf(std::uint16_t glyph, GlyphPath& out, int depth) const
    {
        if (depth > 5 || glyph >= glyphCount || locaOffsets.size() <= glyph + 1)
        {
            return false;
        }

        const std::uint32_t start = locaOffsets[glyph];
        const std::uint32_t end = locaOffsets[glyph + 1];
        if (start >= end)
        {
            return false; // A blank glyph: a space, or an empty contour holder.
        }

        const std::uint32_t base = glyf.offset + start;
        if (static_cast<std::size_t>(base) + 2 > bytes.size())
        {
            return false;
        }

        const std::int16_t contourCount = ReadI16(base);

        if (contourCount >= 0)
        {
            std::uint32_t cursor = base + 10;
            if (static_cast<std::size_t>(cursor) + static_cast<std::size_t>(contourCount) * 2 > bytes.size())
            {
                return false;
            }

            std::vector<std::uint16_t> endPoints(static_cast<std::size_t>(contourCount));
            for (std::int16_t index = 0; index < contourCount; ++index)
            {
                endPoints[static_cast<std::size_t>(index)] = ReadU16(cursor);
                cursor += 2;
            }

            const std::uint16_t instructionLength = ReadU16(cursor);
            cursor += 2 + instructionLength;

            const std::uint32_t pointCount =
                contourCount == 0 ? 0 : static_cast<std::uint32_t>(endPoints.back()) + 1;

            if (pointCount > 10000 || static_cast<std::size_t>(cursor) > bytes.size())
            {
                return false;
            }

            std::vector<std::uint8_t> flags;
            flags.reserve(pointCount);

            while (flags.size() < pointCount)
            {
                if (static_cast<std::size_t>(cursor) >= bytes.size())
                {
                    return false;
                }
                const std::uint8_t flag = bytes[cursor++];
                flags.push_back(flag);

                if ((flag & 0x08u) != 0)
                {
                    if (static_cast<std::size_t>(cursor) >= bytes.size())
                    {
                        return false;
                    }
                    const std::uint8_t repeat = bytes[cursor++];
                    for (std::uint8_t count = 0; count < repeat && flags.size() < pointCount; ++count)
                    {
                        flags.push_back(flag);
                    }
                }
            }

            std::vector<float> xs(pointCount, 0.0f);
            std::vector<float> ys(pointCount, 0.0f);

            std::int32_t value = 0;
            for (std::uint32_t index = 0; index < pointCount; ++index)
            {
                const std::uint8_t flag = flags[index];
                if ((flag & 0x02u) != 0)
                {
                    if (static_cast<std::size_t>(cursor) >= bytes.size())
                    {
                        return false;
                    }
                    const std::uint8_t delta = bytes[cursor++];
                    value += (flag & 0x10u) != 0 ? static_cast<std::int32_t>(delta)
                                                 : -static_cast<std::int32_t>(delta);
                }
                else if ((flag & 0x10u) == 0)
                {
                    value += ReadI16(cursor);
                    cursor += 2;
                }
                xs[index] = static_cast<float>(value);
            }

            value = 0;
            for (std::uint32_t index = 0; index < pointCount; ++index)
            {
                const std::uint8_t flag = flags[index];
                if ((flag & 0x04u) != 0)
                {
                    if (static_cast<std::size_t>(cursor) >= bytes.size())
                    {
                        return false;
                    }
                    const std::uint8_t delta = bytes[cursor++];
                    value += (flag & 0x20u) != 0 ? static_cast<std::int32_t>(delta)
                                                 : -static_cast<std::int32_t>(delta);
                }
                else if ((flag & 0x20u) == 0)
                {
                    value += ReadI16(cursor);
                    cursor += 2;
                }
                ys[index] = static_cast<float>(value);
            }

            std::uint32_t startIndex = 0;
            for (std::int16_t contour = 0; contour < contourCount; ++contour)
            {
                const std::uint32_t last = endPoints[static_cast<std::size_t>(contour)];
                for (std::uint32_t index = startIndex; index <= last && index < pointCount; ++index)
                {
                    ContourPoint point;
                    point.x = xs[index];
                    point.y = ys[index];
                    point.onCurve = (flags[index] & 0x01u) != 0;
                    out.points.push_back(point);
                }
                out.ends.push_back(static_cast<std::uint32_t>(out.points.size()));
                startIndex = last + 1;
            }

            return !out.ends.empty();
        }

        // Composite glyph: assemble the component outlines with their offsets.
        std::uint32_t cursor = base + 10;
        bool any = false;

        for (int component = 0; component < 32; ++component)
        {
            if (static_cast<std::size_t>(cursor) + 4 > bytes.size())
            {
                break;
            }

            const std::uint16_t flags = ReadU16(cursor);
            const std::uint16_t componentGlyph = ReadU16(cursor + 2);
            cursor += 4;

            const bool words = (flags & 0x0001u) != 0;
            const bool xyValues = (flags & 0x0002u) != 0;

            std::int32_t dx = 0;
            std::int32_t dy = 0;

            if (words)
            {
                dx = xyValues ? ReadI16(cursor) : 0;
                dy = xyValues ? ReadI16(cursor + 2) : 0;
                cursor += 4;
            }
            else
            {
                if (xyValues)
                {
                    if (static_cast<std::size_t>(cursor) + 2 > bytes.size())
                    {
                        break;
                    }
                    dx = static_cast<std::int8_t>(bytes[cursor]);
                    dy = static_cast<std::int8_t>(bytes[cursor + 1]);
                }
                cursor += 2;
            }

            float a = 1.0f;
            float b = 0.0f;
            float c = 0.0f;
            float d = 1.0f;

            const auto readF2Dot14 = [&]() -> float {
                const std::int16_t raw = ReadI16(cursor);
                cursor += 2;
                return static_cast<float>(raw) / 16384.0f;
            };

            if ((flags & 0x0008u) != 0)
            {
                a = d = readF2Dot14();
            }
            else if ((flags & 0x0040u) != 0)
            {
                a = readF2Dot14();
                d = readF2Dot14();
            }
            else if ((flags & 0x0080u) != 0)
            {
                a = readF2Dot14();
                b = readF2Dot14();
                c = readF2Dot14();
                d = readF2Dot14();
            }

            GlyphPath componentPath;
            if (ReadGlyf(componentGlyph, componentPath, depth + 1))
            {
                const std::uint32_t partContours =
                    static_cast<std::uint32_t>(componentPath.ends.size());
                std::uint32_t pointIndex = 0;

                for (std::uint32_t contour = 0; contour < partContours; ++contour)
                {
                    const std::uint32_t last = componentPath.ends[contour];
                    for (; pointIndex < last; ++pointIndex)
                    {
                        const ContourPoint& source = componentPath.points[pointIndex];
                        ContourPoint point;
                        point.x = source.x * a + source.y * c + static_cast<float>(dx);
                        point.y = source.x * b + source.y * d + static_cast<float>(dy);
                        point.onCurve = source.onCurve;
                        out.points.push_back(point);
                    }
                    out.ends.push_back(static_cast<std::uint32_t>(out.points.size()));
                }

                any = true;
            }

            if ((flags & 0x0020u) == 0)
            {
                break;
            }
        }

        return any;
    }
}
