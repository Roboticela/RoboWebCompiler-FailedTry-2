#include "RoboRenderer/Text.h"

#include <algorithm>
#include <cmath>

namespace RoboRenderer
{
    namespace
    {
        bool IsSpace(std::uint32_t codepoint)
        {
            return codepoint == ' ' || codepoint == '\t' || codepoint == '\r' || codepoint == '\f' ||
                   codepoint == 0x00A0;
        }

        /// A break opportunity after a hyphen or dash, which real line breaking
        /// allows even without a space.
        bool IsBreakAfter(std::uint32_t codepoint)
        {
            return codepoint == '-' || codepoint == 0x2010 || codepoint == 0x2013;
        }

        float Clamp01(float value)
        {
            return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        }
    }

    std::uint32_t TextLayout::DecodeUtf8(const char* text, std::uint32_t& offset)
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
            // Invalid lead byte: consume it as a replacement character rather
            // than losing the offset.
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

    std::uint32_t TextLayout::CodepointCount(const char* text)
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

    std::uint32_t TextLayout::ByteOffsetForCodepoint(const char* text, std::uint32_t codepointIndex)
    {
        if (text == nullptr)
        {
            return 0;
        }

        std::uint32_t offset = 0;
        std::uint32_t index = 0;

        while (text[offset] != '\0' && index < codepointIndex)
        {
            DecodeUtf8(text, offset);
            ++index;
        }

        return offset;
    }

    std::uint32_t TextLayout::CodepointIndexForByte(const char* text, std::uint32_t byteOffset)
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

    TextLayout::TextLayout(const FontRegistry& fontRegistry)
        : registry(fontRegistry)
    {
    }

    float TextLayout::Measure(const char* text, float fontSize, FontWeightKind weight) const
    {
        if (text == nullptr || text[0] == '\0')
        {
            return 0.0f;
        }

        const FontFace& face = registry.Face(weight);
        const float scale = fontSize / face.UnitsPerEm();

        float width = 0.0f;
        std::uint32_t offset = 0;

        while (text[offset] != '\0')
        {
            const std::uint32_t codepoint = DecodeUtf8(text, offset);

            if (codepoint == '\n')
            {
                // A hard break contributes no advance; the caller splits lines.
                continue;
            }

            const Glyph* glyph = face.FindGlyph(codepoint);
            if (glyph == nullptr)
            {
                glyph = &face.MissingGlyph();
            }

            width += glyph->advance * scale;
        }

        return width;
    }

    float TextLayout::MeasurePrefix(const char* text,
                                    std::uint32_t byteCount,
                                    float fontSize,
                                    FontWeightKind weight) const
    {
        if (text == nullptr || byteCount == 0)
        {
            return 0.0f;
        }

        const FontFace& face = registry.Face(weight);
        const float scale = fontSize / face.UnitsPerEm();

        float width = 0.0f;
        std::uint32_t offset = 0;

        while (text[offset] != '\0' && offset < byteCount)
        {
            const std::uint32_t codepoint = DecodeUtf8(text, offset);
            if (codepoint == '\n')
            {
                continue;
            }

            const Glyph* glyph = face.FindGlyph(codepoint);
            if (glyph == nullptr)
            {
                glyph = &face.MissingGlyph();
            }

            width += glyph->advance * scale;
        }

        return width;
    }

    float TextLayout::LineHeight(float fontSize, FontWeightKind weight) const
    {
        const FontFace& face = registry.Face(weight);
        const float scale = fontSize / face.UnitsPerEm();
        return (face.Ascent() + face.Descent() + face.LineGap()) * scale;
    }

    float TextLayout::Advance(std::uint32_t codepoint, float fontSize, FontWeightKind weight) const
    {
        const FontFace& face = registry.Face(weight);
        const Glyph* glyph = face.FindGlyph(codepoint);
        if (glyph == nullptr)
        {
            glyph = &face.MissingGlyph();
        }

        return glyph->advance * (fontSize / face.UnitsPerEm());
    }

    void TextLayout::Wrap(const char* text,
                          float fontSize,
                          FontWeightKind weight,
                          float availableWidth,
                          bool preserveWhitespace,
                          std::vector<TextLine>& lines) const
    {
        lines.clear();

        if (text == nullptr)
        {
            return;
        }

        const FontFace& face = registry.Face(weight);
        const float scale = fontSize / face.UnitsPerEm();
        const float lineHeight = LineHeight(fontSize, weight);

        const auto advanceOf = [&face, scale](std::uint32_t codepoint) {
            const Glyph* glyph = face.FindGlyph(codepoint);
            if (glyph == nullptr)
            {
                glyph = &face.MissingGlyph();
            }
            return glyph->advance * scale;
        };

        std::uint32_t length = 0;
        while (text[length] != '\0')
        {
            ++length;
        }

        if (length == 0)
        {
            // An empty run still occupies one line box: an empty element with a
            // font size is a line tall.
            TextLine line;
            line.startByte = 0;
            line.endByte = 0;
            line.width = 0.0f;
            line.height = lineHeight;
            lines.push_back(line);
            return;
        }

        const bool wrap = !preserveWhitespace && availableWidth > 0.0f;

        std::uint32_t offset = 0;
        std::uint32_t lineStart = 0;
        std::uint32_t breakByte = 0;
        bool hasBreak = false;
        float lineWidth = 0.0f;
        float widthAtBreak = 0.0f;

        const auto emit = [&](std::uint32_t end, bool hard) {
            TextLine line;
            line.startByte = lineStart;
            line.endByte = end;
            line.width = lineWidth;
            line.height = lineHeight;
            line.hardBreak = hard;
            lines.push_back(line);
        };

        while (offset < length)
        {
            const std::uint32_t codepointStart = offset;
            const std::uint32_t codepoint = DecodeUtf8(text, offset);

            if (codepoint == '\n')
            {
                emit(codepointStart, true);
                lineStart = offset;
                lineWidth = 0.0f;
                hasBreak = false;
                continue;
            }

            if (codepoint == '\r')
            {
                continue;
            }

            if (codepoint == '\t')
            {
                // A tab advances to the next four-space stop. Tab stops are the
                // one piece of the white-space model the HTML-only phase keeps
                // simple; <pre> therefore aligns on a four unit grid.
                lineWidth += advanceOf(' ') * 4.0f;
                continue;
            }

            if (!preserveWhitespace && IsSpace(codepoint))
            {
                // Whitespace is a break opportunity; it is not measured at the
                // end of a line, which is what makes alignment exact.
                if (lineWidth > 0.0f)
                {
                    breakByte = codepointStart;
                    widthAtBreak = lineWidth;
                    hasBreak = true;
                    lineWidth += advanceOf(' ');
                }
                continue;
            }

            const float advance = advanceOf(codepoint);
            const bool overflows = wrap && (lineWidth + advance) > availableWidth && lineWidth > 0.0f;

            if (overflows)
            {
                if (hasBreak)
                {
                    // Break at the last opportunity and rewind to it: the
                    // whitespace that ended the line is not part of its width.
                    lineWidth = widthAtBreak;
                    emit(breakByte, false);
                    offset = breakByte;
                    // Skip the whitespace the break consumed.
                    while (offset < length && IsSpace(static_cast<unsigned char>(text[offset])))
                    {
                        ++offset;
                    }
                    lineStart = offset;
                    lineWidth = 0.0f;
                    hasBreak = false;
                    continue;
                }

                // A single word too long for the line: break inside it rather
                // than overflowing the container.
                emit(codepointStart, false);
                lineStart = codepointStart;
                lineWidth = advance;
            }
            else
            {
                lineWidth += advance;
            }

            if (!preserveWhitespace && IsBreakAfter(codepoint))
            {
                breakByte = offset;
                widthAtBreak = lineWidth;
                hasBreak = true;
            }
        }

        if (lineStart < length || lines.empty())
        {
            emit(length, false);
        }
    }

    // -----------------------------------------------------------------------
    // Glyph cache
    // -----------------------------------------------------------------------

    GlyphCache::GlyphCache(const FontRegistry& fontRegistry)
        : fonts(fontRegistry)
    {
    }

    void GlyphCache::Clear()
    {
        buckets.clear();
        rasterizedGlyphs = 0;
        coverageBytes = 0;
    }

    GlyphCache::SizeBucket& GlyphCache::BucketFor(float fontSize, FontWeightKind weight, float deviceScale)
    {
        // The device font size is what strokes are rasterized at, quantised to a
        // quarter pixel so a slowly animating size does not explode the cache.
        const float deviceSize = fontSize * deviceScale;
        const std::uint32_t sizeKey = static_cast<std::uint32_t>(deviceSize * 4.0f + 0.5f);

        for (SizeBucket& bucket : buckets)
        {
            if (bucket.sizeKey == sizeKey && bucket.weight == weight)
            {
                return bucket;
            }
        }

        SizeBucket bucket;
        bucket.sizeKey = sizeKey;
        bucket.weight = weight;
        bucket.glyphs.assign(StrokeFontFace::GlyphCount(), GlyphBitmap{});
        bucket.glyphState.assign(StrokeFontFace::GlyphCount(), 0);

        buckets.push_back(std::move(bucket));
        return buckets.back();
    }

    void GlyphCache::Rasterize(SizeBucket& bucket, std::uint32_t codepoint)
    {
        if (bucket.glyphs.size() != StrokeFontFace::GlyphCount())
        {
            bucket.glyphs.assign(StrokeFontFace::GlyphCount(), GlyphBitmap{});
            bucket.glyphState.assign(StrokeFontFace::GlyphCount(), 0);
        }

        const FontFace& face = fonts.Face(bucket.weight);
        const float scale = (static_cast<float>(bucket.sizeKey) * 0.25f) / face.UnitsPerEm();

        const Glyph* glyph = face.FindGlyph(codepoint);
        if (glyph == nullptr)
        {
            glyph = &face.MissingGlyph();
        }

        GlyphBitmap bitmap;
        bitmap.advance = glyph->advance * scale;

        // Stroke width follows the font weight, which is what makes bold text
        // read as bold rather than as the same strokes moved sideways.
        const float strokeWidth = std::max(0.75f, (bucket.weight == FontWeightKind::Bold ? 0.115f : 0.085f) *
                                                       static_cast<float>(bucket.sizeKey) * 0.25f);
        const float radius = strokeWidth * 0.5f;
        const float pad = radius + 1.0f;

        if (glyph->hasInk)
        {
            const float left = glyph->x0 * scale - pad;
            const float right = glyph->x1 * scale + pad;
            const float top = glyph->y1 * scale + pad;
            const float bottom = glyph->y0 * scale - pad;

            const int originX = static_cast<int>(std::floor(left));
            const int originY = static_cast<int>(std::floor(-top));
            const int width = static_cast<int>(std::ceil(right)) - originX;
            const int height = static_cast<int>(std::ceil(-bottom)) - originY;

            if (width > 0 && height > 0 && width < 4096 && height < 4096)
            {
                bitmap.width = static_cast<std::uint32_t>(width);
                bitmap.height = static_cast<std::uint32_t>(height);
                bitmap.bearingX = static_cast<float>(originX);
                bitmap.bearingY = static_cast<float>(originY);
                bitmap.blank = false;
                bitmap.firstCoverage = static_cast<std::uint32_t>(bucket.coverage.size());

                const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
                const std::size_t coverageStart = bucket.coverage.size();
                bucket.coverage.resize(coverageStart + pixelCount, 0);
                coverageBytes += pixelCount;

                const StrokePoint* points = face.Points();

                float penX = 0.0f;
                float penY = 0.0f;
                bool havePen = false;

                for (std::uint32_t index = 0; index < glyph->pointCount; ++index)
                {
                    const StrokePoint& point = points[glyph->firstPoint + index];
                    if (point.x < 0.0f)
                    {
                        havePen = false;
                        continue;
                    }

                    // Device coordinates: y grows downwards on the bitmap.
                    const float deviceX = point.x * scale - static_cast<float>(originX);
                    const float deviceY = -point.y * scale - static_cast<float>(originY);

                    if (!havePen)
                    {
                        penX = deviceX;
                        penY = deviceY;
                        havePen = true;

                        // A single point is a dot: the period, the dot of an i.
                        const float dotRadius = radius;
                        const int minX = std::max(0, static_cast<int>(penX - dotRadius - 1.0f));
                        const int maxX = std::min(width - 1, static_cast<int>(penX + dotRadius + 1.0f));
                        const int minY = std::max(0, static_cast<int>(penY - dotRadius - 1.0f));
                        const int maxY = std::min(height - 1, static_cast<int>(penY + dotRadius + 1.0f));

                        for (int y = minY; y <= maxY; ++y)
                        {
                            for (int x = minX; x <= maxX; ++x)
                            {
                                const float dx = static_cast<float>(x) + 0.5f - penX;
                                const float dy = static_cast<float>(y) + 0.5f - penY;
                                const float distance = std::sqrt(dx * dx + dy * dy);
                                const float coverage = Clamp01(radius + 0.5f - distance);
                                if (coverage <= 0.0f)
                                {
                                    continue;
                                }

                                std::uint8_t& target =
                                    bucket.coverage[coverageStart + static_cast<std::size_t>(y) *
                                                                        static_cast<std::size_t>(width) +
                                                                    static_cast<std::size_t>(x)];
                                const std::uint8_t value = static_cast<std::uint8_t>(coverage * 255.0f + 0.5f);
                                target = value > target ? value : target;
                            }
                        }

                        continue;
                    }

                    // Stroke segment from the pen to this point.
                    const float ax = penX;
                    const float ay = penY;
                    const float bx = deviceX;
                    const float by = deviceY;

                    const float minXf = std::min(ax, bx) - radius - 1.0f;
                    const float maxXf = std::max(ax, bx) + radius + 1.0f;
                    const float minYf = std::min(ay, by) - radius - 1.0f;
                    const float maxYf = std::max(ay, by) + radius + 1.0f;

                    const int minX = std::max(0, static_cast<int>(std::floor(minXf)));
                    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(maxXf)));
                    const int minY = std::max(0, static_cast<int>(std::floor(minYf)));
                    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(maxYf)));

                    const float segmentX = bx - ax;
                    const float segmentY = by - ay;
                    const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;

                    for (int y = minY; y <= maxY; ++y)
                    {
                        for (int x = minX; x <= maxX; ++x)
                        {
                            const float px = static_cast<float>(x) + 0.5f;
                            const float py = static_cast<float>(y) + 0.5f;

                            float parameter = 0.0f;
                            if (segmentLengthSquared > 0.0f)
                            {
                                parameter = ((px - ax) * segmentX + (py - ay) * segmentY) / segmentLengthSquared;
                                parameter = parameter < 0.0f ? 0.0f : (parameter > 1.0f ? 1.0f : parameter);
                            }

                            const float closestX = ax + segmentX * parameter;
                            const float closestY = ay + segmentY * parameter;
                            const float dx = px - closestX;
                            const float dy = py - closestY;
                            const float distance = std::sqrt(dx * dx + dy * dy);
                            const float coverage = Clamp01(radius + 0.5f - distance);

                            if (coverage <= 0.0f)
                            {
                                continue;
                            }

                            std::uint8_t& target =
                                bucket.coverage[coverageStart + static_cast<std::size_t>(y) *
                                                                    static_cast<std::size_t>(width) +
                                                                static_cast<std::size_t>(x)];
                            const std::uint8_t value = static_cast<std::uint8_t>(coverage * 255.0f + 0.5f);
                            target = value > target ? value : target;
                        }
                    }

                    penX = deviceX;
                    penY = deviceY;
                }
            }
        }

        if (codepoint >= 32 && codepoint <= 126)
        {
            const std::uint32_t slot = codepoint - 32;
            bucket.glyphs[slot] = bitmap;
            bucket.glyphState[slot] = 1;
        }

        ++rasterizedGlyphs;
    }

    RasterizedGlyph GlyphCache::Get(std::uint32_t codepoint, float fontSize, FontWeightKind weight,
                                    float deviceScale)
    {
        RasterizedGlyph result;

        if (fontSize <= 0.0f || codepoint < 32 || codepoint > 126)
        {
            return result;
        }

        SizeBucket& bucket = BucketFor(fontSize, weight, deviceScale);
        const std::uint32_t slot = codepoint - 32;

        if (slot >= bucket.glyphState.size())
        {
            return result;
        }

        // A glyph is rasterized the first time it is drawn, so a page that never
        // uses 'Q' never pays for it.
        if (bucket.glyphState[slot] == 0)
        {
            Rasterize(bucket, codepoint);
        }

        const GlyphBitmap& bitmap = bucket.glyphs[slot];
        result.bitmap = &bitmap;
        result.coverage = bitmap.blank ? nullptr : bucket.coverage.data() + bitmap.firstCoverage;
        return result;
    }

    std::uint32_t TextLayout::HitTestOffset(const char* text,
                                           float localX,
                                           float fontSize,
                                           FontWeightKind weight) const
    {
        if (text == nullptr)
        {
            return 0;
        }

        const FontFace& face = registry.Face(weight);
        const float scale = fontSize / face.UnitsPerEm();

        float x = 0.0f;
        std::uint32_t offset = 0;
        std::uint32_t best = 0;
        float bestDistance = std::fabs(localX);

        while (text[offset] != '\0')
        {
            const std::uint32_t codepointStart = offset;
            const std::uint32_t codepoint = DecodeUtf8(text, offset);

            const float distance = std::fabs(localX - x);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = codepointStart;
            }

            const Glyph* glyph = face.FindGlyph(codepoint);
            if (glyph == nullptr)
            {
                glyph = &face.MissingGlyph();
            }

            x += glyph->advance * scale;

            const float endDistance = std::fabs(localX - x);
            if (endDistance < bestDistance)
            {
                bestDistance = endDistance;
                best = offset;
            }
        }

        return best;
    }
}
