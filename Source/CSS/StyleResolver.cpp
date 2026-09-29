#include "RoboHTMLCompiler/StyleResolver.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace RoboHTMLCompiler
{
    namespace
    {
        std::string Trim(const std::string& value)
        {
            std::size_t begin = 0;
            std::size_t end = value.size();
            while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
            {
                ++begin;
            }
            while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            {
                --end;
            }
            return value.substr(begin, end - begin);
        }

        std::vector<std::string> SplitTokens(const std::string& value)
        {
            std::vector<std::string> tokens;
            std::istringstream stream(value);
            std::string token;
            while (stream >> token)
            {
                tokens.push_back(token);
            }
            return tokens;
        }

        std::vector<std::string> SplitSelectors(const std::string& value)
        {
            std::vector<std::string> selectors;
            std::size_t position = 0;

            while (position <= value.size())
            {
                const std::size_t terminator = value.find(',', position);
                const std::string selector =
                    Trim(value.substr(position, (terminator == std::string::npos ? value.size() : terminator) -
                                                    position));
                if (!selector.empty())
                {
                    selectors.push_back(selector);
                }

                if (terminator == std::string::npos)
                {
                    break;
                }
                position = terminator + 1;
            }

            return selectors;
        }

        int LineOf(const std::string& text, std::size_t offset)
        {
            int line = 1;
            for (std::size_t index = 0; index < offset && index < text.size(); ++index)
            {
                if (text[index] == '\n')
                {
                    ++line;
                }
            }
            return line;
        }

        void MergeDeclaration(std::vector<StyleDeclaration>& declarations, const StyleDeclaration& declaration)
        {
            for (StyleDeclaration& existing : declarations)
            {
                if (existing.property == declaration.property)
                {
                    existing.value = declaration.value;
                    return;
                }
            }
            declarations.push_back(declaration);
        }
    }

    StyleSheetCompiler::StyleSheetCompiler(DiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
    }

    void StyleSheetCompiler::AddExternalStyleSheet(const std::string& reference)
    {
        externalStyleSheets.push_back(reference);
    }

    void StyleSheetCompiler::AddStyleSheet(const std::string& sheetName, const std::string& cssText)
    {
        ParseRules(sheetName, cssText);

        // Tailwind directives are a build instruction, not runtime CSS. They are
        // recorded so the generated stylesheet can explain where the utilities
        // came from.
        std::size_t directivePosition = 0;
        while ((directivePosition = cssText.find("@tailwind", directivePosition)) != std::string::npos)
        {
            const std::size_t terminator = cssText.find(';', directivePosition);
            const std::string directive =
                cssText.substr(directivePosition,
                               (terminator == std::string::npos ? cssText.size() : terminator) - directivePosition);

            diagnostics.Warning(DiagnosticCode::MalformedStyleSheet,
                                SourceLocation{ sheetName, LineOf(cssText, directivePosition), 1, directivePosition },
                                "Tailwind directive '" + Trim(directive) + "' resolved at build time.",
                                std::string(),
                                "Tailwind utilities used by the markup are expanded into typed declarations.");

            if (terminator == std::string::npos)
            {
                break;
            }
            directivePosition = terminator + 1;
        }
    }

    void StyleSheetCompiler::ParseRules(const std::string& sheetName, const std::string& cssText)
    {
        std::size_t position = 0;

        while (position < cssText.size())
        {
            const std::size_t commentBegin = cssText.find("/*", position);
            const std::size_t ruleBegin = cssText.find('{', position);

            if (commentBegin != std::string::npos && (ruleBegin == std::string::npos || commentBegin < ruleBegin))
            {
                const std::size_t commentEnd = cssText.find("*/", commentBegin + 2);
                position = (commentEnd == std::string::npos) ? cssText.size() : commentEnd + 2;
                continue;
            }

            if (ruleBegin == std::string::npos)
            {
                break;
            }

            const std::string selectorText = Trim(cssText.substr(position, ruleBegin - position));
            const std::size_t ruleEnd = cssText.find('}', ruleBegin);
            if (ruleEnd == std::string::npos)
            {
                diagnostics.Error(DiagnosticCode::MalformedStyleSheet,
                                  SourceLocation{ sheetName, LineOf(cssText, ruleBegin), 1, ruleBegin },
                                  "Unterminated rule block in stylesheet.",
                                  std::string(),
                                  "Add the missing '}'.");
                break;
            }

            const std::string body = cssText.substr(ruleBegin + 1, ruleEnd - ruleBegin - 1);

            StyleRule rule;
            rule.sheetName = sheetName;
            rule.selectorText = selectorText;
            rule.line = LineOf(cssText, position);
            rule.selectors = SplitSelectors(selectorText);

            std::size_t declarationPosition = 0;
            while (declarationPosition < body.size())
            {
                const std::size_t terminator = body.find(';', declarationPosition);
                const std::string declarationText =
                    body.substr(declarationPosition, (terminator == std::string::npos ? body.size() : terminator) -
                                                         declarationPosition);
                const std::size_t colon = declarationText.find(':');
                if (colon != std::string::npos)
                {
                    StyleDeclaration declaration;
                    declaration.property = Trim(declarationText.substr(0, colon));
                    declaration.value = Trim(declarationText.substr(colon + 1));
                    if (!declaration.property.empty() && !declaration.value.empty() &&
                        declaration.property[0] != '@')
                    {
                        rule.declarations.push_back(std::move(declaration));
                    }
                }

                if (terminator == std::string::npos)
                {
                    break;
                }
                declarationPosition = terminator + 1;
            }

            if (!rule.selectorText.empty() && rule.selectorText[0] != '@')
            {
                for (const std::string& selector : rule.selectors)
                {
                    if (!selector.empty() && selector[0] == '.')
                    {
                        const std::string className = selector.substr(1);
                        for (const StyleDeclaration& declaration : rule.declarations)
                        {
                            authoredClasses[className].push_back(declaration);
                        }
                    }
                }
            }

            rules.push_back(std::move(rule));
            position = ruleEnd + 1;
        }
    }

    bool StyleSheetCompiler::IsKnownClass(const std::string& className) const
    {
        if (authoredClasses.find(className) != authoredClasses.end())
        {
            return true;
        }

        std::vector<StyleDeclaration> expanded;
        return ExpandTailwindUtility(className, expanded);
    }

    std::vector<StyleDeclaration> StyleSheetCompiler::ResolveClassList(const std::string& classValue) const
    {
        std::vector<StyleDeclaration> resolved;

        for (const std::string& className : SplitTokens(classValue))
        {
            std::vector<StyleDeclaration> expanded;
            if (ExpandTailwindUtility(className, expanded))
            {
                for (const StyleDeclaration& declaration : expanded)
                {
                    MergeDeclaration(resolved, declaration);
                }
            }

            const auto authored = authoredClasses.find(className);
            if (authored != authoredClasses.end())
            {
                for (const StyleDeclaration& declaration : authored->second)
                {
                    MergeDeclaration(resolved, declaration);
                }
            }
        }

        return resolved;
    }

    std::string StyleSheetCompiler::CanonicalizeDeclarations(const std::vector<StyleDeclaration>& declarations)
    {
        std::vector<std::string> entries;
        entries.reserve(declarations.size());
        for (const StyleDeclaration& declaration : declarations)
        {
            entries.push_back(declaration.property + ":" + declaration.value);
        }
        std::sort(entries.begin(), entries.end());

        std::ostringstream output;
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            if (index > 0)
            {
                output << ";";
            }
            output << entries[index];
        }
        return output.str();
    }

    bool StyleSheetCompiler::ValidateStyleReferences(const MarkupDocument& document)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        document.ForEachElement([this](const ElementNode& element) {
            const AttributeNode* classAttribute = element.FindAttribute("class");
            if (classAttribute == nullptr)
            {
                return;
            }

            for (const std::string& className : SplitTokens(classAttribute->value))
            {
                if (std::find(usedClasses.begin(), usedClasses.end(), className) == usedClasses.end())
                {
                    usedClasses.push_back(className);
                }

                if (!IsKnownClass(className))
                {
                    diagnostics.Error(DiagnosticCode::UnresolvedStyleReference,
                                      classAttribute->location,
                                      "Unresolved class '" + className + "'.",
                                      std::string(),
                                      "Define the class in a stylesheet or correct the name.",
                                      className);
                }
            }
        });

        return diagnostics.ErrorCount() == errorsBefore;
    }

    std::string StyleSheetCompiler::EmitStyleSheet() const
    {
        std::ostringstream output;
        output << "/* Generated by RoboHTMLCompiler - build-time resolved styles. */\n";

        for (const std::string& reference : externalStyleSheets)
        {
            output << "/* external stylesheet reference: " << reference << " */\n";
        }

        std::set<std::string> emittedUtilities;

        for (const std::string& className : usedClasses)
        {
            if (!emittedUtilities.insert(className).second)
            {
                continue;
            }

            std::vector<StyleDeclaration> declarations;
            if (!ExpandTailwindUtility(className, declarations))
            {
                continue;
            }

            output << "." << className << " {";
            for (const StyleDeclaration& declaration : declarations)
            {
                output << " " << declaration.property << ": " << declaration.value << ";";
            }
            output << " }\n";
        }

        for (const StyleRule& rule : rules)
        {
            if (rule.selectorText.empty() || rule.selectorText[0] == '@')
            {
                continue;
            }

            output << rule.selectorText << " {";
            for (const StyleDeclaration& declaration : rule.declarations)
            {
                output << " " << declaration.property << ": " << declaration.value << ";";
            }
            output << " }\n";
        }

        return output.str();
    }

    // -----------------------------------------------------------------------
    // StyleReferenceResolver
    // -----------------------------------------------------------------------

    StyleReferenceResolver::StyleReferenceResolver(const StyleSheetCompiler& compiler, DiagnosticSink& diagnosticSink)
        : styleSheetCompiler(compiler)
        , diagnostics(diagnosticSink)
    {
    }

    StyleId StyleReferenceResolver::Resolve(const ElementNode& element, NativeUiModel& model)
    {
        const AttributeNode* classAttribute = element.FindAttribute("class");
        if (classAttribute == nullptr || classAttribute->value.empty())
        {
            return 0;
        }

        const std::vector<StyleDeclaration> declarations =
            styleSheetCompiler.ResolveClassList(classAttribute->value);
        if (declarations.empty())
        {
            return 0;
        }

        const std::string canonical = StyleSheetCompiler::CanonicalizeDeclarations(declarations);

        const auto existing = internedStyles.find(canonical);
        if (existing != internedStyles.end())
        {
            return existing->second;
        }

        const StyleId styleId = static_cast<StyleId>(model.styleTable.size() + 1);
        model.styleTable.push_back(canonical);
        model.styleDeclarations.push_back(canonical);
        internedStyles[canonical] = styleId;
        ++styleCount;

        return styleId;
    }
}
