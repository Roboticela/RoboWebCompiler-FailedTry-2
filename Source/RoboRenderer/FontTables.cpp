// ---------------------------------------------------------------------------
// RoboRenderer - the font tables that are not geometry or metrics.
//
// `FontFile.cpp` reads the tables a face needs to lay out and fill letters.
// This file reads the ones a face uses to decorate them: OpenType Layout (the
// `GSUB`/`GPOS`/`GDEF` that shape a run), colour glyphs (`COLR`/`CPAL` layers,
// the `CBDT`/`CBLC` bitmaps emoji fonts ship), and the vertical metrics that
// turn a horizontal run on its side.
//
// The readers are deliberately shallow: nothing here allocates until it has
// something to return, and a table the face does not carry is answered in a
// comparison rather than an exception. A colour face has its whole colour
// surface read: COLR v0 layers, the COLR v1 paint graph walked to layer, solid
// or gradient leaves (with the transforms composed, not dropped), the
// `CBDT`/`CBLC` and `sbix` embedded bitmaps, and the `SVG ` documents.
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"

#include "RoboRenderer/Image.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>

namespace RoboRenderer
{
    // --- OpenType Layout ---------------------------------------------------

    const OtLayout* SfntFont::Layout() const
    {
        if (!layoutAttempted)
        {
            layoutAttempted = true;

            if ((gsub.present || gpos.present) && !bytes.empty())
            {
                layout = OtLayout::Create(bytes.data(),
                                          static_cast<std::uint32_t>(bytes.size()),
                                          gsub.present ? gsub.offset : 0,
                                          gsub.present ? gsub.length : 0,
                                          gpos.present ? gpos.offset : 0,
                                          gpos.present ? gpos.length : 0,
                                          gdef.present ? gdef.offset : 0,
                                          gdef.present ? gdef.length : 0);
            }
        }

        return layout.get();
    }

    // --- colour glyphs (COLR v0 / CPAL) ------------------------------------

    bool SfntFont::ColorLayers(std::uint16_t glyph, std::vector<ColorLayer>& out) const
    {
        out.clear();

        if (!colr.present || static_cast<std::uint64_t>(colr.offset) + 14 > bytes.size())
        {
            return false;
        }

        const std::uint16_t version = ReadU16(colr.offset);

        // COLR v1 replaces the flat layer list with a paint graph. Walk it and
        // flatten every glyph/solid pair back to a layer, which is all the
        // rasterizer below needs: the structure collapses to "paint this glyph
        // in this palette colour", in order.
        if (version == 1)
        {
            if (static_cast<std::uint64_t>(colr.offset) + 34 > bytes.size())
            {
                return false;
            }

            const std::uint32_t baseGlyphList = colr.offset + ReadU32(colr.offset + 14);
            const std::uint32_t layerList = colr.offset + ReadU32(colr.offset + 18);
            if (baseGlyphList == colr.offset || layerList == colr.offset)
            {
                return false;
            }

            const auto readU24 = [this](std::uint32_t offset) -> std::uint32_t {
                if (static_cast<std::uint64_t>(offset) + 3 > bytes.size())
                {
                    return 0;
                }
                return (static_cast<std::uint32_t>(bytes[offset]) << 16) |
                       (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) | bytes[offset + 2];
            };

            // BaseGlyphList is sorted by glyph id: { uint32 count; { uint16 glyphID;
            // Offset32 paint }[] }.
            const auto paintForGlyph = [&](std::uint16_t target, std::uint32_t& paint) -> bool {
                const std::uint32_t count = ReadU32(baseGlyphList);
                std::size_t low = 0;
                std::size_t high = count;
                while (low < high)
                {
                    const std::size_t mid = (low + high) / 2;
                    const std::uint16_t id =
                        ReadU16(baseGlyphList + 4 + static_cast<std::uint32_t>(mid) * 6);
                    if (id < target)
                    {
                        low = mid + 1;
                    }
                    else
                    {
                        high = mid;
                    }
                }
                if (low >= count)
                {
                    return false;
                }
                const std::uint32_t record = baseGlyphList + 4 + static_cast<std::uint32_t>(low) * 6;
                if (ReadU16(record) != target)
                {
                    return false;
                }
                // The paint offset is relative to the BaseGlyphList itself.
                paint = baseGlyphList + ReadU32(record + 2);
                return true;
            };

            std::uint32_t root = 0;
            if (!paintForGlyph(glyph, root))
            {
                return false;
            }

            const std::uint32_t layerCount = ReadU32(layerList);

            // A `COLR` v1 paint graph nests transforms, so the walk carries the
            // affine they compose into down to every leaf: a translated or
            // scaled layer's outline lands where the graph put it, and the
            // inverse is what samples its gradient exactly.
            struct Affine
            {
                float xx = 1.0f;
                float xy = 0.0f;
                float yx = 0.0f;
                float yy = 1.0f;
                float dx = 0.0f;
                float dy = 0.0f;
            };

            const auto compose = [](const Affine& outer, const Affine& inner) -> Affine {
                Affine result;
                result.xx = outer.xx * inner.xx + outer.xy * inner.yx;
                result.xy = outer.xx * inner.xy + outer.xy * inner.yy;
                result.yx = outer.yx * inner.xx + outer.yy * inner.yx;
                result.yy = outer.yx * inner.xy + outer.yy * inner.yy;
                result.dx = outer.xx * inner.dx + outer.xy * inner.dy + outer.dx;
                result.dy = outer.yx * inner.dx + outer.yy * inner.dy + outer.dy;
                return result;
            };

            const auto fixed = [this](std::uint32_t offset) -> float {
                return static_cast<float>(ReadI16(offset)) / 16384.0f;
            };

            const auto applyAffine = [](ColorLayer& layer, const Affine& value) {
                layer.xx = value.xx;
                layer.xy = value.xy;
                layer.yx = value.yx;
                layer.yy = value.yy;
                layer.dx = value.dx;
                layer.dy = value.dy;
            };

            const auto readColorLine = [&](std::uint32_t colorLine, FontFace::ColorShade& shade) {
                const std::uint16_t stopCount = ReadU16(colorLine + 1);
                shade.stops.clear();

                for (std::uint16_t index = 0; index < stopCount; ++index)
                {
                    const std::uint32_t stop = colorLine + 3 + static_cast<std::uint32_t>(index) * 6;
                    if (static_cast<std::uint64_t>(stop) + 6 > bytes.size())
                    {
                        break;
                    }

                    FontFace::ColorShade::Stop entry;
                    entry.offset = fixed(stop);
                    entry.paletteIndex = ReadU16(stop + 2);
                    entry.alpha = fixed(stop + 4);
                    shade.stops.push_back(entry);
                }
            };

            std::function<void(std::uint32_t, std::uint16_t, int, Affine)> walk =
                [&](std::uint32_t paint, std::uint16_t current, int depth, Affine transform) {
                    if (depth > 64 || static_cast<std::uint64_t>(paint) + 1 > bytes.size())
                    {
                        return;
                    }

                    switch (bytes[paint])
                    {
                    case 1: // PaintColrLayers
                    {
                        if (static_cast<std::uint64_t>(paint) + 6 > bytes.size())
                        {
                            return;
                        }
                        const std::uint8_t count = bytes[paint + 1];
                        const std::uint32_t first = ReadU32(paint + 2);
                        for (std::uint32_t index = 0; index < count; ++index)
                        {
                            const std::uint32_t layer = first + index;
                            if (layer >= layerCount)
                            {
                                break;
                            }
                            // Layer paint offsets are relative to the LayerList.
                            walk(layerList + ReadU32(layerList + 4 + layer * 4), current, depth + 1, transform);
                        }
                        break;
                    }
                    case 2: // PaintSolid
                    case 3: // PaintVarSolid
                    {
                        if (static_cast<std::uint64_t>(paint) + 3 > bytes.size())
                        {
                            return;
                        }
                        if (current != 0)
                        {
                            ColorLayer layer;
                            layer.glyph = current;
                            layer.paletteIndex = ReadU16(paint + 1);
                            layer.shade.kind = FontFace::ColorShade::Kind::Solid;
                            applyAffine(layer, transform);
                            out.push_back(layer);
                        }
                        break;
                    }
                    case 10: // PaintGlyph
                    {
                        if (static_cast<std::uint64_t>(paint) + 6 > bytes.size())
                        {
                            return;
                        }
                        walk(paint + readU24(paint + 1), ReadU16(paint + 4), depth + 1, transform);
                        break;
                    }
                    case 11: // PaintColrGlyph
                    {
                        if (static_cast<std::uint64_t>(paint) + 3 > bytes.size())
                        {
                            return;
                        }
                        const std::uint16_t target = ReadU16(paint + 1);
                        std::uint32_t sub = 0;
                        if (paintForGlyph(target, sub))
                        {
                            walk(sub, target, depth + 1, transform);
                        }
                        break;
                    }
                    case 4: // PaintLinearGradient
                    case 5: // PaintVarLinearGradient
                    case 6: // PaintRadialGradient
                    case 7: // PaintVarRadialGradient
                    case 8: // PaintSweepGradient
                    case 9: // PaintVarSweepGradient
                    {
                        if (current == 0 || static_cast<std::uint64_t>(paint) + 4 > bytes.size())
                        {
                            return;
                        }

                        const std::uint32_t colorLine = paint + readU24(paint + 1);
                        if (static_cast<std::uint64_t>(colorLine) + 3 > bytes.size())
                        {
                            return;
                        }

                        // Every gradient carries its own geometry in font units,
                        // so its stops are sampled at the exact point they land
                        // on rather than being flattened to a flat colour.
                        const std::uint8_t paintFormat = bytes[paint];
                        FontFace::ColorShade shade;

                        if (paintFormat == 4 || paintFormat == 5)
                        {
                            if (static_cast<std::uint64_t>(paint) + 16 > bytes.size())
                            {
                                return;
                            }
                            shade.kind = FontFace::ColorShade::Kind::Linear;
                            shade.x0 = ReadI16(paint + 4);
                            shade.y0 = ReadI16(paint + 6);
                            shade.x1 = ReadI16(paint + 8);
                            shade.y1 = ReadI16(paint + 10);
                            shade.x2 = ReadI16(paint + 12);
                            shade.y2 = ReadI16(paint + 14);
                        }
                        else if (paintFormat == 6 || paintFormat == 7)
                        {
                            if (static_cast<std::uint64_t>(paint) + 16 > bytes.size())
                            {
                                return;
                            }
                            shade.kind = FontFace::ColorShade::Kind::Radial;
                            shade.x0 = ReadI16(paint + 4);
                            shade.y0 = ReadI16(paint + 6);
                            shade.r0 = ReadU16(paint + 8);
                            shade.x1 = ReadI16(paint + 10);
                            shade.y1 = ReadI16(paint + 12);
                            shade.r1 = ReadU16(paint + 14);
                        }
                        else
                        {
                            if (static_cast<std::uint64_t>(paint) + 12 > bytes.size())
                            {
                                return;
                            }
                            shade.kind = FontFace::ColorShade::Kind::Sweep;
                            shade.x0 = ReadI16(paint + 4);
                            shade.y0 = ReadI16(paint + 6);
                            // Angles are F2Dot14 in units of 180 degrees.
                            shade.x1 = fixed(paint + 8) * 180.0f;
                            shade.y1 = fixed(paint + 10) * 180.0f;
                        }

                        readColorLine(colorLine, shade);
                        if (shade.stops.empty())
                        {
                            return;
                        }

                        ColorLayer layer;
                        layer.glyph = current;
                        layer.paletteIndex = shade.stops.front().paletteIndex;
                        layer.shade = std::move(shade);
                        applyAffine(layer, transform);
                        out.push_back(layer);
                        break;
                    }
                    case 12: // PaintTransform
                    case 14: // PaintTranslate
                    case 16: // PaintScale
                    case 18: // PaintScaleAroundCenter
                    case 20: // PaintScaleUniform
                    case 22: // PaintScaleUniformAroundCenter
                    case 24: // PaintRotate
                    case 26: // PaintRotateAroundCenter
                    case 28: // PaintSkew
                    case 30: // PaintSkewAroundCenter
                    {
                        if (static_cast<std::uint64_t>(paint) + 4 > bytes.size())
                        {
                            return;
                        }

                        const std::uint8_t paintFormat = bytes[paint];
                        Affine local;

                        if (paintFormat == 12) // general affine
                        {
                            const std::uint32_t record = paint + readU24(paint + 4);
                            if (static_cast<std::uint64_t>(record) + 12 > bytes.size())
                            {
                                return;
                            }
                            local.xx = fixed(record);
                            local.yx = fixed(record + 2);
                            local.xy = fixed(record + 4);
                            local.yy = fixed(record + 6);
                            local.dx = fixed(record + 8);
                            local.dy = fixed(record + 10);
                        }
                        else if (paintFormat == 14) // translate
                        {
                            if (static_cast<std::uint64_t>(paint) + 8 > bytes.size())
                            {
                                return;
                            }
                            local.dx = static_cast<float>(ReadI16(paint + 4));
                            local.dy = static_cast<float>(ReadI16(paint + 6));
                        }
                        else if (paintFormat == 16 || paintFormat == 20) // scale
                        {
                            if (static_cast<std::uint64_t>(paint) + (paintFormat == 16 ? 8u : 6u) > bytes.size())
                            {
                                return;
                            }
                            const float scaleX = fixed(paint + 4);
                            const float scaleY = paintFormat == 16 ? fixed(paint + 6) : scaleX;
                            local.xx = scaleX;
                            local.yy = scaleY;
                        }
                        else if (paintFormat == 18 || paintFormat == 22) // scale around centre
                        {
                            const std::uint32_t size = paintFormat == 18 ? 12u : 10u;
                            if (static_cast<std::uint64_t>(paint) + size > bytes.size())
                            {
                                return;
                            }
                            const float scaleX = fixed(paint + 4);
                            const float scaleY = paintFormat == 18 ? fixed(paint + 6) : scaleX;
                            const float centerX = fixed(paint + (paintFormat == 18 ? 8u : 6u));
                            const float centerY = fixed(paint + (paintFormat == 18 ? 10u : 8u));
                            local.xx = scaleX;
                            local.yy = scaleY;
                            local.dx = centerX - scaleX * centerX;
                            local.dy = centerY - scaleY * centerY;
                        }
                        else if (paintFormat == 24 || paintFormat == 26) // rotate
                        {
                            const std::uint32_t size = paintFormat == 24 ? 6u : 10u;
                            if (static_cast<std::uint64_t>(paint) + size > bytes.size())
                            {
                                return;
                            }
                            const float angle = fixed(paint + 4) * 3.14159265358979323846f;
                            const float cosine = std::cos(angle);
                            const float sine = std::sin(angle);
                            local.xx = cosine;
                            local.xy = -sine;
                            local.yx = sine;
                            local.yy = cosine;

                            if (paintFormat == 26)
                            {
                                const float centerX = fixed(paint + 6);
                                const float centerY = fixed(paint + 8);
                                local.dx = centerX - (cosine * centerX - sine * centerY);
                                local.dy = centerY - (sine * centerX + cosine * centerY);
                            }
                        }
                        else // skew (28, 30)
                        {
                            const std::uint32_t size = paintFormat == 28 ? 8u : 12u;
                            if (static_cast<std::uint64_t>(paint) + size > bytes.size())
                            {
                                return;
                            }
                            const float xAngle = fixed(paint + 4) * 3.14159265358979323846f;
                            const float yAngle = fixed(paint + 6) * 3.14159265358979323846f;
                            const float xTangent = std::tan(xAngle);
                            const float yTangent = std::tan(yAngle);
                            local.xy = xTangent;
                            local.yx = yTangent;

                            if (paintFormat == 30)
                            {
                                const float centerX = fixed(paint + 8);
                                const float centerY = fixed(paint + 10);
                                local.dx = -xTangent * centerY;
                                local.dy = -yTangent * centerX;
                            }
                        }

                        walk(paint + readU24(paint + 1), current, depth + 1, compose(transform, local));
                        break;
                    }
                    case 32: // PaintComposite
                    {
                        const std::uint32_t source = paint + readU24(paint + 1);
                        const std::uint32_t backdrop = paint + readU24(paint + 4);
                        walk(backdrop, current, depth + 1, transform);
                        walk(source, current, depth + 1, transform);
                        break;
                    }
                    default:
                        break;
                    }
                };

            walk(root, glyph, 0, Affine{});
            return !out.empty();
        }

        if (version != 0)
        {
            return false;
        }

        const std::uint16_t baseCount = ReadU16(colr.offset + 2);
        const std::uint32_t baseRecords = colr.offset + ReadU32(colr.offset + 4);
        const std::uint32_t layerRecords = colr.offset + ReadU32(colr.offset + 8);

        // Base glyph records are sorted by glyph id.
        std::size_t low = 0;
        std::size_t high = baseCount;

        while (low < high)
        {
            const std::size_t mid = (low + high) / 2;
            const std::uint16_t id = ReadU16(baseRecords + static_cast<std::uint32_t>(mid) * 6);
            if (id < glyph)
            {
                low = mid + 1;
            }
            else
            {
                high = mid;
            }
        }

        if (low >= baseCount)
        {
            return false;
        }

        const std::uint32_t record = baseRecords + static_cast<std::uint32_t>(low) * 6;
        if (ReadU16(record) != glyph)
        {
            return false;
        }

        const std::uint16_t firstLayer = ReadU16(record + 2);
        const std::uint16_t layerCount = ReadU16(record + 4);

        for (std::uint16_t index = 0; index < layerCount; ++index)
        {
            const std::uint32_t layer = layerRecords + (firstLayer + index) * 4;
            ColorLayer entry;
            entry.glyph = ReadU16(layer);
            entry.paletteIndex = ReadU16(layer + 2);
            out.push_back(entry);
        }

        return !out.empty();
    }

    std::uint16_t SfntFont::PaletteCount() const
    {
        if (!cpal.present || static_cast<std::uint64_t>(cpal.offset) + 12 > bytes.size())
        {
            return 0;
        }
        return ReadU16(cpal.offset + 4);
    }

    bool SfntFont::PaletteColor(std::uint16_t index,
                                std::uint8_t& red,
                                std::uint8_t& green,
                                std::uint8_t& blue,
                                std::uint8_t& alpha) const
    {
        red = green = blue = alpha = 0;

        if (!cpal.present || static_cast<std::uint64_t>(cpal.offset) + 14 > bytes.size())
        {
            return false;
        }

        const std::uint16_t entries = ReadU16(cpal.offset + 2);
        const std::uint16_t palettes = ReadU16(cpal.offset + 4);
        if (palettes == 0 || index >= entries)
        {
            return false;
        }

        const std::uint32_t firstRecord = cpal.offset + ReadU32(cpal.offset + 8);
        const std::uint16_t paletteStart = ReadU16(cpal.offset + 12); // colourRecordIndices[0]
        const std::uint32_t record =
            firstRecord + static_cast<std::uint32_t>(paletteStart + index) * 4;

        if (static_cast<std::uint64_t>(record) + 4 > bytes.size())
        {
            return false;
        }

        blue = bytes[record];
        green = bytes[record + 1];
        red = bytes[record + 2];
        alpha = bytes[record + 3];
        return true;
    }

    // --- colour bitmaps (CBDT / CBLC) --------------------------------------

    bool SfntFont::BitmapGlyph(std::uint16_t glyph,
                               std::uint16_t preferPpem,
                               const std::uint8_t*& data,
                               std::uint32_t& length,
                               std::int32_t& bearingX,
                               std::int32_t& bearingY) const
    {
        data = nullptr;
        length = 0;
        bearingX = 0;
        bearingY = 0;

        // A `sbix` face stores its colour glyphs as raw image files, which the
        // caller decodes with the same codec a `CBDT` strike uses. It is tried
        // first because a face that ships `sbix` rarely ships `CBDT` too.
        if (SbixBitmap(glyph, preferPpem, data, length, bearingX, bearingY))
        {
            return true;
        }

        if (!cblc.present || !cbdt.present ||
            static_cast<std::uint64_t>(cblc.offset) + 8 > bytes.size())
        {
            return false;
        }

        const std::uint32_t sizeCount = ReadU32(cblc.offset + 4);

        // Choose the strike that covers the glyph and sits closest to the
        // requested pixel size, so an emoji is not drawn from the wrong bitmap.
        std::uint32_t chosen = sizeCount;
        int bestDelta = 0;

        for (std::uint32_t strike = 0; strike < sizeCount; ++strike)
        {
            const std::uint32_t sizeTable = cblc.offset + 8 + strike * 48;
            if (static_cast<std::uint64_t>(sizeTable) + 48 > bytes.size())
            {
                break;
            }

            const std::uint16_t startGlyph = ReadU16(sizeTable + 40);
            const std::uint16_t endGlyph = ReadU16(sizeTable + 42);
            if (glyph < startGlyph || glyph > endGlyph)
            {
                continue;
            }

            const int ppem = bytes[sizeTable + 45];
            const int delta = preferPpem == 0
                ? static_cast<int>(strike)
                : std::abs(ppem - static_cast<int>(preferPpem));

            if (chosen == sizeCount || delta < bestDelta)
            {
                chosen = strike;
                bestDelta = delta;
            }
        }

        if (chosen >= sizeCount)
        {
            return false;
        }

        {
            const std::uint32_t sizeTable = cblc.offset + 8 + chosen * 48;
            const std::uint32_t subTableArray = cblc.offset + ReadU32(sizeTable);
            const std::uint32_t subTableCount = ReadU32(sizeTable + 8);

            for (std::uint32_t entry = 0; entry < subTableCount; ++entry)
            {
                const std::uint32_t record = subTableArray + entry * 8;
                if (static_cast<std::uint64_t>(record) + 8 > bytes.size())
                {
                    break;
                }

                const std::uint16_t first = ReadU16(record);
                const std::uint16_t last = ReadU16(record + 2);
                if (glyph < first || glyph > last)
                {
                    continue;
                }

                const std::uint32_t subTable = subTableArray + ReadU32(record + 4);
                const std::uint16_t format = ReadU16(subTable);
                const std::uint32_t imageBase = cbdt.offset + ReadU32(subTable + 4);

                std::uint32_t imageOffset = 0;
                std::uint16_t metricSize = 5; // SmallGlyphMetrics

                const std::uint32_t index = glyph - first;

                if (format == 1)
                {
                    imageOffset = ReadU32(subTable + 8 + index * 4);
                }
                else if (format == 2)
                {
                    imageOffset = ReadU32(subTable + 8);
                    metricSize = 8; // BigGlyphMetrics
                }
                else if (format == 3)
                {
                    imageOffset = static_cast<std::uint32_t>(ReadU16(subTable + 8 + index * 2)) * 2u;
                }
                else if (format == 4)
                {
                    const std::uint32_t pairs = subTable + 8;
                    const std::uint16_t entryGlyph = ReadU16(pairs + index * 4);
                    if (entryGlyph != glyph)
                    {
                        break;
                    }
                    imageOffset = ReadU32(pairs + index * 4 + 2);
                }
                else if (format == 5)
                {
                    imageOffset = ReadU32(subTable + 8);
                    metricSize = 8;
                }
                else
                {
                    break;
                }

                const std::uint32_t image = imageBase + imageOffset;

                // The glyph's metrics precede the bitmap payload.
                if (static_cast<std::uint64_t>(image) + metricSize > bytes.size())
                {
                    return false;
                }

                if (metricSize == 8)
                {
                    // BigGlyphMetrics: height, width, horiBearingX, horiBearingY, ...
                    bearingX = static_cast<std::int8_t>(bytes[image + 2]);
                    bearingY = static_cast<std::int8_t>(bytes[image + 3]);
                }
                else
                {
                    // SmallGlyphMetrics: height, width, bearingX, bearingY, advance.
                    bearingX = static_cast<std::int8_t>(bytes[image + 2]);
                    bearingY = static_cast<std::int8_t>(bytes[image + 3]);
                }

                if (static_cast<std::uint64_t>(image) + metricSize < bytes.size())
                {
                    data = bytes.data() + image + metricSize;
                    // The payload length is not declared outside the strike, so
                    // it runs to the next image or the end of the table. Callers
                    // decode it as a self-describing image (PNG) and stop when
                    // the image ends.
                    length = static_cast<std::uint32_t>(bytes.size() - (image + metricSize));
                }

                return data != nullptr;
            }
        }

        return false;
    }

    // --- colour bitmaps (sbix) ---------------------------------------------

    bool SfntFont::SbixBitmap(std::uint16_t glyph,
                              std::uint16_t preferPpem,
                              const std::uint8_t*& data,
                              std::uint32_t& length,
                              std::int32_t& bearingX,
                              std::int32_t& bearingY) const
    {
        data = nullptr;
        length = 0;
        bearingX = 0;
        bearingY = 0;

        // `sbix`: version, flags, strike count, then one offset per strike. A
        // strike is `ppem`, `ppi`, and `glyphCount + 1` offsets into the strike,
        // which bracket each glyph's own image record.
        if (!sbix.present || glyphCount == 0 ||
            static_cast<std::uint64_t>(sbix.offset) + 8 > bytes.size())
        {
            return false;
        }

        const std::uint32_t strikeCount = ReadU32(sbix.offset + 4);
        if (strikeCount == 0)
        {
            return false;
        }

        // Pick the strike whose `ppem` is closest to the requested device size.
        std::uint32_t chosen = strikeCount;
        int bestDelta = 0;

        for (std::uint32_t strike = 0; strike < strikeCount; ++strike)
        {
            const std::uint32_t record = sbix.offset + 8 + strike * 4;
            if (static_cast<std::uint64_t>(record) + 4 > bytes.size())
            {
                break;
            }

            const std::uint32_t strikeOffset = sbix.offset + ReadU32(record);
            if (static_cast<std::uint64_t>(strikeOffset) + 4 > bytes.size())
            {
                continue;
            }

            const int ppem = ReadU16(strikeOffset);
            const int delta = preferPpem == 0 ? static_cast<int>(strike)
                                              : std::abs(ppem - static_cast<int>(preferPpem));

            if (chosen == strikeCount || delta < bestDelta)
            {
                chosen = strike;
                bestDelta = delta;
            }
        }

        if (chosen >= strikeCount)
        {
            return false;
        }

        const std::uint32_t strikeStart =
            sbix.offset + ReadU32(sbix.offset + 8 + chosen * 4);
        const std::uint32_t offsets = strikeStart + 4;

        if (static_cast<std::uint64_t>(offsets) + (static_cast<std::uint64_t>(glyphCount) + 1) * 4 >
            bytes.size())
        {
            return false;
        }

        const std::uint32_t glyphStart = strikeStart + ReadU32(offsets + glyph * 4);
        const std::uint32_t glyphEnd = strikeStart + ReadU32(offsets + (glyph + 1) * 4);

        // An empty range (equal offsets) is "this glyph has no bitmap", which is
        // the ordinary case for the outlines a colour font leaves blank.
        if (glyphEnd <= glyphStart || static_cast<std::uint64_t>(glyphStart) + 8 > bytes.size() ||
            glyphEnd > bytes.size())
        {
            return false;
        }

        // The record is a four-byte graphic type, an origin offset in font
        // pixels (y upwards), then the payload. `dupe` points at another glyph's
        // image, which is an optimization this reader follows only one hop.
        const std::uint8_t* type = bytes.data() + glyphStart;
        if (type[0] == 'd' && type[1] == 'u' && type[2] == 'p' && type[3] == 'e')
        {
            if (static_cast<std::uint64_t>(glyphStart) + 10 > glyphEnd)
            {
                return false;
            }
            const std::uint16_t target = ReadU16(glyphStart + 8);
            if (target == glyph)
            {
                return false;
            }
            return SbixBitmap(target, preferPpem, data, length, bearingX, bearingY);
        }

        const bool isPng = type[0] == 'p' && type[1] == 'n' && type[2] == 'g' && type[3] == ' ';
        if (!isPng)
        {
            // JPEG and TIFF payloads are legal `sbix` graphics the runtime's own
            // decoder does not read; reporting them as absent lets the caller
            // fall back to the outline rather than painting a broken image.
            return false;
        }

        bearingX = ReadI16(glyphStart + 4);
        bearingY = ReadI16(glyphStart + 6);
        data = bytes.data() + glyphStart + 8;
        length = glyphEnd - (glyphStart + 8);
        return true;
    }

    // --- colour glyphs (SVG) -----------------------------------------------

    bool SfntFont::SvgDocument(std::uint16_t glyph, std::vector<std::uint8_t>& out) const
    {
        out.clear();

        // `SVG `: version, an offset to the document list, and a reserved word.
        // The list is a count and one `start..end` record per document, each
        // naming the document's offset and length.
        if (!svg.present || static_cast<std::uint64_t>(svg.offset) + 6 > bytes.size())
        {
            return false;
        }

        const std::uint32_t list = svg.offset + ReadU32(svg.offset + 2);
        if (static_cast<std::uint64_t>(list) + 2 > bytes.size())
        {
            return false;
        }

        const std::uint16_t count = ReadU16(list);

        for (std::uint16_t index = 0; index < count; ++index)
        {
            const std::uint32_t record = list + 2 + static_cast<std::uint32_t>(index) * 12;
            if (static_cast<std::uint64_t>(record) + 12 > bytes.size())
            {
                break;
            }

            const std::uint16_t start = ReadU16(record);
            const std::uint16_t end = ReadU16(record + 2);
            if (glyph < start || glyph > end)
            {
                continue;
            }

            const std::uint32_t document = list + ReadU32(record + 4);
            const std::uint32_t length = ReadU32(record + 8);

            if (length == 0 || static_cast<std::uint64_t>(document) + length > bytes.size())
            {
                return false;
            }

            const std::uint8_t* data = bytes.data() + document;

            // A gzip stream begins 0x1f 0x8b 0x08; a plain document begins its
            // XML. Both are legal, so the bytes decide which they are.
            if (length >= 3 && data[0] == 0x1F && data[1] == 0x8B && data[2] == 0x08)
            {
                return Gunzip(data, length, out);
            }

            out.assign(data, data + length);
            return !out.empty();
        }

        return false;
    }

    // --- vertical metrics --------------------------------------------------

    float SfntFont::VerticalAdvance(std::uint16_t glyph) const
    {
        if (!vhea.present || !vmtx.present)
        {
            return ascent + descent;
        }

        const std::uint16_t longMetrics = ReadU16(vhea.offset + 34);
        if (longMetrics == 0)
        {
            return ascent + descent;
        }

        const std::uint16_t metric = glyph < longMetrics
            ? glyph
            : static_cast<std::uint16_t>(longMetrics - 1);
        return static_cast<float>(ReadU16(vmtx.offset + metric * 4));
    }

    float SfntFont::VerticalOriginY(std::uint16_t glyph) const
    {
        // `VORG` (CFF faces) is the authoritative vertical origin when present.
        if (vorg.present && static_cast<std::uint64_t>(vorg.offset) + 8 <= bytes.size())
        {
            const float defaultOrigin = static_cast<float>(ReadI16(vorg.offset + 4));
            const std::uint16_t metrics = ReadU16(vorg.offset + 6);
            const std::uint32_t base = vorg.offset + 8;

            for (std::uint16_t index = 0; index < metrics; ++index)
            {
                const std::uint32_t record = base + index * 4;
                if (static_cast<std::uint64_t>(record) + 4 > bytes.size())
                {
                    break;
                }
                if (ReadU16(record) == glyph)
                {
                    return static_cast<float>(ReadI16(record + 2));
                }
            }

            return defaultOrigin;
        }

        // A `glyf` face uses the glyph's top bound as the origin.
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        Bounds(glyph, x0, y0, x1, y1);
        if (y1 > 0.0f)
        {
            return y1;
        }

        return vhea.present ? static_cast<float>(ReadI16(vhea.offset + 4)) : ascent;
    }
}
