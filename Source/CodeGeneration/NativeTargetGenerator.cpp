#include "RoboHTMLCompiler/CodeGenerator.h"

#include "JsonEmit.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>

namespace RoboHTMLCompiler
{
    namespace
    {
        void WriteFile(const std::filesystem::path& path, const std::string& contents)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream << contents;
        }

        std::string NamespaceNameFor(const std::string& applicationName)
        {
            std::string name = applicationName.empty() ? std::string("Application") : applicationName;

            std::string sanitized;
            sanitized.reserve(name.size());
            for (const char character : name)
            {
                if (std::isalnum(static_cast<unsigned char>(character)) != 0)
                {
                    sanitized.push_back(character);
                }
            }

            if (sanitized.empty())
            {
                sanitized = "Application";
            }

            if (std::isdigit(static_cast<unsigned char>(sanitized[0])) != 0)
            {
                sanitized.insert(sanitized.begin(), 'A');
            }

            return sanitized + "Generated";
        }

        std::string StringLiteral(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (const char character : value)
            {
                if (character == '"' || character == '\\')
                {
                    escaped.push_back('\\');
                }
                escaped.push_back(character);
            }
            return "\"" + escaped + "\"";
        }

        void EmitElement(std::ostringstream& output,
                         const NativeElement& element,
                         int depth,
                         int& index,
                         std::vector<std::pair<int, int>>& parents);

        void EmitAssignment(std::ostringstream& output, int depth, int index, const std::string& field,
                            const std::string& expression)
        {
            output << std::string(static_cast<std::size_t>(depth), ' ') << "node" << index << "->" << field << " = "
                   << expression << ";\n";
        }

        void EmitAttributes(std::ostringstream& output,
                            const NativeElement& element,
                            int depth,
                            int index)
        {
            const std::string indentation(static_cast<std::size_t>(depth), ' ');

            if (element.state.hidden) EmitAssignment(output, depth, index, "state.hidden", "true");
            if (element.state.disabled) EmitAssignment(output, depth, index, "state.disabled", "true");
            if (element.state.checked) EmitAssignment(output, depth, index, "state.checked", "true");
            if (element.state.selected) EmitAssignment(output, depth, index, "state.selected", "true");
            if (element.state.readOnly) EmitAssignment(output, depth, index, "state.readOnly", "true");
            if (element.state.required) EmitAssignment(output, depth, index, "state.required", "true");
            if (element.state.inert) EmitAssignment(output, depth, index, "state.inert", "true");
            if (element.state.open) EmitAssignment(output, depth, index, "state.open", "true");
            if (element.state.multiple) EmitAssignment(output, depth, index, "state.multiple", "true");
            if (element.state.autofocus) EmitAssignment(output, depth, index, "state.autofocus", "true");

            if (!element.content.id.empty())
            {
                EmitAssignment(output, depth, index, "content.id", StringLiteral(element.content.id));
            }
            if (!element.content.name.empty())
            {
                EmitAssignment(output, depth, index, "content.name", StringLiteral(element.content.name));
            }
            if (!element.content.text.empty())
            {
                EmitAssignment(output, depth, index, "content.text", StringLiteral(element.content.text));
            }
            if (!element.content.value.empty())
            {
                EmitAssignment(output, depth, index, "content.value", StringLiteral(element.content.value));
            }
            if (!element.content.placeholder.empty())
            {
                EmitAssignment(output, depth, index, "content.placeholder", StringLiteral(element.content.placeholder));
            }
            if (!element.content.alternativeText.empty())
            {
                EmitAssignment(output, depth, index, "content.alternativeText",
                               StringLiteral(element.content.alternativeText));
            }
            if (!element.content.language.empty())
            {
                EmitAssignment(output, depth, index, "content.language", StringLiteral(element.content.language));
            }
            if (!element.content.direction.empty())
            {
                EmitAssignment(output, depth, index, "content.direction", StringLiteral(element.content.direction));
            }

            if (element.layout.hasWidth)
            {
                EmitAssignment(output, depth, index, "layout.width",
                               std::to_string(element.layout.width) + "u");
                EmitAssignment(output, depth, index, "layout.hasWidth", "true");
            }
            if (element.layout.hasHeight)
            {
                EmitAssignment(output, depth, index, "layout.height",
                               std::to_string(element.layout.height) + "u");
                EmitAssignment(output, depth, index, "layout.hasHeight", "true");
            }
            if (element.layout.span != 1)
            {
                EmitAssignment(output, depth, index, "layout.span", std::to_string(element.layout.span) + "u");
            }
            if (element.layout.rowSpan != 1)
            {
                EmitAssignment(output, depth, index, "layout.rowSpan", std::to_string(element.layout.rowSpan) + "u");
            }
            if (element.layout.columnSpan != 1)
            {
                EmitAssignment(output, depth, index, "layout.columnSpan",
                               std::to_string(element.layout.columnSpan) + "u");
            }

            if (!element.validation.pattern.empty())
            {
                EmitAssignment(output, depth, index, "validation.pattern", StringLiteral(element.validation.pattern));
            }
            if (!element.validation.accept.empty())
            {
                EmitAssignment(output, depth, index, "validation.accept", StringLiteral(element.validation.accept));
            }
            if (!element.constraints.minimum.empty())
            {
                EmitAssignment(output, depth, index, "constraints.minimum",
                               StringLiteral(element.constraints.minimum));
            }
            if (!element.constraints.maximum.empty())
            {
                EmitAssignment(output, depth, index, "constraints.maximum",
                               StringLiteral(element.constraints.maximum));
            }
            if (!element.constraints.step.empty())
            {
                EmitAssignment(output, depth, index, "constraints.step", StringLiteral(element.constraints.step));
            }

            if (!element.navigation.href.empty())
            {
                EmitAssignment(output, depth, index, "navigation.href", StringLiteral(element.navigation.href));
            }
            if (!element.navigation.target.empty())
            {
                EmitAssignment(output, depth, index, "navigation.target", StringLiteral(element.navigation.target));
            }
            if (!element.navigation.relation.empty())
            {
                EmitAssignment(output, depth, index, "navigation.relation", StringLiteral(element.navigation.relation));
            }

            if (!element.resource.source.empty())
            {
                EmitAssignment(output, depth, index, "resource.source", StringLiteral(element.resource.source));
            }
            if (!element.resource.sourceSet.empty())
            {
                EmitAssignment(output, depth, index, "resource.sourceSet", StringLiteral(element.resource.sourceSet));
            }
            if (!element.resource.crossOrigin.empty())
            {
                EmitAssignment(output, depth, index, "resource.crossOrigin",
                               StringLiteral(element.resource.crossOrigin));
            }

            if (element.media.controls) EmitAssignment(output, depth, index, "media.controls", "true");
            if (element.media.autoPlay) EmitAssignment(output, depth, index, "media.autoPlay", "true");
            if (element.media.loop) EmitAssignment(output, depth, index, "media.loop", "true");
            if (element.media.muted) EmitAssignment(output, depth, index, "media.muted", "true");
            if (!element.media.source.empty())
            {
                EmitAssignment(output, depth, index, "media.source", StringLiteral(element.media.source));
            }
            if (!element.media.poster.empty())
            {
                EmitAssignment(output, depth, index, "media.poster", StringLiteral(element.media.poster));
            }

            if (!element.accessibility.role.empty())
            {
                EmitAssignment(output, depth, index, "accessibility.role",
                               StringLiteral(element.accessibility.role));
            }
            if (!element.accessibility.label.empty())
            {
                EmitAssignment(output, depth, index, "accessibility.label",
                               StringLiteral(element.accessibility.label));
            }
            if (!element.accessibility.labelledBy.empty())
            {
                EmitAssignment(output, depth, index, "accessibility.labelledBy",
                               StringLiteral(element.accessibility.labelledBy));
            }
            if (!element.accessibility.describedBy.empty())
            {
                EmitAssignment(output, depth, index, "accessibility.describedBy",
                               StringLiteral(element.accessibility.describedBy));
            }
            if (!element.accessibility.live.empty())
            {
                EmitAssignment(output, depth, index, "accessibility.live",
                               StringLiteral(element.accessibility.live));
            }
            if (element.accessibility.hidden)
            {
                EmitAssignment(output, depth, index, "accessibility.hidden", "true");
            }

            if (element.interaction.hasTabIndex)
            {
                EmitAssignment(output, depth, index, "interaction.tabIndex",
                               std::to_string(element.interaction.tabIndex));
                EmitAssignment(output, depth, index, "interaction.hasTabIndex", "true");
            }
            if (!element.interaction.command.empty())
            {
                EmitAssignment(output, depth, index, "interaction.command",
                               StringLiteral(element.interaction.command));
            }

            // SVG and MathML properties.
            if (!element.svgGeometry.path.empty())
            {
                EmitAssignment(output, depth, index, "svgGeometry.path", StringLiteral(element.svgGeometry.path));
            }
            if (element.svgGeometry.width != 0.0)
            {
                EmitAssignment(output, depth, index, "svgGeometry.width", CodeGeneration::Quote(element.svgGeometry.width));
            }
            if (element.svgGeometry.height != 0.0)
            {
                EmitAssignment(output, depth, index, "svgGeometry.height", CodeGeneration::Quote(element.svgGeometry.height));
            }
            if (!element.svgGeometry.viewBox.empty())
            {
                EmitAssignment(output, depth, index, "svgGeometry.viewBox", StringLiteral(element.svgGeometry.viewBox));
            }
            if (!element.svgPaint.fill.empty())
            {
                EmitAssignment(output, depth, index, "svgPaint.fill", StringLiteral(element.svgPaint.fill));
            }
            if (!element.svgPaint.stroke.empty())
            {
                EmitAssignment(output, depth, index, "svgPaint.stroke", StringLiteral(element.svgPaint.stroke));
            }
            if (!element.svgTransform.value.empty())
            {
                EmitAssignment(output, depth, index, "svgTransform.value",
                               StringLiteral(element.svgTransform.value));
            }
            if (!element.mathLayout.display.empty())
            {
                EmitAssignment(output, depth, index, "mathLayout.display",
                               StringLiteral(element.mathLayout.display));
            }
            if (!element.mathStyle.color.empty())
            {
                EmitAssignment(output, depth, index, "mathStyle.color", StringLiteral(element.mathStyle.color));
            }

            for (const NativeAttribute& attribute : element.attributes)
            {
                output << indentation << "node" << index << "->attributes.push_back(RoboHTMLCompiler::NativeAttribute{ "
                       << "RoboHTMLCompiler::NativeProperty::" << ToString(attribute.property) << ", "
                       << StringLiteral(attribute.name) << ", " << StringLiteral(attribute.value) << " });\n";
            }

            for (const NativeAttribute& attribute : element.customMetadata.entries)
            {
                output << indentation << "node" << index
                       << "->customMetadata.entries.push_back(RoboHTMLCompiler::NativeAttribute{ "
                       << "RoboHTMLCompiler::NativeProperty::CustomMetadata, " << StringLiteral(attribute.name) << ", "
                       << StringLiteral(attribute.value) << " });\n";
            }
        }

        void EmitElement(std::ostringstream& output,
                         const NativeElement& element,
                         int depth,
                         int& index,
                         std::vector<std::pair<int, int>>& parents)
        {
            const int currentIndex = index++;
            const std::string indentation(static_cast<std::size_t>(depth), ' ');

            output << indentation << "auto node" << currentIndex
                   << " = std::make_unique<RoboHTMLCompiler::NativeElement>();\n";
            output << indentation << "node" << currentIndex
                   << "->kind = RoboHTMLCompiler::ElementKind::" << ToString(element.kind) << ";\n";
            output << indentation << "node" << currentIndex << "->id = " << element.id << "u;\n";

            if (element.styleId != 0)
            {
                output << indentation << "node" << currentIndex << "->styleId = " << element.styleId << "u;\n";
            }

            EmitAttributes(output, element, depth, currentIndex);

            output << indentation << "node" << currentIndex << "->source.tag = " << StringLiteral(element.source.tag)
                   << ";\n";
            output << indentation << "node" << currentIndex << "->source.line = " << element.source.line << ";\n";

            for (const std::unique_ptr<NativeElement>& child : element.children)
            {
                EmitElement(output, *child, depth, index, parents);
                const int childIndex = index - 1;
                output << indentation << "node" << currentIndex << "->AddChild(std::move(node" << childIndex
                       << "));\n";
            }
        }
    }

    NativeTargetGenerator::NativeTargetGenerator(DiagnosticSink& diagnosticSink, const Registry& elementRegistry)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
    {
    }

    std::string NativeTargetGenerator::RenderIrDump(const NativeUiModel& model, const std::string& applicationName)
    {
        std::vector<std::string> lines;
        lines.push_back("{");
        lines.push_back("  \"application\": " + CodeGeneration::Quote(applicationName) + ",");
        lines.push_back("  \"elementCount\": " + std::to_string(model.CountElements()) + ",");
        lines.push_back("  \"styleCount\": " + std::to_string(model.styleTable.size()) + ",");
        lines.push_back("  \"eventBindings\": " + std::to_string(model.eventBindings.size()) + ",");

        std::vector<std::string> styleIds;
        for (std::size_t index = 0; index < model.styleTable.size(); ++index)
        {
            styleIds.push_back("{\"styleId\": " + std::to_string(index + 1) + ", \"declarations\": " +
                               CodeGeneration::Quote(model.styleTable[index]) + "}");
        }
        lines.push_back("  \"styles\": [" + [&styleIds] {
            std::string joined;
            for (std::size_t index = 0; index < styleIds.size(); ++index)
            {
                if (index > 0)
                {
                    joined += ", ";
                }
                joined += styleIds[index];
            }
            return joined;
        }() + "],");

        std::function<std::string(const NativeElement&, int)> render = [&](const NativeElement& element,
                                                                           int depth) -> std::string {
            const std::string indentation(static_cast<std::size_t>(depth) * 2, ' ');
            std::string result;
            result += indentation + "{\n";
            // ToString returns const char*: it is wrapped explicitly so the JSON
            // string overload wins over the boolean one.
            result += indentation + "  \"kind\": " + CodeGeneration::Quote(std::string(ToString(element.kind))) + ",\n";
            result += indentation + "  \"id\": " + std::to_string(element.id) + ",\n";
            result += indentation + "  \"styleId\": " + std::to_string(element.styleId) + ",\n";
            result += indentation + "  \"sourceTag\": " + CodeGeneration::Quote(element.source.tag) + ",\n";
            result += indentation + "  \"namespace\": " + CodeGeneration::Quote(std::string(ToString(element.source.ns))) + ",\n";
            result += indentation + "  \"line\": " + std::to_string(element.source.line) + ",\n";

            std::vector<std::string> stateFlags;
            if (element.state.disabled) stateFlags.push_back("\"disabled\"");
            if (element.state.hidden) stateFlags.push_back("\"hidden\"");
            if (element.state.checked) stateFlags.push_back("\"checked\"");
            if (element.state.selected) stateFlags.push_back("\"selected\"");
            if (element.state.readOnly) stateFlags.push_back("\"readonly\"");
            if (element.state.required) stateFlags.push_back("\"required\"");
            result += indentation + "  \"state\": [" + CodeGeneration::JsonArray(stateFlags) + "],\n";

            std::vector<std::string> attributeEntries;
            for (const NativeAttribute& attribute : element.attributes)
            {
                attributeEntries.push_back("{\"property\": " + CodeGeneration::Quote(std::string(ToString(attribute.property))) +
                                           ", \"name\": " + CodeGeneration::Quote(attribute.name) + ", \"value\": " +
                                           CodeGeneration::Quote(attribute.value) + "}");
            }
            for (const NativeAttribute& attribute : element.customMetadata.entries)
            {
                attributeEntries.push_back("{\"property\": \"CustomMetadata\", \"name\": " +
                                           CodeGeneration::Quote(attribute.name) + ", \"value\": " +
                                           CodeGeneration::Quote(attribute.value) + "}");
            }
            result += indentation + "  \"attributes\": [" + CodeGeneration::JsonArray(attributeEntries) + "],\n";
            result += indentation + "  \"content\": {\"id\": " + CodeGeneration::Quote(element.content.id) +
                      ", \"text\": " + CodeGeneration::Quote(element.content.text) + "},\n";

            result += indentation + "  \"children\": [\n";
            for (std::size_t index = 0; index < element.children.size(); ++index)
            {
                result += render(*element.children[index], depth + 2);
                if (index + 1 != element.children.size())
                {
                    result += ",";
                }
                result += "\n";
            }
            result += indentation + "  ]\n";
            result += indentation + "}";
            return result;
        };

        if (model.root != nullptr)
        {
            lines.push_back("  \"root\": " + render(*model.root, 1) + ",");
        }
        else
        {
            lines.push_back("  \"root\": null,");
        }

        std::vector<std::string> bindings;
        for (const EventBinding& binding : model.eventBindings)
        {
            bindings.push_back("{\"target\": " + CodeGeneration::Quote(binding.targetId) + ", \"event\": " +
                               CodeGeneration::Quote(binding.event) + ", \"handler\": " +
                               CodeGeneration::Quote(binding.handler) + "}");
        }
        lines.push_back("  \"bindings\": [" + CodeGeneration::JsonArray(bindings) + "]");
        lines.push_back("}");

        std::string result;
        for (const std::string& line : lines)
        {
            result += line;
            result += "\n";
        }
        return result;
    }

    bool NativeTargetGenerator::Generate(const NativeUiModel& model,
                                         const std::string& applicationName,
                                         const std::filesystem::path& outputDirectory,
                                         NativeTargetResult& result)
    {
        if (model.root == nullptr)
        {
            diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                              SourceLocation{ applicationName, 1, 1, 0 },
                              "The native model has no root element.",
                              std::string(),
                              "Ensure the application document contains an <html> element.");
            return false;
        }

        const std::string generatedNamespace = NamespaceNameFor(applicationName);

        std::ostringstream header;
        header << "// ---------------------------------------------------------------------------\n";
        header << "// GENERATED FILE - produced by RoboHTMLCompiler for the native target.\n";
        header << "// Strongly typed native UI model for the '" << applicationName << "' application.\n";
        header << "// ---------------------------------------------------------------------------\n";
        header << "#pragma once\n\n";
        header << "#include \"RoboHTMLCompiler/Ir.h\"\n\n";
        header << "#include <memory>\n\n";
        header << "namespace " << generatedNamespace << "\n{\n";
        header << "    /// Builds the native UI model. The native runtime consumes this tree\n";
        header << "    /// directly; it never parses HTML, CSS or JavaScript at run time.\n";
        header << "    std::unique_ptr<RoboHTMLCompiler::NativeElement> BuildNativeModel();\n\n";
        header << "    /// Number of native elements in the generated model.\n";
        header << "    constexpr std::size_t NativeElementCount = " << model.CountElements() << ";\n\n";
        header << "    /// Number of interned styles referenced by the model.\n";
        header << "    constexpr std::size_t StyleCount = " << model.styleTable.size() << ";\n";
        header << "}\n";

        std::ostringstream source;
        source << "// ---------------------------------------------------------------------------\n";
        source << "// GENERATED FILE - produced by RoboHTMLCompiler for the native target.\n";
        source << "// ---------------------------------------------------------------------------\n";
        source << "#include \"" << applicationName << "NativeModel.h\"\n\n";
        source << "#include <memory>\n\n";
        source << "namespace " << generatedNamespace << "\n{\n";
        source << "    std::unique_ptr<RoboHTMLCompiler::NativeElement> BuildNativeModel()\n    {\n";

        int index = 0;
        std::vector<std::pair<int, int>> parents;
        EmitElement(source, *model.root, 2, index, parents);

        source << "        return std::move(node0);\n";
        source << "    }\n";
        source << "}\n";

        const std::filesystem::path headerPath = outputDirectory / (applicationName + "NativeModel.h");
        const std::filesystem::path sourcePath = outputDirectory / (applicationName + "NativeModel.cpp");

        WriteFile(headerPath, header.str());
        WriteFile(sourcePath, source.str());

        result.headerPath = headerPath;
        result.sourcePath = sourcePath;
        result.generated = true;
        return true;
    }
}
