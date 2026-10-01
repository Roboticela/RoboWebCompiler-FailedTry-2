#include "RoboRenderer/SvgImage.h"

#include "RoboRenderer/SvgGlyph.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace RoboRenderer
{
    namespace
    {
        constexpr int SubSamples = 4;
        constexpr int MaxCoverage = SubSamples * SubSamples;
        constexpr float MaxRasterSize = 2048.0f;
        constexpr float MinRasterSize = 1.0f;

        struct Colour
        {
            float red = 0.0f;
            float green = 0.0f;
            float blue = 0.0f;
            float alpha = 1.0f;
        };

        /// A paint resolved at one point in user space.
        Colour ShadePaint(const SvgPaint& paint, float x, float y)
        {
            Colour colour;
            colour.red = paint.red;
            colour.green = paint.green;
            colour.blue = paint.blue;
            colour.alpha = paint.alpha;

            if (paint.kind == SvgPaint::Kind::Solid || paint.stops.empty())
            {
                return colour;
            }

            float position = 0.0f;
            if (paint.kind == SvgPaint::Kind::Linear)
            {
                const float dx = paint.x1 - paint.x0;
                const float dy = paint.y1 - paint.y0;
                const float lengthSquared = dx * dx + dy * dy;
                if (lengthSquared > 0.0f)
                {
                    position = ((x - paint.x0) * dx + (y - paint.y0) * dy) / lengthSquared;
                }
            }
            else
            {
                const float dx = x - paint.x0;
                const float dy = y - paint.y0;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const float span = paint.r1 - paint.r0;
                position = span > 0.0f ? (distance - paint.r0) / span : 0.0f;
            }

            position = std::min(1.0f, std::max(0.0f, position));

            const std::vector<SvgPaint::Stop>& stops = paint.stops;
            const SvgPaint::Stop& first = stops.front();
            const SvgPaint::Stop& last = stops.back();

            if (position <= first.offset)
            {
                colour.red = first.red;
                colour.green = first.green;
                colour.blue = first.blue;
                colour.alpha = first.alpha * paint.alpha;
                return colour;
            }

            if (position >= last.offset)
            {
                colour.red = last.red;
                colour.green = last.green;
                colour.blue = last.blue;
                colour.alpha = last.alpha * paint.alpha;
                return colour;
            }

            for (std::size_t index = 1; index < stops.size(); ++index)
            {
                const SvgPaint::Stop& upper = stops[index];
                if (position > upper.offset)
                {
                    continue;
                }

                const SvgPaint::Stop& lower = stops[index - 1];
                const float span = upper.offset - lower.offset;
                const float t = span > 0.0f ? (position - lower.offset) / span : 0.0f;

                colour.red = lower.red + (upper.red - lower.red) * t;
                colour.green = lower.green + (upper.green - lower.green) * t;
                colour.blue = lower.blue + (upper.blue - lower.blue) * t;
                colour.alpha = (lower.alpha + (upper.alpha - lower.alpha) * t) * paint.alpha;
                return colour;
            }

            return colour;
        }

        void ComputeBounds(const std::vector<std::vector<SvgPoint>>& contours,
                           float& x0,
                           float& y0,
                           float& x1,
                           float& y1)
        {
            x0 = y0 = 1.0e30f;
            x1 = y1 = -1.0e30f;

            for (const std::vector<SvgPoint>& contour : contours)
            {
                for (const SvgPoint& point : contour)
                {
                    x0 = std::min(x0, point.x);
                    y0 = std::min(y0, point.y);
                    x1 = std::max(x1, point.x);
                    y1 = std::max(y1, point.y);
                }
            }
        }

        std::vector<SvgPaint::Stop> SortedStops(const SvgPaint& paint)
        {
            std::vector<SvgPaint::Stop> stops = paint.stops;
            std::sort(stops.begin(), stops.end(),
                      [](const SvgPaint::Stop& left, const SvgPaint::Stop& right) {
                          return left.offset < right.offset;
                      });
            return stops;
        }

        /// Opaque or not, a colour whose alpha is zero is not worth a pass.
        bool Paints(const SvgPaint& paint)
        {
            if (!(paint.alpha > 0.0f))
            {
                return false;
            }
            if (paint.kind != SvgPaint::Kind::Solid && !paint.stops.empty())
            {
                for (const SvgPaint::Stop& stop : paint.stops)
                {
                    if (stop.alpha > 0.0f)
                    {
                        return true;
                    }
                }
            }
            return true;
        }

        /// Scanline coverage of a fill. `polygons` are in device pixels already.
        /// Every polygon is treated as closed, which is what SVG's fill does to
        /// an open subpath too.
        void RasterizeFill(const std::vector<std::vector<SvgPoint>>& polygons,
                           bool evenOdd,
                           int width,
                           int rowBegin,
                           int rowEnd,
                           std::vector<std::uint8_t>& coverage)
        {
            std::vector<float> crossings;
            std::vector<int> directions;
            std::vector<int> accumulator(static_cast<std::size_t>(width), 0);

            for (int row = rowBegin; row < rowEnd; ++row)
            {
                std::fill(accumulator.begin(), accumulator.end(), 0);

                for (int sub = 0; sub < SubSamples; ++sub)
                {
                    const float sampleY =
                        static_cast<float>(row) + (static_cast<float>(sub) + 0.5f) / SubSamples;

                    crossings.clear();
                    directions.clear();

                    for (const std::vector<SvgPoint>& polygon : polygons)
                    {
                        const std::size_t count = polygon.size();
                        if (count < 2)
                        {
                            continue;
                        }

                        for (std::size_t index = 0; index < count; ++index)
                        {
                            const SvgPoint& a = polygon[index];
                            const SvgPoint& b = polygon[(index + 1) % count];

                            const bool aBelow = a.y <= sampleY;
                            const bool bBelow = b.y <= sampleY;
                            if (aBelow == bBelow)
                            {
                                continue;
                            }

                            const float t = (sampleY - a.y) / (b.y - a.y);
                            crossings.push_back(a.x + t * (b.x - a.x));
                            // Even-odd counts crossings, so every edge contributes
                            // one; nonzero carries the edge's direction instead.
                            directions.push_back(evenOdd ? 1 : (b.y > a.y ? 1 : -1));
                        }
                    }

                    // Crossing counts are small, so an insertion sort beats
                    // anything cleverer.
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
                        // Even-odd counts crossings, so an odd count is inside;
                        // nonzero tests the signed winding number itself.
                        const bool inside = evenOdd ? ((winding & 1) != 0) : (winding != 0);

                        if (inside && index > 0)
                        {
                            const float from = crossings[index - 1];
                            const float to = crossings[index];

                            if (to > from)
                            {
                                const int firstPixel = std::max(0, static_cast<int>(std::floor(from)));
                                const int lastPixel = std::min(width - 1, static_cast<int>(std::ceil(to)) - 1);

                                for (int pixel = firstPixel; pixel <= lastPixel; ++pixel)
                                {
                                    for (int sample = 0; sample < SubSamples; ++sample)
                                    {
                                        const float sampleX =
                                            static_cast<float>(pixel) +
                                            (static_cast<float>(sample) + 0.5f) / SubSamples;
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

                std::uint8_t* rowOutput =
                    coverage.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width);

                for (int x = 0; x < width; ++x)
                {
                    const int value = accumulator[static_cast<std::size_t>(x)];
                    rowOutput[x] = static_cast<std::uint8_t>(value * 255 / MaxCoverage);
                }
            }
        }

        /// Turns a contour into the quads one stroke of it covers: a rectangle
        /// per segment, plus a small square at each vertex so joins do not leave
        /// a notch. Square caps are a deliberate approximation of the round and
        /// butt caps SVG offers.
        void AppendStrokePolygons(const std::vector<SvgPoint>& contour,
                                  float halfWidth,
                                  std::vector<std::vector<SvgPoint>>& out)
        {
            const std::size_t count = contour.size();
            if (count < 2 || halfWidth <= 0.0f)
            {
                return;
            }

            const bool closed = count >= 3;
            const std::size_t segments = closed ? count : count - 1;

            for (std::size_t index = 0; index < segments; ++index)
            {
                const SvgPoint& a = contour[index];
                const SvgPoint& b = contour[(index + 1) % count];

                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length <= 0.0f)
                {
                    continue;
                }

                const float nx = -dy / length * halfWidth;
                const float ny = dx / length * halfWidth;

                out.push_back({ SvgPoint{ a.x + nx, a.y + ny }, SvgPoint{ b.x + nx, b.y + ny },
                                SvgPoint{ b.x - nx, b.y - ny }, SvgPoint{ a.x - nx, a.y - ny } });
            }

            for (const SvgPoint& point : contour)
            {
                out.push_back({ SvgPoint{ point.x - halfWidth, point.y - halfWidth },
                                SvgPoint{ point.x + halfWidth, point.y - halfWidth },
                                SvgPoint{ point.x + halfWidth, point.y + halfWidth },
                                SvgPoint{ point.x - halfWidth, point.y + halfWidth } });
            }
        }

        /// Source-over composite of one non-premultiplied sample.
        void BlendPixel(Image& image, std::uint32_t x, std::uint32_t y, const Colour& source, float coverage)
        {
            std::uint8_t* pixel = image.MutablePixel(x, y);
            if (pixel == nullptr)
            {
                return;
            }

            const float sourceAlpha = std::min(1.0f, std::max(0.0f, source.alpha * coverage));
            if (sourceAlpha <= 0.0f)
            {
                return;
            }

            const float destinationAlpha = static_cast<float>(pixel[3]) / 255.0f;
            const float outAlpha = sourceAlpha + destinationAlpha * (1.0f - sourceAlpha);
            if (outAlpha <= 0.0f)
            {
                return;
            }

            const float weights[3] = { source.red, source.green, source.blue };
            for (int channel = 0; channel < 3; ++channel)
            {
                const float destination = static_cast<float>(pixel[channel]) / 255.0f;
                const float blended =
                    (weights[channel] * sourceAlpha + destination * destinationAlpha * (1.0f - sourceAlpha)) /
                    outAlpha;
                pixel[channel] = static_cast<std::uint8_t>(
                    std::min(255.0f, std::max(0.0f, blended * 255.0f + 0.5f)));
            }

            pixel[3] = static_cast<std::uint8_t>(
                std::min(255.0f, std::max(0.0f, outAlpha * 255.0f + 0.5f)));
        }

        /// True when the SVG carries anything this rasterizer can draw.
        bool HasDrawableShape(const std::vector<SvgShape>& shapes)
        {
            for (const SvgShape& shape : shapes)
            {
                if (!shape.contours.empty() && (shape.hasFill || shape.hasStroke))
                {
                    return true;
                }
            }
            return false;
        }
    }

    ImageDecodeResult DecodeSvgDocument(const char* text,
                                        std::size_t size,
                                        float targetWidth,
                                        float targetHeight,
                                        Image& out)
    {
        ImageDecodeResult result;
        result.format = "svg";

        if (text == nullptr || size == 0)
        {
            result.message = "the SVG document is empty";
            return result;
        }

        std::vector<SvgShape> shapes;
        SvgDocumentMetrics metrics;
        if (!RenderSvgDocument(text, size, shapes, &metrics) || !HasDrawableShape(shapes))
        {
            result.message = "the SVG document contains nothing this renderer can draw";
            return result;
        }

        // The document's own size wins; a caller-supplied size is the next best
        // guess; the viewBox answers for a document that has neither.
        bool haveSize = metrics.width > 0.0f && metrics.height > 0.0f;
        float rasterWidth = metrics.width;
        float rasterHeight = metrics.height;

        if (!haveSize && targetWidth > 0.0f && targetHeight > 0.0f)
        {
            rasterWidth = targetWidth;
            rasterHeight = targetHeight;
            haveSize = true;
        }

        if (!haveSize && metrics.hasViewBox)
        {
            rasterWidth = metrics.viewWidth;
            rasterHeight = metrics.viewHeight;
        }

        if (rasterWidth <= 0.0f || rasterHeight <= 0.0f)
        {
            rasterWidth = 300.0f;
            rasterHeight = 150.0f;
        }

        rasterWidth = std::min(MaxRasterSize, std::max(MinRasterSize, rasterWidth));
        rasterHeight = std::min(MaxRasterSize, std::max(MinRasterSize, rasterHeight));

        const std::uint32_t width = static_cast<std::uint32_t>(std::lround(rasterWidth));
        const std::uint32_t height = static_cast<std::uint32_t>(std::lround(rasterHeight));

        // The user-space rectangle the viewport shows. Without a viewBox the
        // document is already in pixels, so the mapping is straight.
        float userX = 0.0f;
        float userY = 0.0f;
        float userWidth = rasterWidth;
        float userHeight = rasterHeight;
        if (metrics.hasViewBox)
        {
            userX = metrics.viewX;
            userY = metrics.viewY;
            userWidth = metrics.viewWidth;
            userHeight = metrics.viewHeight;
        }

        // preserveAspectRatio defaults to xMidYMid meet: one uniform scale that
        // fits, with the leftover centred.
        const float scale = std::min(static_cast<float>(width) / userWidth,
                                     static_cast<float>(height) / userHeight);
        const float offsetX = (static_cast<float>(width) - userWidth * scale) * 0.5f - userX * scale;
        const float offsetY = (static_cast<float>(height) - userHeight * scale) * 0.5f - userY * scale;

        out.Allocate(width, height, PixelFormat::Rgba8);

        const int imageWidth = static_cast<int>(width);
        std::vector<std::uint8_t> fillCoverage(static_cast<std::size_t>(width) * height, 0);
        std::vector<std::uint8_t> strokeCoverage(static_cast<std::size_t>(width) * height, 0);

        std::vector<std::vector<SvgPoint>> fillPolygons;
        std::vector<std::vector<SvgPoint>> strokePolygons;

        for (SvgShape& shape : shapes)
        {
            if (shape.contours.empty())
            {
                continue;
            }

            if (shape.hasFill && shape.paint.kind != SvgPaint::Kind::Solid && !shape.paint.stops.empty())
            {
                shape.paint.stops = SortedStops(shape.paint);
            }
            if (shape.hasStroke && shape.stroke.kind != SvgPaint::Kind::Solid && !shape.stroke.stops.empty())
            {
                shape.stroke.stops = SortedStops(shape.stroke);
            }

            if (shape.hasFill && !Paints(shape.paint))
            {
                shape.hasFill = false;
            }
            if (shape.hasStroke && (shape.strokeWidth <= 0.0f || !Paints(shape.stroke)))
            {
                shape.hasStroke = false;
            }

            if (!shape.hasFill && !shape.hasStroke)
            {
                continue;
            }

            // To device pixels once, then rasterize the whole shape in one pass.
            const auto toDevice = [&](const std::vector<SvgPoint>& contour) {
                std::vector<SvgPoint> device;
                device.reserve(contour.size());
                for (const SvgPoint& point : contour)
                {
                    device.push_back(SvgPoint{ offsetX + point.x * scale, offsetY + point.y * scale });
                }
                return device;
            };

            fillPolygons.clear();
            strokePolygons.clear();

            if (shape.hasFill)
            {
                for (const std::vector<SvgPoint>& contour : shape.contours)
                {
                    if (contour.size() >= 3)
                    {
                        fillPolygons.push_back(toDevice(contour));
                    }
                }
                shape.hasFill = !fillPolygons.empty();
            }

            if (shape.hasStroke)
            {
                const float halfWidth = std::max(0.5f, shape.strokeWidth * 0.5f);
                for (const std::vector<SvgPoint>& contour : shape.contours)
                {
                    std::vector<SvgPoint> device = toDevice(contour);
                    if (device.size() >= 2)
                    {
                        AppendStrokePolygons(device, halfWidth, strokePolygons);
                    }
                }
                shape.hasStroke = !strokePolygons.empty();
            }

            if (!shape.hasFill && !shape.hasStroke)
            {
                continue;
            }

            float bx0 = 0.0f;
            float by0 = 0.0f;
            float bx1 = 0.0f;
            float by1 = 0.0f;
            bool haveBounds = false;

            const auto includeBounds = [&](const std::vector<std::vector<SvgPoint>>& polygons) {
                for (const std::vector<SvgPoint>& polygon : polygons)
                {
                    for (const SvgPoint& point : polygon)
                    {
                        if (!haveBounds)
                        {
                            bx0 = bx1 = point.x;
                            by0 = by1 = point.y;
                            haveBounds = true;
                            continue;
                        }
                        bx0 = std::min(bx0, point.x);
                        by0 = std::min(by0, point.y);
                        bx1 = std::max(bx1, point.x);
                        by1 = std::max(by1, point.y);
                    }
                }
            };

            includeBounds(fillPolygons);
            includeBounds(strokePolygons);

            if (!haveBounds)
            {
                continue;
            }

            const int rowBegin = std::max(0, static_cast<int>(std::floor(by0)));
            const int rowEnd = std::min(static_cast<int>(height), static_cast<int>(std::ceil(by1)) + 1);
            const int columnBegin = std::max(0, static_cast<int>(std::floor(bx0)));
            const int columnEnd =
                std::min(static_cast<int>(width), static_cast<int>(std::ceil(bx1)) + 1);

            if (rowBegin >= rowEnd || columnBegin >= columnEnd)
            {
                continue;
            }

            // Coverage is only meaningful inside the shape's own box, and clearing
            // exactly that box keeps a big document from paying for the whole
            // surface once per shape.
            const auto clearBand = [&](std::vector<std::uint8_t>& buffer) {
                for (int row = rowBegin; row < rowEnd; ++row)
                {
                    std::uint8_t* rowStart =
                        buffer.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
                    std::fill(rowStart + columnBegin, rowStart + columnEnd, std::uint8_t(0));
                }
            };

            if (shape.hasFill)
            {
                clearBand(fillCoverage);
                RasterizeFill(fillPolygons, shape.evenOdd, imageWidth, rowBegin, rowEnd, fillCoverage);
            }
            if (shape.hasStroke)
            {
                clearBand(strokeCoverage);
                RasterizeFill(strokePolygons, false, imageWidth, rowBegin, rowEnd, strokeCoverage);
            }

            for (int row = rowBegin; row < rowEnd; ++row)
            {
                for (int column = columnBegin; column < columnEnd; ++column)
                {
                    const std::size_t offset =
                        static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                        static_cast<std::size_t>(column);

                    const float fillAmount =
                        shape.hasFill ? static_cast<float>(fillCoverage[offset]) / 255.0f : 0.0f;
                    const float strokeAmount =
                        shape.hasStroke ? static_cast<float>(strokeCoverage[offset]) / 255.0f : 0.0f;

                    if (fillAmount <= 0.0f && strokeAmount <= 0.0f)
                    {
                        continue;
                    }

                    const float userPixelX = (static_cast<float>(column) + 0.5f - offsetX) / scale;
                    const float userPixelY = (static_cast<float>(row) + 0.5f - offsetY) / scale;

                    // SVG paints the fill and then the stroke over it.
                    if (fillAmount > 0.0f)
                    {
                        Colour fill = ShadePaint(shape.paint, userPixelX, userPixelY);
                        fill.alpha *= fillAmount;
                        BlendPixel(out, static_cast<std::uint32_t>(column),
                                   static_cast<std::uint32_t>(row), fill, 1.0f);
                    }

                    if (strokeAmount > 0.0f)
                    {
                        Colour stroke = ShadePaint(shape.stroke, userPixelX, userPixelY);
                        stroke.alpha *= strokeAmount;
                        BlendPixel(out, static_cast<std::uint32_t>(column),
                                   static_cast<std::uint32_t>(row), stroke, 1.0f);
                    }
                }
            }
        }

        result.success = true;
        return result;
    }
}
