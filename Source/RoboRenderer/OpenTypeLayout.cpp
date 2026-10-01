// ---------------------------------------------------------------------------
// RoboRenderer - OpenType Layout.
//
// See FontFile.h for what an `OtLayout` is for. This file is the part of the
// font that is not geometry: which glyph replaces which, which pairs are
// kerned, and where a mark attaches to its base.
//
// The tables are copied out of the font on creation, so an `OtLayout` owns its
// bytes and never dereferences a font that has gone away. Every reader is
// bounds-checked and returns zero past the end, which keeps a malformed or
// truncated font from being anything worse than a face that does not shape.
//
// Supported:
//   GSUB  1 single, 2 multiple, 3 alternate, 4 ligature, 5 context 1/2/3,
//         6 chained context 1/2/3, 7 extension, 8 reverse chaining
//   GPOS  1 single, 2 pair 1/2, 3 cursive, 4 mark-to-base, 5 mark-to-ligature,
//         6 mark-to-mark, 7 context 1/2/3, 8 chained context 1/2/3, 9 extension
//   GDEF  glyph classes, mark attachment classes, mark glyph sets
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace RoboRenderer
{
    namespace
    {
        /// A four-character OpenType tag, big-endian: `'kern'` == 0x6B65726Eu.
        constexpr std::uint32_t Tag4(char a, char b, char c, char d)
        {
            return (static_cast<std::uint32_t>(a) << 24) |
                   (static_cast<std::uint32_t>(b) << 16) |
                   (static_cast<std::uint32_t>(c) << 8) |
                   static_cast<std::uint32_t>(d);
        }

        constexpr std::uint16_t kNoCoverage = 0xFFFFu;

        /// A `F2Dot14` fixed-point value, as the font stores rotations and scales.
        inline float F2Dot14(std::int16_t value)
        {
            return static_cast<float>(value) / 16384.0f;
        }
    }

    // --- construction ------------------------------------------------------

    std::unique_ptr<OtLayout> OtLayout::Create(const std::uint8_t* fontBytes,
                                               std::uint32_t fontBytesSize,
                                               std::uint32_t gsubOffset,
                                               std::uint32_t gsubLength,
                                               std::uint32_t gposOffset,
                                               std::uint32_t gposLength,
                                               std::uint32_t gdefOffset,
                                               std::uint32_t gdefLength)
    {
        if (fontBytes == nullptr || (gsubLength == 0 && gposLength == 0))
        {
            return nullptr;
        }

        auto layout = std::unique_ptr<OtLayout>(new OtLayout());

        const auto append = [&](std::uint32_t offset, std::uint32_t length) -> std::uint32_t {
            const std::uint32_t base = static_cast<std::uint32_t>(layout->storage.size());
            if (length == 0 || offset == 0 ||
                static_cast<std::uint64_t>(offset) + length > fontBytesSize)
            {
                return base;
            }
            layout->storage.insert(layout->storage.end(),
                                   fontBytes + offset,
                                   fontBytes + offset + length);
            return base;
        };

        layout->gsub = append(gsubOffset, gsubLength);
        layout->gpos = append(gposOffset, gposLength);
        layout->gdef = append(gdefOffset, gdefLength);
        layout->bytes = layout->storage.data();
        layout->size = static_cast<std::uint32_t>(layout->storage.size());

        if (gdefLength != 0)
        {
            layout->ParseGdef(layout->gdef);
        }

        // GSUB and GPOS are parsed independently. Lookups go into separate pools
        // so a feature's lookup index always means the same thing it meant in
        // the file, and features are tagged with the table they came from.
        if (gsubLength != 0 && static_cast<std::uint64_t>(layout->gsub) + 10 <= layout->size)
        {
            const std::uint32_t base = layout->gsub;
            layout->hasSubstitution = true;
            layout->ParseLookupList(base + layout->ReadU16(base + 8), true);
            layout->featureBase = static_cast<std::uint32_t>(layout->features.size());
            layout->ParseFeatureList(base + layout->ReadU16(base + 6), true);
            layout->ParseScriptList(base + layout->ReadU16(base + 4), true);
        }

        if (gposLength != 0 && static_cast<std::uint64_t>(layout->gpos) + 10 <= layout->size)
        {
            const std::uint32_t base = layout->gpos;
            layout->hasPositioning = true;
            layout->ParseLookupList(base + layout->ReadU16(base + 8), false);
            layout->featureBase = static_cast<std::uint32_t>(layout->features.size());
            layout->ParseFeatureList(base + layout->ReadU16(base + 6), false);
            layout->ParseScriptList(base + layout->ReadU16(base + 4), false);
        }

        return layout;
    }

    // --- readers -----------------------------------------------------------

    std::uint16_t OtLayout::ReadU16(std::uint32_t offset) const
    {
        if (bytes == nullptr || static_cast<std::size_t>(offset) + 2 > size)
        {
            return 0;
        }
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) |
                                          bytes[offset + 1]);
    }

    std::int16_t OtLayout::ReadI16(std::uint32_t offset) const
    {
        return static_cast<std::int16_t>(ReadU16(offset));
    }

    std::uint32_t OtLayout::ReadU32(std::uint32_t offset) const
    {
        return (static_cast<std::uint32_t>(ReadU16(offset)) << 16) | ReadU16(offset + 2);
    }

    // --- table parsing -----------------------------------------------------

    void OtLayout::ParseLookupList(std::uint32_t offset, bool substitution)
    {
        std::vector<LookupRef>& pool = substitution ? substitutionLookups : positioningLookups;

        const std::uint16_t count = ReadU16(offset);
        for (std::uint16_t index = 0; index < count; ++index)
        {
            const std::uint32_t lookupOffset = offset + ReadU16(offset + 2 + index * 2);

            LookupRef ref;
            ref.tableOffset = substitution ? gsub : gpos;
            ref.lookupOffset = lookupOffset;
            ref.type = ReadU16(lookupOffset);
            ref.flags = ReadU16(lookupOffset + 2);

            const std::uint16_t subtableCount = ReadU16(lookupOffset + 4);
            for (std::uint16_t sub = 0; sub < subtableCount; ++sub)
            {
                ref.subtableOffsets.push_back(lookupOffset + ReadU16(lookupOffset + 6 + sub * 2));
            }

            if ((ref.flags & 0x0010u) != 0)
            {
                ref.markFilteringSet = ReadU16(lookupOffset + 6 + subtableCount * 2);
            }

            pool.push_back(std::move(ref));
        }
    }

    void OtLayout::ParseFeatureList(std::uint32_t offset, bool substitution)
    {
        const std::uint16_t count = ReadU16(offset);
        for (std::uint16_t index = 0; index < count; ++index)
        {
            const std::uint32_t record = offset + 2 + index * 6;

            FeatureRef feature;
            feature.tag = ReadU32(record);
            feature.substitution = substitution;

            const std::uint32_t featureTable = offset + ReadU16(record + 4);
            const std::uint16_t lookupCount = ReadU16(featureTable + 2);
            for (std::uint16_t lookup = 0; lookup < lookupCount; ++lookup)
            {
                feature.lookups.push_back(ReadU16(featureTable + 4 + lookup * 2));
            }

            features.push_back(std::move(feature));
        }
    }

    void OtLayout::ParseScriptList(std::uint32_t offset, bool substitution)
    {
        (void)substitution;
        const std::uint16_t count = ReadU16(offset);

        for (std::uint16_t index = 0; index < count; ++index)
        {
            const std::uint32_t record = offset + 2 + index * 6;
            const std::uint32_t scriptTag = ReadU32(record);
            const std::uint32_t script = offset + ReadU16(record + 4);

            std::vector<std::pair<std::uint32_t, std::vector<std::uint16_t>>> languages;

            const auto readLangSys = [&](std::uint32_t langSys) {
                std::vector<std::uint16_t> indices;
                const std::uint16_t featureCount = ReadU16(langSys + 4);
                indices.reserve(featureCount);
                for (std::uint16_t feature = 0; feature < featureCount; ++feature)
                {
                    const std::uint16_t raw = ReadU16(langSys + 6 + feature * 2);
                    indices.push_back(static_cast<std::uint16_t>(raw + featureBase));
                }
                return indices;
            };

            const std::uint16_t defaultLangSys = ReadU16(script);
            if (defaultLangSys != 0)
            {
                languages.emplace_back(0, readLangSys(script + defaultLangSys));
            }

            const std::uint16_t langCount = ReadU16(script + 2);
            for (std::uint16_t lang = 0; lang < langCount; ++lang)
            {
                const std::uint32_t langRecord = script + 4 + lang * 6;
                const std::uint32_t langTag = ReadU32(langRecord);
                const std::uint32_t langSys = script + ReadU16(langRecord + 4);
                languages.emplace_back(langTag, readLangSys(langSys));
            }

            // One entry per script: every language system that declares features.
            scripts.emplace_back(scriptTag, std::move(languages));
        }
    }

    void OtLayout::ExpandClassDef(std::uint32_t offset, std::vector<std::uint8_t>& out) const
    {
        const std::uint16_t format = ReadU16(offset);

        if (format == 1)
        {
            const std::uint16_t start = ReadU16(offset + 2);
            const std::uint16_t count = ReadU16(offset + 4);
            for (std::uint16_t index = 0; index < count; ++index)
            {
                const std::uint32_t glyph = start + index;
                if (glyph >= 0x10000u)
                {
                    break;
                }
                if (out.size() <= glyph)
                {
                    out.resize(glyph + 1, 0);
                }
                out[glyph] = static_cast<std::uint8_t>(ReadU16(offset + 6 + index * 2));
            }
        }
        else if (format == 2)
        {
            const std::uint16_t rangeCount = ReadU16(offset + 2);
            for (std::uint16_t index = 0; index < rangeCount; ++index)
            {
                const std::uint32_t range = offset + 4 + index * 6;
                const std::uint32_t start = ReadU16(range);
                const std::uint32_t end = ReadU16(range + 2);
                const std::uint8_t value = static_cast<std::uint8_t>(ReadU16(range + 4));

                if (start > end || end > 0xFFFFu)
                {
                    continue;
                }
                // A malformed range is clamped rather than allocated blindly.
                const std::uint32_t capped = std::min<std::uint32_t>(end, 0xFFFFu);
                if (out.size() <= capped)
                {
                    out.resize(capped + 1, 0);
                }
                for (std::uint32_t glyph = start; glyph <= capped; ++glyph)
                {
                    out[glyph] = value;
                }
            }
        }
    }

    void OtLayout::ParseGdef(std::uint32_t offset)
    {
        if (static_cast<std::uint64_t>(offset) + 12 > size)
        {
            return;
        }

        const std::uint16_t minor = ReadU16(offset + 2);

        const std::uint16_t glyphClassDef = ReadU16(offset + 4);
        if (glyphClassDef != 0)
        {
            ExpandClassDef(offset + glyphClassDef, glyphClasses);
        }

        const std::uint16_t markAttachClassDef = ReadU16(offset + 10);
        if (markAttachClassDef != 0)
        {
            ExpandClassDef(offset + markAttachClassDef, markAttachClasses);
        }

        if (minor >= 2)
        {
            const std::uint16_t markGlyphSetsDef = ReadU16(offset + 12);
            if (markGlyphSetsDef != 0)
            {
                const std::uint32_t sets = offset + markGlyphSetsDef;
                const std::uint16_t setCount = ReadU16(sets + 2);
                for (std::uint16_t set = 0; set < setCount; ++set)
                {
                    const std::uint32_t coverage = sets + ReadU32(sets + 4 + set * 4);
                    std::vector<std::uint16_t> glyphsInSet;

                    const std::uint16_t format = ReadU16(coverage);
                    if (format == 1)
                    {
                        const std::uint16_t glyphCount = ReadU16(coverage + 2);
                        for (std::uint16_t g = 0; g < glyphCount; ++g)
                        {
                            glyphsInSet.push_back(ReadU16(coverage + 4 + g * 2));
                        }
                    }
                    else if (format == 2)
                    {
                        const std::uint16_t rangeCount = ReadU16(coverage + 2);
                        for (std::uint16_t r = 0; r < rangeCount; ++r)
                        {
                            const std::uint32_t range = coverage + 4 + r * 6;
                            const std::uint32_t start = ReadU16(range);
                            const std::uint32_t end = ReadU16(range + 2);
                            for (std::uint32_t g = start; g <= end && g <= 0xFFFFu; ++g)
                            {
                                glyphsInSet.push_back(static_cast<std::uint16_t>(g));
                            }
                        }
                    }

                    std::sort(glyphsInSet.begin(), glyphsInSet.end());
                    markGlyphSets.push_back(std::move(glyphsInSet));
                }
            }
        }
    }

    // --- GDEF queries ------------------------------------------------------

    std::uint16_t OtLayout::GlyphClass(std::uint16_t glyph) const
    {
        return glyph < glyphClasses.size() ? glyphClasses[glyph] : 0;
    }

    bool OtLayout::IsMarkGlyph(std::uint16_t glyph) const
    {
        return GlyphClass(glyph) == 3;
    }

    bool OtLayout::MarkCovered(std::uint16_t glyph, std::uint32_t markSetIndex) const
    {
        if (markSetIndex >= markGlyphSets.size())
        {
            return false;
        }
        const std::vector<std::uint16_t>& set = markGlyphSets[markSetIndex];
        return std::binary_search(set.begin(), set.end(), glyph);
    }

    // --- shared table readers ----------------------------------------------

    std::uint32_t OtLayout::CoverageIndex(std::uint32_t coverageOffset, std::uint16_t glyph) const
    {
        const std::uint16_t format = ReadU16(coverageOffset);

        if (format == 1)
        {
            const std::uint16_t count = ReadU16(coverageOffset + 2);
            std::uint32_t low = 0;
            std::uint32_t high = count;
            while (low < high)
            {
                const std::uint32_t mid = (low + high) / 2;
                if (ReadU16(coverageOffset + 4 + mid * 2) < glyph)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            if (low < count && ReadU16(coverageOffset + 4 + low * 2) == glyph)
            {
                return low;
            }
            return kNoCoverage;
        }

        if (format == 2)
        {
            const std::uint16_t rangeCount = ReadU16(coverageOffset + 2);
            for (std::uint16_t index = 0; index < rangeCount; ++index)
            {
                const std::uint32_t range = coverageOffset + 4 + index * 6;
                const std::uint32_t start = ReadU16(range);
                const std::uint32_t end = ReadU16(range + 2);
                if (glyph >= start && glyph <= end)
                {
                    return ReadU16(range + 4) + (glyph - start);
                }
            }
        }

        return kNoCoverage;
    }

    std::uint16_t OtLayout::ClassOf(std::uint32_t classDefOffset, std::uint16_t glyph) const
    {
        const std::uint16_t format = ReadU16(classDefOffset);

        if (format == 1)
        {
            const std::uint16_t start = ReadU16(classDefOffset + 2);
            const std::uint16_t count = ReadU16(classDefOffset + 4);
            if (glyph >= start && glyph < start + count)
            {
                return ReadU16(classDefOffset + 6 + (glyph - start) * 2);
            }
            return 0;
        }

        if (format == 2)
        {
            const std::uint16_t rangeCount = ReadU16(classDefOffset + 2);
            for (std::uint16_t index = 0; index < rangeCount; ++index)
            {
                const std::uint32_t range = classDefOffset + 4 + index * 6;
                if (glyph >= ReadU16(range) && glyph <= ReadU16(range + 2))
                {
                    return ReadU16(range + 4);
                }
            }
        }

        return 0;
    }

    bool OtLayout::HasScript(std::uint32_t scriptTag) const
    {
        for (const auto& script : scripts)
        {
            if (script.first == scriptTag)
            {
                return true;
            }
        }
        return false;
    }

    bool OtLayout::HasFeature(std::uint32_t scriptTag, std::uint32_t featureTag) const
    {
        for (const auto& script : scripts)
        {
            if (script.first != scriptTag)
            {
                continue;
            }
            for (const auto& language : script.second)
            {
                for (std::uint16_t featureIndex : language.second)
                {
                    if (featureIndex < features.size() && features[featureIndex].tag == featureTag)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    std::vector<std::uint16_t> OtLayout::FeatureLookups(
        std::uint32_t scriptTag,
        const std::vector<std::uint32_t>& featureTags,
        bool substitution) const
    {
        std::vector<std::uint16_t> lookups;

        for (const auto& script : scripts)
        {
            if (script.first != scriptTag)
            {
                continue;
            }

            for (const auto& language : script.second)
            {
                for (std::uint16_t featureIndex : language.second)
                {
                    if (featureIndex >= features.size())
                    {
                        continue;
                    }
                    const FeatureRef& feature = features[featureIndex];
                    if (feature.substitution != substitution)
                    {
                        continue;
                    }
                    if (std::find(featureTags.begin(), featureTags.end(), feature.tag) ==
                        featureTags.end())
                    {
                        continue;
                    }
                    lookups.insert(lookups.end(), feature.lookups.begin(), feature.lookups.end());
                }
            }
        }

        return lookups;
    }

    // --- feature tables ----------------------------------------------------

    std::vector<std::uint32_t> OtLayout::EffectiveFeatures(const ShapeSettings* settings,
                                                           bool substitution) const
    {
        // The features that are ON by default, in the order a shaper applies
        // them. Discretionary features (`dlig`, `salt`, `smcp`, `onum`, ...) are
        // deliberately absent: the document has to ask for them with
        // `font-feature-settings`, and until it does, enabling them would
        // silently change text.
        static const std::uint32_t kDefaultSubstitution[] = {
            // Shared and Latin.
            Tag4('c', 'c', 'm', 'p'), Tag4('l', 'o', 'c', 'l'), Tag4('r', 'l', 'i', 'g'),
            Tag4('l', 'i', 'g', 'a'), Tag4('c', 'l', 'i', 'g'), Tag4('c', 'a', 'l', 't'),
            // Arabic, Syriac and N'Ko joining.
            Tag4('i', 's', 'o', 'l'), Tag4('i', 'n', 'i', 't'), Tag4('m', 'e', 'd', 'i'),
            Tag4('f', 'i', 'n', 'a'), Tag4('m', 's', 'e', 't'), Tag4('m', 'e', 'd', '2'),
            Tag4('f', 'i', 'n', '2'), Tag4('f', 'i', 'n', '3'),
            // Indic: conjunct formation and presentation forms.
            Tag4('n', 'u', 'k', 't'), Tag4('a', 'k', 'h', 'n'), Tag4('r', 'p', 'h', 'f'),
            Tag4('r', 'k', 'r', 'f'), Tag4('p', 'r', 'e', 'f'), Tag4('b', 'l', 'w', 'f'),
            Tag4('h', 'a', 'l', 'f'), Tag4('a', 'b', 'v', 'f'), Tag4('p', 's', 't', 'f'),
            Tag4('v', 'a', 't', 'u'), Tag4('c', 'j', 'c', 't'), Tag4('c', 'f', 'a', 'r'),
            Tag4('p', 'r', 'e', 's'), Tag4('a', 'b', 'v', 's'), Tag4('b', 'l', 'w', 's'),
            Tag4('p', 's', 't', 's'), Tag4('h', 'a', 'l', 'n'),
            // GPOS defaults ride in the same list so one rule decides both
            // tables; `kern` is dropped below when the document says so.
            Tag4('m', 'a', 'r', 'k'), Tag4('m', 'k', 'm', 'k'),
            Tag4('c', 'u', 'r', 's'), Tag4('d', 'i', 's', 't'),
        };

        static const std::uint32_t kDefaultPositioning[] = {
            Tag4('k', 'e', 'r', 'n'), Tag4('m', 'a', 'r', 'k'),
            Tag4('m', 'k', 'm', 'k'), Tag4('c', 'u', 'r', 's'),
            Tag4('d', 'i', 's', 't'),
        };

        const std::uint32_t kernTag = Tag4('k', 'e', 'r', 'n');

        std::vector<std::uint32_t> tags;

        const auto requested = [settings](std::uint32_t tag, std::uint16_t& value) {
            if (settings == nullptr || settings->features == nullptr)
            {
                return false;
            }

            for (std::size_t index = 0; index < settings->featureCount; ++index)
            {
                if (settings->features[index].tag == tag)
                {
                    value = settings->features[index].value;
                    return true;
                }
            }

            return false;
        };

        if (substitution)
        {
            for (std::uint32_t tag : kDefaultSubstitution)
            {
                std::uint16_t value = 0;

                if (requested(tag, value) && value == 0)
                {
                    continue;
                }

                tags.push_back(tag);
            }
        }
        else
        {
            for (std::uint32_t tag : kDefaultPositioning)
            {
                std::uint16_t value = 0;

                // `font-kerning: none` is how a document removes kerning; the
                // default `auto` leaves it on.
                if (tag == kernTag && settings != nullptr && !settings->kerning)
                {
                    continue;
                }

                if (requested(tag, value) && value == 0)
                {
                    continue;
                }

                tags.push_back(tag);
            }
        }

        // Everything the document asked for by name, whether or not it is a
        // default. A tag already in the list is not applied twice, and a tag
        // turned off above is not brought back by an earlier entry.
        if (settings != nullptr && settings->features != nullptr)
        {
            for (std::size_t index = 0; index < settings->featureCount; ++index)
            {
                const FontFeatureSetting& setting = settings->features[index];

                if (setting.value == 0 || setting.tag == 0)
                {
                    continue;
                }

                if (std::find(tags.begin(), tags.end(), setting.tag) == tags.end())
                {
                    tags.push_back(setting.tag);
                }
            }
        }

        return tags;
    }

    void OtLayout::ApplySubstitution(std::uint32_t scriptTag,
                                     std::vector<ShapedGlyph>& glyphs,
                                     const ShapeSettings* settings) const
    {
        if (!hasSubstitution || scriptTag == 0)
        {
            return;
        }

        for (std::uint32_t tag : EffectiveFeatures(settings, true))
        {
            const std::vector<std::uint16_t> lookups =
                FeatureLookups(scriptTag, {tag}, true);
            if (!lookups.empty())
            {
                ApplyLookups(lookups, substitutionLookups, glyphs);
            }
        }
    }

    void OtLayout::ApplyPositioning(std::uint32_t scriptTag,
                                    std::vector<ShapedGlyph>& glyphs,
                                    const ShapeSettings* settings) const
    {
        if (!hasPositioning || scriptTag == 0)
        {
            return;
        }

        for (std::uint32_t tag : EffectiveFeatures(settings, false))
        {
            const std::vector<std::uint16_t> lookups =
                FeatureLookups(scriptTag, {tag}, false);
            if (!lookups.empty())
            {
                ApplyLookups(lookups, positioningLookups, glyphs);
            }
        }
    }

    void OtLayout::Shape(std::uint32_t scriptTag,
                         std::vector<ShapedGlyph>& glyphs,
                         const ShapeSettings* settings) const
    {
        if (glyphs.empty())
        {
            return;
        }

        for (ShapedGlyph& glyph : glyphs)
        {
            glyph.mark = IsMarkGlyph(glyph.glyph);
        }

        ApplySubstitution(scriptTag, glyphs, settings);
        ApplyPositioning(scriptTag, glyphs, settings);

        for (ShapedGlyph& glyph : glyphs)
        {
            glyph.mark = IsMarkGlyph(glyph.glyph);
        }
    }

    // --- lookup application ------------------------------------------------

    bool OtLayout::ShouldSkip(const LookupRef& lookup, std::uint16_t glyph) const
    {
        const std::uint16_t flags = lookup.flags;
        const std::uint16_t glyphClass = GlyphClass(glyph);

        if ((flags & 0x0010u) != 0 && glyphClass == 3)
        {
            // A mark is only considered when it is in the lookup's set.
            if (!MarkCovered(glyph, lookup.markFilteringSet))
            {
                return true;
            }
        }

        if ((flags & 0x0002u) != 0 && glyphClass == 1) // ignore base glyphs
        {
            return true;
        }
        if ((flags & 0x0004u) != 0 && glyphClass == 2) // ignore ligatures
        {
            return true;
        }
        if ((flags & 0x0008u) != 0 && glyphClass == 3) // ignore marks
        {
            return true;
        }

        const std::uint8_t attachClass = static_cast<std::uint8_t>((flags & 0xFF00u) >> 8);
        if (attachClass != 0)
        {
            const std::uint8_t actual = glyph < markAttachClasses.size() ? markAttachClasses[glyph] : 0;
            if (actual != attachClass)
            {
                return true;
            }
        }

        return false;
    }

    void OtLayout::ApplyLookups(const std::vector<std::uint16_t>& lookupIndices,
                                const std::vector<LookupRef>& pool,
                                std::vector<ShapedGlyph>& glyphs) const
    {
        contextPool = &pool;
        contextSubstitution = (&pool == &substitutionLookups);

        // Each lookup restarts at the beginning of the run: a later lookup in a
        // feature may act on what an earlier one produced.
        for (std::uint16_t lookupIndex : lookupIndices)
        {
            if (lookupIndex >= pool.size())
            {
                continue;
            }
            const LookupRef& lookup = pool[lookupIndex];

            std::size_t index = 0;
            while (index < glyphs.size())
            {
                ApplyLookup(lookup, glyphs, index);
            }
        }
    }

    bool OtLayout::ApplyLookup(const LookupRef& lookup,
                               std::vector<ShapedGlyph>& glyphs,
                               std::size_t& index) const
    {
        if (index >= glyphs.size())
        {
            return false;
        }

        if (ShouldSkip(lookup, glyphs[index].glyph))
        {
            ++index;
            return false;
        }

        const bool substitution = contextSubstitution;

        for (std::uint32_t subtable : lookup.subtableOffsets)
        {
            const std::size_t before = glyphs.size();
            std::size_t probe = index;
            bool applied = false;

            switch (lookup.type)
            {
            case 1:
                applied = substitution ? ApplySingleSubstitution(subtable, glyphs, index)
                                       : ApplySinglePositioning(subtable, glyphs, index);
                break;
            case 2:
                applied = substitution ? ApplyMultipleSubstitution(subtable, glyphs, index)
                                       : ApplyPairPositioning(subtable, glyphs, index);
                break;
            case 3:
                applied = substitution ? ApplyAlternateSubstitution(subtable, glyphs, index)
                                       : ApplyCursive(subtable, glyphs, index);
                break;
            case 4:
                applied = substitution ? ApplyLigatureSubstitution(subtable, glyphs, index)
                                       : ApplyMarkToBase(subtable, glyphs, index);
                break;
            case 5:
                applied = substitution ? ApplyContext(subtable, subtable, glyphs, index, false, true)
                                       : ApplyMarkToBase(subtable, glyphs, index);
                break;
            case 6:
                applied = substitution ? ApplyContext(subtable, subtable, glyphs, index, true, true)
                                       : ApplyMarkToMark(subtable, glyphs, index);
                break;
            case 7:
                applied = substitution ? ApplyExtension(subtable, glyphs, probe, true)
                                       : ApplyContext(subtable, subtable, glyphs, index, false, false);
                break;
            case 8:
                applied = substitution ? ApplyReverseChain(subtable, glyphs, index)
                                       : ApplyContext(subtable, subtable, glyphs, index, true, false);
                break;
            case 9:
                applied = substitution ? false : ApplyExtension(subtable, glyphs, probe, false);
                break;
            default:
                break;
            }

            if (applied)
            {
                if (glyphs.size() > before)
                {
                    index += 1 + (glyphs.size() - before);
                }
                else
                {
                    ++index;
                }
                return true;
            }

            // An extension subtable that changed length leaves `index` alone.
            (void)probe;
        }

        ++index;
        return false;
    }

    bool OtLayout::ApplyLookupAtIndex(std::uint32_t tableOffset,
                                      std::uint16_t lookupIndex,
                                      std::vector<ShapedGlyph>& glyphs,
                                      std::size_t& index,
                                      bool substitution) const
    {
        (void)tableOffset;
        (void)substitution;

        const std::vector<LookupRef>& pool =
            contextSubstitution ? substitutionLookups : positioningLookups;
        if (lookupIndex >= pool.size())
        {
            return false;
        }

        std::size_t probe = index;
        const bool applied = ApplyLookup(pool[lookupIndex], glyphs, probe);
        index = probe;
        return applied;
    }

    bool OtLayout::ApplyExtension(std::uint32_t subtable,
                                  std::vector<ShapedGlyph>& glyphs,
                                  std::size_t& index,
                                  bool substitution) const
    {
        // Extension: uint16 format == 1, uint16 extensionLookupType, uint32 offset.
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1)
        {
            return false;
        }

        const std::uint16_t extensionType = ReadU16(subtable + 2);
        const std::uint32_t extension = subtable + ReadU32(subtable + 4);

        switch (extensionType)
        {
        case 1:
            return substitution ? ApplySingleSubstitution(extension, glyphs, index)
                                : ApplySinglePositioning(extension, glyphs, index);
        case 2:
            return substitution ? ApplyMultipleSubstitution(extension, glyphs, index)
                                : ApplyPairPositioning(extension, glyphs, index);
        case 3:
            return substitution ? ApplyAlternateSubstitution(extension, glyphs, index)
                                : ApplyCursive(extension, glyphs, index);
        case 4:
            return substitution ? ApplyLigatureSubstitution(extension, glyphs, index)
                                : ApplyMarkToBase(extension, glyphs, index);
        case 5:
            return substitution ? ApplyContext(extension, extension, glyphs, index, false, true)
                                : ApplyMarkToBase(extension, glyphs, index);
        case 6:
            return substitution ? ApplyContext(extension, extension, glyphs, index, true, true)
                                : ApplyMarkToMark(extension, glyphs, index);
        case 7:
            return substitution ? false : ApplyContext(extension, extension, glyphs, index, false, false);
        case 8:
            if (substitution)
            {
                return ApplyReverseChain(extension, glyphs, index);
            }
            return ApplyContext(extension, extension, glyphs, index, true, false);
        default:
            return false;
        }
    }

    // --- GSUB subtables ----------------------------------------------------

    bool OtLayout::ApplySingleSubstitution(std::uint32_t subtable,
                                           std::vector<ShapedGlyph>& glyphs,
                                           std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex == kNoCoverage)
        {
            return false;
        }

        if (format == 1)
        {
            const std::int16_t delta = ReadI16(subtable + 4);
            glyphs[index].glyph = static_cast<std::uint16_t>(glyphs[index].glyph + delta);
        }
        else if (format == 2)
        {
            glyphs[index].glyph = ReadU16(subtable + 6 + coverageIndex * 2);
        }
        else
        {
            return false;
        }

        return true;
    }

    bool OtLayout::ApplyMultipleSubstitution(std::uint32_t subtable,
                                             std::vector<ShapedGlyph>& glyphs,
                                             std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1)
        {
            return false;
        }

        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex == kNoCoverage)
        {
            return false;
        }

        const std::uint32_t sequence = subtable + ReadU16(subtable + 6 + coverageIndex * 2);
        const std::uint16_t glyphCount = ReadU16(sequence);

        const ShapedGlyph source = glyphs[index];
        glyphs.erase(glyphs.begin() + static_cast<std::ptrdiff_t>(index));

        std::vector<ShapedGlyph> replacement;
        replacement.reserve(glyphCount);
        for (std::uint16_t i = 0; i < glyphCount; ++i)
        {
            ShapedGlyph glyph = source;
            glyph.glyph = ReadU16(sequence + 2 + i * 2);
            glyph.components = 1;
            replacement.push_back(glyph);
        }

        glyphs.insert(glyphs.begin() + static_cast<std::ptrdiff_t>(index),
                      replacement.begin(),
                      replacement.end());
        return true;
    }

    bool OtLayout::ApplyAlternateSubstitution(std::uint32_t subtable,
                                              std::vector<ShapedGlyph>& glyphs,
                                              std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1)
        {
            return false;
        }

        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex == kNoCoverage)
        {
            return false;
        }

        const std::uint32_t alternateSet = subtable + ReadU16(subtable + 6 + coverageIndex * 2);
        const std::uint16_t count = ReadU16(alternateSet);
        if (count == 0)
        {
            return false;
        }

        // No user-facing feature selector yet: the first alternate is the one
        // the font lists, which is what a default 'salt'-less run wants.
        glyphs[index].glyph = ReadU16(alternateSet + 2);
        return true;
    }

    bool OtLayout::ApplyLigatureSubstitution(std::uint32_t subtable,
                                             std::vector<ShapedGlyph>& glyphs,
                                             std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1)
        {
            return false;
        }

        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex == kNoCoverage)
        {
            return false;
        }

        const std::uint32_t ligatureSet = subtable + ReadU16(subtable + 6 + coverageIndex * 2);
        const std::uint16_t ligatureCount = ReadU16(ligatureSet);

        for (std::uint16_t ligature = 0; ligature < ligatureCount; ++ligature)
        {
            const std::uint32_t table = ligatureSet + ReadU16(ligatureSet + 2 + ligature * 2);
            const std::uint16_t ligatureGlyph = ReadU16(table);
            const std::uint16_t componentCount = ReadU16(table + 2);
            if (componentCount == 0 || index + componentCount > glyphs.size())
            {
                continue;
            }

            bool matches = true;
            for (std::uint16_t component = 1; component < componentCount; ++component)
            {
                if (glyphs[index + component].glyph != ReadU16(table + 2 + component * 2))
                {
                    matches = false;
                    break;
                }
            }

            if (!matches)
            {
                continue;
            }

            ShapedGlyph ligature_ = glyphs[index];
            ligature_.glyph = ligatureGlyph;
            ligature_.components = static_cast<std::uint8_t>(
                std::min<std::uint16_t>(componentCount, 255));

            glyphs[index] = ligature_;
            glyphs.erase(glyphs.begin() + static_cast<std::ptrdiff_t>(index + 1),
                         glyphs.begin() + static_cast<std::ptrdiff_t>(index + componentCount));
            return true;
        }

        return false;
    }

    bool OtLayout::ApplyReverseChain(std::uint32_t subtable,
                                     std::vector<ShapedGlyph>& glyphs,
                                     std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1)
        {
            return false;
        }

        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        if (CoverageIndex(coverage, glyphs[index].glyph) == kNoCoverage)
        {
            return false;
        }

        std::uint32_t cursor = subtable + 4;

        const std::uint16_t backtrackCount = ReadU16(cursor);
        cursor += 2;
        const std::uint32_t backtrackBase = cursor;
        cursor += static_cast<std::uint32_t>(backtrackCount) * 2;

        const std::uint16_t lookaheadCount = ReadU16(cursor);
        cursor += 2;
        const std::uint32_t lookaheadBase = cursor;
        cursor += static_cast<std::uint32_t>(lookaheadCount) * 2;

        const std::uint16_t glyphCount = ReadU16(cursor);
        cursor += 2;
        const std::uint32_t substituteBase = cursor;

        for (std::uint16_t i = 0; i < backtrackCount; ++i)
        {
            if (index < static_cast<std::size_t>(i) + 1)
            {
                return false;
            }
            const std::uint32_t cov = subtable + ReadU16(backtrackBase + i * 2);
            if (CoverageIndex(cov, glyphs[index - 1 - i].glyph) == kNoCoverage)
            {
                return false;
            }
        }

        for (std::uint16_t i = 0; i < lookaheadCount; ++i)
        {
            if (index + 1 + i >= glyphs.size())
            {
                return false;
            }
            const std::uint32_t cov = subtable + ReadU16(lookaheadBase + i * 2);
            if (CoverageIndex(cov, glyphs[index + 1 + i].glyph) == kNoCoverage)
            {
                return false;
            }
        }

        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex >= glyphCount)
        {
            return false;
        }

        glyphs[index].glyph = ReadU16(substituteBase + coverageIndex * 2);
        return true;
    }

    // --- GPOS subtables ----------------------------------------------------

    std::uint32_t OtLayout::ValueRecordSize(std::uint16_t valueFormat) const
    {
        std::uint32_t recordSize = 0;
        for (int bit = 0; bit < 8; ++bit)
        {
            if ((valueFormat & (1u << bit)) != 0)
            {
                recordSize += 2;
            }
        }
        return recordSize;
    }

    void OtLayout::ApplyValueRecord(std::uint32_t offset,
                                    std::uint16_t valueFormat,
                                    ShapedGlyph& glyph) const
    {
        std::uint32_t cursor = offset;

        const auto next = [&]() -> std::int16_t {
            const std::int16_t value = ReadI16(cursor);
            cursor += 2;
            return value;
        };

        // Value records are signed 16-bit values in font design units, which is
        // the same unit `ShapedGlyph` carries. They are *not* `F2Dot14`; only
        // variation data and anchors in device space use that encoding.
        if ((valueFormat & 0x0001u) != 0)
        {
            glyph.xOffset += static_cast<float>(next());
        }
        if ((valueFormat & 0x0002u) != 0)
        {
            glyph.yOffset += static_cast<float>(next());
        }
        if ((valueFormat & 0x0004u) != 0)
        {
            glyph.xAdvance += static_cast<float>(next());
        }
        if ((valueFormat & 0x0008u) != 0)
        {
            glyph.yAdvance += static_cast<float>(next());
        }

        // Device tables (bits 4..7) are skipped, but their offsets still occupy
        // space in the record and must be stepped over.
        for (int bit = 4; bit < 8; ++bit)
        {
            if ((valueFormat & (1u << bit)) != 0)
            {
                (void)next();
            }
        }
    }

    void OtLayout::ApplyAnchor(std::uint32_t offset, ShapedGlyph& glyph, bool markSide) const
    {
        (void)markSide;

        const std::uint16_t format = ReadU16(offset);
        if (format == 0)
        {
            glyph.xOffset = 0.0f;
            glyph.yOffset = 0.0f;
            return;
        }

        glyph.xOffset = static_cast<float>(ReadI16(offset + 2));
        glyph.yOffset = static_cast<float>(ReadI16(offset + 4));
    }

    bool OtLayout::ApplySinglePositioning(std::uint32_t subtable,
                                          std::vector<ShapedGlyph>& glyphs,
                                          std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex == kNoCoverage)
        {
            return false;
        }

        const std::uint16_t valueFormat = ReadU16(subtable + 4);
        const std::uint32_t valueSize = ValueRecordSize(valueFormat);

        if (format == 1)
        {
            ApplyValueRecord(subtable + 6, valueFormat, glyphs[index]);
            return true;
        }

        if (format == 2)
        {
            ApplyValueRecord(subtable + 8 + coverageIndex * valueSize, valueFormat, glyphs[index]);
            return true;
        }

        return false;
    }

    bool OtLayout::ApplyPairPositioning(std::uint32_t subtable,
                                        std::vector<ShapedGlyph>& glyphs,
                                        std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (index + 1 >= glyphs.size())
        {
            return false;
        }

        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        if (CoverageIndex(coverage, glyphs[index].glyph) == kNoCoverage)
        {
            return false;
        }

        const std::uint16_t valueFormat1 = ReadU16(subtable + 4);
        const std::uint16_t valueFormat2 = ReadU16(subtable + 6);
        const std::uint32_t size1 = ValueRecordSize(valueFormat1);
        const std::uint32_t size2 = ValueRecordSize(valueFormat2);

        if (format == 1)
        {
            const std::uint16_t pairSetCount = ReadU16(subtable + 8);

            std::uint32_t pairSet = 0;
            for (std::uint16_t set = 0; set < pairSetCount; ++set)
            {
                const std::uint32_t candidate = subtable + ReadU16(subtable + 10 + set * 2);
                const std::uint16_t count = ReadU16(candidate);
                for (std::uint16_t pair = 0; pair < count; ++pair)
                {
                    const std::uint32_t record = candidate + 2 + pair * (2 + size1 + size2);
                    if (ReadU16(record) == glyphs[index + 1].glyph)
                    {
                        pairSet = record + 2;
                        break;
                    }
                }
                if (pairSet != 0)
                {
                    break;
                }
            }

            if (pairSet == 0)
            {
                return false;
            }

            ApplyValueRecord(pairSet, valueFormat1, glyphs[index]);
            ApplyValueRecord(pairSet + size1, valueFormat2, glyphs[index + 1]);
            return true;
        }

        if (format == 2)
        {
            const std::uint32_t classDef1 = subtable + ReadU16(subtable + 8);
            const std::uint32_t classDef2 = subtable + ReadU16(subtable + 10);
            const std::uint16_t class1Count = ReadU16(subtable + 12);
            const std::uint16_t class2Count = ReadU16(subtable + 14);

            const std::uint16_t class1 = ClassOf(classDef1, glyphs[index].glyph);
            const std::uint16_t class2 = ClassOf(classDef2, glyphs[index + 1].glyph);
            if (class1 >= class1Count || class2 >= class2Count)
            {
                return false;
            }

            const std::uint32_t recordSize = size1 + size2;
            const std::uint32_t offset = subtable + 16 +
                (static_cast<std::uint32_t>(class1) * class2Count + class2) * recordSize;

            ApplyValueRecord(offset, valueFormat1, glyphs[index]);
            ApplyValueRecord(offset + size1, valueFormat2, glyphs[index + 1]);
            return true;
        }

        return false;
    }

    bool OtLayout::ApplyCursive(std::uint32_t subtable,
                                std::vector<ShapedGlyph>& glyphs,
                                std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1 || index + 1 >= glyphs.size())
        {
            return false;
        }

        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t firstIndex = CoverageIndex(coverage, glyphs[index].glyph);
        const std::uint32_t secondIndex = CoverageIndex(coverage, glyphs[index + 1].glyph);
        if (firstIndex == kNoCoverage || secondIndex == kNoCoverage)
        {
            return false;
        }

        const std::uint16_t count = ReadU16(subtable + 4);
        if (firstIndex >= count || secondIndex >= count)
        {
            return false;
        }

        // EntryExitRecord[count] of {entryAnchor, exitAnchor}.
        const std::uint32_t firstRecord = subtable + 6 + firstIndex * 4;
        const std::uint32_t secondRecord = subtable + 6 + secondIndex * 4;

        const std::uint32_t exitAnchor = ReadU16(firstRecord + 2);
        const std::uint32_t entryAnchor = ReadU16(secondRecord);
        if (exitAnchor == 0 || entryAnchor == 0)
        {
            return false;
        }

        ShapedGlyph exit;
        ShapedGlyph entry;
        ApplyAnchor(subtable + exitAnchor, exit, false);
        ApplyAnchor(subtable + entryAnchor, entry, true);

        // The second glyph is pulled so its entry anchor meets the first's exit.
        glyphs[index + 1].xOffset = exit.xOffset - entry.xOffset;
        glyphs[index + 1].yOffset = exit.yOffset - entry.yOffset;
        return true;
    }

    bool OtLayout::ApplyMarkToBase(std::uint32_t subtable,
                                   std::vector<ShapedGlyph>& glyphs,
                                   std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1 || index == 0)
        {
            return false;
        }

        const std::uint32_t markCoverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t baseCoverage = subtable + ReadU16(subtable + 4);
        const std::uint16_t classCount = ReadU16(subtable + 6);
        const std::uint32_t markArray = subtable + ReadU16(subtable + 8);
        const std::uint32_t baseArray = subtable + ReadU16(subtable + 10);

        const std::uint32_t markIndex = CoverageIndex(markCoverage, glyphs[index].glyph);
        if (markIndex == kNoCoverage)
        {
            return false;
        }

        // The nearest preceding glyph the base coverage knows.
        std::size_t baseIndex = index;
        bool found = false;
        while (baseIndex > 0)
        {
            --baseIndex;
            if (CoverageIndex(baseCoverage, glyphs[baseIndex].glyph) != kNoCoverage)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }

        const std::uint32_t baseCoverageIndex =
            CoverageIndex(baseCoverage, glyphs[baseIndex].glyph);

        const std::uint32_t markRecordOffset = markArray + 2 + markIndex * 4;
        const std::uint16_t markClass = ReadU16(markRecordOffset);
        if (markClass >= classCount)
        {
            return false;
        }

        const std::uint32_t markAnchorOffset = ReadU16(markRecordOffset + 2);
        const std::uint32_t baseRecordOffset =
            baseArray + 2 + baseCoverageIndex * static_cast<std::uint32_t>(classCount) * 2;
        const std::uint32_t baseAnchorOffset = ReadU16(baseRecordOffset + markClass * 2);
        if (markAnchorOffset == 0 || baseAnchorOffset == 0)
        {
            return false;
        }

        ShapedGlyph markAnchor;
        ShapedGlyph baseAnchor;
        ApplyAnchor(markArray + markAnchorOffset, markAnchor, true);
        ApplyAnchor(baseArray + baseAnchorOffset, baseAnchor, false);

        glyphs[index].xOffset = baseAnchor.xOffset - markAnchor.xOffset;
        glyphs[index].yOffset = baseAnchor.yOffset - markAnchor.yOffset;
        return true;
    }

    bool OtLayout::ApplyMarkToMark(std::uint32_t subtable,
                                   std::vector<ShapedGlyph>& glyphs,
                                   std::size_t index) const
    {
        const std::uint16_t format = ReadU16(subtable);
        if (format != 1 || index == 0)
        {
            return false;
        }

        const std::uint32_t markCoverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t mark2Coverage = subtable + ReadU16(subtable + 4);
        const std::uint16_t classCount = ReadU16(subtable + 6);
        const std::uint32_t markArray = subtable + ReadU16(subtable + 8);
        const std::uint32_t mark2Array = subtable + ReadU16(subtable + 10);

        const std::uint32_t markIndex = CoverageIndex(markCoverage, glyphs[index].glyph);
        if (markIndex == kNoCoverage)
        {
            return false;
        }

        std::size_t baseIndex = index;
        bool found = false;
        while (baseIndex > 0)
        {
            --baseIndex;
            if (CoverageIndex(mark2Coverage, glyphs[baseIndex].glyph) != kNoCoverage)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }

        const std::uint32_t baseCoverageIndex =
            CoverageIndex(mark2Coverage, glyphs[baseIndex].glyph);

        const std::uint32_t markRecordOffset = markArray + 2 + markIndex * 4;
        const std::uint16_t markClass = ReadU16(markRecordOffset);
        if (markClass >= classCount)
        {
            return false;
        }

        const std::uint32_t markAnchorOffset = ReadU16(markRecordOffset + 2);
        const std::uint32_t baseRecordOffset =
            mark2Array + 2 + baseCoverageIndex * static_cast<std::uint32_t>(classCount) * 2;
        const std::uint32_t baseAnchorOffset = ReadU16(baseRecordOffset + markClass * 2);
        if (markAnchorOffset == 0 || baseAnchorOffset == 0)
        {
            return false;
        }

        ShapedGlyph markAnchor;
        ShapedGlyph baseAnchor;
        ApplyAnchor(markArray + markAnchorOffset, markAnchor, true);
        ApplyAnchor(mark2Array + baseAnchorOffset, baseAnchor, false);

        glyphs[index].xOffset = baseAnchor.xOffset - markAnchor.xOffset;
        glyphs[index].yOffset = baseAnchor.yOffset - markAnchor.yOffset;
        return true;
    }

    // --- contextual lookups ------------------------------------------------

    namespace
    {
        struct SubstRecord
        {
            std::uint16_t sequenceIndex = 0;
            std::uint16_t lookupIndex = 0;
        };
    }

    bool OtLayout::ApplyContext(std::uint32_t tableOffset,
                                std::uint32_t subtable,
                                std::vector<ShapedGlyph>& glyphs,
                                std::size_t index,
                                bool chaining,
                                bool substitution) const
    {
        (void)tableOffset;

        const std::uint16_t format = ReadU16(subtable);

        std::vector<std::size_t> positions;
        std::vector<SubstRecord> records;

        const auto applyRecords = [&](std::vector<std::size_t>& inputPositions,
                                      std::vector<SubstRecord>& substRecords) -> bool {
            std::sort(substRecords.begin(), substRecords.end(),
                      [](const SubstRecord& a, const SubstRecord& b) {
                          return a.sequenceIndex < b.sequenceIndex;
                      });

            const std::size_t start = inputPositions.empty() ? index : inputPositions[0];
            std::ptrdiff_t shift = 0;
            bool any = false;

            for (const SubstRecord& record : substRecords)
            {
                const std::size_t target =
                    static_cast<std::size_t>(static_cast<std::ptrdiff_t>(start) +
                                             static_cast<std::ptrdiff_t>(record.sequenceIndex) + shift);
                if (target >= glyphs.size())
                {
                    continue;
                }

                const std::size_t before = glyphs.size();
                std::size_t probe = target;

                contextPool = substitution ? &substitutionLookups : &positioningLookups;
                contextSubstitution = substitution;

                if (ApplyLookupAtIndex(tableOffset, record.lookupIndex, glyphs, probe, substitution))
                {
                    any = true;
                    if (glyphs.size() >= before)
                    {
                        shift += static_cast<std::ptrdiff_t>(glyphs.size() - before);
                    }
                    else
                    {
                        shift -= static_cast<std::ptrdiff_t>(before - glyphs.size());
                    }
                }
            }

            return any;
        };

        if (format == 3)
        {
            // Coverage lists for the input, plus backtrack/lookahead when chained.
            std::uint32_t cursor = subtable + 2;

            std::uint16_t backtrackCount = 0;
            std::uint32_t backtrackBase = 0;
            if (chaining)
            {
                backtrackCount = ReadU16(cursor);
                cursor += 2;
                backtrackBase = cursor;
                cursor += static_cast<std::uint32_t>(backtrackCount) * 2;
            }

            const std::uint16_t inputCount = ReadU16(cursor);
            cursor += 2;
            const std::uint32_t inputBase = cursor;
            cursor += static_cast<std::uint32_t>(inputCount) * 2;

            std::uint16_t lookaheadCount = 0;
            std::uint32_t lookaheadBase = 0;
            if (chaining)
            {
                lookaheadCount = ReadU16(cursor);
                cursor += 2;
                lookaheadBase = cursor;
                cursor += static_cast<std::uint32_t>(lookaheadCount) * 2;
            }

            const std::uint16_t recordCount = ReadU16(cursor);
            cursor += 2;

            if (inputCount == 0 || index + inputCount > glyphs.size())
            {
                return false;
            }

            for (std::uint16_t i = 0; i < inputCount; ++i)
            {
                const std::uint32_t coverage = subtable + ReadU16(inputBase + i * 2);
                if (CoverageIndex(coverage, glyphs[index + i].glyph) == kNoCoverage)
                {
                    return false;
                }
            }

            for (std::uint16_t i = 0; i < backtrackCount; ++i)
            {
                if (index < static_cast<std::size_t>(i) + 1)
                {
                    return false;
                }
                const std::uint32_t coverage = subtable + ReadU16(backtrackBase + i * 2);
                if (CoverageIndex(coverage, glyphs[index - 1 - i].glyph) == kNoCoverage)
                {
                    return false;
                }
            }

            for (std::uint16_t i = 0; i < lookaheadCount; ++i)
            {
                if (index + inputCount + i >= glyphs.size())
                {
                    return false;
                }
                const std::uint32_t coverage = subtable + ReadU16(lookaheadBase + i * 2);
                if (CoverageIndex(coverage, glyphs[index + inputCount + i].glyph) == kNoCoverage)
                {
                    return false;
                }
            }

            for (std::uint16_t i = 0; i < inputCount; ++i)
            {
                positions.push_back(index + i);
            }
            for (std::uint16_t i = 0; i < recordCount; ++i)
            {
                records.push_back({ReadU16(cursor + i * 4), ReadU16(cursor + i * 4 + 2)});
            }

            return applyRecords(positions, records);
        }

        // Formats 1 and 2 differ only in whether the input is matched by glyph
        // id or by class value, and in where their rule-set array sits.
        const std::uint32_t coverage = subtable + ReadU16(subtable + 2);
        const std::uint32_t coverageIndex = CoverageIndex(coverage, glyphs[index].glyph);
        if (coverageIndex == kNoCoverage)
        {
            return false;
        }

        if (!chaining)
        {
            // Contextual: format 1 is {coverage, count, offsets}; format 2 puts
            // a class definition between the coverage and the count.
            const std::uint16_t ruleSetCount = ReadU16(subtable + (format == 1 ? 4 : 6));
            if (ruleSetCount == 0)
            {
                return false;
            }
            const std::uint32_t ruleSetBase = subtable + (format == 1 ? 6 : 8);
            std::uint32_t ruleSet = 0;

            if (format == 1)
            {
                if (coverageIndex >= ruleSetCount)
                {
                    return false;
                }
                ruleSet = subtable + ReadU16(ruleSetBase + coverageIndex * 2);
            }
            else
            {
                const std::uint32_t classDef = subtable + ReadU16(subtable + 4);
                const std::uint16_t glyphClass = ClassOf(classDef, glyphs[index].glyph);
                if (glyphClass >= ruleSetCount)
                {
                    return false;
                }
                const std::uint16_t ruleSetOffset = ReadU16(ruleSetBase + glyphClass * 2);
                if (ruleSetOffset == 0)
                {
                    return false;
                }
                ruleSet = subtable + ruleSetOffset;
            }

            if (ruleSet == 0)
            {
                return false;
            }

            const std::uint16_t ruleCount = ReadU16(ruleSet);
            for (std::uint16_t rule = 0; rule < ruleCount; ++rule)
            {
                const std::uint32_t table = ruleSet + ReadU16(ruleSet + 2 + rule * 2);
                const std::uint16_t glyphCount = ReadU16(table);
                const std::uint16_t substCount = ReadU16(table + 2);
                if (glyphCount == 0 || index + glyphCount > glyphs.size())
                {
                    continue;
                }

                bool matches = true;
                for (std::uint16_t i = 1; i < glyphCount; ++i)
                {
                    const std::uint16_t wanted = ReadU16(table + 4 + (i - 1) * 2);
                    const std::uint16_t actual =
                        format == 1 ? glyphs[index + i].glyph
                                    : ClassOf(subtable + ReadU16(subtable + 4), glyphs[index + i].glyph);
                    if (wanted != actual)
                    {
                        matches = false;
                        break;
                    }
                }
                if (!matches)
                {
                    continue;
                }

                positions.clear();
                for (std::uint16_t i = 0; i < glyphCount; ++i)
                {
                    positions.push_back(index + i);
                }

                records.clear();
                const std::uint32_t recordBase = table + 4 + static_cast<std::uint32_t>(glyphCount - 1) * 2;
                for (std::uint16_t i = 0; i < substCount; ++i)
                {
                    records.push_back({ReadU16(recordBase + i * 4),
                                       ReadU16(recordBase + i * 4 + 2)});
                }
                return applyRecords(positions, records);
            }

            return false;
        }

        // Chained context, formats 1 and 2. Format 2 carries three class
        // definitions before its count, so the header is four bytes longer.
        const std::uint16_t chainRuleSetCount = ReadU16(subtable + (format == 1 ? 4 : 10));
        if (chainRuleSetCount == 0)
        {
            return false;
        }
        const std::uint32_t ruleSetBase = subtable + (format == 1 ? 6 : 12);
        std::uint32_t ruleSet = 0;

        const std::uint32_t backtrackClassDef = format == 2 ? subtable + ReadU16(subtable + 4) : 0;
        const std::uint32_t inputClassDef = format == 2 ? subtable + ReadU16(subtable + 6) : 0;
        const std::uint32_t lookaheadClassDef = format == 2 ? subtable + ReadU16(subtable + 8) : 0;

        if (format == 1)
        {
            if (coverageIndex >= chainRuleSetCount)
            {
                return false;
            }
            ruleSet = subtable + ReadU16(ruleSetBase + coverageIndex * 2);
        }
        else
        {
            const std::uint16_t glyphClass = ClassOf(inputClassDef, glyphs[index].glyph);
            if (glyphClass >= chainRuleSetCount)
            {
                return false;
            }
            const std::uint16_t ruleSetOffset = ReadU16(ruleSetBase + glyphClass * 2);
            if (ruleSetOffset == 0)
            {
                return false;
            }
            ruleSet = subtable + ruleSetOffset;
        }

        if (ruleSet == 0)
        {
            return false;
        }

        const std::uint16_t ruleCount = ReadU16(ruleSet);
        for (std::uint16_t rule = 0; rule < ruleCount; ++rule)
        {
            const std::uint32_t table = ruleSet + ReadU16(ruleSet + 2 + rule * 2);
            std::uint32_t cursor = table;

            const std::uint16_t backtrackCount = ReadU16(cursor);
            cursor += 2;
            const std::uint32_t backtrackBase = cursor;
            cursor += static_cast<std::uint32_t>(backtrackCount) * 2;

            const std::uint16_t inputCount = ReadU16(cursor);
            cursor += 2;
            const std::uint32_t inputBase = cursor;
            cursor += static_cast<std::uint32_t>(inputCount - 1) * 2;

            const std::uint16_t lookaheadCount = ReadU16(cursor);
            cursor += 2;
            const std::uint32_t lookaheadBase = cursor;
            cursor += static_cast<std::uint32_t>(lookaheadCount) * 2;

            const std::uint16_t substCount = ReadU16(cursor);
            cursor += 2;

            if (inputCount == 0 || index + inputCount > glyphs.size())
            {
                continue;
            }

            bool matches = true;

            for (std::uint16_t i = 0; i < backtrackCount && matches; ++i)
            {
                if (index < static_cast<std::size_t>(i) + 1)
                {
                    matches = false;
                    break;
                }
                const std::uint16_t wanted = ReadU16(backtrackBase + i * 2);
                const std::uint16_t actual =
                    format == 1 ? glyphs[index - 1 - i].glyph
                                : ClassOf(backtrackClassDef, glyphs[index - 1 - i].glyph);
                matches = wanted == actual;
            }

            for (std::uint16_t i = 1; i < inputCount && matches; ++i)
            {
                const std::uint16_t wanted = ReadU16(inputBase + (i - 1) * 2);
                const std::uint16_t actual =
                    format == 1 ? glyphs[index + i].glyph
                                : ClassOf(inputClassDef, glyphs[index + i].glyph);
                matches = wanted == actual;
            }

            for (std::uint16_t i = 0; i < lookaheadCount && matches; ++i)
            {
                if (index + inputCount + i >= glyphs.size())
                {
                    matches = false;
                    break;
                }
                const std::uint16_t wanted = ReadU16(lookaheadBase + i * 2);
                const std::uint16_t actual =
                    format == 1 ? glyphs[index + inputCount + i].glyph
                                : ClassOf(lookaheadClassDef, glyphs[index + inputCount + i].glyph);
                matches = wanted == actual;
            }

            if (!matches)
            {
                continue;
            }

            positions.clear();
            for (std::uint16_t i = 0; i < inputCount; ++i)
            {
                positions.push_back(index + i);
            }

            records.clear();
            for (std::uint16_t i = 0; i < substCount; ++i)
            {
                records.push_back({ReadU16(cursor + i * 4), ReadU16(cursor + i * 4 + 2)});
            }

            return applyRecords(positions, records);
        }

        return false;
    }

    bool OtLayout::ApplyContextFormat3(std::uint32_t tableOffset,
                                       std::uint32_t subtable,
                                       std::vector<ShapedGlyph>& glyphs,
                                       std::size_t index,
                                       bool chaining,
                                       bool substitution) const
    {
        return ApplyContext(tableOffset, subtable, glyphs, index, chaining, substitution);
    }

    // --- script tag lookup -------------------------------------------------

    std::uint32_t OtLayout::ScriptTagForName(const char* scriptName)
    {
        if (scriptName == nullptr)
        {
            return 0;
        }

        std::string name(scriptName);
        for (char& c : name)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        struct Entry
        {
            const char* name;
            std::uint32_t tag;
        };

        static const Entry kTable[] = {
            {"latin", Tag4('l', 'a', 't', 'n')},
            {"common", Tag4('l', 'a', 't', 'n')},
            {"inherited", Tag4('l', 'a', 't', 'n')},
            {"greek", Tag4('g', 'r', 'e', 'k')},
            {"cyrillic", Tag4('c', 'y', 'r', 'l')},
            {"arabic", Tag4('a', 'r', 'a', 'b')},
            {"hebrew", Tag4('h', 'e', 'b', 'r')},
            {"devanagari", Tag4('d', 'e', 'v', 'a')},
            {"bengali", Tag4('b', 'e', 'n', 'g')},
            {"gurmukhi", Tag4('g', 'u', 'r', 'u')},
            {"gujarati", Tag4('g', 'u', 'j', 'r')},
            {"tamil", Tag4('t', 'a', 'm', 'l')},
            {"telugu", Tag4('t', 'e', 'l', 'u')},
            {"kannada", Tag4('k', 'n', 'd', 'a')},
            {"malayalam", Tag4('m', 'l', 'y', 'm')},
            {"sinhala", Tag4('s', 'i', 'n', 'h')},
            {"thai", Tag4('t', 'h', 'a', 'i')},
            {"lao", Tag4('l', 'a', 'o', ' ')},
            {"khmer", Tag4('k', 'h', 'm', 'r')},
            {"myanmar", Tag4('m', 'y', 'm', 'r')},
            {"tibetan", Tag4('t', 'i', 'b', 't')},
            {"georgian", Tag4('g', 'e', 'o', 'r')},
            {"armenian", Tag4('a', 'r', 'm', 'n')},
            {"ethiopic", Tag4('e', 't', 'h', 'i')},
            {"cherokee", Tag4('c', 'h', 'e', 'r')},
            {"canadian_aboriginal", Tag4('c', 'a', 'n', 's')},
            {"yi", Tag4('y', 'i', ' ', ' ')},
            {"han", Tag4('h', 'a', 'n', 'i')},
            {"hiragana", Tag4('k', 'a', 'n', 'a')},
            {"katakana", Tag4('k', 'a', 'n', 'a')},
            {"hangul", Tag4('h', 'a', 'n', 'g')},
            {"mongolian", Tag4('m', 'o', 'n', 'g')},
        };

        for (const Entry& entry : kTable)
        {
            if (name == entry.name)
            {
                return entry.tag;
            }
        }

        return 0;
    }
}
