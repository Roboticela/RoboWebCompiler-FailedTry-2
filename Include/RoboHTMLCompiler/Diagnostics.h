// ---------------------------------------------------------------------------
// RoboHTMLCompiler - diagnostics.
//
// The compiler is strict: source that violates a RoboHTMLCompiler rule is never
// silently repaired, reinterpreted or partially compiled. Every failure carries
// file, line, column, a stable error code, a human readable message, an
// explanation and a suggested fix.
// ---------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    /// A position in a source document.
    struct SourceLocation
    {
        std::string file;
        int line = 1;
        int column = 1;
        std::size_t offset = 0;

        bool IsValid() const { return !file.empty(); }
        std::string Describe() const;
    };

    enum class Severity
    {
        Note,
        Warning,
        Error
    };

    /// Stable diagnostic identifiers. Several codes carry a documented alias so
    /// that older documentation keeps working (for example RHC1007 aliases the
    /// inline-CSS diagnostic MHTML001).
    enum class DiagnosticCode : std::uint32_t
    {
        None = 0,

        // HTML vocabulary and structure
        InlineCssAttribute = 1,          // MHTML001 (alias RHC1007)
        InlineEventHandlerAttribute = 2, // MHTML002
        AttributeNotAllowed = 3,         // MHTML003
        UnknownHtmlElement = 4,          // MHTML004
        UnknownAttribute = 5,            // MHTML005
        UnknownAriaAttribute = 6,        // MHTML006
        InlineStyleElement = 7,          // RHC1001
        InlineScriptContent = 8,         // RHC1002
        ObsoleteElement = 9,             // RHC1004
        ObsoleteAttribute = 10,          // RHC1005
        InvalidCustomElementName = 11,   // RHC1006
        DuplicateAttribute = 12,         // RHC1008
        MalformedMarkup = 13,            // RHC1009
        UnexpectedEndTag = 14,           // RHC1010
        MissingRequiredAttribute = 15,   // RHC1011
        InvalidAttributeValue = 16,      // RHC1012
        InvalidInputType = 17,           // RHC1013
        MissingDocumentElement = 18,     // RHC1014

        // Style references
        UnresolvedStyleReference = 40,   // MCSS001
        MalformedStyleSheet = 41,        // MCSS002

        // Behaviour
        UnresolvedScriptResource = 60,   // MJS001
        MalformedBehaviorScript = 61,    // MJS002

        // SVG
        InvalidSvgAttribute = 80,        // MSVG001
        UnknownSvgElement = 81,          // MSVG002

        // MathML
        InvalidMathMlAttribute = 100,    // MMATH001
        UnknownMathMlElement = 101,      // MMATH002

        // Pipeline
        MissingSourceFile = 120,         // RHC2001
        GeneratedOutputInvalid = 121     // RHC2002
    };

    /// One compiler diagnostic.
    struct Diagnostic
    {
        DiagnosticCode code = DiagnosticCode::None;
        Severity severity = Severity::Error;
        SourceLocation location;
        std::string message;
        std::string explanation;
        std::string suggestedFix;
        std::string found;
        std::string expected;

        /// The tool the reader has to fix the problem in, and the code it should
        /// be reported under, when they are not this compiler's own.
        ///
        /// A CSS problem is a RoboCSSCompiler problem: it is reported under its
        /// RCC code, by name, so that a reader who looks up RCC010 in the CSS
        /// documentation finds the CSS documentation. Both fields empty - the
        /// usual case - means "this compiler, this code".
        std::string tool;
        std::string codeOverride;

        /// The canonical code such as "MHTML001".
        std::string CodeString() const;

        /// The tool that owns the diagnostic: "RoboHTMLCompiler" or
        /// "RoboCSSCompiler".
        std::string ToolName() const;

        /// Additional code accepted for this diagnostic, or an empty string.
        std::string AliasString() const;

        /// Full multi-line report as required by the compiler's error policy.
        std::string Format() const;
    };

    /// Canonical code for a diagnostic identifier.
    const char* GetDiagnosticCodeString(DiagnosticCode code);

    /// Documented alias for a diagnostic identifier, or an empty string.
    const char* GetDiagnosticAlias(DiagnosticCode code);

    /// Short title describing the rule that produced the diagnostic.
    const char* GetDiagnosticTitle(DiagnosticCode code);

    /// Collects diagnostics for one compilation unit.
    class DiagnosticSink
    {
    public:
        void Report(Diagnostic diagnostic);

        /// Reports an error, filling in the standard explanation for the code
        /// when none is supplied.
        void Error(DiagnosticCode code,
                   const SourceLocation& location,
                   std::string message,
                   std::string explanation = std::string(),
                   std::string suggestedFix = std::string(),
                   std::string found = std::string(),
                   std::string expected = std::string());

        /// Reports a warning that does not stop compilation.
        void Warning(DiagnosticCode code,
                     const SourceLocation& location,
                     std::string message,
                     std::string explanation = std::string(),
                     std::string suggestedFix = std::string());

        bool HasErrors() const { return errorCount > 0; }
        std::size_t ErrorCount() const { return errorCount; }
        std::size_t WarningCount() const { return warningCount; }
        std::size_t Count() const { return diagnostics.size(); }

        const std::vector<Diagnostic>& GetDiagnostics() const { return diagnostics; }

        /// Renders every diagnostic, in report order.
        std::string FormatAll() const;

        /// Renders only errors.
        std::string FormatErrors() const;

        /// Renders only warnings. A warning is a feature the compiler read and
        /// could not apply - `max-width` is one - so a build that prints its
        /// errors and hides its warnings silently does nothing for a declaration
        /// the author wrote.
        std::string FormatWarnings() const;

    private:
        std::vector<Diagnostic> diagnostics;
        std::size_t errorCount = 0;
        std::size_t warningCount = 0;
    };
}
