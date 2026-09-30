// ---------------------------------------------------------------------------
// RoboRenderer - text.
//
// Text is a native subsystem, not a set of fixed character widths. Every
// position comes from font metrics: a face supplies per-glyph advances and
// bounds, the layout measures real strings, and lines break at real break
// opportunities.
//
// The HTML-only phase ships one built-in stroke font. The interfaces below are
// the ones a real font stack needs:
//
//     FontFace        metrics and glyph lookup for one face
//     FontRegistry    family + weight + fallback selection
//     TextLayout      measurement, line breaking, caret mapping
//
// Glyph rasterization happens in the graphics backend, which receives stroke
// outlines today and would receive coverage bitmaps or outlines from a real font
// file later; the text layout above it does not care which.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"

#include <cstdint>
#include <vector>

namespace RoboRenderer
{
    /// A point of a glyph outline in font units (y grows upwards from the
    /// baseline). A negative x means "lift the pen": it starts a new stroke.
    struct StrokePoint
    {
        float x;
        float y;
    };

    /// One glyph of a face.
    struct Glyph
    {
        std::uint32_t codepoint = 0;
        /// Horizontal advance in font units.
        float advance = 0.0f;
        /// Bounds in font units, relative to the pen position on the baseline.
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        /// Stroke outline of the glyph.
        std::uint32_t firstPoint = 0;
        std::uint32_t pointCount = 0;
        /// False for space and other blank glyphs.
        bool hasInk = false;
    };

    /// One face of one family. A face is immutable and shared.
    class FontFace
    {
    public:
        virtual ~FontFace() = default;

        virtual const char* FamilyName() const = 0;
        virtual float UnitsPerEm() const = 0;
        /// Distance from the baseline to the top of the text box, in font units.
        virtual float Ascent() const = 0;
        /// Distance from the baseline to the bottom of the text box (positive).
        virtual float Descent() const = 0;
        virtual float LineGap() const = 0;
        virtual float Weight() const = 0;

        /// Glyph for a code point, or nullptr when the face has none.
        virtual const Glyph* FindGlyph(std::uint32_t codepoint) const = 0;
        /// Glyph used for unmapped code points.
        virtual const Glyph& MissingGlyph() const = 0;
        /// Stroke points of one glyph, in font units.
        virtual const StrokePoint* Points() const = 0;
    };

    /// The built-in stroke font. Single weight family, ASCII coverage, designed
    /// on a 1000 unit em grid so a font size of 16 gives a 11.2 pixel cap height,
    /// close to the metrics of a real text face.
    class StrokeFontFace : public FontFace
    {
    public:
        explicit StrokeFontFace(FontWeightKind weight);

        const char* FamilyName() const override { return "Robo Stroke"; }
        float UnitsPerEm() const override { return 1000.0f; }
        float Ascent() const override { return 800.0f; }
        float Descent() const override { return 200.0f; }
        float LineGap() const override { return 120.0f; }
        float Weight() const override;
        const Glyph* FindGlyph(std::uint32_t codepoint) const override;
        const Glyph& MissingGlyph() const override { return missing; }
        const StrokePoint* Points() const override { return points.data(); }

        /// Number of glyphs the face defines.
        static std::uint32_t GlyphCount();

    private:
        float weightValue = 400.0f;
        std::vector<Glyph> glyphs;
        std::vector<StrokePoint> points;
        Glyph missing;
    };

    /// The faces the runtime can use.
    class FontRegistry
    {
    public:
        FontRegistry();

        /// The face for a weight. The HTML-only phase has one family; a real font
        /// stack resolves family, weight and fallback chain here.
        const FontFace& Face(FontWeightKind weight) const;

    private:
        StrokeFontFace regular;
        StrokeFontFace bold;
    };

    /// One laid out line of a text run.
    struct TextLine
    {
        /// Byte range inside the text the line was broken from.
        std::uint32_t startByte = 0;
        std::uint32_t endByte = 0;
        /// Line box: the top of the line, the baseline and the resolved height.
        float x = 0.0f;
        float y = 0.0f;
        float baseline = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        /// True when the line ends with an explicit line break rather than a wrap.
        bool hardBreak = false;
    };

    /// A rasterized glyph: an 8-bit coverage bitmap at device resolution.
    ///
    /// Rasterization belongs to the text subsystem, not to one backend: the
    /// software backend blends these bitmaps, and a GPU backend uploads exactly
    /// the same bitmaps as a texture atlas.
    struct GlyphBitmap
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        /// Offset from the pen position (on the baseline) to the bitmap's
        /// top-left corner, in device pixels.
        float bearingX = 0.0f;
        float bearingY = 0.0f;
        /// Horizontal advance in device pixels, including hinting room.
        float advance = 0.0f;
        /// Index of the coverage data inside the cache's coverage storage.
        std::uint32_t firstCoverage = 0;
        bool blank = true;
    };

    /// A glyph ready to draw: its placement and its coverage data. The two live
    /// together because the bitmap only stores a coverage offset, which keeps
    /// every size bucket in one contiguous block of memory.
    struct RasterizedGlyph
    {
        const GlyphBitmap* bitmap = nullptr;
        const std::uint8_t* coverage = nullptr;
    };

    /// Rasterizes glyph outlines into coverage bitmaps and keeps them.
    ///
    /// The cache is keyed by font size and weight, and each size bucket is a flat
    /// array indexed by code point: no hash map and no allocation per lookup, on
    /// the hottest path text painting has.
    class GlyphCache
    {
    public:
        explicit GlyphCache(const FontRegistry& fonts);

        /// Rasterizes a glyph on first use and returns its bitmap and coverage.
        /// An empty result means the glyph cannot be drawn.
        RasterizedGlyph Get(std::uint32_t codepoint, float fontSize, FontWeightKind weight,
                            float deviceScale);

        std::size_t SizeBucketCount() const { return buckets.size(); }
        std::size_t RasterizedGlyphCount() const { return rasterizedGlyphs; }
        std::size_t CoverageBytes() const { return coverageBytes; }

        /// Drops every bitmap. Used when the device scale changes.
        void Clear();

    private:
        struct SizeBucket
        {
            std::uint32_t sizeKey = 0;
            FontWeightKind weight = FontWeightKind::Normal;
            std::vector<GlyphBitmap> glyphs;
            std::vector<std::uint8_t> glyphState;
            std::vector<std::uint8_t> coverage;
        };

        SizeBucket& BucketFor(float fontSize, FontWeightKind weight, float deviceScale);
        void Rasterize(SizeBucket& bucket, std::uint32_t codepoint);

        const FontRegistry& fonts;
        std::vector<SizeBucket> buckets;
        std::size_t rasterizedGlyphs = 0;
        std::size_t coverageBytes = 0;
    };

    /// Text measurement and line breaking.
    ///
    /// Units: font sizes and results are logical units (see RenderTargetInfo);
    /// the rasterizer scales to device pixels.
    class TextLayout
    {
    public:
        explicit TextLayout(const FontRegistry& registry);

        /// Width of a UTF-8 string at a font size, in logical units.
        float Measure(const char* text, float fontSize, FontWeightKind weight) const;

        /// Width of the first `byteCount` bytes of a UTF-8 string.
        float MeasurePrefix(const char* text, std::uint32_t byteCount, float fontSize,
                            FontWeightKind weight) const;

        /// Line height for a font size (ascent + descent + line gap).
        float LineHeight(float fontSize, FontWeightKind weight) const;

        /// Advance of a single code point, in logical units. Painting needs the
        /// same per-glyph advances that line breaking used, so both read them
        /// from one place.
        float Advance(std::uint32_t codepoint, float fontSize, FontWeightKind weight) const;

        /// Horizontal advance of a tab stop: four spaces, the white-space model
        /// the HTML-only phase uses.
        float TabAdvance(float fontSize, FontWeightKind weight) const
        {
            return Advance(' ', fontSize, weight) * 4.0f;
        }

        /// Breaks a string into lines that fit `availableWidth`.
        ///
        /// Wrapping is done at real break opportunities: spaces, hyphens and the
        /// explicit line breaks the document contains. `preserveWhitespace` turns
        /// wrapping off, which is what <pre> and <textarea> need.
        ///
        /// A width of zero or less means "unbounded": only explicit line breaks
        /// are honoured.
        void Wrap(const char* text,
                  float fontSize,
                  FontWeightKind weight,
                  float availableWidth,
                  bool preserveWhitespace,
                  std::vector<TextLine>& lines) const;

        /// Byte offset of a code point index inside a UTF-8 string. The result is
        /// clamped to the string's end.
        static std::uint32_t ByteOffsetForCodepoint(const char* text, std::uint32_t codepointIndex);

        /// Code point index of a byte offset inside a UTF-8 string.
        static std::uint32_t CodepointIndexForByte(const char* text, std::uint32_t byteOffset);

        /// Code point count of a UTF-8 string.
        static std::uint32_t CodepointCount(const char* text);

        /// Decodes the code point at a byte offset and advances past it.
        static std::uint32_t DecodeUtf8(const char* text, std::uint32_t& offset);

        /// Byte offset of the code point whose box contains `localX`, for caret
        /// placement: the nearest boundary.
        std::uint32_t HitTestOffset(const char* text, float localX, float fontSize,
                                    FontWeightKind weight) const;

    private:
        const FontRegistry& registry;
    };
}
