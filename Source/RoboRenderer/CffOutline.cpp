// ---------------------------------------------------------------------------
// RoboRenderer - CFF / CFF2 charstring outlines.
//
// An `OTTO` face stores its glyphs as PostScript charstrings instead of TrueType
// `glyf` outlines. The charstrings are a tiny stack language: numbers are pushed
// and an operator consumes them to draw lines and cubics relative to the pen.
// This file parses the CFF container (INDEXes, DICTs, the private subrs a
// charstring calls) and interprets the Type 2 language into the same `GlyphPath`
// a `glyf` face produces.
//
// The renderer's contours are quadratic; Type 2 cubics have no quadratic form,
// so each cubic is flattened to a polyline at a tolerance smaller than a font
// unit. The result is geometrically indistinguishable after scaling to device
// pixels, and the rasterizer never learns that the face was CFF.
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace RoboRenderer
{
    namespace
    {
        /// The number a Type 2 subr call has to add to turn a stored index into a
        /// real one. The bias depends on how many subrs exist.
        int SubrBias(std::size_t count)
        {
            if (count < 1240)
            {
                return 107;
            }
            if (count < 33900)
            {
                return 1131;
            }
            return 32768;
        }

        /// Distance from a point to a line segment, used to decide how finely a
        /// cubic has to be flattened.
        float PointSegmentDistance(float px,
                                   float py,
                                   float ax,
                                   float ay,
                                   float bx,
                                   float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float lengthSquared = dx * dx + dy * dy;

            if (lengthSquared <= 0.0f)
            {
                return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
            }

            float t = ((px - ax) * dx + (py - ay) * dy) / lengthSquared;
            t = std::max(0.0f, std::min(1.0f, t));

            const float cx = ax + t * dx;
            const float cy = ay + t * dy;
            return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
        }

        /// Iterates a CFF DICT: operands in, then an operator. The operator is
        /// `b0` for a one-byte operator and `0x0C00 | b1` for a two-byte one.
        void IterateDict(const std::uint8_t* bytes,
                         std::uint32_t size,
                         std::uint32_t start,
                         std::uint32_t end,
                         const std::function<void(std::uint16_t, const std::vector<float>&)>& visit)
        {
            std::vector<float> operands;
            operands.reserve(8);

            std::uint32_t ip = start;
            while (ip < end && ip < size)
            {
                const std::uint8_t b0 = bytes[ip++];

                if (b0 == 28)
                {
                    if (ip + 1 >= size)
                    {
                        return;
                    }
                    const std::int16_t value =
                        static_cast<std::int16_t>((bytes[ip] << 8) | bytes[ip + 1]);
                    ip += 2;
                    operands.push_back(static_cast<float>(value));
                }
                else if (b0 == 29)
                {
                    if (ip + 3 >= size)
                    {
                        return;
                    }
                    const std::int32_t value = static_cast<std::int32_t>(
                        (static_cast<std::uint32_t>(bytes[ip]) << 24) |
                        (static_cast<std::uint32_t>(bytes[ip + 1]) << 16) |
                        (static_cast<std::uint32_t>(bytes[ip + 2]) << 8) |
                        static_cast<std::uint32_t>(bytes[ip + 3]));
                    ip += 4;
                    operands.push_back(static_cast<float>(value));
                }
                else if (b0 == 30)
                {
                    // A real number, encoded as nibbles.
                    std::string text;
                    bool done = false;
                    while (!done && ip < size)
                    {
                        const std::uint8_t packed = bytes[ip++];
                        for (int half = 0; half < 2 && !done; ++half)
                        {
                            const std::uint8_t nibble = half == 0 ? (packed >> 4) : (packed & 0x0F);
                            switch (nibble)
                            {
                                case 0: case 1: case 2: case 3: case 4:
                                case 5: case 6: case 7: case 8: case 9:
                                    text.push_back(static_cast<char>('0' + nibble));
                                    break;
                                case 0x0A: text.push_back('.'); break;
                                case 0x0B: text.push_back('E'); break;
                                case 0x0C: text += "E-"; break;
                                case 0x0D: break; // reserved
                                case 0x0E: text.push_back('-'); break;
                                case 0x0F: done = true; break;
                                default: break;
                            }
                        }
                    }
                    operands.push_back(text.empty() ? 0.0f : static_cast<float>(std::atof(text.c_str())));
                }
                else if (b0 >= 32)
                {
                    int value = 0;
                    if (b0 <= 246)
                    {
                        value = static_cast<int>(b0) - 139;
                    }
                    else if (b0 <= 250)
                    {
                        if (ip >= size) { return; }
                        value = (static_cast<int>(b0) - 247) * 256 + static_cast<int>(bytes[ip++]) + 108;
                    }
                    else if (b0 <= 254)
                    {
                        if (ip >= size) { return; }
                        value = -(static_cast<int>(b0) - 251) * 256 - static_cast<int>(bytes[ip++]) - 108;
                    }
                    else
                    {
                        // 255 is not a DICT operand prefix; treat it as a stray byte.
                        continue;
                    }
                    operands.push_back(static_cast<float>(value));
                }
                else
                {
                    std::uint16_t op = b0;
                    if (b0 == 12)
                    {
                        if (ip >= size) { return; }
                        op = static_cast<std::uint16_t>(0x0C00 | bytes[ip++]);
                    }

                    visit(op, operands);
                    operands.clear();
                }
            }
        }

        /// One code point's worth of line/curve drawing.
        constexpr float kFlatnessTolerance = 0.25f;
    }

    /// The Type 2 interpreter's mutable state. A subroutine call shares it with
    /// its caller, so the pen, the hint count and the contour under construction
    /// survive the call.
    struct CffFont::Interp
    {
        std::vector<float> stack;
        float transient[32] = {};
        int stems = 0;
        bool widthSeen = false;
        float x = 0.0f;
        float y = 0.0f;
        int contourStart = -1;
        GlyphPath* path = nullptr;
    };

    std::unique_ptr<CffFont> CffFont::Load(const std::uint8_t* data,
                                           std::uint32_t size,
                                           std::uint32_t glyphCount)
    {
        if (data == nullptr || size < 4)
        {
            return nullptr;
        }

        auto font = std::unique_ptr<CffFont>(new CffFont());
        font->bytes.assign(data, data + size);
        font->size = size;
        font->glyphCountHint = glyphCount;

        const std::uint8_t major = font->bytes[0];
        font->cff2 = major == 2;

        if (major == 1)
        {
            const std::uint8_t headerSize = font->bytes[2];
            std::uint32_t offset = std::max<std::uint32_t>(headerSize, 4u);

            Index names;
            if (!font->ReadIndex(offset, names, &offset)) { return nullptr; }

            Index topDict;
            if (!font->ReadIndex(offset, topDict, &offset)) { return nullptr; }
            if (topDict.count == 0) { return nullptr; }

            Index strings;
            if (!font->ReadIndex(offset, strings, &offset)) { return nullptr; }

            Index globalSubrs;
            if (!font->ReadIndex(offset, globalSubrs, &offset)) { return nullptr; }
            font->globalSubrs = globalSubrs;

            font->ParseTopDict(topDict.Start(0), topDict.End(0), false);
        }
        else if (major == 2)
        {
            const std::uint8_t headerSize = font->bytes[2];
            const std::uint16_t topDictLength =
                static_cast<std::uint16_t>((font->bytes[3] << 8) | font->bytes[4]);

            const std::uint32_t start = headerSize;
            const std::uint32_t end = start + topDictLength;
            if (end > size) { return nullptr; }

            std::uint32_t offset = end;
            Index globalSubrs;
            if (!font->ReadIndex(offset, globalSubrs, &offset)) { return nullptr; }
            font->globalSubrs = globalSubrs;

            font->ParseTopDict(start, end, true);
        }
        else
        {
            return nullptr;
        }

        if (font->charStringsOffset == 0)
        {
            return nullptr;
        }
        if (!font->ReadIndex(font->charStringsOffset, font->charStrings, nullptr))
        {
            return nullptr;
        }

        if (font->fdArrayOffset != 0)
        {
            Index fdArray;
            if (!font->ReadIndex(font->fdArrayOffset, fdArray, nullptr)) { return nullptr; }

            font->fontDicts.resize(fdArray.count);
            for (std::uint32_t index = 0; index < fdArray.count; ++index)
            {
                font->ParseFontDict(fdArray.Start(index), fdArray.End(index), font->fontDicts[index]);
            }

            if (font->fdSelectOffset != 0)
            {
                font->ReadFdSelect(font->fdSelectOffset);
            }
        }
        else
        {
            FontDict dict;
            dict.priv = font->topPrivate;
            dict.charStringsOffset = font->charStringsOffset;
            font->fontDicts.push_back(dict);
        }

        return font;
    }

    std::uint8_t CffFont::ByteAt(std::uint32_t offset) const
    {
        return offset < size ? bytes[offset] : 0;
    }

    std::uint16_t CffFont::U16At(std::uint32_t offset) const
    {
        return offset + 1 < size
                   ? static_cast<std::uint16_t>((bytes[offset] << 8) | bytes[offset + 1])
                   : 0;
    }

    std::uint32_t CffFont::U32At(std::uint32_t offset) const
    {
        return offset + 3 < size
                   ? (static_cast<std::uint32_t>(bytes[offset]) << 24) |
                         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
                         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
                         static_cast<std::uint32_t>(bytes[offset + 3])
                   : 0;
    }

    bool CffFont::ReadIndex(std::uint32_t offset, Index& out, std::uint32_t* next) const
    {
        out.count = 0;
        out.offsets.clear();

        if (offset + 2 > size)
        {
            return false;
        }

        const std::uint16_t count = U16At(offset);
        out.count = count;

        if (count == 0)
        {
            if (next != nullptr) { *next = offset + 2; }
            return true;
        }

        if (offset + 3 > size)
        {
            return false;
        }

        const std::uint8_t offSize = bytes[offset + 2];
        if (offSize < 1 || offSize > 4)
        {
            return false;
        }

        const std::uint32_t arrayStart = offset + 3;
        const std::uint64_t arrayBytes = static_cast<std::uint64_t>(count + 1) * offSize;

        if (static_cast<std::uint64_t>(arrayStart) + arrayBytes > size)
        {
            return false;
        }

        const std::uint32_t dataStart = static_cast<std::uint32_t>(arrayStart + arrayBytes);

        out.offsets.resize(static_cast<std::size_t>(count) + 1);
        for (std::uint32_t index = 0; index <= count; ++index)
        {
            std::uint32_t value = 0;
            for (std::uint8_t byte = 0; byte < offSize; ++byte)
            {
                value = (value << 8) | bytes[arrayStart + index * offSize + byte];
            }
            // Offsets are one-based relative to the first data byte.
            const std::uint32_t absolute = dataStart + (value == 0 ? 0 : value - 1);
            if (absolute > size)
            {
                return false;
            }
            out.offsets[index] = absolute;
        }

        if (next != nullptr)
        {
            *next = out.offsets[count];
        }
        return true;
    }

    void CffFont::ParsePrivateDict(std::uint32_t offset, std::uint32_t dictSize, Private& out)
    {
        out.present = true;

        std::uint32_t subrsRelative = 0;
        IterateDict(bytes.data(), size, offset, offset + dictSize,
                    [&](std::uint16_t op, const std::vector<float>& operands) {
                        switch (op)
                        {
                            case 19: // Subrs, relative to the Private DICT start
                                if (!operands.empty()) { subrsRelative = static_cast<std::uint32_t>(operands[0]); }
                                break;
                            case 20: // defaultWidthX
                                if (!operands.empty()) { out.defaultWidthX = static_cast<std::int32_t>(operands[0]); }
                                break;
                            case 21: // nominalWidthX
                                if (!operands.empty()) { out.nominalWidthX = static_cast<std::int32_t>(operands[0]); }
                                break;
                            default: break;
                        }
                    });

        if (subrsRelative != 0)
        {
            ReadIndex(offset + subrsRelative, out.subrs, nullptr);
        }
    }

    void CffFont::ParseTopDict(std::uint32_t start, std::uint32_t end, bool isCff2)
    {
        std::uint32_t privateSize = 0;
        std::uint32_t privateOffset = 0;

        IterateDict(bytes.data(), size, start, end,
                    [&](std::uint16_t op, const std::vector<float>& operands) {
                        switch (op)
                        {
                            case 17: // CharStrings
                                if (!operands.empty())
                                {
                                    charStringsOffset = static_cast<std::uint32_t>(operands.back());
                                }
                                break;
                            case 18: // Private: [size, offset]
                                if (operands.size() >= 2)
                                {
                                    privateSize = static_cast<std::uint32_t>(operands[0]);
                                    privateOffset = static_cast<std::uint32_t>(operands[1]);
                                }
                                break;
                            case 0x0C24: // 12 36: FDArray
                                if (!operands.empty()) { fdArrayOffset = static_cast<std::uint32_t>(operands.back()); }
                                break;
                            case 0x0C25: // 12 37: FDSelect
                                if (!operands.empty()) { fdSelectOffset = static_cast<std::uint32_t>(operands.back()); }
                                break;
                            default: break;
                        }
                    });

        if (privateSize > 0 && privateOffset > 0 && privateOffset < size)
        {
            const std::uint32_t clamped = std::min(privateSize, size - privateOffset);
            ParsePrivateDict(privateOffset, clamped, topPrivate);
        }

        (void)isCff2;
    }

    void CffFont::ParseFontDict(std::uint32_t start, std::uint32_t end, FontDict& out)
    {
        std::uint32_t privateSize = 0;
        std::uint32_t privateOffset = 0;

        IterateDict(bytes.data(), size, start, end,
                    [&](std::uint16_t op, const std::vector<float>& operands) {
                        switch (op)
                        {
                            case 17:
                                if (!operands.empty()) { out.charStringsOffset = static_cast<std::uint32_t>(operands.back()); }
                                break;
                            case 18:
                                if (operands.size() >= 2)
                                {
                                    privateSize = static_cast<std::uint32_t>(operands[0]);
                                    privateOffset = static_cast<std::uint32_t>(operands[1]);
                                }
                                break;
                            default: break;
                        }
                    });

        if (privateSize > 0 && privateOffset > 0 && privateOffset < size)
        {
            const std::uint32_t clamped = std::min(privateSize, size - privateOffset);
            ParsePrivateDict(privateOffset, clamped, out.priv);
        }
    }

    void CffFont::ReadFdSelect(std::uint32_t offset)
    {
        glyphFd.assign(charStrings.count, 0);

        if (offset >= size)
        {
            return;
        }

        const std::uint8_t format = bytes[offset];

        if (format == 0)
        {
            for (std::uint32_t glyph = 0; glyph < charStrings.count; ++glyph)
            {
                const std::uint32_t at = offset + 1 + glyph;
                glyphFd[glyph] = at < size ? bytes[at] : 0;
            }
            return;
        }

        if (format == 3)
        {
            const std::uint16_t rangeCount = U16At(offset + 1);
            std::uint32_t at = offset + 3;

            for (std::uint16_t range = 0; range < rangeCount; ++range)
            {
                const std::uint16_t first = U16At(at);
                const std::uint8_t fd = ByteAt(at + 2);
                const std::uint16_t next = U16At(at + 3);
                at += 3;

                for (std::uint32_t glyph = first; glyph < next && glyph < charStrings.count; ++glyph)
                {
                    glyphFd[glyph] = fd;
                }
            }
            return;
        }

        if (format == 4)
        {
            // CFF2: 32-bit glyph ids.
            const std::uint32_t rangeCount = U32At(offset + 1);
            std::uint32_t at = offset + 5;

            for (std::uint32_t range = 0; range < rangeCount; ++range)
            {
                const std::uint32_t first = U32At(at);
                const std::uint16_t fd = U16At(at + 4);
                const std::uint32_t next = U32At(at + 6);
                at += 6;

                for (std::uint32_t glyph = first; glyph < next && glyph < charStrings.count; ++glyph)
                {
                    glyphFd[glyph] = fd;
                }
            }
        }
    }

    std::uint32_t CffFont::FontDictIndexForGlyph(std::uint32_t glyph) const
    {
        if (fontDicts.empty())
        {
            return 0;
        }

        std::uint32_t index = 0;
        if (glyph < glyphFd.size())
        {
            index = glyphFd[glyph];
        }

        if (index >= fontDicts.size())
        {
            index = 0;
        }
        return index;
    }

    bool CffFont::Outline(std::uint32_t glyph, GlyphPath& out) const
    {
        out.points.clear();
        out.ends.clear();

        if (glyph >= charStrings.count)
        {
            return false;
        }

        const std::uint32_t start = charStrings.Start(glyph);
        const std::uint32_t end = charStrings.End(glyph);
        if (end <= start || end > size)
        {
            return false;
        }

        const std::uint32_t fd = FontDictIndexForGlyph(glyph);
        const Index& subrs = fontDicts[fd].priv.subrs;

        Interp state;
        state.path = &out;
        state.stack.reserve(48);

        RunCharstring(bytes.data() + start, end - start, state, subrs, 0);

        // Close the last contour the charstring left open.
        if (state.contourStart >= 0 && static_cast<std::uint32_t>(out.points.size()) >
                                           static_cast<std::uint32_t>(state.contourStart))
        {
            out.ends.push_back(static_cast<std::uint32_t>(out.points.size()));
        }

        return !out.ends.empty();
    }

    void CffFont::RunCharstring(const std::uint8_t* code,
                                std::uint32_t length,
                                Interp& state,
                                const Index& localSubrs,
                                int depth) const
    {
        if (code == nullptr || depth > 10)
        {
            return;
        }

        const auto pushPoint = [&](float px, float py) {
            ContourPoint point;
            point.x = px;
            point.y = py;
            point.onCurve = true;
            state.path->points.push_back(point);
        };

        const auto moveTo = [&](float nx, float ny) {
            GlyphPath* path = state.path;
            if (path != nullptr && state.contourStart >= 0 &&
                static_cast<std::uint32_t>(path->points.size()) >
                    static_cast<std::uint32_t>(state.contourStart))
            {
                path->ends.push_back(static_cast<std::uint32_t>(path->points.size()));
            }

            pushPoint(nx, ny);
            state.contourStart = static_cast<int>(state.path->points.size()) - 1;
            state.x = nx;
            state.y = ny;
        };

        const auto lineTo = [&](float nx, float ny) {
            if (state.contourStart < 0)
            {
                moveTo(nx, ny);
                return;
            }
            pushPoint(nx, ny);
            state.x = nx;
            state.y = ny;
        };

        const auto curveTo = [&](float c1x, float c1y, float c2x, float c2y, float ex, float ey) {
            if (state.contourStart < 0)
            {
                moveTo(state.x, state.y);
            }

            const float px = state.x;
            const float py = state.y;

            const float d1 = PointSegmentDistance(c1x, c1y, px, py, ex, ey);
            const float d2 = PointSegmentDistance(c2x, c2y, px, py, ex, ey);
            const float deviation = std::max(d1, d2);

            int steps = static_cast<int>(std::ceil(std::sqrt(deviation / kFlatnessTolerance)));
            steps = std::max(2, std::min(32, steps));

            for (int step = 1; step < steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                const float u = 1.0f - t;
                const float uu = u * u;
                const float tt = t * t;

                const float bx = uu * u * px + 3.0f * uu * t * c1x + 3.0f * u * tt * c2x + tt * t * ex;
                const float by = uu * u * py + 3.0f * uu * t * c1y + 3.0f * u * tt * c2y + tt * t * ey;
                pushPoint(bx, by);
            }

            pushPoint(ex, ey);
            state.x = ex;
            state.y = ey;
        };

        const auto takeWidth = [&](int expected) {
            if (state.widthSeen)
            {
                return;
            }
            state.widthSeen = true;
            if (static_cast<int>(state.stack.size()) > expected)
            {
                state.stack.erase(state.stack.begin());
            }
        };

        const auto at = [&](std::size_t index) -> float {
            return index < state.stack.size() ? state.stack[index] : 0.0f;
        };

        std::uint32_t ip = 0;
        while (ip < length)
        {
            const std::uint8_t b0 = code[ip++];

            if (b0 == 28)
            {
                if (ip + 1 >= length) { return; }
                const std::int16_t value = static_cast<std::int16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                state.stack.push_back(static_cast<float>(value));
                continue;
            }

            if (b0 == 255)
            {
                if (ip + 3 >= length) { return; }
                const std::int32_t value = static_cast<std::int32_t>(
                    (static_cast<std::uint32_t>(code[ip]) << 24) |
                    (static_cast<std::uint32_t>(code[ip + 1]) << 16) |
                    (static_cast<std::uint32_t>(code[ip + 2]) << 8) |
                    static_cast<std::uint32_t>(code[ip + 3]));
                ip += 4;
                state.stack.push_back(static_cast<float>(value) / 65536.0f);
                continue;
            }

            if (b0 >= 32)
            {
                int value = 0;
                if (b0 <= 246)
                {
                    value = static_cast<int>(b0) - 139;
                }
                else if (b0 <= 250)
                {
                    if (ip >= length) { return; }
                    value = (static_cast<int>(b0) - 247) * 256 + static_cast<int>(code[ip++]) + 108;
                }
                else
                {
                    if (ip >= length) { return; }
                    value = -(static_cast<int>(b0) - 251) * 256 - static_cast<int>(code[ip++]) - 108;
                }
                state.stack.push_back(static_cast<float>(value));
                continue;
            }

            switch (b0)
            {
                case 1: // hstem
                case 3: // vstem
                case 18: // hstemhm
                case 23: // vstemhm
                {
                    const int count = static_cast<int>(state.stack.size());
                    takeWidth(count % 2 == 0 ? count : count - 1);
                    state.stems += static_cast<int>(state.stack.size()) / 2;
                    state.stack.clear();
                    break;
                }

                case 19: // hintmask
                case 20: // cntrmask
                {
                    const int count = static_cast<int>(state.stack.size());
                    takeWidth(count % 2 == 0 ? count : count - 1);
                    state.stems += static_cast<int>(state.stack.size()) / 2;
                    state.stack.clear();
                    ip += static_cast<std::uint32_t>((state.stems + 7) / 8);
                    break;
                }

                case 21: // rmoveto
                {
                    takeWidth(2);
                    const float dx = at(0);
                    const float dy = at(1);
                    moveTo(state.x + dx, state.y + dy);
                    state.stack.clear();
                    break;
                }

                case 22: // hmoveto
                {
                    takeWidth(1);
                    moveTo(state.x + at(0), state.y);
                    state.stack.clear();
                    break;
                }

                case 4: // vmoveto
                {
                    takeWidth(1);
                    moveTo(state.x, state.y + at(0));
                    state.stack.clear();
                    break;
                }

                case 5: // rlineto
                {
                    for (std::size_t index = 0; index + 1 < state.stack.size(); index += 2)
                    {
                        lineTo(state.x + at(index), state.y + at(index + 1));
                    }
                    state.stack.clear();
                    break;
                }

                case 6: // hlineto
                case 7: // vlineto
                {
                    bool horizontal = b0 == 6;
                    for (std::size_t index = 0; index < state.stack.size(); ++index)
                    {
                        if (horizontal)
                        {
                            lineTo(state.x + at(index), state.y);
                        }
                        else
                        {
                            lineTo(state.x, state.y + at(index));
                        }
                        horizontal = !horizontal;
                    }
                    state.stack.clear();
                    break;
                }

                case 8: // rrcurveto
                {
                    for (std::size_t index = 0; index + 5 < state.stack.size(); index += 6)
                    {
                        const float c1x = state.x + at(index);
                        const float c1y = state.y + at(index + 1);
                        const float c2x = c1x + at(index + 2);
                        const float c2y = c1y + at(index + 3);
                        curveTo(c1x, c1y, c2x, c2y, c2x + at(index + 4), c2y + at(index + 5));
                    }
                    state.stack.clear();
                    break;
                }

                case 24: // rcurveline
                {
                    std::size_t index = 0;
                    while (index + 7 < state.stack.size())
                    {
                        const float c1x = state.x + at(index);
                        const float c1y = state.y + at(index + 1);
                        const float c2x = c1x + at(index + 2);
                        const float c2y = c1y + at(index + 3);
                        curveTo(c1x, c1y, c2x, c2y, c2x + at(index + 4), c2y + at(index + 5));
                        index += 6;
                    }
                    if (index + 1 < state.stack.size())
                    {
                        lineTo(state.x + at(index), state.y + at(index + 1));
                    }
                    state.stack.clear();
                    break;
                }

                case 25: // rlinecurve
                {
                    std::size_t index = 0;
                    while (index + 7 < state.stack.size())
                    {
                        lineTo(state.x + at(index), state.y + at(index + 1));
                        index += 2;
                    }
                    if (index + 5 < state.stack.size())
                    {
                        const float c1x = state.x + at(index);
                        const float c1y = state.y + at(index + 1);
                        const float c2x = c1x + at(index + 2);
                        const float c2y = c1y + at(index + 3);
                        curveTo(c1x, c1y, c2x, c2y, c2x + at(index + 4), c2y + at(index + 5));
                    }
                    state.stack.clear();
                    break;
                }

                case 26: // vvcurveto
                {
                    std::size_t index = 0;
                    float dx1 = 0.0f;
                    if (state.stack.size() % 4 == 1)
                    {
                        dx1 = at(0);
                        index = 1;
                    }
                    for (; index + 3 < state.stack.size(); index += 4)
                    {
                        const float c1x = state.x + dx1;
                        const float c1y = state.y + at(index);
                        const float c2x = c1x + at(index + 1);
                        const float c2y = c1y + at(index + 2);
                        curveTo(c1x, c1y, c2x, c2y, c2x, c2y + at(index + 3));
                        dx1 = 0.0f;
                    }
                    state.stack.clear();
                    break;
                }

                case 27: // hhcurveto
                {
                    std::size_t index = 0;
                    float dy1 = 0.0f;
                    if (state.stack.size() % 4 == 1)
                    {
                        dy1 = at(0);
                        index = 1;
                    }
                    for (; index + 3 < state.stack.size(); index += 4)
                    {
                        const float c1x = state.x + at(index);
                        const float c1y = state.y + dy1;
                        const float c2x = c1x + at(index + 1);
                        const float c2y = c1y + at(index + 2);
                        curveTo(c1x, c1y, c2x, c2y, c2x + at(index + 3), c2y);
                        dy1 = 0.0f;
                    }
                    state.stack.clear();
                    break;
                }

                case 30: // vhcurveto
                case 31: // hvcurveto
                {
                    bool horizontal = b0 == 31;
                    std::size_t index = 0;
                    while (index + 3 < state.stack.size())
                    {
                        float c1x = 0.0f;
                        float c1y = 0.0f;
                        float c2x = 0.0f;
                        float c2y = 0.0f;
                        float ex = 0.0f;
                        float ey = 0.0f;

                        if (horizontal)
                        {
                            const float dx1 = at(index++);
                            const float dx2 = at(index++);
                            const float dy2 = at(index++);
                            const float dy3 = at(index++);
                            float dx3 = 0.0f;
                            if (index == state.stack.size() - 1)
                            {
                                dx3 = at(index++);
                            }
                            c1x = state.x + dx1;
                            c1y = state.y;
                            c2x = c1x + dx2;
                            c2y = c1y + dy2;
                            ex = c2x + dx3;
                            ey = c2y + dy3;
                        }
                        else
                        {
                            const float dy1 = at(index++);
                            const float dx2 = at(index++);
                            const float dy2 = at(index++);
                            const float dx3 = at(index++);
                            float dy3 = 0.0f;
                            if (index == state.stack.size() - 1)
                            {
                                dy3 = at(index++);
                            }
                            c1x = state.x;
                            c1y = state.y + dy1;
                            c2x = c1x + dx2;
                            c2y = c1y + dy2;
                            ex = c2x + dx3;
                            ey = c2y + dy3;
                        }

                        curveTo(c1x, c1y, c2x, c2y, ex, ey);
                        horizontal = !horizontal;
                    }
                    state.stack.clear();
                    break;
                }

                case 10: // callsubr
                {
                    if (state.stack.empty()) { state.stack.clear(); break; }
                    const int index =
                        static_cast<int>(state.stack.back()) + SubrBias(localSubrs.count);
                    state.stack.pop_back();

                    if (index >= 0 && static_cast<std::uint32_t>(index) < localSubrs.count)
                    {
                        const std::uint32_t start = localSubrs.Start(static_cast<std::uint32_t>(index));
                        const std::uint32_t end = localSubrs.End(static_cast<std::uint32_t>(index));
                        if (end <= size)
                        {
                            RunCharstring(bytes.data() + start, end - start, state, localSubrs, depth + 1);
                        }
                    }
                    break;
                }

                case 29: // callgsubr
                {
                    if (state.stack.empty()) { state.stack.clear(); break; }
                    const int index =
                        static_cast<int>(state.stack.back()) + SubrBias(globalSubrs.count);
                    state.stack.pop_back();

                    if (index >= 0 && static_cast<std::uint32_t>(index) < globalSubrs.count)
                    {
                        const std::uint32_t start = globalSubrs.Start(static_cast<std::uint32_t>(index));
                        const std::uint32_t end = globalSubrs.End(static_cast<std::uint32_t>(index));
                        if (end <= size)
                        {
                            RunCharstring(bytes.data() + start, end - start, state, localSubrs, depth + 1);
                        }
                    }
                    break;
                }

                case 11: // return
                case 14: // endchar
                    return;

                case 12: // escape
                {
                    if (ip >= length) { return; }
                    const std::uint8_t b1 = code[ip++];

                    switch (b1)
                    {
                        case 35: // flex
                        {
                            const float c1x = state.x + at(0);
                            const float c1y = state.y + at(1);
                            const float c2x = c1x + at(2);
                            const float c2y = c1y + at(3);
                            const float ex = c2x + at(4);
                            const float ey = c2y + at(5);
                            curveTo(c1x, c1y, c2x, c2y, ex, ey);

                            const float d1x = state.x + at(6);
                            const float d1y = state.y + at(7);
                            const float d2x = d1x + at(8);
                            const float d2y = d1y + at(9);
                            curveTo(d1x, d1y, d2x, d2y, d2x + at(10), d2y + at(11));
                            state.stack.clear();
                            break;
                        }

                        case 34: // hflex
                        {
                            const float y0 = state.y;
                            const float c1x = state.x + at(0);
                            const float c2x = c1x + at(1);
                            const float c2y = state.y + at(2);
                            const float ex = c2x + at(3);
                            curveTo(c1x, state.y, c2x, c2y, ex, c2y);

                            const float d1x = ex + at(4);
                            const float d2x = d1x + at(5);
                            curveTo(d1x, state.y, d2x, state.y - at(2), state.x + at(6), y0);
                            state.stack.clear();
                            break;
                        }

                        case 36: // hflex1
                        {
                            const float y0 = state.y;
                            const float c1x = state.x + at(0);
                            const float c1y = state.y + at(1);
                            const float c2x = c1x + at(2);
                            const float c2y = c1y + at(3);
                            const float ex = c2x + at(4);
                            curveTo(c1x, c1y, c2x, c2y, ex, c2y);

                            const float d1x = ex + at(5);
                            const float d1y = state.y;
                            const float d2x = d1x + at(6);
                            const float d2y = d1y + at(7);
                            curveTo(d1x, d1y, d2x, d2y, d2x + at(8), y0);
                            state.stack.clear();
                            break;
                        }

                        case 37: // flex1
                        {
                            const float x0 = state.x;
                            const float y0 = state.y;

                            float sumX = 0.0f;
                            float sumY = 0.0f;
                            for (int index = 0; index < 10; ++index)
                            {
                                if (index % 2 == 0) { sumX += at(index); }
                                else { sumY += at(index); }
                            }

                            const float c1x = state.x + at(0);
                            const float c1y = state.y + at(1);
                            const float c2x = c1x + at(2);
                            const float c2y = c1y + at(3);
                            const float ex = c2x + at(4);
                            const float ey = c2y + at(5);
                            curveTo(c1x, c1y, c2x, c2y, ex, ey);

                            const float d1x = state.x + at(6);
                            const float d1y = state.y + at(7);
                            const float d2x = d1x + at(8);
                            const float d2y = d1y + at(9);

                            float fx = 0.0f;
                            float fy = 0.0f;
                            if (std::fabs(sumX) > std::fabs(sumY))
                            {
                                fx = d2x + at(10);
                                fy = y0;
                            }
                            else
                            {
                                fx = x0;
                                fy = d2y + at(10);
                            }
                            curveTo(d1x, d1y, d2x, d2y, fx, fy);
                            state.stack.clear();
                            break;
                        }

                        case 3: // and
                        case 4: // or
                        case 5: // not
                        case 9: // abs
                        case 10: // add
                        case 11: // sub
                        case 12: // div
                        case 14: // neg
                        case 15: // eq
                        case 18: // drop
                        case 20: // put
                        case 21: // get
                        case 22: // ifelse
                        case 24: // mul
                        case 26: // sqrt
                        case 27: // dup
                        case 28: // exch
                        case 29: // index
                        case 30: // roll
                        {
                            const auto pop = [&]() -> float {
                                if (state.stack.empty()) { return 0.0f; }
                                const float value = state.stack.back();
                                state.stack.pop_back();
                                return value;
                            };

                            switch (b1)
                            {
                                case 3: { const float b = pop(); const float a = pop(); state.stack.push_back((a != 0.0f && b != 0.0f) ? 1.0f : 0.0f); break; }
                                case 4: { const float b = pop(); const float a = pop(); state.stack.push_back((a != 0.0f || b != 0.0f) ? 1.0f : 0.0f); break; }
                                case 5: { const float a = pop(); state.stack.push_back(a == 0.0f ? 1.0f : 0.0f); break; }
                                case 9: { state.stack.push_back(std::fabs(pop())); break; }
                                case 10: { const float b = pop(); const float a = pop(); state.stack.push_back(a + b); break; }
                                case 11: { const float b = pop(); const float a = pop(); state.stack.push_back(a - b); break; }
                                case 12: { const float b = pop(); const float a = pop(); state.stack.push_back(b != 0.0f ? a / b : 0.0f); break; }
                                case 14: { state.stack.push_back(-pop()); break; }
                                case 15: { const float b = pop(); const float a = pop(); state.stack.push_back(a == b ? 1.0f : 0.0f); break; }
                                case 18: { pop(); break; }
                                case 20:
                                {
                                    const float index = pop();
                                    const float value = pop();
                                    if (index >= 0.0f && index < 32.0f) { state.transient[static_cast<int>(index)] = value; }
                                    break;
                                }
                                case 21:
                                {
                                    const float index = pop();
                                    state.stack.push_back(index >= 0.0f && index < 32.0f
                                                              ? state.transient[static_cast<int>(index)]
                                                              : 0.0f);
                                    break;
                                }
                                case 22:
                                {
                                    const float v2 = pop();
                                    const float v1 = pop();
                                    const float s2 = pop();
                                    const float s1 = pop();
                                    state.stack.push_back(s1 <= s2 ? v1 : v2);
                                    break;
                                }
                                case 24: { const float b = pop(); const float a = pop(); state.stack.push_back(a * b); break; }
                                case 26: { const float a = pop(); state.stack.push_back(a >= 0.0f ? std::sqrt(a) : 0.0f); break; }
                                case 27: { const float a = state.stack.empty() ? 0.0f : state.stack.back(); state.stack.push_back(a); break; }
                                case 28:
                                {
                                    const float b = pop();
                                    const float a = pop();
                                    state.stack.push_back(b);
                                    state.stack.push_back(a);
                                    break;
                                }
                                case 29:
                                {
                                    const float index = pop();
                                    if (index >= 0.0f && index < static_cast<float>(state.stack.size()))
                                    {
                                        const std::size_t from = state.stack.size() - 1 - static_cast<std::size_t>(index);
                                        state.stack.push_back(state.stack[from]);
                                    }
                                    break;
                                }
                                case 30:
                                {
                                    const float shift = pop();
                                    const float count = pop();
                                    const int n = static_cast<int>(count);
                                    const int j = static_cast<int>(shift);
                                    if (n > 0 && n <= static_cast<int>(state.stack.size()))
                                    {
                                        std::vector<float> chunk(state.stack.end() - n, state.stack.end());
                                        for (int i = 0; i < n; ++i)
                                        {
                                            int source = (i - j) % n;
                                            if (source < 0) { source += n; }
                                            state.stack[state.stack.size() - n + i] = chunk[static_cast<std::size_t>(source)];
                                        }
                                    }
                                    break;
                                }
                                default: break;
                            }
                            break;
                        }

                        case 16: // CFF2 blend: keep the default values only
                        {
                            if (state.stack.empty()) { break; }
                            const int blends = static_cast<int>(state.stack.back());
                            state.stack.pop_back();
                            if (blends <= 0 || static_cast<int>(state.stack.size()) < blends)
                            {
                                break;
                            }

                            const int total = static_cast<int>(state.stack.size());
                            const int perBlend = total / blends;
                            if (perBlend <= 0)
                            {
                                break;
                            }

                            std::vector<float> defaults;
                            defaults.reserve(static_cast<std::size_t>(blends));
                            const int base = total - blends * perBlend;
                            for (int i = 0; i < blends; ++i)
                            {
                                defaults.push_back(state.stack[base + i * perBlend]);
                            }
                            state.stack.assign(defaults.begin(), defaults.end());
                            break;
                        }

                        case 15: // CFF2 vsindex
                            if (!state.stack.empty()) { state.stack.pop_back(); }
                            break;

                        default: break;
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }
}
