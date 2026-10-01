// ---------------------------------------------------------------------------
// RoboRenderer - variable fonts.
//
// See FontFile.h. A variable font is one outline plus a set of deltas that say
// how to move its points and advances at any point in an axis space. This file
// reads that space (`fvar`, `avar`) and applies it (`gvar`, `HVAR`).
//
// Everything here is off the hot path: `SetInstance` resolves the deltas once,
// and a draw with no instance set never touches a single one of these tables.
// ---------------------------------------------------------------------------
#include "RoboRenderer/FontFile.h"

#include <algorithm>
#include <cmath>

namespace RoboRenderer
{
    namespace
    {
        inline float F2Dot14Value(std::int16_t value)
        {
            return static_cast<float>(value) / 16384.0f;
        }
    }

    // --- names -------------------------------------------------------------

    std::string SfntFont::NameString(std::uint16_t nameId) const
    {
        if (!name.present)
        {
            return {};
        }

        const std::uint16_t count = ReadU16(name.offset + 2);
        const std::uint32_t stringOffset = name.offset + ReadU16(name.offset + 4);

        std::string best;

        for (std::uint16_t index = 0; index < count; ++index)
        {
            const std::uint32_t record = name.offset + 6 + static_cast<std::uint32_t>(index) * 12;
            const std::uint16_t platform = ReadU16(record);
            const std::uint16_t encoding = ReadU16(record + 2);
            const std::uint16_t language = ReadU16(record + 4);
            const std::uint16_t recordNameId = ReadU16(record + 6);
            const std::uint16_t length = ReadU16(record + 8);
            const std::uint16_t offset = ReadU16(record + 10);

            if (recordNameId != nameId)
            {
                continue;
            }

            const std::uint32_t start = stringOffset + offset;
            if (static_cast<std::size_t>(start) + length > bytes.size())
            {
                continue;
            }

            const bool windows = platform == 3 && (encoding == 1 || encoding == 10);
            const bool unicode = platform == 0;
            if (!windows && !unicode && platform != 1)
            {
                continue;
            }

            std::string value;
            if (platform == 1)
            {
                value.assign(reinterpret_cast<const char*>(bytes.data() + start), length);
            }
            else
            {
                // UTF-16BE to UTF-8.
                for (std::uint32_t i = 0; i + 1 < length; i += 2)
                {
                    std::uint32_t code = (static_cast<std::uint32_t>(bytes[start + i]) << 8) |
                                         bytes[start + i + 1];
                    if (code >= 0xD800u && code <= 0xDBFFu && i + 3 < length)
                    {
                        const std::uint32_t low =
                            (static_cast<std::uint32_t>(bytes[start + i + 2]) << 8) |
                            bytes[start + i + 3];
                        if (low >= 0xDC00u && low <= 0xDFFFu)
                        {
                            code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                            i += 2;
                        }
                    }
                    if (code < 0x80u)
                    {
                        value.push_back(static_cast<char>(code));
                    }
                    else if (code < 0x800u)
                    {
                        value.push_back(static_cast<char>(0xC0u | (code >> 6)));
                        value.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
                    }
                    else if (code < 0x10000u)
                    {
                        value.push_back(static_cast<char>(0xE0u | (code >> 12)));
                        value.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
                        value.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
                    }
                    else
                    {
                        value.push_back(static_cast<char>(0xF0u | (code >> 18)));
                        value.push_back(static_cast<char>(0x80u | ((code >> 12) & 0x3Fu)));
                        value.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
                        value.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
                    }
                }
            }

            if (value.empty())
            {
                continue;
            }

            if (windows && (language == 0x0409u || best.empty()))
            {
                best = value;
                if (language == 0x0409u)
                {
                    return best;
                }
            }
            else if (best.empty())
            {
                best = value;
            }
        }

        return best;
    }

    const std::string& SfntFont::InstanceName(std::uint16_t nameId) const
    {
        // Instance names are few; the common ones are resolved on demand and
        // cached so the `name` table is walked once per instance.
        static thread_local std::string scratch;
        scratch = NameString(nameId);
        return scratch;
    }

    // --- fvar / avar -------------------------------------------------------

    bool SfntFont::ReadVariations()
    {
        axes.clear();
        instances.clear();
        advanceDeltas.clear();

        if (fvar.present)
        {
            const std::uint32_t axesOffset = fvar.offset + ReadU16(fvar.offset + 4);
            const std::uint16_t axisCount = ReadU16(fvar.offset + 8);
            const std::uint16_t axisSize = ReadU16(fvar.offset + 10);
            const std::uint16_t instanceCount = ReadU16(fvar.offset + 12);
            const std::uint16_t instanceSize = ReadU16(fvar.offset + 14);

            for (std::uint16_t index = 0; index < axisCount; ++index)
            {
                const std::uint32_t record = axesOffset + static_cast<std::uint32_t>(index) * axisSize;
                VariationAxis axis;
                axis.tag = ReadU32(record);
                axis.minimum = static_cast<float>(ReadI16(record + 4)) / 65536.0f;
                axis.defaultValue = static_cast<float>(ReadI16(record + 8)) / 65536.0f;
                axis.maximum = static_cast<float>(ReadI16(record + 12)) / 65536.0f;
                axis.hidden = (ReadU16(record + 16) & 0x0001u) != 0;
                axes.push_back(axis);
            }

            const std::uint32_t instancesOffset =
                axesOffset + static_cast<std::uint32_t>(axisCount) * axisSize;

            for (std::uint16_t index = 0; index < instanceCount; ++index)
            {
                const std::uint32_t record =
                    instancesOffset + static_cast<std::uint32_t>(index) * instanceSize;
                NamedInstance instance;
                instance.nameId = ReadU16(record);
                for (std::uint16_t axis = 0; axis < axisCount; ++axis)
                {
                    instance.coordinates.push_back(
                        static_cast<float>(ReadI16(record + 4 + axis * 4)) / 65536.0f);
                }
                instances.push_back(std::move(instance));
            }
        }

        for (const VariationAxis& axis : axes)
        {
            instanceCoordinates.push_back(axis.defaultValue);
        }

        // `gvar` is meaningless without axes, but its header can still be read.
        if (gvar.present && !axes.empty())
        {
            variation.sharedTupleCount = ReadU16(gvar.offset + 6);
            variation.sharedTuplesOffset = gvar.offset + ReadU32(gvar.offset + 8);
            variation.glyphCount = ReadU16(gvar.offset + 12);
            variation.flags = ReadU16(gvar.offset + 14);
            variation.dataOffset = gvar.offset + ReadU32(gvar.offset + 16);

            const bool longOffsets = (variation.flags & 0x0001u) != 0;
            const std::uint32_t offsetBase = gvar.offset + 20;

            variation.offsets.assign(static_cast<std::size_t>(variation.glyphCount) + 1, 0);
            for (std::uint32_t index = 0; index <= variation.glyphCount; ++index)
            {
                variation.offsets[index] = longOffsets
                    ? ReadU32(offsetBase + index * 4)
                    : static_cast<std::uint32_t>(ReadU16(offsetBase + index * 2)) * 2u;
            }
        }

        return true;
    }

    void SfntFont::SetInstance(const std::vector<float>& coordinates)
    {
        instanceCoordinates.assign(axes.size(), 0.0f);
        instanceSet = false;

        // MVAR deltas are per-instance and are applied to the global metrics.
        advanceDeltas.clear();
        instanceSet = true;

        for (std::size_t index = 0; index < axes.size(); ++index)
        {
            float value = index < coordinates.size() ? coordinates[index] : axes[index].defaultValue;
            value = std::max(axes[index].minimum, std::min(axes[index].maximum, value));
            instanceCoordinates[index] = value;
        }

        // `avar` remaps the normalized coordinates. Read on demand: a face with
        // no `avar` keeps the linear mapping.
        std::vector<float> normalized(axes.size(), 0.0f);
        for (std::size_t index = 0; index < axes.size(); ++index)
        {
            const VariationAxis& axis = axes[index];
            const float value = instanceCoordinates[index];
            if (value < axis.defaultValue && axis.defaultValue > axis.minimum)
            {
                normalized[index] = -(axis.defaultValue - value) / (axis.defaultValue - axis.minimum);
            }
            else if (value > axis.defaultValue && axis.maximum > axis.defaultValue)
            {
                normalized[index] = (value - axis.defaultValue) / (axis.maximum - axis.defaultValue);
            }
        }

        if (avar.present)
        {
            const std::uint16_t axisCount = ReadU16(avar.offset + 6);

            // Each axis maps to an Offset16 segment list, in axis order. Walk
            // them once into an array so the remap below is a plain lookup.
            std::uint32_t cursor = avar.offset + 8;
            std::vector<std::uint32_t> lists;
            for (std::uint16_t axis = 0; axis < axisCount; ++axis)
            {
                const std::uint32_t listOffset = ReadU16(cursor);
                lists.push_back(listOffset == 0 ? 0 : avar.offset + listOffset);
                cursor += 2;
            }

            for (std::uint16_t axis = 0; axis < axisCount && axis < axes.size(); ++axis)
            {
                const std::uint32_t list = lists[axis];
                if (list == 0)
                {
                    continue;
                }
                const std::uint16_t pairCount = ReadU16(list);
                const float x = normalized[axis];
                float mapped = x;

                for (std::uint16_t pair = 0; pair < pairCount; ++pair)
                {
                    const float from = F2Dot14Value(ReadI16(list + 2 + pair * 4));
                    const float to = F2Dot14Value(ReadI16(list + 2 + pair * 4 + 2));
                    if (pair + 1 < pairCount)
                    {
                        const float nextFrom = F2Dot14Value(ReadI16(list + 2 + (pair + 1) * 4));
                        if (x >= from && x <= nextFrom && nextFrom > from)
                        {
                            mapped = to + (x - from) / (nextFrom - from);
                            break;
                        }
                    }
                    if (x >= from)
                    {
                        mapped = to;
                    }
                }

                normalized[axis] = mapped;
            }
        }

        instanceCoordinates = normalized;

        // Precompute HVAR advance deltas at this instance.
        if (hvar.present && !axes.empty())
        {
            const std::uint32_t itemVariationStore = hvar.offset + ReadU32(hvar.offset + 4);
            const std::size_t glyphs = glyphCount;

            advanceDeltas.assign(glyphs, 0.0f);

            const std::uint32_t regionList = itemVariationStore + ReadU32(itemVariationStore + 2);
            const std::uint16_t regionCount = ReadU16(regionList + 2);
            const std::uint32_t regionBase = regionList + 4;

            const std::uint16_t dataCount = ReadU16(itemVariationStore + 6);
            if (dataCount > 0)
            {
                const std::uint32_t dataTable = itemVariationStore + ReadU32(itemVariationStore + 8);
                const std::uint16_t itemCount = ReadU16(dataTable);
                const std::uint16_t wordDeltaCount = ReadU16(dataTable + 2);
                const std::uint16_t regionIndexCount = ReadU16(dataTable + 4);
                const std::uint32_t regionIndexes = dataTable + 6;

                const bool longWords = (wordDeltaCount & 0x8000u) != 0;
                const std::uint16_t wordCount = wordDeltaCount & 0x7FFFu;

                // Scalar per region.
                std::vector<float> scalars(regionIndexCount, 0.0f);
                for (std::uint16_t region = 0; region < regionIndexCount; ++region)
                {
                    const std::uint16_t axisIndex = ReadU16(regionIndexes + region * 2);
                    if (axisIndex >= regionCount)
                    {
                        continue;
                    }
                    const std::uint32_t regionRecord =
                        regionBase + static_cast<std::uint32_t>(axisIndex) * axes.size() * 6;

                    float scalar = 1.0f;
                    for (std::size_t axis = 0; axis < axes.size(); ++axis)
                    {
                        const float start = F2Dot14Value(ReadI16(regionRecord + axis * 6));
                        const float peak = F2Dot14Value(ReadI16(regionRecord + axis * 6 + 2));
                        const float end = F2Dot14Value(ReadI16(regionRecord + axis * 6 + 4));
                        const float coordinate = instanceCoordinates[axis];

                        if (peak == 0.0f || coordinate == 0.0f)
                        {
                            continue;
                        }
                        if (coordinate < start || coordinate > end)
                        {
                            scalar = 0.0f;
                            break;
                        }
                        if (coordinate == peak)
                        {
                            continue;
                        }
                        if (coordinate < peak)
                        {
                            scalar *= start == peak ? 0.0f : (coordinate - start) / (peak - start);
                        }
                        else
                        {
                            scalar *= end == peak ? 0.0f : (end - coordinate) / (end - peak);
                        }
                    }
                    scalars[region] = scalar;
                }

                // The deltas follow the region indexes. The first `wordCount`
                // of them are 16-bit (or 32-bit when the LONG_WORDS bit is set);
                // the rest are signed bytes.
                const std::uint32_t wordSize = longWords ? 4u : 2u;
                const std::uint16_t wordRegions =
                    std::min<std::uint16_t>(wordCount, regionIndexCount);
                const std::uint32_t rowSize =
                    static_cast<std::uint32_t>(wordRegions) * wordSize +
                    (regionIndexCount - wordRegions);

                const std::uint32_t deltaCursor = dataTable + 6 + regionIndexCount * 2;

                for (std::uint16_t item = 0; item < itemCount && item < glyphs; ++item)
                {
                    float delta = 0.0f;
                    std::uint32_t row = deltaCursor + static_cast<std::uint32_t>(item) * rowSize;

                    for (std::uint16_t region = 0; region < regionIndexCount; ++region)
                    {
                        float value = 0.0f;
                        if (region < wordRegions)
                        {
                            value = longWords
                                ? static_cast<float>(static_cast<std::int32_t>(ReadU32(row)))
                                : static_cast<float>(ReadI16(row));
                            row += wordSize;
                        }
                        else
                        {
                            value = static_cast<float>(static_cast<std::int8_t>(
                                bytes.empty() || row >= bytes.size() ? 0 : bytes[row]));
                            row += 1;
                        }
                        delta += value * scalars[region];
                    }

                    advanceDeltas[item] = delta;
                }
            }
        }

        return;
    }

    float SfntFont::AdvanceDelta(std::uint16_t glyph) const
    {
        if (!instanceSet || advanceDeltas.empty() || glyph >= advanceDeltas.size())
        {
            return 0.0f;
        }
        return advanceDeltas[glyph];
    }

    // --- gvar interpolation ------------------------------------------------

    namespace
    {
        /// A packed point-number run, as `gvar` serializes it.
        std::vector<std::uint32_t> ReadPackedPoints(const std::uint8_t* data,
                                                    std::uint32_t size,
                                                    std::uint32_t& cursor,
                                                    std::uint32_t pointCount)
        {
            std::vector<std::uint32_t> points;
            if (cursor >= size)
            {
                return points;
            }

            std::uint32_t count = data[cursor++];
            if ((count & 0x80u) != 0)
            {
                if (cursor >= size)
                {
                    return points;
                }
                count = ((count & 0x7Fu) << 8) | data[cursor++];
            }

            std::uint32_t value = 0;
            for (std::uint32_t index = 0; index < count && cursor < size; ++index)
            {
                std::uint32_t delta = data[cursor++];
                if ((delta & 0x80u) != 0)
                {
                    if (cursor >= size)
                    {
                        break;
                    }
                    delta = ((delta & 0x7Fu) << 8) | data[cursor++];
                }
                value += delta;
                if (value < pointCount + 4)
                {
                    points.push_back(value);
                }
            }

            return points;
        }

        /// A packed signed delta run.
        std::vector<float> ReadPackedDeltas(const std::uint8_t* data,
                                            std::uint32_t size,
                                            std::uint32_t& cursor,
                                            std::uint32_t count)
        {
            std::vector<float> deltas;
            deltas.reserve(count);

            while (deltas.size() < count && cursor < size)
            {
                std::uint32_t control = data[cursor++];
                if ((control & 0x80u) != 0)
                {
                    const std::uint32_t run = (control & 0x7Fu) + 1;
                    for (std::uint32_t i = 0; i < run && deltas.size() < count; ++i)
                    {
                        deltas.push_back(0.0f);
                    }
                }
                else
                {
                    const std::uint32_t run = control + 1;
                    for (std::uint32_t i = 0; i < run && deltas.size() < count; ++i)
                    {
                        if (cursor >= size)
                        {
                            break;
                        }
                        std::uint32_t value = data[cursor++];
                        if ((value & 0x40u) != 0)
                        {
                            value = static_cast<std::uint32_t>(value | 0xFFFFFF80u);
                        }
                        // Sign extension into an int8 first.
                        const std::int8_t signedValue = static_cast<std::int8_t>(value & 0xFFu);
                        deltas.push_back(static_cast<float>(signedValue));
                    }
                }
            }

            while (deltas.size() < count)
            {
                deltas.push_back(0.0f);
            }

            return deltas;
        }
    }

    bool SfntFont::ApplyVariations(std::uint16_t glyph, GlyphPath& path) const
    {
        if (!instanceSet || !gvar.present || axes.empty() || variation.glyphCount == 0 ||
            glyph >= variation.glyphCount || variation.offsets.size() <= glyph + 1)
        {
            return true;
        }

        // Only simple glyphs carry gvar deltas here: a composite is assembled
        // from components that each carry their own.
        if (locaOffsets.size() > glyph + 1 && locaOffsets[glyph] < locaOffsets[glyph + 1])
        {
            const std::uint32_t base = glyf.offset + locaOffsets[glyph];
            if (static_cast<std::size_t>(base) + 2 <= bytes.size() && ReadI16(base) < 0)
            {
                return true;
            }
        }

        const std::uint32_t dataStart = variation.dataOffset + variation.offsets[glyph];
        const std::uint32_t dataEnd = variation.dataOffset + variation.offsets[glyph + 1];
        if (dataStart >= dataEnd || dataEnd > bytes.size())
        {
            return true;
        }

        std::uint32_t cursor = dataStart;
        const std::uint16_t tupleCountRaw = ReadU16(cursor);
        cursor += 2;
        const std::uint16_t tupleCount = tupleCountRaw & 0x0FFFu;
        const bool sharedPoints = (tupleCountRaw & 0x8000u) != 0;
        const std::uint32_t serializedStart = dataStart + ReadU16(cursor);
        cursor += 2;

        const std::uint32_t pointCount = static_cast<std::uint32_t>(path.points.size());

        std::vector<std::uint32_t> sharedPointNumbers;
        if (sharedPoints)
        {
            std::uint32_t pointCursor = serializedStart;
            sharedPointNumbers = ReadPackedPoints(bytes.data(), bytes.size(), pointCursor, pointCount);
        }

        std::uint32_t cursor2 = cursor;
        std::uint32_t dataCursor = serializedStart;

        for (std::uint16_t tuple = 0; tuple < tupleCount; ++tuple)
        {
            const std::uint16_t variationDataSize = ReadU16(cursor2);
            const std::uint16_t tupleIndex = ReadU16(cursor2 + 2);
            cursor2 += 4;

            // Peak tuple.
            std::vector<float> peak(axes.size(), 0.0f);
            const bool embeddedPeak = (tupleIndex & 0x8000u) != 0;
            if (embeddedPeak)
            {
                for (std::size_t axis = 0; axis < axes.size(); ++axis)
                {
                    peak[axis] = F2Dot14Value(ReadI16(cursor2 + static_cast<std::uint32_t>(axis) * 2));
                }
                cursor2 += static_cast<std::uint32_t>(axes.size()) * 2;
            }
            else
            {
                const std::uint16_t sharedIndex = tupleIndex & 0x0FFFu;
                if (sharedIndex < variation.sharedTupleCount)
                {
                    const std::uint32_t sharedBase =
                        variation.sharedTuplesOffset + static_cast<std::uint32_t>(sharedIndex) * axes.size() * 2;
                    for (std::size_t axis = 0; axis < axes.size(); ++axis)
                    {
                        peak[axis] = F2Dot14Value(ReadI16(sharedBase + static_cast<std::uint32_t>(axis) * 2));
                    }
                }
            }

            // Intermediate region.
            std::vector<float> start(axes.size(), 0.0f);
            std::vector<float> end(axes.size(), 0.0f);
            for (std::size_t axis = 0; axis < axes.size(); ++axis)
            {
                start[axis] = peak[axis] < 0.0f ? peak[axis] : 0.0f;
                end[axis] = peak[axis] < 0.0f ? 0.0f : peak[axis];
            }

            if ((tupleIndex & 0x4000u) != 0)
            {
                for (std::size_t axis = 0; axis < axes.size(); ++axis)
                {
                    start[axis] = F2Dot14Value(ReadI16(cursor2 + static_cast<std::uint32_t>(axis) * 6));
                    peak[axis] = F2Dot14Value(ReadI16(cursor2 + static_cast<std::uint32_t>(axis) * 6 + 2));
                    end[axis] = F2Dot14Value(ReadI16(cursor2 + static_cast<std::uint32_t>(axis) * 6 + 4));
                }
                cursor2 += static_cast<std::uint32_t>(axes.size()) * 6;
            }

            // Scalar for this tuple at the current instance.
            float scalar = 1.0f;
            for (std::size_t axis = 0; axis < axes.size(); ++axis)
            {
                const float coordinate = instanceCoordinates[axis];
                if (peak[axis] == 0.0f || coordinate == 0.0f)
                {
                    continue;
                }
                if (coordinate < start[axis] || coordinate > end[axis])
                {
                    scalar = 0.0f;
                    break;
                }
                if (coordinate == peak[axis])
                {
                    continue;
                }
                if (coordinate < peak[axis])
                {
                    scalar *= peak[axis] == start[axis] ? 0.0f
                        : (coordinate - start[axis]) / (peak[axis] - start[axis]);
                }
                else
                {
                    scalar *= end[axis] == peak[axis] ? 0.0f
                        : (end[axis] - coordinate) / (end[axis] - peak[axis]);
                }
            }

            const bool privatePoints = (tupleIndex & 0x2000u) != 0;
            const bool hasPoints = sharedPoints || privatePoints;

            std::vector<std::uint32_t> points;
            if (hasPoints)
            {
                std::uint32_t pointCursor = dataCursor;
                points = privatePoints
                    ? ReadPackedPoints(bytes.data(), bytes.size(), pointCursor, pointCount)
                    : sharedPointNumbers;
                dataCursor = pointCursor;
            }

            const std::uint32_t deltaCount = hasPoints
                ? static_cast<std::uint32_t>(points.size())
                : pointCount + 4;

            std::vector<float> deltasX = ReadPackedDeltas(bytes.data(), bytes.size(), dataCursor, deltaCount);
            std::vector<float> deltasY = ReadPackedDeltas(bytes.data(), bytes.size(), dataCursor, deltaCount);

            if (scalar != 0.0f)
            {
                for (std::uint32_t i = 0; i < deltaCount; ++i)
                {
                    const std::uint32_t pointIndex = hasPoints ? points[i] : i;
                    if (pointIndex >= pointCount)
                    {
                        continue;
                    }
                    path.points[pointIndex].x += deltasX[i] * scalar;
                    path.points[pointIndex].y += deltasY[i] * scalar;
                }
            }

            (void)variationDataSize;
        }

        return true;
    }
}
