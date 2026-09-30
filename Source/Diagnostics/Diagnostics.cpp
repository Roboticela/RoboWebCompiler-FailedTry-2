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
              "A style attribute puts CSS inside the HTML file, and the compiler reads CSS before it builds "
              "the page. It cannot read a style attribute at that point.",
              "Give the element a class and write the rule in a .css file, for example class=\"card\" with "
              ".card { padding: 16px }." },

            { DiagnosticCode::InlineEventHandlerAttribute, "MHTML002", "",
              "Inline JavaScript event handlers are forbidden",
              "An attribute such as onclick puts JavaScript inside the HTML file, and this compiler reads "
              "JavaScript from .js files instead.",
              "Move the code into a .js file and write element.addEventListener(\"click\", handler)." },

            { DiagnosticCode::AttributeNotAllowed, "MHTML003", "",
              "Attribute is not valid for this element",
              "HTML gives each element its own list of attributes, and this attribute is not on the list for "
              "this element.",
              "Remove the attribute, or use an element that does allow it." },

            { DiagnosticCode::UnknownHtmlElement, "MHTML004", "",
              "Unknown HTML element",
              "The compiler does not know this tag. It is not an HTML element, and it is not a valid custom "
              "element name either.",
              "Check the spelling, or name a custom element with lowercase letters and a hyphen, for example "
              "<my-panel>." },

            { DiagnosticCode::UnknownAttribute, "MHTML005", "",
              "Unknown attribute",
              "This attribute is not part of HTML, so the compiler will not guess what it means.",
              "Correct the spelling, remove it, or use a data-* attribute for values of your own." },

            { DiagnosticCode::UnknownAriaAttribute, "MHTML006", "",
              "Unknown ARIA attribute",
              "This aria-* attribute is not one of the ARIA names the compiler knows.",
              "Check the spelling against the ARIA list, or remove the attribute." },

            { DiagnosticCode::InlineStyleElement, "RHC1001", "",
              "Inline <style> is forbidden",
              "A <style> block puts CSS inside the HTML file, and the compiler reads the CSS files first.",
              "Move the rules into a .css file and link it: <link rel=\"stylesheet\" href=\"app.css\">." },

            { DiagnosticCode::InlineScriptContent, "RHC1002", "",
              "Inline JavaScript is forbidden",
              "Code inside a <script> block sits in the HTML file, and this compiler reads JavaScript from .js "
              "files instead.",
              "Move the code into a .js file and load it with <script src=\"app.js\" defer></script>." },

            { DiagnosticCode::ObsoleteElement, "RHC1004", "",
              "Obsolete element",
              "HTML no longer has this element, so there is nothing for the compiler to build.",
              "Use the element that replaced it. The message names it." },

            { DiagnosticCode::ObsoleteAttribute, "RHC1005", "",
              "Obsolete attribute",
              "HTML no longer has this attribute, so its meaning is gone.",
              "Remove it and express the same idea with a class and a CSS rule, or with a data-* attribute." },

            { DiagnosticCode::InvalidCustomElementName, "RHC1006", "",
              "Invalid custom element name",
              "A custom element name must use lowercase letters, contain a hyphen, and start with a letter.",
              "Rename it, for example <my-panel>." },

            { DiagnosticCode::DuplicateAttribute, "RHC1008", "",
              "Duplicate attribute",
              "The same attribute is written twice, so the compiler would not know which value to use.",
              "Keep one of the two and delete the other." },

            { DiagnosticCode::MalformedMarkup, "RHC1009", "",
              "Malformed markup",
              "The HTML at this position does not follow the HTML rules, so no page can be built from it.",
              "Fix the HTML at the line and column shown above." },

            { DiagnosticCode::UnexpectedEndTag, "RHC1010", "",
              "Unexpected end tag",
              "This closing tag does not close anything, so the structure of the page is unclear.",
              "Delete the closing tag, or add the matching opening tag before it." },

            { DiagnosticCode::MissingRequiredAttribute, "RHC1011", "",
              "Required attribute is missing",
              "Without this attribute the element cannot be built.",
              "Add the attribute named in the message above." },

            { DiagnosticCode::InvalidAttributeValue, "RHC1012", "",
              "Invalid attribute value",
              "This value is not one of the values HTML allows for this attribute.",
              "Use one of the values listed in the message above." },

            { DiagnosticCode::InvalidInputType, "RHC1013", "",
              "Unknown input type",
              "The compiler builds a different control for each input type, and it does not know this one.",
              "Use a type from the HTML list, for example text, email, password or number." },

            { DiagnosticCode::MissingDocumentElement, "RHC1014", "",
              "Missing document element",
              "Every page needs one <html> element to be the root of the built app.",
              "Put the whole page inside <html> ... </html>." },

            { DiagnosticCode::UnresolvedStyleReference, "MCSS001", "",
              "Unresolved style reference",
              "The HTML uses this class, but no CSS rule defines it, so the element would be built without the "
              "style you meant to give it.",
              "Add a rule for the class in a .css file, or correct the class name in the HTML." },

            { DiagnosticCode::MalformedStyleSheet, "MCSS002", "",
              "Malformed stylesheet",
              "The CSS at this position does not follow the CSS rules, so the styles cannot be read.",
              "Fix the CSS at the line and column shown above." },

            { DiagnosticCode::UnresolvedScriptResource, "MJS001", "",
              "Unresolved script or element reference",
              "A click or an input on that id would have nothing to run on, so the compiler stops before it "
              "builds the page.",
              "Add the id to the element in the HTML file, or correct the id in the .js file. The two must "
              "match letter for letter." },

            { DiagnosticCode::MalformedBehaviorScript, "MJS002", "",
              "Malformed behaviour module",
              "The JavaScript does something this compiler does not accept, so no interaction can be built "
              "from it.",
              "Fix the JavaScript at the line and column shown above." },

            { DiagnosticCode::InvalidSvgAttribute, "MSVG001", "",
              "Invalid SVG attribute for this element",
              "SVG gives each element its own list of attributes, and this attribute is not on the list for this "
              "element.",
              "Remove the attribute, or use it on an SVG element that allows it." },

            { DiagnosticCode::UnknownSvgElement, "MSVG002", "",
              "Unknown SVG element",
              "This tag is not an SVG element the compiler knows.",
              "Check the spelling of the SVG element name." },

            { DiagnosticCode::InvalidMathMlAttribute, "MMATH001", "",
              "Invalid MathML attribute for this element",
              "MathML gives each element its own list of attributes, and this attribute is not on the list for "
              "this element.",
              "Remove the attribute, or use it on a MathML element that allows it." },

            { DiagnosticCode::UnknownMathMlElement, "MMATH002", "",
              "Unknown MathML element",
              "This tag is not a MathML element the compiler knows.",
              "Check the spelling of the MathML element name." },

            { DiagnosticCode::MissingSourceFile, "RHC2001", "",
              "Source file is missing",
              "The compiler was asked to read this file or folder, and it is not there.",
              "Check the path for typing mistakes, and make sure the file or folder really exists." },

            { DiagnosticCode::GeneratedOutputInvalid, "RHC2002", "",
              "Generated output failed validation",
              "The compiler checks the code it writes, and this check failed. This is a fault in the compiler, "
              "not in your page.",
              "Send the message above and the generated file to the authors of the compiler." },
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
        return codeOverride.empty() ? std::string(GetDiagnosticCodeString(code)) : codeOverride;
    }

    std::string Diagnostic::ToolName() const
    {
        if (!tool.empty())
        {
            return tool;
        }

        // The code says who owns it: a CSS problem belongs to the CSS compiler,
        // and everything else to this one. Naming the tool in the report matters
        // because the two compilers have separate documentation.
        const std::string codeText = CodeString();
        return codeText.compare(0, 3, "RCC") == 0 ? "RoboCSSCompiler" : "RoboHTMLCompiler";
    }

    std::string Diagnostic::AliasString() const
    {
        return GetDiagnosticAlias(code);
    }

    std::string Diagnostic::Format() const
    {
        const DiagnosticInfo* info = Lookup(code);

        std::ostringstream output;
        // The word matches the severity: a warning that reads "Error" teaches a
        // reader to distrust the line above the code.
        output << ToolName() << (severity == Severity::Error ? " Error " : " Warning ") << CodeString();

        // An alias is this compiler's own second name for a code, so a code that
        // came from another tool has none.
        const char* alias = codeOverride.empty() ? GetDiagnosticAlias(code) : "";
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

        // The catalog explains this compiler's codes. A code that came from
        // another tool brings its own words and must not be given an explanation
        // that belongs to a different failure.
        const bool usesCatalog = codeOverride.empty();

        const std::string effectiveExplanation =
            this->explanation.empty() && info != nullptr && usesCatalog ? info->explanation : this->explanation;
        if (!effectiveExplanation.empty())
        {
            output << "\n" << effectiveExplanation << "\n";
        }

        const std::string effectiveFix =
            suggestedFix.empty() && info != nullptr && usesCatalog ? info->suggestedFix : suggestedFix;
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

        if (diagnostic.message.empty() && diagnostic.codeOverride.empty())
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

    std::string DiagnosticSink::FormatWarnings() const
    {
        std::ostringstream output;
        bool first = true;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity != Severity::Warning)
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
