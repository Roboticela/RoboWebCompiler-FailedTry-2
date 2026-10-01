// ---------------------------------------------------------------------------
// RoboRenderer - drawing an OpenType `SVG ` glyph.
//
// See `SvgGlyph.h` for what this reader covers and what it deliberately skips.
// The document is walked as text rather than through a DOM: a font's SVG glyphs
// are small, flat documents, and a parser that allocates a tree per glyph would
// cost more than the fill it feeds.
// ---------------------------------------------------------------------------
#include "RoboRenderer/SvgGlyph.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace RoboRenderer
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float Tau = 6.28318530717958647692f;

        // -------------------------------------------------------------------
        // An affine transform, in the SVG convention:
        //   x' = a*x + c*y + e ;  y' = b*x + d*y + f
        // -------------------------------------------------------------------
        struct Matrix
        {
            float a = 1.0f;
            float b = 0.0f;
            float c = 0.0f;
            float d = 1.0f;
            float e = 0.0f;
            float f = 0.0f;
        };

        Matrix Multiply(const Matrix& left, const Matrix& right)
        {
            Matrix result;
            result.a = left.a * right.a + left.c * right.b;
            result.b = left.b * right.a + left.d * right.b;
            result.c = left.a * right.c + left.c * right.d;
            result.d = left.b * right.c + left.d * right.d;
            result.e = left.a * right.e + left.c * right.f + left.e;
            result.f = left.b * right.e + left.d * right.f + left.f;
            return result;
        }

        SvgPoint Apply(const Matrix& matrix, float x, float y)
        {
            SvgPoint point;
            point.x = matrix.a * x + matrix.c * y + matrix.e;
            point.y = matrix.b * x + matrix.d * y + matrix.f;
            return point;
        }

        // -------------------------------------------------------------------
        // Text scanning helpers
        // -------------------------------------------------------------------

        bool IsSpace(char character)
        {
            return std::isspace(static_cast<unsigned char>(character)) != 0;
        }

        std::string Trim(const std::string& value)
        {
            std::size_t begin = 0;
            std::size_t end = value.size();
            while (begin < end && IsSpace(value[begin]))
            {
                ++begin;
            }
            while (end > begin && IsSpace(value[end - 1]))
            {
                --end;
            }
            return value.substr(begin, end - begin);
        }

        /// Reads a number starting at `index`: sign, digits, fraction, exponent.
        /// SVG path data may omit the separator between numbers (`10-20`), so a
        /// scanner that trusts whitespace would misread it.
        bool ReadNumber(const std::string& text, std::size_t& index, float& out)
        {
            const std::size_t begin = index;
            bool seenDigit = false;

            if (index < text.size() && (text[index] == '+' || text[index] == '-'))
            {
                ++index;
            }

            while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0)
            {
                ++index;
                seenDigit = true;
            }

            if (index < text.size() && text[index] == '.')
            {
                ++index;
                while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0)
                {
                    ++index;
                    seenDigit = true;
                }
            }

            if (!seenDigit)
            {
                index = begin;
                return false;
            }

            if (index < text.size() && (text[index] == 'e' || text[index] == 'E'))
            {
                const std::size_t exponent = index;
                ++index;
                if (index < text.size() && (text[index] == '+' || text[index] == '-'))
                {
                    ++index;
                }
                if (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0)
                {
                    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0)
                    {
                        ++index;
                    }
                }
                else
                {
                    index = exponent;
                }
            }

            out = static_cast<float>(std::strtod(text.substr(begin, index - begin).c_str(), nullptr));
            return true;
        }

        void SkipSeparators(const std::string& text, std::size_t& index)
        {
            while (index < text.size() && (IsSpace(text[index]) || text[index] == ','))
            {
                ++index;
            }
        }

        /// A length: a number with an optional unit. `em`/`ex` are restricted in
        /// OpenType, and a percentage means "of the em", so both are read as a
        /// straight value with the unit dropped.
        bool ReadLength(const std::string& text, std::size_t& index, float& out)
        {
            if (!ReadNumber(text, index, out))
            {
                return false;
            }

            if (index < text.size() && text[index] == '%')
            {
                ++index;
                out /= 100.0f;
            }
            else
            {
                while (index < text.size() && std::isalpha(static_cast<unsigned char>(text[index])) != 0)
                {
                    ++index;
                }
            }
            return true;
        }

        // -------------------------------------------------------------------
        // Colour parsing
        // -------------------------------------------------------------------

        struct NamedColor
        {
            const char* name;
            float red;
            float green;
            float blue;
        };

        const NamedColor NamedColors[] = {
            { "black", 0, 0, 0 },       { "white", 1, 1, 1 },       { "red", 1, 0, 0 },
            { "green", 0, 0.502f, 0 },  { "lime", 0, 1, 0 },        { "blue", 0, 0, 1 },
            { "yellow", 1, 1, 0 },      { "cyan", 0, 1, 1 },        { "aqua", 0, 1, 1 },
            { "magenta", 1, 0, 1 },     { "fuchsia", 1, 0, 1 },     { "gray", 0.502f, 0.502f, 0.502f },
            { "grey", 0.502f, 0.502f, 0.502f }, { "silver", 0.753f, 0.753f, 0.753f },
            { "maroon", 0.502f, 0, 0 }, { "olive", 0.502f, 0.502f, 0 }, { "navy", 0, 0, 0.502f },
            { "teal", 0, 0.502f, 0.502f }, { "purple", 0.502f, 0, 0.502f },
            { "orange", 1, 0.647f, 0 }, { "gold", 1, 0.843f, 0 },   { "pink", 1, 0.753f, 0.796f },
            { "brown", 0.647f, 0.165f, 0.165f },
        };

        int HexDigit(char character)
        {
            if (character >= '0' && character <= '9')
            {
                return character - '0';
            }
            if (character >= 'a' && character <= 'f')
            {
                return character - 'a' + 10;
            }
            if (character >= 'A' && character <= 'F')
            {
                return character - 'A' + 10;
            }
            return -1;
        }
    }

    bool ParseSvgFill(const char* value, SvgPaint& out)
    {
        out = SvgPaint{};

        if (value == nullptr)
        {
            return false;
        }

        const std::string text = Trim(value);
        if (text.empty() || text == "none" || text == "transparent")
        {
            return false;
        }

        if (text[0] == '#')
        {
            std::string digits = text.substr(1);
            if (digits.size() == 3)
            {
                digits = { digits[0], digits[0], digits[1], digits[1], digits[2], digits[2] };
            }
            if (digits.size() != 6)
            {
                return false;
            }

            int channels[3] = { 0, 0, 0 };
            for (int index = 0; index < 3; ++index)
            {
                const int high = HexDigit(digits[index * 2]);
                const int low = HexDigit(digits[index * 2 + 1]);
                if (high < 0 || low < 0)
                {
                    return false;
                }
                channels[index] = high * 16 + low;
            }

            out.kind = SvgPaint::Kind::Solid;
            out.red = static_cast<float>(channels[0]) / 255.0f;
            out.green = static_cast<float>(channels[1]) / 255.0f;
            out.blue = static_cast<float>(channels[2]) / 255.0f;
            return true;
        }

        if (text.size() > 4 && text.compare(0, 4, "rgb(") == 0)
        {
            float channels[3] = { 0.0f, 0.0f, 0.0f };
            std::size_t index = 4;
            for (int channel = 0; channel < 3; ++channel)
            {
                SkipSeparators(text, index);

                float component = 0.0f;
                if (!ReadNumber(text, index, component))
                {
                    return false;
                }

                // `rgb(0, 128, 255)` is 0..255; `rgb(50%, 0%, 0%)` is a fraction.
                if (index < text.size() && text[index] == '%')
                {
                    ++index;
                    component /= 100.0f;
                }
                else
                {
                    component /= 255.0f;
                }

                channels[channel] = component;
            }

            out.kind = SvgPaint::Kind::Solid;
            out.red = std::min(1.0f, std::max(0.0f, channels[0]));
            out.green = std::min(1.0f, std::max(0.0f, channels[1]));
            out.blue = std::min(1.0f, std::max(0.0f, channels[2]));
            return true;
        }

        for (const NamedColor& named : NamedColors)
        {
            if (text == named.name)
            {
                out.kind = SvgPaint::Kind::Solid;
                out.red = named.red;
                out.green = named.green;
                out.blue = named.blue;
                return true;
            }
        }

        // `url(#…)` and `currentColor` are resolved by the caller, not here.
        return false;
    }

    namespace
    {
        // -------------------------------------------------------------------
        // The document
        // -------------------------------------------------------------------

        struct Gradient
        {
            bool radial = false;
            bool userSpace = false;
            std::string coordinates;
            std::vector<SvgPaint::Stop> stops;
        };

        /// Every attribute of one start tag, as name -> raw value.
        void ParseAttributes(const std::string& tag,
                             std::unordered_map<std::string, std::string>& out)
        {
            std::size_t index = 0;

            // Skip the element name.
            while (index < tag.size() && !IsSpace(tag[index]) && tag[index] != '/' && tag[index] != '>')
            {
                ++index;
            }

            while (index < tag.size())
            {
                while (index < tag.size() && (IsSpace(tag[index]) || tag[index] == '/'))
                {
                    ++index;
                }
                if (index >= tag.size() || tag[index] == '>')
                {
                    break;
                }

                const std::size_t nameBegin = index;
                while (index < tag.size() && tag[index] != '=' && !IsSpace(tag[index]) && tag[index] != '>')
                {
                    ++index;
                }
                const std::string name = tag.substr(nameBegin, index - nameBegin);

                while (index < tag.size() && IsSpace(tag[index]))
                {
                    ++index;
                }

                if (index >= tag.size() || tag[index] != '=')
                {
                    if (!name.empty())
                    {
                        out[name] = std::string();
                    }
                    continue;
                }
                ++index;

                while (index < tag.size() && IsSpace(tag[index]))
                {
                    ++index;
                }

                std::string value;
                if (index < tag.size() && (tag[index] == '"' || tag[index] == '\''))
                {
                    const char quote = tag[index];
                    ++index;
                    while (index < tag.size() && tag[index] != quote)
                    {
                        value += tag[index];
                        ++index;
                    }
                    if (index < tag.size())
                    {
                        ++index;
                    }
                }
                else
                {
                    while (index < tag.size() && !IsSpace(tag[index]) && tag[index] != '>')
                    {
                        value += tag[index];
                        ++index;
                    }
                }

                out[name] = value;
            }
        }

        Matrix ParseTransform(const std::string& text)
        {
            Matrix result;
            std::size_t index = 0;

            while (index < text.size())
            {
                while (index < text.size() && (IsSpace(text[index]) || text[index] == ','))
                {
                    ++index;
                }
                if (index >= text.size())
                {
                    break;
                }

                const std::size_t nameBegin = index;
                while (index < text.size() && std::isalpha(static_cast<unsigned char>(text[index])) != 0)
                {
                    ++index;
                }
                const std::string name = text.substr(nameBegin, index - nameBegin);

                while (index < text.size() && IsSpace(text[index]))
                {
                    ++index;
                }
                if (index >= text.size() || text[index] != '(')
                {
                    break;
                }
                ++index;

                float values[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
                int count = 0;

                while (index < text.size() && text[index] != ')' && count < 6)
                {
                    SkipSeparators(text, index);
                    if (index >= text.size() || text[index] == ')')
                    {
                        break;
                    }
                    if (!ReadNumber(text, index, values[count]))
                    {
                        break;
                    }
                    ++count;
                }

                while (index < text.size() && text[index] != ')')
                {
                    ++index;
                }
                if (index < text.size())
                {
                    ++index;
                }

                Matrix local;
                if (name == "translate")
                {
                    local.e = values[0];
                    local.f = count > 1 ? values[1] : 0.0f;
                }
                else if (name == "scale")
                {
                    local.a = values[0];
                    local.d = count > 1 ? values[1] : values[0];
                }
                else if (name == "rotate")
                {
                    // rotate(angle [cx cy])
                    const float radians = values[0] * Pi / 180.0f;
                    const float cosine = std::cos(radians);
                    const float sine = std::sin(radians);
                    Matrix rotation;
                    rotation.a = cosine;
                    rotation.b = sine;
                    rotation.c = -sine;
                    rotation.d = cosine;

                    if (count >= 3)
                    {
                        Matrix toCenter;
                        toCenter.e = values[1];
                        toCenter.f = values[2];
                        Matrix fromCenter;
                        fromCenter.e = -values[1];
                        fromCenter.f = -values[2];
                        local = Multiply(Multiply(toCenter, rotation), fromCenter);
                    }
                    else
                    {
                        local = rotation;
                    }
                }
                else if (name == "matrix" && count >= 6)
                {
                    local.a = values[0];
                    local.b = values[1];
                    local.c = values[2];
                    local.d = values[3];
                    local.e = values[4];
                    local.f = values[5];
                }
                else if (name == "skewX")
                {
                    local.c = std::tan(values[0] * Pi / 180.0f);
                }
                else if (name == "skewY")
                {
                    local.b = std::tan(values[0] * Pi / 180.0f);
                }

                result = Multiply(result, local);
            }

            return result;
        }

        /// Node of a very small XML scanner: a start tag, an end tag, or text.
        struct Token
        {
            enum class Kind : std::uint8_t
            {
                Start,
                End,
                SelfClosing,
                Text
            };

            Kind kind = Kind::Text;
            std::string name;
            std::string tag;
        };

        bool NextToken(const std::string& document, std::size_t& index, Token& out)
        {
            while (index < document.size())
            {
                if (document[index] != '<')
                {
                    const std::size_t begin = index;
                    while (index < document.size() && document[index] != '<')
                    {
                        ++index;
                    }
                    out.kind = Token::Kind::Text;
                    out.name.clear();
                    out.tag = document.substr(begin, index - begin);
                    return true;
                }

                if (index + 1 >= document.size())
                {
                    return false;
                }

                const char next = document[index + 1];
                if (next == '!' || next == '?')
                {
                    // Comment, doctype or processing instruction: skip to its end.
                    const std::size_t close = document.find('>', index);
                    if (close == std::string::npos)
                    {
                        return false;
                    }
                    index = close + 1;
                    continue;
                }

                const bool closing = next == '/';
                const std::size_t tagBegin = closing ? index + 2 : index + 1;
                const std::size_t close = document.find('>', tagBegin);
                if (close == std::string::npos)
                {
                    return false;
                }

                out.tag = document.substr(tagBegin, close - tagBegin);
                const bool selfClosing = !out.tag.empty() && out.tag.back() == '/';
                if (selfClosing)
                {
                    out.tag.pop_back();
                }

                std::size_t nameEnd = 0;
                while (nameEnd < out.tag.size() && !IsSpace(out.tag[nameEnd]) && out.tag[nameEnd] != '/')
                {
                    ++nameEnd;
                }
                out.name = out.tag.substr(0, nameEnd);

                out.kind = closing ? Token::Kind::End
                                   : (selfClosing ? Token::Kind::SelfClosing : Token::Kind::Start);
                index = close + 1;
                return true;
            }

            return false;
        }

        std::string Attribute(const std::unordered_map<std::string, std::string>& attributes,
                              const char* name)
        {
            const auto found = attributes.find(name);
            return found != attributes.end() ? found->second : std::string();
        }

        bool AttributeNumber(const std::unordered_map<std::string, std::string>& attributes,
                             const char* name,
                             float& out)
        {
            const auto found = attributes.find(name);
            if (found == attributes.end())
            {
                return false;
            }
            std::size_t index = 0;
            return ReadLength(found->second, index, out);
        }

        void AppendPolygon(std::vector<std::vector<SvgPoint>>& contours,
                           const Matrix& matrix,
                           const std::vector<SvgPoint>& points)
        {
            if (points.size() < 3)
            {
                return;
            }

            std::vector<SvgPoint> transformed;
            transformed.reserve(points.size());
            for (const SvgPoint& point : points)
            {
                transformed.push_back(Apply(matrix, point.x, point.y));
            }
            contours.push_back(std::move(transformed));
        }

        std::vector<SvgPoint> EllipsePoints(float cx, float cy, float rx, float ry)
        {
            constexpr int Steps = 48;
            std::vector<SvgPoint> points;
            points.reserve(Steps);

            for (int step = 0; step < Steps; ++step)
            {
                const float angle = Tau * static_cast<float>(step) / Steps;
                points.push_back(SvgPoint{ cx + rx * std::cos(angle), cy + ry * std::sin(angle) });
            }

            return points;
        }

        // -------------------------------------------------------------------
        // Path data
        // -------------------------------------------------------------------

        void FlattenCubic(std::vector<SvgPoint>& contour, float x0, float y0, float x1, float y1,
                          float x2, float y2, float x3, float y3)
        {
            constexpr int Steps = 16;
            for (int step = 1; step <= Steps; ++step)
            {
                const float t = static_cast<float>(step) / Steps;
                const float u = 1.0f - t;
                const float a = u * u * u;
                const float b = 3.0f * u * u * t;
                const float c = 3.0f * u * t * t;
                const float d = t * t * t;
                contour.push_back(SvgPoint{ a * x0 + b * x1 + c * x2 + d * x3,
                                            a * y0 + b * y1 + c * y2 + d * y3 });
            }
        }

        void FlattenQuadratic(std::vector<SvgPoint>& contour, float x0, float y0, float x1, float y1,
                              float x2, float y2)
        {
            constexpr int Steps = 12;
            for (int step = 1; step <= Steps; ++step)
            {
                const float t = static_cast<float>(step) / Steps;
                const float u = 1.0f - t;
                contour.push_back(SvgPoint{ u * u * x0 + 2.0f * u * t * x1 + t * t * x2,
                                            u * u * y0 + 2.0f * u * t * y1 + t * t * y2 });
            }
        }

        /// A relative-capable coordinate for the path scanner.
        struct PathScanner
        {
            const std::string& text;
            std::size_t index = 0;
            bool relative = false;

            bool Number(float& out)
            {
                SkipSeparators(text, index);
                return ReadNumber(text, index, out);
            }

            bool Point(float& x, float& y, float currentX, float currentY)
            {
                float rawX = 0.0f;
                float rawY = 0.0f;
                if (!Number(rawX) || !Number(rawY))
                {
                    return false;
                }
                x = relative ? currentX + rawX : rawX;
                y = relative ? currentY + rawY : rawY;
                return true;
            }
        };

        /// Flattens an SVG elliptical arc into the contour.
        void FlattenArc(std::vector<SvgPoint>& contour, float x0, float y0, float rx, float ry,
                        float rotationDegrees, bool largeArc, bool sweep, float x1, float y1)
        {
            if (rx == 0.0f || ry == 0.0f)
            {
                contour.push_back(SvgPoint{ x1, y1 });
                return;
            }

            rx = std::abs(rx);
            ry = std::abs(ry);
            const float phi = rotationDegrees * Pi / 180.0f;
            const float cosPhi = std::cos(phi);
            const float sinPhi = std::sin(phi);

            const float dx = (x0 - x1) * 0.5f;
            const float dy = (y0 - y1) * 0.5f;
            const float x1p = cosPhi * dx + sinPhi * dy;
            const float y1p = -sinPhi * dx + cosPhi * dy;

            float radiusX = rx;
            float radiusY = ry;
            const float lambda = (x1p * x1p) / (radiusX * radiusX) + (y1p * y1p) / (radiusY * radiusY);
            if (lambda > 1.0f)
            {
                const float scale = std::sqrt(lambda);
                radiusX *= scale;
                radiusY *= scale;
            }

            const float numerator = radiusX * radiusX * radiusY * radiusY -
                                    radiusX * radiusX * y1p * y1p - radiusY * radiusY * x1p * x1p;
            const float denominator = radiusX * radiusX * y1p * y1p + radiusY * radiusY * x1p * x1p;
            float factor = denominator > 0.0f ? std::sqrt(std::max(0.0f, numerator / denominator)) : 0.0f;
            if (largeArc == sweep)
            {
                factor = -factor;
            }

            const float cxp = factor * radiusX * y1p / radiusY;
            const float cyp = -factor * radiusY * x1p / radiusX;
            const float cx = cosPhi * cxp - sinPhi * cyp + (x0 + x1) * 0.5f;
            const float cy = sinPhi * cxp + cosPhi * cyp + (y0 + y1) * 0.5f;

            const auto angleOf = [&](float ux, float uy, float vx, float vy) {
                const float dot = ux * vx + uy * vy;
                const float length = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
                float angle = std::acos(std::min(1.0f, std::max(-1.0f, dot / (length > 0.0f ? length : 1.0f))));
                if (ux * vy - uy * vx < 0.0f)
                {
                    angle = -angle;
                }
                return angle;
            };

            const float startAngle = angleOf(1.0f, 0.0f, (x1p - cxp) / radiusX, (y1p - cyp) / radiusY);
            float sweepAngle = angleOf((x1p - cxp) / radiusX, (y1p - cyp) / radiusY,
                                       (-x1p - cxp) / radiusX, (-y1p - cyp) / radiusY);
            if (!sweep && sweepAngle > 0.0f)
            {
                sweepAngle -= Tau;
            }
            else if (sweep && sweepAngle < 0.0f)
            {
                sweepAngle += Tau;
            }

            const int steps = std::max(2, static_cast<int>(std::abs(sweepAngle) / (Pi / 12.0f)));
            for (int step = 1; step <= steps; ++step)
            {
                const float angle = startAngle + sweepAngle * static_cast<float>(step) / steps;
                const float px = radiusX * std::cos(angle);
                const float py = radiusY * std::sin(angle);
                contour.push_back(SvgPoint{ cosPhi * px - sinPhi * py + cx,
                                            sinPhi * px + cosPhi * py + cy });
            }
        }

        void ParsePathData(const std::string& data, std::vector<std::vector<SvgPoint>>& out)
        {
            PathScanner scanner{ data, 0, false };
            float currentX = 0.0f;
            float currentY = 0.0f;
            float startX = 0.0f;
            float startY = 0.0f;
            float lastControlX = 0.0f;
            float lastControlY = 0.0f;
            char previous = 0;
            std::vector<SvgPoint> contour;

            const auto flush = [&]() {
                if (contour.size() >= 3)
                {
                    out.push_back(contour);
                }
                contour.clear();
            };

            while (true)
            {
                SkipSeparators(data, scanner.index);
                if (scanner.index >= data.size())
                {
                    break;
                }

                char command = data[scanner.index];
                if (std::isalpha(static_cast<unsigned char>(command)) != 0)
                {
                    ++scanner.index;
                }
                else if (previous == 'M')
                {
                    command = 'L';
                }
                else if (previous == 'm')
                {
                    command = 'l';
                }
                else if (previous != 0)
                {
                    command = previous;
                }
                else
                {
                    break;
                }

                scanner.relative = std::islower(static_cast<unsigned char>(command)) != 0;
                const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));

                float x = 0.0f;
                float y = 0.0f;
                float x1 = 0.0f;
                float y1 = 0.0f;
                float x2 = 0.0f;
                float y2 = 0.0f;

                if (upper == 'M')
                {
                    if (!scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    flush();
                    currentX = x;
                    currentY = y;
                    startX = x;
                    startY = y;
                    contour.push_back(SvgPoint{ x, y });
                }
                else if (upper == 'L')
                {
                    if (!scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    contour.push_back(SvgPoint{ x, y });
                    currentX = x;
                    currentY = y;
                }
                else if (upper == 'H')
                {
                    if (!scanner.Number(x))
                    {
                        break;
                    }
                    currentX = scanner.relative ? currentX + x : x;
                    contour.push_back(SvgPoint{ currentX, currentY });
                }
                else if (upper == 'V')
                {
                    if (!scanner.Number(y))
                    {
                        break;
                    }
                    currentY = scanner.relative ? currentY + y : y;
                    contour.push_back(SvgPoint{ currentX, currentY });
                }
                else if (upper == 'C')
                {
                    if (!scanner.Point(x1, y1, currentX, currentY) ||
                        !scanner.Point(x2, y2, currentX, currentY) ||
                        !scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    FlattenCubic(contour, currentX, currentY, x1, y1, x2, y2, x, y);
                    lastControlX = x2;
                    lastControlY = y2;
                    currentX = x;
                    currentY = y;
                }
                else if (upper == 'S')
                {
                    if (!scanner.Point(x2, y2, currentX, currentY) ||
                        !scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    const bool smooth = previous != 0 &&
                                        std::toupper(static_cast<unsigned char>(previous)) != 'C' &&
                                        std::toupper(static_cast<unsigned char>(previous)) != 'S';
                    x1 = smooth ? currentX : 2.0f * currentX - lastControlX;
                    y1 = smooth ? currentY : 2.0f * currentY - lastControlY;
                    FlattenCubic(contour, currentX, currentY, x1, y1, x2, y2, x, y);
                    lastControlX = x2;
                    lastControlY = y2;
                    currentX = x;
                    currentY = y;
                }
                else if (upper == 'Q')
                {
                    if (!scanner.Point(x1, y1, currentX, currentY) ||
                        !scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    FlattenQuadratic(contour, currentX, currentY, x1, y1, x, y);
                    lastControlX = x1;
                    lastControlY = y1;
                    currentX = x;
                    currentY = y;
                }
                else if (upper == 'T')
                {
                    if (!scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    const bool smooth = previous != 0 &&
                                        std::toupper(static_cast<unsigned char>(previous)) != 'Q' &&
                                        std::toupper(static_cast<unsigned char>(previous)) != 'T';
                    x1 = smooth ? currentX : 2.0f * currentX - lastControlX;
                    y1 = smooth ? currentY : 2.0f * currentY - lastControlY;
                    FlattenQuadratic(contour, currentX, currentY, x1, y1, x, y);
                    lastControlX = x1;
                    lastControlY = y1;
                    currentX = x;
                    currentY = y;
                }
                else if (upper == 'A')
                {
                    float rx = 0.0f;
                    float ry = 0.0f;
                    float rotation = 0.0f;
                    float largeArc = 0.0f;
                    float sweepFlag = 0.0f;
                    if (!scanner.Number(rx) || !scanner.Number(ry) || !scanner.Number(rotation) ||
                        !scanner.Number(largeArc) || !scanner.Number(sweepFlag) ||
                        !scanner.Point(x, y, currentX, currentY))
                    {
                        break;
                    }
                    FlattenArc(contour, currentX, currentY, rx, ry, rotation, largeArc != 0.0f,
                               sweepFlag != 0.0f, x, y);
                    currentX = x;
                    currentY = y;
                }
                else if (upper == 'Z')
                {
                    flush();
                    currentX = startX;
                    currentY = startY;
                    previous = command;
                    continue;
                }
                else
                {
                    break;
                }

                previous = command;
            }

            flush();
        }

        // -------------------------------------------------------------------
        // Gradients
        // -------------------------------------------------------------------

        void CollectGradients(const std::string& document,
                              std::unordered_map<std::string, Gradient>& out)
        {
            std::size_t index = 0;
            Token token;
            Gradient current;
            std::string currentId;
            bool inGradient = false;

            while (NextToken(document, index, token))
            {
                if (token.kind == Token::Kind::Text)
                {
                    continue;
                }

                std::unordered_map<std::string, std::string> attributes;
                ParseAttributes(token.tag, attributes);

                const bool opening = token.kind == Token::Kind::Start ||
                                     token.kind == Token::Kind::SelfClosing;
                const bool gradientElement =
                    token.name == "linearGradient" || token.name == "radialGradient";

                if (opening && gradientElement)
                {
                    inGradient = true;
                    current = Gradient{};
                    current.radial = token.name == "radialGradient";
                    current.userSpace = Attribute(attributes, "gradientUnits") == "userSpaceOnUse";
                    current.coordinates = Attribute(attributes, "x1") + "," + Attribute(attributes, "y1") +
                                           "," + Attribute(attributes, "x2") + "," + Attribute(attributes, "y2") +
                                           "," + Attribute(attributes, "cx") + "," + Attribute(attributes, "cy") +
                                           "," + Attribute(attributes, "r") + "," +
                                           Attribute(attributes, "fx") + "," + Attribute(attributes, "fy");
                    currentId = Attribute(attributes, "id");
                }
                else if (opening && token.name == "stop" && inGradient)
                {
                    SvgPaint::Stop stop;
                    std::size_t position = 0;
                    ReadLength(Attribute(attributes, "offset"), position, stop.offset);

                    SvgPaint colour;
                    const std::string stopColor = Attribute(attributes, "stop-color");
                    if (ParseSvgFill(stopColor.empty() ? "black" : stopColor.c_str(), colour))
                    {
                        stop.red = colour.red;
                        stop.green = colour.green;
                        stop.blue = colour.blue;
                    }

                    float opacity = 1.0f;
                    std::size_t opacityIndex = 0;
                    const std::string stopOpacity = Attribute(attributes, "stop-opacity");
                    if (!stopOpacity.empty() && ReadLength(stopOpacity, opacityIndex, opacity))
                    {
                        stop.alpha = opacity;
                    }

                    current.stops.push_back(stop);
                }

                if ((token.kind == Token::Kind::End ||
                     (token.kind == Token::Kind::SelfClosing && gradientElement)) &&
                    gradientElement && inGradient)
                {
                    if (!currentId.empty())
                    {
                        out[currentId] = current;
                    }
                    inGradient = false;
                    currentId.clear();
                }
            }
        }

        /// The coordinate pairs a gradient stores, split out of the packed string
        /// collected above.
        void GradientCoordinates(const Gradient& gradient, float values[9], bool present[9])
        {
            for (int slot = 0; slot < 9; ++slot)
            {
                values[slot] = 0.0f;
                present[slot] = false;
            }

            std::size_t index = 0;
            for (int slot = 0; slot < 9; ++slot)
            {
                const std::size_t comma = gradient.coordinates.find(',', index);
                const std::size_t end =
                    comma == std::string::npos ? gradient.coordinates.size() : comma;
                const std::string piece = gradient.coordinates.substr(index, end - index);
                index = comma == std::string::npos ? gradient.coordinates.size() : comma + 1;

                if (!piece.empty())
                {
                    std::size_t position = 0;
                    present[slot] = ReadLength(piece, position, values[slot]);
                }
            }
        }

        /// Resolves a gradient against a shape's bounding box. SVG gradient
        /// coordinates default to objectBoundingBox (0..1 over the shape), which
        /// is why the box is passed in.
        SvgPaint ResolveGradient(const Gradient& gradient, float bx0, float by0, float bx1, float by1)
        {
            SvgPaint paint;
            paint.kind = gradient.radial ? SvgPaint::Kind::Radial : SvgPaint::Kind::Linear;
            paint.stops = gradient.stops;

            float values[9];
            bool present[9];
            GradientCoordinates(gradient, values, present);

            const float width = bx1 - bx0;
            const float height = by1 - by0;

            const auto mapX = [&](float value) { return gradient.userSpace ? value : bx0 + value * width; };
            const auto mapY = [&](float value) { return gradient.userSpace ? value : by0 + value * height; };

            if (!gradient.radial)
            {
                paint.x0 = mapX(present[0] ? values[0] : 0.0f);
                paint.y0 = mapY(present[1] ? values[1] : 0.0f);
                paint.x1 = mapX(present[2] ? values[2] : 1.0f);
                paint.y1 = mapY(present[3] ? values[3] : 0.0f);
            }
            else
            {
                const float radiusScale = gradient.userSpace ? 1.0f : 0.5f * (std::abs(width) + std::abs(height));
                paint.x1 = mapX(present[4] ? values[4] : 0.5f);
                paint.y1 = mapY(present[5] ? values[5] : 0.5f);
                paint.r1 = (present[6] ? values[6] : 0.5f) * radiusScale;
                paint.x0 = mapX(present[7] ? values[7] : (present[4] ? values[4] : 0.5f));
                paint.y0 = mapY(present[8] ? values[8] : (present[5] ? values[5] : 0.5f));
                paint.r0 = 0.0f;
            }

            return paint;
        }

        // -------------------------------------------------------------------
        // The renderer
        // -------------------------------------------------------------------

        struct PendingShape
        {
            std::vector<std::vector<SvgPoint>> contours;
            SvgPaint paint;
            std::string gradientId;
        };

        void ComputeBounds(const std::vector<std::vector<SvgPoint>>& contours,
                           float& x0,
                           float& y0,
                           float& x1,
                           float& y1)
        {
            x0 = y0 = 1.0e30f;
            x1 = y1 = -1.0e30f;

            for (const std::vector<SvgPoint>& contour : contours)
            {
                for (const SvgPoint& point : contour)
                {
                    x0 = std::min(x0, point.x);
                    y0 = std::min(y0, point.y);
                    x1 = std::max(x1, point.x);
                    y1 = std::max(y1, point.y);
                }
            }
        }
    }

    bool RenderSvgGlyph(const std::uint8_t* data, std::size_t size, std::vector<SvgShape>& out)
    {
        out.clear();

        if (data == nullptr || size == 0)
        {
            return false;
        }

        const std::string document(reinterpret_cast<const char*>(data), size);

        std::unordered_map<std::string, Gradient> gradients;
        CollectGradients(document, gradients);

        std::vector<PendingShape> pending;
        std::vector<Matrix> stack;
        Matrix current;

        std::size_t index = 0;
        Token token;

        while (NextToken(document, index, token))
        {
            if (token.kind == Token::Kind::Text)
            {
                continue;
            }

            if (token.kind == Token::Kind::End)
            {
                if (token.name == "g" && !stack.empty())
                {
                    current = stack.back();
                    stack.pop_back();
                }
                continue;
            }

            std::unordered_map<std::string, std::string> attributes;
            ParseAttributes(token.tag, attributes);

            if (token.name == "g")
            {
                if (token.kind == Token::Kind::SelfClosing)
                {
                    continue;
                }
                stack.push_back(current);
                current = Multiply(current, ParseTransform(Attribute(attributes, "transform")));
                continue;
            }

            if (token.name == "defs" || token.name == "linearGradient" || token.name == "radialGradient" ||
                token.name == "style" || token.name == "metadata" || token.name == "desc" ||
                token.name == "title" || token.name == "symbol" || token.name == "clipPath" ||
                token.name == "mask" || token.name == "filter" || token.name == "pattern")
            {
                continue;
            }

            const Matrix shapeTransform = Multiply(current, ParseTransform(Attribute(attributes, "transform")));

            const auto read = [&](const char* name, float fallback) {
                float value = fallback;
                AttributeNumber(attributes, name, value);
                return value;
            };

            if (token.name == "path")
            {
                pending.emplace_back();
                ParsePathData(Attribute(attributes, "d"), pending.back().contours);
            }
            else if (token.name == "rect")
            {
                const float x = read("x", 0.0f);
                const float y = read("y", 0.0f);
                const float w = read("width", 0.0f);
                const float h = read("height", 0.0f);
                pending.emplace_back();
                AppendPolygon(pending.back().contours, Matrix{},
                              { SvgPoint{ x, y }, SvgPoint{ x + w, y }, SvgPoint{ x + w, y + h },
                                SvgPoint{ x, y + h } });
            }
            else if (token.name == "circle")
            {
                const float cx = read("cx", 0.0f);
                const float cy = read("cy", 0.0f);
                const float r = read("r", 0.0f);
                pending.emplace_back();
                AppendPolygon(pending.back().contours, Matrix{}, EllipsePoints(cx, cy, r, r));
            }
            else if (token.name == "ellipse")
            {
                const float cx = read("cx", 0.0f);
                const float cy = read("cy", 0.0f);
                const float rx = read("rx", 0.0f);
                const float ry = read("ry", 0.0f);
                pending.emplace_back();
                AppendPolygon(pending.back().contours, Matrix{}, EllipsePoints(cx, cy, rx, ry));
            }
            else if (token.name == "polygon" || token.name == "polyline")
            {
                const std::string values = Attribute(attributes, "points");
                std::vector<SvgPoint> polygon;
                std::size_t position = 0;
                while (position < values.size())
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    if (!ReadNumber(values, position, x))
                    {
                        break;
                    }
                    SkipSeparators(values, position);
                    if (!ReadNumber(values, position, y))
                    {
                        break;
                    }
                    polygon.push_back(SvgPoint{ x, y });
                }
                pending.emplace_back();
                AppendPolygon(pending.back().contours, Matrix{}, polygon);
            }
            else
            {
                continue;
            }

            // Transform the shape's contours now; the fill's default is black.
            for (std::vector<SvgPoint>& contour : pending.back().contours)
            {
                for (SvgPoint& point : contour)
                {
                    point = Apply(shapeTransform, point.x, point.y);
                }
            }

            const std::string fill = Attribute(attributes, "fill");
            if (!fill.empty() && fill.size() > 5 && fill.compare(0, 5, "url(#") == 0)
            {
                pending.back().gradientId = fill.substr(5, fill.size() - 6);
            }
            else if (!ParseSvgFill(fill.empty() ? "black" : fill.c_str(), pending.back().paint))
            {
                // `none` or an unreadable colour: the shape paints nothing, which
                // is what SVG says a missing fill means for `none`.
                pending.back().contours.clear();
            }

            float opacity = 1.0f;
            if (AttributeNumber(attributes, "fill-opacity", opacity))
            {
                pending.back().paint.alpha *= opacity;
            }
        }

        // Resolve gradients, drop empty shapes, and flip to font units (y up).
        for (PendingShape& shape : pending)
        {
            if (shape.contours.empty())
            {
                continue;
            }

            if (!shape.gradientId.empty())
            {
                const auto found = gradients.find(shape.gradientId);
                if (found == gradients.end())
                {
                    continue;
                }

                float bx0 = 0.0f;
                float by0 = 0.0f;
                float bx1 = 0.0f;
                float by1 = 0.0f;
                ComputeBounds(shape.contours, bx0, by0, bx1, by1);
                shape.paint = ResolveGradient(found->second, bx0, by0, bx1, by1);
            }

            SvgShape result;
            result.contours.resize(shape.contours.size());
            for (std::size_t contour = 0; contour < shape.contours.size(); ++contour)
            {
                result.contours[contour].reserve(shape.contours[contour].size());
                for (const SvgPoint& point : shape.contours[contour])
                {
                    // SVG y grows downwards; font units grow upwards.
                    result.contours[contour].push_back(SvgPoint{ point.x, -point.y });
                }
            }

            result.paint = shape.paint;
            result.paint.y0 = -result.paint.y0;
            result.paint.y1 = -result.paint.y1;

            out.push_back(std::move(result));
        }

        return !out.empty();
    }
}
