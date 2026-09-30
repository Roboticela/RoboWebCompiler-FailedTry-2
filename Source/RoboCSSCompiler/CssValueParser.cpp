#include "RoboCSSCompiler/CssValueParser.h"

#include "RoboCSSCompiler/CssSyntax.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace RoboCSSCompiler
{
    namespace
    {
        const double Pi = 3.14159265358979323846;

        std::string LowerCase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](char character) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            });
            return value;
        }

        double Clamp(double value, double minimum, double maximum)
        {
            return value < minimum ? minimum : (value > maximum ? maximum : value);
        }

        /// The top-level component values of a list, with the separators and the
        /// end-of-input marker removed.
        std::vector<const CSSComponentValue*> Significant(const std::vector<CSSComponentValue>& values)
        {
            std::vector<const CSSComponentValue*> result;

            for (const CSSComponentValue& value : values)
            {
                if (value.token.type == CSSTokenType::Whitespace || value.token.type == CSSTokenType::Comma ||
                    value.token.type == CSSTokenType::EndOfFile)
                {
                    continue;
                }

                result.push_back(&value);
            }

            return result;
        }

        /// The significant component values inside a function call.
        std::vector<const CSSComponentValue*> FunctionArguments(const CSSComponentValue& function)
        {
            return Significant(function.children);
        }

        // -------------------------------------------------------------------
        // Color conversion
        // -------------------------------------------------------------------

        double SrgbToLinear(double channel)
        {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }

        double LinearToSrgb(double channel)
        {
            return channel <= 0.0031308 ? channel * 12.92 : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
        }

        CSSColor FromLinearRgb(double red, double green, double blue, double alpha)
        {
            const auto toByte = [](double value) -> float {
                const double clamped = Clamp(value, 0.0, 1.0);
                return static_cast<float>(clamped * 255.0 + 0.5);
            };

            return CSSColor::Rgba(toByte(LinearToSrgb(red)), toByte(LinearToSrgb(green)), toByte(LinearToSrgb(blue)),
                                  static_cast<float>(Clamp(alpha, 0.0, 1.0)));
        }

        /// Oklab to linear sRGB, the conversion the specification publishes.
        CSSColor FromOklab(double lightness, double a, double b, double alpha)
        {
            const double l = lightness + 0.3963377774 * a + 0.2158037573 * b;
            const double m = lightness - 0.1055613458 * a - 0.0638541728 * b;
            const double s = lightness - 0.0894841775 * a - 1.2914855480 * b;

            const double l3 = l * l * l;
            const double m3 = m * m * m;
            const double s3 = s * s * s;

            const double red = 4.0767416621 * l3 - 3.3077115913 * m3 + 0.2309699292 * s3;
            const double green = -1.2684380046 * l3 + 2.6097574011 * m3 - 0.3413193965 * s3;
            const double blue = -0.0041960863 * l3 - 0.7034186147 * m3 + 1.7076147010 * s3;

            return FromLinearRgb(red, green, blue, alpha);
        }

        /// CIE Lab (D50) to sRGB, through XYZ with a Bradford adaptation to D65.
        CSSColor FromLab(double lightness, double a, double b, double alpha)
        {
            // Lab -> XYZ, D50.
            const double fy = (lightness + 16.0) / 116.0;
            const double fx = fy + a / 500.0;
            const double fz = fy - b / 200.0;

            const auto finverse = [](double value) -> double {
                const double cube = value * value * value;
                return cube > 0.008856 ? cube : (value - 16.0 / 116.0) / 7.787;
            };

            const double x50 = 0.96422 * finverse(fx);
            const double y50 = 1.00000 * finverse(fy);
            const double z50 = 0.82521 * finverse(fz);

            // Bradford adaptation, D50 -> D65.
            const double x65 = 0.9555766 * x50 - 0.0230393 * y50 + 0.0631636 * z50;
            const double y65 = -0.0282895 * x50 + 1.0099416 * y50 + 0.0210077 * z50;
            const double z65 = 0.0122982 * x50 - 0.0204830 * y50 + 1.3299098 * z50;

            // XYZ -> linear sRGB.
            const double red = 3.2404542 * x65 - 1.5371385 * y65 - 0.4985314 * z65;
            const double green = -0.9692660 * x65 + 1.8760108 * y65 + 0.0415560 * z65;
            const double blue = 0.0556434 * x65 - 0.2040259 * y65 + 1.0572252 * z65;

            return FromLinearRgb(red, green, blue, alpha);
        }

        CSSColor FromHsl(double hue, double saturation, double lightness, double alpha)
        {
            hue = std::fmod(hue, 360.0);
            if (hue < 0.0)
            {
                hue += 360.0;
            }

            saturation = Clamp(saturation, 0.0, 1.0);
            lightness = Clamp(lightness, 0.0, 1.0);

            const auto channel = [hue](double n) -> double {
                const double k = std::fmod(n + hue / 30.0, 12.0);
                const double a = std::min(std::min(k - 3.0, 9.0 - k), 1.0);
                return std::max(0.0, a);
            };

            const double chroma = (1.0 - std::fabs(2.0 * lightness - 1.0)) * saturation;

            const double red = lightness + chroma * channel(0.0) - chroma * 0.5;
            const double green = lightness + chroma * channel(8.0) - chroma * 0.5;
            const double blue = lightness + chroma * channel(4.0) - chroma * 0.5;

            return CSSColor::Rgba(static_cast<float>(Clamp(red, 0.0, 1.0) * 255.0 + 0.5),
                                  static_cast<float>(Clamp(green, 0.0, 1.0) * 255.0 + 0.5),
                                  static_cast<float>(Clamp(blue, 0.0, 1.0) * 255.0 + 0.5),
                                  static_cast<float>(Clamp(alpha, 0.0, 1.0)));
        }

        CSSColor FromHwb(double hue, double whiteness, double blackness, double alpha)
        {
            whiteness = Clamp(whiteness, 0.0, 1.0);
            blackness = Clamp(blackness, 0.0, 1.0);

            if (whiteness + blackness >= 1.0)
            {
                const double gray = whiteness / (whiteness + blackness);
                return CSSColor::Gray(static_cast<float>(gray * 255.0 + 0.5));
            }

            const CSSColor pure = FromHsl(hue, 1.0, 0.5, 1.0);
            const double factor = 1.0 - whiteness - blackness;

            return CSSColor::Rgba(static_cast<float>((pure.red / 255.0 * factor + whiteness) * 255.0 + 0.5),
                                  static_cast<float>((pure.green / 255.0 * factor + whiteness) * 255.0 + 0.5),
                                  static_cast<float>((pure.blue / 255.0 * factor + whiteness) * 255.0 + 0.5),
                                  static_cast<float>(Clamp(alpha, 0.0, 1.0)));
        }

        /// An angle in degrees from a dimension or a number.
        double AngleDegrees(const CSSComponentValue& value, bool& ok)
        {
            ok = true;

            if (value.token.type == CSSTokenType::Number)
            {
                return value.token.number;
            }

            if (value.token.type != CSSTokenType::Dimension)
            {
                ok = false;
                return 0.0;
            }

            CSSUnit unit = CSSUnit::None;
            if (!ParseUnit(value.token.unit, unit) || UnitFamily(unit) != CSSUnitFamily::Angle)
            {
                // Unitless numbers are degrees; anything else is not an angle.
                if (value.token.unit == "deg")
                {
                    return value.token.number;
                }

                ok = false;
                return 0.0;
            }

            return value.token.number * AbsoluteUnitScale(unit);
        }

        /// A percentage as a fraction, or a number as-is.
        bool NumberOrFraction(const CSSComponentValue& value, double& out, bool percentageIsFraction)
        {
            if (value.token.type == CSSTokenType::Number)
            {
                out = value.token.number;
                return true;
            }

            if (value.token.type == CSSTokenType::Percentage)
            {
                out = percentageIsFraction ? value.token.number / 100.0 : value.token.number;
                return true;
            }

            return false;
        }

        /// System colors are real CSS values, and this runtime has one theme, so
        /// they are reported rather than mapped onto an invented palette.
        bool IsSystemColorName(const std::string& name)
        {
            static const char* names[] = {
                "canvas", "canvastext", "linktext", "visitedtext", "activetext", "buttonface", "buttontext",
                "buttonborder", "field", "fieldtext", "highlight", "highlighttext", "selecteditem",
                "selecteditemtext", "mark", "marktext", "graytext", "accentcolor", "accentcolortext", "window",
                "windowtext", "windowframe", "infobackground", "infotext", "menutext", "scrollbar", "threeddarkshadow",
                "threedface", "threedhighlight", "threedlightshadow", "threedshadow", "activeborder",
                "activecaption", "appworkspace", "background", "buttonhighlight", "buttonshadow", "captiontext",
                "inactiveborder", "inactivecaption", "inactivecaptiontext"
            };

            for (const char* candidate : names)
            {
                if (name == candidate)
                {
                    return true;
                }
            }

            return false;
        }
    }

    std::string ComponentsToText(const std::vector<CSSComponentValue>& components)
    {
        std::string text;

        for (const CSSComponentValue& value : components)
        {
            if (value.token.type == CSSTokenType::Whitespace)
            {
                if (!text.empty() && text.back() != ' ')
                {
                    text += ' ';
                }
                continue;
            }

            if (!value.token.raw.empty())
            {
                text += value.token.raw;
            }
            else
            {
                text += value.token.text;
            }

            if (!value.children.empty())
            {
                text += ComponentsToText(value.children);
            }

            if (value.isFunction)
            {
                text += ")";
            }
            else if (value.block == CSSBlockKind::Square)
            {
                text += "]";
            }
            else if (value.block == CSSBlockKind::Curly)
            {
                text += "}";
            }
        }

        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }

        return text;
    }

    // -----------------------------------------------------------------------
    // var()
    // -----------------------------------------------------------------------

    bool SubstituteVariables(const std::vector<CSSComponentValue>& input,
                             const CSSCustomPropertyLookup& lookup,
                             std::vector<CSSComponentValue>& output,
                             CSSDiagnosticSink& diagnostics,
                             const CSSSourceLocation& location,
                             std::string& unresolvedName,
                             int depth)
    {
        if (depth > 16)
        {
            diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                              location,
                              "var() references are nested too deeply.",
                              "the substitution chain is longer than 16 links, which is a cycle",
                              "Break the cycle between the custom properties.",
                              "var()");
            return false;
        }

        for (const CSSComponentValue& value : input)
        {
            if (value.isFunction && LowerCase(value.token.text) == "var")
            {
                const std::vector<const CSSComponentValue*> arguments = Significant(value.children);

                if (arguments.empty() || arguments[0]->token.type != CSSTokenType::Ident ||
                    arguments[0]->token.text.rfind("--", 0) != 0)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                                      value.location,
                                      "var() must name a custom property.",
                                      "expected 'var(--name)'",
                                      "Write the custom property's name, including the two leading dashes.",
                                      "var()");
                    return false;
                }

                const std::string name = arguments[0]->token.text;
                const std::vector<CSSComponentValue>* replacement =
                    lookup != nullptr ? lookup(name) : nullptr;

                if (replacement != nullptr)
                {
                    // Substitution is recursive: a custom property may itself
                    // reference another one.
                    if (!SubstituteVariables(*replacement, lookup, output, diagnostics, location, unresolvedName,
                                             depth + 1))
                    {
                        return false;
                    }

                    continue;
                }

                // The fallback, if the reference wrote one.
                if (arguments.size() > 1)
                {
                    std::vector<CSSComponentValue> fallback;
                    std::size_t start = 0;

                    for (std::size_t index = 0; index < value.children.size(); ++index)
                    {
                        if (value.children[index].token.type == CSSTokenType::Comma)
                        {
                            start = index + 1;
                            break;
                        }
                    }

                    for (std::size_t index = start; index < value.children.size(); ++index)
                    {
                        fallback.push_back(value.children[index]);
                    }

                    if (!SubstituteVariables(fallback, lookup, output, diagnostics, location, unresolvedName,
                                             depth + 1))
                    {
                        return false;
                    }

                    continue;
                }

                unresolvedName = name;
                return false;
            }

            output.push_back(value);
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // Colors
    // -----------------------------------------------------------------------

    namespace
    {
        bool ParseColorFunction(const CSSComponentValue& function,
                                CSSColor& out,
                                const CSSSourceLocation& location,
                                CSSDiagnosticSink& diagnostics)
        {
            const std::string name = LowerCase(function.token.text);
            const std::vector<const CSSComponentValue*> arguments = FunctionArguments(function);

            if (arguments.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  name + "() has no arguments.",
                                  "expected a color's components",
                                  "Write the components, for example 'rgb(0 0 0)'.",
                                  name + "()");
                return false;
            }

            // Split on commas when the legacy syntax is used, otherwise take the
            // arguments in order and let `/` separate the alpha.
            std::vector<const CSSComponentValue*> components;
            const CSSComponentValue* alphaValue = nullptr;

            for (std::size_t index = 0; index < function.children.size(); ++index)
            {
                const CSSComponentValue& child = function.children[index];

                if (child.token.type == CSSTokenType::Whitespace)
                {
                    continue;
                }

                if (child.token.type == CSSTokenType::Comma)
                {
                    continue;
                }

                if (child.token.IsDelim('/'))
                {
                    if (index + 1 < function.children.size())
                    {
                        for (std::size_t ahead = index + 1; ahead < function.children.size(); ++ahead)
                        {
                            if (function.children[ahead].token.type != CSSTokenType::Whitespace)
                            {
                                alphaValue = &function.children[ahead];
                                break;
                            }
                        }
                    }
                    break;
                }

                components.push_back(&child);
            }

            double alpha = 1.0;

            if (alphaValue != nullptr)
            {
                if (!NumberOrFraction(*alphaValue, alpha, true))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "A color's alpha must be a number or a percentage.",
                                      std::string("found '") + alphaValue->token.raw + "'",
                                      "Write an alpha between 0 and 1, or a percentage.",
                                      name + "()");
                    return false;
                }
            }

            const auto channel = [](const CSSComponentValue& value, double& out) {
                if (value.token.type == CSSTokenType::Number)
                {
                    out = value.token.number;
                    return true;
                }

                if (value.token.type == CSSTokenType::Percentage)
                {
                    out = value.token.number * 255.0 / 100.0;
                    return true;
                }

                if (value.token.type == CSSTokenType::Dimension && value.token.unit == "none")
                {
                    out = 0.0;
                    return true;
                }

                return false;
            };

            const auto fraction = [](const CSSComponentValue& value, double& out) {
                if (value.token.type == CSSTokenType::Number)
                {
                    out = value.token.number;
                    return true;
                }

                if (value.token.type == CSSTokenType::Percentage)
                {
                    out = value.token.number / 100.0;
                    return true;
                }

                return false;
            };

            if (name == "rgb" || name == "rgba")
            {
                if (components.size() < 3)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "rgb() needs three colour components.",
                                      std::to_string(components.size()) + " given",
                                      "Write 'rgb(r g b)' or 'rgb(r g b / a)'.",
                                      name + "()");
                    return false;
                }

                double red = 0.0;
                double green = 0.0;
                double blue = 0.0;

                if (!channel(*components[0], red) || !channel(*components[1], green) ||
                    !channel(*components[2], blue))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "rgb() components must be numbers or percentages.",
                                      ComponentsToText(function.children),
                                      "Write 'rgb(0 0 0)' or 'rgb(0% 0% 0%)'.",
                                      name + "()");
                    return false;
                }

                // A legacy rgba() call carries its fourth argument as the alpha.
                if (alphaValue == nullptr && components.size() > 3)
                {
                    if (!NumberOrFraction(*components[3], alpha, true))
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                          "A color's alpha must be a number or a percentage.",
                                          ComponentsToText(function.children),
                                          "Write 'rgba(0, 0, 0, 0.5)'.", name + "()");
                        return false;
                    }
                }

                out = CSSColor::Rgba(static_cast<float>(Clamp(red, 0.0, 255.0)),
                                     static_cast<float>(Clamp(green, 0.0, 255.0)),
                                     static_cast<float>(Clamp(blue, 0.0, 255.0)),
                                     static_cast<float>(Clamp(alpha, 0.0, 1.0)));
                return true;
            }

            if (name == "hsl" || name == "hsla")
            {
                if (components.size() < 3)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "hsl() needs three components.",
                                      std::to_string(components.size()) + " given",
                                      "Write 'hsl(0 0% 0%)'.", name + "()");
                    return false;
                }

                bool angleOk = false;
                const double hue = AngleDegrees(*components[0], angleOk);
                double saturation = 0.0;
                double lightness = 0.0;

                if (!angleOk || !fraction(*components[1], saturation) || !fraction(*components[2], lightness))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "hsl() components are a hue, a saturation and a lightness.",
                                      ComponentsToText(function.children),
                                      "Write 'hsl(210 50% 40%)'.", name + "()");
                    return false;
                }

                if (alphaValue == nullptr && components.size() > 3)
                {
                    NumberOrFraction(*components[3], alpha, true);
                }

                out = FromHsl(hue, saturation, lightness, alpha);
                return true;
            }

            if (name == "hwb")
            {
                if (components.size() < 3)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location, "hwb() needs three components.",
                                      std::to_string(components.size()) + " given",
                                      "Write 'hwb(210 20% 30%)'.", "hwb()");
                    return false;
                }

                bool angleOk = false;
                const double hue = AngleDegrees(*components[0], angleOk);
                double whiteness = 0.0;
                double blackness = 0.0;

                if (!angleOk || !fraction(*components[1], whiteness) || !fraction(*components[2], blackness))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "hwb() components are a hue, a whiteness and a blackness.",
                                      ComponentsToText(function.children), "Write 'hwb(210 20% 30%)'.", "hwb()");
                    return false;
                }

                out = FromHwb(hue, whiteness, blackness, alpha);
                return true;
            }

            if (name == "lab" || name == "oklab")
            {
                if (components.size() < 3)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location, name + "() needs three components.",
                                      std::to_string(components.size()) + " given",
                                      "Write '" + name + "(50% 0 0)'.", name + "()");
                    return false;
                }

                double lightness = 0.0;
                double a = 0.0;
                double b = 0.0;

                // A lightness percentage is 0..100 for lab() and 0..1 for oklab().
                const bool lightnessOk = NumberOrFraction(*components[0], lightness, name == "oklab");

                if (!lightnessOk || !NumberOrFraction(*components[1], a, false) ||
                    !NumberOrFraction(*components[2], b, false))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      name + "() components are a lightness and two axes.",
                                      ComponentsToText(function.children),
                                      "Write '" + name + "(50% 0.1 -0.1)'.", name + "()");
                    return false;
                }

                out = name == "oklab" ? FromOklab(lightness, a, b, alpha) : FromLab(lightness, a, b, alpha);
                return true;
            }

            if (name == "lch" || name == "oklch")
            {
                if (components.size() < 3)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location, name + "() needs three components.",
                                      std::to_string(components.size()) + " given",
                                      "Write '" + name + "(50% 0.1 120)'.", name + "()");
                    return false;
                }

                double lightness = 0.0;
                double chroma = 0.0;
                bool angleOk = false;
                const double hue = AngleDegrees(*components[2], angleOk);

                // A lightness percentage is 0..100 for lch() and 0..1 for oklch().
                if (!NumberOrFraction(*components[0], lightness, name == "oklch") ||
                    !NumberOrFraction(*components[1], chroma, false) || !angleOk)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      name + "() components are a lightness, a chroma and a hue.",
                                      ComponentsToText(function.children),
                                      "Write '" + name + "(50% 0.1 120)'.", name + "()");
                    return false;
                }

                const double radians = hue * Pi / 180.0;
                const double a = chroma * std::cos(radians);
                const double b = chroma * std::sin(radians);

                out = name == "oklch" ? FromOklab(lightness, a, b, alpha) : FromLab(lightness, a, b, alpha);
                return true;
            }

            if (name == "color")
            {
                if (components.size() < 4 || components[0]->token.type != CSSTokenType::Ident)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "color() needs a colour space and three components.",
                                      ComponentsToText(function.children),
                                      "Write 'color(srgb 1 0 0)'.",
                                      "color()");
                    return false;
                }

                const std::string space = LowerCase(components[0]->token.text);
                double red = 0.0;
                double green = 0.0;
                double blue = 0.0;

                if (!NumberOrFraction(*components[1], red, false) ||
                    !NumberOrFraction(*components[2], green, false) ||
                    !NumberOrFraction(*components[3], blue, false))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "color() components must be numbers or percentages.",
                                      ComponentsToText(function.children), "Write 'color(srgb 1 0 0)'.",
                                      "color()");
                    return false;
                }

                if (space == "srgb")
                {
                    out = CSSColor::Rgba(static_cast<float>(Clamp(red, 0.0, 1.0) * 255.0 + 0.5),
                                         static_cast<float>(Clamp(green, 0.0, 1.0) * 255.0 + 0.5),
                                         static_cast<float>(Clamp(blue, 0.0, 1.0) * 255.0 + 0.5),
                                         static_cast<float>(Clamp(alpha, 0.0, 1.0)));
                    return true;
                }

                if (space == "srgb-linear")
                {
                    out = FromLinearRgb(red, green, blue, alpha);
                    return true;
                }

                if (space == "display-p3")
                {
                    // P3 -> linear sRGB, then to sRGB.
                    const double rl = SrgbToLinear(Clamp(red, 0.0, 1.0));
                    const double gl = SrgbToLinear(Clamp(green, 0.0, 1.0));
                    const double bl = SrgbToLinear(Clamp(blue, 0.0, 1.0));

                    const double x = 1.22494 * rl - 0.22494 * gl;
                    const double y = -0.04206 * rl + 1.04206 * gl;
                    const double z = -0.01964 * rl - 0.07865 * gl + 1.09828 * bl;

                    out = FromLinearRgb(x, y, z, alpha);
                    return true;
                }

                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "color() with an unsupported colour space: " + space + ".",
                                    "this compiler resolves srgb, srgb-linear and display-p3",
                                    "Use rgb(), hsl() or a named color.",
                                    "color(" + space + ")");
                return false;
            }

            diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                location,
                                "Unknown color function: " + name + "().",
                                "the CSS reference does not define this function",
                                "Use rgb(), hsl(), hwb(), lab(), lch(), oklab(), oklch() or color().",
                                name + "()");
            return false;
        }
    }

    bool ParseColorComponents(const std::vector<CSSComponentValue>& components,
                              CSSColor& out,
                              const CSSSourceLocation& location,
                              CSSDiagnosticSink& diagnostics,
                              std::string& subject)
    {
        const std::vector<const CSSComponentValue*> values = Significant(components);

        if (values.size() != 1)
        {
            subject = ComponentsToText(components);
            diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                              location,
                              "A color is a single value.",
                              std::to_string(values.size()) + " values given",
                              "Write one color, for example 'red' or '#2563eb'.",
                              subject);
            return false;
        }

        const CSSComponentValue& value = *values[0];
        subject = value.token.raw.empty() ? value.token.text : value.token.raw;

        if (value.token.type == CSSTokenType::Hash)
        {
            if (ParseHexColor(value.token.text, out))
            {
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Malformed hex color: #" + value.token.text + ".",
                              "a hex color has 3, 4, 6 or 8 hexadecimal digits",
                              "Write '#rrggbb' or '#rgb'.",
                              subject);
            return false;
        }

        if (value.token.type == CSSTokenType::Ident)
        {
            const std::string name = LowerCase(value.token.text);

            if (name == "currentcolor")
            {
                out = CSSColor::FromCurrentColor();
                return true;
            }

            if (IsSystemColorName(name))
            {
                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "System color '" + name + "' is not implemented.",
                                    "a native runtime has one theme and the compiler does not invent a palette",
                                    "Use an explicit color, or leave the property unspecified to keep the "
                                    "native default.",
                                    name);
                return false;
            }

            if (ParseNamedColor(name, out))
            {
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Unknown color: '" + value.token.text + "'.",
                              "not a named color, a system color, or a function",
                              "Use a named color, a hex color, or rgb()/hsl().",
                              subject);
            return false;
        }

        if (value.isFunction)
        {
            return ParseColorFunction(value, out, location, diagnostics);
        }

        diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                          location,
                          "Expected a color.",
                          std::string("found ") + ToString(value.token.type) + " '" + value.token.raw + "'",
                          "Write a color, for example 'rgb(0 0 0)'.",
                          subject);
        return false;
    }

    // -----------------------------------------------------------------------
    // calc()
    // -----------------------------------------------------------------------

    namespace
    {
        /// Evaluates a math program whose operands are all absolute. Returns
        /// false when an operand needs a runtime context.
        bool EvaluateAbsoluteProgram(const CSSMathProgram& program, double& out, bool& isLength, CSSUnit& unit)
        {
            double stack[4] = { 0.0, 0.0, 0.0, 0.0 };
            bool lengthStack[4] = { false, false, false, false };
            CSSUnit unitStack[4] = { CSSUnit::None, CSSUnit::None, CSSUnit::None, CSSUnit::None };
            int depth = 0;

            for (const CSSMathProgram::Step& step : program.steps)
            {
                if (step.op == CSSMathOperator::Invalid)
                {
                    if (step.isPercentage)
                    {
                        return false;
                    }

                    if (step.unit != CSSUnit::None)
                    {
                        if (!IsAbsoluteUnit(step.unit))
                        {
                            return false;
                        }

                        if (depth >= 4)
                        {
                            return false;
                        }

                        stack[depth] = step.value * AbsoluteUnitScale(step.unit);
                        lengthStack[depth] = true;
                        unitStack[depth] = step.unit;
                        ++depth;
                        continue;
                    }

                    if (depth >= 4)
                    {
                        return false;
                    }

                    stack[depth] = step.value;
                    lengthStack[depth] = false;
                    unitStack[depth] = CSSUnit::None;
                    ++depth;
                    continue;
                }

                if (step.op == CSSMathOperator::Min || step.op == CSSMathOperator::Max ||
                    step.op == CSSMathOperator::Clamp)
                {
                    const int count = step.op == CSSMathOperator::Clamp ? 3 : 2;
                    if (depth < count)
                    {
                        return false;
                    }

                    const double first = stack[depth - count];

                    if (step.op == CSSMathOperator::Clamp)
                    {
                        const double lower = first;
                        const double value = stack[depth - 2];
                        const double upper = stack[depth - 1];
                        stack[depth - count] = Clamp(value, lower, upper);
                    }
                    else
                    {
                        const double second = stack[depth - 1];
                        stack[depth - count] = step.op == CSSMathOperator::Min ? std::min(first, second)
                                                                               : std::max(first, second);
                    }

                    depth -= count - 1;
                    continue;
                }

                if (depth < 2)
                {
                    return false;
                }

                const double left = stack[depth - 2];
                const double right = stack[depth - 1];
                const bool leftLength = lengthStack[depth - 2];
                const bool rightLength = lengthStack[depth - 1];

                double result = 0.0;

                switch (step.op)
                {
                case CSSMathOperator::Add:
                case CSSMathOperator::Subtract:
                    if (leftLength != rightLength)
                    {
                        return false;
                    }

                    result = step.op == CSSMathOperator::Add ? left + right : left - right;
                    break;

                case CSSMathOperator::Multiply:
                    result = left * right;
                    break;

                case CSSMathOperator::Divide:
                    if (right == 0.0)
                    {
                        return false;
                    }
                    result = left / right;
                    break;

                default:
                    return false;
                }

                stack[depth - 2] = result;
                lengthStack[depth - 2] = leftLength || rightLength;
                unitStack[depth - 2] = leftLength ? unitStack[depth - 2] : unitStack[depth - 1];
                --depth;
            }

            if (depth != 1)
            {
                return false;
            }

            out = stack[0];
            isLength = lengthStack[0];
            unit = unitStack[0];
            return true;
        }

        /// Parses an operand of a math function.
        bool ParseMathOperand(const CSSComponentValue& value,
                              CSSMathProgram& program,
                              CSSDiagnosticSink& diagnostics,
                              const CSSSourceLocation& location);

        /// Builds the program for a parenthesised sub-expression.
        bool ParseMathExpression(const CSSComponentValue& value,
                                 CSSMathProgram& program,
                                 CSSDiagnosticSink& diagnostics,
                                 const CSSSourceLocation& location)
        {
            const std::vector<const CSSComponentValue*> items = FunctionArguments(value);

            if (items.empty())
            {
                return false;
            }

            // An operand, an operator, an operand, ... evaluated left to right,
            // with `*` and `/` binding tighter than `+` and `-`. The program is
            // built in reverse polish order, so precedence is resolved here and
            // nowhere else.
            struct Term
            {
                CSSMathProgram program;
                CSSMathOperator pendingOp = CSSMathOperator::Invalid;
            };

            std::vector<Term> sums;
            CSSMathOperator pending = CSSMathOperator::Invalid;

            for (const CSSComponentValue* item : items)
            {
                if (item->token.type == CSSTokenType::Comma)
                {
                    // `min(a, b)` and `clamp(a, b, c)` are separate arguments;
                    // they are combined by the caller.
                    continue;
                }

                if (item->token.type == CSSTokenType::Delim)
                {
                    if (item->token.text == "+")
                    {
                        pending = CSSMathOperator::Add;
                        continue;
                    }
                    if (item->token.text == "-")
                    {
                        pending = CSSMathOperator::Subtract;
                        continue;
                    }
                    if (item->token.text == "*")
                    {
                        pending = CSSMathOperator::Multiply;
                        continue;
                    }
                    if (item->token.text == "/")
                    {
                        pending = CSSMathOperator::Divide;
                        continue;
                    }

                    continue;
                }

                CSSMathProgram operand;
                if (!ParseMathOperand(*item, operand, diagnostics, location))
                {
                    return false;
                }

                if (!sums.empty() && pending != CSSMathOperator::Invalid &&
                    (pending == CSSMathOperator::Multiply || pending == CSSMathOperator::Divide))
                {
                    // Fold into the previous term.
                    CSSMathProgram combined = sums.back().program;
                    for (const CSSMathProgram::Step& step : operand.steps)
                    {
                        combined.steps.push_back(step);
                    }

                    CSSMathProgram::Step operatorStep;
                    operatorStep.op = pending;
                    combined.steps.push_back(operatorStep);

                    sums.back().program = std::move(combined);
                    pending = CSSMathOperator::Invalid;
                    continue;
                }

                Term term;
                term.program = std::move(operand);
                term.pendingOp = pending;
                pending = CSSMathOperator::Invalid;
                sums.push_back(std::move(term));
            }

            if (sums.empty())
            {
                return false;
            }

            program = sums.front().program;

            for (std::size_t index = 1; index < sums.size(); ++index)
            {
                for (const CSSMathProgram::Step& step : sums[index].program.steps)
                {
                    program.steps.push_back(step);
                }

                CSSMathProgram::Step operatorStep;
                operatorStep.op = sums[index].pendingOp == CSSMathOperator::Invalid ? CSSMathOperator::Add
                                                                                    : sums[index].pendingOp;
                program.steps.push_back(operatorStep);
            }

            return true;
        }

        bool ParseMathOperand(const CSSComponentValue& value,
                              CSSMathProgram& program,
                              CSSDiagnosticSink& diagnostics,
                              const CSSSourceLocation& location)
        {
            if (value.token.type == CSSTokenType::Number)
            {
                CSSMathProgram::Step step;
                step.value = value.token.number;
                program.steps.push_back(step);
                return true;
            }

            if (value.token.type == CSSTokenType::Percentage)
            {
                CSSMathProgram::Step step;
                step.value = value.token.number;
                step.isPercentage = true;
                program.steps.push_back(step);
                return true;
            }

            if (value.token.type == CSSTokenType::Dimension)
            {
                CSSUnit unit = CSSUnit::None;

                if (!ParseUnit(value.token.unit, unit) || UnitFamily(unit) != CSSUnitFamily::Length)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidUnit,
                                      location,
                                      "Unknown unit in a math expression: " + value.token.unit + ".",
                                      std::string("'") + value.token.raw + "'",
                                      "Use a length unit.",
                                      value.token.unit);
                    return false;
                }

                CSSMathProgram::Step step;
                step.value = value.token.number;
                step.unit = unit;
                program.steps.push_back(step);
                return true;
            }

            if (value.token.type == CSSTokenType::LeftParen)
            {
                return ParseMathExpression(value, program, diagnostics, location);
            }

            if (value.isFunction)
            {
                const std::string name = LowerCase(value.token.text);

                // A nested calc() is just its expression.
                if (name == "calc")
                {
                    return ParseMathExpression(value, program, diagnostics, location);
                }
            }

            // A var() has already been substituted by this point; anything else
            // is not an operand.
            diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                              location,
                              "Not a valid math operand.",
                              value.token.raw.empty() ? value.token.text : value.token.raw,
                              "Use a number, a length or a percentage.",
                              "calc()");
            return false;
        }
    }

    bool ParseMathFunction(const CSSComponentValue& functionValue,
                           CSSValue& out,
                           const CSSSourceLocation& location,
                           CSSDiagnosticSink& diagnostics)
    {
        const std::string name = LowerCase(functionValue.token.text);

        if (name == "calc")
        {
            CSSMathProgram program;

            if (!ParseMathExpression(functionValue, program, diagnostics, location))
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                                  location,
                                  "calc() could not be evaluated.",
                                  ComponentsToText(functionValue.children),
                                  "Check the operands and the operators around them.",
                                  "calc()");
                return false;
            }

            double value = 0.0;
            bool isLength = false;
            CSSUnit unit = CSSUnit::None;

            if (EvaluateAbsoluteProgram(program, value, isLength, unit))
            {
                out = isLength ? CSSValue::MakeDimension(value, unit) : CSSValue::MakeNumber(value);
                return true;
            }

            // A program that mixes a relative operand with an absolute one has
            // two dimensions, and a compiled native length has one.
            bool hasAbsolute = false;
            bool hasRelative = false;

            for (const CSSMathProgram::Step& step : program.steps)
            {
                if (step.op != CSSMathOperator::Invalid)
                {
                    continue;
                }

                if (step.isPercentage || (step.unit != CSSUnit::None && !IsAbsoluteUnit(step.unit)))
                {
                    hasRelative = true;
                }
                else
                {
                    hasAbsolute = true;
                }
            }

            if (hasAbsolute && hasRelative)
            {
                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "calc() mixes a relative and an absolute length.",
                                    "a compiled native length carries one dimension, so this expression cannot be "
                                    "resolved by the runtime",
                                    "Split the declaration, or use a single unit.",
                                    "calc(" + ComponentsToText(functionValue.children) + ")");
                return false;
            }

            if (program.steps.size() == 1)
            {
                const CSSMathProgram::Step& step = program.steps.front();

                if (step.isPercentage)
                {
                    out = CSSValue::MakePercentage(step.value);
                    return true;
                }

                if (step.unit != CSSUnit::None)
                {
                    out = CSSValue::MakeDimension(step.value, step.unit);
                    return true;
                }

                out = CSSValue::MakeNumber(step.value);
                return true;
            }

            out.kind = CSSValueKind::Math;
            out.math = program;
            return true;
        }

        if (name == "min" || name == "max" || name == "clamp")
        {
            const std::vector<const CSSComponentValue*> arguments = FunctionArguments(functionValue);

            // The arguments are separated by commas; build one program per
            // argument and reduce them with the operator.
            std::vector<CSSMathProgram> programs;
            CSSMathProgram current;

            for (const CSSComponentValue& child : functionValue.children)
            {
                if (child.token.type == CSSTokenType::Whitespace)
                {
                    continue;
                }

                if (child.token.type == CSSTokenType::Comma)
                {
                    if (!current.steps.empty())
                    {
                        programs.push_back(current);
                        current.steps.clear();
                    }
                    continue;
                }

                if (!ParseMathOperand(child, current, diagnostics, location))
                {
                    return false;
                }
            }

            if (!current.steps.empty())
            {
                programs.push_back(current);
            }

            const std::size_t required = name == "clamp" ? 3 : 2;

            if (programs.size() < required)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                                  location,
                                  name + "() needs " + std::to_string(required) + " arguments.",
                                  std::to_string(programs.size()) + " given",
                                  "Write the arguments separated by commas.",
                                  name + "()");
                return false;
            }

            CSSMathProgram combined = programs.front();

            for (std::size_t index = 1; index < programs.size(); ++index)
            {
                for (const CSSMathProgram::Step& step : programs[index].steps)
                {
                    combined.steps.push_back(step);
                }

                CSSMathProgram::Step operatorStep;
                operatorStep.op = name == "min" ? CSSMathOperator::Min
                                                : (name == "max" ? CSSMathOperator::Max : CSSMathOperator::Clamp);
                combined.steps.push_back(operatorStep);
            }

            double value = 0.0;
            bool isLength = false;
            CSSUnit unit = CSSUnit::None;

            if (EvaluateAbsoluteProgram(combined, value, isLength, unit))
            {
                out = isLength ? CSSValue::MakeDimension(value, unit) : CSSValue::MakeNumber(value);
                return true;
            }

            if (combined.steps.size() == 1)
            {
                const CSSMathProgram::Step& step = combined.steps.front();

                if (step.isPercentage)
                {
                    out = CSSValue::MakePercentage(step.value);
                    return true;
                }

                if (step.unit != CSSUnit::None)
                {
                    out = CSSValue::MakeDimension(step.value, step.unit);
                    return true;
                }
            }

            diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                location,
                                name + "() has no compile-time value.",
                                "the operands mix relative and absolute lengths",
                                "Use one unit.",
                                name + "()");
            return false;
        }

        diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                            location,
                            "Unknown math function: " + name + "().",
                            "the reference defines calc(), min(), max() and clamp()",
                            "Use one of those.",
                            name + "()");
        return false;
    }

    // -----------------------------------------------------------------------
    // Property grammars
    // -----------------------------------------------------------------------

    namespace
    {
        bool ParseLengthPercentage(const std::vector<CSSComponentValue>& components,
                                   CSSValue& out,
                                   const CSSSourceLocation& location,
                                   CSSDiagnosticSink& diagnostics,
                                   bool allowAuto,
                                   bool allowNumberZero,
                                   const char* subject);

        bool ParseSizeValue(const std::vector<CSSComponentValue>& components,
                            CSSValue& out,
                            const CSSSourceLocation& location,
                            CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "A size is a single value.",
                                  std::to_string(values.size()) + " values given",
                                  "Write 'auto', 'none' or one length.",
                                  ComponentsToText(components));
                return false;
            }

            const CSSComponentValue& value = *values[0];

            if (value.token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(value.token.text);

                if (name == "auto" || name == "none" || name == "min-content" || name == "max-content")
                {
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }

                if (name == "fit-content")
                {
                    out = CSSValue::MakeKeyword("fit-content");
                    return true;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "Unknown size keyword: '" + value.token.text + "'.",
                                  "a size is auto, none, min-content, max-content, fit-content() or a length",
                                  "Use one of those.",
                                  value.token.text);
                return false;
            }

            if (value.isFunction && LowerCase(value.token.text) == "fit-content")
            {
                const std::vector<const CSSComponentValue*> arguments = FunctionArguments(value);

                if (arguments.size() != 1)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                                      location,
                                      "fit-content() takes one argument.",
                                      std::to_string(arguments.size()) + " given",
                                      "Write 'fit-content(10rem)'.",
                                      "fit-content()");
                    return false;
                }

                CSSValue argument;

                if (!ParseSizeValue(std::vector<CSSComponentValue>{ *arguments[0] }, argument, location, diagnostics))
                {
                    return false;
                }

                out = CSSValue::MakeKeyword("fit-content");
                out.items.push_back(argument);
                return true;
            }

            return ParseLengthPercentage(components, out, location, diagnostics, false, true, "size");
        }

        bool ParseLengthPercentage(const std::vector<CSSComponentValue>& components,
                                   CSSValue& out,
                                   const CSSSourceLocation& location,
                                   CSSDiagnosticSink& diagnostics,
                                   bool allowAuto,
                                   bool allowNumberZero,
                                   const char* subject)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "Expected a single length.",
                                  std::to_string(values.size()) + " values given",
                                  "Write one length, size or percentage.",
                                  subject);
                return false;
            }

            const CSSComponentValue& value = *values[0];

            if (value.token.type == CSSTokenType::Percentage)
            {
                out = CSSValue::MakePercentage(value.token.number);
                return true;
            }

            if (value.token.type == CSSTokenType::Dimension)
            {
                CSSUnit unit = CSSUnit::None;

                if (!ParseUnit(value.token.unit, unit) || UnitFamily(unit) != CSSUnitFamily::Length)
                {
                    // A math function's result, or a unit that does not exist.
                    diagnostics.Error(CSSDiagnosticCode::InvalidUnit,
                                      location,
                                      "Unknown unit: '" + value.token.unit + "'.",
                                      std::string("in '") + value.token.raw + "'",
                                      "Use a length unit such as px, rem, em, vw or vh.",
                                      value.token.unit);
                    return false;
                }

                out = CSSValue::MakeDimension(value.token.number, unit);
                return true;
            }

            if (value.token.type == CSSTokenType::Number)
            {
                if (allowNumberZero && value.token.number == 0.0)
                {
                    out = CSSValue::MakeDimension(0.0, CSSUnit::Px);
                    return true;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "A length needs a unit.",
                                  std::string("found '") + value.token.raw + "'",
                                  "Write '0' only for zero; every other length needs a unit such as px or rem.",
                                  subject);
                return false;
            }

            if (value.token.type == CSSTokenType::Ident && allowAuto && LowerCase(value.token.text) == "auto")
            {
                out = CSSValue::MakeKeyword("auto");
                return true;
            }

            if (value.isFunction)
            {
                const std::string name = LowerCase(value.token.text);

                if (name == "calc" || name == "min" || name == "max" || name == "clamp")
                {
                    return ParseMathFunction(value, out, location, diagnostics);
                }

                if (name == "var")
                {
                    // Unsubstituted: the cascade reports it.
                    out = CSSValue::MakeInvalid();
                    return false;
                }

                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "Unknown function in a length: " + name + "().",
                                    "the reference defines calc(), min(), max() and clamp() here",
                                    "Use a length or one of those functions.",
                                    name + "()");
                return false;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                              location,
                              "Expected a length, a percentage or a size keyword.",
                              std::string("found ") + ToString(value.token.type) + " '" + value.token.raw + "'",
                              "Write a length such as '1rem' or a percentage such as '50%'.",
                              subject);
            return false;
        }

        bool ParseBorderWidthValue(const std::vector<CSSComponentValue>& components,
                                   CSSValue& out,
                                   const CSSSourceLocation& location,
                                   CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(values[0]->token.text);

                if (name == "thin")
                {
                    out = CSSValue::MakeDimension(1.0, CSSUnit::Px);
                    return true;
                }
                if (name == "medium")
                {
                    out = CSSValue::MakeDimension(3.0, CSSUnit::Px);
                    return true;
                }
                if (name == "thick")
                {
                    out = CSSValue::MakeDimension(5.0, CSSUnit::Px);
                    return true;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "Unknown border width: '" + values[0]->token.text + "'.",
                                  "a border width is thin, medium, thick or a length",
                                  "Use one of those.",
                                  values[0]->token.text);
                return false;
            }

            return ParseLengthPercentage(components, out, location, diagnostics, false, false, "border-width");
        }

        bool ParseBorderStyleValue(const std::vector<CSSComponentValue>& components,
                                   CSSValue& out,
                                   const CSSSourceLocation& location,
                                   CSSDiagnosticSink& diagnostics)
        {
            static const char* styles[] = { "none",  "hidden", "dotted", "dashed", "solid",
                                            "double", "groove", "ridge",  "inset",  "outset" };

            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1 || values[0]->token.type != CSSTokenType::Ident)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "Expected a border style.",
                                  ComponentsToText(components),
                                  "Write 'none', 'solid', 'dashed' and so on.",
                                  "border-style");
                return false;
            }

            const std::string name = LowerCase(values[0]->token.text);

            for (const char* candidate : styles)
            {
                if (name == candidate)
                {
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Unknown border style: '" + values[0]->token.text + "'.",
                              "the CSS reference defines ten border styles",
                              "Write 'none', 'solid', 'dashed' and so on.",
                              values[0]->token.text);
            return false;
        }

        bool ParseDisplayValue(const std::vector<CSSComponentValue>& components,
                               CSSValue& out,
                               const CSSSourceLocation& location,
                               CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1 || values[0]->token.type != CSSTokenType::Ident)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "Expected a display keyword.",
                                  ComponentsToText(components),
                                  "Write 'block', 'inline', 'inline-block' or 'none'.",
                                  "display");
                return false;
            }

            const std::string name = LowerCase(values[0]->token.text);

            if (name == "block" || name == "inline" || name == "inline-block" || name == "none")
            {
                out = CSSValue::MakeKeyword(name == "inline-block" ? "inline-block" : name);
                return true;
            }

            if (name == "flex" || name == "grid" || name == "table" || name == "inline-flex" ||
                name == "inline-grid" || name == "contents" || name == "flow-root" || name == "list-item" ||
                name == "table-row" || name == "table-cell")
            {
                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "display: " + name + " is not implemented.",
                                    "the native layout engine implements block, inline, inline-block and none",
                                    "Use a supported display value; the declaration was skipped.",
                                    "display: " + name);
                return false;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Unknown display value: '" + values[0]->token.text + "'.",
                              "not a display keyword the reference defines",
                              "Write 'block', 'inline', 'inline-block' or 'none'.",
                              values[0]->token.text);
            return false;
        }

        bool ParseOverflowValue(const std::vector<CSSComponentValue>& components,
                                CSSValue& out,
                                const CSSSourceLocation& location,
                                CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1 || values[0]->token.type != CSSTokenType::Ident)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "Expected an overflow keyword.",
                                  ComponentsToText(components),
                                  "Write 'visible', 'hidden', 'clip', 'scroll' or 'auto'.",
                                  "overflow");
                return false;
            }

            const std::string name = LowerCase(values[0]->token.text);

            if (name == "visible" || name == "hidden" || name == "clip" || name == "scroll" || name == "auto")
            {
                out = CSSValue::MakeKeyword(name);
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Unknown overflow value: '" + values[0]->token.text + "'.",
                              "the reference defines five overflow keywords",
                              "Write 'visible', 'hidden', 'clip', 'scroll' or 'auto'.",
                              values[0]->token.text);
            return false;
        }

        bool ParseTextAlignValue(const std::vector<CSSComponentValue>& components,
                                 CSSValue& out,
                                 const CSSSourceLocation& location,
                                 CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1 || values[0]->token.type != CSSTokenType::Ident)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location, "Expected a text-align keyword.",
                                  ComponentsToText(components),
                                  "Write 'start', 'center', 'end', 'left' or 'right'.", "text-align");
                return false;
            }

            const std::string name = LowerCase(values[0]->token.text);

            if (name == "start" || name == "center" || name == "end" || name == "left" || name == "right")
            {
                out = CSSValue::MakeKeyword(name == "left" ? "start" : (name == "right" ? "end" : name));
                return true;
            }

            if (name == "justify" || name == "match-parent")
            {
                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "text-align: " + name + " is not implemented.",
                                    "the native text engine aligns left, centre and right",
                                    "Use 'start', 'center' or 'end'.",
                                    "text-align: " + name);
                return false;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Unknown text-align value: '" + values[0]->token.text + "'.",
                              "not a text-align keyword the reference defines",
                              "Write 'start', 'center' or 'end'.",
                              values[0]->token.text);
            return false;
        }

        bool ParseFontWeightValue(const std::vector<CSSComponentValue>& components,
                                  CSSValue& out,
                                  const CSSSourceLocation& location,
                                  CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Number)
            {
                const double weight = values[0]->token.number;

                if (weight < 1.0 || weight > 1000.0)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "A font weight is between 1 and 1000.",
                                      std::string("found '") + values[0]->token.raw + "'",
                                      "Write a number between 1 and 1000.",
                                      values[0]->token.raw);
                    return false;
                }

                out = weight >= 600.0 ? CSSValue::MakeKeyword("bold") : CSSValue::MakeKeyword("normal");
                out.number = weight;
                return true;
            }

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(values[0]->token.text);

                if (name == "normal" || name == "bold")
                {
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }

                if (name == "bolder" || name == "lighter")
                {
                    // Relative weights need the inherited weight, which the
                    // cascade knows, so they are resolved there.
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "Unknown font weight: '" + values[0]->token.text + "'.",
                                  "a font weight is normal, bold, bolder, lighter or 1..1000",
                                  "Use one of those.",
                                  values[0]->token.text);
                return false;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location, "Expected a font weight.",
                              ComponentsToText(components), "Write 'bold' or '600'.", "font-weight");
            return false;
        }

        bool ParseFontSizeValue(const std::vector<CSSComponentValue>& components,
                                CSSValue& out,
                                const CSSSourceLocation& location,
                                CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(values[0]->token.text);

                static const struct
                {
                    const char* name;
                    double scale;
                } AbsoluteSizes[] = {
                    { "xx-small", 0.5625 }, { "x-small", 0.625 }, { "small", 0.8125 }, { "medium", 1.0 },
                    { "large", 1.125 },     { "x-large", 1.5 },   { "xx-large", 2.0 },  { "xxx-large", 3.0 }
                };

                for (const auto& size : AbsoluteSizes)
                {
                    if (name == size.name)
                    {
                        // A keyword size resolves against the root font size, which
                        // is a compile-time constant.
                        out = CSSValue::MakeDimension(size.scale, CSSUnit::Rem);
                        return true;
                    }
                }

                if (name == "smaller" || name == "larger")
                {
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }

                if (name == "math")
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        location,
                                        "font-size: math is not implemented.",
                                        "the native runtime has one font face and no math font",
                                        "Use a length.",
                                        "font-size: math");
                    return false;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "Unknown font size: '" + values[0]->token.text + "'.",
                                  "a font size is an absolute size keyword, a relative keyword or a length",
                                  "Write '1rem', 'large' or 'smaller'.",
                                  values[0]->token.text);
                return false;
            }

            return ParseLengthPercentage(components, out, location, diagnostics, false, false, "font-size");
        }

        bool ParseBorderRadiusValue(const std::vector<CSSComponentValue>& components,
                                    CSSValue& out,
                                    const CSSSourceLocation& location,
                                    CSSDiagnosticSink& diagnostics)
        {
            // A slash separates the horizontal radius from the vertical one.
            for (const CSSComponentValue& value : components)
            {
                if (value.token.IsDelim('/'))
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        location,
                                        "An elliptical border radius is not implemented.",
                                        "the native border radius is circular",
                                        "Use a single radius.",
                                        "border-radius");
                    return false;
                }
            }

            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.empty() || values.size() > 4)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "border-radius takes one to four values.",
                                  std::to_string(values.size()) + " given",
                                  "Write '1rem', '1rem 2rem' or four values.",
                                  "border-radius");
                return false;
            }

            CSSValue resolved;

            if (!ParseLengthPercentage(std::vector<CSSComponentValue>{ *values[0] }, resolved, location, diagnostics,
                                       false, true, "border-radius"))
            {
                return false;
            }

            // The four corners are equal unless the shorthand gave more values,
            // which the expansion layer handles; here only the first is kept.
            out = resolved;
            return true;
        }
    }

    /// True when `name` is one of the space separated keywords the registry
    /// lists for a keyword-only property. The list is walked in place: there is
    /// no reason to build a container for a handful of words per declaration.
    bool KeywordIsInRegistry(const CSSPropertyDefinition& definition, const std::string& name)
    {
        if (definition.keywords == nullptr || name.empty())
        {
            return false;
        }

        const char* cursor = definition.keywords;

        while (*cursor != '\0')
        {
            while (*cursor == ' ')
            {
                ++cursor;
            }

            const char* begin = cursor;

            while (*cursor != '\0' && *cursor != ' ')
            {
                ++cursor;
            }

            const std::size_t length = static_cast<std::size_t>(cursor - begin);

            if (length == name.size() && std::strncmp(begin, name.c_str(), length) == 0)
            {
                return true;
            }
        }

        return false;
    }

    namespace
    {
        /// The value of a property this compiler *carries* rather than
        /// interprets.
        ///
        /// The specification states a grammar for every one of the properties
        /// this compiler carries, and the compiler models none of those
        /// grammars. Judging the value against a grammar it does not have would
        /// mean refusing declarations that are valid CSS, so it keeps what was
        /// written: one token per item, in source order.
        ///
        /// That is all a carried value needs to be: it is stored against the
        /// property, inherited when the property inherits, compared and interned
        /// as the author's own text, and printed back by the coverage report and
        /// the diagnostics. No stage reads meaning into it, and nothing is
        /// lowered from it.
        ///
        /// The one thing refused is an empty value, and that is not a judgement
        /// about the property's grammar: `opacity: ;` is not a value of any
        /// property in any specification.
        bool ParseDynamicValue(const CSSPropertyDefinition& definition,
                               const std::vector<CSSComponentValue>& components,
                               CSSValue& out,
                               const CSSSourceLocation& location,
                               CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "Invalid value for '" + std::string(definition.name) + "'.",
                                  "the declaration has no value",
                                  "Write a value, or delete the declaration.",
                                  definition.name);
                return false;
            }

            // As written: `50%` stays a percentage, `#FFF` its own spelling, and
            // a function call its own text. Normalising would mean interpreting,
            // which is the one thing a carried value is not for.
            const auto written = [](const CSSComponentValue* value) {
                return value->token.raw.empty() ? value->token.text : value->token.raw;
            };

            // One token is the whole value, and staying a single `Ident` keeps it
            // readable in a report without a list wrapper around it.
            if (values.size() == 1)
            {
                out = CSSValue::MakeIdent(written(values[0]));
                out.location = location;
                return true;
            }

            out = CSSValue();
            out.kind = CSSValueKind::List;
            out.location = location;
            out.items.reserve(values.size());

            for (const CSSComponentValue* value : values)
            {
                out.items.push_back(CSSValue::MakeIdent(written(value)));
            }

            return true;
        }
    }

    bool ParsePropertyValue(const CSSPropertyDefinition& definition,
                            const std::vector<CSSComponentValue>& components,
                            CSSValue& out,
                            const CSSSourceLocation& location,
                            CSSDiagnosticSink& diagnostics)
    {
        switch (definition.grammar)
        {
        case CSSValueGrammar::Size:
            return ParseSizeValue(components, out, location, diagnostics);

        case CSSValueGrammar::LengthPercentageOrAuto:
            return ParseLengthPercentage(components, out, location, diagnostics, true, true, definition.name);

        case CSSValueGrammar::LengthPercentage:
            return ParseLengthPercentage(components, out, location, diagnostics, false, true, definition.name);

        case CSSValueGrammar::BorderWidth:
            return ParseBorderWidthValue(components, out, location, diagnostics);

        case CSSValueGrammar::LengthOrNormal:
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(values[0]->token.text);

                if (name == "normal")
                {
                    out = CSSValue::MakeKeyword("normal");
                    return true;
                }
            }

            return ParseLengthPercentage(components, out, location, diagnostics, false, false, definition.name);
        }

        case CSSValueGrammar::LineHeight:
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident &&
                LowerCase(values[0]->token.text) == "normal")
            {
                out = CSSValue::MakeKeyword("normal");
                return true;
            }

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Number)
            {
                out = CSSValue::MakeNumber(values[0]->token.number);
                return true;
            }

            return ParseLengthPercentage(components, out, location, diagnostics, false, false, "line-height");
        }

        case CSSValueGrammar::Color:
        {
            CSSColor color;
            std::string subject;

            if (!ParseColorComponents(components, color, location, diagnostics, subject))
            {
                return false;
            }

            out = CSSValue::MakeColor(color);
            return true;
        }

        case CSSValueGrammar::BorderStyle:
            return ParseBorderStyleValue(components, out, location, diagnostics);

        case CSSValueGrammar::BorderRadius:
            return ParseBorderRadiusValue(components, out, location, diagnostics);

        case CSSValueGrammar::Display:
            return ParseDisplayValue(components, out, location, diagnostics);

        case CSSValueGrammar::Overflow:
            return ParseOverflowValue(components, out, location, diagnostics);

        case CSSValueGrammar::TextAlign:
            return ParseTextAlignValue(components, out, location, diagnostics);

        case CSSValueGrammar::FontWeight:
            return ParseFontWeightValue(components, out, location, diagnostics);

        case CSSValueGrammar::FontSize:
            return ParseFontSizeValue(components, out, location, diagnostics);

        case CSSValueGrammar::Dynamic:
            return ParseDynamicValue(definition, components, out, location, diagnostics);

        case CSSValueGrammar::Keyword:
        {
            // The closed keyword set is the registry's, and it is the whole
            // grammar: a keyword-only property accepts one word from that list.
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(values[0]->token.text);

                if (KeywordIsInRegistry(definition, name))
                {
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }
            }

            const std::string keywords =
                definition.keywords != nullptr ? std::string(definition.keywords) : std::string();
            std::string written;

            for (const CSSComponentValue* value : values)
            {
                if (!written.empty())
                {
                    written += " ";
                }
                written += value->token.raw.empty() ? value->token.text : value->token.raw;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Invalid value for '" + std::string(definition.name) + "'.",
                              "the keyword set is " + keywords,
                              "Write one of: " + keywords + ".",
                              written);
            return false;
        }

        case CSSValueGrammar::Unknown:
            break;
        }

        diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                          location,
                          "Property '" + std::string(definition.name) + "' has no value grammar.",
                          "the registry has no grammar for this property",
                          "This is a compiler bug: the property is registered without a grammar.",
                          definition.name);
        return false;
    }
}
