// ---------------------------------------------------------------------------
// RoboHTMLCompiler - CSS / Tailwind module.
//
// HTML never contains CSS. The HTML frontend only exposes references
// (class="..."); this module is the independent CSS/Tailwind compiler that
// turns those references into a typed style representation. Because classes are
// resolved at build time, the native runtime never parses CSS.
//
//     class="flex items-center px-4 text-red-500"
//                     |
//                     v
//              StyleId 17
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace RoboHTMLCompiler
{
    struct StyleDeclaration
    {
        std::string property;
        std::string value;
    };

    struct StyleRule
    {
        std::string sheetName;
        std::string selectorText;
        std::vector<std::string> selectors;
        std::vector<StyleDeclaration> declarations;
        int line = 0;
    };

    /// Parses stylesheets, expands Tailwind utilities and answers class-list
    /// resolution queries.
    class StyleSheetCompiler
    {
    public:
        explicit StyleSheetCompiler(DiagnosticSink& diagnostics);

        /// Parses a .css file and appends its rules.
        void AddStyleSheet(const std::string& sheetName, const std::string& cssText);

        /// Registers a stylesheet reference that could not be loaded (for
        /// example a remote Tailwind build). Reported when unresolved classes
        /// are encountered.
        void AddExternalStyleSheet(const std::string& reference);

        /// Resolves a class attribute value into ordered declarations.
        /// Tailwind utilities are expanded before authored rules are merged.
        std::vector<StyleDeclaration> ResolveClassList(const std::string& classValue) const;

        /// Canonical, order-independent text for a declaration list. Used as
        /// the interning key of a StyleId.
        static std::string CanonicalizeDeclarations(const std::vector<StyleDeclaration>& declarations);

        /// True when a class token is understood by the Tailwind expander or by
        /// an authored rule.
        bool IsKnownClass(const std::string& className) const;

        /// Reports every class referenced by the document that no stylesheet or
        /// Tailwind utility defines.
        bool ValidateStyleReferences(const MarkupDocument& document);

        /// Renders the compiled stylesheet, containing only the rules required
        /// by the templates plus the expansions of the utilities in use.
        std::string EmitStyleSheet() const;

        const std::vector<StyleRule>& GetRules() const { return rules; }
        const std::vector<std::string>& GetExternalStyleSheets() const { return externalStyleSheets; }

        /// Expands a Tailwind utility class. Returns false when the token is not
        /// a recognised utility.
        static bool ExpandTailwindUtility(const std::string& className,
                                          std::vector<StyleDeclaration>& out);

    private:
        void ParseRules(const std::string& sheetName, const std::string& cssText);

        DiagnosticSink& diagnostics;
        std::vector<StyleRule> rules;
        std::vector<std::string> externalStyleSheets;

        /// Class name -> declarations authored in a .css file.
        std::unordered_map<std::string, std::vector<StyleDeclaration>> authoredClasses;

        /// Every class token seen while validating, in first-use order.
        mutable std::vector<std::string> usedClasses;
    };

    /// Interns resolved class lists into StyleId values.
    class StyleReferenceResolver
    {
    public:
        StyleReferenceResolver(const StyleSheetCompiler& styleSheetCompiler, DiagnosticSink& diagnostics);

        /// Resolves and interns the styling of an element. Returns 0 when the
        /// element carries no styling.
        StyleId Resolve(const ElementNode& element, NativeUiModel& model);

        std::size_t GetStyleCount() const { return styleCount; }

    private:
        const StyleSheetCompiler& styleSheetCompiler;
        DiagnosticSink& diagnostics;
        std::unordered_map<std::string, StyleId> internedStyles;
        std::size_t styleCount = 0;
    };
}
