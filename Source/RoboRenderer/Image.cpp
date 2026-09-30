#include "RoboRenderer/Image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace RoboRenderer
{
    namespace
    {
        constexpr std::uint32_t WindowSize = 32768;
        constexpr std::uint32_t MinMatch = 3;
        constexpr std::uint32_t MaxMatch = 258;

        std::uint32_t Crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc)
        {
            static std::uint32_t table[256];
            static bool ready = false;

            if (!ready)
            {
                for (std::uint32_t index = 0; index < 256; ++index)
                {
                    std::uint32_t value = index;
                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value & 1u) != 0u ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
                    }
                    table[index] = value;
                }
                ready = true;
            }

            std::uint32_t result = crc ^ 0xFFFFFFFFu;
            for (std::size_t index = 0; index < size; ++index)
            {
                result = table[(result ^ data[index]) & 0xFFu] ^ (result >> 8);
            }

            return result ^ 0xFFFFFFFFu;
        }

        std::uint32_t Adler32(const std::uint8_t* data, std::size_t size)
        {
            std::uint32_t low = 1;
            std::uint32_t high = 0;

            for (std::size_t index = 0; index < size; ++index)
            {
                low = (low + data[index]) % 65521u;
                high = (high + low) % 65521u;
            }

            return (high << 16) | low;
        }

        // -------------------------------------------------------------------
        // Bit level I/O for DEFLATE
        // -------------------------------------------------------------------

        class BitReader
        {
        public:
            BitReader(const std::uint8_t* bytes, std::size_t byteCount)
                : data(bytes)
                , size(byteCount)
            {
            }

            /// Fast bit read on the hot path; returns -1 at the end of the stream.
            int ReadBit()
            {
                if (bitCount == 0 && !Refill())
                {
                    return -1;
                }

                const int bit = static_cast<int>(bitBuffer & 1u);
                bitBuffer >>= 1;
                --bitCount;
                return bit;
            }

            bool ReadBits(int count, std::uint32_t& value)
            {
                value = 0;

                for (int index = 0; index < count; ++index)
                {
                    const int bit = ReadBit();
                    if (bit < 0)
                    {
                        return false;
                    }

                    value |= static_cast<std::uint32_t>(bit) << index;
                }

                return true;
            }

            /// Discards bits up to the next byte boundary.
            void Align()
            {
                bitBuffer = 0;
                bitCount = 0;
            }

            bool ReadBytes(std::uint8_t* out, std::size_t count)
            {
                if (position + count > size)
                {
                    return false;
                }

                std::memcpy(out, data + position, count);
                position += count;
                return true;
            }

            std::size_t Position() const { return position; }

        private:
            bool Refill()
            {
                if (bitCount > 24 || position >= size)
                {
                    return bitCount > 0;
                }

                bitBuffer |= static_cast<std::uint32_t>(data[position++]) << bitCount;
                bitCount += 8;
                return true;
            }

            const std::uint8_t* data = nullptr;
            std::size_t size = 0;
            std::size_t position = 0;
            std::uint32_t bitBuffer = 0;
            int bitCount = 0;
        };

        class BitWriter
        {
        public:
            void WriteBits(std::uint32_t value, int count)
            {
                for (int index = 0; index < count; ++index)
                {
                    AppendBit((value >> index) & 1u);
                }
            }

            /// Writes a Huffman code most significant bit first, which is what
            /// the DEFLATE specification requires of canonical codes.
            void WriteCode(std::uint32_t code, int length)
            {
                for (int bit = length - 1; bit >= 0; --bit)
                {
                    AppendBit((code >> bit) & 1u);
                }
            }

            void WriteBytes(const std::uint8_t* bytes, std::size_t count)
            {
                for (std::size_t index = 0; index < count; ++index)
                {
                    WriteBits(bytes[index], 8);
                }
            }

            void Finish()
            {
                if (bitCount > 0)
                {
                    bytes.push_back(static_cast<std::uint8_t>(bitBuffer & 0xFFu));
                    bitBuffer = 0;
                    bitCount = 0;
                }
            }

            const std::vector<std::uint8_t>& Bytes() const { return bytes; }

        private:
            void AppendBit(std::uint32_t bit)
            {
                bitBuffer |= (bit & 1u) << bitCount;
                ++bitCount;

                if (bitCount == 8)
                {
                    bytes.push_back(static_cast<std::uint8_t>(bitBuffer & 0xFFu));
                    bitBuffer = 0;
                    bitCount = 0;
                }
            }

            std::vector<std::uint8_t> bytes;
            std::uint32_t bitBuffer = 0;
            int bitCount = 0;
        };

        // -------------------------------------------------------------------
        // Canonical Huffman tables
        // -------------------------------------------------------------------

        struct HuffmanTable
        {
            std::uint16_t counts[16] = {};
            std::vector<std::uint16_t> symbols;

            bool Build(const std::uint8_t* lengths, int count)
            {
                for (int length = 0; length < 16; ++length)
                {
                    counts[length] = 0;
                }

                for (int index = 0; index < count; ++index)
                {
                    ++counts[lengths[index]];
                }

                if (counts[0] == count)
                {
                    return true;
                }

                // A code may be incomplete only when it has a single symbol; a
                // table that over-subscribes its length budget is malformed.
                int left = 1;
                for (int length = 1; length < 16; ++length)
                {
                    left <<= 1;
                    left -= counts[length];
                    if (left < 0)
                    {
                        return false;
                    }
                }

                std::uint16_t offsets[16] = {};
                for (int length = 1; length < 16; ++length)
                {
                    offsets[length] = static_cast<std::uint16_t>(offsets[length - 1] + counts[length - 1]);
                }

                symbols.assign(static_cast<std::size_t>(count), 0);

                for (int index = 0; index < count; ++index)
                {
                    if (lengths[index] != 0)
                    {
                        symbols[offsets[lengths[index]]++] = static_cast<std::uint16_t>(index);
                    }
                }

                return true;
            }

            int Decode(BitReader& reader) const
            {
                int code = 0;
                int first = 0;
                int index = 0;

                for (int length = 1; length < 16; ++length)
                {
                    const int bit = reader.ReadBit();
                    if (bit < 0)
                    {
                        return -1;
                    }

                    code |= bit;

                    const int count = counts[length];
                    if (code - first < count)
                    {
                        return symbols[static_cast<std::size_t>(index + (code - first))];
                    }

                    index += count;
                    first = (first + count) << 1;
                    code <<= 1;
                }

                return -1;
            }
        };

        void BuildFixedTables(HuffmanTable& literals, HuffmanTable& distances)
        {
            std::uint8_t literalLengths[288];
            for (int index = 0; index < 144; ++index)
            {
                literalLengths[index] = 8;
            }
            for (int index = 144; index < 256; ++index)
            {
                literalLengths[index] = 9;
            }
            for (int index = 256; index < 280; ++index)
            {
                literalLengths[index] = 7;
            }
            for (int index = 280; index < 288; ++index)
            {
                literalLengths[index] = 8;
            }

            literals.Build(literalLengths, 288);

            std::uint8_t distanceLengths[32];
            for (int index = 0; index < 32; ++index)
            {
                distanceLengths[index] = 5;
            }

            distances.Build(distanceLengths, 32);
        }

        const std::uint16_t LengthBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                                               31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
        const std::uint8_t LengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
        const std::uint16_t DistanceBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                                 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193,
                                                 12289, 16385, 24577 };
        const std::uint8_t DistanceExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                                 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

        bool InflateRaw(BitReader& reader, std::vector<std::uint8_t>& out, std::string& error)
        {
            for (;;)
            {
                std::uint32_t final = 0;
                std::uint32_t type = 0;

                if (!reader.ReadBits(1, final) || !reader.ReadBits(2, type))
                {
                    error = "deflate stream ended inside a block header";
                    return false;
                }

                if (type == 0)
                {
                    reader.Align();

                    std::uint8_t header[4];
                    if (!reader.ReadBytes(header, 4))
                    {
                        error = "deflate stored block header is truncated";
                        return false;
                    }

                    const std::uint32_t length =
                        static_cast<std::uint32_t>(header[0]) | (static_cast<std::uint32_t>(header[1]) << 8);
                    const std::uint32_t complement =
                        static_cast<std::uint32_t>(header[2]) | (static_cast<std::uint32_t>(header[3]) << 8);

                    if ((length ^ 0xFFFFu) != complement)
                    {
                        error = "deflate stored block length does not match its complement";
                        return false;
                    }

                    const std::size_t start = out.size();
                    out.resize(start + length);
                    if (!reader.ReadBytes(out.data() + start, length))
                    {
                        error = "deflate stored block is truncated";
                        return false;
                    }
                }
                else if (type == 1 || type == 2)
                {
                    HuffmanTable literals;
                    HuffmanTable distances;

                    if (type == 1)
                    {
                        BuildFixedTables(literals, distances);
                    }
                    else
                    {
                        std::uint32_t literalCount = 0;
                        std::uint32_t distanceCount = 0;
                        std::uint32_t codeLengthCount = 0;

                        if (!reader.ReadBits(5, literalCount) || !reader.ReadBits(5, distanceCount) ||
                            !reader.ReadBits(4, codeLengthCount))
                        {
                            error = "deflate dynamic block header is truncated";
                            return false;
                        }

                        literalCount += 257;
                        distanceCount += 1;
                        codeLengthCount += 4;

                        if (literalCount > 286 || distanceCount > 30)
                        {
                            error = "deflate dynamic block declares too many codes";
                            return false;
                        }

                        static const int Order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1,
                                                      15 };
                        std::uint8_t codeLengthLengths[19] = {};

                        for (std::uint32_t index = 0; index < codeLengthCount; ++index)
                        {
                            std::uint32_t value = 0;
                            if (!reader.ReadBits(3, value))
                            {
                                error = "deflate code length table is truncated";
                                return false;
                            }
                            codeLengthLengths[Order[index]] = static_cast<std::uint8_t>(value);
                        }

                        HuffmanTable codeLengthTable;
                        if (!codeLengthTable.Build(codeLengthLengths, 19))
                        {
                            error = "deflate code length table is malformed";
                            return false;
                        }

                        std::vector<std::uint8_t> lengths;
                        lengths.reserve(static_cast<std::size_t>(literalCount + distanceCount));

                        while (lengths.size() < static_cast<std::size_t>(literalCount + distanceCount))
                        {
                            const int symbol = codeLengthTable.Decode(reader);
                            if (symbol < 0)
                            {
                                error = "deflate code length symbols are truncated";
                                return false;
                            }

                            if (symbol < 16)
                            {
                                lengths.push_back(static_cast<std::uint8_t>(symbol));
                                continue;
                            }

                            std::uint32_t repeat = 0;
                            std::uint8_t value = 0;

                            if (symbol == 16)
                            {
                                if (lengths.empty())
                                {
                                    error = "deflate repeat has no previous code length";
                                    return false;
                                }

                                value = lengths.back();
                                if (!reader.ReadBits(2, repeat))
                                {
                                    error = "deflate repeat count is truncated";
                                    return false;
                                }
                                repeat += 3;
                            }
                            else if (symbol == 17)
                            {
                                if (!reader.ReadBits(3, repeat))
                                {
                                    error = "deflate repeat count is truncated";
                                    return false;
                                }
                                repeat += 3;
                            }
                            else
                            {
                                if (!reader.ReadBits(7, repeat))
                                {
                                    error = "deflate repeat count is truncated";
                                    return false;
                                }
                                repeat += 11;
                            }

                            for (std::uint32_t index = 0;
                                 index < repeat && lengths.size() < static_cast<std::size_t>(literalCount + distanceCount);
                                 ++index)
                            {
                                lengths.push_back(value);
                            }
                        }

                        if (!literals.Build(lengths.data(), static_cast<int>(literalCount)) ||
                            !distances.Build(lengths.data() + literalCount, static_cast<int>(distanceCount)))
                        {
                            error = "deflate literal or distance table is malformed";
                            return false;
                        }
                    }

                    for (;;)
                    {
                        const int symbol = literals.Decode(reader);
                        if (symbol < 0)
                        {
                            error = "deflate stream ended inside a block";
                            return false;
                        }

                        if (symbol < 256)
                        {
                            out.push_back(static_cast<std::uint8_t>(symbol));
                            continue;
                        }

                        if (symbol == 256)
                        {
                            break;
                        }

                        const int lengthIndex = symbol - 257;
                        if (lengthIndex < 0 || lengthIndex >= 29)
                        {
                            error = "deflate length symbol is out of range";
                            return false;
                        }

                        std::uint32_t extra = 0;
                        if (!reader.ReadBits(LengthExtra[lengthIndex], extra))
                        {
                            error = "deflate length extra bits are truncated";
                            return false;
                        }

                        const std::uint32_t length = LengthBase[lengthIndex] + extra;

                        const int distanceSymbol = distances.Decode(reader);
                        if (distanceSymbol < 0 || distanceSymbol >= 30)
                        {
                            error = "deflate distance symbol is out of range";
                            return false;
                        }

                        std::uint32_t distanceExtra = 0;
                        if (!reader.ReadBits(DistanceExtra[distanceSymbol], distanceExtra))
                        {
                            error = "deflate distance extra bits are truncated";
                            return false;
                        }

                        const std::uint32_t distance = DistanceBase[distanceSymbol] + distanceExtra;
                        if (distance > out.size())
                        {
                            error = "deflate distance points before the output start";
                            return false;
                        }

                        const std::size_t start = out.size();
                        out.resize(start + length);

                        for (std::uint32_t index = 0; index < length; ++index)
                        {
                            out[start + index] = out[start + index - distance];
                        }
                    }
                }
                else
                {
                    error = "deflate block type 3 is reserved";
                    return false;
                }

                if (final != 0)
                {
                    return true;
                }
            }
        }

        bool ZlibDecompress(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out,
                            std::string& error)
        {
            if (size < 6)
            {
                error = "zlib stream is too short";
                return false;
            }

            const std::uint8_t cmf = data[0];
            const std::uint8_t flg = data[1];

            if ((cmf & 0x0Fu) != 8 || ((cmf >> 4) & 0x0Fu) > 7)
            {
                error = "zlib stream does not use the deflate method";
                return false;
            }

            if (((static_cast<std::uint32_t>(cmf) << 8) | flg) % 31u != 0u)
            {
                error = "zlib header check value does not match";
                return false;
            }

            BitReader reader(data + 2, size - 2);
            if (!InflateRaw(reader, out, error))
            {
                return false;
            }

            return true;
        }

        // -------------------------------------------------------------------
        // DEFLATE compression: fixed Huffman codes over a greedy LZ77 match
        // -------------------------------------------------------------------

        void EmitFixedLiteral(BitWriter& writer, std::uint32_t symbol)
        {
            if (symbol < 144)
            {
                writer.WriteCode(0x30 + symbol, 8);
            }
            else if (symbol < 256)
            {
                writer.WriteCode(0x190 + (symbol - 144), 9);
            }
            else if (symbol < 280)
            {
                writer.WriteCode(symbol - 256, 7);
            }
            else
            {
                writer.WriteCode(0xC0 + (symbol - 280), 8);
            }
        }

        void EmitMatch(BitWriter& writer, std::uint32_t length, std::uint32_t distance)
        {
            int lengthCode = 28;
            for (int index = 0; index < 29; ++index)
            {
                const std::uint32_t maximum = LengthBase[index] + ((1u << LengthExtra[index]) - 1u);
                if (length <= maximum)
                {
                    lengthCode = index;
                    break;
                }
            }

            EmitFixedLiteral(writer, static_cast<std::uint32_t>(257 + lengthCode));
            writer.WriteBits(length - LengthBase[lengthCode], LengthExtra[lengthCode]);

            int distanceCode = 29;
            for (int index = 0; index < 30; ++index)
            {
                const std::uint32_t maximum = DistanceBase[index] + ((1u << DistanceExtra[index]) - 1u);
                if (distance <= maximum)
                {
                    distanceCode = index;
                    break;
                }
            }

            writer.WriteCode(static_cast<std::uint32_t>(distanceCode), 5);
            writer.WriteBits(distance - DistanceBase[distanceCode], DistanceExtra[distanceCode]);
        }

        std::uint32_t HashBytes(const std::uint8_t* data, std::size_t position, std::size_t size)
        {
            if (position + 3 > size)
            {
                return 0;
            }

            const std::uint32_t value = static_cast<std::uint32_t>(data[position]) |
                                        (static_cast<std::uint32_t>(data[position + 1]) << 8) |
                                        (static_cast<std::uint32_t>(data[position + 2]) << 16);
            return (value * 2654435761u) >> 17;
        }

        void DeflateFixed(const std::uint8_t* data, std::size_t size, BitWriter& writer)
        {
            writer.WriteBits(1, 1); // final block
            writer.WriteBits(1, 2); // fixed Huffman codes

            constexpr std::size_t HashSize = 1u << 15;
            std::vector<std::int32_t> head(HashSize, -1);
            std::vector<std::int32_t> previous(size, -1);

            std::size_t position = 0;

            while (position < size)
            {
                std::uint32_t bestLength = 0;
                std::uint32_t bestDistance = 0;

                if (position + MinMatch <= size)
                {
                    const std::uint32_t hash = HashBytes(data, position, size);
                    std::int32_t candidate = head[hash];

                    int chain = 0;
                    while (candidate >= 0 && chain < 32)
                    {
                        const std::size_t distance = position - static_cast<std::size_t>(candidate);
                        if (distance == 0 || distance > WindowSize)
                        {
                            break;
                        }

                        const std::size_t limit = std::min<std::size_t>(MaxMatch, size - position);
                        std::size_t length = 0;

                        while (length < limit && data[candidate + length] == data[position + length])
                        {
                            ++length;
                        }

                        if (length > bestLength)
                        {
                            bestLength = static_cast<std::uint32_t>(length);
                            bestDistance = static_cast<std::uint32_t>(distance);

                            if (bestLength >= MaxMatch)
                            {
                                break;
                            }
                        }

                        candidate = previous[static_cast<std::size_t>(candidate)];
                        ++chain;
                    }
                }

                if (bestLength >= MinMatch)
                {
                    EmitMatch(writer, bestLength, bestDistance);

                    const std::size_t end = position + bestLength;
                    while (position < end)
                    {
                        if (position + MinMatch <= size)
                        {
                            const std::uint32_t hash = HashBytes(data, position, size);
                            previous[position] = head[hash];
                            head[hash] = static_cast<std::int32_t>(position);
                        }
                        ++position;
                    }
                }
                else
                {
                    EmitFixedLiteral(writer, data[position]);

                    if (position + MinMatch <= size)
                    {
                        const std::uint32_t hash = HashBytes(data, position, size);
                        previous[position] = head[hash];
                        head[hash] = static_cast<std::int32_t>(position);
                    }
                    ++position;
                }
            }

            EmitFixedLiteral(writer, 256);
            writer.Finish();
        }

        std::vector<std::uint8_t> ZlibCompress(const std::uint8_t* data, std::size_t size)
        {
            BitWriter writer;
            DeflateFixed(data, size, writer);

            const std::vector<std::uint8_t>& deflated = writer.Bytes();

            std::vector<std::uint8_t> out;
            out.reserve(deflated.size() + 6);
            out.push_back(0x78);
            out.push_back(0x9C);
            out.insert(out.end(), deflated.begin(), deflated.end());

            const std::uint32_t adler = Adler32(data, size);
            out.push_back(static_cast<std::uint8_t>((adler >> 24) & 0xFFu));
            out.push_back(static_cast<std::uint8_t>((adler >> 16) & 0xFFu));
            out.push_back(static_cast<std::uint8_t>((adler >> 8) & 0xFFu));
            out.push_back(static_cast<std::uint8_t>(adler & 0xFFu));
            return out;
        }

        // -------------------------------------------------------------------
        // PNG
        // -------------------------------------------------------------------

        const std::uint8_t PngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

        std::uint32_t ReadBigEndian(const std::uint8_t* data)
        {
            return (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) |
                   (static_cast<std::uint32_t>(data[2]) << 8) | static_cast<std::uint32_t>(data[3]);
        }

        void WriteBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
            out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
            out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
            out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        }

        void WriteChunk(std::vector<std::uint8_t>& out, const char type[4], const std::uint8_t* data, std::size_t size)
        {
            WriteBigEndian(out, static_cast<std::uint32_t>(size));

            const std::size_t crcStart = out.size();
            out.insert(out.end(), type, type + 4);
            if (size != 0)
            {
                out.insert(out.end(), data, data + size);
            }

            const std::uint32_t crc = Crc32(out.data() + crcStart, out.size() - crcStart, 0);
            WriteBigEndian(out, crc);
        }

        int PaethPredictor(int left, int above, int upperLeft)
        {
            const int estimate = left + above - upperLeft;
            const int distanceLeft = std::abs(estimate - left);
            const int distanceAbove = std::abs(estimate - above);
            const int distanceUpperLeft = std::abs(estimate - upperLeft);

            if (distanceLeft <= distanceAbove && distanceLeft <= distanceUpperLeft)
            {
                return left;
            }

            return distanceAbove <= distanceUpperLeft ? above : upperLeft;
        }

        /// Filters one row and returns how well it compressed: the classic sum of
        /// absolute signed differences. The cheapest filter wins, per row.
        std::size_t ApplyFilter(int filter,
                                const std::uint8_t* row,
                                const std::uint8_t* previousRow,
                                std::size_t stride,
                                std::size_t bytesPerPixel,
                                std::vector<std::uint8_t>& out)
        {
            std::size_t score = 0;

            for (std::size_t index = 0; index < stride; ++index)
            {
                const int left = index >= bytesPerPixel ? row[index - bytesPerPixel] : 0;
                const int above = previousRow != nullptr ? previousRow[index] : 0;
                const int upperLeft =
                    previousRow != nullptr && index >= bytesPerPixel ? previousRow[index - bytesPerPixel] : 0;

                int value = 0;

                switch (filter)
                {
                case 0: value = row[index]; break;
                case 1: value = row[index] - left; break;
                case 2: value = row[index] - above; break;
                case 3: value = row[index] - ((left + above) >> 1); break;
                default: value = row[index] - PaethPredictor(left, above, upperLeft); break;
                }

                const std::uint8_t byte = static_cast<std::uint8_t>(value & 0xFF);
                out.push_back(byte);
                score += static_cast<std::size_t>(byte < 128 ? byte : 256 - byte);
            }

            return score;
        }

        // -------------------------------------------------------------------
        // BMP and PPM
        // -------------------------------------------------------------------

        ImageDecodeResult DecodeBmp(const std::uint8_t* data, std::size_t size, Image& out)
        {
            ImageDecodeResult result;
            result.format = "bmp";

            if (size < 54 || data[0] != 'B' || data[1] != 'M')
            {
                result.message = "not a BMP file";
                return result;
            }

            const std::uint32_t pixelOffset =
                static_cast<std::uint32_t>(data[10]) | (static_cast<std::uint32_t>(data[11]) << 8) |
                (static_cast<std::uint32_t>(data[12]) << 16) | (static_cast<std::uint32_t>(data[13]) << 24);
            const std::uint32_t headerSize =
                static_cast<std::uint32_t>(data[14]) | (static_cast<std::uint32_t>(data[15]) << 8);

            if (headerSize < 40)
            {
                result.message = "unsupported BMP header size";
                return result;
            }

            const std::int32_t width = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(data[18]) | (static_cast<std::uint32_t>(data[19]) << 8) |
                (static_cast<std::uint32_t>(data[20]) << 16) | (static_cast<std::uint32_t>(data[21]) << 24));
            const std::int32_t rawHeight = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(data[22]) | (static_cast<std::uint32_t>(data[23]) << 8) |
                (static_cast<std::uint32_t>(data[24]) << 16) | (static_cast<std::uint32_t>(data[25]) << 24));
            const std::uint16_t bitsPerPixel =
                static_cast<std::uint16_t>(data[28] | (static_cast<std::uint16_t>(data[29]) << 8));
            const std::uint32_t compression =
                static_cast<std::uint32_t>(data[30]) | (static_cast<std::uint32_t>(data[31]) << 8) |
                (static_cast<std::uint32_t>(data[32]) << 16) | (static_cast<std::uint32_t>(data[33]) << 24);

            if (width <= 0 || rawHeight == 0)
            {
                result.message = "BMP has no pixels";
                return result;
            }

            if (compression != 0)
            {
                result.message = "only uncompressed BMP is supported";
                return result;
            }

            if (bitsPerPixel != 24 && bitsPerPixel != 32)
            {
                result.message = "only 24 and 32 bit BMP is supported";
                return result;
            }

            const bool topDown = rawHeight < 0;
            const std::uint32_t height =
                static_cast<std::uint32_t>(topDown ? -rawHeight : rawHeight);

            const std::size_t sourceStride = (static_cast<std::size_t>(width) * bitsPerPixel / 8u + 3u) & ~std::size_t(3);
            const std::size_t required = static_cast<std::size_t>(pixelOffset) + sourceStride * height;

            if (size < required)
            {
                result.message = "BMP pixel data is truncated";
                result.offset = size;
                return result;
            }

            out.Allocate(static_cast<std::uint32_t>(width), height, PixelFormat::Rgba8);

            for (std::uint32_t row = 0; row < height; ++row)
            {
                const std::uint32_t sourceRow = topDown ? row : height - 1u - row;
                const std::uint8_t* source = data + pixelOffset + sourceStride * sourceRow;
                std::uint8_t* destination = out.MutablePixel(0, row);

                for (std::uint32_t column = 0; column < static_cast<std::uint32_t>(width); ++column)
                {
                    const std::uint8_t* pixel = source + column * (bitsPerPixel / 8u);
                    destination[column * 4 + 0] = pixel[2];
                    destination[column * 4 + 1] = pixel[1];
                    destination[column * 4 + 2] = pixel[0];
                    destination[column * 4 + 3] = bitsPerPixel == 32 ? pixel[3] : 255;
                }
            }

            result.success = true;
            return result;
        }

        ImageDecodeResult DecodePpm(const std::uint8_t* data, std::size_t size, Image& out)
        {
            ImageDecodeResult result;
            result.format = "ppm";

            if (size < 8 || data[0] != 'P' || data[1] != '6')
            {
                result.message = "not a binary PPM file";
                return result;
            }

            std::size_t position = 2;
            const auto skipWhitespace = [&]() {
                while (position < size)
                {
                    const std::uint8_t byte = data[position];
                    if (byte == '#')
                    {
                        while (position < size && data[position] != '\n')
                        {
                            ++position;
                        }
                        continue;
                    }

                    if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r')
                    {
                        ++position;
                        continue;
                    }

                    break;
                }
            };

            const auto readNumber = [&](std::uint32_t& value) -> bool {
                skipWhitespace();

                if (position >= size || data[position] < '0' || data[position] > '9')
                {
                    return false;
                }

                value = 0;
                while (position < size && data[position] >= '0' && data[position] <= '9')
                {
                    value = value * 10u + static_cast<std::uint32_t>(data[position] - '0');
                    ++position;
                }

                return true;
            };

            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t maximum = 0;

            if (!readNumber(width) || !readNumber(height) || !readNumber(maximum))
            {
                result.message = "PPM header is malformed";
                return result;
            }

            if (maximum == 0 || maximum > 255)
            {
                result.message = "PPM maximum channel value is unsupported";
                return result;
            }

            ++position; // single whitespace byte after the maximum value

            const std::size_t pixelBytes = static_cast<std::size_t>(width) * height * 3;
            if (position + pixelBytes > size)
            {
                result.message = "PPM pixel data is truncated";
                result.offset = size;
                return result;
            }

            out.Allocate(width, height, PixelFormat::Rgba8);

            for (std::uint32_t y = 0; y < height; ++y)
            {
                for (std::uint32_t x = 0; x < width; ++x)
                {
                    const std::uint8_t* source = data + position + (static_cast<std::size_t>(y) * width + x) * 3;
                    std::uint8_t* destination = out.MutablePixel(x, y);
                    destination[0] = source[0];
                    destination[1] = source[1];
                    destination[2] = source[2];
                    destination[3] = 255;
                }
            }

            result.success = true;
            return result;
        }

        ImageDecodeResult DecodePng(const std::uint8_t* data, std::size_t size, Image& out)
        {
            ImageDecodeResult result;
            result.format = "png";

            if (size < 8 || std::memcmp(data, PngSignature, 8) != 0)
            {
                result.message = "not a PNG file";
                return result;
            }

            std::size_t position = 8;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint8_t bitDepth = 0;
            std::uint8_t colorType = 0;
            std::uint8_t interlace = 0;
            bool sawHeader = false;
            std::vector<std::uint8_t> compressed;

            while (position + 8 <= size)
            {
                const std::uint32_t length = ReadBigEndian(data + position);
                position += 4;

                if (position + 4 + length + 4 > size)
                {
                    result.message = "PNG chunk runs past the end of the file";
                    result.offset = position;
                    return result;
                }

                const char* type = reinterpret_cast<const char*>(data + position);
                const std::uint8_t* chunk = data + position + 4;
                position += 4 + length + 4;

                if (std::memcmp(type, "IHDR", 4) == 0)
                {
                    if (length < 13)
                    {
                        result.message = "PNG header chunk is truncated";
                        return result;
                    }

                    width = ReadBigEndian(chunk);
                    height = ReadBigEndian(chunk + 4);
                    bitDepth = chunk[8];
                    colorType = chunk[9];
                    interlace = chunk[12];
                    sawHeader = true;
                    continue;
                }

                if (std::memcmp(type, "IDAT", 4) == 0)
                {
                    compressed.insert(compressed.end(), chunk, chunk + length);
                    continue;
                }

                if (std::memcmp(type, "IEND", 4) == 0)
                {
                    break;
                }
            }

            if (!sawHeader || width == 0 || height == 0)
            {
                result.message = "PNG has no header";
                return result;
            }

            if (bitDepth != 8)
            {
                result.message = "only 8 bit PNG channels are supported";
                return result;
            }

            if (interlace != 0)
            {
                result.message = "interlaced PNG is not supported";
                return result;
            }

            std::size_t channels = 0;

            switch (colorType)
            {
            case 0: channels = 1; break;
            case 2: channels = 3; break;
            case 4: channels = 2; break;
            case 6: channels = 4; break;
            default:
                result.message = "unsupported PNG colour type";
                return result;
            }

            std::vector<std::uint8_t> raw;
            std::string error;
            if (!ZlibDecompress(compressed.data(), compressed.size(), raw, error))
            {
                result.message = error;
                return result;
            }

            const std::size_t stride = static_cast<std::size_t>(width) * channels;
            const std::size_t expected = (stride + 1) * height;

            if (raw.size() < expected)
            {
                result.message = "PNG pixel data is truncated";
                result.offset = raw.size();
                return result;
            }

            out.Allocate(width, height, PixelFormat::Rgba8);

            std::vector<std::uint8_t> current(stride, 0);
            std::vector<std::uint8_t> previous(stride, 0);

            std::size_t offset = 0;

            for (std::uint32_t row = 0; row < height; ++row)
            {
                const std::uint8_t filter = raw[offset++];

                for (std::size_t index = 0; index < stride; ++index)
                {
                    const int left = index >= channels ? current[index - channels] : 0;
                    const int above = previous[index];
                    const int upperLeft = index >= channels ? previous[index - channels] : 0;
                    const std::uint8_t source = raw[offset + index];

                    int value = 0;

                    switch (filter)
                    {
                    case 0: value = source; break;
                    case 1: value = source + left; break;
                    case 2: value = source + above; break;
                    case 3: value = source + ((left + above) >> 1); break;
                    case 4: value = source + PaethPredictor(left, above, upperLeft); break;
                    default:
                        result.message = "PNG row uses an unknown filter";
                        return result;
                    }

                    current[index] = static_cast<std::uint8_t>(value & 0xFF);
                }

                offset += stride;

                std::uint8_t* destination = out.MutablePixel(0, row);

                for (std::uint32_t column = 0; column < width; ++column)
                {
                    const std::uint8_t* source = current.data() + static_cast<std::size_t>(column) * channels;
                    std::uint8_t* pixel = destination + static_cast<std::size_t>(column) * 4;

                    switch (colorType)
                    {
                    case 0:
                        pixel[0] = source[0];
                        pixel[1] = source[0];
                        pixel[2] = source[0];
                        pixel[3] = 255;
                        break;
                    case 2:
                        pixel[0] = source[0];
                        pixel[1] = source[1];
                        pixel[2] = source[2];
                        pixel[3] = 255;
                        break;
                    case 4:
                        pixel[0] = source[0];
                        pixel[1] = source[0];
                        pixel[2] = source[0];
                        pixel[3] = source[1];
                        break;
                    default:
                        pixel[0] = source[0];
                        pixel[1] = source[1];
                        pixel[2] = source[2];
                        pixel[3] = source[3];
                        break;
                    }
                }

                previous = current;
            }

            result.success = true;
            return result;
        }
    }

    const char* ToString(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::Rgba8: return "Rgba8";
        case PixelFormat::Rgba8Premultiplied: return "Rgba8Premultiplied";
        case PixelFormat::Bgra8: return "Bgra8";
        case PixelFormat::Gray8: return "Gray8";
        }

        return "Unknown";
    }

    ImageDecodeResult DecodeImage(const std::uint8_t* data, std::size_t size, Image& out)
    {
        if (data == nullptr || size == 0)
        {
            ImageDecodeResult result;
            result.message = "no image data";
            return result;
        }

        if (size >= 8 && std::memcmp(data, PngSignature, 8) == 0)
        {
            return DecodePng(data, size, out);
        }

        if (size >= 2 && data[0] == 'B' && data[1] == 'M')
        {
            return DecodeBmp(data, size, out);
        }

        if (size >= 2 && data[0] == 'P' && data[1] == '6')
        {
            return DecodePpm(data, size, out);
        }

        ImageDecodeResult result;
        result.message = "unrecognised image format";
        return result;
    }

    ImageDecodeResult DecodeImageFile(const char* path, Image& out)
    {
        ImageDecodeResult result;

        if (path == nullptr)
        {
            result.message = "no path";
            return result;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            result.message = "cannot open file";
            return result;
        }

        const std::vector<std::uint8_t> buffer{ std::istreambuf_iterator<char>(file),
                                                std::istreambuf_iterator<char>() };

        if (buffer.empty())
        {
            result.message = "file is empty";
            return result;
        }

        return DecodeImage(buffer.data(), buffer.size(), out);
    }

    std::vector<std::uint8_t> EncodePng(const Image& image)
    {
        std::vector<std::uint8_t> out;

        if (!image.IsValid())
        {
            return out;
        }

        out.insert(out.end(), PngSignature, PngSignature + 8);

        std::uint8_t header[13];
        header[0] = static_cast<std::uint8_t>((image.width >> 24) & 0xFFu);
        header[1] = static_cast<std::uint8_t>((image.width >> 16) & 0xFFu);
        header[2] = static_cast<std::uint8_t>((image.width >> 8) & 0xFFu);
        header[3] = static_cast<std::uint8_t>(image.width & 0xFFu);
        header[4] = static_cast<std::uint8_t>((image.height >> 24) & 0xFFu);
        header[5] = static_cast<std::uint8_t>((image.height >> 16) & 0xFFu);
        header[6] = static_cast<std::uint8_t>((image.height >> 8) & 0xFFu);
        header[7] = static_cast<std::uint8_t>(image.height & 0xFFu);
        header[8] = 8;  // bit depth
        header[9] = 6;  // colour type: truecolour with alpha
        header[10] = 0; // compression
        header[11] = 0; // filter method
        header[12] = 0; // no interlace

        WriteChunk(out, "IHDR", header, sizeof(header));

        // Rows are re-filtered through every filter and the cheapest one is kept,
        // which is what turns a flat UI screenshot into a small file.
        std::vector<std::uint8_t> filtered;
        filtered.reserve(static_cast<std::size_t>(image.height) * (image.Stride() + 1));

        std::vector<std::uint8_t> candidate;
        std::vector<std::uint8_t> best;

        for (std::uint32_t row = 0; row < image.height; ++row)
        {
            const std::uint8_t* source = image.pixels.data() + static_cast<std::size_t>(row) * image.Stride();
            const std::uint8_t* previous =
                row == 0 ? nullptr : image.pixels.data() + static_cast<std::size_t>(row - 1) * image.Stride();

            int bestFilter = 0;
            std::size_t bestScore = 0;

            for (int filter = 0; filter < 5; ++filter)
            {
                candidate.clear();
                const std::size_t score =
                    ApplyFilter(filter, source, previous, image.Stride(), image.BytesPerPixel(), candidate);

                if (filter == 0 || score < bestScore)
                {
                    bestScore = score;
                    bestFilter = filter;
                    best = candidate;
                }
            }

            filtered.push_back(static_cast<std::uint8_t>(bestFilter));
            filtered.insert(filtered.end(), best.begin(), best.end());
        }

        const std::vector<std::uint8_t> deflated = ZlibCompress(filtered.data(), filtered.size());
        WriteChunk(out, "IDAT", deflated.data(), deflated.size());
        WriteChunk(out, "IEND", nullptr, 0);

        return out;
    }

    bool WritePngFile(const char* path, const Image& image)
    {
        const std::vector<std::uint8_t> encoded = EncodePng(image);
        if (encoded.empty() || path == nullptr)
        {
            return false;
        }

        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            return false;
        }

        file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
        return file.good();
    }

    bool WritePpmFile(const char* path, const Image& image)
    {
        if (!image.IsValid() || path == nullptr)
        {
            return false;
        }

        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            return false;
        }

        file << "P6\n" << image.width << ' ' << image.height << "\n255\n";

        std::vector<std::uint8_t> row(static_cast<std::size_t>(image.width) * 3);

        for (std::uint32_t y = 0; y < image.height; ++y)
        {
            for (std::uint32_t x = 0; x < image.width; ++x)
            {
                const std::uint8_t* pixel = image.Sample(x, y);
                row[static_cast<std::size_t>(x) * 3 + 0] = pixel[0];
                row[static_cast<std::size_t>(x) * 3 + 1] = pixel[1];
                row[static_cast<std::size_t>(x) * 3 + 2] = pixel[2];
            }

            file.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
        }

        return file.good();
    }

    Image MakeSolidImage(std::uint32_t width, std::uint32_t height, std::uint8_t red, std::uint8_t green,
                         std::uint8_t blue, std::uint8_t alpha)
    {
        Image image;
        image.Allocate(width == 0 ? 1u : width, height == 0 ? 1u : height, PixelFormat::Rgba8);

        for (std::uint32_t y = 0; y < image.height; ++y)
        {
            for (std::uint32_t x = 0; x < image.width; ++x)
            {
                std::uint8_t* pixel = image.MutablePixel(x, y);
                pixel[0] = red;
                pixel[1] = green;
                pixel[2] = blue;
                pixel[3] = alpha;
            }
        }

        return image;
    }

    Image MakeCheckerboardImage(std::uint32_t width, std::uint32_t height, std::uint32_t cell)
    {
        Image image;
        image.Allocate(width == 0 ? 1u : width, height == 0 ? 1u : height, PixelFormat::Rgba8);

        const std::uint32_t size = cell == 0 ? 8u : cell;

        for (std::uint32_t y = 0; y < image.height; ++y)
        {
            for (std::uint32_t x = 0; x < image.width; ++x)
            {
                const bool light = ((x / size) + (y / size)) % 2u == 0u;
                std::uint8_t* pixel = image.MutablePixel(x, y);
                pixel[0] = light ? 226 : 200;
                pixel[1] = light ? 232 : 206;
                pixel[2] = light ? 240 : 216;
                pixel[3] = 255;
            }
        }

        return image;
    }

    Image MakePlaceholderImage(std::uint32_t width, std::uint32_t height)
    {
        const std::uint32_t safeWidth = width == 0 ? 1u : width;
        const std::uint32_t safeHeight = height == 0 ? 1u : height;

        Image image = MakeSolidImage(safeWidth, safeHeight, 241, 245, 249, 255);

        // A diagonal hatch plus a border, so a missing resource is obvious.
        for (std::uint32_t y = 0; y < safeHeight; ++y)
        {
            for (std::uint32_t x = 0; x < safeWidth; ++x)
            {
                const bool nearBorder = x == 0 || y == 0 || x + 1 == safeWidth || y + 1 == safeHeight;

                if (nearBorder)
                {
                    std::uint8_t* pixel = image.MutablePixel(x, y);
                    pixel[0] = 203;
                    pixel[1] = 213;
                    pixel[2] = 225;
                    continue;
                }

                if ((x + y) % 12u == 0u)
                {
                    std::uint8_t* pixel = image.MutablePixel(x, y);
                    pixel[0] = 226;
                    pixel[1] = 232;
                    pixel[2] = 240;
                }
            }
        }

        return image;
    }
}
