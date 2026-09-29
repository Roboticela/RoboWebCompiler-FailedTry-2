#include "RoboHTMLCompiler/Parsers.h"

#include <algorithm>

namespace RoboHTMLCompiler
{
    namespace
    {
        /// Marks an attribute as application metadata when it uses the data-*
        /// extension mechanism.
        void ClassifyAttributes(ElementNode& element)
        {
            for (AttributeNode& attribute : element.attributes)
            {
                attribute.isDataAttribute = Registry::IsDataAttribute(attribute.name);
            }
        }

        /// The markup lexer lower-cases attribute names. SVG and MathML declare
        /// case-sensitive spellings ('viewBox', 'definitionURL'), so an
        /// attribute that resolves in the namespace is rewritten to the
        /// canonical spelling the registry declares. Unknown attributes keep the
        /// spelling the lexer produced.
        void CanonicalizeAttributeNames(const Registry& registry, MarkupNamespace ns, ElementNode& element)
        {
            for (AttributeNode& attribute : element.attributes)
            {
                const AttributeDefinition* definition =
                    registry.FindAttribute(ns, attribute.name, element.sourceTag);
                if (definition != nullptr)
                {
                    attribute.name = definition->name;
                }
            }
        }

        /// True when a MathML <annotation-xml> encoding names an HTML flavour,
        /// which turns the element into an HTML integration point.
        bool IsHtmlAnnotationEncoding(std::string_view encoding)
        {
            return EqualsIgnoreCase(encoding, "text/html") ||
                   EqualsIgnoreCase(encoding, "application/xhtml+xml");
        }

        /// SVG and MathML tag names are case-sensitive in the source language,
        /// but the markup lexer lower-cases every tag name. The namespace
        /// frontends restore the spelling the registry declares, so a source
        /// <lineargradient> is normalised to the canonical <linearGradient>.
        std::string CanonicalTagName(const Registry& registry, MarkupNamespace ns, std::string_view name)
        {
            const ElementDefinition* definition = registry.FindElement(ns, name);
            return definition != nullptr ? definition->name : std::string(name);
        }
    }

    // -----------------------------------------------------------------------
    // HtmlParser
    // -----------------------------------------------------------------------

    HtmlParser::HtmlParser(DiagnosticSink& diagnosticSink, const Registry& elementRegistry, std::string file)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
        , sourceFile(std::move(file))
    {
    }

    void HtmlParser::AppendNode(std::vector<std::unique_ptr<Node>>& sink,
                                std::vector<Frame>& stack,
                                std::unique_ptr<Node> node) const
    {
        if (!stack.empty() && stack.back().element != nullptr)
        {
            ElementNode* parentElement = stack.back().element;
            if (node->kind == NodeKind::Element)
            {
                static_cast<ElementNode*>(node.get())->parent = parentElement;
            }
            parentElement->children.push_back(std::move(node));
            return;
        }

        sink.push_back(std::move(node));
    }

    void HtmlParser::CloseElement(std::vector<Frame>& stack, const Token& token, bool fragment)
    {
        for (std::size_t index = stack.size(); index-- > 0;)
        {
            if (stack[index].tag == token.name)
            {
                // An end tag may only close the element it names. Any element
                // still open above it is not closed implicitly: that would be
                // silent recovery, and the compiler never repairs source.
                for (std::size_t open = index + 1; open < stack.size(); ++open)
                {
                    const Frame& frame = stack[open];
                    diagnostics.Error(DiagnosticCode::MalformedMarkup,
                                      frame.element != nullptr ? frame.element->location : token.location,
                                      "Element <" + frame.tag + "> is never closed.",
                                      std::string(),
                                      "Add the matching end tag before </" + token.name + ">.",
                                      "<" + frame.tag + ">");
                }

                stack.resize(index);
                return;
            }
        }

        if (fragment && token.name == "html")
        {
            return;
        }

        diagnostics.Error(DiagnosticCode::UnexpectedEndTag,
                          token.location,
                          "End tag </" + token.name + "> does not match any open element.",
                          std::string(),
                          "Remove the end tag, or add the matching start tag.",
                          "</" + token.name + ">");
    }

    std::unique_ptr<MarkupDocument> HtmlParser::Parse(const std::string& source)
    {
        return ParseInternal(source, false);
    }

    std::unique_ptr<MarkupDocument> HtmlParser::ParseFragment(const std::string& source)
    {
        return ParseInternal(source, true);
    }

    std::unique_ptr<MarkupDocument> HtmlParser::ParseInternal(const std::string& source, bool fragment)
    {
        auto document = std::make_unique<MarkupDocument>();
        document->sourceFile = sourceFile;
        document->root = std::make_unique<DocumentNode>(SourceLocation{ sourceFile, 1, 1, 0 });

        HtmlLexer lexer(source, sourceFile, diagnostics);
        SvgParser svgParser(diagnostics, registry, sourceFile);
        MathMlParser mathMlParser(diagnostics, registry, sourceFile);

        std::vector<Frame> stack;

        while (true)
        {
            Token token = lexer.Next();

            switch (token.kind)
            {
            case TokenKind::EndOfFile:
            {
                for (const Frame& frame : stack)
                {
                    diagnostics.Error(DiagnosticCode::MalformedMarkup,
                                      frame.element != nullptr ? frame.element->location : token.location,
                                      "Element <" + frame.tag + "> is never closed.",
                                      std::string(),
                                      "Add the matching end tag.",
                                      "<" + frame.tag + ">");
                }
                stack.clear();
                return document;
            }

            case TokenKind::Doctype:
            {
                AppendNode(document->root->children, stack,
                           std::make_unique<DoctypeNode>(token.location, token.name));
                break;
            }

            case TokenKind::Comment:
            {
                AppendNode(document->root->children, stack,
                           std::make_unique<CommentNode>(token.location, token.text));
                break;
            }

            case TokenKind::Text:
            {
                auto textNode = std::make_unique<TextNode>(token.location, token.text);
                AppendNode(document->root->children, stack, std::move(textNode));
                break;
            }

            case TokenKind::EndTag:
            {
                CloseElement(stack, token, fragment);
                break;
            }

            case TokenKind::StartTag:
            {
                const bool isSvgRoot = token.name == "svg" &&
                                       (stack.empty() || stack.back().element->ns == MarkupNamespace::Html);
                const bool isMathRoot = token.name == "math" &&
                                        (stack.empty() || stack.back().element->ns == MarkupNamespace::Html);

                // An <svg> or <math> in HTML content switches the parser frontend:
                // the subtree is parsed by the dedicated namespace parser, which
                // appends the root element to the supplied parent.
                if (isSvgRoot)
                {
                    ElementNode* parentElement = stack.empty() ? nullptr : stack.back().element;
                    std::unique_ptr<ElementNode> element = svgParser.ParseElement(lexer, token, parentElement);
                    if (element != nullptr)
                    {
                        AppendNode(document->root->children, stack, std::move(element));
                    }
                    break;
                }

                if (isMathRoot)
                {
                    ElementNode* parentElement = stack.empty() ? nullptr : stack.back().element;
                    std::unique_ptr<ElementNode> element = mathMlParser.ParseElement(lexer, token, parentElement);
                    if (element != nullptr)
                    {
                        AppendNode(document->root->children, stack, std::move(element));
                    }
                    break;
                }

                auto element = std::make_unique<ElementNode>(token.location, token.name, MarkupNamespace::Html);
                element->attributes = token.attributes;
                ClassifyAttributes(*element);

                const ElementDefinition* definition = registry.FindHtmlElement(token.name);
                const bool isRawText = (definition != nullptr) && definition->isRawText;
                const bool isVoid = (definition != nullptr) && definition->isVoid;
                const bool selfClosing = token.selfClosing;

                ElementNode* rawElement = element.get();
                if (!stack.empty())
                {
                    rawElement->parent = stack.back().element;
                }

                if (stack.empty())
                {
                    document->root->children.push_back(std::move(element));
                }
                else
                {
                    stack.back().element->children.push_back(std::move(element));
                }

                if (rawElement->sourceTag == "html" && document->root->documentElement == nullptr)
                {
                    document->root->documentElement = rawElement;
                }

                if (isVoid || selfClosing)
                {
                    break;
                }

                if (isRawText)
                {
                    lexer.EnterRawTextMode(rawElement->sourceTag);
                    Token content = lexer.Next();
                    if (content.kind == TokenKind::Text)
                    {
                        auto textNode = std::make_unique<TextNode>(content.location, content.text);
                        textNode->raw = true;
                        rawElement->children.push_back(std::move(textNode));
                    }

                    Token endTag = lexer.Next();
                    if (endTag.kind == TokenKind::EndTag && endTag.name != rawElement->sourceTag)
                    {
                        CloseElement(stack, endTag, fragment);
                    }
                    break;
                }

                stack.push_back(Frame{ rawElement, rawElement->sourceTag });
                break;
            }
            }
        }
    }

    // -----------------------------------------------------------------------
    // SvgParser
    // -----------------------------------------------------------------------

    SvgParser::SvgParser(DiagnosticSink& diagnosticSink, const Registry& elementRegistry, std::string file)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
        , sourceFile(std::move(file))
    {
    }

    std::unique_ptr<ElementNode> SvgParser::ParseElement(HtmlLexer& lexer, const Token& startTag, ElementNode* parent)
    {
        auto root = std::make_unique<ElementNode>(
            startTag.location, CanonicalTagName(registry, MarkupNamespace::Svg, startTag.name), MarkupNamespace::Svg);
        root->attributes = startTag.attributes;
        ClassifyAttributes(*root);
        CanonicalizeAttributeNames(registry, MarkupNamespace::Svg, *root);
        root->parent = parent;

        ElementNode* current = root.get();
        std::vector<ElementNode*> stack;
        stack.push_back(current);

        const bool selfClosed = startTag.selfClosing;

        if (!selfClosed)
        {
            while (true)
            {
                Token token = lexer.Next();

                if (token.kind == TokenKind::EndOfFile)
                {
                    diagnostics.Error(DiagnosticCode::MalformedMarkup,
                                      current->location,
                                      "SVG element <" + current->sourceTag + "> is never closed.",
                                      std::string(),
                                      "Add the matching end tag.",
                                      "</" + current->sourceTag + ">");
                    break;
                }

                if (token.kind == TokenKind::EndTag)
                {
                    if (EqualsIgnoreCase(token.name, stack.back()->sourceTag))
                    {
                        stack.pop_back();
                        if (stack.empty())
                        {
                            break;
                        }
                        current = stack.back();
                        continue;
                    }

                    diagnostics.Error(DiagnosticCode::UnexpectedEndTag,
                                      token.location,
                                      "End tag </" + token.name + "> does not match the open SVG element <" +
                                          stack.back()->sourceTag + ">.",
                                      std::string(),
                                      "Remove the end tag, or add the matching start tag.",
                                      "</" + token.name + ">");
                    continue;
                }

                if (token.kind == TokenKind::Text)
                {
                    current->children.push_back(std::make_unique<TextNode>(token.location, token.text));
                    continue;
                }

                if (token.kind == TokenKind::Comment)
                {
                    current->children.push_back(std::make_unique<CommentNode>(token.location, token.text));
                    continue;
                }

                if (token.kind == TokenKind::Doctype)
                {
                    diagnostics.Error(DiagnosticCode::MalformedMarkup,
                                      token.location,
                                      "A doctype is not allowed inside SVG content.",
                                      std::string(),
                                      "Remove the doctype.");
                    continue;
                }

                // Start tag.
                if (current->sourceTag == "foreignObject")
                {
                    // HTML integration point: hand the subtree back to the HTML
                    // parser by treating it as HTML vocabulary below.
                    auto element = std::make_unique<ElementNode>(token.location, token.name, MarkupNamespace::Html);
                    element->attributes = token.attributes;
                    ClassifyAttributes(*element);
                    element->parent = current;
                    ElementNode* htmlElement = element.get();
                    current->children.push_back(std::move(element));

                    const ElementDefinition* definition = registry.FindHtmlElement(token.name);
                    if (definition != nullptr && definition->isVoid)
                    {
                        continue;
                    }

                    stack.push_back(htmlElement);
                    current = htmlElement;
                    continue;
                }

                auto element = std::make_unique<ElementNode>(
                    token.location, CanonicalTagName(registry, MarkupNamespace::Svg, token.name), MarkupNamespace::Svg);
                element->attributes = token.attributes;
                ClassifyAttributes(*element);
                CanonicalizeAttributeNames(registry, MarkupNamespace::Svg, *element);
                element->parent = current;

                ElementNode* rawElement = element.get();
                current->children.push_back(std::move(element));

                if (token.selfClosing)
                {
                    continue;
                }

                stack.push_back(rawElement);
                current = rawElement;
            }
        }

        // The caller attaches the returned subtree to its parent: the namespace
        // frontends own parsing, the HTML frontend owns document assembly.
        return root;
    }

    // -----------------------------------------------------------------------
    // MathMlParser
    // -----------------------------------------------------------------------

    MathMlParser::MathMlParser(DiagnosticSink& diagnosticSink, const Registry& elementRegistry, std::string file)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
        , sourceFile(std::move(file))
    {
    }

    std::unique_ptr<ElementNode> MathMlParser::ParseElement(HtmlLexer& lexer, const Token& startTag, ElementNode* parent)
    {
        auto root = std::make_unique<ElementNode>(
            startTag.location, CanonicalTagName(registry, MarkupNamespace::MathMl, startTag.name), MarkupNamespace::MathMl);
        root->attributes = startTag.attributes;
        ClassifyAttributes(*root);
        CanonicalizeAttributeNames(registry, MarkupNamespace::MathMl, *root);
        root->parent = parent;

        ElementNode* current = root.get();
        std::vector<ElementNode*> stack;
        stack.push_back(current);

        if (!startTag.selfClosing)
        {
            while (true)
            {
                Token token = lexer.Next();

                if (token.kind == TokenKind::EndOfFile)
                {
                    diagnostics.Error(DiagnosticCode::MalformedMarkup,
                                      current->location,
                                      "MathML element <" + current->sourceTag + "> is never closed.",
                                      std::string(),
                                      "Add the matching end tag.",
                                      "</" + current->sourceTag + ">");
                    break;
                }

                if (token.kind == TokenKind::EndTag)
                {
                    if (EqualsIgnoreCase(token.name, stack.back()->sourceTag))
                    {
                        stack.pop_back();
                        if (stack.empty())
                        {
                            break;
                        }
                        current = stack.back();
                        continue;
                    }

                    diagnostics.Error(DiagnosticCode::UnexpectedEndTag,
                                      token.location,
                                      "End tag </" + token.name + "> does not match the open MathML element <" +
                                          stack.back()->sourceTag + ">.",
                                      std::string(),
                                      "Remove the end tag, or add the matching start tag.",
                                      "</" + token.name + ">");
                    continue;
                }

                if (token.kind == TokenKind::Text)
                {
                    current->children.push_back(std::make_unique<TextNode>(token.location, token.text));
                    continue;
                }

                if (token.kind == TokenKind::Comment)
                {
                    current->children.push_back(std::make_unique<CommentNode>(token.location, token.text));
                    continue;
                }

                if (token.kind == TokenKind::Doctype)
                {
                    diagnostics.Error(DiagnosticCode::MalformedMarkup,
                                      token.location,
                                      "A doctype is not allowed inside MathML content.",
                                      std::string(),
                                      "Remove the doctype.");
                    continue;
                }

                if (current->sourceTag == "annotation-xml" && IsHtmlAnnotationEncoding(current->GetAttribute("encoding")))
                {
                    // HTML integration point: an <annotation-xml> whose encoding
                    // is an HTML flavour hosts ordinary HTML content.
                    auto element = std::make_unique<ElementNode>(token.location, token.name, MarkupNamespace::Html);
                    element->attributes = token.attributes;
                    ClassifyAttributes(*element);
                    element->parent = current;
                    ElementNode* htmlElement = element.get();
                    current->children.push_back(std::move(element));

                    const ElementDefinition* definition = registry.FindHtmlElement(token.name);
                    if (definition != nullptr && definition->isVoid)
                    {
                        continue;
                    }

                    stack.push_back(htmlElement);
                    current = htmlElement;
                    continue;
                }

                auto element = std::make_unique<ElementNode>(
                    token.location, CanonicalTagName(registry, MarkupNamespace::MathMl, token.name), MarkupNamespace::MathMl);
                element->attributes = token.attributes;
                ClassifyAttributes(*element);
                CanonicalizeAttributeNames(registry, MarkupNamespace::MathMl, *element);
                element->parent = current;

                ElementNode* rawElement = element.get();
                current->children.push_back(std::move(element));

                if (token.selfClosing)
                {
                    continue;
                }

                stack.push_back(rawElement);
                current = rawElement;
            }
        }

        return root;
    }
}
