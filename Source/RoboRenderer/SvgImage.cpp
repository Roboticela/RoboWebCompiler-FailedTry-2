#include "RoboRenderer/SvgImage.h"

#include "RoboRenderer/SvgGlyph.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace RoboRenderer
{
    namespace
    {
        constexpr int SamplesPerAxis = 4;
        constexpr int SamplesPerPixel = SamplesPerAxis * SamplesPerAxis;
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

        /// Nonzero winding number of the point against every contour.
        int WindingNumber(const std::vector<std::vector<SvgPoint>>& contours, float x, float y)
        {
            int winding = 0;

            for (const std::vector<SvgPoint>& contour : contours)
            {
                if (contour.size() < 3)
                {
                    continue;
                }

                for (std::size_t index = 0; index < contour.size(); ++index)
                {
                    const SvgPoint& a = contour[index];
                    const SvgPoint& b = contour[(index + 1) % contour.size()];

                    if (a.y <= y)
                    {
                        if (b.y > y && (b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y) > 0.0f)
                        {
                            ++winding;
                        }
                    }
                    else if (b.y <= y && (b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y) < 0.0f)
                    {
                        --winding;
                    }
                }
            }

            return winding;
        }

        /// Even-odd rule: inside when an odd number of edges is crossed to the
        /// left of the point. The two rules agree unless contours overlap.
        bool EvenOddInside(const std::vector<std::vector<SvgPoint>>& contours, float x, float y)
        {
            int crossings = 0;

            for (const std::vector<SvgPoint>& contour : contours)
            {
                if (contour.size() < 3)
                {
                    continue;
                }

                for (std::size_t index = 0; index < contour.size(); ++index)
                {
                    const SvgPoint& a = contour[index];
                    const SvgPoint& b = contour[(index + 1) % contour.size()];

                    if ((a.y <= y) == (b.y <= y))
                    {
                        continue;
                    }

                    const float t = (y - a.y) / (b.y - a.y);
                    if (x < a.x + t * (b.x - a.x))
                    {
                        ++crossings;
                    }
                }
            }

            return (crossings & 1) != 0;
        }

        float DistanceToSegment(float px, float py, float ax, float ay, float bx, float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float lengthSquared = dx * dx + dy * dy;

            float t = 0.0f;
            if (lengthSquared > 0.0f)
            {
                t = ((px - ax) * dx + (py - ay) * dy) / lengthSquared;
                t = std::min(1.0f, std::max(0.0f, t));
            }

            const float cx = ax + dx * t;
            const float cy = ay + dy * t;
            const float ex = px - cx;
            const float ey = py - cy;
            return std::sqrt(ex * ex + ey * ey);
        }

        /// Distance from the point to the nearest edge of any contour.
        float DistanceToContours(const std::vector<std::vector<SvgPoint>>& contours, float x, float y)
        {
            float best = 1.0e30f;

            for (const std::vector<SvgPoint>& contour : contours)
            {
                if (contour.size() < 2)
                {
                    continue;
                }

                const std::size_t segments = contour.size() >= 3 ? contour.size() : contour.size() - 1;
                for (std::size_t index = 0; index < segments; ++index)
                {
                    const SvgPoint& a = contour[index];
                    const SvgPoint& b = contour[(index + 1) % contour.size()];
                    best = std::min(best, DistanceToSegment(x, y, a.x, a.y, b.x, b.y));
                }
            }

            return best;
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
            haveSize = true;
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

        const float sampleStep = 1.0f / static_cast<float>(SamplesPerAxis);

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

            float bx0 = 0.0f;
            float by0 = 0.0f;
            float bx1 = 0.0f;
            float by1 = 0.0f;
            ComputeBounds(shape.contours, bx0, by0, bx1, by1);

            const float strokePad = shape.hasStroke ? shape.strokeWidth * 0.5f + 1.0f : 1.0f;

            const float left = (offsetX + bx0 * scale) - strokePad;
            const float top = (offsetY + by0 * scale) - strokePad;
            const float right = (offsetX + bx1 * scale) + strokePad;
            const float bottom = (offsetY + by1 * scale) + strokePad;

            const std::int32_t beginX = std::max<std::int32_t>(0, static_cast<std::int32_t>(std::floor(left)));
            const std::int32_t beginY = std::max<std::int32_t>(0, static_cast<std::int32_t>(std::floor(top)));
            const std::int32_t endX =
                std::min<std::int32_t>(static_cast<std::int32_t>(width), static_cast<std::int32_t>(std::ceil(right)));
            const std::int32_t endY =
                std::min<std::int32_t>(static_cast<std::int32_t>(height), static_cast<std::int32_t>(std::ceil(bottom)));

            const float strokeHalf = shape.strokeWidth * 0.5f;

            for (std::int32_t py = beginY; py < endY; ++py)
            {
                for (std::int32_t px = beginX; px < endX; ++px)
                {
                    float accumulatedRed = 0.0f;
                    float accumulatedGreen = 0.0f;
                    float accumulatedBlue = 0.0f;
                    float accumulatedAlpha = 0.0f;

                    for (int sy = 0; sy < SamplesPerAxis; ++sy)
                    {
                        for (int sx = 0; sx < SamplesPerAxis; ++sx)
                        {
                            const float deviceX =
                                static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) * sampleStep;
                            const float deviceY =
                                static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) * sampleStep;
                            const float ux = (deviceX - offsetX) / scale;
                            const float uy = (deviceY - offsetY) / scale;

                            const bool stroked =
                                shape.hasStroke && DistanceToContours(shape.contours, ux, uy) <= strokeHalf;
                            const bool filled =
                                shape.hasFill && !stroked &&
                                (shape.evenOdd ? EvenOddInside(shape.contours, ux, uy)
                                               : WindingNumber(shape.contours, ux, uy) != 0);

                            if (!stroked && !filled)
                            {
                                continue;
                            }

                            const Colour colour = stroked
                                                      ? ShadePaint(shape.stroke, ux, uy)
                                                      : ShadePaint(shape.paint, ux, uy);

                            accumulatedRed += colour.red * colour.alpha;
                            accumulatedGreen += colour.green * colour.alpha;
                            accumulatedBlue += colour.blue * colour.alpha;
                            accumulatedAlpha += colour.alpha;
                        }
                    }

                    if (accumulatedAlpha <= 0.0f)
                    {
                        continue;
                    }

                    Colour sample;
                    sample.red = accumulatedRed / accumulatedAlpha;
                    sample.green = accumulatedGreen / accumulatedAlpha;
                    sample.blue = accumulatedBlue / accumulatedAlpha;
                    sample.alpha = accumulatedAlpha / static_cast<float>(SamplesPerPixel);

                    BlendPixel(out, static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), sample, 1.0f);
                }
            }
        }

        result.success = true;
        return result;
    }
}
