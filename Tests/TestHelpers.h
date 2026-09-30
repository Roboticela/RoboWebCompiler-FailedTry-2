#pragma once

#include "RoboHTMLCompiler/Parsers.h"
#include "RoboHTMLCompiler/Registry.h"
#include "RoboHTMLCompiler/Validation.h"

#include <memory>
#include <string>

namespace RoboTest
{
    using namespace RoboHTMLCompiler;

    /// Wraps a fragment in a complete document so validation rules that need a
    /// document element can run.
    inline std::string WrapBody(const std::string& body)
    {
        return "<!DOCTYPE html>\n<html lang=\"en\"><head><title>T</title></head><body>\n" + body +
               "\n</body></html>\n";
    }

    inline std::unique_ptr<MarkupDocument> ParseHtml(const std::string& source,
                                                     DiagnosticSink& sink,
                                                     const std::string& file = "Test.html")
    {
        HtmlParser parser(sink, Registry::Instance(), file);
        return parser.Parse(source);
    }

    /// Parses and validates a complete document.
    inline bool ValidateDocument(const std::string& source,
                                 DiagnosticSink& sink,
                                 ValidationOptions options = ValidationOptions{},
                                 const std::string& file = "Test.html")
    {
        const std::size_t errorsBefore = sink.ErrorCount();

        std::unique_ptr<MarkupDocument> document = ParseHtml(source, sink, file);
        if (document == nullptr)
        {
            return false;
        }

        HtmlValidator validator(sink, Registry::Instance(), options);
        const bool validated = validator.Validate(*document);

        // Parsing is part of the pipeline: a document that produced a diagnostic
        // while it was being read is not valid, even when the validator's own
        // rules are satisfied.
        return validated && sink.ErrorCount() == errorsBefore;
    }

    /// Parses and validates a body fragment wrapped in a document.
    inline bool ValidateBody(const std::string& body,
                             DiagnosticSink& sink,
                             ValidationOptions options = ValidationOptions{})
    {
        return ValidateDocument(WrapBody(body), sink, options);
    }

    /// The first error with the given code, so a test can assert on where the
    /// report points and not only that it happened.
    inline const Diagnostic* FindDiagnostic(const DiagnosticSink& sink, DiagnosticCode code)
    {
        for (const Diagnostic& diagnostic : sink.GetDiagnostics())
        {
            if (diagnostic.severity == Severity::Error && diagnostic.code == code)
            {
                return &diagnostic;
            }
        }
        return nullptr;
    }

    /// True when the sink contains an error with the given code.
    inline bool HasError(const DiagnosticSink& sink, DiagnosticCode code)
    {
        return FindDiagnostic(sink, code) != nullptr;
    }

    /// The first warning with the given code, so a test can assert on a report
    /// that must not stop a build.
    inline const Diagnostic* FindWarning(const DiagnosticSink& sink, DiagnosticCode code)
    {
        for (const Diagnostic& diagnostic : sink.GetDiagnostics())
        {
            if (diagnostic.severity == Severity::Warning && diagnostic.code == code)
            {
                return &diagnostic;
            }
        }
        return nullptr;
    }

    /// True when the sink contains a warning with the given code.
    inline bool HasWarning(const DiagnosticSink& sink, DiagnosticCode code)
    {
        return FindWarning(sink, code) != nullptr;
    }

    inline std::string FormatDiagnostics(const DiagnosticSink& sink)
    {
        return sink.FormatAll();
    }
}
