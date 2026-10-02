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

            // A function or a block keeps a reference in its arguments, which is
            // where `calc(var(--gap) * 2)` and `rgb(var(--r) 0 0)` keep theirs,
            // so the substitution descends into it rather than treating the
            // whole function as opaque.
            if (!value.children.empty())
            {
                CSSComponentValue container = value;
                container.children.clear();

                if (!SubstituteVariables(value.children, lookup, container.children, diagnostics, location,
                                         unresolvedName, depth))
                {
                    return false;
                }

                output.push_back(std::move(container));
                continue;
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
        /// One slot of the evaluator's stack: a number, a length or an angle.
        /// `None` is a plain number, which is what the reference calls
        /// `<number>`; a percentage never reaches a slot, because its reference
        /// is a runtime fact.
        struct MathSlot
        {
            double value = 0.0;
            CSSUnit unit = CSSUnit::None;
            CSSUnitFamily family = CSSUnitFamily::None;
        };

        /// The absolute units land on one canonical unit per family, so a sum of
        /// mixed units is one number in one unit. `1in + 1px` is 97px, which is
        /// what a browser computes; keeping the left operand's unit would make it
        /// 97 inches.
        void CanonicalizeAbsoluteUnit(double& value, CSSUnit& unit, CSSUnitFamily family)
        {
            if (family == CSSUnitFamily::Length)
            {
                value *= AbsoluteUnitScale(unit);
                unit = CSSUnit::Px;
            }
            else if (family == CSSUnitFamily::Angle)
            {
                value *= AbsoluteUnitScale(unit);
                unit = CSSUnit::Deg;
            }
        }

        /// How many operands a step consumes. The operators with a fixed count
        /// answer for themselves; the variadic functions carry their own count.
        int MathStepOperands(const CSSMathProgram::Step& step)
        {
            switch (step.op)
            {
            case CSSMathOperator::Clamp:
                return 3;
            case CSSMathOperator::Add:
            case CSSMathOperator::Subtract:
            case CSSMathOperator::Multiply:
            case CSSMathOperator::Divide:
            case CSSMathOperator::Min:
            case CSSMathOperator::Max:
            case CSSMathOperator::Pow:
            case CSSMathOperator::Mod:
            case CSSMathOperator::Rem:
            case CSSMathOperator::Atan2:
                return 2;
            case CSSMathOperator::Hypot:
            case CSSMathOperator::Log:
            case CSSMathOperator::Round:
                return step.operands;
            case CSSMathOperator::Abs:
            case CSSMathOperator::Sign:
            case CSSMathOperator::Sqrt:
            case CSSMathOperator::Exp:
            case CSSMathOperator::Sin:
            case CSSMathOperator::Cos:
            case CSSMathOperator::Tan:
            case CSSMathOperator::Asin:
            case CSSMathOperator::Acos:
            case CSSMathOperator::Atan:
                return 1;
            default:
                return 0;
            }
        }

        /// True when every operand names the same family, which is what the
        /// reference requires of a sum, a comparison and the functions that keep
        /// their argument's type.
        bool SameMathFamily(const MathSlot* operands, int count)
        {
            for (int index = 1; index < count; ++index)
            {
                if (operands[index].family != operands[0].family)
                {
                    return false;
                }
            }

            return true;
        }

        const MathSlot* NumberOperand(const MathSlot* operands, int count)
        {
            for (int index = 0; index < count; ++index)
            {
                if (operands[index].family != CSSUnitFamily::None)
                {
                    return nullptr;
                }
            }

            return operands;
        }

        /// The degrees of one angle operand.
        bool AngleOperand(const MathSlot& slot, double& out)
        {
            if (slot.family != CSSUnitFamily::Angle)
            {
                return false;
            }

            out = slot.value;
            return true;
        }

        /// The reference's rounding strategies, as `round()` applies them.
        double ApplyRounding(CSSRoundingStrategy strategy, double value, double step)
        {
            if (step == 0.0)
            {
                return value;
            }

            const double quotient = value / step;

            switch (strategy)
            {
            case CSSRoundingStrategy::Up:
                return std::ceil(quotient) * step;
            case CSSRoundingStrategy::Down:
                return std::floor(quotient) * step;
            case CSSRoundingStrategy::ToZero:
                return std::trunc(quotient) * step;
            case CSSRoundingStrategy::Nearest:
            default:
                // A tie goes to the larger of the two, which is what the
                // reference calls "the closest whole number, with ties rounding
                // up" - and for a negative value, "up" still means away from
                // zero, so the same expression on the quotient answers both.
                return std::floor(quotient + 0.5) * step;
            }
        }

        /// Applies one operator or function to the operands the stack holds.
        bool ApplyMathStep(const CSSMathProgram::Step& step,
                           const MathSlot* operands,
                           int count,
                           MathSlot& out)
        {
            out = operands[0];

            switch (step.op)
            {
            case CSSMathOperator::Add:
                if (!SameMathFamily(operands, 2))
                {
                    return false;
                }
                out.value = operands[0].value + operands[1].value;
                return true;

            case CSSMathOperator::Subtract:
                if (!SameMathFamily(operands, 2))
                {
                    return false;
                }
                out.value = operands[0].value - operands[1].value;
                return true;

            case CSSMathOperator::Multiply:
                // One side has to be a plain number: `2px * 3px` would be an
                // area, which no property this compiler lowers takes.
                if (operands[0].family != CSSUnitFamily::None && operands[1].family != CSSUnitFamily::None)
                {
                    return false;
                }
                out = operands[0].family != CSSUnitFamily::None ? operands[0] : operands[1];
                out.value = operands[0].value * operands[1].value;
                return true;

            case CSSMathOperator::Divide:
            {
                if (operands[1].value == 0.0)
                {
                    return false;
                }

                if (operands[1].family == CSSUnitFamily::None)
                {
                    // A number divides a typed value and keeps its type.
                    out = operands[0];
                }
                else if (operands[0].family == operands[1].family)
                {
                    // A value divided by its own kind is a plain ratio.
                    out = MathSlot{};
                }
                else
                {
                    return false;
                }

                out.value = operands[0].value / operands[1].value;
                return true;
            }

            case CSSMathOperator::Min:
            case CSSMathOperator::Max:
            case CSSMathOperator::Clamp:
            {
                const int used = step.op == CSSMathOperator::Clamp ? 3 : 2;

                if (!SameMathFamily(operands, used))
                {
                    return false;
                }

                if (step.op == CSSMathOperator::Clamp)
                {
                    out.value = Clamp(operands[1].value, operands[0].value, operands[2].value);
                }
                else if (step.op == CSSMathOperator::Min)
                {
                    out.value = std::min(operands[0].value, operands[1].value);
                }
                else
                {
                    out.value = std::max(operands[0].value, operands[1].value);
                }

                return true;
            }

            case CSSMathOperator::Abs:
                out.value = std::fabs(operands[0].value);
                return true;

            case CSSMathOperator::Sign:
            {
                const double value = operands[0].value;
                out = MathSlot{};
                out.value = value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
                return true;
            }

            case CSSMathOperator::Sqrt:
                if (operands[0].value < 0.0)
                {
                    return false;
                }
                out.value = std::sqrt(operands[0].value);
                return true;

            case CSSMathOperator::Hypot:
            {
                if (!SameMathFamily(operands, count))
                {
                    return false;
                }

                double sum = 0.0;
                for (int index = 0; index < count; ++index)
                {
                    sum += operands[index].value * operands[index].value;
                }

                out.value = std::sqrt(sum);
                return true;
            }

            case CSSMathOperator::Pow:
            {
                if (NumberOperand(operands, 2) == nullptr)
                {
                    return false;
                }

                out = MathSlot{};
                out.value = std::pow(operands[0].value, operands[1].value);

                if (!std::isfinite(out.value))
                {
                    return false;
                }

                return true;
            }

            case CSSMathOperator::Log:
            {
                if (NumberOperand(operands, count) == nullptr || operands[0].value <= 0.0)
                {
                    return false;
                }

                double result = std::log(operands[0].value);
                if (count == 2)
                {
                    if (operands[1].value <= 0.0 || operands[1].value == 1.0)
                    {
                        return false;
                    }

                    result /= std::log(operands[1].value);
                }

                out = MathSlot{};
                out.value = result;
                return true;
            }

            case CSSMathOperator::Exp:
                if (NumberOperand(operands, 1) == nullptr)
                {
                    return false;
                }

                out = MathSlot{};
                out.value = std::exp(operands[0].value);

                if (!std::isfinite(out.value))
                {
                    return false;
                }

                return true;

            case CSSMathOperator::Mod:
            case CSSMathOperator::Rem:
            {
                if (!SameMathFamily(operands, 2) || operands[1].value == 0.0)
                {
                    return false;
                }

                const double left = operands[0].value;
                const double right = operands[1].value;
                const double quotient = std::trunc(left / right);

                // `mod` keeps the sign of the divisor, `rem` the sign of the
                // dividend, which is the one difference between them.
                out.value = step.op == CSSMathOperator::Rem ? left - quotient * right
                                                            : left - std::floor(left / right) * right;
                return true;
            }

            case CSSMathOperator::Round:
            {
                if (!SameMathFamily(operands, count))
                {
                    return false;
                }

                // One operand is the value; the optional second is the step.
                out.value = ApplyRounding(step.rounding, operands[0].value,
                                          count == 2 ? operands[1].value : 1.0);
                return true;
            }

            case CSSMathOperator::Sin:
            case CSSMathOperator::Cos:
            case CSSMathOperator::Tan:
            {
                double degrees = 0.0;
                if (!AngleOperand(operands[0], degrees))
                {
                    return false;
                }

                const double radians = degrees * Pi / 180.0;
                double result = step.op == CSSMathOperator::Sin
                                    ? std::sin(radians)
                                    : (step.op == CSSMathOperator::Cos ? std::cos(radians)
                                                                       : std::tan(radians));

                // A right angle has no tangent, and the reference makes that
                // value infinity rather than a number.
                if (!std::isfinite(result))
                {
                    return false;
                }

                // The reference states these to the precision of a double.
                if (std::fabs(result) < 1e-15)
                {
                    result = 0.0;
                }

                out = MathSlot{};
                out.value = result;
                return true;
            }

            case CSSMathOperator::Asin:
            case CSSMathOperator::Acos:
            case CSSMathOperator::Atan:
            {
                if (NumberOperand(operands, 1) == nullptr)
                {
                    return false;
                }

                const double value = operands[0].value;

                if ((step.op == CSSMathOperator::Asin || step.op == CSSMathOperator::Acos) &&
                    (value < -1.0 || value > 1.0))
                {
                    return false;
                }

                const double radians = step.op == CSSMathOperator::Asin
                                           ? std::asin(value)
                                           : (step.op == CSSMathOperator::Acos ? std::acos(value)
                                                                               : std::atan(value));

                out = MathSlot{};
                out.unit = CSSUnit::Deg;
                out.family = CSSUnitFamily::Angle;
                out.value = radians * 180.0 / Pi;
                return true;
            }

            case CSSMathOperator::Atan2:
            {
                if (NumberOperand(operands, 2) == nullptr)
                {
                    return false;
                }

                out = MathSlot{};
                out.unit = CSSUnit::Deg;
                out.family = CSSUnitFamily::Angle;
                out.value = std::atan2(operands[0].value, operands[1].value) * 180.0 / Pi;
                return true;
            }

            default:
                return false;
            }
        }

        /// Evaluates a math program whose operands are all absolute. Returns
        /// false when an operand needs a runtime context, or when the expression
        /// is not a value CSS defines - a length times a length, say.
        bool EvaluateAbsoluteProgram(const CSSMathProgram& program, double& out, bool& isLength, CSSUnit& unit)
        {
            MathSlot stack[8];
            int depth = 0;

            for (const CSSMathProgram::Step& step : program.steps)
            {
                if (step.op == CSSMathOperator::Invalid)
                {
                    if (step.isPercentage)
                    {
                        return false;
                    }

                    MathSlot slot;
                    slot.value = step.value;
                    slot.unit = step.unit;
                    slot.family = step.unit == CSSUnit::None ? CSSUnitFamily::None : UnitFamily(step.unit);

                    if (slot.family != CSSUnitFamily::None)
                    {
                        if (!IsAbsoluteUnit(step.unit))
                        {
                            return false;
                        }

                        CanonicalizeAbsoluteUnit(slot.value, slot.unit, slot.family);
                    }

                    if (depth >= 8)
                    {
                        return false;
                    }

                    stack[depth++] = slot;
                    continue;
                }

                const int operands = MathStepOperands(step);

                if (operands <= 0 || depth < operands)
                {
                    return false;
                }

                MathSlot result;

                if (!ApplyMathStep(step, stack + depth - operands, operands, result))
                {
                    return false;
                }

                depth -= operands - 1;
                stack[depth - 1] = result;
            }

            if (depth != 1)
            {
                return false;
            }

            out = stack[0].value;
            isLength = stack[0].family != CSSUnitFamily::None;
            unit = stack[0].unit;
            return true;
        }
        /// The functions a `<calc-sum>` may contain: the ones this compiler can
        /// give a value to. Everything else the reference defines (`env()`,
        /// `attr()`, `anchor()`, `random()`, the tree counters) needs a document
        /// or a runtime it does not have, and is reported where it is used.
        bool IsMathFunctionName(const std::string& name)
        {
            static const char* const names[] = { "calc",  "min",   "max",    "clamp", "abs",
                                                 "sign",  "sqrt",  "hypot",  "pow",   "log",
                                                 "exp",   "mod",   "rem",    "round", "sin",
                                                 "cos",   "tan",   "asin",   "acos",  "atan",
                                                 "atan2" };

            for (const char* candidate : names)
            {
                if (name == candidate)
                {
                    return true;
                }
            }

            return false;
        }

        /// Pushes the operands a finished math value contributes. A function
        /// whose operands are all absolute has already folded to a number or a
        /// length by the time it gets here, so the two cases are one step and a
        /// spliced program.
        bool AppendMathValue(const CSSValue& value,
                             CSSMathProgram& program,
                             const CSSSourceLocation& location,
                             CSSDiagnosticSink& diagnostics,
                             const std::string& name)
        {
            if (value.kind == CSSValueKind::Math)
            {
                for (const CSSMathProgram::Step& step : value.math.steps)
                {
                    program.steps.push_back(step);
                }

                return true;
            }

            CSSMathProgram::Step step;

            if (value.kind == CSSValueKind::Number)
            {
                step.value = value.number;
                program.steps.push_back(step);
                return true;
            }

            if (value.kind == CSSValueKind::Dimension)
            {
                step.value = value.number;
                step.unit = value.unit;
                program.steps.push_back(step);
                return true;
            }

            if (value.kind == CSSValueKind::Percentage)
            {
                step.value = value.number;
                step.isPercentage = true;
                program.steps.push_back(step);
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                              location,
                              name + "() is not a number, a length or an angle.",
                              "a math function has to produce one of those to be an operand",
                              "Check what the function was given.",
                              name + "()");
            return false;
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
                const CSSUnitFamily family =
                    ParseUnit(value.token.unit, unit) ? UnitFamily(unit) : CSSUnitFamily::None;

                // A math expression carries a length or an angle, which are the
                // two families the functions this compiler implements accept. A
                // time or a resolution has no property here to be a value of.
                if (family != CSSUnitFamily::Length && family != CSSUnitFamily::Angle)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidUnit,
                                      location,
                                      "Unknown unit in a math expression: " + value.token.unit + ".",
                                      std::string("'") + value.token.raw + "'",
                                      "Use a length unit, or an angle unit with a trigonometric function.",
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

                // Every other function is an operand of its own: nesting one
                // inside a calc() is the same as calling it there.
                if (IsMathFunctionName(name))
                {
                    CSSValue inner;

                    if (!ParseMathFunction(value, inner, location, diagnostics))
                    {
                        return false;
                    }

                    return AppendMathValue(inner, program, location, diagnostics, name);
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

        /// Splits a function's comma-separated arguments, each one a program of
        /// its own. The reference writes every function this way, so the split
        /// is shared by all of them.
        bool CollectMathArguments(const CSSComponentValue& functionValue,
                                  std::vector<CSSMathProgram>& programs,
                                  CSSDiagnosticSink& diagnostics,
                                  const CSSSourceLocation& location)
        {
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

            // An empty argument cannot be left out: `clamp(1px,,2px)` is not a
            // value, and the count check below is what reports it.
            if (!current.steps.empty())
            {
                programs.push_back(current);
            }

            return true;
        }

        /// The rounding strategy `round()` may be written with, as its first
        /// argument.
        bool ParseRoundingStrategy(const CSSComponentValue& value, CSSRoundingStrategy& out)
        {
            if (value.token.type != CSSTokenType::Ident)
            {
                return false;
            }

            const std::string name = LowerCase(value.token.text);

            if (name == "nearest")
            {
                out = CSSRoundingStrategy::Nearest;
                return true;
            }
            if (name == "up")
            {
                out = CSSRoundingStrategy::Up;
                return true;
            }
            if (name == "down")
            {
                out = CSSRoundingStrategy::Down;
                return true;
            }
            if (name == "to-zero")
            {
                out = CSSRoundingStrategy::ToZero;
                return true;
            }

            return false;
        }

        /// Wraps a program in a comma-list of arguments and reduces it with the
        /// function's operator, then gives the folded result a value.
        bool FinishMathFunction(const std::string& name,
                                const std::vector<CSSMathProgram>& programs,
                                const CSSMathProgram::Step* functionStep,
                                CSSValue& out,
                                CSSDiagnosticSink& diagnostics,
                                const CSSSourceLocation& location)
        {
            CSSMathProgram combined;

            for (const CSSMathProgram& program : programs)
            {
                for (const CSSMathProgram::Step& step : program.steps)
                {
                    combined.steps.push_back(step);
                }
            }

            if (functionStep != nullptr)
            {
                combined.steps.push_back(*functionStep);
            }

            double value = 0.0;
            bool isLength = false;
            CSSUnit unit = CSSUnit::None;

            if (EvaluateAbsoluteProgram(combined, value, isLength, unit))
            {
                out = isLength ? CSSValue::MakeDimension(value, unit) : CSSValue::MakeNumber(value);
                return true;
            }

            // One operand that the runtime has to resolve: the function is kept
            // as an expression for the runtime, exactly as `calc()` is.
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

                out = CSSValue::MakeNumber(step.value);
                return true;
            }

            diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                location,
                                name + "() has no compile-time value.",
                                "a compiled native length carries one dimension, and this expression mixes a "
                                "relative operand with an absolute one",
                                "Use one unit.",
                                name + "()");
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
            // The arguments are separated by commas; build one program per
            // argument and reduce them with the operator.
            std::vector<CSSMathProgram> programs;

            if (!CollectMathArguments(functionValue, programs, diagnostics, location))
            {
                return false;
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

            if (name == "clamp")
            {
                CSSMathProgram::Step functionStep;
                functionStep.op = CSSMathOperator::Clamp;
                return FinishMathFunction(name, programs, &functionStep, out, diagnostics, location);
            }

            // `min` and `max` take as many arguments as they are given; the
            // program reduces them pairwise, which is the same value and keeps
            // the operator at a fixed arity.
            CSSMathProgram combined = programs.front();

            for (std::size_t index = 1; index < programs.size(); ++index)
            {
                for (const CSSMathProgram::Step& step : programs[index].steps)
                {
                    combined.steps.push_back(step);
                }

                CSSMathProgram::Step operatorStep;
                operatorStep.op = name == "min" ? CSSMathOperator::Min : CSSMathOperator::Max;
                combined.steps.push_back(operatorStep);
            }

            std::vector<CSSMathProgram> folded;
            folded.push_back(combined);

            return FinishMathFunction(name, folded, nullptr, out, diagnostics, location);
        }

        // The numeric functions. Each is a pure function of its arguments, so
        // its whole value is decided here when they are absolute.
        if (IsMathFunctionName(name) && name != "calc" && name != "min" && name != "max" &&
            name != "clamp")
        {
            std::vector<CSSMathProgram> programs;

            CSSMathProgram::Step functionStep;
            std::size_t required = 1;
            std::size_t maximum = 1;
            bool variadic = false;

            if (name == "round")
            {
                // The optional leading keyword names the strategy rather than an
                // operand, so it is taken off before the arguments are read.
                CSSRoundingStrategy strategy = CSSRoundingStrategy::Nearest;
                std::vector<const CSSComponentValue*> items;

                for (const CSSComponentValue& child : functionValue.children)
                {
                    if (child.token.type != CSSTokenType::Whitespace)
                    {
                        items.push_back(&child);
                    }
                }

                std::size_t start = 0;

                if (!items.empty() && ParseRoundingStrategy(*items[0], strategy))
                {
                    functionStep.rounding = strategy;
                    start = 1;
                }

                CSSMathProgram current;

                for (std::size_t index = start; index < items.size(); ++index)
                {
                    const CSSComponentValue& item = *items[index];

                    if (item.token.type == CSSTokenType::Comma)
                    {
                        if (!current.steps.empty())
                        {
                            programs.push_back(current);
                            current.steps.clear();
                        }
                        continue;
                    }

                    if (!ParseMathOperand(item, current, diagnostics, location))
                    {
                        return false;
                    }
                }

                if (!current.steps.empty())
                {
                    programs.push_back(current);
                }

                required = 1;
                maximum = 2;
            }
            else if (name == "log")
            {
                if (!CollectMathArguments(functionValue, programs, diagnostics, location))
                {
                    return false;
                }

                required = 1;
                maximum = 2;
            }
            else if (name == "hypot")
            {
                if (!CollectMathArguments(functionValue, programs, diagnostics, location))
                {
                    return false;
                }

                required = 1;
                maximum = 0;
                variadic = true;
            }
            else if (name == "pow" || name == "mod" || name == "rem" || name == "atan2")
            {
                if (!CollectMathArguments(functionValue, programs, diagnostics, location))
                {
                    return false;
                }

                required = 2;
                maximum = 2;
            }
            else
            {
                if (!CollectMathArguments(functionValue, programs, diagnostics, location))
                {
                    return false;
                }

                required = 1;
                maximum = 1;
            }

            static const struct
            {
                const char* name;
                CSSMathOperator op;
            } FunctionOperators[] = {
                { "abs", CSSMathOperator::Abs },       { "sign", CSSMathOperator::Sign },
                { "sqrt", CSSMathOperator::Sqrt },     { "hypot", CSSMathOperator::Hypot },
                { "pow", CSSMathOperator::Pow },       { "log", CSSMathOperator::Log },
                { "exp", CSSMathOperator::Exp },       { "mod", CSSMathOperator::Mod },
                { "rem", CSSMathOperator::Rem },       { "round", CSSMathOperator::Round },
                { "sin", CSSMathOperator::Sin },       { "cos", CSSMathOperator::Cos },
                { "tan", CSSMathOperator::Tan },       { "asin", CSSMathOperator::Asin },
                { "acos", CSSMathOperator::Acos },     { "atan", CSSMathOperator::Atan },
                { "atan2", CSSMathOperator::Atan2 }
            };

            for (const auto& entry : FunctionOperators)
            {
                if (name == entry.name)
                {
                    functionStep.op = entry.op;
                    break;
                }
            }

            functionStep.operands = static_cast<std::uint8_t>(programs.size());

            if (programs.size() < required || (!variadic && programs.size() > maximum))
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidFunction,
                                  location,
                                  name + "() was given " + std::to_string(programs.size()) +
                                      " arguments.",
                                  variadic ? std::string("it takes one or more")
                                           : (required == maximum
                                                  ? ("it takes exactly " + std::to_string(required))
                                                  : ("it takes " + std::to_string(required) + " or " +
                                                     std::to_string(maximum))),
                                  "Write the arguments separated by commas.",
                                  name + "()");
                return false;
            }

            return FinishMathFunction(name, programs, &functionStep, out, diagnostics, location);
        }

        diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                            location,
                            "Unknown math function: " + name + "().",
                            "this compiler implements the numeric functions the reference defines "
                            "(calc, min, max, clamp, abs, sign, sqrt, hypot, pow, log, exp, mod, rem, "
                            "round and the trigonometric set), and has no value for this one",
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

        /// True when the parsed value is a number, length or percentage below
        /// zero, which the calling property does not allow. CSS names a handful
        /// of properties that cannot be negative -- a size, a padding, a border
        /// width, a radius, a gap, a font size, a line height and
        /// `flex-grow`/`flex-shrink` -- and the parser rejects the value there
        /// rather than clamping it, so a wrong value is an error above the
        /// declaration just like a wrong property. `margin`, `text-indent`,
        /// `letter-spacing` and `word-spacing` do allow a negative value and do
        /// not call this.
        bool RejectNegativeValue(const CSSValue& value,
                                 const CSSSourceLocation& location,
                                 CSSDiagnosticSink& diagnostics,
                                 const char* subject)
        {
            const bool isNumeric = value.kind == CSSValueKind::Dimension ||
                                   value.kind == CSSValueKind::Percentage ||
                                   value.kind == CSSValueKind::Number;

            if (!isNumeric || value.number >= 0.0)
            {
                return false;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              std::string(subject) + " cannot be negative.",
                              "a negative value is not valid here",
                              "Write a value of zero or more.",
                              subject);
            return true;
        }

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

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, true, "size"))
            {
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, "a size");
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

                if (IsMathFunctionName(name))
                {
                    if (!ParseMathFunction(value, out, location, diagnostics))
                    {
                        return false;
                    }

                    // A function can compute to an angle (`atan`, `atan2`) or to
                    // a plain number (`sin`, `sqrt` of a number). Neither is a
                    // length, and the reference would not apply one here either.
                    if (out.kind == CSSValueKind::Dimension && UnitFamily(out.unit) != CSSUnitFamily::Length)
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                          location,
                                          name + "() computed to an angle.",
                                          "this property takes a length",
                                          "Use the angle with a function that turns it into a length, such as "
                                          "calc() multiplying it by 1px.",
                                          name + "()");
                        return false;
                    }

                    return true;
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
                                    "this compiler implements the numeric functions the reference defines, "
                                    "which is calc(), min(), max(), clamp() and the abs/sign/sqrt/hypot/pow/log/"
                                    "exp/mod/rem/round and trigonometric set; the rest need a document or a "
                                    "runtime it does not have",
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

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, false, "border-width"))
            {
                return false;
            }

            if (out.kind == CSSValueKind::Percentage)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                  "border-width does not take a percentage.", "a border width is a length",
                                  "Write a length such as 2px, or thin, medium or thick.", "border-width");
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, "a border width");
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

            // `inline-flex` differs from `flex` only in the container's own
            // outer display type, which this runtime models as a block-level
            // box either way, so both become the same keyword.
            if (name == "flex" || name == "inline-flex")
            {
                out = CSSValue::MakeKeyword("flex");
                return true;
            }

            if (name == "grid" || name == "table" ||
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

            // `overlay` is the reference's deprecated spelling of `auto` and
            // computes to it, so it is stored as `auto`.
            const std::string keyword = name == "overlay" ? "auto" : name;

            if (keyword == "visible" || keyword == "hidden" || keyword == "clip" || keyword == "scroll" ||
                keyword == "auto")
            {
                out = CSSValue::MakeKeyword(keyword);
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

        /// `white-space`: the five classic keywords, or one or two keywords that
        /// name a `white-space-collapse` value and a `text-wrap-mode` value. The
        /// reference makes the property a shorthand for those two, and every
        /// combination computes to one of the five classic keywords, which is the
        /// keyword the parser stores.
        bool ParseWhiteSpaceValue(const std::vector<CSSComponentValue>& components,
                                  CSSValue& out,
                                  const CSSSourceLocation& location,
                                  CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.empty() || values.size() > 2)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "white-space takes one or two keywords.",
                                  std::to_string(values.size()) + " values given",
                                  "Write 'normal', 'pre-wrap' or 'preserve nowrap'.",
                                  ComponentsToText(components));
                return false;
            }

            std::string collapse;
            std::string wrapMode;

            for (const CSSComponentValue* value : values)
            {
                if (value->token.type != CSSTokenType::Ident)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                      location,
                                      "white-space takes keywords.",
                                      ComponentsToText(components),
                                      "Write 'normal', 'pre-wrap' or 'preserve nowrap'.",
                                      "white-space");
                    return false;
                }

                const std::string name = LowerCase(value->token.text);

                // The four keywords the shorthand kept for itself are whole-value
                // spellings of a collapse value and a wrap mode.
                if (name == "normal" || name == "pre" || name == "pre-wrap" || name == "pre-line")
                {
                    if (values.size() != 1)
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                          location,
                                          "'" + name + "' is a whole white-space value, not one half of one.",
                                          "the reference spells one or two of the collapse and wrap keywords",
                                          "Write just '" + name + "'.",
                                          name);
                        return false;
                    }

                    out = CSSValue::MakeKeyword(name);
                    return true;
                }

                if (name == "collapse" || name == "preserve" || name == "preserve-breaks" ||
                    name == "preserve-spaces" || name == "break-spaces")
                {
                    if (!collapse.empty())
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                          "white-space names the whitespace collapse once.",
                                          "'" + collapse + "' was already given",
                                          "Write one collapse keyword and one wrap keyword.", name);
                        return false;
                    }

                    collapse = name;
                    continue;
                }

                if (name == "wrap" || name == "nowrap")
                {
                    if (!wrapMode.empty())
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                          "white-space names the wrap mode once.",
                                          "'" + wrapMode + "' was already given",
                                          "Write one collapse keyword and one wrap keyword.", name);
                        return false;
                    }

                    wrapMode = name;
                    continue;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  location,
                                  "Unknown white-space value: '" + value->token.text + "'.",
                                  "not a white-space, white-space-collapse or text-wrap-mode keyword",
                                  "Write 'normal', 'pre', 'pre-wrap', 'pre-line', 'nowrap' or 'break-spaces'.",
                                  value->token.text);
                return false;
            }

            // Each half defaults to its initial value as the two longhands do,
            // so `white-space: collapse` is collapse plus wrap and
            // `white-space: nowrap` is collapse plus nowrap.
            const std::string collapseValue = collapse.empty() ? "collapse" : collapse;
            const std::string wrapValue = wrapMode.empty() ? "wrap" : wrapMode;
            const bool preserveBreaks = collapseValue == "preserve-breaks";
            const bool preserveSpaces = collapseValue == "preserve" || collapseValue == "preserve-spaces" ||
                                        collapseValue == "break-spaces";

            if (preserveBreaks)
            {
                // Collapse the spaces, honour the newlines, wrap.
                out = CSSValue::MakeKeyword("pre-line");
                return true;
            }

            if (preserveSpaces)
            {
                // Keep the spaces; wrapping is what tells `pre` from `pre-wrap`.
                out = CSSValue::MakeKeyword(wrapValue == "nowrap" ? "pre" : "pre-wrap");
                return true;
            }

            // The spaces collapse, so only the wrap mode survives: `nowrap` is
            // the one classic keyword that does not wrap.
            out = CSSValue::MakeKeyword(wrapValue == "nowrap" ? "nowrap" : "normal");
            return true;
        }

        /// `z-index`: an integer or the keyword `auto`. A number that is not an
        /// integer is not a value CSS defines here, so it is refused rather than
        /// rounded.
        bool ParseZIndexValue(const std::vector<CSSComponentValue>& components,
                              CSSValue& out,
                              const CSSSourceLocation& location,
                              CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar,
                                  location,
                                  "Expected one value.",
                                  ComponentsToText(components),
                                  "Write 'auto' or an integer such as 0, 1 or -1.",
                                  "z-index");
                return false;
            }

            if (values[0]->token.type == CSSTokenType::Ident &&
                LowerCase(values[0]->token.text) == "auto")
            {
                out = CSSValue::MakeKeyword("auto");
                return true;
            }

            if (values[0]->token.type == CSSTokenType::Number && values[0]->token.isInteger)
            {
                out = CSSValue::MakeNumber(values[0]->token.number, true);
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Invalid z-index value: '" + ComponentsToText(components) + "'.",
                              "z-index is an integer or the keyword 'auto'",
                              "Write 'auto' or an integer such as 0, 1 or -1.",
                              ComponentsToText(components));
            return false;
        }

        /// A single `<number>`, as `flex-grow` and `flex-shrink` are written.
        /// A percentage is not a number, and neither is a bare ident.
        bool ParseNumberValue(const std::vector<CSSComponentValue>& components,
                              CSSValue& out,
                              const CSSSourceLocation& location,
                              CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Number)
            {
                out = CSSValue::MakeNumber(values[0]->token.number, values[0]->token.isInteger);
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Invalid number value: '" + ComponentsToText(components) + "'.",
                              "the property takes a single number",
                              "Write a number such as 0, 1 or 2.5.",
                              ComponentsToText(components));
            return false;
        }

        /// A single `<integer>`, as `order` is written.
        bool ParseIntegerValue(const std::vector<CSSComponentValue>& components,
                               CSSValue& out,
                               const CSSSourceLocation& location,
                               CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Number &&
                values[0]->token.isInteger)
            {
                out = CSSValue::MakeNumber(values[0]->token.number, true);
                return true;
            }

            diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                              location,
                              "Invalid integer value: '" + ComponentsToText(components) + "'.",
                              "the property takes a single integer",
                              "Write an integer such as -1, 0 or 2.",
                              ComponentsToText(components));
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

            if (name == "justify")
            {
                out = CSSValue::MakeKeyword("justify");
                return true;
            }

            if (name == "match-parent")
            {
                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    location,
                                    "text-align: match-parent is not implemented.",
                                    "the native text engine has no parent writing mode to inherit",
                                    "Use 'start', 'center', 'end' or 'justify'.",
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

        bool ParseTextDecorationLineValue(const std::vector<CSSComponentValue>& components,
                                          CSSValue& out,
                                          const CSSSourceLocation& location,
                                          CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "Expected a text-decoration-line keyword.", "no value given",
                                  "Write 'underline', 'overline', 'line-through' or 'none'.",
                                  "text-decoration-line");
                return false;
            }

            static const char* const kKeywords[] = { "none", "underline", "overline", "line-through",
                                                     "spelling-error", "grammar-error", "blink" };

            std::vector<CSSValue> keywords;

            for (const CSSComponentValue* value : values)
            {
                if (value->token.type != CSSTokenType::Ident)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "Invalid value for 'text-decoration-line'.",
                                      "the grammar is one or more of none, underline, overline, "
                                      "line-through, spelling-error, grammar-error, blink",
                                      "Write 'underline', 'spelling-error' or 'none'.",
                                      ComponentsToText(components));
                    return false;
                }

                const std::string name = LowerCase(value->token.text);
                bool known = false;

                for (const char* keyword : kKeywords)
                {
                    known = known || name == keyword;
                }

                // `blink` is valid CSS and is a declaration to do nothing: it is
                // accepted and dropped rather than refused.
                if (name == "blink")
                {
                    continue;
                }

                if (!known)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "Unknown text-decoration-line keyword: '" + value->token.text + "'.",
                                      "not a keyword the reference defines",
                                      "Write 'underline', 'spelling-error' or 'none'.",
                                      value->token.text);
                    return false;
                }

                keywords.push_back(CSSValue::MakeKeyword(name));
            }

            if (keywords.empty())
            {
                out = CSSValue::MakeKeyword("none");
                return true;
            }

            if (keywords.size() == 1)
            {
                out = keywords[0];
                return true;
            }

            out = CSSValue();
            out.kind = CSSValueKind::List;
            out.items = std::move(keywords);
            out.location = location;
            return true;
        }

        /// One or more keywords from the property's own closed set. `normal` and
        /// `none` stand alone; any other keyword may be combined.
        bool ParseKeywordSetListValue(const CSSPropertyDefinition& definition,
                                      const std::vector<CSSComponentValue>& components,
                                      CSSValue& out,
                                      const CSSSourceLocation& location,
                                      CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "Expected a keyword for '" + std::string(definition.name) + "'.",
                                  "no value given",
                                  std::string("Write one of: ") + definition.keywords + ".",
                                  definition.name);
                return false;
            }

            // The registry's keyword set is a space separated list; asking it
            // directly is what keeps the property and its validator in step.
            const auto userOwnsKeyword = [&definition](const std::string& name) {
                if (definition.keywords == nullptr)
                {
                    return false;
                }

                const std::string set(definition.keywords);
                std::size_t position = 0;

                while (position < set.size())
                {
                    while (position < set.size() && std::isspace(static_cast<unsigned char>(set[position])) != 0)
                    {
                        ++position;
                    }

                    const std::size_t begin = position;
                    while (position < set.size() && std::isspace(static_cast<unsigned char>(set[position])) == 0)
                    {
                        ++position;
                    }

                    if (position > begin && set.compare(begin, position - begin, name) == 0)
                    {
                        return true;
                    }
                }

                return false;
            };

            std::vector<CSSValue> keywords;

            for (const CSSComponentValue* value : values)
            {
                if (value->token.type != CSSTokenType::Ident)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "Invalid value for '" + std::string(definition.name) + "'.",
                                      std::string("the keyword set is ") + definition.keywords,
                                      std::string("Write one of: ") + definition.keywords + ".",
                                      ComponentsToText(components));
                    return false;
                }

                const std::string name = LowerCase(value->token.text);
                if (!userOwnsKeyword(name))
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "Unknown keyword for '" + std::string(definition.name) +
                                          "': '" + value->token.text + "'.",
                                      "not a keyword the property defines",
                                      std::string("Write one of: ") + definition.keywords + ".",
                                      value->token.text);
                    return false;
                }

                keywords.push_back(CSSValue::MakeKeyword(name));
            }

            if (keywords.size() == 1)
            {
                out = keywords[0];
                return true;
            }

            // `normal` and `none` say "nothing here", so they cannot be combined
            // with the keyword that would say something.
            for (const CSSValue& keyword : keywords)
            {
                if (keyword.text == "normal" || keyword.text == "none")
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                      location,
                                      "Invalid value for '" + std::string(definition.name) + "'.",
                                      "'" + keyword.text + "' cannot be combined with another keyword",
                                      "Write '" + keyword.text + "' on its own.",
                                      ComponentsToText(components));
                    return false;
                }
            }

            out = CSSValue();
            out.kind = CSSValueKind::List;
            out.items = std::move(keywords);
            out.location = location;
            return true;
        }

        /// `auto | from-font | <length-percentage>`. `auto` and `from-font` are
        /// keywords, so they cannot ride the length parser; everything else can.
        bool ParseTextDecorationThicknessValue(const CSSPropertyDefinition& definition,
                                               const std::vector<CSSComponentValue>& components,
                                               CSSValue& out,
                                               const CSSSourceLocation& location,
                                               CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident)
            {
                const std::string name = LowerCase(values[0]->token.text);

                if (name == "auto" || name == "from-font")
                {
                    out = CSSValue::MakeKeyword(name);
                    return true;
                }
            }

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, true, definition.name))
            {
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, definition.name);
        }

        /// `<length-percentage> && hanging? && each-line?`. The length is
        /// required and either keyword may sit on either side of it. When a
        /// keyword is present the value is kept as a list, so the lowerer can
        /// read the indent and the mode together; a bare length is returned as
        /// the length itself, which is what the common case lowers to.
        bool ParseTextIndentValue(const CSSPropertyDefinition& definition,
                                  const std::vector<CSSComponentValue>& components,
                                  CSSValue& out,
                                  const CSSSourceLocation& location,
                                  CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "text-indent needs a length or percentage.", "no value given",
                                  "Write '2em', '10%' or '-3em hanging'.", definition.name);
                return false;
            }

            bool hanging = false;
            bool eachLine = false;
            bool hasLength = false;
            CSSValue length;

            for (const CSSComponentValue* value : values)
            {
                if (value->token.type == CSSTokenType::Ident)
                {
                    const std::string name = LowerCase(value->token.text);

                    if (name == "hanging")
                    {
                        if (hanging)
                        {
                            diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                              "text-indent names 'hanging' twice.",
                                              "a keyword may appear only once",
                                              "Write 'hanging' once.", definition.name);
                            return false;
                        }

                        hanging = true;
                        continue;
                    }

                    if (name == "each-line")
                    {
                        if (eachLine)
                        {
                            diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                              "text-indent names 'each-line' twice.",
                                              "a keyword may appear only once",
                                              "Write 'each-line' once.", definition.name);
                            return false;
                        }

                        eachLine = true;
                        continue;
                    }

                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "Unknown text-indent keyword: '" + value->token.text + "'.",
                                      "the value is a length, optionally with 'hanging' and "
                                      "'each-line'",
                                      "Write '2em', '10%' or '-3em hanging each-line'.",
                                      value->token.text);
                    return false;
                }

                if (hasLength)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                      "text-indent takes a single length or percentage.",
                                      "a second length was given",
                                      "Write one length, optionally followed by 'hanging' and "
                                      "'each-line'.",
                                      definition.name);
                    return false;
                }

                std::vector<CSSComponentValue> single{ *value };

                if (!ParseLengthPercentage(single, length, location, diagnostics, false, true,
                                           definition.name))
                {
                    return false;
                }

                hasLength = true;
            }

            if (!hasLength)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                  "text-indent needs a length or percentage.",
                                  "only keywords were given",
                                  "Write '2em hanging', not 'hanging' on its own.", definition.name);
                return false;
            }

            if (!hanging && !eachLine)
            {
                out = length;
                return true;
            }

            out = CSSValue();
            out.kind = CSSValueKind::List;
            out.items.push_back(length);

            if (hanging)
            {
                out.items.push_back(CSSValue::MakeKeyword("hanging"));
            }

            if (eachLine)
            {
                out.items.push_back(CSSValue::MakeKeyword("each-line"));
            }

            out.location = location;
            return true;
        }

        /// `<number> | <length>`. A number is a count of space advances, which
        /// is how the reference writes a tab stop.
        bool ParseTabSizeValue(const std::vector<CSSComponentValue>& components,
                               CSSValue& out,
                               const CSSSourceLocation& location,
                               CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() != 1)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "tab-size takes a single number or length.",
                                  std::to_string(values.size()) + " values given",
                                  "Write '4' or '2rem'.", "tab-size");
                return false;
            }

            const CSSComponentValue& value = *values[0];

            if (value.token.type == CSSTokenType::Number)
            {
                if (value.token.number < 0.0)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "tab-size cannot be negative.", "a tab stop is a width of at least zero",
                                      "Write a positive number of spaces.", "tab-size");
                    return false;
                }

                out = CSSValue::MakeNumber(value.token.number, true);
                return true;
            }

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, true, "tab-size"))
            {
                return false;
            }

            if (out.kind == CSSValueKind::Percentage)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                  "tab-size does not take a percentage.",
                                  "a tab stop is a count of spaces or a length",
                                  "Write '4' or '2rem'.", "tab-size");
                return false;
            }

            return true;
        }

        bool ParseFontFeatureSettingsValue(const std::vector<CSSComponentValue>& components,
                                           CSSValue& out,
                                           const CSSSourceLocation& location,
                                           CSSDiagnosticSink& diagnostics)
        {
            const std::vector<const CSSComponentValue*> values = Significant(components);

            if (values.size() == 1 && values[0]->token.type == CSSTokenType::Ident &&
                LowerCase(values[0]->token.text) == "normal")
            {
                out = CSSValue::MakeKeyword("normal");
                return true;
            }

            // The value is a comma separated list, so the separators are part of
            // it and the raw components are walked rather than the significant
            // ones. Each entry is `"tag"` or `"tag" <number>`; a bare run of
            // letters is accepted too, because that is how the vocabulary spells
            // the CSS 2.1 font-feature keywords.
            std::vector<CSSValue> settings;
            std::vector<CSSValue> pending;

            const auto flush = [&]() {
                if (pending.empty())
                {
                    return true;
                }

                if (pending.size() == 1)
                {
                    settings.push_back(pending[0]);
                }
                else
                {
                    CSSValue entry;
                    entry.kind = CSSValueKind::List;
                    entry.items = pending;
                    entry.location = location;
                    settings.push_back(std::move(entry));
                }

                pending.clear();
                return true;
            };

            for (const CSSComponentValue& component : components)
            {
                if (component.token.type == CSSTokenType::Whitespace ||
                    component.token.type == CSSTokenType::EndOfFile)
                {
                    continue;
                }

                if (component.token.type == CSSTokenType::Comma)
                {
                    if (pending.empty())
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                          "Expected a feature tag before the comma.",
                                          "an empty entry is not a font feature setting",
                                          "Write \"liga\" 1, \"kern\" 0.", "font-feature-settings");
                        return false;
                    }

                    flush();
                    continue;
                }

                if (pending.size() >= 2)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "Too many components in a font feature setting.",
                                      "a setting is a tag and at most one number",
                                      "Write \"liga\" 1, \"kern\" 0.",                        "font-feature-settings");
                    return false;
                }

                if (pending.empty())
                {
                    if (component.token.type != CSSTokenType::QuotedString &&
                        component.token.type != CSSTokenType::Ident)
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                          "Expected a quoted feature tag.",
                                          "a font feature tag is a four character string",
                                          "Write \"liga\" 1, \"kern\" 0.", "font-feature-settings");
                        return false;
                    }

                    pending.push_back(CSSValue::MakeString(component.token.text));
                    continue;
                }

                if (component.token.type != CSSTokenType::Number)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "Expected the feature's value number.",
                                      "a setting is a tag and at most one number",
                                      "Write \"liga\" 1, \"kern\" 0.", "font-feature-settings");
                    return false;
                }

                pending.push_back(CSSValue::MakeNumber(component.token.number, component.token.isInteger));
            }

            if (pending.size() == 1 && pending[0].kind == CSSValueKind::String)
            {
                // `font-feature-settings: "liga"` is not enough: a setting needs
                // its value, and a tag on its own is a syntax error in CSS too.
                diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                  "Expected the feature's value number after '" + pending[0].text + "'.",
                                  "a setting is a tag and its number",
                                  "Write \"liga\" 1, \"kern\" 0.", "font-feature-settings");
                return false;
            }

            flush();

            if (settings.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "Expected a font feature setting.", "no value given",
                                  "Write \"liga\" 1, or 'normal'.", "font-feature-settings");
                return false;
            }

            out = CSSValue();
            out.kind = CSSValueKind::CommaList;
            out.items = std::move(settings);
            out.location = location;
            return true;
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

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, false, "font-size"))
            {
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, "a font size");
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

            if (RejectNegativeValue(resolved, location, diagnostics, "a border radius"))
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

        /// `font-family`: a comma separated list of names, each of which is a
        /// run of identifiers (`Segoe UI`) or one quoted string (`"Times New
        /// Roman"`). The generic families are ordinary identifiers, so they need
        /// no special case here; the runtime is what decides that `monospace`
        /// means the family it has.
        bool ParseFontFamilyValue(const std::vector<CSSComponentValue>& components,
                                  CSSValue& out,
                                  const CSSSourceLocation& location,
                                  CSSDiagnosticSink& diagnostics)
        {
            // The separators *are* the value here, so the commas survive: this is
            // the one parser that cannot use `Significant`, which drops them.
            std::vector<const CSSComponentValue*> values;

            for (const CSSComponentValue& component : components)
            {
                if (component.token.type != CSSTokenType::Whitespace &&
                    component.token.type != CSSTokenType::EndOfFile)
                {
                    values.push_back(&component);
                }
            }

            if (values.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "Expected a font family name.", ComponentsToText(components),
                                  "Write a family name, or a comma separated list of them.", "font-family");
                return false;
            }

            std::vector<CSSValue> families;
            std::string current;
            bool pending = false;

            for (const CSSComponentValue* value : values)
            {
                if (value->token.type == CSSTokenType::Comma)
                {
                    if (!pending)
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                          "Empty font family in the list.", ComponentsToText(components),
                                          "Remove the doubled comma.", "font-family");
                        return false;
                    }

                    families.push_back(CSSValue::MakeIdent(current));
                    current.clear();
                    pending = false;
                    continue;
                }

                if (value->token.type == CSSTokenType::Ident ||
                    value->token.type == CSSTokenType::QuotedString)
                {
                    if (pending)
                    {
                        current += ' ';
                    }
                    current += value->token.text;
                    pending = true;
                    continue;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "Expected a font family name.", ComponentsToText(components),
                                  "Write a family name, or a comma separated list of them.", "font-family");
                return false;
            }

            if (!pending)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidGrammar, location,
                                  "Expected a font family name.", ComponentsToText(components),
                                  "Write a family name, or a comma separated list of them.", "font-family");
                return false;
            }

            families.push_back(CSSValue::MakeIdent(current));

            out = CSSValue();
            out.kind = CSSValueKind::CommaList;
            out.location = location;
            out.items = std::move(families);
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
        case CSSValueGrammar::FontFamily:
            return ParseFontFamilyValue(components, out, location, diagnostics);

        case CSSValueGrammar::Size:
            return ParseSizeValue(components, out, location, diagnostics);

        case CSSValueGrammar::LengthPercentageOrAuto:
            return ParseLengthPercentage(components, out, location, diagnostics, true, true, definition.name);

        case CSSValueGrammar::LengthPercentage:
            return ParseLengthPercentage(components, out, location, diagnostics, false, true, definition.name);

        case CSSValueGrammar::NonNegativeLengthPercentage:
        {
            if (!ParseLengthPercentage(components, out, location, diagnostics, false, true, definition.name))
            {
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, definition.name);
        }

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

        case CSSValueGrammar::LengthPercentageOrNormal:
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

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, true, definition.name))
            {
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, definition.name);
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
                if (values[0]->token.number < 0.0)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidValue, location,
                                      "line-height cannot be negative.",
                                      "a negative line height is not valid",
                                      "Write a number or length of zero or more.", "line-height");
                    return false;
                }

                out = CSSValue::MakeNumber(values[0]->token.number);
                return true;
            }

            if (!ParseLengthPercentage(components, out, location, diagnostics, false, false, "line-height"))
            {
                return false;
            }

            return !RejectNegativeValue(out, location, diagnostics, "a line height");
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

        case CSSValueGrammar::WhiteSpace:
            return ParseWhiteSpaceValue(components, out, location, diagnostics);

        case CSSValueGrammar::ZIndex:
            return ParseZIndexValue(components, out, location, diagnostics);

        case CSSValueGrammar::Number:
        {
            if (!ParseNumberValue(components, out, location, diagnostics))
            {
                return false;
            }

            // `Number` is the grammar of `flex-grow` and `flex-shrink`, both of
            // which are a number of zero or more.
            return !RejectNegativeValue(out, location, diagnostics, definition.name);
        }

        case CSSValueGrammar::Integer:
            return ParseIntegerValue(components, out, location, diagnostics);

        case CSSValueGrammar::TextAlign:
            return ParseTextAlignValue(components, out, location, diagnostics);

        case CSSValueGrammar::TextDecorationLine:
            return ParseTextDecorationLineValue(components, out, location, diagnostics);

        case CSSValueGrammar::TextIndent:
            return ParseTextIndentValue(definition, components, out, location, diagnostics);

        case CSSValueGrammar::FontFeatureSettings:
            return ParseFontFeatureSettingsValue(components, out, location, diagnostics);

        case CSSValueGrammar::KeywordSetList:
            return ParseKeywordSetListValue(definition, components, out, location, diagnostics);

        case CSSValueGrammar::TextDecorationThickness:
            return ParseTextDecorationThicknessValue(definition, components, out, location, diagnostics);

        case CSSValueGrammar::TabSize:
            return ParseTabSizeValue(components, out, location, diagnostics);

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
