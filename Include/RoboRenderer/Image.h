// ---------------------------------------------------------------------------
// RoboRenderer - images and image codecs.
//
// The runtime never hands a raw encoded file to the graphics backend. It decodes
// once, into a tightly packed surface the backend can upload or blend directly,
// and keeps it alive for as long as the compiled resource that named it lives.
//
// The codecs are self contained on purpose: PNG and BMP are the formats the
// compiled examples use and PPM is what the visual regression tests write, and
// none of the three is worth a third party dependency in a runtime whose whole
// point is not to drag a platform stack along.
// ---------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RoboRenderer
{
    enum class PixelFormat : std::uint8_t
    {
        /// 8 bits per channel, red first: the format the backends and the PNG
        /// encoder both use, so no conversion happens on the common path.
        Rgba8 = 0,
        /// Premultiplied RGBA8, which is what a compositor wants.
        Rgba8Premultiplied = 1,
        Bgra8 = 2,
        Gray8 = 3
    };

    const char* ToString(PixelFormat format);

    /// A decoded image surface. Rows are packed top down.
    struct Image
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        PixelFormat format = PixelFormat::Rgba8;
        std::vector<std::uint8_t> pixels;

        bool IsValid() const
        {
            return width > 0 && height > 0 && pixels.size() >= Stride() * height;
        }

        std::size_t BytesPerPixel() const
        {
            switch (format)
            {
            case PixelFormat::Gray8: return 1;
            case PixelFormat::Bgra8:
            case PixelFormat::Rgba8:
            case PixelFormat::Rgba8Premultiplied: return 4;
            }

            return 4;
        }

        std::size_t Stride() const { return static_cast<std::size_t>(width) * BytesPerPixel(); }

        /// Suspends the image: the returned pointer is nullptr for a coordinate
        /// outside it, which is what a sampler needs.
        const std::uint8_t* Sample(std::uint32_t x, std::uint32_t y) const
        {
            if (x >= width || y >= height)
            {
                return nullptr;
            }

            const std::size_t offset = static_cast<std::size_t>(y) * Stride() + static_cast<std::size_t>(x) * BytesPerPixel();
            return pixels.data() + offset;
        }

        std::uint8_t* MutablePixel(std::uint32_t x, std::uint32_t y)
        {
            if (x >= width || y >= height)
            {
                return nullptr;
            }

            const std::size_t offset =
                static_cast<std::size_t>(y) * Stride() + static_cast<std::size_t>(x) * BytesPerPixel();
            return pixels.data() + offset;
        }

        void Allocate(std::uint32_t newWidth, std::uint32_t newHeight, PixelFormat newFormat)
        {
            width = newWidth;
            height = newHeight;
            format = newFormat;
            pixels.assign(static_cast<std::size_t>(newWidth) * newHeight * BytesPerPixel(), 0);
        }

        void Clear(const std::uint8_t* rgba)
        {
            for (std::uint32_t y = 0; y < height; ++y)
            {
                for (std::uint32_t x = 0; x < width; ++x)
                {
                    std::uint8_t* pixel = MutablePixel(x, y);
                    if (pixel == nullptr)
                    {
                        continue;
                    }

                    for (std::size_t channel = 0; channel < BytesPerPixel(); ++channel)
                    {
                        pixel[channel] = rgba[channel];
                    }
                }
            }
        }
    };

    /// What a decode produced, so a diagnostic can say why a resource failed
    /// instead of leaving a hole in the window.
    struct ImageDecodeResult
    {
        bool success = false;
        /// File format the bytes turned out to be, or "unknown".
        const char* format = "unknown";
        /// Byte offset of the first byte the codec rejected.
        std::size_t offset = 0;
        std::string message;
    };

    /// Inflates a raw DEFLATE stream (no zlib or gzip wrapper). Used by the `SVG`
    /// glyph reader, whose documents may be stored deflated.
    bool InflateDeflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out);

    /// Decompresses an RFC 1952 gzip stream into `out`. Returns false when the
    /// bytes are not a usable gzip stream; an empty result is a valid empty
    /// document.
    bool Gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out);

    /// Decodes an encoded image, choosing the codec from the bytes themselves.
    ImageDecodeResult DecodeImage(const std::uint8_t* data, std::size_t size, Image& out);

    /// Reads and decodes a file. `path` is relative to the process working
    /// directory unless it is absolute.
    ImageDecodeResult DecodeImageFile(const char* path, Image& out);

    /// Encodes an RGBA8 image as a PNG. The result is a complete file.
    std::vector<std::uint8_t> EncodePng(const Image& image);

    /// Writes an image as PNG. Returns false when the file cannot be written.
    bool WritePngFile(const char* path, const Image& image);

    /// Writes an image as a binary PPM, the format the visual tests compare.
    bool WritePpmFile(const char* path, const Image& image);

    /// A flat colour swatch, used as the placeholder for a resource that could
    /// not be loaded: a missing image is visible, and never a crash.
    Image MakeSolidImage(std::uint32_t width, std::uint32_t height, std::uint8_t red, std::uint8_t green,
                         std::uint8_t blue, std::uint8_t alpha = 255);

    /// The checkerboard a transparent image is shown against in tests.
    Image MakeCheckerboardImage(std::uint32_t width, std::uint32_t height, std::uint32_t cell = 8);

    /// A placeholder that reads as "image did not load": a hatched block with a
    /// border, so a broken resource is obvious rather than invisible.
    Image MakePlaceholderImage(std::uint32_t width, std::uint32_t height);
}
