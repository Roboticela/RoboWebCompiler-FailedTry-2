#include "RoboCSSCompiler/CssDiagnostics.h"

#include <cstdio>
#include <sstream>

namespace RoboCSSCompiler
{
    const char* ToString(CSSDiagnosticCode code)
    {
        switch (code)
        {
        case CSSDiagnosticCode::UnknownProperty: return "RCC001";
        case CSSDiagnosticCode::InvalidValue: return "RCC002";
        case CSSDiagnosticCode::InvalidSelector: return "RCC003";
        case CSSDiagnosticCode::InvalidDeclaration: return "RCC004";
        case CSSDiagnosticCode::InvalidFunction: return "RCC005";
        case CSSDiagnosticCode::InvalidUnit: return "RCC006";
        case CSSDiagnosticCode::InvalidAtRule: return "RCC007";
        case CSSDiagnosticCode::InvalidGrammar: return "RCC008";
        case CSSDiagnosticCode::InvalidShorthand: return "RCC009";
        case CSSDiagnosticCode::UnsupportedFeature: return "RCC010";
        case CSSDiagnosticCode::MalformedStylesheet: return "RCC011";
        case CSSDiagnosticCode::IgnoredRule: return "RCC012";
        }

        return "RCC???";
    }

    const char* ToString(CSSSeverity severity)
    {
        return severity == CSSSeverity::Error ? "error" : "warning";
    }

    void CSSDiagnosticSink::Report(CSSDiagnosticCode code,
                                   CSSSeverity severity,
                                   const CSSSourceLocation& location,
                                   std::string message,
                                   std::string detail,
                                   std::string suggestion,
                                   std::string subject)
    {
        if (severity == CSSSeverity::Error)
        {
            ++errorCount;
        }
        else
        {
            ++warningCount;
        }

        if (entries.size() >= MaxEntries)
        {
            ++suppressedCount;
            return;
        }

        CSSDiagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.severity = severity;
        diagnostic.location = location;
        diagnostic.message = std::move(message);
        diagnostic.detail = std::move(detail);
        diagnostic.suggestion = std::move(suggestion);
        diagnostic.subject = std::move(subject);
        entries.push_back(std::move(diagnostic));
    }

    void CSSDiagnosticSink::Error(CSSDiagnosticCode code,
                                  const CSSSourceLocation& location,
                                  std::string message,
                                  std::string detail,
                                  std::string suggestion,
                                  std::string subject)
    {
        Report(code, CSSSeverity::Error, location, std::move(message), std::move(detail), std::move(suggestion),
               std::move(subject));
    }

    void CSSDiagnosticSink::Warning(CSSDiagnosticCode code,
                                    const CSSSourceLocation& location,
                                    std::string message,
                                    std::string detail,
                                    std::string suggestion,
                                    std::string subject)
    {
        Report(code, CSSSeverity::Warning, location, std::move(message), std::move(detail), std::move(suggestion),
               std::move(subject));
    }

    void CSSDiagnosticSink::Clear()
    {
        entries.clear();
        errorCount = 0;
        warningCount = 0;
        suppressedCount = 0;
    }

    bool CSSDiagnosticSink::HasCode(CSSDiagnosticCode code) const
    {
        for (const CSSDiagnostic& entry : entries)
        {
            if (entry.code == code)
            {
                return true;
            }
        }

        return false;
    }

    std::string CSSDiagnosticSink::Format() const
    {
        std::ostringstream output;

        for (const CSSDiagnostic& entry : entries)
        {
            if (entry.location.IsValid())
            {
                output << entry.location.file << ":" << entry.location.line << ":" << entry.location.column << ": ";
            }

            output << ToString(entry.severity) << " " << ToString(entry.code) << ": " << entry.message;

            if (!entry.detail.empty())
            {
                output << " (" << entry.detail << ")";
            }

            output << "\n";

            if (!entry.suggestion.empty())
            {
                output << "    " << entry.suggestion << "\n";
            }
        }

        if (suppressedCount > 0)
        {
            output << "... " << suppressedCount << " further diagnostics suppressed\n";
        }

        return output.str();
    }
}
