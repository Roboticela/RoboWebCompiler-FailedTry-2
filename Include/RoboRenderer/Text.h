// ---------------------------------------------------------------------------
// RoboRenderer - text.
//
// Text is a native subsystem, not a set of fixed character widths. Every
// position comes from font metrics: a face supplies per-glyph advances and
// bounds, the layout measures real strings, and lines break at real break
// opportunities.
//
// The faces are real: `FontRegistry` loads OpenType/TrueType files from disk
// (Inter first, then a fallback chain per code point), a face hands back filled
// glyph contours, and `GlyphCache` rasterizes them into coverage bitmaps.
//
//     FontFace        metrics, outlines and glyph lookup for one face
//     FontRegistry    family + weight + per-code-point fallback selection
//     TextLayout      measurement, line breaking, caret mapping
//     GlyphCache      outline -> coverage, cached per face, size and weight
//
// A face may instead be the built-in stroke font, which draws strokes rather
// than filled outlines; the layout and the cache branch on `UsesOutlines()` and
// nothing above them cares which kind of face answered.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
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

    /// A point of a filled outline in font units (y grows upwards from the
    /// baseline). Quadratic segments are represented the way TrueType stores
    /// them: an off-curve point between two on-curve points is a control point.
    struct ContourPoint
    {
        float x = 0.0f;
        float y = 0.0f;
        bool onCurve = true;
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
        /// Filled outline of the glyph: a range of closed contours. Contour `k`
        /// spans the points between the previous contour's end and
        /// `ContourEnds()[k]`, exactly as a TrueType `glyf` contour is stored.
        std::uint32_t firstContour = 0;
        std::uint32_t contourCount = 0;
        /// True when the glyph is drawn as a filled outline rather than strokes.
        bool filled = false;
        /// Synthetic emboldening, in font units. Zero means the face already has
        /// the weight it was asked for. A rasterizer dilates the coverage by this
        /// much when it is non-zero, which is what lets one Inter instance stand
        /// in for every weight.
        float embolden = 0.0f;
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

        // --- filled outlines ---------------------------------------------
        //
        // A real OpenType face hands back filled contours; the built-in stroke
        // face hands back stroke points. The text layout above does not care
        // which, and the rasterizer branches on `UsesOutlines()`.

        /// True when the face's glyphs are filled outlines.
        virtual bool UsesOutlines() const { return false; }
        /// Flat contour point pool for the whole face (indexed by `Glyph`
        /// contour ranges).
        virtual const ContourPoint* ContourPoints() const { return nullptr; }
        /// One past the last point of contour `k`. The pool is shared by every
        /// glyph, so a glyph's contour range indexes into it.
        virtual const std::uint32_t* ContourEnds() const { return nullptr; }

        /// True when the face maps the code point at all, blank glyphs included.
        /// This is what a fallback chain asks before it moves to the next face.
        virtual bool Covers(std::uint32_t codepoint) const
        {
            return FindGlyph(codepoint) != nullptr;
        }

        /// CSS weight class (100..900) the face actually provides. A face that
        /// reports less than the weight it was asked for is emboldened
        /// synthetically rather than silently drawn light.
        virtual int WeightClass() const { return 400; }

        /// True when the face carries colour glyph tables (COLR/CPAL, CBDT or
        /// sbix). Reported so a caller can prefer a monochrome sibling when it
        /// wants outlines and only the colour face has them.
        virtual bool HasColorGlyphs() const { return false; }
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

    /// The faces the runtime can use, and the order in which it looks for one
    /// that covers a code point.
    ///
    /// The registry loads real OpenType/TrueType faces from disk -- Inter first,
    /// because that is the application's default family, then a curated fallback
    /// chain so that CJK, Indic, Arabic, Hebrew, Thai and emoji text all find a
    /// face that can draw them. When no font file is found at all it falls back
    /// to the built-in stroke face, so a host with no fonts still renders.
    class FontRegistry
    {
    public:
        /// Loads from the default search path: `Fonts/` beside the working
        /// directory, `ROBO_FONT_DIRS`, and the platform's font directory.
        FontRegistry();

        /// Loads from an explicit list of directories. Each is scanned for
        /// `.ttf`, `.otf` and `.ttc` files.
        explicit FontRegistry(const std::vector<std::string>& fontDirectories);

        ~FontRegistry();

        FontRegistry(const FontRegistry&) = delete;
        FontRegistry& operator=(const FontRegistry&) = delete;

        /// The primary face for a weight: the family the application asked for.
        const FontFace& Face(FontWeightKind weight) const;

        /// The face that actually covers a code point, walking the fallback
        /// chain. Never null: an uncovered code point resolves to the primary
        /// face, whose missing glyph is the .notdef box.
        const FontFace& FaceFor(std::uint32_t codepoint, FontWeightKind weight) const;

        /// Position of a face in the chain, used as a cache key. A face that is
        /// not in the chain reports 0.
        std::uint32_t IndexOf(const FontFace& face) const;

        /// The family the primary faces resolve to ("Inter" when it is present).
        const std::string& PrimaryFamily() const { return primaryFamily; }

        /// True when bold had to be synthesized from the regular face.
        bool HasSyntheticBold() const { return syntheticBold; }

        /// True when no real font file was found and the stroke face is in use.
        bool UsesStrokeFallback() const { return usesStrokeFallback; }

        /// Number of faces in the fallback chain.
        std::size_t FaceCount() const { return chain.size(); }

    private:
        void Load(const std::vector<std::string>& directories);
        void Build(std::vector<std::shared_ptr<FontFace>> loaded);

        std::vector<std::shared_ptr<FontFace>> owned;
        std::vector<const FontFace*> chain;
        mutable std::unordered_map<std::uint64_t, const FontFace*> resolveCache;

        StrokeFontFace strokeRegular;
        StrokeFontFace strokeBold;

        const FontFace* faceRegular = nullptr;
        const FontFace* faceBold = nullptr;
        std::string primaryFamily = "Robo Stroke";
        bool syntheticBold = false;
        bool usesStrokeFallback = true;
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
        /// A rasterization bucket: one face at one device size and weight. Real
        /// faces cover thousands of code points, so the glyphs are a hash map
        /// rather than an array -- the array was only ever right for the
        /// 95-glyph stroke font.
        struct SizeBucket
        {
            std::uint32_t sizeKey = 0;
            FontWeightKind weight = FontWeightKind::Normal;
            std::uint32_t faceId = 0;
            std::vector<std::uint8_t> coverage;
            std::unordered_map<std::uint32_t, GlyphBitmap> glyphs;
        };

        SizeBucket& BucketFor(std::uint32_t faceId,
                              float fontSize,
                              FontWeightKind weight,
                              float deviceScale);
        void Rasterize(SizeBucket& bucket, const FontFace& face, std::uint32_t codepoint);

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
