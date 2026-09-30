#include "RoboHTMLCompiler/CssStyleBuilder.h"

#include "RoboCSSCompiler/CssParser.h"
#include "RoboCSSCompiler/CssValueParser.h"
#include "RoboHTMLCompiler/Ast.h"
#include "RoboRenderer/CompiledUi.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>

namespace RoboHTMLCompiler
{
    namespace
    {
        using RoboCSSCompiler::CSSColor;
        using RoboCSSCompiler::CSSColorKind;
        using RoboCSSCompiler::CSSComputedStyle;
        using RoboCSSCompiler::CSSElementNode;
        using RoboCSSCompiler::CSSPropertyId;
        using RoboCSSCompiler::CSSUnit;
        using RoboCSSCompiler::CSSValue;
        using RoboCSSCompiler::CSSValueKind;

        using RoboRenderer::CompiledStyle;
        using RoboRenderer::DisplayKind;
        using RoboRenderer::FontWeightKind;
        using RoboRenderer::LayoutValue;
        using RoboRenderer::LayoutValueType;
        using RoboRenderer::OverflowKind;
        using RoboRenderer::StyleFlagBackground;
        using RoboRenderer::StyleFlagBorderBottomWidth;
        using RoboRenderer::StyleFlagBorderColor;
        using RoboRenderer::StyleFlagBorderLeftWidth;
        using RoboRenderer::StyleFlagBorderRadius;
        using RoboRenderer::StyleFlagBorderRightWidth;
        using RoboRenderer::StyleFlagBorderTopWidth;
        using RoboRenderer::StyleFlagDisplay;
        using RoboRenderer::StyleFlagFontSize;
        using RoboRenderer::StyleFlagFontWeight;
        using RoboRenderer::StyleFlagForeground;
        using RoboRenderer::StyleFlagHeight;
        using RoboRenderer::StyleFlagLetterSpacing;
        using RoboRenderer::StyleFlagMarginBottom;
        using RoboRenderer::StyleFlagMarginLeft;
        using RoboRenderer::StyleFlagMarginRight;
        using RoboRenderer::StyleFlagMarginTop;
        using RoboRenderer::StyleFlagMinimumHeight;
        using RoboRenderer::StyleFlagMinimumWidth;
        using RoboRenderer::StyleFlagOverflowX;
        using RoboRenderer::StyleFlagOverflowY;
        using RoboRenderer::StyleFlagPaddingBottom;
        using RoboRenderer::StyleFlagPaddingLeft;
        using RoboRenderer::StyleFlagPaddingRight;
        using RoboRenderer::StyleFlagPaddingTop;
        using RoboRenderer::StyleFlagTextAlign;
        using RoboRenderer::StyleFlagWidth;
        using RoboRenderer::TextAlignKind;

        std::string LowerCase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](char character) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            });
            return value;
        }

        std::vector<std::string> SplitWhitespace(const std::string& value)
        {
            std::vector<std::string> tokens;
            std::string token;

            for (std::size_t index = 0; index <= value.size(); ++index)
            {
                const bool boundary = index == value.size() ||
                                      std::isspace(static_cast<unsigned char>(value[index])) != 0;

                if (boundary)
                {
                    if (!token.empty())
                    {
                        tokens.push_back(token);
                    }

                    token.clear();
                    continue;
                }

                token += value[index];
            }

            return tokens;
        }

        const char* NamespaceName(MarkupNamespace markupNamespace)
        {
            switch (markupNamespace)
            {
            case MarkupNamespace::Html: return "html";
            case MarkupNamespace::Svg: return "svg";
            case MarkupNamespace::MathMl: return "mathml";
            }

            return "html";
        }

        void WarnAboutStyle(DiagnosticSink& diagnostics,
                            const SourceLocation& location,
                            std::string message,
                            std::string detail,
                            std::string suggestion,
                            std::string subject);

        /// A length in logical units, or -1 when this compiler cannot resolve it.
        struct LengthResolution
        {
            float value = -1.0f;
            /// The unit's name, for a diagnostic that has to name what it could
            /// not resolve.
            std::string unit;
        };

        /// The element's own font size, which `em`, `ex` and `ch` are relative to.
        float FontSizeOf(const CSSComputedStyle* style, float fallback)
        {
            if (style == nullptr)
            {
                return fallback;
            }

            const CSSValue* fontSize = style->Get(CSSPropertyId::FontSize);

            if (fontSize == nullptr)
            {
                return fallback;
            }

            float absolute = 0.0f;

            if (fontSize->IsAbsoluteLength(absolute))
            {
                return absolute;
            }

            return fallback;
        }

        /// Resolves a length that the compiler can resolve: absolute units, and
        /// the font-relative units whose context is known during compilation.
        ///
        /// `rem` is the root font size, which the document declares. `em` is the
        /// element's own font size, which the cascade has already computed -
        /// resolving it here is not an approximation, it is what CSS calls the
        /// computed value. Percentages, viewport units and the font metrics the
        /// runtime owns (`ex`, `ch`, `lh`) are left to the caller to report.
        LengthResolution ResolveLength(const CSSValue& value,
                                       const CssLengthContext& context,
                                       float elementFontSize,
                                       const char* property,
                                       DiagnosticSink& diagnostics,
                                       const SourceLocation& location)
        {
            LengthResolution result;

            if (value.kind == CSSValueKind::Dimension)
            {
                result.unit = RoboCSSCompiler::UnitName(value.unit);

                switch (value.unit)
                {
                case CSSUnit::Em:
                    result.value = static_cast<float>(value.number) * elementFontSize;
                    return result;

                case CSSUnit::Rem:
                    result.value = static_cast<float>(value.number) * context.rootFontSize;
                    return result;

                case CSSUnit::Ex:
                case CSSUnit::Ch:
                case CSSUnit::Cap:
                case CSSUnit::Ic:
                case CSSUnit::Lh:
                case CSSUnit::Rlh:
                    // These need the font's own metrics, which the runtime owns.
                    WarnAboutStyle(diagnostics,
                                        location,
                                        std::string(property) + ": the unit '" + result.unit +
                                            "' needs font metrics the compiler does not have.",
                                        "the native text engine resolves x-height, character advance and line "
                                        "height at run time",
                                        "Use px or rem, or write the value the metric would produce.",
                                        property);
                    return result;

                case CSSUnit::Vw:
                case CSSUnit::Vh:
                case CSSUnit::Vmin:
                case CSSUnit::Vmax:
                case CSSUnit::Vb:
                case CSSUnit::Vi:
                case CSSUnit::Svw:
                case CSSUnit::Svh:
                case CSSUnit::Svmin:
                case CSSUnit::Svmax:
                case CSSUnit::Svb:
                case CSSUnit::Svi:
                case CSSUnit::Lvw:
                case CSSUnit::Lvh:
                case CSSUnit::Lvmin:
                case CSSUnit::Lvmax:
                case CSSUnit::Lvb:
                case CSSUnit::Lvi:
                case CSSUnit::Dvw:
                case CSSUnit::Dvh:
                case CSSUnit::Dvmin:
                case CSSUnit::Dvmax:
                case CSSUnit::Dvb:
                case CSSUnit::Dvi:
                    // Viewport units in a *sized* property are representable: the
                    // caller stores the fraction so the runtime resolves it against
                    // the window it actually has.
                    result.value = static_cast<float>(value.number);
                    return result;

                case CSSUnit::Cqw:
                case CSSUnit::Cqh:
                case CSSUnit::Cqi:
                case CSSUnit::Cqb:
                case CSSUnit::Cqmin:
                case CSSUnit::Cqmax:
                    WarnAboutStyle(diagnostics,
                                        location,
                                        std::string(property) + ": '" + result.unit +
                                            "' needs a container, which this runtime has no concept of.",
                                        "container query units resolve against a query container's size",
                                        "Use px, rem or a percentage.",
                                        property);
                    return result;

                default:
                    break;
                }

                if (RoboCSSCompiler::IsAbsoluteUnit(value.unit))
                {
                    result.value = static_cast<float>(value.number * RoboCSSCompiler::AbsoluteUnitScale(value.unit));
                    return result;
                }

                WarnAboutStyle(diagnostics,
                                    location,
                                    std::string(property) + ": unknown unit '" + result.unit + "'.",
                                    "the unit is not one this compiler can resolve",
                                    "Use px, rem or a percentage.",
                                    property);
                return result;
            }

            if (value.kind == CSSValueKind::Number && value.number == 0.0)
            {
                // Zero is the one length a stylesheet may write without a unit.
                result.value = 0.0f;
                result.unit = "0";
                return result;
            }

            if (value.kind == CSSValueKind::Math)
            {
                // The value parser folds a program whose operands are all
                // absolute; one that reaches here mixes dimensions.
                WarnAboutStyle(diagnostics,
                                    location,
                                    std::string(property) + ": the expression mixes dimensions this runtime "
                                                           "cannot combine.",
                                    "a native length carries one dimension and a percentage is not a length "
                                    "until layout resolves it",
                                    "Use a single unit, or split the declaration.",
                                    property);
                return result;
            }

            WarnAboutStyle(diagnostics,
                                location,
                                std::string(property) + ": this value is not a length.",
                                "expected a length in logical units",
                                "Write a length such as 1rem or 8px.",
                                property);
            return result;
        }

        /// True for a viewport unit, which the caller stores as a fraction.
        bool IsViewportUnit(CSSUnit unit)
        {
            switch (unit)
            {
            case CSSUnit::Vw:
            case CSSUnit::Vh:
            case CSSUnit::Vmin:
            case CSSUnit::Vmax:
            case CSSUnit::Vb:
            case CSSUnit::Vi:
            case CSSUnit::Svw:
            case CSSUnit::Svh:
            case CSSUnit::Svmin:
            case CSSUnit::Svmax:
            case CSSUnit::Svb:
            case CSSUnit::Svi:
            case CSSUnit::Lvw:
            case CSSUnit::Lvh:
            case CSSUnit::Lvmin:
            case CSSUnit::Lvmax:
            case CSSUnit::Lvb:
            case CSSUnit::Lvi:
            case CSSUnit::Dvw:
            case CSSUnit::Dvh:
            case CSSUnit::Dvmin:
            case CSSUnit::Dvmax:
            case CSSUnit::Dvb:
            case CSSUnit::Dvi:
                return true;

            default:
                return false;
            }
        }

        /// The fraction of the viewport a `vw`-family unit denotes, or -1 when the
        /// unit is not one of them.
        float ViewportFraction(CSSUnit unit, float number, const CssLengthContext& context)
        {
            const float width = context.viewportWidth > 0.0f ? context.viewportWidth : 1.0f;
            const float height = context.viewportHeight > 0.0f ? context.viewportHeight : 1.0f;
            const float minimum = std::min(width, height);
            const float maximum = std::max(width, height);

            switch (unit)
            {
            case CSSUnit::Vw:
            case CSSUnit::Svw:
            case CSSUnit::Lvw:
            case CSSUnit::Dvw:
            case CSSUnit::Vb:
            case CSSUnit::Svb:
            case CSSUnit::Lvb:
            case CSSUnit::Dvb:
                return number / 100.0f;

            case CSSUnit::Vh:
            case CSSUnit::Svh:
            case CSSUnit::Lvh:
            case CSSUnit::Dvh:
            case CSSUnit::Vi:
            case CSSUnit::Svi:
            case CSSUnit::Lvi:
            case CSSUnit::Dvi:
                return number / 100.0f * (height / width);

            case CSSUnit::Vmin:
            case CSSUnit::Svmin:
            case CSSUnit::Lvmin:
            case CSSUnit::Dvmin:
                return number / 100.0f * (minimum / width);

            case CSSUnit::Vmax:
            case CSSUnit::Svmax:
            case CSSUnit::Lvmax:
            case CSSUnit::Dvmax:
                return number / 100.0f * (maximum / width);

            default:
                return -1.0f;
            }
        }

        /// The native colour of a computed CSS colour. `currentColor` is resolved
        /// by the caller against the element's computed `color`.
        RoboRenderer::Color ToNativeColor(const CSSColor& color)
        {
            const auto channel = [](float value) {
                const float clamped = value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value);
                return static_cast<std::uint8_t>(clamped + 0.5f);
            };

            const float alpha = color.alpha < 0.0f ? 0.0f : (color.alpha > 1.0f ? 1.0f : color.alpha);

            return RoboRenderer::Color{ channel(color.red), channel(color.green), channel(color.blue),
                                        static_cast<std::uint8_t>(alpha * 255.0f + 0.5f) };
        }

        /// The compiler's warning form has no `found` field, so the subject
        /// becomes part of the sentence rather than being dropped.
        void WarnAboutStyle(DiagnosticSink& diagnostics,
                            const SourceLocation& location,
                            std::string message,
                            std::string detail,
                            std::string suggestion,
                            std::string subject)
        {
            if (!subject.empty())
            {
                message += " (" + subject + ")";
            }

            diagnostics.Warning(DiagnosticCode::MalformedStyleSheet, location, std::move(message),
                                std::move(detail), std::move(suggestion));
        }

        /// The lowerer. One instance per compilation, holding the diagnostic sink
        /// and the context every value is resolved against.
        class StyleLowerer
        {
        public:
            StyleLowerer(DiagnosticSink& diagnosticSink, const CssLengthContext& lengthContext)
                : diagnostics(diagnosticSink)
                , context(lengthContext)
            {
            }

            /// Lowers one computed style. Returns false when the style is empty,
            /// which means the element keeps the native default for its category.
            bool Lower(const CSSComputedStyle& computed,
                       const SourceLocation& location,
                       CompiledStyle& out)
            {
                out = CompiledStyle{};
                out.specified = 0u;

                const float elementFontSize = FontSizeOf(&computed, context.rootFontSize);

                LowerDisplay(computed, location, out);
                LowerSize(computed, CSSPropertyId::Width, StyleFlagWidth, location, elementFontSize, out.width, out);
                LowerSize(computed, CSSPropertyId::Height, StyleFlagHeight, location, elementFontSize, out.height,
                          out);
                LowerSize(computed, CSSPropertyId::MinWidth, StyleFlagMinimumWidth, location, elementFontSize,
                          out.minimumWidth, out);
                LowerSize(computed, CSSPropertyId::MinHeight, StyleFlagMinimumHeight, location, elementFontSize,
                          out.minimumHeight, out);

                // A maximum size has no field in the compiled style, so a
                // declaration that sets one is reported rather than dropped.
                for (CSSPropertyId maximum : { CSSPropertyId::MaxWidth, CSSPropertyId::MaxHeight })
                {
                    if (Value(computed, maximum) != nullptr)
                    {
                        ReportNotLowered(maximum, location,
                                         "the native box model carries a minimum size and no maximum",
                                         "Remove the declaration, or express the bound as a minimum on the "
                                         "content.");
                    }
                }

                LowerEdges(computed, CSSPropertyId::MarginTop, CSSPropertyId::MarginRight,
                           CSSPropertyId::MarginBottom, CSSPropertyId::MarginLeft, StyleFlagMarginTop,
                           StyleFlagMarginRight, StyleFlagMarginBottom, StyleFlagMarginLeft, "margin", location,
                           elementFontSize, out.margin, out);

                LowerEdges(computed, CSSPropertyId::PaddingTop, CSSPropertyId::PaddingRight,
                           CSSPropertyId::PaddingBottom, CSSPropertyId::PaddingLeft, StyleFlagPaddingTop,
                           StyleFlagPaddingRight, StyleFlagPaddingBottom, StyleFlagPaddingLeft, "padding",
                           location, elementFontSize, out.padding, out);

                LowerBorders(computed, location, elementFontSize, out);
                LowerBackground(computed, location, out);
                LowerText(computed, location, elementFontSize, out);
                LowerOverflow(computed, location, out);

                return out.specified != 0u;
            }

        private:
            const CSSValue* Value(const CSSComputedStyle& computed, CSSPropertyId id) const
            {
                return computed.Get(id);
            }

            /// Reports a feature the compiler parsed and computed but cannot put
            /// into the native style table.
            void ReportNotLowered(CSSPropertyId id, const SourceLocation& location, const char* reason,
                                  const char* suggestion)
            {
                const RoboCSSCompiler::CSSPropertyDefinition* definition =
                    RoboCSSCompiler::CSSPropertyRegistry::Instance().Find(id);
                const char* name = definition != nullptr ? definition->name : "property";

                WarnAboutStyle(diagnostics,
                                    location,
                                    std::string("CSS property '") + name + "' is compiled but not applied.",
                                    reason,
                                    suggestion,
                                    name);
            }

            void LowerDisplay(const CSSComputedStyle& computed, const SourceLocation& location, CompiledStyle& out)
            {
                const CSSValue* value = Value(computed, CSSPropertyId::Display);

                if (value == nullptr)
                {
                    return;
                }

                if (value->text == "block")
                {
                    out.display = DisplayKind::Block;
                }
                else if (value->text == "inline")
                {
                    out.display = DisplayKind::Inline;
                }
                else if (value->text == "inline-block")
                {
                    out.display = DisplayKind::InlineBlock;
                }
                else if (value->text == "none")
                {
                    out.display = DisplayKind::Hidden;
                }
                else
                {
                    ReportNotLowered(CSSPropertyId::Display, location, "the native layout engine has four display "
                                                                      "kinds",
                                     "Use block, inline, inline-block or none.");
                    return;
                }

                out.specified |= StyleFlagDisplay;
            }

            void LowerSize(const CSSComputedStyle& computed,
                           CSSPropertyId id,
                           std::uint32_t flag,
                           const SourceLocation& location,
                           float elementFontSize,
                           LayoutValue& target,
                           CompiledStyle& out)
            {
                const CSSValue* value = Value(computed, id);

                if (value == nullptr)
                {
                    return;
                }

                if (value->kind == CSSValueKind::Keyword)
                {
                    if (value->text == "auto")
                    {
                        target = LayoutValue::Auto();
                    }
                    else if (value->text == "none")
                    {
                        // `max-width: none` and `height: none` both mean "no
                        // bound", which for a native box is the content size.
                        target = LayoutValue::Content();
                    }
                    else if (value->text == "min-content" || value->text == "max-content")
                    {
                        target = LayoutValue::Content();
                    }
                    else if (value->text == "fit-content")
                    {
                        target = LayoutValue::Content();
                    }
                    else
                    {
                        ReportNotLowered(id, location, "the native layout engine sizes from a length or the "
                                                       "content",
                                         "Use a length, auto or none.");
                        return;
                    }

                    out.specified |= flag;
                    return;
                }

                if (value->kind == CSSValueKind::Percentage)
                {
                    // A percentage of the containing block is not a length until
                    // layout knows the containing block, so the fraction is stored
                    // and resolved by the engine.
                    target = LayoutValue{ static_cast<float>(value->number) / 100.0f,
                                          LayoutValueType::ParentRelative };
                    out.specified |= flag;
                    return;
                }

                if (value->kind == CSSValueKind::Dimension && IsViewportUnit(value->unit))
                {
                    const float fraction = ViewportFraction(value->unit, static_cast<float>(value->number), context);

                    if (fraction < 0.0f)
                    {
                        ReportNotLowered(id, location, "this viewport unit has no native equivalent",
                                         "Use vw, vh, vmin, vmax or a length.");
                        return;
                    }

                    target = LayoutValue{ fraction, LayoutValueType::ViewportRelative };
                    out.specified |= flag;
                    return;
                }

                const LengthResolution resolved =
                    ResolveLength(*value, context, elementFontSize, PropertyName(id), diagnostics, location);

                if (resolved.value < 0.0f)
                {
                    return;
                }

                target = LayoutValue::Fixed(resolved.value);
                out.specified |= flag;
            }

            void LowerEdges(const CSSComputedStyle& computed,
                            CSSPropertyId top,
                            CSSPropertyId right,
                            CSSPropertyId bottom,
                            CSSPropertyId left,
                            std::uint32_t topFlag,
                            std::uint32_t rightFlag,
                            std::uint32_t bottomFlag,
                            std::uint32_t leftFlag,
                            const char* subject,
                            const SourceLocation& location,
                            float elementFontSize,
                            RoboRenderer::Edges& target,
                            CompiledStyle& out)
            {
                const CSSPropertyId ids[4] = { top, right, bottom, left };
                const std::uint32_t flags[4] = { topFlag, rightFlag, bottomFlag, leftFlag };
                float* edges[4] = { &target.top, &target.right, &target.bottom, &target.left };

                for (int index = 0; index < 4; ++index)
                {
                    const CSSValue* value = Value(computed, ids[index]);

                    if (value == nullptr)
                    {
                        continue;
                    }

                    if (value->kind == CSSValueKind::Keyword && value->text == "auto")
                    {
                        // Auto margins need a distribution algorithm the native
                        // layout engine does not have.
                        WarnAboutStyle(diagnostics,
                                            location,
                                            std::string("CSS '") + subject + "' uses 'auto'.",
                                            "auto margins distribute the containing block's free space, which the "
                                            "native layout engine does not do",
                                            "Use a length, or centre the content another way.",
                                            subject);
                        continue;
                    }

                    if (value->kind == CSSValueKind::Percentage)
                    {
                        WarnAboutStyle(diagnostics,
                                            location,
                                            std::string("CSS '") + subject + "' uses a percentage.",
                                            "a percentage margin or padding resolves against the containing "
                                            "block's width, and the native box model holds resolved lengths",
                                            "Use px or rem.",
                                            subject);
                        continue;
                    }

                    if (value->kind == CSSValueKind::Dimension && IsViewportUnit(value->unit))
                    {
                        WarnAboutStyle(diagnostics,
                                            location,
                                            std::string("CSS '") + subject + "' uses a viewport unit.",
                                            "the native box model holds resolved lengths, and a window can be "
                                            "resized after compilation",
                                            "Use px or rem.",
                                            subject);
                        continue;
                    }

                    const LengthResolution resolved =
                        ResolveLength(*value, context, elementFontSize, subject, diagnostics, location);

                    if (resolved.value < 0.0f)
                    {
                        continue;
                    }

                    *edges[index] = resolved.value;
                    out.specified |= flags[index];
                }
            }

            void LowerBorders(const CSSComputedStyle& computed,
                              const SourceLocation& location,
                              float elementFontSize,
                              CompiledStyle& out)
            {
                const CSSPropertyId widths[4] = { CSSPropertyId::BorderTopWidth, CSSPropertyId::BorderRightWidth,
                                                  CSSPropertyId::BorderBottomWidth,
                                                  CSSPropertyId::BorderLeftWidth };
                const CSSPropertyId styles[4] = { CSSPropertyId::BorderTopStyle, CSSPropertyId::BorderRightStyle,
                                                  CSSPropertyId::BorderBottomStyle,
                                                  CSSPropertyId::BorderLeftStyle };
                const CSSPropertyId colors[4] = { CSSPropertyId::BorderTopColor, CSSPropertyId::BorderRightColor,
                                                  CSSPropertyId::BorderBottomColor,
                                                  CSSPropertyId::BorderLeftColor };
                const std::uint32_t flags[4] = { StyleFlagBorderTopWidth, StyleFlagBorderRightWidth,
                                                 StyleFlagBorderBottomWidth, StyleFlagBorderLeftWidth };
                float* edges[4] = { &out.borderWidth.top, &out.borderWidth.right, &out.borderWidth.bottom,
                                    &out.borderWidth.left };

                bool anyWidth = false;
                bool anyColor = false;

                for (int index = 0; index < 4; ++index)
                {
                    const CSSValue* style = Value(computed, styles[index]);
                    const CSSValue* width = Value(computed, widths[index]);

                    // A border style of none or hidden means no border, and that is
                    // a decision the native painter has to be told about: it draws
                    // a border from the width alone.
                    if (style != nullptr && (style->text == "none" || style->text == "hidden"))
                    {
                        *edges[index] = 0.0f;
                        out.specified |= flags[index];
                        continue;
                    }

                    if (width != nullptr)
                    {
                        const LengthResolution resolved =
                            ResolveLength(*width, context, elementFontSize, "border-width", diagnostics, location);

                        if (resolved.value < 0.0f)
                        {
                            continue;
                        }

                        *edges[index] = resolved.value;
                        out.specified |= flags[index];

                        if (resolved.value > 0.0f)
                        {
                            anyWidth = true;
                        }

                        continue;
                    }

                    if (style != nullptr)
                    {
                        // A style with no width computes to the initial width,
                        // `medium`, exactly as it does in a browser.
                        *edges[index] = 3.0f;
                        out.specified |= flags[index];
                        anyWidth = true;
                    }
                }

                for (int index = 0; index < 4; ++index)
                {
                    const CSSValue* color = Value(computed, colors[index]);

                    if (color == nullptr || color->kind != CSSValueKind::Color)
                    {
                        continue;
                    }

                    if (color->color.kind == CSSColorKind::CurrentColor)
                    {
                        // `border-color: currentColor` is the element's own
                        // colour, which the cascade has already resolved for this
                        // element when the stylesheet set one.
                        const CSSValue* foreground = Value(computed, CSSPropertyId::Color);

                        if (foreground == nullptr || foreground->kind != CSSValueKind::Color ||
                            foreground->color.kind != CSSColorKind::CurrentColor)
                        {
                            WarnAboutStyle(diagnostics,
                                                location,
                                                "A border colour of currentColor cannot be resolved.",
                                                "the element's colour is the native default, which the compiler "
                                                "does not see",
                                                "Set an explicit 'color' on the element or an explicit border "
                                                "colour.",
                                                "border-color");
                            continue;
                        }

                        out.borderColor = ToNativeColor(foreground->color);
                        anyColor = true;
                        continue;
                    }

                    out.borderColor = ToNativeColor(color->color);
                    anyColor = true;
                    break;
                }

                const CSSValue* radius = Value(computed, CSSPropertyId::BorderRadius);

                if (radius != nullptr)
                {
                    if (radius->kind == CSSValueKind::Percentage)
                    {
                        WarnAboutStyle(diagnostics,
                                            location,
                                            "CSS 'border-radius' uses a percentage.",
                                            "a percentage radius resolves against the box's size, and the native "
                                            "radius is a resolved length",
                                            "Use px or rem.",
                                            "border-radius");
                    }
                    else
                    {
                        const LengthResolution resolved = ResolveLength(*radius, context, elementFontSize,
                                                                        "border-radius", diagnostics, location);

                        if (resolved.value >= 0.0f)
                        {
                            out.borderRadius = resolved.value;
                            out.specified |= StyleFlagBorderRadius;
                        }
                    }
                }

                if (anyWidth)
                {
                    out.hasBorder = 1;
                }

                if (anyColor)
                {
                    out.specified |= StyleFlagBorderColor;
                }
            }

            void LowerBackground(const CSSComputedStyle& computed, const SourceLocation& location, CompiledStyle& out)
            {
                const CSSValue* value = Value(computed, CSSPropertyId::BackgroundColor);

                if (value == nullptr || value->kind != CSSValueKind::Color)
                {
                    return;
                }

                if (value->color.kind == CSSColorKind::CurrentColor)
                {
                    WarnAboutStyle(diagnostics,
                                        location,
                                        "CSS 'background-color' is currentColor.",
                                        "the element's colour is the native default, which the compiler does not see",
                                        "Set an explicit colour.",
                                        "background-color");
                    return;
                }

                out.background = ToNativeColor(value->color);
                out.hasBackground = 1;
                out.specified |= StyleFlagBackground;
            }

            void LowerText(const CSSComputedStyle& computed,
                           const SourceLocation& location,
                           float elementFontSize,
                           CompiledStyle& out)
            {
                const CSSValue* color = Value(computed, CSSPropertyId::Color);

                if (color != nullptr && color->kind == CSSValueKind::Color &&
                    color->color.kind != CSSColorKind::CurrentColor)
                {
                    out.foreground = ToNativeColor(color->color);
                    out.hasForeground = 1;
                    out.specified |= StyleFlagForeground;
                }

                const CSSValue* fontSize = Value(computed, CSSPropertyId::FontSize);

                if (fontSize != nullptr)
                {
                    if (fontSize->kind == CSSValueKind::Percentage)
                    {
                        WarnAboutStyle(diagnostics,
                                            location,
                                            "CSS 'font-size' uses a percentage.",
                                            "a percentage font size is relative to the inherited size, which is "
                                            "the native default when the document does not state one",
                                            "Use px or rem.",
                                            "font-size");
                    }
                    else
                    {
                        const LengthResolution resolved = ResolveLength(*fontSize, context, elementFontSize,
                                                                        "font-size", diagnostics, location);

                        if (resolved.value > 0.0f)
                        {
                            out.fontSize = resolved.value;
                            out.hasFontSize = 1;
                            out.specified |= StyleFlagFontSize;
                        }
                    }
                }

                const CSSValue* weight = Value(computed, CSSPropertyId::FontWeight);

                if (weight != nullptr && (weight->kind == CSSValueKind::Keyword || weight->kind == CSSValueKind::Number))
                {
                    const bool bold = weight->text == "bold" || weight->number >= 600.0;
                    out.fontWeight = bold ? FontWeightKind::Bold : FontWeightKind::Normal;
                    out.specified |= StyleFlagFontWeight;
                }

                const CSSValue* spacing = Value(computed, CSSPropertyId::LetterSpacing);

                if (spacing != nullptr)
                {
                    if (spacing->kind == CSSValueKind::Keyword && spacing->text == "normal")
                    {
                        out.letterSpacing = 0.0f;
                        out.specified |= StyleFlagLetterSpacing;
                    }
                    else
                    {
                        const LengthResolution resolved = ResolveLength(*spacing, context, elementFontSize,
                                                                        "letter-spacing", diagnostics, location);

                        if (resolved.value >= 0.0f)
                        {
                            out.letterSpacing = resolved.value;
                            out.specified |= StyleFlagLetterSpacing;
                        }
                    }
                }

                const CSSValue* align = Value(computed, CSSPropertyId::TextAlign);

                if (align != nullptr && align->kind == CSSValueKind::Keyword)
                {
                    if (align->text == "start")
                    {
                        out.textAlign = TextAlignKind::Start;
                    }
                    else if (align->text == "center")
                    {
                        out.textAlign = TextAlignKind::Center;
                    }
                    else if (align->text == "end")
                    {
                        out.textAlign = TextAlignKind::End;
                    }
                    else
                    {
                        return;
                    }

                    out.specified |= StyleFlagTextAlign;
                }

                // `line-height` is reported rather than compiled: the text engine
                // derives a line's height from the font's metrics and the compiled
                // style has no field that overrides it. Compiling the value into a
                // field nothing reads would be support in name only.
                if (Value(computed, CSSPropertyId::LineHeight) != nullptr)
                {
                    ReportNotLowered(CSSPropertyId::LineHeight, location,
                                     "the native text engine derives line height from the font's metrics, and the "
                                     "compiled style has no field that overrides it",
                                     "Set the font size instead, or remove the declaration.");
                }
            }

            void LowerOverflow(const CSSComputedStyle& computed, const SourceLocation& location, CompiledStyle& out)
            {
                const CSSPropertyId ids[2] = { CSSPropertyId::OverflowX, CSSPropertyId::OverflowY };
                const std::uint32_t flags[2] = { StyleFlagOverflowX, StyleFlagOverflowY };
                OverflowKind* kinds[2] = { &out.overflowX, &out.overflowY };

                for (int index = 0; index < 2; ++index)
                {
                    const CSSValue* value = Value(computed, ids[index]);

                    if (value == nullptr || value->kind != CSSValueKind::Keyword)
                    {
                        continue;
                    }

                    if (value->text == "visible")
                    {
                        *kinds[index] = OverflowKind::Visible;
                    }
                    else if (value->text == "hidden" || value->text == "clip")
                    {
                        *kinds[index] = OverflowKind::Clip;
                    }
                    else if (value->text == "scroll" || value->text == "auto")
                    {
                        // `auto` is a scroll container that shows its scrollbars
                        // only when needed; the native runtime reveals its
                        // scrollbars on interaction, which is the same behaviour.
                        *kinds[index] = OverflowKind::Scroll;
                    }
                    else
                    {
                        continue;
                    }

                    out.specified |= flags[index];
                }

                (void)location;
            }

            static const char* PropertyName(CSSPropertyId id)
            {
                const RoboCSSCompiler::CSSPropertyDefinition* definition =
                    RoboCSSCompiler::CSSPropertyRegistry::Instance().Find(id);
                return definition != nullptr ? definition->name : "property";
            }

            DiagnosticSink& diagnostics;
            CssLengthContext context;
        };

        /// Builds the selector's view of one element.
        void BuildElementView(const ElementNode& source,
                              CSSElementNode& target,
                              std::vector<std::unique_ptr<CSSElementNode>>& pool)
        {
            target.tagName = LowerCase(source.sourceTag);
            target.namespaceName = NamespaceName(source.ns);
            target.textContent = source.DirectTextContent();

            for (const AttributeNode& attribute : source.attributes)
            {
                if (attribute.name == "id")
                {
                    target.id = attribute.value;
                }
                else if (attribute.name == "class")
                {
                    target.classes = SplitWhitespace(attribute.value);
                }

                target.attributes.emplace_back(attribute.name, attribute.value);
            }

            // The state the document itself declares. Anything that depends on the
            // user - hover, focus, press - is not a compile-time fact, which is why
            // a rule that needs it is reported rather than applied.
            const bool disabled = source.HasAttribute("disabled");
            target.state.disabled = disabled;
            target.state.enabled = !disabled;
            target.state.readOnly = source.HasAttribute("readonly");
            target.state.required = source.HasAttribute("required");
            target.state.open = source.HasAttribute("open");
            target.state.checked = source.HasAttribute("checked") || source.HasAttribute("selected");

            if (target.tagName == "a")
            {
                target.state.link = source.HasAttribute("href");
            }

            for (const std::unique_ptr<Node>& child : source.children)
            {
                if (child->kind != NodeKind::Element)
                {
                    continue;
                }

                const ElementNode& childElement = static_cast<const ElementNode&>(*child);

                pool.push_back(std::make_unique<CSSElementNode>());
                CSSElementNode& childView = *pool.back();
                childView.parent = &target;
                BuildElementView(childElement, childView, pool);
                target.children.push_back(&childView);
            }
        }
    }

    CssStyleBuilder::CssStyleBuilder(DiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
    }

    bool CssStyleBuilder::Build(const MarkupDocument& document,
                                const std::vector<LoadedStyleSheet>& sheets,
                                const CssLengthContext& context,
                                CssStyleTable& out)
    {
        out.styles.clear();
        out.indexBySourcePosition.clear();

        // Index 0 stays empty: a style index of 0 is the compiled format's way of
        // saying "the native default", so the authored table starts at 1.
        out.styles.emplace_back(CompiledStyle{});

        if (document.root == nullptr || document.root->children.empty())
        {
            return true;
        }

        // --- the element view -------------------------------------------------
        // The cascade starts at the document element, exactly where the native
        // lowering started: the two walks have to agree for the element identities
        // to line up afterwards.
        const ElementNode* html = document.root->documentElement;

        if (html == nullptr)
        {
            for (const std::unique_ptr<Node>& child : document.root->children)
            {
                if (child->kind == NodeKind::Element)
                {
                    html = &static_cast<const ElementNode&>(*child);
                    break;
                }
            }
        }

        if (html == nullptr)
        {
            return true;
        }

        std::vector<std::unique_ptr<CSSElementNode>> pool;
        std::map<const CSSElementNode*, const ElementNode*> sourceByView;

        pool.push_back(std::make_unique<CSSElementNode>());
        CSSElementNode& root = *pool.back();
        BuildElementView(*html, root, pool);

        // The identity and source position of every view. Element ids belong to
        // the native lowering, which walked the same tree, so a parallel walk
        // recovers them - and pairs them the same way, skipping the text and
        // comment nodes the view does not carry.
        {
            std::vector<std::pair<const ElementNode*, CSSElementNode*>> pending;
            pending.emplace_back(html, &root);

            while (!pending.empty())
            {
                const ElementNode* source = pending.back().first;
                CSSElementNode* view = pending.back().second;
                pending.pop_back();

                sourceByView[view] = source;

                std::size_t childIndex = view->children.size();

                for (std::size_t index = source->children.size(); index > 0; --index)
                {
                    const Node& child = *source->children[index - 1];

                    if (child.kind != NodeKind::Element || childIndex == 0)
                    {
                        continue;
                    }

                    --childIndex;
                    pending.emplace_back(&static_cast<const ElementNode&>(child), view->children[childIndex]);
                }
            }
        }

        CSSElementNode::NumberSiblings(root);

        // --- the cascade ------------------------------------------------------
        RoboCSSCompiler::CSSDiagnosticSink cssDiagnostics;
        RoboCSSCompiler::CSSCascade cascade(cssDiagnostics);

        RoboCSSCompiler::CSSCascadeOptions options;
        options.viewportWidth = context.viewportWidth;
        options.viewportHeight = context.viewportHeight;
        options.rootFontSize = context.rootFontSize;
        cascade.SetOptions(options);

        for (const LoadedStyleSheet& sheet : sheets)
        {
            RoboCSSCompiler::CSSStylesheet parsed =
                RoboCSSCompiler::CSSParser(cssDiagnostics).ParseStylesheet(sheet.name, sheet.text);
            cascade.AddStyleSheet(parsed, RoboCSSCompiler::CSSOrigin::Author);
        }

        const RoboCSSCompiler::CSSCascadeResult result = cascade.Compute(root);

        // A CSS error is a build error: `completely-invalid-value` must not
        // quietly become a native value, and a stylesheet that cannot be matched
        // must not quietly render as if it were absent.
        const bool cssFailed = cssDiagnostics.HasErrors();
        Mirror(cssDiagnostics, document.sourceFile);

        if (cssFailed)
        {
            return false;
        }

        out.computedStyleCount = result.StyleCount();
        out.appliedDeclarationCount = result.statistics.appliedDeclarationCount;
        out.droppedDeclarationCount = result.statistics.invalidDeclarationCount +
                                      result.statistics.overriddenDeclarationCount;

        // --- the lowering -----------------------------------------------------
        StyleLowerer lowerer(diagnostics, context);

        // Two computed styles can lower to the same native style (`margin: 0` and
        // `margin: 0px`), and two elements with the same computed style share one
        // entry already; the second interning pass is over the native values, which
        // is what the runtime's table is indexed by.
        std::map<std::string, std::uint32_t> interned;

        for (std::size_t index = 0; index < result.elements.size(); ++index)
        {
            const std::uint32_t computedIndex = result.internedIndex[index];

            if (computedIndex == 0)
            {
                continue;
            }

            const CSSElementNode* view = result.elements[index];
            const auto source = sourceByView.find(view);

            if (source == sourceByView.end() || source->second == nullptr)
            {
                continue;
            }

            const CSSComputedStyle& computed = result.internedStyles[computedIndex - 1];

            // A lowering problem is about an element, so the diagnostic points at
            // where that element was written.
            const SourceLocation location{ source->second->location.file.empty() ? document.sourceFile
                                                                                : source->second->location.file,
                                           source->second->location.line, source->second->location.column,
                                           source->second->location.offset };

            CompiledStyle lowered;

            if (!lowerer.Lower(computed, location, lowered))
            {
                continue;
            }

            const std::uint64_t position =
                CssStyleTable::PositionKey(source->second->location.line, source->second->location.column);

            const std::string key(reinterpret_cast<const char*>(&lowered), sizeof(lowered));
            const auto existing = interned.find(key);

            if (existing != interned.end())
            {
                // An element that reuses an interned style is still a styled
                // element; only the table entry is shared.
                out.indexBySourcePosition[position] = existing->second;
                ++out.styledElementCount;
                continue;
            }

            // A compiled element carries its style as a 16-bit index, so the table
            // has a hard ceiling. A document that reaches it has a styling problem
            // worth reporting rather than silently truncating.
            if (out.styles.size() >= 0xFFFFu)
            {
                diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                                  SourceLocation{ document.sourceFile, 1, 1, 0 },
                                  "The document has more distinct styles than the compiled format can index.",
                                  "a compiled element carries its style as a 16-bit index",
                                  "Reduce the number of distinct declarations, or increase the width of the "
                                  "style index in the compiled format.");
                return false;
            }

            const auto styleIndex = static_cast<std::uint32_t>(out.styles.size());
            lowered.sourceStyleId = styleIndex;
            interned.emplace(key, styleIndex);
            out.styles.push_back(lowered);
            out.indexBySourcePosition[position] = styleIndex;
            ++out.styledElementCount;
        }

        out.byteSize = out.styles.size() * sizeof(RoboRenderer::CompiledStyle);

        return true;
    }

    void CssStyleBuilder::Mirror(RoboCSSCompiler::CSSDiagnosticSink& cssDiagnostics, const std::string& fallbackFile)
    {
        for (const RoboCSSCompiler::CSSDiagnostic& entry : cssDiagnostics.Entries())
        {
            const std::string file = entry.location.file.empty() ? fallbackFile : entry.location.file;
            const SourceLocation location{ file, static_cast<int>(entry.location.line),
                                           static_cast<int>(entry.location.column), entry.location.offset };

            // The CSS code space is preserved in the message: RCC001 is a
            // different failure from RCC010, and a report that flattened both
            // into "styles.css is broken" would hide which one it was.
            const std::string message = std::string(RoboCSSCompiler::ToString(entry.code)) + ": " + entry.message;

            if (entry.severity == RoboCSSCompiler::CSSSeverity::Error)
            {
                diagnostics.Error(DiagnosticCode::MalformedStyleSheet, location, message, entry.detail,
                                  entry.suggestion, entry.subject);
            }
            else
            {
                // The warning form has no `found` field, so the subject becomes
                // part of the sentence rather than being dropped.
                const std::string full =
                    entry.subject.empty() ? message : message + " (" + entry.subject + ")";

                diagnostics.Warning(DiagnosticCode::MalformedStyleSheet, location, full, entry.detail,
                                    entry.suggestion);
            }
        }

        cssDiagnostics.Clear();
    }
}
