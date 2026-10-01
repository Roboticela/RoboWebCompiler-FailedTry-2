#include "RoboHTMLCompiler/CodeGenerator.h"

#include "CodeEmit.h"
#include "RoboHTMLCompiler/CompiledUiBuilder.h"
#include "RoboRenderer/UiDiagnostics.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    using namespace RoboRenderer;

    namespace
    {
        void WriteFile(const std::filesystem::path& path, const std::string& contents)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream << contents;
        }

        std::string FlagsExpression(std::uint64_t flags)
        {
            if (flags == 0)
            {
                return "0";
            }

            struct NamedFlag
            {
                ElementFlags flag;
                const char* name;
            };

            static const NamedFlag named[] = {
                { ElementFlagHidden, "ElementFlagHidden" },
                { ElementFlagDisabled, "ElementFlagDisabled" },
                { ElementFlagChecked, "ElementFlagChecked" },
                { ElementFlagSelected, "ElementFlagSelected" },
                { ElementFlagReadOnly, "ElementFlagReadOnly" },
                { ElementFlagRequired, "ElementFlagRequired" },
                { ElementFlagInert, "ElementFlagInert" },
                { ElementFlagOpen, "ElementFlagOpen" },
                { ElementFlagMultiple, "ElementFlagMultiple" },
                { ElementFlagAutofocus, "ElementFlagAutofocus" },
                { ElementFlagControls, "ElementFlagControls" },
                { ElementFlagAutoPlay, "ElementFlagAutoPlay" },
                { ElementFlagLoop, "ElementFlagLoop" },
                { ElementFlagMuted, "ElementFlagMuted" },
                { ElementFlagReversed, "ElementFlagReversed" },
                { ElementFlagNoValidate, "ElementFlagNoValidate" },
                { ElementFlagHasWidth, "ElementFlagHasWidth" },
                { ElementFlagHasHeight, "ElementFlagHasHeight" },
                { ElementFlagHasRows, "ElementFlagHasRows" },
                { ElementFlagHasColumns, "ElementFlagHasColumns" },
                { ElementFlagHasValue, "ElementFlagHasValue" },
                { ElementFlagHasMinimum, "ElementFlagHasMinimum" },
                { ElementFlagHasMaximum, "ElementFlagHasMaximum" },
                { ElementFlagHasTabIndex, "ElementFlagHasTabIndex" },
                { ElementFlagHasId, "ElementFlagHasId" },
                { ElementFlagHasText, "ElementFlagHasText" },
                { ElementFlagHasHref, "ElementFlagHasHref" },
                { ElementFlagHasPlaceholder, "ElementFlagHasPlaceholder" },
                { ElementFlagHasLabelTarget, "ElementFlagHasLabelTarget" },
                { ElementFlagHasAlternativeText, "ElementFlagHasAlternativeText" },
                { ElementFlagHasResource, "ElementFlagHasResource" },
                { ElementFlagHasName, "ElementFlagHasName" },
                { ElementFlagHasListStart, "ElementFlagHasListStart" },
                { ElementFlagHasListStyle, "ElementFlagHasListStyle" },
                { ElementFlagHasNumericValue, "ElementFlagHasNumericValue" },
                { ElementFlagInteractive, "ElementFlagInteractive" },
                { ElementFlagFocusable, "ElementFlagFocusable" },
                { ElementFlagTextRun, "ElementFlagTextRun" },
                { ElementFlagInline, "ElementFlagInline" },
                { ElementFlagPreserveWhitespace, "ElementFlagPreserveWhitespace" },
                { ElementFlagRightToLeft, "ElementFlagRightToLeft" }
            };

            std::string expression;
            for (const NamedFlag& entry : named)
            {
                if ((flags & entry.flag) == 0)
                {
                    continue;
                }

                if (!expression.empty())
                {
                    expression += " | ";
                }

                expression += "RoboRenderer::";
                expression += entry.name;
            }

            if (expression.empty())
            {
                // Bits the generated table does not name: emit the raw value so
                // the data stays exact.
                std::ostringstream raw;
                raw << "0x" << std::hex << flags << "ull";
                return raw.str();
            }

            return expression;
        }

        /// A `specified` mask as named flags, which is what makes the generated
        /// table readable: the bits *are* the documentation.
        std::string StyleFlagsExpression(std::uint64_t specified)
        {
            if (specified == 0ull)
            {
                return "0ull";
            }

            struct NamedStyleFlag
            {
                StyleFlags flag;
                const char* name;
            };

            static const NamedStyleFlag named[] = {
                { StyleFlagWidth, "StyleFlagWidth" },
                { StyleFlagHeight, "StyleFlagHeight" },
                { StyleFlagMinimumWidth, "StyleFlagMinimumWidth" },
                { StyleFlagMinimumHeight, "StyleFlagMinimumHeight" },
                { StyleFlagMarginTop, "StyleFlagMarginTop" },
                { StyleFlagMarginRight, "StyleFlagMarginRight" },
                { StyleFlagMarginBottom, "StyleFlagMarginBottom" },
                { StyleFlagMarginLeft, "StyleFlagMarginLeft" },
                { StyleFlagPaddingTop, "StyleFlagPaddingTop" },
                { StyleFlagPaddingRight, "StyleFlagPaddingRight" },
                { StyleFlagPaddingBottom, "StyleFlagPaddingBottom" },
                { StyleFlagPaddingLeft, "StyleFlagPaddingLeft" },
                { StyleFlagBorderTopWidth, "StyleFlagBorderTopWidth" },
                { StyleFlagBorderRightWidth, "StyleFlagBorderRightWidth" },
                { StyleFlagBorderBottomWidth, "StyleFlagBorderBottomWidth" },
                { StyleFlagBorderLeftWidth, "StyleFlagBorderLeftWidth" },
                { StyleFlagBorderRadius, "StyleFlagBorderRadius" },
                { StyleFlagBackground, "StyleFlagBackground" },
                { StyleFlagForeground, "StyleFlagForeground" },
                { StyleFlagBorderColor, "StyleFlagBorderColor" },
                { StyleFlagFontSize, "StyleFlagFontSize" },
                { StyleFlagFontWeight, "StyleFlagFontWeight" },
                { StyleFlagLetterSpacing, "StyleFlagLetterSpacing" },
                { StyleFlagTextAlign, "StyleFlagTextAlign" },
                { StyleFlagDisplay, "StyleFlagDisplay" },
                { StyleFlagOverflowX, "StyleFlagOverflowX" },
                { StyleFlagOverflowY, "StyleFlagOverflowY" },
                { StyleFlagLineHeight, "StyleFlagLineHeight" },
                { StyleFlagWordSpacing, "StyleFlagWordSpacing" },
                { StyleFlagTextIndent, "StyleFlagTextIndent" },
                { StyleFlagWhiteSpace, "StyleFlagWhiteSpace" },
                { StyleFlagTextTransform, "StyleFlagTextTransform" },
                { StyleFlagDirection, "StyleFlagDirection" },
                { StyleFlagFontStyle, "StyleFlagFontStyle" },
                { StyleFlagFontFamily, "StyleFlagFontFamily" },
                { StyleFlagFontKerning, "StyleFlagFontKerning" },
                { StyleFlagFontFeatures, "StyleFlagFontFeatures" },
                { StyleFlagTextDecoration, "StyleFlagTextDecoration" },
                { StyleFlagTextDecorationColor, "StyleFlagTextDecorationColor" }
            };

            std::string expression;

            for (const NamedStyleFlag& entry : named)
            {
                if ((specified & static_cast<std::uint64_t>(entry.flag)) == 0ull)
                {
                    continue;
                }

                if (!expression.empty())
                {
                    expression += " | ";
                }

                expression += "RoboRenderer::";
                expression += entry.name;
            }

            return expression.empty() ? std::string("0ull") : expression;
        }

        /// A float as a C++ literal. `8` is an integer constant and `8f` is not a
        /// valid one, so a whole number still needs its point.
        std::string FloatLiteral(float value)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));

            std::string text(buffer);

            if (text.find('.') == std::string::npos && text.find('e') == std::string::npos &&
                text.find("inf") == std::string::npos && text.find("nan") == std::string::npos)
            {
                text += ".0";
            }

            return text + "f";
        }

        std::string LayoutValueExpression(const LayoutValue& value)
        {
            switch (value.type)
            {
            case LayoutValueType::Auto:
                return "RoboRenderer::LayoutValue::Auto()";

            case LayoutValueType::Content:
                return "RoboRenderer::LayoutValue::Content()";

            case LayoutValueType::Fixed:
                return "RoboRenderer::LayoutValue::Fixed(" + FloatLiteral(value.value) + ")";

            case LayoutValueType::ParentRelative:
            case LayoutValueType::ViewportRelative:
                return "RoboRenderer::LayoutValue{" + FloatLiteral(value.value) +
                       ", RoboRenderer::LayoutValueType::" +
                       (value.type == LayoutValueType::ParentRelative ? "ParentRelative" : "ViewportRelative") +
                       "}";
            }

            return "RoboRenderer::LayoutValue::Auto()";
        }

        std::string ColorExpression(const Color& color)
        {
            std::ostringstream out;
            out << "RoboRenderer::Color{" << static_cast<int>(color.red) << ", " << static_cast<int>(color.green)
                << ", " << static_cast<int>(color.blue) << ", " << static_cast<int>(color.alpha) << "}";
            return out.str();
        }

        const char* DisplayName(DisplayKind display)
        {
            switch (display)
            {
            case DisplayKind::Block: return "Block";
            case DisplayKind::Inline: return "Inline";
            case DisplayKind::InlineBlock: return "InlineBlock";
            case DisplayKind::TableRow: return "TableRow";
            case DisplayKind::TableCell: return "TableCell";
            case DisplayKind::Hidden: return "Hidden";
            }

            return "Block";
        }

        const char* TextAlignName(TextAlignKind align)
        {
            switch (align)
            {
            case TextAlignKind::Start: return "Start";
            case TextAlignKind::Center: return "Center";
            case TextAlignKind::End: return "End";
            }

            return "Start";
        }

        const char* OverflowName(OverflowKind overflow)
        {
            switch (overflow)
            {
            case OverflowKind::Visible: return "Visible";
            case OverflowKind::Clip: return "Clip";
            case OverflowKind::Scroll: return "Scroll";
            }

            return "Visible";
        }

        const char* FontSlantName(FontSlantKind slant)
        {
            return slant == FontSlantKind::Italic ? "Italic" : "Upright";
        }

        const char* WhiteSpaceName(WhiteSpaceKind space)
        {
            switch (space)
            {
            case WhiteSpaceKind::Normal: return "Normal";
            case WhiteSpaceKind::NoWrap: return "NoWrap";
            case WhiteSpaceKind::Pre: return "Pre";
            case WhiteSpaceKind::PreWrap: return "PreWrap";
            case WhiteSpaceKind::PreLine: return "PreLine";
            }

            return "Normal";
        }

        const char* TextTransformName(TextTransformKind transform)
        {
            switch (transform)
            {
            case TextTransformKind::None: return "None";
            case TextTransformKind::Uppercase: return "Uppercase";
            case TextTransformKind::Lowercase: return "Lowercase";
            case TextTransformKind::Capitalize: return "Capitalize";
            }

            return "None";
        }

        const char* DirectionName(TextDirectionKind direction)
        {
            switch (direction)
            {
            case TextDirectionKind::Auto: return "Auto";
            case TextDirectionKind::LeftToRight: return "LeftToRight";
            case TextDirectionKind::RightToLeft: return "RightToLeft";
            }

            return "Auto";
        }

        const char* FontKerningName(FontKerningKind kerning)
        {
            switch (kerning)
            {
            case FontKerningKind::Auto: return "Auto";
            case FontKerningKind::Normal: return "Normal";
            case FontKerningKind::None: return "None";
            }

            return "Auto";
        }

        /// Emits only the edges a style specified. An unspecified edge keeps the
        /// native default, which is what makes `margin-top: 8px` mean what it says.
        void EmitEdge(std::ostringstream& source,
                      std::uint32_t index,
                      const char* field,
                      StyleFlags top,
                      StyleFlags right,
                      StyleFlags bottom,
                      StyleFlags left,
                      std::uint64_t specified,
                      const Edges& edges)
        {
            struct Edge
            {
                StyleFlags flag;
                const char* name;
                float value;
            };

            const Edge entries[4] = { { top, "top", edges.top },
                                      { right, "right", edges.right },
                                      { bottom, "bottom", edges.bottom },
                                      { left, "left", edges.left } };

            for (const Edge& edge : entries)
            {
                if (!IsSpecified(specified, edge.flag))
                {
                    continue;
                }

                source << "            tables.styles[" << index << "]." << field << "." << edge.name << " = "
                       << FloatLiteral(edge.value) << ";\n";
            }
        }

        const char* ButtonBehaviorName(ButtonBehavior behavior)
        {
            switch (behavior)
            {
            case ButtonBehavior::Submit: return "Submit";
            case ButtonBehavior::Reset: return "Reset";
            case ButtonBehavior::None: return "None";
            }
            return "Submit";
        }

        /// Human readable flag list for the generated comments, so the emitted
        /// data can be read without decoding the flag word.
        std::string FlagsComment(std::uint64_t flags)
        {
            if (flags == 0)
            {
                return std::string();
            }

            const std::string expression = FlagsExpression(flags);
            std::string comment;
            const std::string prefix = "RoboRenderer::ElementFlag";

            std::size_t cursor = 0;
            while (cursor < expression.size())
            {
                std::size_t end = expression.find(" | ", cursor);
                if (end == std::string::npos)
                {
                    end = expression.size();
                }

                const std::string token = expression.substr(cursor, end - cursor);
                if (token.compare(0, prefix.size(), prefix) == 0)
                {
                    // CamelCase -> lower-case-with-dashes, for a compact comment.
                    std::string readable;
                    for (std::size_t index = prefix.size(); index < token.size(); ++index)
                    {
                        const char character = token[index];
                        if (character >= 'A' && character <= 'Z' && !readable.empty())
                        {
                            readable += '-';
                            readable += static_cast<char>(character - 'A' + 'a');
                        }
                        else
                        {
                            readable += character;
                        }
                    }

                    if (!comment.empty())
                    {
                        comment += ' ';
                    }
                    comment += readable;
                }

                cursor = end + 3;
            }

            return comment;
        }
    }

    RuntimeTargetGenerator::RuntimeTargetGenerator(DiagnosticSink& diagnosticSink, const Registry& elementRegistry)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
    {
    }

    bool RuntimeTargetGenerator::Generate(const NativeUiModel& model,
                                          const std::string& applicationName,
                                          const std::filesystem::path& outputDirectory,
                                          RuntimeTargetResult& result,
                                          const CssStyleTable* authoredStyles)
    {
        // Diagnostics and generated comments name categories with the compiler's
        // own vocabulary: the name table is compiler knowledge, and the renderer
        // takes it through an installed function instead of embedding a copy.
        ConfigureElementKindNames(static_cast<ElementKindNameFunction>(&RoboHTMLCompiler::ToString));

        CompiledUiBuilder builder(diagnostics);
        CompiledUiStorage storage;

        if (!builder.Build(model, applicationName, storage, authoredStyles))
        {
            return false;
        }

        const CompiledDocument document = storage.View();

        // The compiler validates its own runtime output with the runtime's
        // validator. A compiled document that the renderer would refuse is a
        // compiler failure here, not a surprise in the field.
        RendererDiagnostics validation;
        if (!ValidateCompiledDocument(document, validation))
        {
            diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                              SourceLocation{ applicationName + ".html", 1, 1, 0 },
                              "The compiled UI failed runtime validation.",
                              validation.Format(),
                              "This is a compiler defect: report the document and the diagnostics above.");
            return false;
        }

        // Comments in the generated table name the source element, which makes a
        // compiled document readable when someone has to inspect it.
        std::vector<const NativeElement*> sourceNodes;
        sourceNodes.push_back(model.root.get());
        model.root->CollectDescendants(sourceNodes);

        const std::string generatedNamespace = CodeGeneration::NamespaceNameFor(applicationName);
        const std::string headerName = applicationName + "CompiledUi.h";

        const std::uint32_t elementCount = document.elementCount;
        const std::uint32_t resourceCount = document.resourceCount != 0 ? document.resourceCount : 1;
        const std::uint32_t stringCount = document.stringCount != 0 ? document.stringCount : 1;
        const std::uint32_t bindingCount = document.bindingCount != 0 ? document.bindingCount : 1;
        const std::uint32_t identifierCount = document.identifierCount != 0 ? document.identifierCount : 1;
        const std::uint32_t styleCount = document.styleCount != 0 ? document.styleCount : 1;

        std::ostringstream header;
        header << "// ---------------------------------------------------------------------------\n";
        header << "// GENERATED FILE - produced by RoboHTMLCompiler for RoboRenderer.\n";
        header << "//\n";
        header << "// The compiled UI of the '" << applicationName << "' application, as flat\n";
        header << "// data. It is consumed by RoboRenderer, which performs native layout, text\n";
        header << "// measurement, hit testing, input handling and GPU submission at run time.\n";
        header << "// No HTML, CSS or JavaScript is parsed at run time.\n";
        header << "// ---------------------------------------------------------------------------\n";
        header << "#pragma once\n\n";
        header << "#include \"RoboRenderer/CompiledUi.h\"\n\n";
        header << "#include <cstdint>\n\n";
        header << "namespace " << generatedNamespace << "\n{\n";
        header << "    /// The compiled UI data of this application. Validation of the data is the\n";
        header << "    /// renderer's job: RoboRenderer::RoboRenderer checks it before the first\n";
        header << "    /// frame and refuses to draw from data it could not validate.\n";
        header << "    const RoboRenderer::CompiledDocument& CompiledUi();\n\n";
        header << "    constexpr std::uint32_t CompiledElementCount = " << document.elementCount << ";\n";
        header << "    constexpr std::uint32_t CompiledStringCount = " << document.stringCount << ";\n";
        header << "    constexpr std::uint32_t CompiledResourceCount = " << document.resourceCount << ";\n";
        header << "    constexpr std::uint32_t CompiledBindingCount = " << document.bindingCount << ";\n";
        header << "    constexpr std::uint32_t CompiledIdentifierCount = " << document.identifierCount << ";\n";
        header << "    constexpr std::uint32_t CompiledStyleCount = " << document.styleCount << ";\n";
        header << "}\n";

        std::ostringstream source;
        source << "// ---------------------------------------------------------------------------\n";
        source << "// GENERATED FILE - produced by RoboHTMLCompiler for RoboRenderer.\n";
        source << "// Do not edit: recompile the document instead.\n";
        source << "// ---------------------------------------------------------------------------\n";
        source << "#include \"" << headerName << "\"\n\n";
        source << "#include <cstdint>\n\n";
        source << "namespace " << generatedNamespace << "\n{\n";
        source << "    namespace\n    {\n";
        source << "        /// Every table is contiguous and complete: the runtime never allocates\n";
        source << "        /// while it walks the document, and a traversal is a forward walk over\n";
        source << "        /// neighbouring cache lines.\n";
        source << "        struct CompiledTables\n        {\n";
        source << "            RoboRenderer::CompiledElement elements[" << elementCount << "];\n";
        source << "            RoboRenderer::CompiledResource resources[" << resourceCount << "];\n";
        source << "            RoboRenderer::CompiledBinding bindings[" << bindingCount << "];\n";
        source << "            RoboRenderer::CompiledIdEntry identifiers[" << identifierCount << "];\n";
        source << "            RoboRenderer::CompiledStyle styles[" << styleCount << "];\n";
        source << "            const char* strings[" << stringCount << "];\n";
        source << "        };\n\n";
        source << "        /// constexpr: the table is built by the compiler, checked by a normal\n";
        source << "        /// C++ compiler and placed in read-only data. Nothing runs at startup.\n";
        source << "        constexpr CompiledTables BuildTables()\n        {\n";
        source << "            CompiledTables tables{};\n\n";

        source << "            // Interned strings. Index 0 is the empty string, so a zero string\n";
        source << "            // index always means \"no text\".\n";
        const std::vector<std::string>& strings = storage.Strings();
        for (std::size_t index = 0; index < strings.size(); ++index)
        {
            source << "            tables.strings[" << index << "] = "
                   << CodeGeneration::StringLiteral(strings[index]) << ";\n";
        }
        source << "\n";

        if (document.resourceCount > 0)
        {
            source << "            // Statically discovered resources. Elements reference them by index,\n";
            source << "            // so a file referenced twice is loaded and decoded once.\n";
            for (std::uint32_t index = 0; index < document.resourceCount; ++index)
            {
                const CompiledResource& resource = document.resources[index];
                source << "            // #" << index << " " << RoboRenderer::ToString(resource.kind) << " \""
                       << StringAt(document, resource.source) << "\"\n";
                source << "            tables.resources[" << index << "].source = " << resource.source << ";\n";
                source << "            tables.resources[" << index << "].kind = RoboRenderer::ResourceKind::"
                       << RoboRenderer::ToString(resource.kind) << ";\n";
                source << "            tables.resources[" << index << "].inferredKind = RoboRenderer::ResourceKind::"
                       << RoboRenderer::ToString(resource.inferredKind) << ";\n";
                if (resource.declaredWidth != 0)
                {
                    source << "            tables.resources[" << index << "].declaredWidth = "
                           << resource.declaredWidth << ";\n";
                }
                if (resource.declaredHeight != 0)
                {
                    source << "            tables.resources[" << index << "].declaredHeight = "
                           << resource.declaredHeight << ";\n";
                }
                if (resource.hasAlternativeText != 0)
                {
                    source << "            tables.resources[" << index << "].hasAlternativeText = 1;\n";
                }
            }
            source << "\n";
        }

        source << "            // Elements, in document order. Index 0 is the root.\n";
        for (std::uint32_t index = 0; index < document.elementCount; ++index)
        {
            const CompiledElement& element = document.elements[index];
            const char* tag = (index < sourceNodes.size() && sourceNodes[index] != nullptr)
                                  ? sourceNodes[index]->source.tag.c_str()
                                  : "";

            source << "            // #" << index << " " << ElementKindName(element.kind) << " <" << tag
                   << "> line " << element.sourceLine;
            const std::string flags = FlagsComment(element.flags);
            if (!flags.empty())
            {
                source << "  [" << flags << "]";
            }
            source << "\n";

            source << "            tables.elements[" << index << "].kind = RoboRenderer::ElementKind::"
                   << ElementKindName(element.kind) << ";\n";

            if (element.flags != 0)
            {
                source << "            tables.elements[" << index << "].flags = " << FlagsExpression(element.flags)
                       << ";\n";
            }
            if (element.styleIndex != 0)
            {
                source << "            tables.elements[" << index << "].styleIndex = " << element.styleIndex << ";\n";
            }
            if (element.headingLevel != 0)
            {
                source << "            tables.elements[" << index << "].headingLevel = " << element.headingLevel
                       << ";\n";
            }
            if (element.columnSpan != 1)
            {
                source << "            tables.elements[" << index << "].columnSpan = " << element.columnSpan << ";\n";
            }
            if (element.rowSpan != 1)
            {
                source << "            tables.elements[" << index << "].rowSpan = " << element.rowSpan << ";\n";
            }
            if (element.columns != 0)
            {
                source << "            tables.elements[" << index << "].columns = " << element.columns << ";\n";
            }
            if (element.rows != 0)
            {
                source << "            tables.elements[" << index << "].rows = " << element.rows << ";\n";
            }
            if (element.tabIndex != 0)
            {
                source << "            tables.elements[" << index << "].tabIndex = " << element.tabIndex << ";\n";
            }
            if (element.buttonBehavior != ButtonBehavior::Submit)
            {
                source << "            tables.elements[" << index
                       << "].buttonBehavior = RoboRenderer::ButtonBehavior::"
                       << ButtonBehaviorName(element.buttonBehavior) << ";\n";
            }
            if (element.listStart != 1)
            {
                source << "            tables.elements[" << index << "].listStart = " << element.listStart << ";\n";
            }

            // Tree links are always emitted, including the `NoElement`
            // sentinel. A generated table is zero initialised, and zero is a
            // valid element index, so an omitted link would silently point at
            // the root instead of at nothing - which is a cycle, not a leaf.
            const auto link = [](ElementIndex value) {
                return value == NoElement ? std::string("RoboRenderer::NoElement") : std::to_string(value);
            };

            source << "            tables.elements[" << index << "].parent = " << link(element.parent) << ";\n";
            source << "            tables.elements[" << index << "].firstChild = " << link(element.firstChild)
                   << ";\n";
            source << "            tables.elements[" << index << "].lastChild = " << link(element.lastChild)
                   << ";\n";
            source << "            tables.elements[" << index << "].nextSibling = " << link(element.nextSibling)
                   << ";\n";

            if (element.text != 0)
            {
                source << "            tables.elements[" << index << "].text = " << element.text << ";  // "
                       << CodeGeneration::StringLiteral(StringAt(document, element.text)) << "\n";
            }
            if (element.identifier != 0)
            {
                source << "            tables.elements[" << index << "].identifier = " << element.identifier
                       << ";\n";
            }
            if (element.name != 0)
            {
                source << "            tables.elements[" << index << "].name = " << element.name << ";\n";
            }
            if (element.auxiliaryText != 0)
            {
                source << "            tables.elements[" << index << "].auxiliaryText = " << element.auxiliaryText
                       << ";\n";
            }
            if (element.value != 0)
            {
                source << "            tables.elements[" << index << "].value = " << element.value << ";\n";
            }
            if (element.href != 0)
            {
                source << "            tables.elements[" << index << "].href = " << element.href << ";\n";
            }
            if (element.labelTarget != 0)
            {
                source << "            tables.elements[" << index << "].labelTarget = " << element.labelTarget
                       << ";\n";
            }
            if (element.listStyle != 0)
            {
                source << "            tables.elements[" << index << "].listStyle = " << element.listStyle
                       << ";  // " << StringAt(document, element.listStyle) << "\n";
            }
            if (element.resource != 0)
            {
                source << "            tables.elements[" << index << "].resource = " << element.resource << ";\n";
            }
            if (element.HasFlag(ElementFlagHasWidth))
            {
                source << "            tables.elements[" << index << "].attributeWidth = "
                       << CodeGeneration::FloatLiteral(element.attributeWidth) << ";\n";
            }
            if (element.HasFlag(ElementFlagHasHeight))
            {
                source << "            tables.elements[" << index << "].attributeHeight = "
                       << CodeGeneration::FloatLiteral(element.attributeHeight) << ";\n";
            }
            if (element.HasFlag(ElementFlagHasNumericValue))
            {
                source << "            tables.elements[" << index << "].numericValue = "
                       << CodeGeneration::FloatLiteral(element.numericValue) << ";\n";
            }
            if (element.HasFlag(ElementFlagHasMinimum))
            {
                source << "            tables.elements[" << index << "].minimum = "
                       << CodeGeneration::FloatLiteral(element.minimum) << ";\n";
            }
            if (element.HasFlag(ElementFlagHasMaximum))
            {
                source << "            tables.elements[" << index << "].maximum = "
                       << CodeGeneration::FloatLiteral(element.maximum) << ";\n";
            }
            if (element.bindingCount != 0)
            {
                source << "            tables.elements[" << index << "].firstBinding = " << element.firstBinding
                       << ";\n";
                source << "            tables.elements[" << index << "].bindingCount = " << element.bindingCount
                       << ";\n";
            }
            if (element.sourceLine != 0)
            {
                source << "            tables.elements[" << index << "].sourceLine = " << element.sourceLine
                       << ";\n";
                source << "            tables.elements[" << index << "].sourceColumn = " << element.sourceColumn
                       << ";\n";
            }
        }
        source << "\n";

        if (document.identifierCount > 0)
        {
            source << "            // Identifiers, sorted by text: the runtime binary searches them.\n";
            for (std::uint32_t index = 0; index < document.identifierCount; ++index)
            {
                const CompiledIdEntry& entry = document.identifiers[index];
                source << "            tables.identifiers[" << index << "].identifier = " << entry.identifier
                       << ";  // " << StringAt(document, entry.identifier) << "\n";
                source << "            tables.identifiers[" << index << "].element = " << entry.element << ";\n";
            }
            source << "\n";
        }

        if (document.bindingCount > 0)
        {
            source << "            // Behaviour bindings, grouped by element.\n";
            for (std::uint32_t index = 0; index < document.bindingCount; ++index)
            {
                const CompiledBinding& binding = document.bindings[index];
                source << "            tables.bindings[" << index << "].element = " << binding.element << ";\n";
                source << "            tables.bindings[" << index << "].event = RoboRenderer::EventKind::"
                       << RoboRenderer::ToString(binding.event) << ";\n";
                source << "            tables.bindings[" << index << "].handler = " << binding.handler
                       << ";  // " << StringAt(document, binding.handler) << "\n";
            }
            source << "\n";
        }

        if (document.styleCount > 0)
        {
            source << "            // The document's styles, as the CSS compiler resolved them. Each\n";
            source << "            // one is sparse: `specified` names the fields the stylesheet\n";
            source << "            // decided, and the native default fills the rest.\n";

            for (std::uint32_t index = 0; index < document.styleCount; ++index)
            {
                const CompiledStyle& style = document.styles[index];

                source << "            // #" << index;

                if (index == 0)
                {
                    source << "  no authored style: the native default for the element's category\n";
                    continue;
                }

                source << "  " << StyleFlagsExpression(style.specified) << "\n";

                source << "            tables.styles[" << index << "].specified = "
                       << StyleFlagsExpression(style.specified) << ";\n";
                source << "            tables.styles[" << index << "].sourceStyleId = " << style.sourceStyleId
                       << ";\n";

                if (IsSpecified(style.specified, StyleFlagWidth))
                {
                    source << "            tables.styles[" << index << "].width = " << LayoutValueExpression(style.width)
                           << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagHeight))
                {
                    source << "            tables.styles[" << index << "].height = "
                           << LayoutValueExpression(style.height) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagMinimumWidth))
                {
                    source << "            tables.styles[" << index << "].minimumWidth = "
                           << LayoutValueExpression(style.minimumWidth) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagMinimumHeight))
                {
                    source << "            tables.styles[" << index << "].minimumHeight = "
                           << LayoutValueExpression(style.minimumHeight) << ";\n";
                }

                EmitEdge(source, index, "margin", StyleFlagMarginTop, StyleFlagMarginRight, StyleFlagMarginBottom,
                         StyleFlagMarginLeft, style.specified, style.margin);
                EmitEdge(source, index, "padding", StyleFlagPaddingTop, StyleFlagPaddingRight,
                         StyleFlagPaddingBottom, StyleFlagPaddingLeft, style.specified, style.padding);
                EmitEdge(source, index, "borderWidth", StyleFlagBorderTopWidth, StyleFlagBorderRightWidth,
                         StyleFlagBorderBottomWidth, StyleFlagBorderLeftWidth, style.specified, style.borderWidth);

                if (IsSpecified(style.specified, StyleFlagBorderRadius))
                {
                    source << "            tables.styles[" << index
                           << "].borderRadius = " << FloatLiteral(style.borderRadius) << ";\n";
                }

                if (IsSpecified(style.specified, StyleFlagBackground))
                {
                    source << "            tables.styles[" << index << "].background = "
                           << ColorExpression(style.background) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagForeground))
                {
                    source << "            tables.styles[" << index << "].foreground = "
                           << ColorExpression(style.foreground) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagBorderColor))
                {
                    source << "            tables.styles[" << index << "].borderColor = "
                           << ColorExpression(style.borderColor) << ";\n";
                }

                if (IsSpecified(style.specified, StyleFlagFontSize))
                {
                    source << "            tables.styles[" << index
                           << "].fontSize = " << FloatLiteral(style.fontSize) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagFontWeight))
                {
                    source << "            tables.styles[" << index << "].fontWeight = RoboRenderer::FontWeightKind::"
                           << (style.fontWeight == FontWeightKind::Bold ? "Bold" : "Normal") << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagLetterSpacing))
                {
                    source << "            tables.styles[" << index
                           << "].letterSpacing = " << FloatLiteral(style.letterSpacing) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagTextAlign))
                {
                    source << "            tables.styles[" << index << "].textAlign = RoboRenderer::TextAlignKind::"
                           << TextAlignName(style.textAlign) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagLineHeight))
                {
                    source << "            tables.styles[" << index
                           << "].lineHeight = " << FloatLiteral(style.lineHeight) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagWordSpacing))
                {
                    source << "            tables.styles[" << index
                           << "].wordSpacing = " << FloatLiteral(style.wordSpacing) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagTextIndent))
                {
                    source << "            tables.styles[" << index
                           << "].textIndent = " << FloatLiteral(style.textIndent) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagWhiteSpace))
                {
                    source << "            tables.styles[" << index
                           << "].whiteSpace = RoboRenderer::WhiteSpaceKind::" << WhiteSpaceName(style.whiteSpace)
                           << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagTextTransform))
                {
                    source << "            tables.styles[" << index
                           << "].textTransform = RoboRenderer::TextTransformKind::"
                           << TextTransformName(style.textTransform) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagDirection))
                {
                    source << "            tables.styles[" << index
                           << "].direction = RoboRenderer::TextDirectionKind::"
                           << DirectionName(style.direction) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagFontStyle))
                {
                    source << "            tables.styles[" << index
                           << "].fontStyle = RoboRenderer::FontSlantKind::" << FontSlantName(style.fontStyle)
                           << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagFontFamily))
                {
                    source << "            tables.styles[" << index
                           << "].fontFamily = " << style.fontFamily << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagFontKerning))
                {
                    source << "            tables.styles[" << index
                           << "].fontKerning = RoboRenderer::FontKerningKind::"
                           << FontKerningName(style.fontKerning) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagFontFeatures))
                {
                    source << "            tables.styles[" << index
                           << "].fontFeatures = " << style.fontFeatures << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagTextDecoration))
                {
                    source << "            tables.styles[" << index << "].textDecoration = "
                           << static_cast<unsigned>(style.textDecoration) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagTextDecorationColor))
                {
                    source << "            tables.styles[" << index
                           << "].textDecorationColor = " << ColorExpression(style.textDecorationColor)
                           << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagDisplay))
                {
                    source << "            tables.styles[" << index << "].display = RoboRenderer::DisplayKind::"
                           << DisplayName(style.display) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagOverflowX))
                {
                    source << "            tables.styles[" << index << "].overflowX = RoboRenderer::OverflowKind::"
                           << OverflowName(style.overflowX) << ";\n";
                }
                if (IsSpecified(style.specified, StyleFlagOverflowY))
                {
                    source << "            tables.styles[" << index << "].overflowY = RoboRenderer::OverflowKind::"
                           << OverflowName(style.overflowY) << ";\n";
                }

                if (style.hasBackground != 0)
                {
                    source << "            tables.styles[" << index << "].hasBackground = 1;\n";
                }
                if (style.hasForeground != 0)
                {
                    source << "            tables.styles[" << index << "].hasForeground = 1;\n";
                }
                if (style.hasBorder != 0)
                {
                    source << "            tables.styles[" << index << "].hasBorder = 1;\n";
                }
                if (style.hasFontSize != 0)
                {
                    source << "            tables.styles[" << index << "].hasFontSize = 1;\n";
                }
            }

            source << "\n";
        }

        source << "            return tables;\n        }\n\n";
        source << "        constexpr CompiledTables Tables = BuildTables();\n\n";
        source << "        /// The compiled document. Every pointer inside points at a constexpr\n";
        source << "        /// table above: the whole UI is in read-only data.\n";
        source << "        constexpr RoboRenderer::CompiledDocument Document = {\n";
        source << "            " << CodeGeneration::StringLiteral(applicationName) << ",  // application name\n";
        source << "            Tables.elements,   // elements\n";
        source << "            " << document.elementCount << ",   // element count\n";
        source << "            Tables.strings,    // interned strings\n";
        source << "            " << document.stringCount << ",   // string count\n";
        source << "            Tables.resources,  // resources\n";
        source << "            " << document.resourceCount << ",   // resource count\n";
        source << "            Tables.bindings,   // bindings\n";
        source << "            " << document.bindingCount << ",   // binding count\n";
        source << "            Tables.identifiers, // identifiers\n";
        source << "            " << document.identifierCount << ",   // identifier count\n";
        source << "            Tables.styles,     // resolved styles\n";
        source << "            " << document.styleCount << ",   // style count\n";
        source << "            " << document.root << ",   // root element index\n";
        source << "            " << CodeGeneration::StringLiteral(CompiledUiBuilder::CompilerVersion())
               << ",  // compiler version\n";
        source << "            " << (document.hasSourcePositions != 0 ? 1 : 0) << "    // source positions\n";
        source << "        };\n";
        source << "    }\n\n";
        source << "    const RoboRenderer::CompiledDocument& CompiledUi()\n    {\n";
        source << "        return Document;\n    }\n";
        source << "}\n";

        const std::filesystem::path headerPath = outputDirectory / headerName;
        const std::filesystem::path sourcePath = outputDirectory / (applicationName + "CompiledUi.cpp");

        WriteFile(headerPath, header.str());
        WriteFile(sourcePath, source.str());

        result.headerPath = headerPath;
        result.sourcePath = sourcePath;
        result.elementCount = document.elementCount;
        result.resourceCount = document.resourceCount;
        result.stringCount = document.stringCount;
        result.bindingCount = document.bindingCount;
        result.generated = true;
        return true;
    }
}
