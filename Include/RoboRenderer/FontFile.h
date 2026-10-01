// ---------------------------------------------------------------------------
// RoboRenderer - font files.
//
// A real text stack has to read the fonts the host already has. This is the
// smallest honest OpenType/TrueType reader that can do it: the sfnt container,
// the metrics tables the layout needs, `cmap` so a code point resolves to a
// glyph, `glyf`/`loca` or the `CFF `/`CFF2` Type 2 charstrings so a glyph
// resolves to a filled outline, `name` so a family can be chosen by name, and
// `kern` so a pair can be tightened.
//
// On top of the geometry sit the tables that make text *type* rather than a row
// of shapes: `GSUB`/`GPOS`/`GDEF` (see `OtLayout`) so a run shapes, `fvar`/
// `avar`/`gvar`/`HVAR` so a variable face instantiates, and the colour tables so
// an emoji draws in colour. All three colour formats are read: `COLR`/`CPAL`
// (v0 layers and the full v1 paint graph, transforms and gradients composited
// exactly), `CBDT`/`CBLC` and `sbix` embedded bitmaps, and `SVG ` glyph
// documents (`SvgGlyph.cpp`).
//
// Everything is in font units, y upwards from the baseline, exactly as the
// font stores it. The rasterizer is the only thing that knows about pixels.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/Text.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RoboRenderer
{
    /// One glyph outline: closed contours of quadratic points, in font units.
    struct GlyphPath
    {
        /// Flat point stream. Contour `k` spans `[ends[k-1], ends[k])`.
        std::vector<ContourPoint> points;
        std::vector<std::uint32_t> ends;

        bool Empty() const { return ends.empty(); }
    };

    /// A parsed `CFF ` or `CFF2` table: the charstrings an `OTTO` face draws
    /// with instead of `glyf`.
    ///
    /// Type 2 charstrings describe cubic curves, which the renderer's contour
    /// format does not have -- it stores quadratic TrueType contours. Rather
    /// than grow a second curve type, each cubic is flattened to the polyline
    /// the rasterizer already fills, so a CFF face produces exactly the same
    /// kind of `GlyphPath` a TrueType face does and nothing downstream has to
    /// know which kind of face produced it.
    class CffFont
    {
    public:
        /// Parses the table. `glyphCount` comes from `maxp` and bounds the
        /// charstring index. Returns null when the bytes are not a usable CFF.
        static std::unique_ptr<CffFont> Load(const std::uint8_t* data,
                                             std::uint32_t size,
                                             std::uint32_t glyphCount);

        ~CffFont() = default;

        std::uint32_t GlyphCount() const { return charStrings.count; }

        /// The filled outline of one glyph. Returns false for a blank glyph or
        /// an index outside the charstring INDEX.
        bool Outline(std::uint32_t glyph, GlyphPath& out) const;

    private:
        CffFont() = default;

        /// A CFF INDEX: a run of byte strings addressed by a shared offset
        /// array. Offsets here are absolute into the table's bytes.
        struct Index
        {
            std::uint32_t count = 0;
            std::vector<std::uint32_t> offsets; // count + 1 absolute offsets
            std::uint32_t Start(std::uint32_t index) const { return offsets[index]; }
            std::uint32_t End(std::uint32_t index) const { return offsets[index + 1]; }
        };

        /// One DICT's private data: the local subrs a charstring calls, and the
        /// widths the charstrings omit.
        struct Private
        {
            bool present = false;
            std::int32_t defaultWidthX = 0;
            std::int32_t nominalWidthX = 0;
            Index subrs;
        };

        /// A font DICT: a CID-keyed face has several, one per subfont.
        struct FontDict
        {
            Private priv;
            std::uint32_t charStringsOffset = 0;
        };

        struct Interp;

        std::uint8_t ByteAt(std::uint32_t offset) const;
        std::uint16_t U16At(std::uint32_t offset) const;
        std::uint32_t U32At(std::uint32_t offset) const;

        bool ReadIndex(std::uint32_t offset, Index& out, std::uint32_t* next) const;
        void ParseTopDict(std::uint32_t start, std::uint32_t end, bool cff2);
        void ParseFontDict(std::uint32_t start, std::uint32_t end, FontDict& out);
        void ParsePrivateDict(std::uint32_t offset, std::uint32_t size, Private& out);
        void ReadFdSelect(std::uint32_t offset);

        void RunCharstring(const std::uint8_t* code,
                           std::uint32_t length,
                           Interp& state,
                           const Index& localSubrs,
                           int depth) const;

        std::uint32_t FontDictIndexForGlyph(std::uint32_t glyph) const;

        std::vector<std::uint8_t> bytes;
        std::uint32_t size = 0;
        std::uint32_t glyphCountHint = 0;
        bool cff2 = false;

        Index globalSubrs;
        Index charStrings;
        std::uint32_t charStringsOffset = 0;
        std::uint32_t fdArrayOffset = 0;
        std::uint32_t fdSelectOffset = 0;
        Private topPrivate;
        std::vector<FontDict> fontDicts;
        std::vector<std::uint16_t> glyphFd;
    };

    /// OpenType Layout: the `GSUB` and `GPOS` tables, and the shaping they imply.
    ///
    /// This is the part of a font that is not geometry: which glyph replaces
    /// which, which pairs are kerned, and where a mark sits relative to its base.
    /// Without it Arabic letters never join, ligatures never form and kerning is
    /// never applied -- which is exactly the difference between drawing letters
    /// and setting type.
    class OtLayout
    {
    public:
        /// Parses the layout tables. Returns null when the face has none.
        static std::unique_ptr<OtLayout> Create(const std::uint8_t* bytes,
                                                std::uint32_t size,
                                                std::uint32_t gsubOffset,
                                                std::uint32_t gsubLength,
                                                std::uint32_t gposOffset,
                                                std::uint32_t gposLength,
                                                std::uint32_t gdefOffset,
                                                std::uint32_t gdefLength);

        ~OtLayout() = default;

        bool HasSubstitution() const { return hasSubstitution; }
        bool HasPositioning() const { return hasPositioning; }

        /// True when the face declares a script system for the tag.
        bool HasScript(std::uint32_t scriptTag) const;

        /// True when the script declares the feature.
        bool HasFeature(std::uint32_t scriptTag, std::uint32_t featureTag) const;

        /// A script tag for a Unicode script name, or 0 when none applies.
        static std::uint32_t ScriptTagForName(const char* scriptName);

        /// Shapes a run in place: substitution first, then positioning. Glyph
        /// ids come in already mapped from code points. `settings` is the
        /// document's `font-kerning` and `font-feature-settings`; null means the
        /// shaper's defaults.
        void Shape(std::uint32_t scriptTag,
                   std::vector<ShapedGlyph>& glyphs,
                   const ShapeSettings* settings = nullptr) const;

        /// True when the glyph is a mark in `GDEF`.
        bool IsMarkGlyph(std::uint16_t glyph) const;

    private:
        OtLayout() = default;

        struct LookupRef
        {
            std::uint32_t tableOffset = 0; // start of the subtable's table
            std::uint32_t lookupOffset = 0;
            std::uint16_t type = 0;
            std::uint16_t flags = 0;
            std::uint32_t markFilteringSet = 0;
            std::vector<std::uint32_t> subtableOffsets;
        };

        struct FeatureRef
        {
            std::uint32_t tag = 0;
            bool substitution = true;
            std::vector<std::uint16_t> lookups;
        };

        std::uint16_t ReadU16(std::uint32_t offset) const;
        std::int16_t ReadI16(std::uint32_t offset) const;
        std::uint32_t ReadU32(std::uint32_t offset) const;

        void ParseScriptList(std::uint32_t offset, bool substitution);
        void ParseFeatureList(std::uint32_t offset, bool substitution);
        void ParseLookupList(std::uint32_t offset, bool substitution);
        void ParseGdef(std::uint32_t offset);

        /// The features a run turns on, in the order a shaper applies them: the
        /// defaults, minus anything `font-feature-settings` turned off, plus
        /// anything it turned on. `substitution` picks the GSUB or GPOS table.
        std::vector<std::uint32_t> EffectiveFeatures(const ShapeSettings* settings,
                                                     bool substitution) const;

        void ApplySubstitution(std::uint32_t scriptTag,
                               std::vector<ShapedGlyph>& glyphs,
                               const ShapeSettings* settings) const;
        void ApplyPositioning(std::uint32_t scriptTag,
                              std::vector<ShapedGlyph>& glyphs,
                              const ShapeSettings* settings) const;
        void ApplyLookups(const std::vector<std::uint16_t>& lookupIndices,
                          const std::vector<LookupRef>& pool,
                          std::vector<ShapedGlyph>& glyphs) const;
        bool ApplyLookup(const LookupRef& lookup,
                         std::vector<ShapedGlyph>& glyphs,
                         std::size_t& index) const;
        bool ApplyLookupAtIndex(std::uint32_t tableOffset,
                                std::uint16_t lookupIndex,
                                std::vector<ShapedGlyph>& glyphs,
                                std::size_t& index,
                                bool substitution) const;

        std::vector<std::uint16_t> FeatureLookups(std::uint32_t scriptTag,
                                                  const std::vector<std::uint32_t>& featureTags,
                                                  bool substitution) const;

        bool ShouldSkip(const LookupRef& lookup, std::uint16_t glyph) const;

        // Table readers shared by GSUB and GPOS.
        std::uint32_t CoverageIndex(std::uint32_t coverageOffset, std::uint16_t glyph) const;
        std::uint16_t ClassOf(std::uint32_t classDefOffset, std::uint16_t glyph) const;
        void ExpandClassDef(std::uint32_t offset, std::vector<std::uint8_t>& out) const;

        bool ApplyContext(std::uint32_t tableOffset,
                          std::uint32_t subtable,
                          std::vector<ShapedGlyph>& glyphs,
                          std::size_t index,
                          bool chaining,
                          bool substitution) const;
        bool ApplyContextFormat3(std::uint32_t tableOffset,
                                 std::uint32_t subtable,
                                 std::vector<ShapedGlyph>& glyphs,
                                 std::size_t index,
                                 bool chaining,
                                 bool substitution) const;
        bool ApplyExtension(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs,
                            std::size_t& index, bool substitution) const;
        bool ApplyReverseChain(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs,
                               std::size_t index) const;
        bool ApplySingleSubstitution(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyMultipleSubstitution(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyAlternateSubstitution(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyLigatureSubstitution(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplySinglePositioning(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyPairPositioning(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyCursive(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyMarkToBase(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        bool ApplyMarkToMark(std::uint32_t subtable, std::vector<ShapedGlyph>& glyphs, std::size_t index) const;
        std::uint32_t ValueRecordSize(std::uint16_t valueFormat) const;
        void ApplyValueRecord(std::uint32_t offset, std::uint16_t valueFormat, ShapedGlyph& glyph) const;
        void ApplyAnchor(std::uint32_t offset, ShapedGlyph& glyph, bool markSide) const;
        std::uint16_t GlyphClass(std::uint16_t glyph) const;
        bool MarkCovered(std::uint16_t glyph, std::uint32_t markSetIndex) const;

        std::vector<std::uint8_t> storage;
        const std::uint8_t* bytes = nullptr;
        std::uint32_t size = 0;

        std::uint32_t gsub = 0;
        std::uint32_t gpos = 0;
        std::uint32_t gdef = 0;

        bool hasSubstitution = false;
        bool hasPositioning = false;

        std::vector<std::pair<std::uint32_t, std::vector<std::pair<std::uint32_t, std::vector<std::uint16_t>>>>>
            scripts; // script tag -> (language tag, feature indices)
        std::vector<FeatureRef> features;
        std::vector<LookupRef> substitutionLookups;
        std::vector<LookupRef> positioningLookups;

        std::vector<std::uint8_t> glyphClasses;      // GDEF GlyphClassDef, per glyph
        std::vector<std::uint8_t> markAttachClasses; // GDEF MarkAttachClassDef, per glyph
        std::vector<std::vector<std::uint16_t>> markGlyphSets;

        // The lookup pool an in-progress contextual lookup resolves records
        // against. Const because shaping never mutates the tables.
        mutable const std::vector<LookupRef>* contextPool = nullptr;
        mutable bool contextSubstitution = true;
        std::uint32_t featureBase = 0;
    };

    /// One parsed sfnt face: a font file, or one face of a collection.
    class SfntFont
    {
    public:
        ~SfntFont() = default;

        /// Parses a font from memory. Returns null when the bytes are not a
        /// usable sfnt and the face index does not exist.
        static std::unique_ptr<SfntFont> Load(std::vector<std::uint8_t> data,
                                              std::uint32_t faceIndex = 0);

        /// Reads and parses a file. `.ttc` collections load face 0 by default.
        static std::unique_ptr<SfntFont> LoadFile(const std::string& path,
                                                  std::uint32_t faceIndex = 0);

        /// Number of faces a file holds (1 for a plain sfnt, N for a `.ttc`).
        static std::uint32_t FaceCount(const std::vector<std::uint8_t>& data);

        // --- identity ------------------------------------------------------
        const std::string& FamilyName() const { return familyName; }
        const std::string& SubfamilyName() const { return subfamilyName; }
        const std::string& FullName() const { return fullName; }
        const std::string& PostScriptName() const { return postScriptName; }

        bool IsBold() const { return bold; }
        bool IsItalic() const { return italic; }
        int WeightClass() const { return weightClass; }

        /// True when the face carries colour glyph tables.
        bool HasColorGlyphs() const { return hasColor; }

        // --- metrics (font units) -----------------------------------------
        float UnitsPerEm() const { return static_cast<float>(unitsPerEm); }
        float Ascent() const { return ascent; }
        float Descent() const { return descent; }
        float LineGap() const { return lineGap; }
        float CapHeight() const { return capHeight; }
        float XHeight() const { return xHeight; }

        std::uint16_t GlyphCount() const { return glyphCount; }

        // --- glyph lookup --------------------------------------------------
        /// Glyph index for a code point, or 0 (`.notdef`) when uncovered.
        std::uint16_t GlyphIndex(std::uint32_t codepoint) const;
        bool HasCodepoint(std::uint32_t codepoint) const
        {
            return GlyphIndex(codepoint) != 0 || codepoint == 0;
        }

        /// Horizontal advance of a glyph, in font units.
        float Advance(std::uint16_t glyph) const;

        /// Tight bounds of a glyph, in font units, relative to the pen.
        void Bounds(std::uint16_t glyph, float& x0, float& y0, float& x1, float& y1) const;

        /// Kerning between two glyphs, in font units (usually zero).
        std::int32_t Kern(std::uint16_t left, std::uint16_t right) const;

        /// Filled outline of a glyph, composites resolved. Returns false when the
        /// face has no outline for it (a blank glyph, or a CFF face).
        bool Outline(std::uint16_t glyph, GlyphPath& out) const;

        /// True when the face carries `glyf` outlines at all.
        bool HasOutlines() const { return hasGlyf; }

        /// True when the face has `CFF ` outlines (an `OTTO` face).
        bool HasCffOutlines() const { return hasCff; }

        /// The layout tables, parsed on first use.
        const OtLayout* Layout() const;

        /// True when the face carries `GSUB`/`GPOS` at all.
        bool HasLayoutTables() const { return gsub.present || gpos.present; }

        /// Variable-font axes, in the order the file declares them.
        struct VariationAxis
        {
            std::uint32_t tag = 0;
            float minimum = 0.0f;
            float defaultValue = 0.0f;
            float maximum = 0.0f;
            bool hidden = false;
        };

        const std::vector<VariationAxis>& Axes() const { return axes; }
        bool IsVariable() const { return !axes.empty() && gvar.present; }

        /// Named instances the file declares (name id and axis coordinates).
        struct NamedInstance
        {
            std::uint16_t nameId = 0;
            std::vector<float> coordinates;
        };
        const std::vector<NamedInstance>& Instances() const { return instances; }
        const std::string& InstanceName(std::uint16_t nameId) const;

        /// Instantiates the outline at the given axis coordinates: `gvar` deltas
        /// applied to `glyf` points, and `HVAR` deltas applied to advances.
        /// Coordinates are in the order of `Axes()`.
        void SetInstance(const std::vector<float>& coordinates);
        bool HasInstance() const { return instanceSet; }

        /// Colour glyph tables.
        bool HasColorLayers() const { return colr.present && cpal.present; }

        /// Colour layers of a glyph: a base glyph painted as a stack of other
        /// glyphs, each with a palette index. `COLR` v0 stores that stack
        /// directly; v1's paint graph is flattened down to the same thing.
        struct ColorLayer
        {
            std::uint16_t glyph = 0;
            std::uint16_t paletteIndex = 0;
            /// The exact shading: a flat palette entry or a gradient.
            FontFace::ColorShade shade;
            /// The affine transform the paint graph placed on this layer.
            float xx = 1.0f;
            float xy = 0.0f;
            float yx = 0.0f;
            float yy = 1.0f;
            float dx = 0.0f;
            float dy = 0.0f;
        };
        bool ColorLayers(std::uint16_t glyph, std::vector<ColorLayer>& out) const;

        /// A palette entry.
        bool PaletteColor(std::uint16_t index, std::uint8_t& red, std::uint8_t& green,
                          std::uint8_t& blue, std::uint8_t& alpha) const;
        std::uint16_t PaletteCount() const;

        /// The colour font's own drawable bitmaps, when it ships them (`CBDT`
        /// strikes or an `sbix` table). `preferPpem` picks the strike closest to
        /// that pixel size; zero means take the first strike that covers the
        /// glyph.
        bool BitmapGlyph(std::uint16_t glyph,
                         std::uint16_t preferPpem,
                         const std::uint8_t*& data,
                         std::uint32_t& length,
                         std::int32_t& bearingX,
                         std::int32_t& bearingY) const;

        /// True when the face carries an `SVG ` table.
        bool HasSvgGlyphs() const { return svg.present; }

        /// The decoded SVG document for a glyph, gunzipped when the font stored
        /// it compressed. False when there is no document for the glyph.
        bool SvgDocument(std::uint16_t glyph, std::vector<std::uint8_t>& out) const;

        /// Vertical metrics and origin, when the face declares them.
        bool HasVerticalMetrics() const { return vhea.present && vmtx.present; }
        float VerticalAdvance(std::uint16_t glyph) const;
        float VerticalOriginY(std::uint16_t glyph) const;

    private:
        SfntFont() = default;

        struct Table
        {
            std::uint32_t offset = 0;
            std::uint32_t length = 0;
            bool present = false;
        };

        bool Parse(std::uint32_t faceIndex);
        const Table& Find(const char* tag) const;

        std::uint16_t ReadU16(std::uint32_t offset) const;
        std::int16_t ReadI16(std::uint32_t offset) const;
        std::uint32_t ReadU32(std::uint32_t offset) const;

        /// The `sbix` half of `BitmapGlyph`: locate a glyph's image record in
        /// the strike nearest the requested size and hand back its payload.
        bool SbixBitmap(std::uint16_t glyph,
                        std::uint16_t preferPpem,
                        const std::uint8_t*& data,
                        std::uint32_t& length,
                        std::int32_t& bearingX,
                        std::int32_t& bearingY) const;

        void ReadHead();
        void ReadHhea();
        void ReadMaxp();
        void ReadHmtx();
        void ReadOs2();
        void ReadName();
        std::string NameString(std::uint16_t nameId) const;
        void ReadCmap();
        void ReadKern();
        bool ReadGlyf(std::uint16_t glyph, GlyphPath& out, int depth) const;

        std::vector<std::uint8_t> bytes;

        Table head;
        Table hhea;
        Table hmtx;
        Table maxp;
        Table cmap;
        Table glyf;
        Table loca;
        Table os2;
        Table name;
        Table kern;
        Table colr;
        Table cpal;
        Table cbdt;
        Table cblc;
        Table sbix;
        Table svg;
        Table gsub;
        Table gpos;
        Table gdef;
        Table fvar;
        Table avar;
        Table gvar;
        Table hvar;
        Table mvar;
        Table vhea;
        Table vmtx;
        Table vorg;
        Table cff;

        bool hasCff = false;

        std::uint16_t unitsPerEm = 1000;
        std::uint16_t glyphCount = 0;
        std::uint16_t numberOfHMetrics = 0;
        std::int16_t indexToLocFormat = 0;
        std::vector<std::uint32_t> locaOffsets;

        float ascent = 800.0f;
        float descent = 200.0f;
        float lineGap = 0.0f;
        float capHeight = 700.0f;
        float xHeight = 500.0f;

        int weightClass = 400;
        bool bold = false;
        bool italic = false;
        bool hasGlyf = false;
        bool hasColor = false;

        std::string familyName;
        std::string subfamilyName;
        std::string fullName;
        std::string postScriptName;

        struct Range
        {
            std::uint32_t start = 0;
            std::uint32_t end = 0;
            std::uint32_t startGlyph = 0;
        };
        std::vector<Range> cmapRanges;
        std::unordered_map<std::uint32_t, std::uint16_t> cmapSingles;

        struct KernPair
        {
            std::uint16_t left = 0;
            std::uint16_t right = 0;
        };
        std::unordered_map<std::uint32_t, std::int16_t> kernPairs;

        // --- variable fonts ------------------------------------------------
        std::vector<VariationAxis> axes;
        std::vector<NamedInstance> instances;
        std::vector<float> instanceCoordinates;
        bool instanceSet = false;

        // Per-glyph variation data, resolved once at the top level so a glyph
        // outline does not have to walk the axis space on every draw.
        struct GlyphVariation
        {
            std::uint32_t dataOffset = 0;   // into the gvar data block
            std::uint32_t sharedTupleIndex = 0;
            std::uint32_t sharedPointCount = 0;
            std::uint32_t sharedTupleCount = 0;
            std::uint32_t sharedTuplesOffset = 0;
            std::uint32_t glyphCount = 0;
            std::uint32_t flags = 0;
            std::vector<std::uint32_t> offsets;
        };
        GlyphVariation variation;

        // Instance deltas for advances (HVAR), keyed by glyph.
        std::vector<float> advanceDeltas;

        // Colour palettes, parsed once.
        std::uint32_t paletteColorCount = 0;
        std::uint32_t paletteRecordOffset = 0;
        std::uint32_t paletteColorOffsetsOffset = 0;

        mutable std::unique_ptr<OtLayout> layout;
        mutable bool layoutAttempted = false;

        /// The CFF charstrings, parsed on first use (an `OTTO` face has no `glyf`).
        const CffFont* Cff() const;
        mutable std::unique_ptr<CffFont> cffOutline;
        mutable bool cffAttempted = false;

        bool ReadVariations();
        bool ApplyVariations(std::uint16_t glyph, GlyphPath& path) const;
        float AdvanceDelta(std::uint16_t glyph) const;
    };
}
