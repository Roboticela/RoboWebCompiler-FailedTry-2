// ---------------------------------------------------------------------------
// RoboCSSCompiler - CSS diagnostics.
//
// CSS problems are CSS problems, so they get their own code space:
//
//     RoboHTMLCompiler   RHC / MHTML / MCSS / MJS / MSVG / MMATH
//     RoboRenderer       RRE
//     RoboCSSCompiler    RCC
//
// Every diagnostic carries where it came from (file, line, column, offset) and
// the text it is about, because a stylesheet error is almost always about one
// declaration and the fastest fix is being told which one.
// ---------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    /// Where in a stylesheet something was written.
    struct CSSSourceLocation
    {
        std::string file;
        std::uint32_t line = 1;
        std::uint32_t column = 1;
        /// Byte offset of the token, which is what a span in an editor needs.
        std::uint32_t offset = 0;

        bool IsValid() const { return !file.empty(); }
    };

    /// The CSS error codes. RCC001..RCC010 are the ones the specification of the
    /// compiler promises; the rest subdivide the same ground so that a message
    /// can be precise without inventing a new code for every sentence.
    enum class CSSDiagnosticCode : std::uint16_t
    {
        /// A property name the compiler does not know at all.
        UnknownProperty = 1,
        /// The value is not legal for the property's grammar.
        InvalidValue = 2,
        /// A selector could not be parsed.
        InvalidSelector = 3,
        /// A declaration could not be parsed (missing colon, missing value).
        InvalidDeclaration = 4,
        /// A function call is malformed or the function is unknown.
        InvalidFunction = 5,
        /// A unit is unknown, or is not allowed for this value type.
        InvalidUnit = 6,
        /// An at-rule is malformed, or is not one the compiler knows.
        InvalidAtRule = 7,
        /// The value parses but does not fit the property's grammar shape.
        InvalidGrammar = 8,
        /// A shorthand's component values are contradictory.
        InvalidShorthand = 9,
        /// The feature exists in the reference and is not implemented yet.
        UnsupportedFeature = 10,
        /// A structural problem in the stylesheet's syntax.
        MalformedStylesheet = 11,
        /// Between the supported tokens but meaningless (e.g. an empty selector).
        IgnoredRule = 12
    };

    enum class CSSSeverity
    {
        /// Reported; the declaration is dropped and the rest of the sheet stands.
        Warning,
        /// Reported; the compilation fails.
        Error
    };

    const char* ToString(CSSDiagnosticCode code);
    const char* ToString(CSSSeverity severity);

    struct CSSDiagnostic
    {
        CSSDiagnosticCode code;
        CSSSeverity severity;
        CSSSourceLocation location;
        std::string message;
        /// Longer explanation: why the value is wrong, what was expected.
        std::string detail;
        /// What the compiler suggests doing instead.
        std::string suggestion;
        /// The property or selector text the diagnostic is about.
        std::string subject;
    };

    /// Collects diagnostics with a hard entry limit, so a pathological
    /// stylesheet reports its first N problems and counts the rest.
    class CSSDiagnosticSink
    {
    public:
        static constexpr std::size_t MaxEntries = 128;

        void Report(CSSDiagnosticCode code,
                    CSSSeverity severity,
                    const CSSSourceLocation& location,
                    std::string message,
                    std::string detail = std::string(),
                    std::string suggestion = std::string(),
                    std::string subject = std::string());

        void Error(CSSDiagnosticCode code,
                   const CSSSourceLocation& location,
                   std::string message,
                   std::string detail = std::string(),
                   std::string suggestion = std::string(),
                   std::string subject = std::string());

        void Warning(CSSDiagnosticCode code,
                     const CSSSourceLocation& location,
                     std::string message,
                     std::string detail = std::string(),
                     std::string suggestion = std::string(),
                     std::string subject = std::string());

        void Clear();

        const std::vector<CSSDiagnostic>& Entries() const { return entries; }

        std::size_t ErrorCount() const { return errorCount; }
        std::size_t WarningCount() const { return warningCount; }
        bool HasErrors() const { return errorCount != 0; }
        std::size_t SuppressedCount() const { return suppressedCount; }

        /// True when this exact code has been reported already, which is how a
        /// repeated problem in a large sheet collapses to one line.
        bool HasCode(CSSDiagnosticCode code) const;

        /// One diagnostic per line, "file:line:col: severity RCCnnn: message".
        std::string Format() const;

    private:
        std::vector<CSSDiagnostic> entries;
        std::size_t errorCount = 0;
        std::size_t warningCount = 0;
        std::size_t suppressedCount = 0;
    };
}
