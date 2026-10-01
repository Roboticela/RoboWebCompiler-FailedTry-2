// ---------------------------------------------------------------------------
// RoboRenderer - the font stack.
//
// This is where a code point becomes a face. It has three jobs:
//
//   SfntFontFace        adapts a parsed SfntFont to the FontFace the text
//                       layout and the rasterizer already speak.
//   SyntheticBoldFace   stands in for a weight a file does not provide, by
//                       widening advances and dilating coverage, so "bold"
//                       means bold even with a single Inter instance.
//   FontRegistry        loads faces from disk, picks the primary family and
//                       walks a fallback chain per code point.
//
// Parsed faces are cached process-wide: a `.ttc` that costs a second to read
// is read once, not once per renderer, which keeps constructing a registry
// cheap enough for a test to do it in a loop.
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"
#include "RoboRenderer/Text.h"
#include "RoboRenderer/Unicode.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RoboRenderer
{
    namespace
    {
        std::string Lower(std::string value)
        {
            for (char& character : value)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return value;
        }

        /// A four-character OpenType tag, big-endian, as the layout engine
        /// compares them.
        constexpr std::uint32_t Tag4(char a, char b, char c, char d)
        {
            return (static_cast<std::uint32_t>(a) << 24) |
                   (static_cast<std::uint32_t>(b) << 16) |
                   (static_cast<std::uint32_t>(c) << 8) |
                   static_cast<std::uint32_t>(d);
        }

        /// A face backed by one parsed sfnt face. Outlines are pulled lazily: a
        /// document that never draws a glyph never parses its contour.
        class SfntFontFace final : public FontFace
        {
        public:
            SfntFontFace(std::shared_ptr<SfntFont> font, std::string path)
                : font(std::move(font))
                , path(std::move(path))
                , family(EnsureFamily())
            {
            }

            const char* FamilyName() const override { return family.c_str(); }
            float UnitsPerEm() const override { return font->UnitsPerEm(); }
            float Ascent() const override { return font->Ascent(); }
            float Descent() const override { return font->Descent(); }
            float LineGap() const override { return font->LineGap(); }
            float Weight() const override { return static_cast<float>(font->WeightClass()); }
            int WeightClass() const override { return font->WeightClass(); }
            bool HasColorGlyphs() const override { return font->HasColorGlyphs(); }
            bool UsesOutlines() const override { return font->HasOutlines() || font->HasCffOutlines(); }

            bool ColorLayers(std::uint16_t glyphId, std::vector<ColorLayer>& out) const override
            {
                std::vector<SfntFont::ColorLayer> layers;
                if (!font->ColorLayers(glyphId, layers))
                {
                    return false;
                }

                out.clear();
                for (const SfntFont::ColorLayer& layer : layers)
                {
                    ColorLayer entry;
                    entry.glyphId = layer.glyph;
                    entry.paletteIndex = layer.paletteIndex;
                    out.push_back(entry);
                }
                return !out.empty();
            }

            bool PaletteEntry(std::uint16_t index, std::uint8_t& red, std::uint8_t& green,
                              std::uint8_t& blue, std::uint8_t& alpha) const override
            {
                return font->PaletteColor(index, red, green, blue, alpha);
            }

            bool BitmapGlyph(std::uint16_t glyphId, std::uint16_t preferPpem, const std::uint8_t*& data,
                             std::uint32_t& length, std::int32_t& bearingX,
                             std::int32_t& bearingY) const override
            {
                return font->BitmapGlyph(glyphId, preferPpem, data, length, bearingX, bearingY);
            }

            const ContourPoint* ContourPoints() const override
            {
                return points.empty() ? nullptr : points.data();
            }

            const std::uint32_t* ContourEnds() const override
            {
                return ends.empty() ? nullptr : ends.data();
            }

            bool Covers(std::uint32_t codepoint) const override
            {
                return font->GlyphIndex(codepoint) != 0;
            }

            const Glyph* FindGlyph(std::uint32_t codepoint) const override
            {
                const std::uint16_t glyphIndex = font->GlyphIndex(codepoint);
                if (glyphIndex == 0)
                {
                    return nullptr;
                }

                const auto found = glyphs.find(codepoint);
                if (found != glyphs.end())
                {
                    return &found->second;
                }

                Glyph glyph = BuildGlyph(codepoint, glyphIndex);
                const auto inserted = glyphs.emplace(codepoint, glyph);
                return &inserted.first->second;
            }

            const Glyph* FindGlyphById(std::uint16_t glyphId) const override
            {
                if (glyphId == 0 || glyphId >= font->GlyphCount())
                {
                    return nullptr;
                }

                const auto found = glyphsById.find(glyphId);
                if (found != glyphsById.end())
                {
                    return &found->second;
                }

                Glyph glyph = BuildGlyph(0, glyphId);
                const auto inserted = glyphsById.emplace(glyphId, glyph);
                return &inserted.first->second;
            }

            bool ShapeRun(const std::uint32_t* codepoints,
                          std::size_t count,
                          std::vector<ShapedGlyph>& out) const override
            {
                out.clear();

                const OtLayout* layout = font->Layout();
                if (layout == nullptr || count == 0)
                {
                    return false;
                }

                // The script of the run's first strong character chooses the
                // script system. Latin text lands on `latn`; a font without the
                // tag still shapes through its default script.
                std::uint32_t scriptTag = 0;
                for (std::size_t index = 0; index < count; ++index)
                {
                    const std::uint32_t tag =
                        OtLayout::ScriptTagForName(ScriptName(ScriptOf(codepoints[index])));
                    if (tag != 0 && layout->HasScript(tag))
                    {
                        scriptTag = tag;
                        break;
                    }
                }

                if (scriptTag == 0)
                {
                    if (layout->HasScript(Tag4('D', 'F', 'L', 'T')))
                    {
                        scriptTag = Tag4('D', 'F', 'L', 'T');
                    }
                    else if (layout->HasScript(Tag4('l', 'a', 't', 'n')))
                    {
                        scriptTag = Tag4('l', 'a', 't', 'n');
                    }
                }

                if (scriptTag == 0)
                {
                    return false;
                }

                out.reserve(count);
                for (std::size_t index = 0; index < count; ++index)
                {
                    ShapedGlyph glyph;
                    glyph.codepoint = codepoints[index];
                    glyph.glyph = font->GlyphIndex(codepoints[index]);
                    glyph.cluster = static_cast<std::uint32_t>(index);
                    glyph.xAdvance = font->Advance(glyph.glyph);
                    out.push_back(glyph);
                }

                layout->Shape(scriptTag, out);
                return true;
            }

            const Glyph& MissingGlyph() const override
            {
                if (!missingBuilt)
                {
                    missing = BuildGlyph(0, 0);
                    missing.codepoint = 0xFFFD;
                    missingBuilt = true;
                }
                return missing;
            }

            const StrokePoint* Points() const override { return nullptr; }

            const std::string& Path() const { return path; }
            bool IsBold() const { return font->IsBold(); }
            bool IsItalic() const override { return font->IsItalic(); }
            bool HasOutlines() const { return font->HasOutlines() || font->HasCffOutlines(); }
            const SfntFont& Font() const { return *font; }

        private:
            std::string EnsureFamily()
            {
                if (!font->FamilyName().empty())
                {
                    return font->FamilyName();
                }
                return "Unknown";
            }

            Glyph BuildGlyph(std::uint32_t codepoint, std::uint16_t glyphIndex) const
            {
                Glyph glyph;
                glyph.codepoint = codepoint;
                glyph.glyphId = glyphIndex;
                glyph.advance = font->Advance(glyphIndex);

                float x0 = 0.0f;
                float y0 = 0.0f;
                float x1 = 0.0f;
                float y1 = 0.0f;
                font->Bounds(glyphIndex, x0, y0, x1, y1);

                GlyphPath path;
                const bool hasPath = font->Outline(glyphIndex, path);

                glyph.x0 = x0;
                glyph.y0 = y0;
                glyph.x1 = x1;
                glyph.y1 = y1;
                glyph.hasInk = hasPath && !path.Empty();
                glyph.filled = font->HasOutlines() || font->HasCffOutlines();

                if (hasPath && !path.Empty())
                {
                    glyph.firstContour = static_cast<std::uint32_t>(ends.size());
                    const std::uint32_t base = static_cast<std::uint32_t>(points.size());

                    points.insert(points.end(), path.points.begin(), path.points.end());
                    for (const std::uint32_t end : path.ends)
                    {
                        ends.push_back(base + end);
                    }

                    glyph.contourCount = static_cast<std::uint32_t>(path.ends.size());
                }

                return glyph;
            }

            std::shared_ptr<SfntFont> font;
            std::string path;
            std::string family;

            mutable std::unordered_map<std::uint32_t, Glyph> glyphs;
            mutable std::unordered_map<std::uint16_t, Glyph> glyphsById;
            mutable std::vector<ContourPoint> points;
            mutable std::vector<std::uint32_t> ends;
            mutable Glyph missing;
            mutable bool missingBuilt = false;
        };

        /// A real face's weight, synthesized. Advances widen a little and the
        /// rasterizer dilates the coverage; both changes are visible in
        /// measurement, so bold text occupies the room it looks like it occupies.
        class SyntheticBoldFace final : public FontFace
        {
        public:
            explicit SyntheticBoldFace(std::shared_ptr<const FontFace> base)
                : base(std::move(base))
                , family(std::string(this->base->FamilyName()) + " Bold")
            {
            }

            const char* FamilyName() const override { return family.c_str(); }
            float UnitsPerEm() const override { return base->UnitsPerEm(); }
            float Ascent() const override { return base->Ascent(); }
            float Descent() const override { return base->Descent(); }
            float LineGap() const override { return base->LineGap(); }
            float Weight() const override { return 700.0f; }
            int WeightClass() const override { return 700; }
            bool HasColorGlyphs() const override { return base->HasColorGlyphs(); }
            bool UsesOutlines() const override { return base->UsesOutlines(); }
            bool IsItalic() const override { return base->IsItalic(); }

            // Colour glyphs are not emboldened: their colours come from the
            // palette, not a synthetic dilation of coverage.
            bool ColorLayers(std::uint16_t glyphId, std::vector<ColorLayer>& out) const override
            {
                return base->ColorLayers(glyphId, out);
            }

            bool PaletteEntry(std::uint16_t index, std::uint8_t& red, std::uint8_t& green,
                              std::uint8_t& blue, std::uint8_t& alpha) const override
            {
                return base->PaletteEntry(index, red, green, blue, alpha);
            }

            bool BitmapGlyph(std::uint16_t glyphId, std::uint16_t preferPpem, const std::uint8_t*& data,
                             std::uint32_t& length, std::int32_t& bearingX,
                             std::int32_t& bearingY) const override
            {
                return base->BitmapGlyph(glyphId, preferPpem, data, length, bearingX, bearingY);
            }
            const ContourPoint* ContourPoints() const override { return base->ContourPoints(); }
            const std::uint32_t* ContourEnds() const override { return base->ContourEnds(); }
            const StrokePoint* Points() const override { return base->Points(); }

            bool Covers(std::uint32_t codepoint) const override { return base->Covers(codepoint); }

            const Glyph* FindGlyph(std::uint32_t codepoint) const override
            {
                const Glyph* source = base->FindGlyph(codepoint);
                if (source == nullptr)
                {
                    return nullptr;
                }

                const auto found = glyphs.find(codepoint);
                if (found != glyphs.end())
                {
                    return &found->second;
                }

                const float unit = base->UnitsPerEm() > 0.0f ? base->UnitsPerEm() : 1000.0f;

                Glyph glyph = *source;
                glyph.advance = source->advance * 1.035f;
                glyph.embolden = unit * 0.020f;

                const auto inserted = glyphs.emplace(codepoint, glyph);
                return &inserted.first->second;
            }

            const Glyph& MissingGlyph() const override
            {
                if (!missingBuilt)
                {
                    const Glyph& source = base->MissingGlyph();
                    const float unit = base->UnitsPerEm() > 0.0f ? base->UnitsPerEm() : 1000.0f;
                    missing = source;
                    missing.advance = source.advance * 1.035f;
                    missing.embolden = unit * 0.020f;
                    missingBuilt = true;
                }
                return missing;
            }

            const Glyph* FindGlyphById(std::uint16_t glyphId) const override
            {
                const Glyph* source = base->FindGlyphById(glyphId);
                if (source == nullptr)
                {
                    return nullptr;
                }

                const auto found = glyphsById.find(glyphId);
                if (found != glyphsById.end())
                {
                    return &found->second;
                }

                const float unit = base->UnitsPerEm() > 0.0f ? base->UnitsPerEm() : 1000.0f;

                Glyph glyph = *source;
                glyph.advance = source->advance * 1.035f;
                glyph.embolden = unit * 0.020f;

                const auto inserted = glyphsById.emplace(glyphId, glyph);
                return &inserted.first->second;
            }

            bool ShapeRun(const std::uint32_t* codepoints,
                          std::size_t count,
                          std::vector<ShapedGlyph>& out) const override
            {
                if (!base->ShapeRun(codepoints, count, out))
                {
                    return false;
                }

                // The synthetic weight widens advances exactly as `FindGlyph`
                // does, so measurement and painting stay in step.
                for (ShapedGlyph& glyph : out)
                {
                    glyph.xAdvance *= 1.035f;
                }
                return true;
            }

        private:
            std::shared_ptr<const FontFace> base;
            std::string family;
            mutable std::unordered_map<std::uint32_t, Glyph> glyphs;
            mutable std::unordered_map<std::uint16_t, Glyph> glyphsById;
            mutable Glyph missing;
            mutable bool missingBuilt = false;
        };

        /// An italic the host does not have, synthesized by shearing the upright
        /// design. Advances are unchanged, so measurement is untouched and only
        /// the painted outline leans; the rasterizer reads `Glyph::slant`.
        class SyntheticObliqueFace final : public FontFace
        {
        public:
            explicit SyntheticObliqueFace(std::shared_ptr<const FontFace> base)
                : base(std::move(base))
                , family(std::string(this->base->FamilyName()) + " Italic")
            {
            }

            const char* FamilyName() const override { return family.c_str(); }
            float UnitsPerEm() const override { return base->UnitsPerEm(); }
            float Ascent() const override { return base->Ascent(); }
            float Descent() const override { return base->Descent(); }
            float LineGap() const override { return base->LineGap(); }
            float Weight() const override { return base->Weight(); }
            int WeightClass() const override { return base->WeightClass(); }
            bool HasColorGlyphs() const override { return base->HasColorGlyphs(); }
            bool UsesOutlines() const override { return base->UsesOutlines(); }
            bool IsItalic() const override { return true; }
            bool IsSyntheticItalic() const override { return true; }

            bool ColorLayers(std::uint16_t glyphId, std::vector<ColorLayer>& out) const override
            {
                return base->ColorLayers(glyphId, out);
            }

            bool PaletteEntry(std::uint16_t index, std::uint8_t& red, std::uint8_t& green,
                              std::uint8_t& blue, std::uint8_t& alpha) const override
            {
                return base->PaletteEntry(index, red, green, blue, alpha);
            }

            bool BitmapGlyph(std::uint16_t glyphId, std::uint16_t preferPpem, const std::uint8_t*& data,
                             std::uint32_t& length, std::int32_t& bearingX,
                             std::int32_t& bearingY) const override
            {
                return base->BitmapGlyph(glyphId, preferPpem, data, length, bearingX, bearingY);
            }

            const ContourPoint* ContourPoints() const override { return base->ContourPoints(); }
            const std::uint32_t* ContourEnds() const override { return base->ContourEnds(); }
            const StrokePoint* Points() const override { return base->Points(); }

            bool Covers(std::uint32_t codepoint) const override { return base->Covers(codepoint); }

            const Glyph* FindGlyph(std::uint32_t codepoint) const override
            {
                const Glyph* source = base->FindGlyph(codepoint);
                if (source == nullptr)
                {
                    return nullptr;
                }

                const auto found = glyphs.find(codepoint);
                if (found != glyphs.end())
                {
                    return &found->second;
                }

                Glyph glyph = *source;
                glyph.slant = Slant;

                const auto inserted = glyphs.emplace(codepoint, glyph);
                return &inserted.first->second;
            }

            const Glyph& MissingGlyph() const override
            {
                if (!missingBuilt)
                {
                    missing = base->MissingGlyph();
                    missing.slant = Slant;
                    missingBuilt = true;
                }
                return missing;
            }

            const Glyph* FindGlyphById(std::uint16_t glyphId) const override
            {
                const Glyph* source = base->FindGlyphById(glyphId);
                if (source == nullptr)
                {
                    return nullptr;
                }

                const auto found = glyphsById.find(glyphId);
                if (found != glyphsById.end())
                {
                    return &found->second;
                }

                Glyph glyph = *source;
                glyph.slant = Slant;

                const auto inserted = glyphsById.emplace(glyphId, glyph);
                return &inserted.first->second;
            }

            bool ShapeRun(const std::uint32_t* codepoints,
                          std::size_t count,
                          std::vector<ShapedGlyph>& out) const override
            {
                // A shear changes no advance, so the shaped metrics are the
                // upright ones and only painting leans.
                return base->ShapeRun(codepoints, count, out);
            }

        private:
            /// tan(11.75 degrees), the oblique angle a browser uses.
            static constexpr float Slant = 0.2079f;

            std::shared_ptr<const FontFace> base;
            std::string family;
            mutable std::unordered_map<std::uint32_t, Glyph> glyphs;
            mutable std::unordered_map<std::uint16_t, Glyph> glyphsById;
            mutable Glyph missing;
            mutable bool missingBuilt = false;
        };

        bool FileExists(const std::string& path)
        {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
        }

        void CollectFontFiles(const std::string& directory,
                              std::vector<std::string>& out,
                              int depth,
                              std::size_t limit)
        {
            std::error_code error;
            if (!std::filesystem::is_directory(directory, error))
            {
                return;
            }

            for (const auto& entry : std::filesystem::directory_iterator(directory, error))
            {
                if (error || out.size() >= limit)
                {
                    break;
                }

                if (entry.is_directory(error))
                {
                    if (depth > 0)
                    {
                        CollectFontFiles(entry.path().string(), out, depth - 1, limit);
                    }
                    continue;
                }

                const std::string extension = Lower(entry.path().extension().string());
                if (extension == ".ttf" || extension == ".otf" || extension == ".ttc" || extension == ".otc")
                {
                    out.push_back(entry.path().string());
                }
            }
        }

        std::string EnvironmentValue(const char* name)
        {
            const char* value = std::getenv(name);
            return value == nullptr ? std::string() : std::string(value);
        }

        /// `Fonts/` beside the working directory, or in a parent of it, so the
        /// repository's own bundled fonts are found whichever directory a tool
        /// was launched from.
        std::vector<std::string> ProjectFontDirectories()
        {
            std::vector<std::string> directories;

            std::error_code error;
            std::filesystem::path directory = std::filesystem::current_path(error);

            for (int level = 0; level < 6 && !directory.empty(); ++level)
            {
                const std::filesystem::path candidate = directory / "Fonts";
                if (std::filesystem::is_directory(candidate, error))
                {
                    directories.push_back(candidate.string());
                }

                const std::filesystem::path parent = directory.parent_path();
                if (parent == directory)
                {
                    break;
                }
                directory = parent;
            }

            return directories;
        }

        std::string SystemFontDirectory()
        {
            const std::string windir = EnvironmentValue("WINDIR");
            if (!windir.empty())
            {
                const std::string fonts = windir + "\\Fonts";
                if (FileExists(fonts) || std::filesystem::is_directory(fonts))
                {
                    return fonts;
                }
            }

            const std::string systemRoot = EnvironmentValue("SystemRoot");
            if (!systemRoot.empty())
            {
                return systemRoot + "\\Fonts";
            }

            return "/usr/share/fonts";
        }

        /// The system files worth opening first, chosen for script coverage: one
        /// face per writing system the showcase exercises.
        const std::vector<std::string>& CuratedSystemFontFiles()
        {
            static const std::vector<std::string> files = {
                "segoeui.ttf",  "segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf", "segoeuil.ttf",
                "seguisym.ttf", "seguihis.ttf", "segmdl2.ttf",  "segoeicons.ttf", "seguiemj.ttf",
                "arial.ttf",    "arialbd.ttf",  "tahoma.ttf",   "tahomabd.ttf",   "verdana.ttf",
                "calibri.ttf",  "georgia.ttf",  "candara.ttf",  "cambria.ttc",
                "arabtype.ttf", "majalla.ttf",  "aldhabi.ttf",  "urdtype.ttf",
                "nirmala.ttc",
                "leelawui.ttf", "leelawad.ttf", "taile.ttf",
                "mmrtext.ttf",
                "sylfaen.ttf",
                "ebrima.ttf",
                "monbaiti.ttf",
                "himalaya.ttf",
                "gadugi.ttf",
                "msyi.ttf",     "ntailu.ttf",
                "msyh.ttc",     "msjh.ttc",     "simsun.ttc",
                "yugothm.ttc",  "msgothic.ttc",
                "malgun.ttf",
            };
            return files;
        }

        bool IsEmojiFamilyName(const std::string& family)
        {
            const std::string lower = Lower(family);
            return lower.find("emoji") != std::string::npos;
        }

        bool IsSymbolFamilyName(const std::string& family)
        {
            const std::string lower = Lower(family);
            return lower.find("symbol") != std::string::npos || lower.find("mdl2") != std::string::npos ||
                   lower.find("icon") != std::string::npos || lower.find("historic") != std::string::npos;
        }

        /// Family preference for the primary face: the application's default
        /// first, then the sans faces a desktop is likely to have.
        int PrimaryFamilyRank(const std::string& family)
        {
            static const std::vector<std::string> order = {
                "inter", "inter display", "segoe ui", "arial", "tahoma", "verdana", "calibri",
                "helvetica", "helvetica neue", "noto sans", "dejavu sans", "liberation sans",
                "roboto", "open sans", "lato", "ubuntu",
            };

            const std::string lower = Lower(family);
            for (std::size_t index = 0; index < order.size(); ++index)
            {
                if (lower == order[index])
                {
                    return static_cast<int>(index);
                }
            }
            return -1;
        }

        /// Fallback order for everything the primary face does not cover. Emoji
        /// and symbol faces come last: they cover stray pictographs, not text.
        int FallbackFamilyRank(const std::string& family)
        {
            static const std::vector<std::string> order = {
                "inter",        "segoe ui",        "arial",           "tahoma",
                "nirmala ui",   "leelawadee ui",   "malgun gothic",   "microsoft yahei",
                "microsoft jhenghei", "simsun",    "yu gothic",       "ms gothic",
                "microsoft himalaya", "gadugi",    "microsoft yi baiti", "microsoft new tai lue",
                "ebrima",       "sylfaen",         "mongolian baiti", "aldhabi",
                "arabic typesetting", "majalla ui", "urdu typesetting", "myanmar text",
                "segoe ui symbol", "segoe ui historic", "segoe mdl2 assets",
                "segoe ui emoji", "noto color emoji", "apple color emoji",
            };

            const std::string lower = Lower(family);
            for (std::size_t index = 0; index < order.size(); ++index)
            {
                if (lower == order[index])
                {
                    return static_cast<int>(index);
                }
            }
            return 1000;
        }

        // --- process-wide font file cache ----------------------------------

        std::mutex& CacheMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::unordered_map<std::string, std::shared_ptr<SfntFontFace>>& FaceCache()
        {
            static std::unordered_map<std::string, std::shared_ptr<SfntFontFace>> cache;
            return cache;
        }

        /// Number of faces a file holds, read from its first bytes alone so a
        /// huge collection is not loaded twice just to count it.
        std::uint32_t FileFaceCount(const std::string& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return 0;
            }

            std::uint8_t header[12] = {};
            stream.read(reinterpret_cast<char*>(header), sizeof(header));
            const std::streamsize read = stream.gcount();

            if (read < 4)
            {
                return 0;
            }

            if (header[0] == 't' && header[1] == 't' && header[2] == 'c' && header[3] == 'f')
            {
                if (read < 12)
                {
                    return 1;
                }
                const std::uint32_t count = (static_cast<std::uint32_t>(header[8]) << 24) |
                                            (static_cast<std::uint32_t>(header[9]) << 16) |
                                            (static_cast<std::uint32_t>(header[10]) << 8) |
                                            static_cast<std::uint32_t>(header[11]);
                return count == 0 ? 1u : count;
            }

            return 1;
        }

        std::shared_ptr<SfntFontFace> LoadFaceCached(const std::string& path, std::uint32_t faceIndex)
        {
            const std::string key = path + "#" + std::to_string(faceIndex);

            std::lock_guard<std::mutex> lock(CacheMutex());
            auto& cache = FaceCache();

            const auto found = cache.find(key);
            if (found != cache.end())
            {
                return found->second;
            }

            std::shared_ptr<SfntFont> font = SfntFont::LoadFile(path, faceIndex);
            if (!font)
            {
                cache[key] = nullptr;
                return nullptr;
            }

            auto face = std::make_shared<SfntFontFace>(std::move(font), path);
            cache[key] = face;
            return face;
        }
    }

    FontRegistry::FontRegistry()
        : strokeRegular(FontWeightKind::Normal)
        , strokeBold(FontWeightKind::Bold)
    {
        Load({});
    }

    FontRegistry::FontRegistry(const std::vector<std::string>& fontDirectories)
        : strokeRegular(FontWeightKind::Normal)
        , strokeBold(FontWeightKind::Bold)
    {
        Load(fontDirectories);
    }

    FontRegistry::~FontRegistry() = default;

    void FontRegistry::Load(const std::vector<std::string>& directories)
    {
        std::vector<std::string> files;

        if (!directories.empty())
        {
            for (const std::string& directory : directories)
            {
                CollectFontFiles(directory, files, 3, 512);
            }
        }
        else
        {
            for (const std::string& directory : ProjectFontDirectories())
            {
                CollectFontFiles(directory, files, 3, 512);
            }

            const std::string systemDirectory = SystemFontDirectory();
            for (const std::string& name : CuratedSystemFontFiles())
            {
                const std::string candidate = systemDirectory + "/" + name;
                if (FileExists(candidate))
                {
                    files.push_back(candidate);
                }
            }

            if (files.empty())
            {
                CollectFontFiles(systemDirectory, files, 1, 256);
            }
        }

        std::vector<std::shared_ptr<FontFace>> loaded;

        for (const std::string& path : files)
        {
            const std::uint32_t faceCount = FileFaceCount(path);
            for (std::uint32_t faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                std::shared_ptr<SfntFontFace> face = LoadFaceCached(path, faceIndex);
                if (face)
                {
                    loaded.push_back(face);
                }
            }
        }

        Build(std::move(loaded));
    }

    void FontRegistry::Build(std::vector<std::shared_ptr<FontFace>> loaded)
    {
        owned = std::move(loaded);

        if (owned.empty())
        {
            faceRegular = &strokeRegular;
            faceBold = &strokeBold;
            faceItalic = &strokeRegular;
            faceBoldItalic = &strokeBold;
            chain = { faceRegular, faceBold, faceItalic, faceBoldItalic };
            primaryFamily = "Robo Stroke";
            syntheticBold = false;
            syntheticItalic = false;
            usesStrokeFallback = true;
            return;
        }

        usesStrokeFallback = false;

        const auto familyRank = [](const FontFace* face) {
            return PrimaryFamilyRank(face->FamilyName());
        };

        const auto choosePrimary = [&](bool wantBold, bool wantItalic) -> const FontFace* {
            const FontFace* best = nullptr;
            int bestFamily = 10000;
            int bestWeightDistance = 10000;

            for (const std::shared_ptr<FontFace>& face : owned)
            {
                if (!face->UsesOutlines())
                {
                    continue;
                }
                if (face->WeightClass() < 100)
                {
                    continue;
                }

                // An italic request takes only italic faces and an upright one
                // only upright faces, so a host with both never answers the
                // wrong style for a plain weight.
                if (face->IsItalic() != wantItalic)
                {
                    continue;
                }

                const int family = familyRank(face.get());
                if (family < 0)
                {
                    continue;
                }

                const int weightClass = face->WeightClass();
                const int weightDistance = wantBold ? std::abs(weightClass - 700)
                                                    : std::abs(weightClass - 400);

                const bool boldish = weightClass >= 600;
                if (boldish != wantBold)
                {
                    continue;
                }

                if (family < bestFamily || (family == bestFamily && weightDistance < bestWeightDistance))
                {
                    best = face.get();
                    bestFamily = family;
                    bestWeightDistance = weightDistance;
                }
            }

            return best;
        };

        const FontFace* regular = choosePrimary(false, false);
        const FontFace* boldFace = choosePrimary(true, false);

        // A desktop without any of the preferred families: take the first face
        // that has outlines at all, so the renderer still draws real glyphs.
        if (regular == nullptr)
        {
            for (const std::shared_ptr<FontFace>& face : owned)
            {
                if (face->UsesOutlines() && !IsEmojiFamilyName(face->FamilyName()) &&
                    !IsSymbolFamilyName(face->FamilyName()))
                {
                    regular = face.get();
                    break;
                }
            }
        }

        if (regular == nullptr)
        {
            for (const std::shared_ptr<FontFace>& face : owned)
            {
                if (face->UsesOutlines())
                {
                    regular = face.get();
                    break;
                }
            }
        }

        if (regular == nullptr)
        {
            faceRegular = &strokeRegular;
            faceBold = &strokeBold;
            faceItalic = &strokeRegular;
            faceBoldItalic = &strokeBold;
            chain = { faceRegular, faceBold, faceItalic, faceBoldItalic };
            primaryFamily = "Robo Stroke";
            syntheticBold = false;
            syntheticItalic = false;
            usesStrokeFallback = true;
            return;
        }

        faceRegular = regular;
        primaryFamily = regular->FamilyName();

        // Find a shared_ptr for the regular face so the synthetic wrapper can
        // hold it, and so `owned` keeps it alive.
        std::shared_ptr<const FontFace> regularShared;
        for (const std::shared_ptr<FontFace>& face : owned)
        {
            if (face.get() == regular)
            {
                regularShared = face;
                break;
            }
        }

        // An italic sibling must belong to the family the primary face came
        // from: an italic Inter is wanted, not whatever italic the host happens
        // to install first. A missing one is synthesized below.
        const auto findSibling = [&](bool wantBold, bool wantItalic) -> const FontFace* {
            for (const std::shared_ptr<FontFace>& face : owned)
            {
                if (!face->UsesOutlines() || face->WeightClass() < 100)
                {
                    continue;
                }
                if (std::strcmp(face->FamilyName(), faceRegular->FamilyName()) != 0)
                {
                    continue;
                }
                if ((face->WeightClass() >= 600) != wantBold)
                {
                    continue;
                }
                if (face->IsItalic() != wantItalic)
                {
                    continue;
                }
                return face.get();
            }
            return nullptr;
        };

        const FontFace* italicFace = findSibling(false, true);
        const FontFace* boldItalicFace = findSibling(true, true);

        if (boldFace != nullptr)
        {
            faceBold = boldFace;
            syntheticBold = false;
        }
        else
        {
            auto synthetic = std::make_shared<SyntheticBoldFace>(regularShared);
            faceBold = synthetic.get();
            owned.push_back(synthetic);
            syntheticBold = true;
        }

        // The italic siblings. A real italic file wins; when the host has none,
        // the upright design is sheared so `font-style: italic` never resolves
        // to nothing. A shared_ptr for the bold face lets the shear wrap it too.
        std::shared_ptr<const FontFace> boldShared;
        for (const std::shared_ptr<FontFace>& face : owned)
        {
            if (face.get() == faceBold)
            {
                boldShared = face;
                break;
            }
        }

        if (italicFace != nullptr)
        {
            faceItalic = italicFace;
        }
        else
        {
            auto synthetic = std::make_shared<SyntheticObliqueFace>(regularShared);
            faceItalic = synthetic.get();
            owned.push_back(synthetic);
            syntheticItalic = true;
        }

        if (boldItalicFace != nullptr)
        {
            faceBoldItalic = boldItalicFace;
        }
        else
        {
            auto synthetic = std::make_shared<SyntheticObliqueFace>(boldShared);
            faceBoldItalic = synthetic.get();
            owned.push_back(synthetic);
            syntheticItalic = true;
        }

        // Build the fallback order. The primary four are first; everything else
        // follows by family preference, outlines before colour-only faces.
        std::vector<const FontFace*> fallbacks;
        for (const std::shared_ptr<FontFace>& face : owned)
        {
            if (face.get() == faceRegular || face.get() == faceBold || face.get() == faceItalic ||
                face.get() == faceBoldItalic)
            {
                continue;
            }
            fallbacks.push_back(face.get());
        }

        std::stable_sort(fallbacks.begin(), fallbacks.end(), [](const FontFace* a, const FontFace* b) {
            const bool aOutline = a->UsesOutlines();
            const bool bOutline = b->UsesOutlines();
            if (aOutline != bOutline)
            {
                return aOutline;
            }
            return FallbackFamilyRank(a->FamilyName()) < FallbackFamilyRank(b->FamilyName());
        });

        chain.clear();
        chain.push_back(faceRegular);
        chain.push_back(faceBold);
        chain.push_back(faceItalic);
        chain.push_back(faceBoldItalic);
        for (const FontFace* face : fallbacks)
        {
            chain.push_back(face);
        }
    }

    namespace
    {
        /// Case-insensitive family comparison, with the requested name trimmed.
        bool FamilyEquals(const char* faceFamily, const std::string& requested)
        {
            std::size_t begin = 0;
            std::size_t end = requested.size();
            while (begin < end && std::isspace(static_cast<unsigned char>(requested[begin])) != 0)
            {
                ++begin;
            }
            while (end > begin && std::isspace(static_cast<unsigned char>(requested[end - 1])) != 0)
            {
                --end;
            }

            if (begin == end)
            {
                return false;
            }

            std::size_t index = 0;
            for (; index < end - begin; ++index)
            {
                const char actual = faceFamily[index];
                if (actual == '\0')
                {
                    return false;
                }
                if (std::tolower(static_cast<unsigned char>(actual)) !=
                    std::tolower(static_cast<unsigned char>(requested[begin + index])))
                {
                    return false;
                }
            }

            return faceFamily[index] == '\0';
        }

        /// True for a CSS generic family, which names a *kind* of face rather
        /// than a family the host may or may not have.
        bool IsGenericFamily(const std::string& name)
        {
            return name == "serif" || name == "sans-serif" || name == "monospace" || name == "cursive" ||
                   name == "fantasy" || name == "system-ui" || name == "ui-serif" ||
                   name == "ui-sans-serif" || name == "ui-monospace" || name == "ui-rounded" ||
                   name == "emoji" || name == "math" || name == "fangsong";
        }
    }

    const FontFace& FontRegistry::Face(FontWeightKind weight) const
    {
        return weight == FontWeightKind::Bold ? *faceBold : *faceRegular;
    }

    const FontFace& FontRegistry::Face(FontWeightKind weight, const char* family,
                                       FontSlantKind slant) const
    {
        const bool wantBold = weight == FontWeightKind::Bold;
        const bool wantItalic = slant == FontSlantKind::Italic;

        const FontFace* primary =
            wantItalic ? (wantBold ? faceBoldItalic : faceItalic) : (wantBold ? faceBold : faceRegular);

        if (family == nullptr || family[0] == '\0')
        {
            return *primary;
        }

        const char* cursor = family;
        while (*cursor != '\0')
        {
            std::string name;
            while (*cursor != '\0' && *cursor != ',')
            {
                name += *cursor;
                ++cursor;
            }
            if (*cursor == ',')
            {
                ++cursor;
            }

            if (IsGenericFamily(name))
            {
                continue;
            }

            for (const FontFace* face : chain)
            {
                if (face == nullptr || !FamilyEquals(face->FamilyName(), name))
                {
                    continue;
                }

                const bool bold = face->WeightClass() >= 600;
                if (bold == wantBold && face->IsItalic() == wantItalic)
                {
                    return *face;
                }
            }
        }

        return *primary;
    }

    const FontFace& FontRegistry::FaceFor(std::uint32_t codepoint,
                                          FontWeightKind weight,
                                          const char* family,
                                          FontSlantKind slant) const
    {
        const bool wantBold = weight == FontWeightKind::Bold;
        const bool wantItalic = slant == FontSlantKind::Italic;

        if (family == nullptr || family[0] == '\0')
        {
            // No named family: the italic sibling when one covers the code
            // point, otherwise the ordinary fallback chain.
            if (wantItalic)
            {
                const FontFace& italic = wantBold ? *faceBoldItalic : *faceItalic;
                if (italic.Covers(codepoint))
                {
                    return italic;
                }
            }

            return FaceFor(codepoint, weight);
        }

        const char* cursor = family;
        while (*cursor != '\0')
        {
            std::string name;
            while (*cursor != '\0' && *cursor != ',')
            {
                name += *cursor;
                ++cursor;
            }
            if (*cursor == ',')
            {
                ++cursor;
            }

            if (IsGenericFamily(name))
            {
                continue;
            }

            for (const FontFace* face : chain)
            {
                if (face != nullptr && FamilyEquals(face->FamilyName(), name) &&
                    face->IsItalic() == wantItalic && face->Covers(codepoint))
                {
                    return *face;
                }
            }
        }

        // No installed family covers the code point; the ordinary fallback chain
        // draws it, which is exactly what a browser does for a glyph the chosen
        // family does not have.
        return FaceFor(codepoint, weight);
    }

    std::uint32_t FontRegistry::IndexOf(const FontFace& face) const
    {
        for (std::size_t index = 0; index < chain.size(); ++index)
        {
            if (chain[index] == &face)
            {
                return static_cast<std::uint32_t>(index);
            }
        }
        return 0;
    }

    const FontFace& FontRegistry::FaceFor(std::uint32_t codepoint, FontWeightKind weight) const
    {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(weight) << 32) | static_cast<std::uint64_t>(codepoint);

        const auto cached = resolveCache.find(key);
        if (cached != resolveCache.end())
        {
            return *cached->second;
        }

        const FontFace* primary = weight == FontWeightKind::Bold ? faceBold : faceRegular;
        const FontFace* secondary = weight == FontWeightKind::Bold ? faceRegular : faceBold;

        const FontFace* result = primary;

        if (primary->Covers(codepoint))
        {
            result = primary;
        }
        else if (secondary->Covers(codepoint))
        {
            result = secondary;
        }
        else
        {
            const FontFace* firstCover = nullptr;
            const FontFace* colorCover = nullptr;

            for (std::size_t index = 2; index < chain.size(); ++index)
            {
                if (!chain[index]->Covers(codepoint))
                {
                    continue;
                }
                if (firstCover == nullptr)
                {
                    firstCover = chain[index];
                }
                if (colorCover == nullptr && chain[index]->HasColorGlyphs())
                {
                    colorCover = chain[index];
                }
            }

            // An emoji code point is meant to be coloured, so a colour face that
            // covers it wins over an earlier monochrome symbol face. Text-default
            // symbols keep the chain order they had.
            const bool preferColor =
                IsEmojiPresentation(codepoint) || IsEmoji(codepoint);

            result = (preferColor && colorCover != nullptr)
                ? colorCover
                : (firstCover != nullptr ? firstCover : primary);
        }

        resolveCache[key] = result;
        return *result;
    }
}
