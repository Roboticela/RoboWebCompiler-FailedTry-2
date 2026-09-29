#include "RoboHTMLCompiler/CodeGenerator.h"

#include "RoboHTMLCompiler/HtmlLexer.h"
#include "RoboHTMLCompiler/Parsers.h"
#include "RoboHTMLCompiler/Validation.h"

#include <algorithm>
#include <fstream>
#include <sstream>

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

        std::string EscapeText(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size());

            for (const char character : value)
            {
                switch (character)
                {
                case '&': escaped += "&amp;"; break;
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                default: escaped.push_back(character); break;
                }
            }

            return escaped;
        }

        std::string EscapeAttribute(const std::string& value)
        {
            std::string escaped = EscapeText(value);

            std::string result;
            result.reserve(escaped.size());
            for (const char character : escaped)
            {
                if (character == '"')
                {
                    result += "&quot;";
                }
                else
                {
                    result.push_back(character);
                }
            }
            return result;
        }

        void AppendElement(std::ostringstream& output, const ElementNode& element, int depth);

        void AppendChildren(std::ostringstream& output, const ElementNode& element, int depth)
        {
            for (const std::unique_ptr<Node>& child : element.children)
            {
                switch (child->kind)
                {
                case NodeKind::Element:
                    AppendElement(output, *static_cast<const ElementNode*>(child.get()), depth);
                    break;

                case NodeKind::Text:
                {
                    const TextNode& text = *static_cast<const TextNode*>(child.get());
                    if (TrimAscii(text.text).empty())
                    {
                        break;
                    }
                    output << std::string(static_cast<std::size_t>(depth) * 2, ' ') << EscapeText(TrimAscii(text.text))
                           << "\n";
                    break;
                }

                case NodeKind::Comment:
                {
                    const CommentNode& comment = *static_cast<const CommentNode*>(child.get());
                    output << std::string(static_cast<std::size_t>(depth) * 2, ' ') << "<!--" << comment.text << "-->\n";
                    break;
                }

                default:
                    break;
                }
            }
        }

        void AppendElement(std::ostringstream& output, const ElementNode& element, int depth)
        {
            const std::string indentation(static_cast<std::size_t>(depth) * 2, ' ');

            output << indentation << "<" << element.sourceTag;
            for (const AttributeNode& attribute : element.attributes)
            {
                output << " " << attribute.name;
                if (attribute.hasValue)
                {
                    output << "=\"" << EscapeAttribute(attribute.value) << "\"";
                }
            }

            const ElementDefinition* definition = Registry::Instance().FindElement(element.ns, element.sourceTag);
            const bool isVoid = definition != nullptr && definition->isVoid;

            if (isVoid)
            {
                output << ">\n";
                return;
            }

            const bool hasElementChildren = std::any_of(
                element.children.begin(), element.children.end(),
                [](const std::unique_ptr<Node>& child) { return child->kind == NodeKind::Element; });

            const bool hasTextChildren = std::any_of(
                element.children.begin(), element.children.end(),
                [](const std::unique_ptr<Node>& child) {
                    return child->kind == NodeKind::Text &&
                           !TrimAscii(static_cast<const TextNode*>(child.get())->text).empty();
                });

            if (!hasElementChildren && hasTextChildren)
            {
                output << ">";
                for (const std::unique_ptr<Node>& child : element.children)
                {
                    if (child->kind == NodeKind::Text)
                    {
                        output << EscapeText(TrimAscii(static_cast<const TextNode*>(child.get())->text));
                    }
                }
                output << "</" << element.sourceTag << ">\n";
                return;
            }

            if (!hasElementChildren && !hasTextChildren && element.children.empty())
            {
                output << "></" << element.sourceTag << ">\n";
                return;
            }

            output << ">\n";
            AppendChildren(output, element, depth + 1);
            output << indentation << "</" << element.sourceTag << ">\n";
        }
    }

    WebTargetGenerator::WebTargetGenerator(DiagnosticSink& diagnosticSink, const Registry& elementRegistry)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
    {
    }

    std::string WebTargetGenerator::RenderDocument(const MarkupDocument& document,
                                                   const std::string& styleSheetFileName,
                                                   const std::string& scriptFileName)
    {
        std::ostringstream output;
        output << "<!DOCTYPE html>\n";

        if (document.root == nullptr)
        {
            return output.str();
        }

        // Emit the doctype, then everything except the injected resource
        // references, which are inserted into <head> and before </body>.
        const ElementNode* documentElement = document.root->documentElement;

        if (documentElement == nullptr)
        {
            for (const std::unique_ptr<Node>& child : document.root->children)
            {
                if (child->kind == NodeKind::Element)
                {
                    documentElement = static_cast<const ElementNode*>(child.get());
                    break;
                }
            }
        }

        if (documentElement == nullptr)
        {
            return output.str();
        }

        output << "<" << documentElement->sourceTag;
        for (const AttributeNode& attribute : documentElement->attributes)
        {
            output << " " << attribute.name;
            if (attribute.hasValue)
            {
                output << "=\"" << EscapeAttribute(attribute.value) << "\"";
            }
        }
        output << ">\n";

        for (const std::unique_ptr<Node>& child : documentElement->children)
        {
            if (child->kind != NodeKind::Element)
            {
                continue;
            }

            const ElementNode& element = *static_cast<const ElementNode*>(child.get());

            if (element.sourceTag == "head")
            {
                output << "  <head";
                for (const AttributeNode& attribute : element.attributes)
                {
                    output << " " << attribute.name;
                    if (attribute.hasValue)
                    {
                        output << "=\"" << EscapeAttribute(attribute.value) << "\"";
                    }
                }
                output << ">\n";
                AppendChildren(output, element, 2);
                output << "    <link rel=\"stylesheet\" href=\"" << EscapeAttribute(styleSheetFileName) << "\">\n";
                output << "  </head>\n";
                continue;
            }

            if (element.sourceTag == "body")
            {
                output << "  <body";
                for (const AttributeNode& attribute : element.attributes)
                {
                    output << " " << attribute.name;
                    if (attribute.hasValue)
                    {
                        output << "=\"" << EscapeAttribute(attribute.value) << "\"";
                    }
                }
                output << ">\n";
                AppendChildren(output, element, 2);
                output << "    <script src=\"" << EscapeAttribute(scriptFileName) << "\" defer></script>\n";
                output << "  </body>\n";
                continue;
            }

            AppendElement(output, element, 1);
        }

        output << "</" << documentElement->sourceTag << ">\n";
        return output.str();
    }

    bool WebTargetGenerator::Generate(const MarkupDocument& document,
                                      const StyleSheetCompiler& styleSheetCompiler,
                                      const JavaScriptModuleCompiler& behaviourModule,
                                      const std::filesystem::path& outputDirectory,
                                      const std::string& styleSheetFileName,
                                      const std::string& scriptFileName,
                                      WebTargetResult& result)
    {
        std::filesystem::create_directories(outputDirectory);

        const std::string html = RenderDocument(document, styleSheetFileName, scriptFileName);
        const std::filesystem::path documentPath = outputDirectory / "index.html";
        const std::filesystem::path styleSheetPath = outputDirectory / styleSheetFileName;
        const std::filesystem::path scriptPath = outputDirectory / scriptFileName;

        if (!ValidateGeneratedDocument(html, documentPath))
        {
            return false;
        }

        WriteFile(documentPath, html);
        WriteFile(styleSheetPath, styleSheetCompiler.EmitStyleSheet());

        // The browser target republishes the compiled behaviour modules
        // unchanged: JavaScript is already its own language.
        std::ostringstream scripts;
        scripts << "// Generated by RoboHTMLCompiler - browser target.\n";
        for (const EventBinding& binding : behaviourModule.GetEventBindings())
        {
            scripts << "// binding: #" << binding.targetId << " " << binding.event << " -> " << binding.handler
                    << "\n";
        }
        WriteFile(scriptPath, scripts.str());

        result.documentPath = documentPath;
        result.styleSheetPath = styleSheetPath;
        result.scriptPath = scriptPath;
        result.generated = true;
        return true;
    }

    bool WebTargetGenerator::ValidateGeneratedDocument(const std::string& html, const std::filesystem::path& path)
    {
        // The compiler must never report success for malformed output.
        DiagnosticSink validationSink;

        HtmlParser parser(validationSink, registry, path.generic_string());
        std::unique_ptr<MarkupDocument> generated = parser.Parse(html);

        if (generated == nullptr || validationSink.HasErrors())
        {
            diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                              SourceLocation{ path.generic_string(), 1, 1, 0 },
                              "The generated document could not be re-parsed.",
                              std::string(),
                              "This is a compiler defect; please report it.");
            return false;
        }

        ValidationOptions options;
        options.strictness = Strictness::Strict;
        options.requireDocumentElement = true;

        HtmlValidator validator(validationSink, registry, options);
        if (!validator.Validate(*generated))
        {
            diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                              SourceLocation{ path.generic_string(), 1, 1, 0 },
                              "The generated document failed validation.",
                              std::string(),
                              "This is a compiler defect; please report it.");
            return false;
        }

        return true;
    }
}
