// ---------------------------------------------------------------------------
// RoboCSSCompiler - the CSS parser and its AST.
//
//     stylesheet text  ->  CSSTokenizer  ->  CSSParser  ->  CSSRule tree
//
// The tree keeps the shape of the source: rules, at-rules (with nested rules),
// declarations and component values. Component values are the syntax
// specification's own intermediate form - a preserved token, a function call, or
// a simple block - which is what makes `var()` and `calc()` possible at all:
// their arguments must survive parsing as *tokens*, because they are not parsed
// against a property's grammar until after substitution.
//
// This is a source-language representation. It is compiled into typed values by
// the value parser and into typed computed styles by the cascade; none of it
// reaches the runtime.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssDiagnostics.h"
#include "RoboCSSCompiler/CssSyntax.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    /// A block a component value can open.
    enum class CSSBlockKind : std::uint8_t
    {
        None = 0,
        Parentheses,
        Square,
        Curly
    };

    /// One component value: a token, or a function/block with children.
    struct CSSComponentValue
    {
        CSSToken token;

        /// Parentheses, square or curly. `None` for a plain token or a function.
        CSSBlockKind block = CSSBlockKind::None;
        /// Function calls use the block kind of their parentheses.
        bool isFunction = false;

        /// The contents of a function or a block.
        std::vector<CSSComponentValue> children;

        CSSSourceLocation location;

        /// The name of a function, or an ident's name; empty otherwise.
        const std::string& Name() const { return token.text; }
    };

    struct CSSDeclaration
    {
        /// Lower-cased property name as written, including a custom property's
        /// leading `--`. A custom property keeps its case, which is why `cased`
        /// exists.
        std::string property;
        bool isCustomProperty = false;
        bool important = false;

        std::vector<CSSComponentValue> value;

        CSSSourceLocation location;
    };

    struct CSSRule;

    /// An at-rule: `@name prelude { block }`, `@name prelude;`, or both shapes
    /// depending on which at-rule it is.
    struct CSSAtRule
    {
        /// Lower-cased name without the `@`.
        std::string name;
        std::vector<CSSComponentValue> prelude;
        std::string preludeText;

        /// True when the at-rule had a `{ ... }` block at all.
        bool hasBlock = false;
        /// True when the block held declarations rather than rules
        /// (`@font-face`, `@page`, `@property`, `@counter-style`).
        bool blockIsDeclarationList = false;

        std::vector<CSSDeclaration> declarations;
        std::vector<CSSRule> children;

        CSSSourceLocation location;
    };

    /// A rule: either a qualified rule (`selectors { declarations }`) or an
    /// at-rule. One type, because the cascade cares about the order they appear
    /// in and a parallel vector would lose it.
    struct CSSRule
    {
        enum class Kind : std::uint8_t
        {
            Qualified = 0,
            At
        };

        Kind kind = Kind::Qualified;

        /// The selector list exactly as written, then split on top-level commas.
        std::string selectorText;
        std::vector<std::string> selectors;

        std::vector<CSSDeclaration> declarations;
        CSSAtRule at;

        CSSSourceLocation location;
    };

    /// A parsed stylesheet, in source order.
    struct CSSStylesheet
    {
        std::string fileName;
        std::vector<CSSRule> rules;

        bool IsEmpty() const { return rules.empty(); }
        std::size_t RuleCount() const { return rules.size(); }
    };

    /// Parses a stylesheet. Never throws and never repairs: what cannot be
    /// parsed is reported and dropped, so the caller can decide whether a
    /// warning is acceptable for the build.
    class CSSParser
    {
    public:
        CSSParser(CSSDiagnosticSink& diagnostics);

        /// Parses `text` as a stylesheet named `fileName`.
        CSSStylesheet ParseStylesheet(const std::string& fileName, const std::string& text);

        /// Parses a declaration list on its own, which is what a test uses to
        /// check a grammar without a whole stylesheet.
        std::vector<CSSDeclaration> ParseDeclarationList(const std::string& fileName, const std::string& text);

    private:
        std::vector<CSSRule> ParseRuleList(std::size_t begin, std::size_t end, bool declarationBlockExpected);
        std::vector<CSSDeclaration> ParseDeclarations(std::size_t begin, std::size_t end);
        /// Consumes one component value at `index`, advancing past it.
        CSSComponentValue ConsumeComponentValue(std::size_t& index, std::size_t end);
        /// Finds the index of the token that closes a block opened before
        /// `begin`, or `end` when there is none.
        std::size_t FindBlockEnd(std::size_t begin, std::size_t end, CSSTokenType opener, CSSTokenType closer) const;
        /// Text of a token range, for diagnostics.
        std::string TextOf(std::size_t begin, std::size_t end) const;
        const CSSToken& TokenAt(std::size_t index) const;

        CSSDiagnosticSink& diagnostics;
        std::vector<CSSToken> tokens;
        std::string currentFile;
    };
}
