// ---------------------------------------------------------------------------
// RoboHTMLCompiler - namespace frontends.
//
// HTML, SVG and MathML are independent languages with independent frontends:
//
//     HtmlParser   -> HTML AST   (structure only)
//     SvgParser    -> SVG AST    (vector graphics structure)
//     MathMlParser -> MathML AST (mathematical structure)
//
// The HTML frontend delegates <svg> and <math> subtrees to their dedicated
// parsers instead of treating them as generic HTML.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/HtmlLexer.h"
#include "RoboHTMLCompiler/Registry.h"

#include <memory>
#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    class MathMlParser;
    class SvgParser;

    class HtmlParser
    {
    public:
        HtmlParser(DiagnosticSink& diagnostics, const Registry& registry, std::string sourceFile);

        /// Parses a whole document. Returns nullptr when no document could be
        /// produced at all.
        std::unique_ptr<MarkupDocument> Parse(const std::string& source);

        /// Parses a fragment (no document root required).
        std::unique_ptr<MarkupDocument> ParseFragment(const std::string& source);

    private:
        struct Frame
        {
            ElementNode* element = nullptr;
            std::string tag;
        };

        std::unique_ptr<MarkupDocument> ParseInternal(const std::string& source, bool fragment);

        /// Appends a node to the innermost open element, or to the document.
        void AppendNode(std::vector<std::unique_ptr<Node>>& sink,
                        std::vector<Frame>& stack,
                        std::unique_ptr<Node> node) const;

        void CloseElement(std::vector<Frame>& stack, const Token& token, bool fragment);

        DiagnosticSink& diagnostics;
        const Registry& registry;
        std::string sourceFile;
    };

    /// Parser for the SVG namespace. Runs its own vocabulary handling and
    /// produces SVG-native categories.
    class SvgParser
    {
    public:
        SvgParser(DiagnosticSink& diagnostics, const Registry& registry, std::string sourceFile);

        /// Parses an <svg> subtree. The lexer position is left after the
        /// matching end tag.
        std::unique_ptr<ElementNode> ParseElement(HtmlLexer& lexer, const Token& startTag, ElementNode* parent);

    private:
        DiagnosticSink& diagnostics;
        const Registry& registry;
        std::string sourceFile;
    };

    /// Parser for the MathML namespace.
    class MathMlParser
    {
    public:
        MathMlParser(DiagnosticSink& diagnostics, const Registry& registry, std::string sourceFile);

        /// Parses a <math> subtree.
        std::unique_ptr<ElementNode> ParseElement(HtmlLexer& lexer, const Token& startTag, ElementNode* parent);

    private:
        DiagnosticSink& diagnostics;
        const Registry& registry;
        std::string sourceFile;
    };
}
