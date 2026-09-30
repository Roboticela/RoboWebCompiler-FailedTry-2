// ---------------------------------------------------------------------------
// RoboRenderer - font files.
//
// A real text stack has to read the fonts the host already has. This is the
// smallest honest OpenType/TrueType reader that can do it: the sfnt container,
// the metrics tables the layout needs, `cmap` so a code point resolves to a
// glyph, `glyf`/`loca` so a glyph resolves to a filled outline, `name` so a
// family can be chosen by name, and `kern` so a pair can be tightened.
//
// What it deliberately does not do, because nothing above it needs it yet:
//   - CFF/Type 2 charstrings (an `OTTO` face reports no outlines rather than
//     guessing),
//   - OpenType Layout (`GSUB`/`GPOS`): shaping lives in the text engine,
//   - colour glyph tables beyond reporting that they exist.
//
// Everything is in font units, y upwards from the baseline, exactly as the
// font stores it. The rasterizer is the only thing that knows about pixels.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/Text.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
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

        void ReadHead();
        void ReadHhea();
        void ReadMaxp();
        void ReadHmtx();
        void ReadOs2();
        void ReadName();
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
    };
}
