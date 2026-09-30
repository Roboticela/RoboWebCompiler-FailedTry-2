#include "RoboCSSCompiler/CssValue.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace RoboCSSCompiler
{
    // -----------------------------------------------------------------------
    // Units
    // -----------------------------------------------------------------------

    namespace
    {
        struct UnitRecord
        {
            const char* name;
            CSSUnit unit;
            CSSUnitFamily family;
            CSSUnitContext context;
            /// Multiplier to the canonical absolute unit of the family.
            double scale;
        };

        // One table, so the name, the family, the context and the scale of a
        // unit cannot drift apart.
        const UnitRecord UnitTable[] = {
            { "px", CSSUnit::Px, CSSUnitFamily::Length, CSSUnitContext::Absolute, 1.0 },
            { "cm", CSSUnit::Cm, CSSUnitFamily::Length, CSSUnitContext::Absolute, 96.0 / 2.54 },
            { "mm", CSSUnit::Mm, CSSUnitFamily::Length, CSSUnitContext::Absolute, 96.0 / 25.4 },
            { "q", CSSUnit::Q, CSSUnitFamily::Length, CSSUnitContext::Absolute, 96.0 / 101.6 },
            { "in", CSSUnit::In, CSSUnitFamily::Length, CSSUnitContext::Absolute, 96.0 },
            { "pt", CSSUnit::Pt, CSSUnitFamily::Length, CSSUnitContext::Absolute, 96.0 / 72.0 },
            { "pc", CSSUnit::Pc, CSSUnitFamily::Length, CSSUnitContext::Absolute, 16.0 },

            { "em", CSSUnit::Em, CSSUnitFamily::Length, CSSUnitContext::FontRelative, 1.0 },
            { "rem", CSSUnit::Rem, CSSUnitFamily::Length, CSSUnitContext::RootFontRelative, 1.0 },
            { "ex", CSSUnit::Ex, CSSUnitFamily::Length, CSSUnitContext::FontRelative, 1.0 },
            { "ch", CSSUnit::Ch, CSSUnitFamily::Length, CSSUnitContext::FontRelative, 1.0 },
            { "cap", CSSUnit::Cap, CSSUnitFamily::Length, CSSUnitContext::FontRelative, 1.0 },
            { "ic", CSSUnit::Ic, CSSUnitFamily::Length, CSSUnitContext::FontRelative, 1.0 },
            { "lh", CSSUnit::Lh, CSSUnitFamily::Length, CSSUnitContext::FontRelative, 1.0 },
            { "rlh", CSSUnit::Rlh, CSSUnitFamily::Length, CSSUnitContext::RootFontRelative, 1.0 },

            { "vw", CSSUnit::Vw, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "vh", CSSUnit::Vh, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "vmin", CSSUnit::Vmin, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "vmax", CSSUnit::Vmax, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "vb", CSSUnit::Vb, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "vi", CSSUnit::Vi, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },

            { "svw", CSSUnit::Svw, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "svh", CSSUnit::Svh, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "svmin", CSSUnit::Svmin, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "svmax", CSSUnit::Svmax, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "svb", CSSUnit::Svb, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "svi", CSSUnit::Svi, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },

            { "lvw", CSSUnit::Lvw, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "lvh", CSSUnit::Lvh, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "lvmin", CSSUnit::Lvmin, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "lvmax", CSSUnit::Lvmax, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "lvb", CSSUnit::Lvb, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "lvi", CSSUnit::Lvi, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },

            { "dvw", CSSUnit::Dvw, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "dvh", CSSUnit::Dvh, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "dvmin", CSSUnit::Dvmin, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "dvmax", CSSUnit::Dvmax, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "dvb", CSSUnit::Dvb, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },
            { "dvi", CSSUnit::Dvi, CSSUnitFamily::Length, CSSUnitContext::ViewportRelative, 1.0 },

            { "cqw", CSSUnit::Cqw, CSSUnitFamily::Length, CSSUnitContext::ContainerRelative, 1.0 },
            { "cqh", CSSUnit::Cqh, CSSUnitFamily::Length, CSSUnitContext::ContainerRelative, 1.0 },
            { "cqi", CSSUnit::Cqi, CSSUnitFamily::Length, CSSUnitContext::ContainerRelative, 1.0 },
            { "cqb", CSSUnit::Cqb, CSSUnitFamily::Length, CSSUnitContext::ContainerRelative, 1.0 },
            { "cqmin", CSSUnit::Cqmin, CSSUnitFamily::Length, CSSUnitContext::ContainerRelative, 1.0 },
            { "cqmax", CSSUnit::Cqmax, CSSUnitFamily::Length, CSSUnitContext::ContainerRelative, 1.0 },

            { "deg", CSSUnit::Deg, CSSUnitFamily::Angle, CSSUnitContext::Absolute, 1.0 },
            { "rad", CSSUnit::Rad, CSSUnitFamily::Angle, CSSUnitContext::Absolute, 180.0 / 3.14159265358979323846 },
            { "grad", CSSUnit::Grad, CSSUnitFamily::Angle, CSSUnitContext::Absolute, 0.9 },
            { "turn", CSSUnit::Turn, CSSUnitFamily::Angle, CSSUnitContext::Absolute, 360.0 },

            { "s", CSSUnit::S, CSSUnitFamily::Time, CSSUnitContext::Absolute, 1.0 },
            { "ms", CSSUnit::Ms, CSSUnitFamily::Time, CSSUnitContext::Absolute, 0.001 },

            { "hz", CSSUnit::Hz, CSSUnitFamily::Frequency, CSSUnitContext::Absolute, 1.0 },
            { "khz", CSSUnit::Khz, CSSUnitFamily::Frequency, CSSUnitContext::Absolute, 1000.0 },

            { "dpi", CSSUnit::Dpi, CSSUnitFamily::Resolution, CSSUnitContext::Absolute, 1.0 },
            { "dpcm", CSSUnit::Dpcm, CSSUnitFamily::Resolution, CSSUnitContext::Absolute, 2.54 },
            { "dppx", CSSUnit::Dppx, CSSUnitFamily::Resolution, CSSUnitContext::Absolute, 96.0 },

            { "fr", CSSUnit::Fr, CSSUnitFamily::Flex, CSSUnitContext::Absolute, 1.0 }
        };

        const UnitRecord* FindUnit(CSSUnit unit)
        {
            for (const UnitRecord& record : UnitTable)
            {
                if (record.unit == unit)
                {
                    return &record;
                }
            }

            return nullptr;
        }
    }

    CSSUnitFamily UnitFamily(CSSUnit unit)
    {
        const UnitRecord* record = FindUnit(unit);
        return record != nullptr ? record->family : CSSUnitFamily::None;
    }

    CSSUnitContext UnitContext(CSSUnit unit)
    {
        const UnitRecord* record = FindUnit(unit);
        return record != nullptr ? record->context : CSSUnitContext::Absolute;
    }

    bool IsAbsoluteUnit(CSSUnit unit)
    {
        if (unit == CSSUnit::None)
        {
            return false;
        }

        return UnitContext(unit) == CSSUnitContext::Absolute;
    }

    const char* UnitName(CSSUnit unit)
    {
        const UnitRecord* record = FindUnit(unit);
        return record != nullptr ? record->name : "";
    }

    bool ParseUnit(const std::string& text, CSSUnit& unit)
    {
        for (const UnitRecord& record : UnitTable)
        {
            if (text.size() == std::strlen(record.name) && text == record.name)
            {
                unit = record.unit;
                return true;
            }
        }

        return false;
    }

    double AbsoluteUnitScale(CSSUnit unit)
    {
        const UnitRecord* record = FindUnit(unit);
        return record != nullptr ? record->scale : 1.0;
    }

    // -----------------------------------------------------------------------
    // Colors
    // -----------------------------------------------------------------------

    CSSColor CSSColor::Rgba(float r, float g, float b, float a)
    {
        CSSColor color;
        color.kind = CSSColorKind::Srgba;
        color.red = r;
        color.green = g;
        color.blue = b;
        color.alpha = a;
        return color;
    }

    CSSColor CSSColor::Gray(float value)
    {
        return Rgba(value, value, value);
    }

    CSSColor CSSColor::FromCurrentColor()
    {
        CSSColor color;
        color.kind = CSSColorKind::CurrentColor;
        return color;
    }

    std::uint32_t CSSColor::Packed() const
    {
        const auto channel = [](float value) -> std::uint32_t {
            const float clamped = value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value);
            return static_cast<std::uint32_t>(clamped + 0.5f);
        };

        const std::uint32_t a = kind == CSSColorKind::CurrentColor ? 255u : channel(alpha * 255.0f);
        return (a << 24) | (channel(red) << 16) | (channel(green) << 8) | channel(blue);
    }

    std::string CSSColor::ToString() const
    {
        if (kind == CSSColorKind::CurrentColor)
        {
            return "currentColor";
        }

        char buffer[48];

        if (alpha >= 0.999f)
        {
            std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", static_cast<unsigned>(Packed() >> 16) & 0xFFu,
                          static_cast<unsigned>(Packed() >> 8) & 0xFFu, static_cast<unsigned>(Packed()) & 0xFFu);
        }
        else
        {
            std::snprintf(buffer, sizeof(buffer), "rgba(%u, %u, %u, %.3g)",
                          static_cast<unsigned>(red + 0.5f), static_cast<unsigned>(green + 0.5f),
                          static_cast<unsigned>(blue + 0.5f), static_cast<double>(alpha));
        }

        return buffer;
    }

    namespace
    {
        struct NamedColorRecord
        {
            const char* name;
            std::uint32_t rgb;
        };

        // The named colors of css-color-4, minus the two that are not colors:
        // `transparent` is alpha 0 and `currentColor` is resolved by the cascade.
        const NamedColorRecord NamedColors[] = {
            { "aliceblue", 0xf0f8ff }, { "antiquewhite", 0xfaebd7 }, { "aqua", 0x00ffff },
            { "aquamarine", 0x7fffd4 }, { "azure", 0xf0ffff }, { "beige", 0xf5f5dc },
            { "bisque", 0xffe4c4 }, { "black", 0x000000 }, { "blanchedalmond", 0xffebcd },
            { "blue", 0x0000ff }, { "blueviolet", 0x8a2be2 }, { "brown", 0xa52a2a },
            { "burlywood", 0xdeb887 }, { "cadetblue", 0x5f9ea0 }, { "chartreuse", 0x7fff00 },
            { "chocolate", 0xd2691e }, { "coral", 0xff7f50 }, { "cornflowerblue", 0x6495ed },
            { "cornsilk", 0xfff8dc }, { "crimson", 0xdc143c }, { "cyan", 0x00ffff },
            { "darkblue", 0x00008b }, { "darkcyan", 0x008b8b }, { "darkgoldenrod", 0xb8860b },
            { "darkgray", 0xa9a9a9 }, { "darkgreen", 0x006400 }, { "darkgrey", 0xa9a9a9 },
            { "darkkhaki", 0xbdb76b }, { "darkmagenta", 0x8b008b }, { "darkolivegreen", 0x556b2f },
            { "darkorange", 0xff8c00 }, { "darkorchid", 0x9932cc }, { "darkred", 0x8b0000 },
            { "darksalmon", 0xe9967a }, { "darkseagreen", 0x8fbc8f }, { "darkslateblue", 0x483d8b },
            { "darkslategray", 0x2f4f4f }, { "darkslategrey", 0x2f4f4f }, { "darkturquoise", 0x00ced1 },
            { "darkviolet", 0x9400d3 }, { "deeppink", 0xff1493 }, { "deepskyblue", 0x00bfff },
            { "dimgray", 0x696969 }, { "dimgrey", 0x696969 }, { "dodgerblue", 0x1e90ff },
            { "firebrick", 0xb22222 }, { "floralwhite", 0xfffaf0 }, { "forestgreen", 0x228b22 },
            { "fuchsia", 0xff00ff }, { "gainsboro", 0xdcdcdc }, { "ghostwhite", 0xf8f8ff },
            { "gold", 0xffd700 }, { "goldenrod", 0xdaa520 }, { "gray", 0x808080 },
            { "green", 0x008000 }, { "greenyellow", 0xadff2f }, { "grey", 0x808080 },
            { "honeydew", 0xf0fff0 }, { "hotpink", 0xff69b4 }, { "indianred", 0xcd5c5c },
            { "indigo", 0x4b0082 }, { "ivory", 0xfffff0 }, { "khaki", 0xf0e68c },
            { "lavender", 0xe6e6fa }, { "lavenderblush", 0xfff0f5 }, { "lawngreen", 0x7cfc00 },
            { "lemonchiffon", 0xfffacd }, { "lightblue", 0xadd8e6 }, { "lightcoral", 0xf08080 },
            { "lightcyan", 0xe0ffff }, { "lightgoldenrodyellow", 0xfafad2 }, { "lightgray", 0xd3d3d3 },
            { "lightgreen", 0x90ee90 }, { "lightgrey", 0xd3d3d3 }, { "lightpink", 0xffb6c1 },
            { "lightsalmon", 0xffa07a }, { "lightseagreen", 0x20b2aa }, { "lightskyblue", 0x87cefa },
            { "lightslategray", 0x778899 }, { "lightslategrey", 0x778899 }, { "lightsteelblue", 0xb0c4de },
            { "lightyellow", 0xffffe0 }, { "lime", 0x00ff00 }, { "limegreen", 0x32cd32 },
            { "linen", 0xfaf0e6 }, { "magenta", 0xff00ff }, { "maroon", 0x800000 },
            { "mediumaquamarine", 0x66cdaa }, { "mediumblue", 0x0000cd }, { "mediumorchid", 0xba55d3 },
            { "mediumpurple", 0x9370db }, { "mediumseagreen", 0x3cb371 }, { "mediumslateblue", 0x7b68ee },
            { "mediumspringgreen", 0x00fa9a }, { "mediumturquoise", 0x48d1cc }, { "mediumvioletred", 0xc71585 },
            { "midnightblue", 0x191970 }, { "mintcream", 0xf5fffa }, { "mistyrose", 0xffe4e1 },
            { "moccasin", 0xffe4b5 }, { "navajowhite", 0xffdead }, { "navy", 0x000080 },
            { "oldlace", 0xfdf5e6 }, { "olive", 0x808000 }, { "olivedrab", 0x6b8e23 },
            { "orange", 0xffa500 }, { "orangered", 0xff4500 }, { "orchid", 0xda70d6 },
            { "palegoldenrod", 0xeee8aa }, { "palegreen", 0x98fb98 }, { "paleturquoise", 0xafeeee },
            { "palevioletred", 0xdb7093 }, { "papayawhip", 0xffefd5 }, { "peachpuff", 0xffdab9 },
            { "peru", 0xcd853f }, { "pink", 0xffc0cb }, { "plum", 0xdda0dd },
            { "powderblue", 0xb0e0e6 }, { "purple", 0x800080 }, { "rebeccapurple", 0x663399 },
            { "red", 0xff0000 }, { "rosybrown", 0xbc8f8f }, { "royalblue", 0x4169e1 },
            { "saddlebrown", 0x8b4513 }, { "salmon", 0xfa8072 }, { "sandybrown", 0xf4a460 },
            { "seagreen", 0x2e8b57 }, { "seashell", 0xfff5ee }, { "sienna", 0xa0522d },
            { "silver", 0xc0c0c0 }, { "skyblue", 0x87ceeb }, { "slateblue", 0x6a5acd },
            { "slategray", 0x708090 }, { "slategrey", 0x708090 }, { "snow", 0xfffafa },
            { "springgreen", 0x00ff7f }, { "steelblue", 0x4682b4 }, { "tan", 0xd2b48c },
            { "teal", 0x008080 }, { "thistle", 0xd8bfd8 }, { "tomato", 0xff6347 },
            { "turquoise", 0x40e0d0 }, { "violet", 0xee82ee }, { "wheat", 0xf5deb3 },
            { "white", 0xffffff }, { "whitesmoke", 0xf5f5f5 }, { "yellow", 0xffff00 },
            { "yellowgreen", 0x9acd32 }
        };
    }

    bool ParseNamedColor(const std::string& name, CSSColor& color)
    {
        if (name == "transparent")
        {
            color = CSSColor::Rgba(0.0f, 0.0f, 0.0f, 0.0f);
            return true;
        }

        for (const NamedColorRecord& record : NamedColors)
        {
            if (name.size() == std::strlen(record.name) && name == record.name)
            {
                color = CSSColor::Rgba(static_cast<float>((record.rgb >> 16) & 0xFFu),
                                       static_cast<float>((record.rgb >> 8) & 0xFFu),
                                       static_cast<float>(record.rgb & 0xFFu));
                return true;
            }
        }

        return false;
    }

    namespace
    {
        int HexDigitValue(char character)
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

    bool IsHexColorBody(const std::string& text)
    {
        if (text.size() != 3 && text.size() != 4 && text.size() != 6 && text.size() != 8)
        {
            return false;
        }

        for (char character : text)
        {
            if (HexDigitValue(character) < 0)
            {
                return false;
            }
        }

        return true;
    }

    bool ParseHexColor(const std::string& body, CSSColor& color)
    {
        if (!IsHexColorBody(body))
        {
            return false;
        }

        const auto pair = [&](std::size_t index) -> float {
            return static_cast<float>(HexDigitValue(body[index]) * 16 + HexDigitValue(body[index + 1]));
        };

        if (body.size() == 3 || body.size() == 4)
        {
            const auto nibble = [&](std::size_t index) -> float {
                return static_cast<float>(HexDigitValue(body[index]) * 17);
            };

            color = CSSColor::Rgba(nibble(0), nibble(1), nibble(2), body.size() == 4 ? nibble(3) / 255.0f : 1.0f);
            return true;
        }

        color = CSSColor::Rgba(pair(0), pair(2), pair(4), body.size() == 8 ? pair(6) / 255.0f : 1.0f);
        return true;
    }

    // -----------------------------------------------------------------------
    // calc()
    // -----------------------------------------------------------------------

    const char* ToString(CSSMathOperator op)
    {
        switch (op)
        {
        case CSSMathOperator::Add: return "+";
        case CSSMathOperator::Subtract: return "-";
        case CSSMathOperator::Multiply: return "*";
        case CSSMathOperator::Divide: return "/";
        case CSSMathOperator::Min: return "min";
        case CSSMathOperator::Max: return "max";
        case CSSMathOperator::Clamp: return "clamp";
        case CSSMathOperator::Invalid: break;
        }

        return "?";
    }

    bool CSSMathProgram::operator==(const CSSMathProgram& other) const
    {
        if (steps.size() != other.steps.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            if (steps[index].op != other.steps[index].op || steps[index].value != other.steps[index].value ||
                steps[index].unit != other.steps[index].unit ||
                steps[index].isPercentage != other.steps[index].isPercentage)
            {
                return false;
            }
        }

        return true;
    }

    std::string CSSMathProgram::ToString() const
    {
        std::string text = "calc(";
        bool first = true;

        for (const Step& step : steps)
        {
            if (!first)
            {
                text += " ";
            }
            first = false;

            if (step.op != CSSMathOperator::Invalid)
            {
                // Qualified: this class also has a ToString, and the free one is
                // the operator's name.
                text += RoboCSSCompiler::ToString(step.op);
            }
            else
            {
                char buffer[64];

                if (step.isPercentage)
                {
                    std::snprintf(buffer, sizeof(buffer), "%.6g%%", step.value);
                    text += buffer;
                }
                else if (step.unit == CSSUnit::None)
                {
                    std::snprintf(buffer, sizeof(buffer), "%.6g", step.value);
                    text += buffer;
                }
                else
                {
                    std::snprintf(buffer, sizeof(buffer), "%.6g%s", step.value, UnitName(step.unit));
                    text += buffer;
                }
            }
        }

        text += ")";
        return text;
    }

    // -----------------------------------------------------------------------
    // Values
    // -----------------------------------------------------------------------

    const char* ToString(CSSValueKind kind)
    {
        switch (kind)
        {
        case CSSValueKind::Invalid: return "invalid";
        case CSSValueKind::Keyword: return "keyword";
        case CSSValueKind::Ident: return "ident";
        case CSSValueKind::Number: return "number";
        case CSSValueKind::Percentage: return "percentage";
        case CSSValueKind::Dimension: return "dimension";
        case CSSValueKind::Color: return "color";
        case CSSValueKind::String: return "string";
        case CSSValueKind::Url: return "url";
        case CSSValueKind::Function: return "function";
        case CSSValueKind::List: return "list";
        case CSSValueKind::CommaList: return "comma-list";
        case CSSValueKind::Math: return "math";
        case CSSValueKind::Variable: return "variable";
        }

        return "?";
    }

    CSSValue CSSValue::MakeInvalid()
    {
        CSSValue value;
        value.kind = CSSValueKind::Invalid;
        return value;
    }

    CSSValue CSSValue::MakeKeyword(std::string name)
    {
        CSSValue value;
        value.kind = CSSValueKind::Keyword;
        value.text = std::move(name);
        return value;
    }

    CSSValue CSSValue::MakeIdent(std::string name)
    {
        CSSValue value;
        value.kind = CSSValueKind::Ident;
        value.text = std::move(name);
        return value;
    }

    CSSValue CSSValue::MakeNumber(double number, bool isInteger)
    {
        CSSValue value;
        value.kind = CSSValueKind::Number;
        value.number = number;
        value.isInteger = isInteger;
        return value;
    }

    CSSValue CSSValue::MakePercentage(double number)
    {
        CSSValue value;
        value.kind = CSSValueKind::Percentage;
        value.number = number;
        return value;
    }

    CSSValue CSSValue::MakeDimension(double number, CSSUnit unit)
    {
        CSSValue value;
        value.kind = CSSValueKind::Dimension;
        value.number = number;
        value.unit = unit;
        return value;
    }

    CSSValue CSSValue::MakeColor(const CSSColor& color)
    {
        CSSValue value;
        value.kind = CSSValueKind::Color;
        value.color = color;
        return value;
    }

    CSSValue CSSValue::MakeString(std::string text)
    {
        CSSValue value;
        value.kind = CSSValueKind::String;
        value.text = std::move(text);
        return value;
    }

    CSSValue CSSValue::MakeUrl(std::string text)
    {
        CSSValue value;
        value.kind = CSSValueKind::Url;
        value.text = std::move(text);
        return value;
    }

    bool CSSValue::IsKeyword(const char* name) const
    {
        if (kind != CSSValueKind::Keyword || name == nullptr)
        {
            return false;
        }

        return text == name;
    }

    bool CSSValue::IsAnyKeyword(std::initializer_list<const char*> names) const
    {
        for (const char* name : names)
        {
            if (IsKeyword(name))
            {
                return true;
            }
        }

        return false;
    }

    bool CSSValue::IsIdent(const char* name) const
    {
        if (name == nullptr)
        {
            return false;
        }

        if (kind != CSSValueKind::Keyword && kind != CSSValueKind::Ident)
        {
            return false;
        }

        return text == name;
    }

    bool CSSValue::IsAbsoluteLength(float& out) const
    {
        if (kind == CSSValueKind::Dimension && UnitFamily(unit) == CSSUnitFamily::Length)
        {
            if (!IsAbsoluteUnit(unit))
            {
                return false;
            }

            out = static_cast<float>(number * AbsoluteUnitScale(unit));
            return true;
        }

        if (kind == CSSValueKind::Number && number == 0.0)
        {
            // A unitless zero is a length, which is what `margin: 0` means.
            out = 0.0f;
            return true;
        }

        return false;
    }

    bool CSSValue::IsRuntimeDependent() const
    {
        switch (kind)
        {
        case CSSValueKind::Percentage:
            return number != 0.0;

        case CSSValueKind::Dimension:
            return !IsAbsoluteUnit(unit);

        case CSSValueKind::Math:
            return !math.IsEmpty();

        case CSSValueKind::Variable:
            return true;

        case CSSValueKind::Function:
        case CSSValueKind::List:
        case CSSValueKind::CommaList:
            for (const CSSValue& item : items)
            {
                if (item.IsRuntimeDependent())
                {
                    return true;
                }
            }
            return false;

        default:
            return false;
        }
    }

    bool CSSValue::operator==(const CSSValue& other) const
    {
        if (kind != other.kind || number != other.number || isInteger != other.isInteger || unit != other.unit ||
            text != other.text)
        {
            return false;
        }

        if (kind == CSSValueKind::Color && !(color == other.color))
        {
            return false;
        }

        if (kind == CSSValueKind::Math && !(math == other.math))
        {
            return false;
        }

        if (items.size() != other.items.size() || fallback.size() != other.fallback.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (!(items[index] == other.items[index]))
            {
                return false;
            }
        }

        for (std::size_t index = 0; index < fallback.size(); ++index)
        {
            if (!(fallback[index] == other.fallback[index]))
            {
                return false;
            }
        }

        return true;
    }

    std::string CSSValue::ToString() const
    {
        char buffer[64];

        switch (kind)
        {
        case CSSValueKind::Invalid:
            return "invalid";

        case CSSValueKind::Keyword:
        case CSSValueKind::Ident:
            return text;

        case CSSValueKind::Number:
            std::snprintf(buffer, sizeof(buffer), "%.6g", number);
            return buffer;

        case CSSValueKind::Percentage:
            std::snprintf(buffer, sizeof(buffer), "%.6g%%", number);
            return buffer;

        case CSSValueKind::Dimension:
            std::snprintf(buffer, sizeof(buffer), "%.6g%s", number, UnitName(unit));
            return buffer;

        case CSSValueKind::Color:
            return color.ToString();

        case CSSValueKind::String:
            return "\"" + text + "\"";

        case CSSValueKind::Url:
            return "url(" + text + ")";

        case CSSValueKind::Math:
            return math.ToString();

        case CSSValueKind::Variable:
            return "var(" + text + ")";

        case CSSValueKind::Function:
        {
            std::string result = text + "(";
            for (std::size_t index = 0; index < items.size(); ++index)
            {
                if (index > 0)
                {
                    result += ", ";
                }
                result += items[index].ToString();
            }
            result += ")";
            return result;
        }

        case CSSValueKind::List:
        case CSSValueKind::CommaList:
        {
            const char* separator = kind == CSSValueKind::List ? " " : ", ";
            std::string result;
            for (std::size_t index = 0; index < items.size(); ++index)
            {
                if (index > 0)
                {
                    result += separator;
                }
                result += items[index].ToString();
            }
            return result;
        }
        }

        return "";
    }

    void CSSValue::AppendKey(std::string& out) const
    {
        char buffer[64];

        out += RoboCSSCompiler::ToString(kind);
        out += ':';

        switch (kind)
        {
        case CSSValueKind::Keyword:
        case CSSValueKind::Ident:
        case CSSValueKind::String:
        case CSSValueKind::Url:
        case CSSValueKind::Variable:
            out += text;
            break;

        case CSSValueKind::Number:
            std::snprintf(buffer, sizeof(buffer), "%.9g", number);
            out += buffer;
            break;

        case CSSValueKind::Percentage:
            std::snprintf(buffer, sizeof(buffer), "%.9g", number);
            out += buffer;
            break;

        case CSSValueKind::Dimension:
            std::snprintf(buffer, sizeof(buffer), "%.9g/%s", number, UnitName(unit));
            out += buffer;
            break;

        case CSSValueKind::Color:
            std::snprintf(buffer, sizeof(buffer), "%08x", color.Packed());
            out += buffer;
            break;

        case CSSValueKind::Math:
            out += math.ToString();
            break;

        case CSSValueKind::Function:
            out += text;
            out += '(';
            break;

        default:
            break;
        }

        out += '[';

        for (const CSSValue& item : items)
        {
            item.AppendKey(out);
            out += ',';
        }

        out += ']';

        if (!fallback.empty())
        {
            out += "?";
            for (const CSSValue& item : fallback)
            {
                item.AppendKey(out);
                out += ',';
            }
        }
    }

    std::string JoinValueList(const std::vector<CSSValue>& values)
    {
        std::string result;

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                result += ", ";
            }

            result += values[index].ToString();
        }

        return result;
    }
}
