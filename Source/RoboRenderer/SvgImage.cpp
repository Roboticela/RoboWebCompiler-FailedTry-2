#include "RoboRenderer/SvgImage.h"

#include "RoboRenderer/SvgGlyph.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace RoboRenderer
{
    namespace
    {
        constexpr float Tau = 6.28318530717958647692f;
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

        /// Twice the area the shoelace formula gives, so the sign is a polygon's
        /// winding direction.
        float SignedArea(const std::vector<SvgPoint>& polygon)
        {
            float total = 0.0f;
            const std::size_t count = polygon.size();
            for (std::size_t index = 0; index < count; ++index)
            {
                const SvgPoint& a = polygon[index];
                const SvgPoint& b = polygon[(index + 1) % count];
                total += a.x * b.y - b.x * a.y;
            }
            return total * 0.5f;
        }

        /// Adds one piece of a stroke, wound the same way as every other piece.
        /// The non-zero rule *adds* overlapping pieces, so a piece wound the
        /// other way from the ones it touches cancels them out: a stroke built
        /// from pieces of mixed winding comes out speckled rather than solid.
        void AppendStrokePiece(std::vector<std::vector<SvgPoint>>& out, std::vector<SvgPoint> piece)
        {
            if (piece.size() < 3)
            {
                return;
            }

            if (SignedArea(piece) < 0.0f)
            {
                std::reverse(piece.begin(), piece.end());
            }

            out.push_back(std::move(piece));
        }

        /// The disc a round cap or a round join covers, as a polygon.
        void AppendStrokeDisc(std::vector<std::vector<SvgPoint>>& out,
                              float centreX,
                              float centreY,
                              float radius)
        {
            constexpr int Steps = 16;
            std::vector<SvgPoint> disc;
            disc.reserve(Steps);

            for (int step = 0; step < Steps; ++step)
            {
                const float angle = Tau * static_cast<float>(step) / static_cast<float>(Steps);
                disc.push_back(SvgPoint{ centreX + radius * std::cos(angle),
                                         centreY + radius * std::sin(angle) });
            }

            AppendStrokePiece(out, std::move(disc));
        }

        /// The rectangle one segment of a stroke covers.
        void AppendStrokeSegment(std::vector<std::vector<SvgPoint>>& out,
                                 const SvgPoint& a,
                                 const SvgPoint& b,
                                 float halfWidth)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length <= 0.0f)
            {
                return;
            }

            const float nx = -dy / length * halfWidth;
            const float ny = dx / length * halfWidth;

            AppendStrokePiece(out, { SvgPoint{ a.x + nx, a.y + ny }, SvgPoint{ b.x + nx, b.y + ny },
                                     SvgPoint{ b.x - nx, b.y - ny }, SvgPoint{ a.x - nx, a.y - ny } });
        }

        /// The end of an open contour. `outwardX/outwardY` is the unit direction
        /// the contour travels in, so the cap sits on the far side of the end.
        void AppendStrokeCap(std::vector<std::vector<SvgPoint>>& out,
                             const SvgPoint& end,
                             float outwardX,
                             float outwardY,
                             float halfWidth,
                             const SvgStrokeStyle& style)
        {
            if (style.cap == SvgStrokeStyle::Cap::Butt)
            {
                return;
            }

            if (style.cap == SvgStrokeStyle::Cap::Round)
            {
                AppendStrokeDisc(out, end.x, end.y, halfWidth);
                return;
            }

            const float nx = -outwardY * halfWidth;
            const float ny = outwardX * halfWidth;
            const float reach = halfWidth;

            AppendStrokePiece(out, { SvgPoint{ end.x + nx, end.y + ny },
                                     SvgPoint{ end.x + nx + outwardX * reach,
                                               end.y + ny + outwardY * reach },
                                     SvgPoint{ end.x - nx + outwardX * reach,
                                               end.y - ny + outwardY * reach },
                                     SvgPoint{ end.x - nx, end.y - ny } });
        }

        /// The wedge the two segment rectangles leave bare where they meet. The
        /// outside of the corner is the side where both segments put their offset
        /// on the same hand, and it is filled out to the miter point, an arc, or
        /// the straight cut between the two offset corners.
        void AppendStrokeJoin(std::vector<std::vector<SvgPoint>>& out,
                              const SvgPoint& corner,
                              float incomingX,
                              float incomingY,
                              float outgoingX,
                              float outgoingY,
                              float halfWidth,
                              const SvgStrokeStyle& style)
        {
            const float cross = incomingX * outgoingY - incomingY * outgoingX;
            if (cross == 0.0f)
            {
                // Straight through: the rectangles already meet here.
                return;
            }

            const float side = cross > 0.0f ? -1.0f : 1.0f;
            const float firstNormalX = side * -incomingY;
            const float firstNormalY = side * incomingX;
            const float secondNormalX = side * -outgoingY;
            const float secondNormalY = side * outgoingX;

            const SvgPoint first{ corner.x + firstNormalX * halfWidth,
                                  corner.y + firstNormalY * halfWidth };
            const SvgPoint second{ corner.x + secondNormalX * halfWidth,
                                   corner.y + secondNormalY * halfWidth };

            if (style.join == SvgStrokeStyle::Join::Round)
            {
                AppendStrokeDisc(out, corner.x, corner.y, halfWidth);
                return;
            }

            if (style.join == SvgStrokeStyle::Join::Miter)
            {
                // The miter tip is where the two offset edges cross, and its
                // distance from the corner over the stroke width is the ratio
                // `stroke-miterlimit` bounds: past the limit the corner is cut
                // back to a bevel rather than spiking off the joint.
                const float dot = incomingX * outgoingX + incomingY * outgoingY;
                const float ratio = std::sqrt(2.0f / std::max(0.0f, 1.0f + dot));
                if (ratio <= style.miterLimit)
                {
                    const float normals = 1.0f + firstNormalX * secondNormalX +
                                          firstNormalY * secondNormalY;
                    if (normals > 0.0f)
                    {
                        const float tipX =
                            corner.x + (firstNormalX + secondNormalX) / normals * halfWidth;
                        const float tipY =
                            corner.y + (firstNormalY + secondNormalY) / normals * halfWidth;
                        AppendStrokePiece(out, { first, SvgPoint{ tipX, tipY }, second, corner });
                        return;
                    }
                }
            }

            AppendStrokePiece(out, { first, second, corner });
        }

        /// Turns a stroked contour into the pieces one stroke of it covers: a
        /// rectangle per segment, a wedge at every corner, and the requested cap
        /// at each end of an open contour. `closed` says whether the contour
        /// joins its last point back to its first, which is the difference
        /// between a `Z` and a path that simply stops.
        void AppendStrokePolygons(const std::vector<SvgPoint>& contour,
                                  bool closed,
                                  float halfWidth,
                                  const SvgStrokeStyle& style,
                                  std::vector<std::vector<SvgPoint>>& out)
        {
            const std::size_t count = contour.size();
            if (count < 2 || halfWidth <= 0.0f)
            {
                return;
            }

            const bool closedContour = closed && count >= 3;
            const std::size_t segments = closedContour ? count : count - 1;

            for (std::size_t index = 0; index < segments; ++index)
            {
                AppendStrokeSegment(out, contour[index], contour[(index + 1) % count], halfWidth);
            }

            const auto unitDirection = [](const SvgPoint& from, const SvgPoint& to, float& x, float& y) {
                const float dx = to.x - from.x;
                const float dy = to.y - from.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length <= 0.0f)
                {
                    return false;
                }

                x = dx / length;
                y = dy / length;
                return true;
            };

            // A closed contour meets itself at every vertex; an open one has
            // interior corners only, because its two ends are capped.
            const std::size_t joinBegin = closedContour ? 0 : 1;
            const std::size_t joinEnd = closedContour ? count : count - 1;

            for (std::size_t index = joinBegin; index < joinEnd; ++index)
            {
                const SvgPoint& previous = contour[(index + count - 1) % count];
                const SvgPoint& corner = contour[index];
                const SvgPoint& next = contour[(index + 1) % count];

                float incomingX = 0.0f;
                float incomingY = 0.0f;
                float outgoingX = 0.0f;
                float outgoingY = 0.0f;
                if (!unitDirection(previous, corner, incomingX, incomingY) ||
                    !unitDirection(corner, next, outgoingX, outgoingY))
                {
                    continue;
                }

                AppendStrokeJoin(out, corner, incomingX, incomingY, outgoingX, outgoingY, halfWidth,
                                 style);
            }

            if (!closedContour)
            {
                float outwardX = 0.0f;
                float outwardY = 0.0f;
                if (unitDirection(contour[count - 2], contour[count - 1], outwardX, outwardY))
                {
                    AppendStrokeCap(out, contour[count - 1], outwardX, outwardY, halfWidth, style);
                }
                if (unitDirection(contour[1], contour[0], outwardX, outwardY))
                {
                    AppendStrokeCap(out, contour[0], outwardX, outwardY, halfWidth, style);
                }
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
                // The stroke width is in user units, so it has to be scaled like
                // the geometry it outlines. A floor of half a pixel keeps a
                // hairline visible in a downscaled raster.
                const float halfWidth = std::max(0.5f, shape.strokeWidth * scale * 0.5f);
                for (std::size_t index = 0; index < shape.contours.size(); ++index)
                {
                    std::vector<SvgPoint> device = toDevice(shape.contours[index]);
                    if (device.size() < 2)
                    {
                        continue;
                    }

                    // Closure is per contour: a document may hold a closed
                    // outline and an open one in the same shape.
                    const bool closed =
                        index < shape.contourClosed.size() ? shape.contourClosed[index] : true;
                    AppendStrokePolygons(device, closed, halfWidth, shape.strokeStyle, strokePolygons);
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
