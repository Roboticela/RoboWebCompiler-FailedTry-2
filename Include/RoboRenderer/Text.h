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
    class FontFace;

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

    /// One positioned glyph produced by shaping a run. Advances and offsets are
    /// in font units; the caller converts with the face's `UnitsPerEm`.
    ///
    /// Shaping is what turns a sequence of code points into the glyphs a font
    /// actually wants drawn: a ligature replaces several, a mark is offset onto
    /// its base, a pair is kerned, and an Arabic letter takes its contextual
    /// form. A run that has not been shaped is one glyph per code point.
    struct ShapedGlyph
    {
        /// Source code point the glyph came from (the first one, for a ligature).
        std::uint32_t codepoint = 0;
        std::uint16_t glyph = 0;
        /// Index of the first source code point this glyph covers.
        std::uint32_t cluster = 0;
        /// How many source code points the glyph replaced (1 unless a ligature).
        std::uint8_t components = 1;
        float xAdvance = 0.0f;
        float yAdvance = 0.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        /// True when the glyph is a mark, which the shaper keeps attached.
        bool mark = false;
    };

    /// A glyph of a shaped run, in logical order and ready to place. This is
    /// what line measurement and painting both walk, so the two always agree on
    /// where a glyph lands.
    struct PositionedGlyph
    {
        /// The face that draws the glyph, and its index in the registry's chain
        /// (the rasterizer's cache key).
        const FontFace* face = nullptr;
        std::uint32_t faceIndex = 0;
        /// The font's own glyph id, or 0 when the face has none (the stroke
        /// font), in which case the code point identifies the glyph instead.
        std::uint16_t glyphId = 0;
        /// Source code point (the first one, for a ligature).
        std::uint32_t codepoint = 0;
        /// Byte offset in the source string of the glyph's first code point.
        std::uint32_t byteStart = 0;
        /// Pen advance in logical units.
        float advance = 0.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        /// True when the glyph is a mark positioned on another glyph.
        bool mark = false;
    };

    /// One explicit OpenType feature setting, from `font-feature-settings`.
    /// `value` is the CSS number: 0 turns the feature off, anything else
    /// requests it (a stylistic set or character variant names its index).
    struct FontFeatureSetting
    {
        std::uint32_t tag = 0;
        std::uint16_t value = 1;
    };

    /// How a run is shaped: the OpenType settings the document asked for.
    ///
    /// A null `settings` means the defaults: the features a shaper applies with
    /// nothing said (`ccmp`, `liga`, `calt`, Arabic joining, `kern`, `mark`), and
    /// the discretionary ones (`dlig`, `salt`, `smcp`, ...) off, because a
    /// document that says nothing must not get them.
    struct ShapeSettings
    {
        /// `font-kerning: none` turns the `kern` feature off; the default `auto`
        /// leaves it on, which is what a shaper does when nothing is said.
        bool kerning = true;
        /// `font-feature-settings`, parsed. The array must outlive the call.
        const FontFeatureSetting* features = nullptr;
        std::size_t featureCount = 0;
    };

    /// One glyph of a face.
    struct Glyph
    {
        std::uint32_t codepoint = 0;
        /// The font's own glyph id, for a face backed by a real font. Shaped
        /// glyphs are addressed by this rather than by code point.
        std::uint16_t glyphId = 0;
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
        /// Synthetic oblique, as the shear applied to x per unit of y above the
        /// baseline (a font unit ratio). Zero means the face is upright. Used
        /// when `font-style: italic` is asked for and no italic file is
        /// installed, which is what a browser obliques too.
        float slant = 0.0f;
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

        /// True when the face is an italic or oblique design, whether it ships
        /// one or the runtime synthesized it. `font-style: italic` selects on
        /// this, preferring a real file over a shear.
        virtual bool IsItalic() const { return false; }

        /// True when the face's oblique is synthetic (a shear of an upright
        /// design) rather than a design of its own.
        virtual bool IsSyntheticItalic() const { return false; }

        /// True when the face carries colour glyph tables (COLR/CPAL, CBDT or
        /// sbix). Reported so a caller can prefer a monochrome sibling when it
        /// wants outlines and only the colour face has them.
        virtual bool HasColorGlyphs() const { return false; }

        /// How one colour layer is shaded: a flat palette entry, or a gradient
        /// with its own stops. A gradient that survives this far is composited
        /// exactly rather than approximated by its first stop.
        struct ColorShade
        {
            enum class Kind : std::uint8_t
            {
                Solid = 0,
                Linear,
                Radial,
                Sweep
            };

            struct Stop
            {
                /// Position along the gradient, 0..1.
                float offset = 0.0f;
                std::uint16_t paletteIndex = 0;
                /// Stop alpha, 0..1.
                float alpha = 1.0f;
            };

            Kind kind = Kind::Solid;
            std::vector<Stop> stops;

            /// Linear: `(x0,y0)` to `(x1,y1)`, with `(x2,y2)` giving the axis the
            /// gradient is perpendicular to. Radial: circle `(x0,y0,r0)` to
            /// `(x1,y1,r1)`. Sweep: centre `(x0,y0)`, angles `x1`..`y1` in degrees.
            /// All in font units, in the layer's own (pre-transform) space.
            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;
            float r0 = 0.0f;
            float r1 = 0.0f;
        };

        /// One layer of a colour (COLR) glyph: another glyph painted in a
        /// palette colour or a gradient, composited in order. `xx..dy` is the
        /// affine transform the paint graph placed on the layer, applied to the
        /// glyph's outline and inverted to sample a gradient exactly.
        struct ColorLayer
        {
            std::uint16_t glyphId = 0;
            std::uint16_t paletteIndex = 0;
            ColorShade shade;
            float xx = 1.0f;
            float xy = 0.0f;
            float yx = 0.0f;
            float yy = 1.0f;
            float dx = 0.0f;
            float dy = 0.0f;
        };

        /// The COLR layers of a glyph. False when the glyph is a plain outline
        /// or the face has no `COLR` table.
        virtual bool ColorLayers(std::uint16_t glyphId, std::vector<ColorLayer>& out) const
        {
            (void)glyphId;
            (void)out;
            return false;
        }

        /// A `CPAL` entry, addressed by colour index within the first palette.
        virtual bool PaletteEntry(std::uint16_t index, std::uint8_t& red, std::uint8_t& green,
                                  std::uint8_t& blue, std::uint8_t& alpha) const
        {
            (void)index;
            (void)red;
            (void)green;
            (void)blue;
            (void)alpha;
            return false;
        }

        /// The face's own embedded bitmap for a glyph (`CBDT`/`CBLC`), when it
        /// ships one. `data` is the encoded image (usually PNG) and `bearingX`/
        /// `bearingY` are in font pixels, y upwards.
        virtual bool BitmapGlyph(std::uint16_t glyphId,
                                 std::uint16_t preferPpem,
                                 const std::uint8_t*& data,
                                 std::uint32_t& length,
                                 std::int32_t& bearingX,
                                 std::int32_t& bearingY) const
        {
            (void)glyphId;
            (void)preferPpem;
            (void)data;
            (void)length;
            (void)bearingX;
            (void)bearingY;
            return false;
        }

        // --- shaping -------------------------------------------------------
        //
        // A face that carries OpenType Layout shapes a run itself; a face that
        // does not (the stroke font, or a font with no `GSUB`/`GPOS`) reports
        // false and the caller falls back to one glyph per code point.

        /// Shapes `count` code points into positioned glyphs in font units.
        /// Returns false when the face cannot shape the run.
        virtual bool ShapeRun(const std::uint32_t* codepoints,
                              std::size_t count,
                              std::vector<ShapedGlyph>& glyphs,
                              const ShapeSettings* settings = nullptr) const
        {
            (void)codepoints;
            (void)count;
            (void)glyphs;
            (void)settings;
            return false;
        }

        /// The glyph a shaped run asked for, addressed by the font's glyph id.
        /// Returns nullptr for a face whose glyphs are only reachable by code
        /// point (the built-in stroke font), in which case the code point is
        /// used instead.
        virtual const Glyph* FindGlyphById(std::uint16_t glyphId) const
        {
            (void)glyphId;
            return nullptr;
        }

        /// The `SVG ` glyph document a colour glyph is drawn from, decoded from
        /// its gzip wrapper when the font stored it compressed. False when the
        /// face has no `SVG ` table or no document for the glyph.
        virtual bool SvgGlyph(std::uint16_t glyphId, std::vector<std::uint8_t>& out) const
        {
            (void)glyphId;
            (void)out;
            return false;
        }
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

        /// A face from a CSS `font-family` list: the first family the host
        /// actually has that covers the code point, or the ordinary fallback
        /// when no family matches. `family` is a comma separated list.
        const FontFace& FaceFor(std::uint32_t codepoint,
                                FontWeightKind weight,
                                const char* family,
                                FontSlantKind slant = FontSlantKind::Upright) const;

        /// The metrics face for a `font-family` list and weight: the matching
        /// family when the host has it, otherwise the primary face.
        const FontFace& Face(FontWeightKind weight,
                             const char* family,
                             FontSlantKind slant = FontSlantKind::Upright) const;

        /// Position of a face in the chain, used as a cache key. A face that is
        /// not in the chain reports 0.
        std::uint32_t IndexOf(const FontFace& face) const;

        /// The family the primary faces resolve to ("Inter" when it is present).
        const std::string& PrimaryFamily() const { return primaryFamily; }

        /// True when bold had to be synthesized from the regular face.
        bool HasSyntheticBold() const { return syntheticBold; }

        /// True when the italic face is a shear of the upright design rather
        /// than a file of its own.
        bool HasSyntheticItalic() const { return syntheticItalic; }

        /// True when no real font file was found and the stroke face is in use.
        bool UsesStrokeFallback() const { return usesStrokeFallback; }

        /// Number of faces in the fallback chain.
        std::size_t FaceCount() const { return chain.size(); }

        /// The face at a chain index, or nullptr when the index is out of range.
        /// The inverse of `IndexOf`, used to resolve a shaped glyph's face.
        const FontFace* FaceAt(std::uint32_t index) const
        {
            return index < chain.size() ? chain[index] : nullptr;
        }

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
        /// The italic/oblique siblings, real when the host has them and a shear
        /// of the upright design otherwise.
        const FontFace* faceItalic = nullptr;
        const FontFace* faceBoldItalic = nullptr;
        std::string primaryFamily = "Robo Stroke";
        bool syntheticBold = false;
        bool syntheticItalic = false;
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
        /// True when the bitmap carries its own RGBA colour (a colour glyph), in
        /// which case `firstColor` addresses the colour storage and the coverage
        /// storage is unused.
        bool color = false;
        /// Index of the colour data inside the cache's colour storage, in pixels.
        std::uint32_t firstColor = 0;
        bool blank = true;
    };

    /// A glyph ready to draw: its placement and its coverage data. The two live
    /// together because the bitmap only stores a coverage offset, which keeps
    /// every size bucket in one contiguous block of memory.
    struct RasterizedGlyph
    {
        const GlyphBitmap* bitmap = nullptr;
        const std::uint8_t* coverage = nullptr;
        /// RGBA pixels (straight alpha) for a colour glyph, or nullptr for a
        /// coverage glyph. `bitmap->width * bitmap->height * 4` bytes.
        const std::uint8_t* color = nullptr;
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

        /// Rasterizes a shaped glyph, addressed by the face's chain index and
        /// the font's glyph id. A ligature or a contextual form has no code
        /// point to look up, so it is fetched by id instead.
        RasterizedGlyph GetShaped(std::uint32_t faceIndex, std::uint16_t glyphId, float fontSize,
                                  FontWeightKind weight, float deviceScale);

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
            /// RGBA pixels of the colour glyphs in this bucket, 4 bytes each.
            std::vector<std::uint8_t> colors;
            /// Keyed by code point, or by `(1 << 32) | glyphId` for a shaped
            /// glyph that has no code point of its own.
            std::unordered_map<std::uint64_t, GlyphBitmap> glyphs;
        };

        SizeBucket& BucketFor(std::uint32_t faceId,
                              float fontSize,
                              FontWeightKind weight,
                              float deviceScale);
        void RasterizeGlyph(SizeBucket& bucket, const FontFace& face, const Glyph& glyph,
                            std::uint64_t key);

        /// Rasterizes a colour glyph: `COLR` layers composited in their palette
        /// colours, or the face's own `CBDT` bitmap. Returns false when the
        /// glyph carries no colour, and the caller falls back to the outline.
        bool RasterizeColor(SizeBucket& bucket, const FontFace& face, const Glyph& glyph,
                            float scale, std::uint64_t key);

        const FontRegistry& fonts;
        /// Buckets are held by pointer so a `GlyphBitmap*` handed out by `Get`
        /// stays valid when a later size or face grows the cache. A plain
        /// vector of buckets would move them on reallocation.
        std::vector<std::unique_ptr<SizeBucket>> buckets;
        std::size_t rasterizedGlyphs = 0;
        std::size_t coverageBytes = 0;
    };

    /// The text-affecting options one run is laid out with. Everything here is
    /// opt-in: a null `TextRunStyle` means exactly the behaviour an unstyled
    /// document had, so the CSS text properties cannot change a document that
    /// does not use them.
    struct TextRunStyle
    {
        /// `letter-spacing`, in logical units, added after every glyph.
        float letterSpacing = 0.0f;
        /// `word-spacing`, in logical units, added to every space.
        float wordSpacing = 0.0f;
        /// Comma separated `font-family` list, or nullptr for the primary
        /// family. The string must outlive the call; a document string does.
        const char* fontFamily = nullptr;
        /// `font-style`: whether an italic or oblique face is wanted.
        FontSlantKind slant = FontSlantKind::Upright;
        /// `direction`: the paragraph's base direction. `Auto` keeps the
        /// runtime's rule (the first strong character decides).
        TextDirectionKind direction = TextDirectionKind::Auto;
        /// `font-kerning: none` suppresses the `kern` feature. The default `auto`
        /// leaves it on.
        bool kerning = true;
        /// `font-feature-settings` as the document wrote it, or nullptr for the
        /// defaults. Parsed on the way to the shaper rather than stored typed,
        /// because the shaper is the only consumer.
        const char* fontFeatures = nullptr;
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
        float Measure(const char* text,
                      float fontSize,
                      FontWeightKind weight,
                      const TextRunStyle* runStyle = nullptr) const;

        /// Width of the first `byteCount` bytes of a UTF-8 string.
        float MeasurePrefix(const char* text,
                            std::uint32_t byteCount,
                            float fontSize,
                            FontWeightKind weight,
                            const TextRunStyle* runStyle = nullptr) const;

        /// Line height for a font size (ascent + descent + line gap).
        float LineHeight(float fontSize, FontWeightKind weight) const;

        /// Advance of a single code point, in logical units. Painting needs the
        /// same per-glyph advances that line breaking used, so both read them
        /// from one place.
        float Advance(std::uint32_t codepoint,
                      float fontSize,
                      FontWeightKind weight,
                      const TextRunStyle* runStyle = nullptr) const;

        /// Shapes a UTF-8 run into positioned glyphs in logical order, using the
        /// covering face's OpenType Layout where it has one. This is the single
        /// source of truth for advances: measurement, hit testing and painting
        /// all walk the same glyphs, so they cannot disagree.
        void ShapeRun(const char* text,
                      std::uint32_t startByte,
                      std::uint32_t endByte,
                      float fontSize,
                      FontWeightKind weight,
                      std::vector<PositionedGlyph>& out,
                      const TextRunStyle* runStyle = nullptr) const;

        /// Parses a `font-feature-settings` declaration into settings. A tag is
        /// a quoted string (or a bare pair of letters); the number after it is
        /// the CSS value, and a missing number means 1. Returns how many
        /// settings were written, which is bounded by `capacity`. `normal`
        /// parses to nothing, exactly as CSS defines it.
        static std::size_t ParseFontFeatures(const char* text,
                                             FontFeatureSetting* out,
                                             std::size_t capacity);

        /// The shaper settings a run style asks for. `storage` is filled only
        /// when the style names features, and the returned settings point into
        /// it, so `storage` must outlive them.
        static ShapeSettings SettingsFor(const TextRunStyle* runStyle,
                                         std::vector<FontFeatureSetting>& storage);

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
                  std::vector<TextLine>& lines,
                  const TextRunStyle* runStyle = nullptr) const;

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
        std::uint32_t HitTestOffset(const char* text,
                                    float localX,
                                    float fontSize,
                                    FontWeightKind weight,
                                    const TextRunStyle* runStyle = nullptr) const;

    private:
        const FontRegistry& registry;
    };
}
