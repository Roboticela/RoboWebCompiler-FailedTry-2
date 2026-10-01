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
        using RoboRenderer::StyleFlags;
        using RoboRenderer::StyleFlagBackground;
        using RoboRenderer::StyleFlagBorderBottomWidth;
        using RoboRenderer::StyleFlagBorderColor;
        using RoboRenderer::StyleFlagBorderLeftWidth;
        using RoboRenderer::StyleFlagBorderRadius;
        using RoboRenderer::StyleFlagBorderRightWidth;
        using RoboRenderer::StyleFlagBorderTopWidth;
        using RoboRenderer::StyleFlagDisplay;
        using RoboRenderer::FontKerningKind;
        using RoboRenderer::StyleFlagFontFeatures;
        using RoboRenderer::StyleFlagFontKerning;
        using RoboRenderer::StyleFlagFontSize;
        using RoboRenderer::StyleFlagFontWeight;
        using RoboRenderer::StyleFlagTextDecoration;
        using RoboRenderer::StyleFlagTextDecorationColor;
        using RoboRenderer::StyleFlagTextDecorationStyle;
        using RoboRenderer::StyleFlagTextDecorationThickness;
        using RoboRenderer::StyleFlagTextUnderlineOffset;
        using RoboRenderer::StyleFlagHyphens;
        using RoboRenderer::StyleFlagOverflowWrap;
        using RoboRenderer::StyleFlagWordBreak;
        using RoboRenderer::StyleFlagLineBreak;
        using RoboRenderer::StyleFlagTabSize;
        using RoboRenderer::StyleFlagTextAlignLast;
        using RoboRenderer::StyleFlagTextJustify;
        using RoboRenderer::HyphensKind;
        using RoboRenderer::OverflowWrapKind;
        using RoboRenderer::WordBreakKind;
        using RoboRenderer::LineBreakKind;
        using RoboRenderer::TextJustifyKind;
        using RoboRenderer::StyleFlagWritingMode;
        using RoboRenderer::StyleFlagTextOrientation;
        using RoboRenderer::WritingModeKind;
        using RoboRenderer::TextOrientationKind;
        using RoboRenderer::StyleFlagDirection;
        using RoboRenderer::StyleFlagFontFamily;
        using RoboRenderer::StyleFlagFontStyle;
        using RoboRenderer::StyleFlagForeground;
        using RoboRenderer::StyleFlagHeight;
        using RoboRenderer::StyleFlagLetterSpacing;
        using RoboRenderer::StyleFlagLineHeight;
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
        using RoboRenderer::StyleFlagTextIndent;
        using RoboRenderer::StyleFlagTextTransform;
        using RoboRenderer::StyleFlagWhiteSpace;
        using RoboRenderer::StyleFlagWidth;
        using RoboRenderer::StyleFlagWordSpacing;
        using RoboRenderer::TextAlignKind;
        using RoboRenderer::TextDecorationFlags;
        using RoboRenderer::TextDecorationLineThrough;
        using RoboRenderer::TextDecorationNone;
        using RoboRenderer::TextDecorationOverline;
        using RoboRenderer::TextDecorationStyleKind;
        using RoboRenderer::TextDecorationThicknessKind;
        using RoboRenderer::TextDecorationUnderline;
        using RoboRenderer::TextUnderlineOffsetKind;

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
            /// True when a length was actually resolved. `value` alone cannot say
            /// so: -1 is a legal length, so a caller that must tell "minus one"
            /// from "unresolved" reads this instead.
            bool resolved = false;
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
                    result.resolved = true;
                    return result;

                case CSSUnit::Rem:
                    result.value = static_cast<float>(value.number) * context.rootFontSize;
                    result.resolved = true;
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
                    result.resolved = true;
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
                    result.resolved = true;
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
                result.resolved = true;
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

        /// Reports a declaration the compiler compiled and could not apply.
        ///
        /// It is a CSS problem and it is reported as one: the CSS compiler's
        /// name, its own RCC code, and the declaration as the subject rather than
        /// a parenthesis inside the sentence. The location is the caller's, which
        /// is the declaration's own when the cascade kept track of it: a warning
        /// that points at the line of the element that used the style sends the
        /// reader to the wrong file.
        void WarnAboutStyle(DiagnosticSink& diagnostics,
                            const SourceLocation& location,
                            std::string message,
                            std::string detail,
                            std::string suggestion,
                            std::string subject)
        {
            Diagnostic diagnostic;
            diagnostic.code = DiagnosticCode::MalformedStyleSheet;
            diagnostic.severity = Severity::Warning;
            diagnostic.tool = "RoboCSSCompiler";
            diagnostic.codeOverride =
                RoboCSSCompiler::ToString(RoboCSSCompiler::CSSDiagnosticCode::UnsupportedFeature);
            diagnostic.location = location;
            diagnostic.message = std::move(message);
            diagnostic.explanation = std::move(detail);
            diagnostic.suggestedFix = std::move(suggestion);
            diagnostic.found = std::move(subject);
            diagnostics.Report(std::move(diagnostic));
        }

        /// The element location replaced by the declaration's own when the cascade
        /// kept track of where the value was written. A CSS problem reported at
        /// the line of the element that used the style sends the reader to the
        /// wrong file.
        SourceLocation DeclarationLocation(const RoboCSSCompiler::CSSComputedStyle& computed,
                                          RoboCSSCompiler::CSSPropertyId id,
                                          const SourceLocation& fallback)
        {
            const RoboCSSCompiler::CSSSourceLocation written = computed.SourceLocationOf(id);

            if (!written.IsValid())
            {
                return fallback;
            }

            return SourceLocation{ written.file, static_cast<int>(written.line),
                                   static_cast<int>(written.column), written.offset };
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
                       CompiledStyle& out,
                       std::string& fontFamily,
                       std::string& featureSettings)
            {
                out = CompiledStyle{};
                out.specified = 0u;
                fontFamily.clear();
                featureSettings.clear();

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
                        ReportNotLowered(computed, maximum, location,
                                         "The native box model has a minimum size and no maximum, so a maximum "
                                         "bound cannot be stored.",
                                         "Remove the declaration, or use min-width and min-height instead.");
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
                LowerText(computed, location, elementFontSize, out, fontFamily, featureSettings);
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
            void ReportNotLowered(const RoboCSSCompiler::CSSComputedStyle& computed, CSSPropertyId id,
                                  const SourceLocation& location, const char* reason,
                                  const char* suggestion)
            {
                const RoboCSSCompiler::CSSPropertyDefinition* definition =
                    RoboCSSCompiler::CSSPropertyRegistry::Instance().Find(id);
                const char* name = definition != nullptr ? definition->name : "property";

                // The declaration's own line: that is the line the reader has to
                // change, and it is in the stylesheet rather than in the page.
                WarnAboutStyle(diagnostics,
                                    DeclarationLocation(computed, id, location),
                                    std::string("'") + name + "' is compiled but not applied.",
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
                    ReportNotLowered(computed, CSSPropertyId::Display, location,
                                     "the native layout engine has four display "
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
                        ReportNotLowered(computed, id, location, "the native layout engine sizes from a length or the "
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
                        ReportNotLowered(computed, id, location, "this viewport unit has no native equivalent",
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
                           CompiledStyle& out,
                           std::string& fontFamily,
                           std::string& featureSettings)
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
                    else if (align->text == "justify")
                    {
                        out.textAlign = TextAlignKind::Justify;
                    }
                    else
                    {
                        return;
                    }

                    out.specified |= StyleFlagTextAlign;
                }

                LowerFontFamily(computed, out, fontFamily);
                LowerFontStyle(computed, out);
                LowerLineHeight(computed, location, elementFontSize, out);
                LowerWordSpacing(computed, location, elementFontSize, out);
                LowerTextIndent(computed, location, elementFontSize, out);
                LowerTextTransform(computed, out);
                LowerWhiteSpace(computed, out);
                LowerDirection(computed, out);
                LowerFontKerning(computed, out);
                LowerFontFeatures(computed, out, featureSettings);
                LowerTextDecoration(computed, location, elementFontSize, out);
                LowerLineBreaking(computed, location, elementFontSize, out);
            }

            void LowerFontKerning(const CSSComputedStyle& computed, CompiledStyle& out)
            {
                const CSSValue* kerning = Value(computed, CSSPropertyId::FontKerning);

                if (kerning == nullptr || kerning->kind != CSSValueKind::Keyword)
                {
                    return;
                }

                if (kerning->text == "none")
                {
                    out.fontKerning = FontKerningKind::None;
                }
                else if (kerning->text == "normal")
                {
                    out.fontKerning = FontKerningKind::Normal;
                }
                else
                {
                    out.fontKerning = FontKerningKind::Auto;
                }

                out.specified |= StyleFlagFontKerning;
            }

            void LowerFontFeatures(const CSSComputedStyle& computed,
                                   CompiledStyle& out,
                                   std::string& featureSettings)
            {
                // One compact declaration the shaper can parse: the compiled
                // style carries a document string index rather than a vector of
                // settings, so everything is written back out here.
                std::string written;
                std::vector<std::string> writtenTags;

                const auto addFeature = [&](const char* tag, int value) {
                    const std::string name(tag);
                    for (const std::string& existing : writtenTags)
                    {
                        // An explicit `font-feature-settings` entry wins over a
                        // `font-variant-*` longhand, so a tag is written once.
                        if (existing == name)
                        {
                            return;
                        }
                    }

                    if (!written.empty())
                    {
                        written += ", ";
                    }

                    written += "\"" + name + "\" " + std::to_string(value);
                    writtenTags.push_back(name);
                };

                const CSSValue* settings = Value(computed, CSSPropertyId::FontFeatureSettings);

                if (settings != nullptr && settings->kind != CSSValueKind::Keyword)
                {
                    for (const CSSValue& entry : settings->items)
                    {
                        const CSSValue* tag = &entry;
                        const CSSValue* number = nullptr;

                        if (entry.kind == CSSValueKind::List)
                        {
                            if (entry.items.empty())
                            {
                                continue;
                            }

                            tag = &entry.items[0];
                            number = entry.items.size() > 1 ? &entry.items[1] : nullptr;
                        }

                        if (tag->kind != CSSValueKind::String)
                        {
                            continue;
                        }

                        if (!written.empty())
                        {
                            written += ", ";
                        }

                        written += "\"" + tag->text + "\"";

                        if (number != nullptr)
                        {
                            written += " ";

                            // CSS numbers arrive as doubles; a feature value is
                            // a small non-negative integer, and a negative one is
                            // clamped the way the grammar says.
                            const long value = std::lround(number->number);
                            written += std::to_string(value < 0 ? 0 : value);
                        }

                        writtenTags.push_back(tag->text);
                    }
                }

                LowerFontVariants(computed, addFeature);

                if (written.empty())
                {
                    return;
                }

                featureSettings = written;
                out.specified |= StyleFlagFontFeatures;
            }

            /// Turns the `font-variant-*` longhands into OpenType features. Each
            /// keyword is the CSS spelling of a feature the shaper already
            /// knows, which is why this needs no new runtime state.
            template <typename AddFeature>
            void LowerFontVariants(const CSSComputedStyle& computed, AddFeature&& addFeature)
            {
                const auto forEachKeyword = [&](CSSPropertyId id, auto&& visit) {
                    const CSSValue* value = Value(computed, id);
                    if (value == nullptr)
                    {
                        return;
                    }

                    // `normal` is the initial value and asks for nothing, but a
                    // lone keyword is a real request: `small-caps`, `sub` and
                    // `none` all arrive this way.
                    if (value->kind == CSSValueKind::Keyword)
                    {
                        if (value->text != "normal")
                        {
                            visit(value->text);
                        }
                        return;
                    }

                    if (value->kind == CSSValueKind::List)
                    {
                        for (const CSSValue& entry : value->items)
                        {
                            if (entry.text != "normal")
                            {
                                visit(entry.text);
                            }
                        }
                    }
                };

                forEachKeyword(CSSPropertyId::FontVariantLigatures, [&](const std::string& keyword) {
                    if (keyword == "none")
                    {
                        addFeature("liga", 0);
                        addFeature("clig", 0);
                        addFeature("dlig", 0);
                        addFeature("hlig", 0);
                        addFeature("calt", 0);
                    }
                    else if (keyword == "common-ligatures")
                    {
                        addFeature("liga", 1);
                        addFeature("clig", 1);
                    }
                    else if (keyword == "no-common-ligatures")
                    {
                        addFeature("liga", 0);
                        addFeature("clig", 0);
                    }
                    else if (keyword == "discretionary-ligatures")
                    {
                        addFeature("dlig", 1);
                    }
                    else if (keyword == "no-discretionary-ligatures")
                    {
                        addFeature("dlig", 0);
                    }
                    else if (keyword == "historical-ligatures")
                    {
                        addFeature("hlig", 1);
                    }
                    else if (keyword == "no-historical-ligatures")
                    {
                        addFeature("hlig", 0);
                    }
                    else if (keyword == "contextual")
                    {
                        addFeature("calt", 1);
                    }
                    else if (keyword == "no-contextual")
                    {
                        addFeature("calt", 0);
                    }
                });

                forEachKeyword(CSSPropertyId::FontVariantCaps, [&](const std::string& keyword) {
                    if (keyword == "small-caps")
                    {
                        addFeature("smcp", 1);
                    }
                    else if (keyword == "all-small-caps")
                    {
                        addFeature("smcp", 1);
                        addFeature("c2sc", 1);
                    }
                    else if (keyword == "petite-caps")
                    {
                        addFeature("pcap", 1);
                    }
                    else if (keyword == "all-petite-caps")
                    {
                        addFeature("pcap", 1);
                        addFeature("c2pc", 1);
                    }
                    else if (keyword == "unicase")
                    {
                        addFeature("unic", 1);
                    }
                    else if (keyword == "titling-caps")
                    {
                        addFeature("titl", 1);
                    }
                });

                forEachKeyword(CSSPropertyId::FontVariantNumeric, [&](const std::string& keyword) {
                    if (keyword == "lining-nums")
                    {
                        addFeature("lnum", 1);
                    }
                    else if (keyword == "oldstyle-nums")
                    {
                        addFeature("onum", 1);
                    }
                    else if (keyword == "proportional-nums")
                    {
                        addFeature("pnum", 1);
                    }
                    else if (keyword == "tabular-nums")
                    {
                        addFeature("tnum", 1);
                    }
                    else if (keyword == "diagonal-fractions")
                    {
                        addFeature("frac", 1);
                    }
                    else if (keyword == "stacked-fractions")
                    {
                        addFeature("afrc", 1);
                    }
                    else if (keyword == "ordinal")
                    {
                        addFeature("ordn", 1);
                    }
                    else if (keyword == "slashed-zero")
                    {
                        addFeature("zero", 1);
                    }
                });

                forEachKeyword(CSSPropertyId::FontVariantPosition, [&](const std::string& keyword) {
                    if (keyword == "sub")
                    {
                        addFeature("subs", 1);
                    }
                    else if (keyword == "super")
                    {
                        addFeature("sups", 1);
                    }
                });

                forEachKeyword(CSSPropertyId::FontVariantEastAsian, [&](const std::string& keyword) {
                    if (keyword == "jis78")
                    {
                        addFeature("jp78", 1);
                    }
                    else if (keyword == "jis83")
                    {
                        addFeature("jp83", 1);
                    }
                    else if (keyword == "jis90")
                    {
                        addFeature("jp90", 1);
                    }
                    else if (keyword == "jis04")
                    {
                        addFeature("jp04", 1);
                    }
                    else if (keyword == "simplified")
                    {
                        addFeature("smpl", 1);
                    }
                    else if (keyword == "traditional")
                    {
                        addFeature("trad", 1);
                    }
                    else if (keyword == "full-width")
                    {
                        addFeature("fwid", 1);
                    }
                    else if (keyword == "proportional-width")
                    {
                        addFeature("pwid", 1);
                    }
                    else if (keyword == "ruby")
                    {
                        addFeature("ruby", 1);
                    }
                });
            }

            void LowerTextDecoration(const CSSComputedStyle& computed,
                                     const SourceLocation& location,
                                     float elementFontSize,
                                     CompiledStyle& out)
            {
                const CSSValue* line = Value(computed, CSSPropertyId::TextDecorationLine);
                const CSSValue* color = Value(computed, CSSPropertyId::TextDecorationColor);
                const CSSValue* style = Value(computed, CSSPropertyId::TextDecorationStyle);
                const CSSValue* thickness = Value(computed, CSSPropertyId::TextDecorationThickness);
                const CSSValue* offset = Value(computed, CSSPropertyId::TextUnderlineOffset);

                if (line == nullptr && color == nullptr && style == nullptr && thickness == nullptr &&
                    offset == nullptr)
                {
                    return;
                }

                std::uint8_t decoration = TextDecorationNone;
                bool anyLine = false;

                const auto addLine = [&](const CSSValue& value) {
                    if (value.kind != CSSValueKind::Keyword && value.kind != CSSValueKind::Ident)
                    {
                        return;
                    }

                    anyLine = true;

                    if (value.text == "underline")
                    {
                        decoration |= TextDecorationUnderline;
                    }
                    else if (value.text == "overline")
                    {
                        decoration |= TextDecorationOverline;
                    }
                    else if (value.text == "line-through")
                    {
                        decoration |= TextDecorationLineThrough;
                    }
                };

                if (line != nullptr)
                {
                    if (line->kind == CSSValueKind::List)
                    {
                        for (const CSSValue& entry : line->items)
                        {
                            addLine(entry);
                        }
                    }
                    else
                    {
                        addLine(*line);
                    }

                    out.textDecoration = decoration;
                }
                else
                {
                    // Only the colour was stated: the line keeps the default.
                    out.textDecoration = TextDecorationNone;
                }

                if (color != nullptr && color->kind == CSSValueKind::Color &&
                    color->color.kind != CSSColorKind::CurrentColor)
                {
                    out.textDecorationColor = ToNativeColor(color->color);
                    out.specified |= StyleFlagTextDecorationColor;
                }

                // `none` is the initial value, so a style that says only `none`
                // has nothing to store; stating it still counts as authoring the
                // property, which is what the flag records.
                if (anyLine || line != nullptr)
                {
                    out.specified |= StyleFlagTextDecoration;
                }

                if (style != nullptr && style->kind == CSSValueKind::Keyword)
                {
                    TextDecorationStyleKind kind = TextDecorationStyleKind::Solid;

                    if (style->text == "double")
                    {
                        kind = TextDecorationStyleKind::Double;
                    }
                    else if (style->text == "dotted")
                    {
                        kind = TextDecorationStyleKind::Dotted;
                    }
                    else if (style->text == "dashed")
                    {
                        kind = TextDecorationStyleKind::Dashed;
                    }
                    else if (style->text == "wavy")
                    {
                        kind = TextDecorationStyleKind::Wavy;
                    }

                    out.textDecorationStyle = kind;
                    out.specified |= StyleFlagTextDecorationStyle;
                }

                if (thickness != nullptr)
                {
                    if (thickness->kind == CSSValueKind::Keyword && thickness->text == "from-font")
                    {
                        out.textDecorationThicknessKind = TextDecorationThicknessKind::FromFont;
                        out.textDecorationThickness = 0.0f;
                        out.specified |= StyleFlagTextDecorationThickness;
                    }
                    else if (thickness->kind == CSSValueKind::Keyword && thickness->text == "auto")
                    {
                        out.textDecorationThicknessKind = TextDecorationThicknessKind::Auto;
                        out.textDecorationThickness = 0.0f;
                        out.specified |= StyleFlagTextDecorationThickness;
                    }
                    else
                    {
                        const LengthResolution resolved =
                            ResolveLength(*thickness, context, elementFontSize, "text-decoration-thickness",
                                          diagnostics, location);

                        if (resolved.resolved && resolved.value >= 0.0f)
                        {
                            out.textDecorationThicknessKind = TextDecorationThicknessKind::Length;
                            out.textDecorationThickness = resolved.value;
                            out.specified |= StyleFlagTextDecorationThickness;
                        }
                    }
                }

                if (offset != nullptr)
                {
                    if (offset->kind == CSSValueKind::Keyword && offset->text == "auto")
                    {
                        out.textUnderlineOffsetKind = TextUnderlineOffsetKind::Auto;
                        out.specified |= StyleFlagTextUnderlineOffset;
                    }
                    else
                    {
                        const LengthResolution resolved = ResolveLength(
                            *offset, context, elementFontSize, "text-underline-offset", diagnostics, location);

                        if (resolved.resolved)
                        {
                            out.textUnderlineOffsetKind = TextUnderlineOffsetKind::Length;
                            out.textUnderlineOffset = resolved.value;
                            out.specified |= StyleFlagTextUnderlineOffset;
                        }
                    }
                }
            }

            /// The line-breaking family, and the two justification properties.
            /// Each is a closed keyword set, so the lowering is a switch over the
            /// computed keyword; `tab-size` is the one that takes a number.
            void LowerLineBreaking(const CSSComputedStyle& computed,
                                   const SourceLocation& location,
                                   float elementFontSize,
                                   CompiledStyle& out)
            {
                const auto lowerKeyword = [&](CSSPropertyId id, const char* const* names,
                                              const std::uint8_t* kinds, std::size_t count,
                                              StyleFlags flag, std::uint8_t& target) {
                    const CSSValue* value = Value(computed, id);

                    if (value == nullptr || value->kind != CSSValueKind::Keyword)
                    {
                        return;
                    }

                    for (std::size_t index = 0; index < count; ++index)
                    {
                        if (value->text == names[index])
                        {
                            target = kinds[index];
                            out.specified |= flag;
                            return;
                        }
                    }
                };

                static const char* const kHyphens[] = { "none", "manual", "auto" };
                static const std::uint8_t kHyphensKinds[] = {
                    static_cast<std::uint8_t>(HyphensKind::None), static_cast<std::uint8_t>(HyphensKind::Manual),
                    static_cast<std::uint8_t>(HyphensKind::Auto)
                };
                static const char* const kOverflowWrap[] = { "normal", "break-word", "anywhere" };
                static const std::uint8_t kOverflowWrapKinds[] = {
                    static_cast<std::uint8_t>(OverflowWrapKind::Normal),
                    static_cast<std::uint8_t>(OverflowWrapKind::BreakWord),
                    static_cast<std::uint8_t>(OverflowWrapKind::Anywhere)
                };
                static const char* const kWordBreak[] = { "normal", "break-all", "keep-all" };
                static const std::uint8_t kWordBreakKinds[] = {
                    static_cast<std::uint8_t>(WordBreakKind::Normal),
                    static_cast<std::uint8_t>(WordBreakKind::BreakAll),
                    static_cast<std::uint8_t>(WordBreakKind::KeepAll)
                };
                static const char* const kLineBreak[] = { "auto", "loose", "normal", "strict", "anywhere" };
                static const std::uint8_t kLineBreakKinds[] = {
                    static_cast<std::uint8_t>(LineBreakKind::Auto), static_cast<std::uint8_t>(LineBreakKind::Loose),
                    static_cast<std::uint8_t>(LineBreakKind::Normal),
                    static_cast<std::uint8_t>(LineBreakKind::Strict),
                    static_cast<std::uint8_t>(LineBreakKind::Anywhere)
                };
                static const char* const kTextJustify[] = { "auto", "none", "inter-word", "inter-character" };
                static const std::uint8_t kTextJustifyKinds[] = {
                    static_cast<std::uint8_t>(TextJustifyKind::Auto),
                    static_cast<std::uint8_t>(TextJustifyKind::None),
                    static_cast<std::uint8_t>(TextJustifyKind::InterWord),
                    static_cast<std::uint8_t>(TextJustifyKind::InterCharacter)
                };

                std::uint8_t hyphens = static_cast<std::uint8_t>(out.hyphens);
                std::uint8_t overflowWrap = static_cast<std::uint8_t>(out.overflowWrap);
                std::uint8_t wordBreak = static_cast<std::uint8_t>(out.wordBreak);
                std::uint8_t lineBreak = static_cast<std::uint8_t>(out.lineBreak);
                std::uint8_t textJustify = static_cast<std::uint8_t>(out.textJustify);

                lowerKeyword(CSSPropertyId::Hyphens, kHyphens, kHyphensKinds, 3, StyleFlagHyphens, hyphens);
                lowerKeyword(CSSPropertyId::OverflowWrap, kOverflowWrap, kOverflowWrapKinds, 3,
                             StyleFlagOverflowWrap, overflowWrap);
                lowerKeyword(CSSPropertyId::WordBreak, kWordBreak, kWordBreakKinds, 3, StyleFlagWordBreak,
                             wordBreak);
                lowerKeyword(CSSPropertyId::LineBreak, kLineBreak, kLineBreakKinds, 5, StyleFlagLineBreak,
                             lineBreak);
                lowerKeyword(CSSPropertyId::TextJustify, kTextJustify, kTextJustifyKinds, 4,
                             StyleFlagTextJustify, textJustify);

                out.hyphens = static_cast<HyphensKind>(hyphens);
                out.overflowWrap = static_cast<OverflowWrapKind>(overflowWrap);
                out.wordBreak = static_cast<WordBreakKind>(wordBreak);
                out.lineBreak = static_cast<LineBreakKind>(lineBreak);
                out.textJustify = static_cast<TextJustifyKind>(textJustify);

                const CSSValue* last = Value(computed, CSSPropertyId::TextAlignLast);

                if (last != nullptr && last->kind == CSSValueKind::Keyword)
                {
                    TextAlignKind kind = TextAlignKind::Start;
                    bool known = true;

                    if (last->text == "start" || last->text == "left")
                    {
                        kind = TextAlignKind::Start;
                    }
                    else if (last->text == "center")
                    {
                        kind = TextAlignKind::Center;
                    }
                    else if (last->text == "end" || last->text == "right")
                    {
                        kind = TextAlignKind::End;
                    }
                    else if (last->text == "justify")
                    {
                        kind = TextAlignKind::Justify;
                    }
                    else if (last->text == "auto")
                    {
                        // `auto` means "whatever `text-align` says for the last
                        // line", which the layout already does; recording it as
                        // start keeps the flag meaningful without overriding.
                        kind = TextAlignKind::Start;
                    }
                    else
                    {
                        known = false;
                    }

                    if (known)
                    {
                        out.textAlignLast = kind;
                        out.hasTextAlignLast = last->text != "auto";
                        out.specified |= StyleFlagTextAlignLast;
                    }
                }

                const CSSValue* tab = Value(computed, CSSPropertyId::TabSize);

                if (tab != nullptr)
                {
                    if (tab->kind == CSSValueKind::Number)
                    {
                        out.tabSize = static_cast<float>(tab->number);
                        out.tabSizeLength = 0.0f;
                        out.specified |= StyleFlagTabSize;
                    }
                    else
                    {
                        const LengthResolution resolved =
                            ResolveLength(*tab, context, elementFontSize, "tab-size", diagnostics, location);

                        if (resolved.resolved && resolved.value >= 0.0f)
                        {
                            out.tabSize = 0.0f;
                            out.tabSizeLength = resolved.value;
                            out.specified |= StyleFlagTabSize;
                        }
                    }
                }

                const CSSValue* writingMode = Value(computed, CSSPropertyId::WritingMode);

                if (writingMode != nullptr && writingMode->kind == CSSValueKind::Keyword)
                {
                    WritingModeKind mode = WritingModeKind::HorizontalTb;

                    if (writingMode->text == "horizontal-tb")
                    {
                        mode = WritingModeKind::HorizontalTb;
                    }
                    else if (writingMode->text == "vertical-rl")
                    {
                        mode = WritingModeKind::VerticalRl;
                    }
                    else if (writingMode->text == "vertical-lr")
                    {
                        mode = WritingModeKind::VerticalLr;
                    }
                    else if (writingMode->text == "sideways-rl")
                    {
                        mode = WritingModeKind::SidewaysRl;
                    }
                    else if (writingMode->text == "sideways-lr")
                    {
                        mode = WritingModeKind::SidewaysLr;
                    }

                    out.writingMode = mode;
                    out.specified |= StyleFlagWritingMode;
                }

                const CSSValue* orientation = Value(computed, CSSPropertyId::TextOrientation);

                if (orientation != nullptr && orientation->kind == CSSValueKind::Keyword)
                {
                    TextOrientationKind kind = TextOrientationKind::Mixed;

                    if (orientation->text == "upright")
                    {
                        kind = TextOrientationKind::Upright;
                    }
                    else if (orientation->text == "sideways")
                    {
                        kind = TextOrientationKind::Sideways;
                    }

                    // The sideways writing modes force a sideways orientation,
                    // whatever `text-orientation` says, which is what the
                    // reference specifies.
                    if (out.writingMode == WritingModeKind::SidewaysRl ||
                        out.writingMode == WritingModeKind::SidewaysLr)
                    {
                        kind = TextOrientationKind::Sideways;
                    }

                    out.textOrientation = kind;
                    out.specified |= StyleFlagTextOrientation;
                }
            }

            void LowerFontFamily(const CSSComputedStyle& computed, CompiledStyle& out, std::string& fontFamily)
            {
                const CSSValue* family = Value(computed, CSSPropertyId::FontFamily);

                if (family == nullptr || family->kind != CSSValueKind::CommaList || family->items.empty())
                {
                    return;
                }

                fontFamily.clear();
                for (const CSSValue& item : family->items)
                {
                    if (!fontFamily.empty())
                    {
                        fontFamily += ", ";
                    }
                    fontFamily += item.text;
                }

                if (!fontFamily.empty())
                {
                    out.specified |= StyleFlagFontFamily;
                }
            }

            void LowerFontStyle(const CSSComputedStyle& computed, CompiledStyle& out)
            {
                const CSSValue* style = Value(computed, CSSPropertyId::FontStyle);

                if (style == nullptr || style->kind != CSSValueKind::Keyword)
                {
                    return;
                }

                out.fontStyle = style->text == "italic" ? RoboRenderer::FontSlantKind::Italic
                                                         : RoboRenderer::FontSlantKind::Upright;
                out.specified |= StyleFlagFontStyle;
            }

            void LowerLineHeight(const CSSComputedStyle& computed,
                                 const SourceLocation& location,
                                 float elementFontSize,
                                 CompiledStyle& out)
            {
                const CSSValue* height = Value(computed, CSSPropertyId::LineHeight);

                if (height == nullptr)
                {
                    return;
                }

                if (height->kind == CSSValueKind::Keyword && height->text == "normal")
                {
                    out.lineHeight = 0.0f;
                    out.specified |= StyleFlagLineHeight;
                    return;
                }

                if (height->kind == CSSValueKind::Number)
                {
                    out.lineHeight = static_cast<float>(height->number);
                    out.specified |= StyleFlagLineHeight;
                    return;
                }

                // A length is absolute once resolved; the runtime stores a
                // multiple of the font size, so the resolution is done here.
                const LengthResolution resolved =
                    ResolveLength(*height, context, elementFontSize, "line-height", diagnostics, location);

                if (resolved.value >= 0.0f && elementFontSize > 0.0f)
                {
                    out.lineHeight = resolved.value / elementFontSize;
                    out.specified |= StyleFlagLineHeight;
                }
            }

            void LowerWordSpacing(const CSSComputedStyle& computed,
                                  const SourceLocation& location,
                                  float elementFontSize,
                                  CompiledStyle& out)
            {
                const CSSValue* spacing = Value(computed, CSSPropertyId::WordSpacing);

                if (spacing == nullptr)
                {
                    return;
                }

                if (spacing->kind == CSSValueKind::Keyword && spacing->text == "normal")
                {
                    out.wordSpacing = 0.0f;
                    out.specified |= StyleFlagWordSpacing;
                    return;
                }

                const LengthResolution resolved =
                    ResolveLength(*spacing, context, elementFontSize, "word-spacing", diagnostics, location);

                if (resolved.value != -1.0f)
                {
                    out.wordSpacing = resolved.value;
                    out.specified |= StyleFlagWordSpacing;
                }
            }

            void LowerTextIndent(const CSSComputedStyle& computed,
                                 const SourceLocation& location,
                                 float elementFontSize,
                                 CompiledStyle& out)
            {
                const CSSValue* indent = Value(computed, CSSPropertyId::TextIndent);

                if (indent == nullptr)
                {
                    return;
                }

                const LengthResolution resolved =
                    ResolveLength(*indent, context, elementFontSize, "text-indent", diagnostics, location);

                if (resolved.value != -1.0f)
                {
                    out.textIndent = resolved.value;
                    out.specified |= StyleFlagTextIndent;
                }
            }

            void LowerTextTransform(const CSSComputedStyle& computed, CompiledStyle& out)
            {
                const CSSValue* transform = Value(computed, CSSPropertyId::TextTransform);

                if (transform == nullptr || transform->kind != CSSValueKind::Keyword)
                {
                    return;
                }

                if (transform->text == "uppercase")
                {
                    out.textTransform = RoboRenderer::TextTransformKind::Uppercase;
                }
                else if (transform->text == "lowercase")
                {
                    out.textTransform = RoboRenderer::TextTransformKind::Lowercase;
                }
                else if (transform->text == "capitalize")
                {
                    out.textTransform = RoboRenderer::TextTransformKind::Capitalize;
                }
                else
                {
                    out.textTransform = RoboRenderer::TextTransformKind::None;
                }

                out.specified |= StyleFlagTextTransform;
            }

            void LowerWhiteSpace(const CSSComputedStyle& computed, CompiledStyle& out)
            {
                const CSSValue* space = Value(computed, CSSPropertyId::WhiteSpace);

                if (space == nullptr || space->kind != CSSValueKind::Keyword)
                {
                    return;
                }

                if (space->text == "nowrap")
                {
                    out.whiteSpace = RoboRenderer::WhiteSpaceKind::NoWrap;
                }
                else if (space->text == "pre")
                {
                    out.whiteSpace = RoboRenderer::WhiteSpaceKind::Pre;
                }
                else if (space->text == "pre-wrap")
                {
                    out.whiteSpace = RoboRenderer::WhiteSpaceKind::PreWrap;
                }
                else if (space->text == "pre-line")
                {
                    out.whiteSpace = RoboRenderer::WhiteSpaceKind::PreLine;
                }
                else
                {
                    out.whiteSpace = RoboRenderer::WhiteSpaceKind::Normal;
                }

                out.specified |= StyleFlagWhiteSpace;
            }

            void LowerDirection(const CSSComputedStyle& computed, CompiledStyle& out)
            {
                const CSSValue* direction = Value(computed, CSSPropertyId::Direction);

                if (direction == nullptr || direction->kind != CSSValueKind::Keyword)
                {
                    return;
                }

                out.direction = direction->text == "rtl" ? RoboRenderer::TextDirectionKind::RightToLeft
                                                          : RoboRenderer::TextDirectionKind::LeftToRight;
                out.specified |= StyleFlagDirection;
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
        out.fontFamilies.clear();
        out.featureSettings.clear();
        out.indexBySourcePosition.clear();

        // Index 0 stays empty: a style index of 0 is the compiled format's way of
        // saying "the native default", so the authored table starts at 1.
        out.styles.emplace_back(CompiledStyle{});
        out.fontFamilies.emplace_back();
        out.featureSettings.emplace_back();

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
            std::string fontFamily;
            std::string featureSettings;

            if (!lowerer.Lower(computed, location, lowered, fontFamily, featureSettings))
            {
                continue;
            }

            const std::uint64_t position =
                CssStyleTable::PositionKey(source->second->location.line, source->second->location.column);

            // The family name is interned later, so it is not part of the style
            // bytes; two styles that differ only in family must not share an
            // entry, so the interning key carries it explicitly.
            // The key names every field the style carries. Dumping the struct's
            // bytes would be shorter, but a struct has padding bytes no member
            // names, and two styles that agree field for field must not compare
            // unequal on whatever happened to sit in the padding. Listing the
            // fields is the only way to say that without depending on layout.
            std::string key;
            const auto appendField = [&key](const auto& field) {
                key.append(reinterpret_cast<const char*>(&field), sizeof(field));
            };

            appendField(lowered.specified);
            appendField(lowered.width);
            appendField(lowered.height);
            appendField(lowered.minimumWidth);
            appendField(lowered.minimumHeight);
            appendField(lowered.margin);
            appendField(lowered.padding);
            appendField(lowered.borderWidth);
            appendField(lowered.borderRadius);
            appendField(lowered.background);
            appendField(lowered.foreground);
            appendField(lowered.borderColor);
            appendField(lowered.fontSize);
            appendField(lowered.lineHeight);
            appendField(lowered.letterSpacing);
            appendField(lowered.wordSpacing);
            appendField(lowered.textIndent);
            appendField(lowered.display);
            appendField(lowered.overflowX);
            appendField(lowered.overflowY);
            appendField(lowered.textAlign);
            appendField(lowered.fontWeight);
            appendField(lowered.fontStyle);
            appendField(lowered.whiteSpace);
            appendField(lowered.textTransform);
            appendField(lowered.direction);
            appendField(lowered.hyphens);
            appendField(lowered.overflowWrap);
            appendField(lowered.wordBreak);
            appendField(lowered.lineBreak);
            appendField(lowered.tabSize);
            appendField(lowered.tabSizeLength);
            appendField(lowered.textAlignLast);
            appendField(lowered.hasTextAlignLast);
            appendField(lowered.textJustify);
            appendField(lowered.writingMode);
            appendField(lowered.textOrientation);
            appendField(lowered.fontFamily);
            appendField(lowered.fontFeatures);
            appendField(lowered.fontKerning);
            appendField(lowered.textDecoration);
            appendField(lowered.textDecorationColor);
            appendField(lowered.textDecorationStyle);
            appendField(lowered.textDecorationThicknessKind);
            appendField(lowered.textDecorationThickness);
            appendField(lowered.textUnderlineOffsetKind);
            appendField(lowered.textUnderlineOffset);
            appendField(lowered.hasBackground);
            appendField(lowered.hasForeground);
            appendField(lowered.hasBorder);
            appendField(lowered.hasFontSize);
            key += '|';
            key += fontFamily;
            key += '|';
            key += featureSettings;
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
            out.fontFamilies.push_back(fontFamily);
            out.featureSettings.push_back(featureSettings);
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

            // A CSS problem is reported as a CSS problem: by the CSS compiler's
            // own name and its own code. Folding RCC001 and RCC010 into "the
            // stylesheet is malformed" would hide which of the two it was, and
            // printing the code into the message would leave the report
            // claiming a code that the header line does not agree with.
            Diagnostic diagnostic;
            diagnostic.code = DiagnosticCode::MalformedStyleSheet;
            diagnostic.tool = "RoboCSSCompiler";
            diagnostic.codeOverride = RoboCSSCompiler::ToString(entry.code);
            diagnostic.location = location;
            diagnostic.explanation = entry.detail;
            diagnostic.suggestedFix = entry.suggestion;
            diagnostic.found = entry.subject;

            if (entry.severity == RoboCSSCompiler::CSSSeverity::Error)
            {
                diagnostic.severity = Severity::Error;
                diagnostic.message = entry.message;
            }
            else
            {
                // The subject is already in `found`, so the message stays the
                // sentence it is.
                diagnostic.severity = Severity::Warning;
                diagnostic.message = entry.message;
            }

            diagnostics.Report(std::move(diagnostic));
        }

        cssDiagnostics.Clear();
    }
}
