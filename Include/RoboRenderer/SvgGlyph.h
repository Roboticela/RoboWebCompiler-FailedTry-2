// ---------------------------------------------------------------------------
// RoboRenderer - `SVG ` colour glyphs.
//
// An OpenType `SVG ` glyph is a small, self-contained SVG 1.1 document, not an
// outline. Drawing one needs a renderer, and this is the smallest honest one:
// the shapes a colour font actually uses (paths and the basic shapes), grouped
// with affine transforms, filled with a solid colour or a linear/radial
// gradient. Filters, patterns, masks, images and CSS styling are outside what a
// font may rely on and are skipped rather than guessed at; an element this
// reader does not understand contributes nothing, and a document it cannot read
// at all reports false so the caller falls back to the glyph's outline.
//
// SVG draws y downwards from the glyph origin; font units grow y upwards, so the
// reader flips the axis and hands the caller ordinary font-unit contours.
// ---------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace RoboRenderer
{
    /// A point in font units, y upwards from the baseline.
    struct SvgPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    /// How one SVG shape is painted.
    struct SvgPaint
    {
        enum class Kind : std::uint8_t
        {
            Solid = 0,
            Linear,
            Radial
        };

        struct Stop
        {
            float offset = 0.0f;
            float red = 0.0f;
            float green = 0.0f;
            float blue = 0.0f;
            float alpha = 1.0f;
        };

        Kind kind = Kind::Solid;
        /// Solid colour, each channel 0..1.
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        float alpha = 1.0f;

        /// Linear: `(x0,y0)` to `(x1,y1)`. Radial: circle `(x0,y0,r0)` to
        /// `(x1,y1,r1)`. All in font units, the same space as the contours.
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float r0 = 0.0f;
        float r1 = 0.0f;

        std::vector<Stop> stops;
    };

    /// One filled shape: closed contours in font units, and its paint.
    struct SvgShape
    {
        std::vector<std::vector<SvgPoint>> contours;
        SvgPaint paint;
    };

    /// Renders an SVG glyph document into filled shapes. Returns false when the
    /// bytes are not a document this reader can draw at all, in which case the
    /// caller keeps the glyph's outline.
    bool RenderSvgGlyph(const std::uint8_t* data, std::size_t size, std::vector<SvgShape>& out);

    /// Applies a `fill` value the same way `RenderSvgGlyph` does, exposed so a
    /// test can pin the colour parsing without a font. Returns false for `none`
    /// and for a value the reader does not understand.
    bool ParseSvgFill(const char* value, SvgPaint& out);
}
