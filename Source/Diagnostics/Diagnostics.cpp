#include "RoboHTMLCompiler/Diagnostics.h"

#include <sstream>

namespace RoboHTMLCompiler
{
    namespace
    {
        struct DiagnosticInfo
        {
            DiagnosticCode code;
            const char* codeString;
            const char* alias;
            const char* title;
            const char* explanation;
            const char* suggestedFix;
        };

        const DiagnosticInfo DiagnosticCatalog[] = {
            { DiagnosticCode::InlineCssAttribute, "MHTML001", "RHC1007",
              "Inline CSS is forbidden",
              "RoboHTMLCompiler keeps structure, appearance and behaviour in separate languages. Appearance lives "
              "in CSS or Tailwind stylesheets that the style compiler owns, so a style content attribute would "
              "bypass build-time style resolution and reintroduce runtime CSS parsing.",
              "Replace style=\"...\" with a class reference and define the rule in a stylesheet." },

            { DiagnosticCode::InlineEventHandlerAttribute, "MHTML002", "",
              "Inline JavaScript event handlers are forbidden",
              "Content attribute event handlers embed JavaScript source inside markup. Behaviour is a separate "
              "language with its own frontend, so inline handlers are rejected and never compiled.",
              "Use a JavaScript module: element.addEventListener(\"click\", handler)." },

            { DiagnosticCode::AttributeNotAllowed, "MHTML003", "",
              "Attribute is not valid for this element",
              "The reference vocabulary declares which attributes may appear on which elements. Silently accepting "
              "an attribute would invent semantics the element does not have.",
              "Remove the attribute, or use an element that the attribute is defined for." },

            { DiagnosticCode::UnknownHtmlElement, "MHTML004", "",
              "Unknown HTML element",
              "The element is not part of the registered vocabulary and does not follow the custom element naming "
              "rules, so the compiler cannot assign it a native category.",
              "Check the spelling, or register the element as a custom element (lowercase, containing a hyphen)." },

            { DiagnosticCode::UnknownAttribute, "MHTML005", "",
              "Unknown attribute",
              "Unknown attributes are never allowed to disappear silently: an unrecognised attribute usually means "
              "a typo or a feature the compiler does not yet implement. data-* attributes are the documented "
              "extension mechanism.",
              "Remove the attribute, correct its spelling, or use a data-* attribute." },

            { DiagnosticCode::UnknownAriaAttribute, "MHTML006", "",
              "Unknown ARIA attribute",
              "ARIA attributes are validated against the ARIA registry and mapped into typed accessibility "
              "properties; an unrecognised aria-* attribute would have to be treated as an opaque string.",
              "Use an ARIA attribute from the registry, or remove the attribute." },

            { DiagnosticCode::InlineStyleElement, "RHC1001", "",
              "Inline <style> is forbidden",
              "A <style> element embeds CSS source inside HTML. Stylesheets must live in their own files so the "
              "style compiler can resolve them at build time.",
              "Move the rules into an external .css file referenced by the application." },

            { DiagnosticCode::InlineScriptContent, "RHC1002", "",
              "Inline JavaScript is forbidden",
              "JavaScript source inside markup cannot be compiled as a separate module and would force the native "
              "runtime to carry a browser-style script engine.",
              "Move the code into a .js file and reference it with <script src=\"...\">." },

            { DiagnosticCode::ObsoleteElement, "RHC1004", "",
              "Obsolete element",
              "The reference marks this element as obsolete: it has no modern native equivalent and the compiler "
              "will not invent semantics for it.",
              "Use the replacement element the reference recommends." },

            { DiagnosticCode::ObsoleteAttribute, "RHC1005", "",
              "Obsolete attribute",
              "The reference marks this attribute as obsolete. Compiling it would require inventing behaviour that "
              "no longer exists.",
              "Remove the attribute and express the intent with current vocabulary or a stylesheet." },

            { DiagnosticCode::InvalidCustomElementName, "RHC1006", "",
              "Invalid custom element name",
              "Custom element names must be lowercase, contain at least one hyphen, and start with an ASCII letter.",
              "Rename the element, for example <my-component>." },

            { DiagnosticCode::DuplicateAttribute, "RHC1008", "",
              "Duplicate attribute",
              "An element may not carry the same attribute twice; the value would be ambiguous.",
              "Remove the duplicate attribute." },

            { DiagnosticCode::MalformedMarkup, "RHC1009", "",
              "Malformed markup",
              "The markup could not be tokenised, so no reliable AST exists. The compiler never guesses at the "
              "author's intent.",
              "Fix the markup at the reported position." },

            { DiagnosticCode::UnexpectedEndTag, "RHC1010", "",
              "Unexpected end tag",
              "An end tag was found that does not match any open element, so the document structure is ambiguous.",
              "Remove the end tag or add the matching start tag." },

            { DiagnosticCode::MissingRequiredAttribute, "RHC1011", "",
              "Required attribute is missing",
              "The element has no useful native representation without this attribute.",
              "Add the attribute listed in the suggestion." },

            { DiagnosticCode::InvalidAttributeValue, "RHC1012", "",
              "Invalid attribute value",
              "The value is outside the vocabulary the registry defines for this attribute, which would leave the "
              "native property in an undefined state.",
              "Use one of the accepted values listed in the suggestion." },

            { DiagnosticCode::InvalidInputType, "RHC1013", "",
              "Unknown input type",
              "The input element is specialised from its type attribute at compile time, so an unregistered type "
              "cannot be lowered to a native control.",
              "Use a registered input type." },

            { DiagnosticCode::MissingDocumentElement, "RHC1014", "",
              "Missing document element",
              "A compilable application needs an <html> document element so the native tree has a single root.",
              "Wrap the content in <html> ... </html>." },

            { DiagnosticCode::UnresolvedStyleReference, "MCSS001", "",
              "Unresolved style reference",
              "A class token is referenced by the markup but no stylesheet rule or Tailwind utility defines it, so "
              "the build-time style resolution would silently drop styling.",
              "Define the class in a stylesheet or correct the class name." },

            { DiagnosticCode::MalformedStyleSheet, "MCSS002", "",
              "Malformed stylesheet",
              "The stylesheet could not be parsed, so no style resolution is possible.",
              "Fix the reported position in the stylesheet." },

            { DiagnosticCode::UnresolvedScriptResource, "MJS001", "",
              "Unresolved script or element reference",
              "The behaviour module references an element that does not exist in the validated document, so the "
              "generated interaction table would contain a dangling binding.",
              "Add the missing id in the markup or correct the reference." },

            { DiagnosticCode::MalformedBehaviorScript, "MJS002", "",
              "Malformed behaviour module",
              "The behaviour module could not be analysed.",
              "Fix the reported position in the script." },

            { DiagnosticCode::InvalidSvgAttribute, "MSVG001", "",
              "Invalid SVG attribute for this element",
              "SVG attributes are individually registered and scoped to the elements the specification permits.",
              "Remove the attribute, or use it on an element the registry allows." },

            { DiagnosticCode::UnknownSvgElement, "MSVG002", "",
              "Unknown SVG element",
              "SVG is a first-class namespace with its own vocabulary; an unrecognised element cannot be given an "
              "SVG native category.",
              "Correct the element name." },

            { DiagnosticCode::InvalidMathMlAttribute, "MMATH001", "",
              "Invalid MathML attribute for this element",
              "MathML attributes are individually registered and scoped to the elements the specification permits.",
              "Remove the attribute, or use it on an element the registry allows." },

            { DiagnosticCode::UnknownMathMlElement, "MMATH002", "",
              "Unknown MathML element",
              "MathML is a first-class namespace with its own vocabulary and layout representation.",
              "Correct the element name." },

            { DiagnosticCode::MissingSourceFile, "RHC2001", "",
              "Source file is missing",
              "An application source referenced by the project could not be read.",
              "Check the file path in the project directory." },

            { DiagnosticCode::GeneratedOutputInvalid, "RHC2002", "",
              "Generated output failed validation",
              "The compiler validates its own browser output before declaring success, so a malformed build can "
              "never be reported as successful.",
              "This is a compiler defect; please report it with the generated document." },
        };

        const DiagnosticInfo* Lookup(DiagnosticCode code)
        {
            for (const DiagnosticInfo& info : DiagnosticCatalog)
            {
                if (info.code == code)
                {
                    return &info;
                }
            }
            return nullptr;
        }
    }

    std::string SourceLocation::Describe() const
    {
        std::ostringstream output;
        output << (file.empty() ? "<unknown>" : file) << ":" << line << ":" << column;
        return output.str();
    }

    const char* GetDiagnosticCodeString(DiagnosticCode code)
    {
        const DiagnosticInfo* info = Lookup(code);
        return (info != nullptr) ? info->codeString : "RHC0000";
    }

    const char* GetDiagnosticAlias(DiagnosticCode code)
    {
        const DiagnosticInfo* info = Lookup(code);
        return (info != nullptr) ? info->alias : "";
    }

    const char* GetDiagnosticTitle(DiagnosticCode code)
    {
        const DiagnosticInfo* info = Lookup(code);
        return (info != nullptr) ? info->title : "";
    }

    std::string Diagnostic::CodeString() const
    {
        return GetDiagnosticCodeString(code);
    }

    std::string Diagnostic::AliasString() const
    {
        return GetDiagnosticAlias(code);
    }

    std::string Diagnostic::Format() const
    {
        const DiagnosticInfo* info = Lookup(code);

        std::ostringstream output;
        output << "MyDevKit Error " << GetDiagnosticCodeString(code);

        const char* alias = GetDiagnosticAlias(code);
        if (alias != nullptr && alias[0] != '\0')
        {
            output << " (alias " << alias << ")";
        }
        output << "\n\n";

        output << "File: " << (location.file.empty() ? "<unknown>" : location.file) << "\n";
        output << "Line: " << location.line << "\n";
        output << "Column: " << location.column << "\n\n";

        output << message << "\n";

        if (!found.empty())
        {
            output << "\nFound:\n    " << found << "\n";
        }

        if (!expected.empty())
        {
            output << "\nUse:\n    " << expected << "\n";
        }

        const std::string effectiveExplanation =
            this->explanation.empty() && info != nullptr ? info->explanation : this->explanation;
        if (!effectiveExplanation.empty())
        {
            output << "\n" << effectiveExplanation << "\n";
        }

        const std::string effectiveFix =
            suggestedFix.empty() && info != nullptr ? info->suggestedFix : suggestedFix;
        if (!effectiveFix.empty())
        {
            output << "\nSuggested fix:\n    " << effectiveFix << "\n";
        }

        return output.str();
    }

    void DiagnosticSink::Report(Diagnostic diagnostic)
    {
        if (diagnostic.severity == Severity::Error)
        {
            ++errorCount;
        }
        else if (diagnostic.severity == Severity::Warning)
        {
            ++warningCount;
        }

        if (diagnostic.message.empty())
        {
            diagnostic.message = GetDiagnosticTitle(diagnostic.code);
        }

        diagnostics.push_back(std::move(diagnostic));
    }

    void DiagnosticSink::Error(DiagnosticCode code,
                               const SourceLocation& location,
                               std::string message,
                               std::string explanation,
                               std::string suggestedFix,
                               std::string found,
                               std::string expected)
    {
        Diagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.severity = Severity::Error;
        diagnostic.location = location;
        diagnostic.message = std::move(message);
        diagnostic.explanation = std::move(explanation);
        diagnostic.suggestedFix = std::move(suggestedFix);
        diagnostic.found = std::move(found);
        diagnostic.expected = std::move(expected);
        Report(std::move(diagnostic));
    }

    void DiagnosticSink::Warning(DiagnosticCode code,
                                 const SourceLocation& location,
                                 std::string message,
                                 std::string explanation,
                                 std::string suggestedFix)
    {
        Diagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.severity = Severity::Warning;
        diagnostic.location = location;
        diagnostic.message = std::move(message);
        diagnostic.explanation = std::move(explanation);
        diagnostic.suggestedFix = std::move(suggestedFix);
        Report(std::move(diagnostic));
    }

    std::string DiagnosticSink::FormatAll() const
    {
        std::ostringstream output;
        for (std::size_t index = 0; index < diagnostics.size(); ++index)
        {
            if (index > 0)
            {
                output << "\n";
            }
            output << diagnostics[index].Format() << "\n";
        }
        return output.str();
    }

    std::string DiagnosticSink::FormatErrors() const
    {
        std::ostringstream output;
        bool first = true;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity != Severity::Error)
            {
                continue;
            }
            if (!first)
            {
                output << "\n";
            }
            first = false;
            output << diagnostic.Format() << "\n";
        }
        return output.str();
    }
}
