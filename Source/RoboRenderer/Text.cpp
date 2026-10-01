#include "RoboRenderer/Text.h"
#include "RoboRenderer/Bidi.h"
#include "RoboRenderer/Image.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

        struct Vec2
        {
            float x = 0.0f;
            float y = 0.0f;
        };

        /// Flattens one TrueType contour into a closed polygon of line segments.
        ///
        /// Quadratic segments are subdivided, and consecutive off-curve points get
        /// the implied on-curve midpoint the format omits, which is exactly the
        /// rule `glyf` uses. The subdivision count tracks the segment's length so
        /// a bowl curves smoothly while a tiny tick stays cheap.
        void FlattenContour(const ContourPoint* points,
                            std::uint32_t begin,
                            std::uint32_t end,
                            std::vector<Vec2>& out)
        {
            const std::uint32_t count = end - begin;
            if (count == 0)
            {
                return;
            }

            const auto at = [&](std::uint32_t index) -> const ContourPoint& {
                return points[begin + (index % count)];
            };

            std::vector<ContourPoint> sequence;
            sequence.reserve(count + 2);

            if (at(0).onCurve)
            {
                for (std::uint32_t index = 0; index < count; ++index)
                {
                    sequence.push_back(at(index));
                }
            }
            else
            {
                ContourPoint midpoint;
                midpoint.x = (at(count - 1).x + at(0).x) * 0.5f;
                midpoint.y = (at(count - 1).y + at(0).y) * 0.5f;
                midpoint.onCurve = true;
                sequence.push_back(midpoint);

                for (std::uint32_t index = 0; index < count; ++index)
                {
                    sequence.push_back(at(index));
                }
            }

            std::vector<ContourPoint> normalized;
            normalized.reserve(sequence.size() + 4);

            for (std::size_t index = 0; index < sequence.size(); ++index)
            {
                normalized.push_back(sequence[index]);

                const ContourPoint& next = sequence[(index + 1) % sequence.size()];
                if (!sequence[index].onCurve && !next.onCurve)
                {
                    ContourPoint midpoint;
                    midpoint.x = (sequence[index].x + next.x) * 0.5f;
                    midpoint.y = (sequence[index].y + next.y) * 0.5f;
                    midpoint.onCurve = true;
                    normalized.push_back(midpoint);
                }
            }

            if (!normalized.back().onCurve)
            {
                normalized.push_back(normalized.front());
            }

            const auto emitQuadratic = [&out](const Vec2& from, const Vec2& control, const Vec2& to) {
                const float dx0 = control.x - from.x;
                const float dy0 = control.y - from.y;
                const float dx1 = to.x - control.x;
                const float dy1 = to.y - control.y;
                const float approximate = std::sqrt(dx0 * dx0 + dy0 * dy0) + std::sqrt(dx1 * dx1 + dy1 * dy1);

                int steps = static_cast<int>(approximate * 0.5f) + 4;
                if (steps > 24)
                {
                    steps = 24;
                }

                for (int step = 1; step < steps; ++step)
                {
                    const float t = static_cast<float>(step) / static_cast<float>(steps);
                    const float u = 1.0f - t;
                    Vec2 point;
                    point.x = u * u * from.x + 2.0f * u * t * control.x + t * t * to.x;
                    point.y = u * u * from.y + 2.0f * u * t * control.y + t * t * to.y;
                    out.push_back(point);
                }

                out.push_back(to);
            };

            Vec2 current{ normalized.front().x, normalized.front().y };
            out.push_back(current);

            for (std::size_t index = 1; index < normalized.size(); ++index)
            {
                const ContourPoint& point = normalized[index];

                if (point.onCurve)
                {
                    current = Vec2{ point.x, point.y };
                    out.push_back(current);
                    continue;
                }

                // Off-curve: the normalized sequence guarantees a following
                // on-curve point, because consecutive offs were split.
                const ContourPoint& to = normalized[index + 1];
                emitQuadratic(current, Vec2{ point.x, point.y }, Vec2{ to.x, to.y });
                current = Vec2{ to.x, to.y };
                ++index;
            }
        }

        /// A glyph's contours in bitmap space (device pixels, y downwards from the
        /// bitmap's top-left corner).
        void FlattenGlyph(const FontFace& face,
                          const Glyph& glyph,
                          float scale,
                          float originX,
                          float originY,
                          std::vector<std::vector<Vec2>>& polygons)
        {
            const ContourPoint* points = face.ContourPoints();
            const std::uint32_t* ends = face.ContourEnds();

            if (points == nullptr || ends == nullptr || glyph.contourCount == 0)
            {
                return;
            }

            for (std::uint32_t contour = 0; contour < glyph.contourCount; ++contour)
            {
                const std::uint32_t index = glyph.firstContour + contour;
                const std::uint32_t begin = index == 0 ? 0u : ends[index - 1];
                const std::uint32_t end = ends[index];

                if (end <= begin)
                {
                    continue;
                }

                std::vector<Vec2> raw;
                FlattenContour(points, begin, end, raw);

                if (raw.size() < 3)
                {
                    continue;
                }

                std::vector<Vec2> transformed;
                transformed.reserve(raw.size());

                for (const Vec2& point : raw)
                {
                    Vec2 device;
                    device.x = point.x * scale - originX;
                    device.y = -point.y * scale - originY;
                    transformed.push_back(device);
                }

                polygons.push_back(std::move(transformed));
            }
        }

        /// Fills closed contours with the non-zero winding rule, sampling each
        /// pixel on a 4x4 grid. TrueType contours are wound so that a hole runs
        /// the other way, which is exactly what the rule keys on.
        void FillPolygons(const std::vector<std::vector<Vec2>>& polygons,
                          std::uint8_t* coverage,
                          int width,
                          int height)
        {
            constexpr int SubSamplesY = 4;
            constexpr int SubSamplesX = 4;
            constexpr int MaxCoverage = SubSamplesY * SubSamplesX;

            std::vector<float> crossings;
            std::vector<int> directions;
            std::vector<int> accumulator(static_cast<std::size_t>(width), 0);

            for (int row = 0; row < height; ++row)
            {
                std::fill(accumulator.begin(), accumulator.end(), 0);

                for (int sub = 0; sub < SubSamplesY; ++sub)
                {
                    const float sampleY =
                        static_cast<float>(row) + (static_cast<float>(sub) + 0.5f) / SubSamplesY;

                    crossings.clear();
                    directions.clear();

                    for (const std::vector<Vec2>& polygon : polygons)
                    {
                        const std::size_t count = polygon.size();

                        for (std::size_t index = 0; index < count; ++index)
                        {
                            const Vec2& a = polygon[index];
                            const Vec2& b = polygon[(index + 1) % count];

                            const bool aBelow = a.y <= sampleY;
                            const bool bBelow = b.y <= sampleY;
                            if (aBelow == bBelow)
                            {
                                continue;
                            }

                            const float t = (sampleY - a.y) / (b.y - a.y);
                            crossings.push_back(a.x + t * (b.x - a.x));
                            directions.push_back(b.y > a.y ? 1 : -1);
                        }
                    }

                    // Crossing counts are tiny, so an insertion sort beats the
                    // overhead of anything cleverer here.
                    for (std::size_t index = 1; index < crossings.size(); ++index)
                    {
                        const float x = crossings[index];
                        const int direction = directions[index];
                        std::size_t position = index;

                        while (position > 0 && crossings[position - 1] > x)
                        {
                            crossings[position] = crossings[position - 1];
                            directions[position] = directions[position - 1];
                            --position;
                        }

                        crossings[position] = x;
                        directions[position] = direction;
                    }

                    int winding = 0;
                    for (std::size_t index = 0; index < crossings.size(); ++index)
                    {
                        if (winding != 0 && index > 0)
                        {
                            const float from = crossings[index - 1];
                            const float to = crossings[index];

                            if (to > from)
                            {
                                const int firstPixel = std::max(0, static_cast<int>(std::floor(from)));
                                const int lastPixel = std::min(width - 1, static_cast<int>(std::ceil(to)) - 1);

                                for (int pixel = firstPixel; pixel <= lastPixel; ++pixel)
                                {
                                    for (int sample = 0; sample < SubSamplesX; ++sample)
                                    {
                                        const float sampleX =
                                            static_cast<float>(pixel) +
                                            (static_cast<float>(sample) + 0.5f) / SubSamplesX;
                                        if (sampleX >= from && sampleX < to)
                                        {
                                            accumulator[static_cast<std::size_t>(pixel)] += 1;
                                        }
                                    }
                                }
                            }
                        }

                        winding += directions[index];
                    }
                }

                std::uint8_t* rowOutput = coverage + static_cast<std::size_t>(row) * static_cast<std::size_t>(width);

                for (int x = 0; x < width; ++x)
                {
                    const int value = accumulator[static_cast<std::size_t>(x)];
                    rowOutput[x] = static_cast<std::uint8_t>(value * 255 / MaxCoverage);
                }
            }
        }

        /// Thickens coverage by `radius` device pixels: the synthetic bold of a
        /// face that ships one weight. Separable, so it costs two passes rather
        /// than a disc kernel.
        void DilateCoverage(std::uint8_t* coverage, int width, int height, float radius)
        {
            const int reach = static_cast<int>(std::ceil(radius * 0.5f));
            if (reach <= 0 || width <= 0 || height <= 0)
            {
                return;
            }

            const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            std::vector<std::uint8_t> source(coverage, coverage + pixelCount);

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    std::uint8_t best = 0;
                    for (int offset = -reach; offset <= reach; ++offset)
                    {
                        const int sample = x + offset;
                        if (sample < 0 || sample >= width)
                        {
                            continue;
                        }
                        best = std::max(best, source[static_cast<std::size_t>(y) * width + sample]);
                    }
                    coverage[static_cast<std::size_t>(y) * width + x] = best;
                }
            }

            std::copy(coverage, coverage + pixelCount, source.begin());

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    std::uint8_t best = 0;
                    for (int offset = -reach; offset <= reach; ++offset)
                    {
                        const int sample = y + offset;
                        if (sample < 0 || sample >= height)
                        {
                            continue;
                        }
                        best = std::max(best, source[static_cast<std::size_t>(sample) * width + x]);
                    }
                    coverage[static_cast<std::size_t>(y) * width + x] = best;
                }
            }
        }

        /// The built-in stroke face's rasterizer: strokes and dots, with a width
        /// that follows the weight.
        void RasterizeStrokes(const FontFace& face,
                              const Glyph& glyph,
                              float scale,
                              int originX,
                              int originY,
                              int width,
                              int height,
                              FontWeightKind weight,
                              float deviceSize,
                              std::uint8_t* coverage)
        {
            const float strokeWidth =
                std::max(0.75f, (weight == FontWeightKind::Bold ? 0.115f : 0.085f) * deviceSize);
            const float radius = strokeWidth * 0.5f;

            const StrokePoint* points = face.Points();
            if (points == nullptr)
            {
                return;
            }

            float penX = 0.0f;
            float penY = 0.0f;
            bool havePen = false;

            for (std::uint32_t index = 0; index < glyph.pointCount; ++index)
            {
                const StrokePoint& point = points[glyph.firstPoint + index];

                if (point.x < 0.0f)
                {
                    havePen = false;
                    continue;
                }

                const float deviceX = point.x * scale - static_cast<float>(originX);
                const float deviceY = -point.y * scale - static_cast<float>(originY);

                if (!havePen)
                {
                    penX = deviceX;
                    penY = deviceY;
                    havePen = true;

                    const int minX = std::max(0, static_cast<int>(penX - radius - 1.0f));
                    const int maxX = std::min(width - 1, static_cast<int>(penX + radius + 1.0f));
                    const int minY = std::max(0, static_cast<int>(penY - radius - 1.0f));
                    const int maxY = std::min(height - 1, static_cast<int>(penY + radius + 1.0f));

                    for (int y = minY; y <= maxY; ++y)
                    {
                        for (int x = minX; x <= maxX; ++x)
                        {
                            const float dx = static_cast<float>(x) + 0.5f - penX;
                            const float dy = static_cast<float>(y) + 0.5f - penY;
                            const float distance = std::sqrt(dx * dx + dy * dy);
                            const float value = Clamp01(radius + 0.5f - distance);
                            if (value <= 0.0f)
                            {
                                continue;
                            }

                            std::uint8_t& target =
                                coverage[static_cast<std::size_t>(y) * width + x];
                            const std::uint8_t byte = static_cast<std::uint8_t>(value * 255.0f + 0.5f);
                            target = byte > target ? byte : target;
                        }
                    }

                    continue;
                }

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
                        const float value = Clamp01(radius + 0.5f - distance);

                        if (value <= 0.0f)
                        {
                            continue;
                        }

                        std::uint8_t& target = coverage[static_cast<std::size_t>(y) * width + x];
                        const std::uint8_t byte = static_cast<std::uint8_t>(value * 255.0f + 0.5f);
                        target = byte > target ? byte : target;
                    }
                }

                penX = deviceX;
                penY = deviceY;
            }
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

    void TextLayout::ShapeRun(const char* text,
                              std::uint32_t startByte,
                              std::uint32_t endByte,
                              float fontSize,
                              FontWeightKind weight,
                              std::vector<PositionedGlyph>& out) const
    {
        out.clear();

        if (text == nullptr || fontSize <= 0.0f || startByte >= endByte)
        {
            return;
        }

        // Decode the run once, remembering each code point's byte offset and
        // which face in the fallback chain covers it. Runs are grouped by face
        // so a mixed-script string is shaped by each face in turn.
        std::vector<std::uint32_t> codepoints;
        std::vector<std::uint32_t> byteOffsets;
        std::vector<const FontFace*> faces;

        std::uint32_t offset = startByte;
        while (text[offset] != '\0' && offset < endByte)
        {
            const std::uint32_t start = offset;
            const std::uint32_t codepoint = DecodeUtf8(text, offset);

            // Line breaks carry no advance; the caller has already split lines.
            if (codepoint == '\r' || codepoint == '\n')
            {
                continue;
            }

            codepoints.push_back(codepoint);
            byteOffsets.push_back(start);
            faces.push_back(&registry.FaceFor(codepoint, weight));
        }

        const std::size_t count = codepoints.size();
        std::size_t index = 0;

        while (index < count)
        {
            const FontFace* face = faces[index];
            const std::uint32_t faceIndex = registry.IndexOf(*face);

            // A tab is a fixed four-space stop, not a shaped glyph.
            if (codepoints[index] == '\t')
            {
                PositionedGlyph glyph;
                glyph.face = face;
                glyph.faceIndex = faceIndex;
                glyph.codepoint = '\t';
                glyph.byteStart = byteOffsets[index];
                glyph.advance = TabAdvance(fontSize, weight);
                out.push_back(glyph);
                ++index;
                continue;
            }

            // Extend the run while the same face keeps covering it.
            std::size_t runEnd = index;
            while (runEnd < count && faces[runEnd] == face && codepoints[runEnd] != '\t')
            {
                ++runEnd;
            }

            const float scale = fontSize / (face->UnitsPerEm() > 0.0f ? face->UnitsPerEm() : 1000.0f);

            std::vector<ShapedGlyph> shaped;
            if (face->ShapeRun(codepoints.data() + index, runEnd - index, shaped) && !shaped.empty())
            {
                for (const ShapedGlyph& glyph : shaped)
                {
                    const std::size_t local = std::min<std::size_t>(
                        static_cast<std::size_t>(glyph.cluster), runEnd - index - 1);
                    const std::size_t source = index + local;

                    PositionedGlyph positioned;
                    positioned.face = face;
                    positioned.faceIndex = faceIndex;
                    positioned.glyphId = glyph.glyph;
                    positioned.codepoint = codepoints[source];
                    positioned.byteStart = byteOffsets[source];
                    positioned.advance = glyph.xAdvance * scale;
                    positioned.xOffset = glyph.xOffset * scale;
                    positioned.yOffset = glyph.yOffset * scale;
                    positioned.mark = glyph.mark;
                    out.push_back(positioned);
                }
            }
            else
            {
                // No layout tables, or the face cannot shape: one glyph per
                // code point, which is exactly what the font's `cmap` says.
                for (std::size_t position = index; position < runEnd; ++position)
                {
                    const Glyph* glyph = face->FindGlyph(codepoints[position]);
                    if (glyph == nullptr)
                    {
                        glyph = &face->MissingGlyph();
                    }

                    PositionedGlyph positioned;
                    positioned.face = face;
                    positioned.faceIndex = faceIndex;
                    positioned.glyphId = glyph->glyphId;
                    positioned.codepoint = codepoints[position];
                    positioned.byteStart = byteOffsets[position];
                    positioned.advance = glyph->advance * scale;
                    out.push_back(positioned);
                }
            }

            index = runEnd;
        }
    }

    float TextLayout::Measure(const char* text, float fontSize, FontWeightKind weight) const
    {
        if (text == nullptr || text[0] == '\0')
        {
            return 0.0f;
        }

        std::uint32_t length = 0;
        while (text[length] != '\0')
        {
            ++length;
        }

        std::vector<PositionedGlyph> glyphs;
        ShapeRun(text, 0, length, fontSize, weight, glyphs);

        float width = 0.0f;
        for (const PositionedGlyph& glyph : glyphs)
        {
            width += glyph.advance;
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

        std::uint32_t length = 0;
        while (text[length] != '\0')
        {
            ++length;
        }

        if (byteCount > length)
        {
            byteCount = length;
        }

        std::vector<PositionedGlyph> glyphs;
        ShapeRun(text, 0, length, fontSize, weight, glyphs);

        float width = 0.0f;
        for (const PositionedGlyph& glyph : glyphs)
        {
            // A glyph belongs to the prefix when it starts inside it. A ligature
            // whose last code point straddles the edge counts whole, which keeps
            // the arithmetic monotonic.
            if (glyph.byteStart < byteCount)
            {
                width += glyph.advance;
            }
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
        const FontFace& face = registry.FaceFor(codepoint, weight);
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

        const float lineHeight = LineHeight(fontSize, weight);

        const auto advanceOf = [this, fontSize, weight](std::uint32_t codepoint) {
            const FontFace& face = registry.FaceFor(codepoint, weight);
            const Glyph* glyph = face.FindGlyph(codepoint);
            if (glyph == nullptr)
            {
                glyph = &face.MissingGlyph();
            }
            return glyph->advance * (fontSize / face.UnitsPerEm());
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
        std::uint32_t previousCodepoint = 0;
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
            const std::uint32_t previous = previousCodepoint;
            previousCodepoint = codepoint;

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
                // A tab advances to the next four-space stop.
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

            // A line never breaks inside a grapheme cluster: a combining mark, a
            // variation selector, a skin-tone modifier or anything after a
            // zero-width joiner stays with the base character it decorates.
            const bool clusterContinuation = IsClusterExtender(codepoint) || previous == 0x200D;
            const bool overflows =
                wrap && !clusterContinuation && (lineWidth + advance) > availableWidth && lineWidth > 0.0f;

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

    GlyphCache::SizeBucket& GlyphCache::BucketFor(std::uint32_t faceId,
                                                  float fontSize,
                                                  FontWeightKind weight,
                                                  float deviceScale)
    {
        // The device font size is what glyphs are rasterized at, quantised to a
        // quarter pixel so a slowly animating size does not explode the cache.
        const float deviceSize = fontSize * deviceScale;
        const std::uint32_t sizeKey = static_cast<std::uint32_t>(deviceSize * 4.0f + 0.5f);

        for (const std::unique_ptr<SizeBucket>& existing : buckets)
        {
            if (existing->sizeKey == sizeKey && existing->weight == weight &&
                existing->faceId == faceId)
            {
                return *existing;
            }
        }

        auto bucket = std::make_unique<SizeBucket>();
        bucket->sizeKey = sizeKey;
        bucket->weight = weight;
        bucket->faceId = faceId;

        SizeBucket& result = *bucket;
        buckets.push_back(std::move(bucket));
        return result;
    }

    bool GlyphCache::RasterizeColor(SizeBucket& bucket, const FontFace& face, const Glyph& glyph,
                                    float scale, std::uint64_t key)
    {
        if (glyph.glyphId == 0 || !face.HasColorGlyphs())
        {
            return false;
        }

        // A COLR glyph paints as a stack of other glyphs, each in a palette
        // colour. The base glyph usually has no outline of its own, so the
        // extent comes from the union of the layers.
        std::vector<FontFace::ColorLayer> layers;
        if (face.ColorLayers(glyph.glyphId, layers) && !layers.empty())
        {
            std::vector<const Glyph*> layerGlyphs;
            layerGlyphs.reserve(layers.size());

            bool anyInk = false;
            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;

            for (const FontFace::ColorLayer& layer : layers)
            {
                const Glyph* layerGlyph = face.FindGlyphById(layer.glyphId);
                layerGlyphs.push_back(layerGlyph);

                if (layerGlyph == nullptr || !layerGlyph->hasInk)
                {
                    continue;
                }

                if (!anyInk)
                {
                    x0 = layerGlyph->x0;
                    y0 = layerGlyph->y0;
                    x1 = layerGlyph->x1;
                    y1 = layerGlyph->y1;
                    anyInk = true;
                }
                else
                {
                    x0 = std::min(x0, layerGlyph->x0);
                    y0 = std::min(y0, layerGlyph->y0);
                    x1 = std::max(x1, layerGlyph->x1);
                    y1 = std::max(y1, layerGlyph->y1);
                }
            }

            if (!anyInk)
            {
                return false;
            }

            const float pad = 1.0f;
            const int originX = static_cast<int>(std::floor(x0 * scale - pad));
            const int originY = static_cast<int>(std::floor(-(y1 * scale + pad)));
            const int width = static_cast<int>(std::ceil(x1 * scale + pad)) - originX;
            const int height = static_cast<int>(std::ceil(-(y0 * scale - pad))) - originY;

            if (width <= 0 || height <= 0 || width >= 4096 || height >= 4096)
            {
                return false;
            }

            const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            const std::size_t colorStart = bucket.colors.size();
            bucket.colors.resize(colorStart + pixels * 4, 0);
            std::uint8_t* target = bucket.colors.data() + colorStart;

            std::vector<std::uint8_t> layerCoverage(pixels, 0);

            for (std::size_t index = 0; index < layers.size(); ++index)
            {
                const Glyph* layerGlyph = layerGlyphs[index];
                if (layerGlyph == nullptr || !layerGlyph->hasInk)
                {
                    continue;
                }

                std::fill(layerCoverage.begin(), layerCoverage.end(), 0);

                std::vector<std::vector<Vec2>> polygons;
                FlattenGlyph(face, *layerGlyph, scale, static_cast<float>(originX),
                             static_cast<float>(originY), polygons);
                FillPolygons(polygons, layerCoverage.data(), width, height);

                std::uint8_t red = 0;
                std::uint8_t green = 0;
                std::uint8_t blue = 0;
                std::uint8_t alpha = 0;

                // A palette index the face does not have paints nothing, rather
                // than an invented black.
                if (!face.PaletteEntry(layers[index].paletteIndex, red, green, blue, alpha) ||
                    alpha == 0)
                {
                    continue;
                }

                for (std::size_t pixel = 0; pixel < pixels; ++pixel)
                {
                    const std::uint32_t coverage = layerCoverage[pixel];
                    if (coverage == 0)
                    {
                        continue;
                    }

                    const std::uint32_t sourceAlpha = (coverage * alpha) / 255u;
                    const std::uint32_t inverse = 255u - sourceAlpha;
                    std::uint8_t* destination = target + pixel * 4;

                    destination[0] = static_cast<std::uint8_t>((red * sourceAlpha + destination[0] * inverse) / 255u);
                    destination[1] = static_cast<std::uint8_t>((green * sourceAlpha + destination[1] * inverse) / 255u);
                    destination[2] = static_cast<std::uint8_t>((blue * sourceAlpha + destination[2] * inverse) / 255u);
                    destination[3] = static_cast<std::uint8_t>(sourceAlpha + (destination[3] * inverse) / 255u);
                }
            }

            GlyphBitmap bitmap;
            bitmap.advance = glyph.advance * scale;
            bitmap.width = static_cast<std::uint32_t>(width);
            bitmap.height = static_cast<std::uint32_t>(height);
            bitmap.bearingX = static_cast<float>(originX);
            bitmap.bearingY = static_cast<float>(originY);
            bitmap.blank = false;
            bitmap.color = true;
            bitmap.firstColor = static_cast<std::uint32_t>(colorStart);
            bucket.glyphs[key] = bitmap;
            return true;
        }

        // An embedded bitmap (`CBDT`): the face ships the pixels, usually PNG.
        const std::uint8_t* data = nullptr;
        std::uint32_t length = 0;
        std::int32_t bearingX = 0;
        std::int32_t bearingY = 0;
        const std::uint16_t ppem = static_cast<std::uint16_t>(
            std::max(1.0f, std::round(static_cast<float>(bucket.sizeKey) * 0.25f)));

        if (!face.BitmapGlyph(glyph.glyphId, ppem, data, length, bearingX, bearingY) || data == nullptr ||
            length == 0)
        {
            return false;
        }

        Image image;
        const ImageDecodeResult decoded = DecodeImage(data, length, image);
        if (!decoded.success || image.width == 0 || image.height == 0)
        {
            return false;
        }

        const std::size_t pixels = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
        const std::size_t colorStart = bucket.colors.size();
        bucket.colors.resize(colorStart + pixels * 4, 0);
        std::uint8_t* target = bucket.colors.data() + colorStart;

        // Normalise whatever the decoder produced to straight RGBA.
        for (std::size_t pixel = 0; pixel < pixels; ++pixel)
        {
            const std::uint8_t* source = image.pixels.data() + pixel * image.BytesPerPixel();
            std::uint8_t red = 0;
            std::uint8_t green = 0;
            std::uint8_t blue = 0;
            std::uint8_t alpha = 255;

            switch (image.format)
            {
            case PixelFormat::Bgra8:
                blue = source[0];
                green = source[1];
                red = source[2];
                alpha = source[3];
                break;
            case PixelFormat::Gray8:
                red = green = blue = source[0];
                break;
            case PixelFormat::Rgba8Premultiplied:
                alpha = source[3];
                if (alpha != 0)
                {
                    red = static_cast<std::uint8_t>(std::min(255u, source[0] * 255u / alpha));
                    green = static_cast<std::uint8_t>(std::min(255u, source[1] * 255u / alpha));
                    blue = static_cast<std::uint8_t>(std::min(255u, source[2] * 255u / alpha));
                }
                break;
            case PixelFormat::Rgba8:
            default:
                red = source[0];
                green = source[1];
                blue = source[2];
                alpha = source[3];
                break;
            }

            target[pixel * 4 + 0] = red;
            target[pixel * 4 + 1] = green;
            target[pixel * 4 + 2] = blue;
            target[pixel * 4 + 3] = alpha;
        }

        GlyphBitmap bitmap;
        bitmap.advance = glyph.advance * scale;
        bitmap.width = image.width;
        bitmap.height = image.height;
        bitmap.bearingX = static_cast<float>(bearingX);
        // CBDT bearings are y upwards; the bitmap top sits above the baseline.
        bitmap.bearingY = static_cast<float>(-bearingY);
        bitmap.blank = false;
        bitmap.color = true;
        bitmap.firstColor = static_cast<std::uint32_t>(colorStart);
        bucket.glyphs[key] = bitmap;
        return true;
    }

    void GlyphCache::RasterizeGlyph(SizeBucket& bucket, const FontFace& face, const Glyph& sourceGlyph,
                                    std::uint64_t key)
    {
        const Glyph& glyphValue = sourceGlyph;
        const Glyph* glyph = &glyphValue;

        const float deviceSize = static_cast<float>(bucket.sizeKey) * 0.25f;
        const float unitsPerEm = face.UnitsPerEm() > 0.0f ? face.UnitsPerEm() : 1000.0f;
        const float scale = deviceSize / unitsPerEm;

        // A colour glyph carries its own pixels; try that before any outline.
        if (RasterizeColor(bucket, face, *glyph, scale, key))
        {
            ++rasterizedGlyphs;
            return;
        }

        GlyphBitmap bitmap;
        bitmap.advance = glyph->advance * scale;

        if (!glyph->hasInk)
        {
            // A space, or a glyph whose outline the face does not carry: it has
            // an advance and no coverage, which is exactly what blank means.
            bucket.glyphs[key] = bitmap;
            ++rasterizedGlyphs;
            return;
        }

        const float embolden = glyph->embolden * scale;
        const float pad = embolden + 1.0f;

        const float left = glyph->x0 * scale - pad;
        const float right = glyph->x1 * scale + pad;
        const float top = glyph->y1 * scale + pad;
        const float bottom = glyph->y0 * scale - pad;

        const int originX = static_cast<int>(std::floor(left));
        const int originY = static_cast<int>(std::floor(-top));
        const int width = static_cast<int>(std::ceil(right)) - originX;
        const int height = static_cast<int>(std::ceil(-bottom)) - originY;

        if (width <= 0 || height <= 0 || width >= 4096 || height >= 4096)
        {
            bucket.glyphs[key] = bitmap;
            ++rasterizedGlyphs;
            return;
        }

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

        std::uint8_t* coverage = bucket.coverage.data() + coverageStart;

        if (face.UsesOutlines())
        {
            std::vector<std::vector<Vec2>> polygons;
            FlattenGlyph(face, *glyph, scale, static_cast<float>(originX), static_cast<float>(originY),
                         polygons);
            FillPolygons(polygons, coverage, width, height);

            if (embolden > 0.0f)
            {
                DilateCoverage(coverage, width, height, embolden);
            }
        }
        else
        {
            RasterizeStrokes(face, *glyph, scale, originX, originY, width, height, bucket.weight,
                             deviceSize, coverage);
        }

        bucket.glyphs[key] = bitmap;
        ++rasterizedGlyphs;
    }

    RasterizedGlyph GlyphCache::Get(std::uint32_t codepoint, float fontSize, FontWeightKind weight,
                                    float deviceScale)
    {
        RasterizedGlyph result;

        if (fontSize <= 0.0f)
        {
            return result;
        }

        const FontFace& face = fonts.FaceFor(codepoint, weight);

        // A code point no face in the chain maps has no coverage: the caller
        // paints nothing for it rather than an invented box.
        const Glyph* glyph = face.FindGlyph(codepoint);
        if (glyph == nullptr)
        {
            return result;
        }

        const std::uint32_t faceId = fonts.IndexOf(face);
        SizeBucket& bucket = BucketFor(faceId, fontSize, weight, deviceScale);
        const std::uint64_t key = codepoint;

        if (bucket.glyphs.find(key) == bucket.glyphs.end())
        {
            RasterizeGlyph(bucket, face, *glyph, key);
        }

        const auto found = bucket.glyphs.find(key);
        if (found == bucket.glyphs.end())
        {
            return result;
        }

        const GlyphBitmap& bitmap = found->second;
        result.bitmap = &bitmap;
        result.coverage =
            bitmap.blank || bitmap.color ? nullptr : bucket.coverage.data() + bitmap.firstCoverage;
        result.color =
            bitmap.color ? bucket.colors.data() + static_cast<std::size_t>(bitmap.firstColor) * 4
                         : nullptr;
        return result;
    }

    RasterizedGlyph GlyphCache::GetShaped(std::uint32_t faceIndex,
                                         std::uint16_t glyphId,
                                         float fontSize,
                                         FontWeightKind weight,
                                         float deviceScale)
    {
        RasterizedGlyph result;

        if (fontSize <= 0.0f)
        {
            return result;
        }

        const FontFace* face = fonts.FaceAt(faceIndex);
        if (face == nullptr)
        {
            return result;
        }

        const Glyph* glyph = face->FindGlyphById(glyphId);
        if (glyph == nullptr)
        {
            return result;
        }

        SizeBucket& bucket = BucketFor(faceIndex, fontSize, weight, deviceScale);
        const std::uint64_t key = (1ull << 32) | glyphId;

        if (bucket.glyphs.find(key) == bucket.glyphs.end())
        {
            RasterizeGlyph(bucket, *face, *glyph, key);
        }

        const auto found = bucket.glyphs.find(key);
        if (found == bucket.glyphs.end())
        {
            return result;
        }

        const GlyphBitmap& bitmap = found->second;
        result.bitmap = &bitmap;
        result.coverage =
            bitmap.blank || bitmap.color ? nullptr : bucket.coverage.data() + bitmap.firstCoverage;
        result.color =
            bitmap.color ? bucket.colors.data() + static_cast<std::size_t>(bitmap.firstColor) * 4
                         : nullptr;
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

        // Before the first glyph is always the start of the string.
        if (localX <= 0.0f)
        {
            return 0;
        }

        std::uint32_t length = 0;
        while (text[length] != '\0')
        {
            ++length;
        }

        std::vector<PositionedGlyph> glyphs;
        ShapeRun(text, 0, length, fontSize, weight, glyphs);

        float x = 0.0f;
        std::uint32_t best = 0;
        float bestDistance = localX; // distance to the string's start

        for (std::size_t index = 0; index < glyphs.size(); ++index)
        {
            const float startDistance = std::fabs(localX - x);
            if (startDistance < bestDistance)
            {
                bestDistance = startDistance;
                best = glyphs[index].byteStart;
            }

            x += glyphs[index].advance;

            const std::uint32_t endByte =
                (index + 1 < glyphs.size()) ? glyphs[index + 1].byteStart : length;
            const float endDistance = std::fabs(localX - x);
            if (endDistance < bestDistance)
            {
                bestDistance = endDistance;
                best = endByte;
            }
        }

        return best;
    }
}
