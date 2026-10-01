#include "RoboRenderer/Backend.h"

#include <algorithm>
#include <cmath>

namespace RoboRenderer
{
    namespace
    {
        std::uint8_t FloatToByte(float value)
        {
            if (value <= 0.0f)
            {
                return 0;
            }
            if (value >= 255.0f)
            {
                return 255;
            }

            return static_cast<std::uint8_t>(value + 0.5f);
        }

        float ClampUnit(float value)
        {
            return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        }

        /// The signed distance from a point to a rounded rectangle, in device
        /// pixels. Zero is the outline, negative is inside - which is exactly
        /// what an anti-aliased edge needs, one formula for the fill and the
        /// stroke of every rounded shape.
        float RoundedDistance(float pointX,
                              float pointY,
                              float left,
                              float top,
                              float right,
                              float bottom,
                              float radius)
        {
            const float halfWidth = (right - left) * 0.5f;
            const float halfHeight = (bottom - top) * 0.5f;
            const float centerX = left + halfWidth;
            const float centerY = top + halfHeight;

            const float boxRadius = std::min(radius, std::min(halfWidth, halfHeight));
            const float innerX = halfWidth - boxRadius;
            const float innerY = halfHeight - boxRadius;

            const float distanceX = std::fabs(pointX - centerX) - innerX;
            const float distanceY = std::fabs(pointY - centerY) - innerY;

            const float outsideX = std::max(distanceX, 0.0f);
            const float outsideY = std::max(distanceY, 0.0f);
            const float outside = std::sqrt(outsideX * outsideX + outsideY * outsideY);
            const float inside = std::min(std::max(distanceX, distanceY), 0.0f);

            return outside + inside - boxRadius;
        }
    }

    SoftwareBackend::SoftwareBackend()
    {
        target.logicalWidth = 0.0f;
        target.logicalHeight = 0.0f;
        target.deviceScale = 1.0f;
    }

    bool SoftwareBackend::Initialize(const RenderTargetInfo& newTarget)
    {
        target = newTarget;

        if (target.deviceScale <= 0.0f)
        {
            lastError = "device scale must be positive";
            return false;
        }

        const std::uint32_t width = target.DeviceWidth();
        const std::uint32_t height = target.DeviceHeight();

        if (width == 0 || height == 0)
        {
            lastError = "render target has no size";
            return false;
        }

        framebuffer.Allocate(width, height, PixelFormat::Rgba8);
        Clear(clearColor);

        initialized = true;
        lastError = "no error";
        return true;
    }

    void SoftwareBackend::Shutdown()
    {
        initialized = false;
        frameOpen = false;
        clipStack.clear();
        transformStack.clear();
        framebuffer = Image{};
    }

    bool SoftwareBackend::Resize(const RenderTargetInfo& newTarget)
    {
        if (newTarget.logicalWidth <= 0.0f || newTarget.logicalHeight <= 0.0f || newTarget.deviceScale <= 0.0f)
        {
            lastError = "render target size is invalid";
            return false;
        }

        target = newTarget;
        framebuffer.Allocate(target.DeviceWidth(), target.DeviceHeight(), PixelFormat::Rgba8);

        // A resize invalidates every pixel: the background is re-established and
        // the next frame repaints the whole target.
        Clear(clearColor);
        lastError = "no error";
        return true;
    }

    void SoftwareBackend::Clear(const Color& color)
    {
        if (!framebuffer.IsValid())
        {
            return;
        }

        clipStack.clear();
        transformStack.clear();

        for (std::uint32_t y = 0; y < framebuffer.height; ++y)
        {
            std::uint8_t* row = framebuffer.MutablePixel(0, y);

            for (std::uint32_t x = 0; x < framebuffer.width; ++x)
            {
                std::uint8_t* pixel = row + static_cast<std::size_t>(x) * 4;
                pixel[0] = color.red;
                pixel[1] = color.green;
                pixel[2] = color.blue;
                pixel[3] = 255;
            }
        }
    }

    bool SoftwareBackend::BeginFrame(const Rect& damage, const FrameContext& context)
    {
        if (!initialized)
        {
            lastError = "backend is not initialized";
            return false;
        }

        frameContext = context;
        damageRegion = damage;
        drawnPixels = 0;
        executedCommands = 0;
        frameOpen = true;

        clipStack.clear();
        transformStack.clear();

        // Only the damaged region is repainted. Everything outside it keeps the
        // pixels the previous frame left, which is what makes an incremental
        // frame cheap instead of a full redraw.
        FillRectangle(DamageOrWholeTarget(), clearColor);
        clipStack.clear();

        return true;
    }

    bool SoftwareBackend::EndFrame()
    {
        if (!frameOpen)
        {
            lastError = "no frame is open";
            return false;
        }

        frameOpen = false;
        ++frameNumber;
        lastError = "no error";
        return true;
    }

    std::uint32_t SoftwareBackend::FrameNumber() const
    {
        return frameNumber;
    }

    SoftwareBackend::ClipRegion SoftwareBackend::ToDevice(const Rect& logical) const
    {
        ClipRegion region;

        const float left = (logical.x + TransformX()) * target.deviceScale;
        const float top = (logical.y + TransformY()) * target.deviceScale;
        const float right = (logical.Right() + TransformX()) * target.deviceScale;
        const float bottom = (logical.Bottom() + TransformY()) * target.deviceScale;

        region.left = static_cast<std::int32_t>(std::floor(left - 0.5f));
        region.top = static_cast<std::int32_t>(std::floor(top - 0.5f));
        region.right = static_cast<std::int32_t>(std::ceil(right + 0.5f));
        region.bottom = static_cast<std::int32_t>(std::ceil(bottom + 0.5f));

        return region;
    }

    std::int32_t SoftwareBackend::DeviceX(float logicalX) const
    {
        return static_cast<std::int32_t>(std::floor((logicalX + TransformX()) * target.deviceScale + 0.5f));
    }

    std::int32_t SoftwareBackend::DeviceY(float logicalY) const
    {
        return static_cast<std::int32_t>(std::floor((logicalY + TransformY()) * target.deviceScale + 0.5f));
    }

    float SoftwareBackend::LogicalX(float deviceX) const
    {
        return deviceX / target.deviceScale - TransformX();
    }

    float SoftwareBackend::LogicalY(float deviceY) const
    {
        return deviceY / target.deviceScale - TransformY();
    }

    float SoftwareBackend::TransformX() const
    {
        return transformStack.empty() ? 0.0f : transformStack.back().first;
    }

    float SoftwareBackend::TransformY() const
    {
        return transformStack.empty() ? 0.0f : transformStack.back().second;
    }

    Rect SoftwareBackend::DamageOrWholeTarget() const
    {
        if (damageRegion.IsEmpty())
        {
            return Rect{ 0.0f, 0.0f, target.logicalWidth, target.logicalHeight };
        }

        return damageRegion.Intersection(Rect{ 0.0f, 0.0f, target.logicalWidth, target.logicalHeight });
    }

    SoftwareBackend::ClipRegion SoftwareBackend::CurrentClip() const
    {
        if (clipStack.empty())
        {
            ClipRegion full;
            full.left = 0;
            full.top = 0;
            full.right = static_cast<std::int32_t>(framebuffer.width);
            full.bottom = static_cast<std::int32_t>(framebuffer.height);
            return full;
        }

        return clipStack.back();
    }

    void SoftwareBackend::Execute(const PaintCommand& command)
    {
        ++executedCommands;

        switch (command.type)
        {
        case PaintCommandType::FillRect:
            FillRectangle(command.bounds, command.color);
            break;

        case PaintCommandType::FillRoundedRect:
            FillRoundedRectangle(command.bounds, command.radius, command.color);
            break;

        case PaintCommandType::StrokeRect:
            StrokeRectangle(command.bounds, command.edges, command.color);
            break;

        case PaintCommandType::StrokeRoundedRect:
            StrokeRoundedRectangle(command.bounds, command.radius, command.edges, command.color);
            break;

        case PaintCommandType::DrawImage:
            DrawImage(command.bounds, command.resource);
            break;

        case PaintCommandType::DrawText:
            DrawText(command);
            break;

        case PaintCommandType::PushClip:
        {
            ClipRegion region = ToDevice(command.bounds);
            const ClipRegion current = CurrentClip();

            region.left = std::max(region.left, current.left);
            region.top = std::max(region.top, current.top);
            region.right = std::min(region.right, current.right);
            region.bottom = std::min(region.bottom, current.bottom);
            clipStack.push_back(region);
            break;
        }

        case PaintCommandType::PopClip:
            if (!clipStack.empty())
            {
                clipStack.pop_back();
            }
            break;

        case PaintCommandType::PushTransform:
        {
            const float offsetX = TransformX() + command.bounds.x;
            const float offsetY = TransformY() + command.bounds.y;
            transformStack.emplace_back(offsetX, offsetY);
            break;
        }

        case PaintCommandType::PopTransform:
            if (!transformStack.empty())
            {
                transformStack.pop_back();
            }
            break;
        }
    }

    void SoftwareBackend::BlendPixel(std::int32_t x, std::int32_t y, const Color& color, float coverage)
    {
        if (coverage <= 0.0f || x < 0 || y < 0 || x >= static_cast<std::int32_t>(framebuffer.width) ||
            y >= static_cast<std::int32_t>(framebuffer.height))
        {
            return;
        }

        const float alpha = (static_cast<float>(color.alpha) / 255.0f) * coverage;
        if (alpha <= 0.0f)
        {
            return;
        }

        std::uint8_t* pixel = framebuffer.MutablePixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
        const float inverse = 1.0f - alpha;

        pixel[0] = FloatToByte(static_cast<float>(color.red) * alpha + static_cast<float>(pixel[0]) * inverse);
        pixel[1] = FloatToByte(static_cast<float>(color.green) * alpha + static_cast<float>(pixel[1]) * inverse);
        pixel[2] = FloatToByte(static_cast<float>(color.blue) * alpha + static_cast<float>(pixel[2]) * inverse);
        pixel[3] = 255;

        ++drawnPixels;
    }

    void SoftwareBackend::FillRectPrecise(float left, float top, float right, float bottom, const Color& color)
    {
        const ClipRegion clip = CurrentClip();
        if (clip.IsEmpty() || right <= left || bottom <= top)
        {
            return;
        }

        const std::int32_t firstX = std::max(clip.left, static_cast<std::int32_t>(std::floor(left)));
        const std::int32_t lastX = std::min(clip.right, static_cast<std::int32_t>(std::ceil(right)));
        const std::int32_t firstY = std::max(clip.top, static_cast<std::int32_t>(std::floor(top)));
        const std::int32_t lastY = std::min(clip.bottom, static_cast<std::int32_t>(std::ceil(bottom)));

        // A fully covered opaque pixel takes the same value whether it is
        // blended or written, so the interior of an axis aligned opaque
        // rectangle is written directly. Backgrounds and panel fills are almost
        // always exactly that, and the alternative is a blend per pixel for the
        // whole target: this is what keeps a full repaint cheap.
        const bool opaque = color.alpha == 255;

        for (std::int32_t y = firstY; y < lastY; ++y)
        {
            const float coverageY = std::min(bottom, static_cast<float>(y) + 1.0f) -
                                    std::max(top, static_cast<float>(y));
            if (coverageY <= 0.0f)
            {
                continue;
            }

            const bool coveredRow = coverageY >= 1.0f;

            std::int32_t spanBegin = firstX;
            std::int32_t spanEnd = firstX;

            if (opaque && coveredRow)
            {
                spanBegin = std::max(firstX, static_cast<std::int32_t>(std::ceil(left)));
                spanEnd = std::min(lastX, static_cast<std::int32_t>(std::floor(right)));

                if (spanEnd > spanBegin)
                {
                    std::uint8_t* row = framebuffer.MutablePixel(static_cast<std::uint32_t>(spanBegin),
                                                                static_cast<std::uint32_t>(y));

                    if (row != nullptr)
                    {
                        for (std::int32_t x = spanBegin; x < spanEnd; ++x)
                        {
                            row[0] = color.red;
                            row[1] = color.green;
                            row[2] = color.blue;
                            row[3] = 255;
                            row += 4;
                        }

                        drawnPixels += static_cast<std::uint64_t>(spanEnd - spanBegin);
                    }
                }

                // A rectangle narrower than one pixel has no interior: leave the
                // span empty so the two partial passes meet where they should.
                if (spanEnd < spanBegin)
                {
                    spanEnd = spanBegin;
                }
            }

            for (std::int32_t x = firstX; x < spanBegin; ++x)
            {
                const float coverageX =
                    std::min(right, static_cast<float>(x) + 1.0f) - std::max(left, static_cast<float>(x));
                if (coverageX <= 0.0f)
                {
                    continue;
                }

                BlendPixel(x, y, color, ClampUnit(coverageX) * ClampUnit(coverageY));
            }

            for (std::int32_t x = spanEnd; x < lastX; ++x)
            {
                const float coverageX =
                    std::min(right, static_cast<float>(x) + 1.0f) - std::max(left, static_cast<float>(x));
                if (coverageX <= 0.0f)
                {
                    continue;
                }

                BlendPixel(x, y, color, ClampUnit(coverageX) * ClampUnit(coverageY));
            }
        }
    }

    void SoftwareBackend::FillRectangle(const Rect& logicalBounds, const Color& color)
    {
        const float scale = target.deviceScale;
        const float left = (logicalBounds.x + TransformX()) * scale;
        const float top = (logicalBounds.y + TransformY()) * scale;
        const float right = (logicalBounds.Right() + TransformX()) * scale;
        const float bottom = (logicalBounds.Bottom() + TransformY()) * scale;

        FillRectPrecise(left, top, right, bottom, color);
    }

    void SoftwareBackend::FillRoundedRectangle(const Rect& logicalBounds, float radius, const Color& color)
    {
        const float scale = target.deviceScale;
        const float left = (logicalBounds.x + TransformX()) * scale;
        const float top = (logicalBounds.y + TransformY()) * scale;
        const float right = (logicalBounds.Right() + TransformX()) * scale;
        const float bottom = (logicalBounds.Bottom() + TransformY()) * scale;

        if (radius <= 0.25f)
        {
            FillRectPrecise(left, top, right, bottom, color);
            return;
        }

        const ClipRegion clip = CurrentClip();
        if (clip.IsEmpty())
        {
            return;
        }

        const float deviceRadius = radius * scale;

        const std::int32_t firstX = std::max(clip.left, static_cast<std::int32_t>(std::floor(left - 1.0f)));
        const std::int32_t lastX = std::min(clip.right, static_cast<std::int32_t>(std::ceil(right + 1.0f)));
        const std::int32_t firstY = std::max(clip.top, static_cast<std::int32_t>(std::floor(top - 1.0f)));
        const std::int32_t lastY = std::min(clip.bottom, static_cast<std::int32_t>(std::ceil(bottom + 1.0f)));

        for (std::int32_t y = firstY; y < lastY; ++y)
        {
            const float pixelY = static_cast<float>(y) + 0.5f;

            for (std::int32_t x = firstX; x < lastX; ++x)
            {
                const float pixelX = static_cast<float>(x) + 0.5f;
                const float distance =
                    RoundedDistance(pixelX, pixelY, left, top, right, bottom, deviceRadius);
                const float coverage = ClampUnit(0.5f - distance);

                if (coverage > 0.0f)
                {
                    BlendPixel(x, y, color, coverage);
                }
            }
        }
    }

    void SoftwareBackend::StrokeRectangle(const Rect& logicalBounds, const Edges& edges, const Color& color)
    {
        // Four edge rectangles, so a cell can draw only the sides its resolved
        // border asks for - and a zero-width edge costs nothing.
        if (edges.top > 0.0f)
        {
            FillRectangle(Rect{ logicalBounds.x, logicalBounds.y, logicalBounds.width, edges.top }, color);
        }

        if (edges.bottom > 0.0f)
        {
            FillRectangle(Rect{ logicalBounds.x, logicalBounds.Bottom() - edges.bottom, logicalBounds.width,
                                edges.bottom },
                          color);
        }

        if (edges.left > 0.0f)
        {
            FillRectangle(Rect{ logicalBounds.x, logicalBounds.y, edges.left, logicalBounds.height }, color);
        }

        if (edges.right > 0.0f)
        {
            FillRectangle(Rect{ logicalBounds.Right() - edges.right, logicalBounds.y, edges.right,
                                logicalBounds.height },
                          color);
        }
    }

    void SoftwareBackend::StrokeRoundedRectangle(const Rect& logicalBounds,
                                                 float radius,
                                                 const Edges& edges,
                                                 const Color& color)
    {
        if (edges.top <= 0.0f && edges.right <= 0.0f && edges.bottom <= 0.0f && edges.left <= 0.0f)
        {
            return;
        }

        const float scale = target.deviceScale;
        const float width = (edges.top + edges.right + edges.bottom + edges.left) * 0.25f * scale;

        if (radius <= 0.25f || width <= 0.25f)
        {
            StrokeRectangle(logicalBounds, edges, color);
            return;
        }

        const float left = (logicalBounds.x + TransformX()) * scale;
        const float top = (logicalBounds.y + TransformY()) * scale;
        const float right = (logicalBounds.Right() + TransformX()) * scale;
        const float bottom = (logicalBounds.Bottom() + TransformY()) * scale;
        const float deviceRadius = radius * scale;

        const ClipRegion clip = CurrentClip();
        if (clip.IsEmpty())
        {
            return;
        }

        const std::int32_t firstX = std::max(clip.left, static_cast<std::int32_t>(std::floor(left - 1.0f)));
        const std::int32_t lastX = std::min(clip.right, static_cast<std::int32_t>(std::ceil(right + 1.0f)));
        const std::int32_t firstY = std::max(clip.top, static_cast<std::int32_t>(std::floor(top - 1.0f)));
        const std::int32_t lastY = std::min(clip.bottom, static_cast<std::int32_t>(std::ceil(bottom + 1.0f)));

        for (std::int32_t y = firstY; y < lastY; ++y)
        {
            const float pixelY = static_cast<float>(y) + 0.5f;

            for (std::int32_t x = firstX; x < lastX; ++x)
            {
                const float pixelX = static_cast<float>(x) + 0.5f;

                const float outer = RoundedDistance(pixelX, pixelY, left, top, right, bottom, deviceRadius);
                const float inner = RoundedDistance(pixelX, pixelY, left + width, top + width, right - width,
                                                    bottom - width, std::max(0.0f, deviceRadius - width));

                const float outerCoverage = ClampUnit(0.5f - outer);
                const float innerCoverage = ClampUnit(0.5f - inner);
                const float coverage = ClampUnit(outerCoverage - innerCoverage);

                if (coverage > 0.0f)
                {
                    BlendPixel(x, y, color, coverage);
                }
            }
        }
    }

    void SoftwareBackend::DrawImage(const Rect& logicalBounds, ResourceIndex resource)
    {
        if (frameContext.resources == nullptr || logicalBounds.IsEmpty())
        {
            return;
        }

        const Image& image = frameContext.resources->ImageFor(resource);
        if (!image.IsValid() || image.format != PixelFormat::Rgba8)
        {
            return;
        }

        const float scale = target.deviceScale;
        const float left = (logicalBounds.x + TransformX()) * scale;
        const float top = (logicalBounds.y + TransformY()) * scale;
        const float right = (logicalBounds.Right() + TransformX()) * scale;
        const float bottom = (logicalBounds.Bottom() + TransformY()) * scale;

        const ClipRegion clip = CurrentClip();
        if (clip.IsEmpty() || right <= left || bottom <= top)
        {
            return;
        }

        const std::int32_t firstX = std::max(clip.left, static_cast<std::int32_t>(std::floor(left)));
        const std::int32_t lastX = std::min(clip.right, static_cast<std::int32_t>(std::ceil(right)));
        const std::int32_t firstY = std::max(clip.top, static_cast<std::int32_t>(std::floor(top)));
        const std::int32_t lastY = std::min(clip.bottom, static_cast<std::int32_t>(std::ceil(bottom)));

        const float sourceWidth = static_cast<float>(image.width);
        const float sourceHeight = static_cast<float>(image.height);
        const float unitX = sourceWidth / (right - left);
        const float unitY = sourceHeight / (bottom - top);

        for (std::int32_t y = firstY; y < lastY; ++y)
        {
            const float sampleY = (static_cast<float>(y) + 0.5f - top) * unitY - 0.5f;

            for (std::int32_t x = firstX; x < lastX; ++x)
            {
                const float sampleX = (static_cast<float>(x) + 0.5f - left) * unitX - 0.5f;

                // Bilinear sampling with clamped edges: an image scaled up reads
                // as smooth rather than as blocks, and one scaled down keeps the
                // detail it can.
                const float clampedX = sampleX < 0.0f ? 0.0f : (sampleX > sourceWidth - 1.0f ? sourceWidth - 1.0f : sampleX);
                const float clampedY = sampleY < 0.0f ? 0.0f : (sampleY > sourceHeight - 1.0f ? sourceHeight - 1.0f : sampleY);

                const std::uint32_t x0 = static_cast<std::uint32_t>(clampedX);
                const std::uint32_t y0 = static_cast<std::uint32_t>(clampedY);
                const std::uint32_t x1 = std::min(x0 + 1u, image.width - 1u);
                const std::uint32_t y1 = std::min(y0 + 1u, image.height - 1u);

                const float fractionX = clampedX - static_cast<float>(x0);
                const float fractionY = clampedY - static_cast<float>(y0);

                const std::uint8_t* p00 = image.Sample(x0, y0);
                const std::uint8_t* p10 = image.Sample(x1, y0);
                const std::uint8_t* p01 = image.Sample(x0, y1);
                const std::uint8_t* p11 = image.Sample(x1, y1);

                float channels[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

                for (int channel = 0; channel < 4; ++channel)
                {
                    const float topValue = static_cast<float>(p00[channel]) * (1.0f - fractionX) +
                                           static_cast<float>(p10[channel]) * fractionX;
                    const float bottomValue = static_cast<float>(p01[channel]) * (1.0f - fractionX) +
                                              static_cast<float>(p11[channel]) * fractionX;
                    channels[channel] = topValue * (1.0f - fractionY) + bottomValue * fractionY;
                }

                const Color color{ FloatToByte(channels[0]), FloatToByte(channels[1]), FloatToByte(channels[2]),
                                   FloatToByte(channels[3]) };

                BlendPixel(x, y, color, 1.0f);
            }
        }
    }

    void SoftwareBackend::DrawText(const PaintCommand& command)
    {
        if (frameContext.glyphs == nullptr || frameContext.commands == nullptr)
        {
            return;
        }

        const std::vector<PaintGlyph>& glyphs = frameContext.commands->Glyphs();
        const float scale = target.deviceScale;

        for (std::uint32_t index = 0; index < command.glyphCount; ++index)
        {
            const std::size_t glyphIndex = static_cast<std::size_t>(command.firstGlyph) + index;
            if (glyphIndex >= glyphs.size())
            {
                break;
            }

            const PaintGlyph& glyph = glyphs[glyphIndex];

            // A shaped glyph is fetched by its font glyph id; a run the face
            // could not shape (or the built-in stroke font) falls back to the
            // code point.
            const RasterizedGlyph rasterized =
                glyph.glyphId != 0
                    ? frameContext.glyphs->GetShaped(glyph.faceId, glyph.glyphId, command.fontSize,
                                                     command.weight, scale)
                    : frameContext.glyphs->Get(glyph.codepoint, command.fontSize, command.weight, scale);

            if (rasterized.bitmap == nullptr ||
                (rasterized.coverage == nullptr && rasterized.color == nullptr))
            {
                continue;
            }

            const GlyphBitmap& bitmap = *rasterized.bitmap;

            const float penX = (glyph.x + TransformX()) * scale;
            const float baselineY = (glyph.y + TransformY()) * scale;

            const float bitmapLeft = penX + bitmap.bearingX;
            const float bitmapTop = baselineY + bitmap.bearingY;

            const ClipRegion clip = CurrentClip();
            if (clip.IsEmpty())
            {
                continue;
            }

            const std::int32_t sourceLeft = static_cast<std::int32_t>(std::floor(bitmapLeft));
            const std::int32_t sourceTop = static_cast<std::int32_t>(std::floor(bitmapTop));
            const std::int32_t sourceRight = sourceLeft + static_cast<std::int32_t>(bitmap.width);
            const std::int32_t sourceBottom = sourceTop + static_cast<std::int32_t>(bitmap.height);

            // Blends one device pixel sampled from one bitmap pixel. The source
            // address is in device space; the caller maps it, which is what lets
            // a turned glyph reuse this sampler unchanged.
            const auto blendSource = [&](std::int32_t deviceX, std::int32_t deviceY, std::int32_t sourceX,
                                         std::int32_t sourceY) {
                const std::int32_t bitmapX = sourceX - sourceLeft;
                const std::int32_t bitmapY = sourceY - sourceTop;

                if (bitmapX < 0 || bitmapX >= static_cast<std::int32_t>(bitmap.width) || bitmapY < 0 ||
                    bitmapY >= static_cast<std::int32_t>(bitmap.height))
                {
                    return;
                }

                if (rasterized.color != nullptr)
                {
                    // A colour glyph brings its own RGBA pixels; the command's
                    // colour is ignored.
                    const std::uint8_t* source =
                        rasterized.color +
                        (static_cast<std::size_t>(bitmapY) * bitmap.width + static_cast<std::size_t>(bitmapX)) * 4;

                    if (source[3] == 0)
                    {
                        return;
                    }

                    BlendPixel(deviceX, deviceY, Color{ source[0], source[1], source[2], source[3] }, 1.0f);
                    return;
                }

                const std::uint8_t coverage =
                    rasterized.coverage[static_cast<std::size_t>(bitmapY) * bitmap.width +
                                         static_cast<std::size_t>(bitmapX)];

                if (coverage == 0)
                {
                    return;
                }

                BlendPixel(deviceX, deviceY, command.color, static_cast<float>(coverage) / 255.0f);
            };

            if (glyph.rotation != 0)
            {
                // A quarter turn clockwise about the glyph's own pen position:
                // a source vector (dx, dy) becomes (-dy, dx), so the advance runs
                // down the page and the glyph's top faces right. The output box
                // is walked and each pixel mapped back into the bitmap, which
                // leaves no holes a forward map or a transpose would.
                const float outLeftF = penX - (static_cast<float>(sourceBottom) - baselineY);
                const float outRightF = penX - (static_cast<float>(sourceTop) - baselineY);
                const float outTopF = baselineY + (static_cast<float>(sourceLeft) - penX);
                const float outBottomF = baselineY + (static_cast<float>(sourceRight) - penX);

                const std::int32_t firstX = std::max(clip.left, static_cast<std::int32_t>(std::floor(outLeftF)));
                const std::int32_t lastX = std::min(clip.right, static_cast<std::int32_t>(std::ceil(outRightF)));
                const std::int32_t firstY = std::max(clip.top, static_cast<std::int32_t>(std::floor(outTopF)));
                const std::int32_t lastY = std::min(clip.bottom, static_cast<std::int32_t>(std::ceil(outBottomF)));

                for (std::int32_t y = firstY; y < lastY; ++y)
                {
                    for (std::int32_t x = firstX; x < lastX; ++x)
                    {
                        const float relX = static_cast<float>(x) - penX;
                        const float relY = static_cast<float>(y) - baselineY;
                        const std::int32_t sourceX = static_cast<std::int32_t>(std::floor(penX + relY));
                        const std::int32_t sourceY = static_cast<std::int32_t>(std::floor(baselineY - relX));
                        blendSource(x, y, sourceX, sourceY);
                    }
                }

                continue;
            }

            const std::int32_t firstX = std::max(clip.left, sourceLeft);
            const std::int32_t lastX = std::min(clip.right, sourceRight);
            const std::int32_t firstY = std::max(clip.top, sourceTop);
            const std::int32_t lastY = std::min(clip.bottom, sourceBottom);

            for (std::int32_t y = firstY; y < lastY; ++y)
            {
                for (std::int32_t x = firstX; x < lastX; ++x)
                {
                    blendSource(x, y, x, y);
                }
            }
        }
    }

    Color SoftwareBackend::ReadPixel(std::uint32_t x, std::uint32_t y) const
    {
        const std::uint8_t* pixel = framebuffer.Sample(x, y);
        if (pixel == nullptr)
        {
            return Color{ 0, 0, 0, 0 };
        }

        return Color{ pixel[0], pixel[1], pixel[2], pixel[3] };
    }
}
