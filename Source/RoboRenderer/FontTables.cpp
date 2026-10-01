// ---------------------------------------------------------------------------
// RoboRenderer - the font tables that are not geometry or metrics.
//
// `FontFile.cpp` reads the tables a face needs to lay out and fill letters.
// This file reads the ones a face uses to decorate them: OpenType Layout (the
// `GSUB`/`GPOS`/`GDEF` that shape a run), colour glyphs (`COLR`/`CPAL` layers,
// the `CBDT`/`CBLC` bitmaps emoji fonts ship), and the vertical metrics that
// turn a horizontal run on its side.
//
// Both readers are deliberately shallow: a colour face that only ships COLR v1
// reports no layers rather than guessing at a paint graph, and a face whose
// bitmap is at a size the caller did not ask for still reports the glyph it
// has. Nothing here allocates until it has something to return.
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"

#include <algorithm>
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

            std::function<void(std::uint32_t, std::uint16_t, int)> walk =
                [&](std::uint32_t paint, std::uint16_t current, int depth) {
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
                            walk(layerList + ReadU32(layerList + 4 + layer * 4), current, depth + 1);
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
                        walk(paint + readU24(paint + 1), ReadU16(paint + 4), depth + 1);
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
                            walk(sub, target, depth + 1);
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
                        // The rasterizer paints flat layers, so a gradient is
                        // approximated by its first colour stop. The glyph mask
                        // is exact; only the shading is flattened.
                        if (static_cast<std::uint64_t>(paint) + 4 > bytes.size())
                        {
                            return;
                        }
                        const std::uint32_t colorLine = paint + readU24(paint + 1);
                        if (static_cast<std::uint64_t>(colorLine) + 9 > bytes.size())
                        {
                            return;
                        }
                        const std::uint16_t stopCount = ReadU16(colorLine + 1);
                        if (stopCount == 0 || current == 0)
                        {
                            return;
                        }
                        ColorLayer layer;
                        layer.glyph = current;
                        layer.paletteIndex = ReadU16(colorLine + 5); // first stop
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
                        // The geometry changes the placement of the sub-paint, not
                        // the layer structure, so the sub-paint is walked as-is.
                        walk(paint + readU24(paint + 1), current, depth + 1);
                        break;
                    }
                    case 32: // PaintComposite
                    {
                        const std::uint32_t source = paint + readU24(paint + 1);
                        const std::uint32_t backdrop = paint + readU24(paint + 4);
                        walk(backdrop, current, depth + 1);
                        walk(source, current, depth + 1);
                        break;
                    }
                    default:
                        break;
                    }
                };

            walk(root, glyph, 0);
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
