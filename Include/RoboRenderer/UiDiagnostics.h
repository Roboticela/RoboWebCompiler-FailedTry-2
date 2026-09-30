// ---------------------------------------------------------------------------
// RoboRenderer - runtime diagnostics.
//
// Renderer diagnostics are separate from compiler diagnostics on purpose:
//
//     RoboHTMLCompiler   RHC / MHTML / MCSS / MJS / ...   source problems
//     RoboRenderer       RRE                               compiled data and
//                                                          backend problems
//
// Nothing the runtime cannot act on is ever silently ignored. Compiling a
// document that produces invalid compiled data fails loudly at load time, and
// the renderer refuses to draw from data it could not validate.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RoboRenderer
{
    enum class RendererDiagnosticCode : std::uint16_t
    {
        /// An element index is out of range or is not a valid element.
        InvalidElementId = 1,
        /// A child/sibling link is inconsistent or out of range.
        InvalidChildIndex = 2,
        /// A resource index is out of range.
        InvalidResourceId = 3,
        /// The element category cannot be rendered by the HTML-only runtime.
        UnsupportedElementKind = 4,
        /// Geometry or a numeric field is not a usable layout input.
        InvalidLayoutData = 5,
        /// The graphics backend reported a failure.
        RenderingBackendFailure = 6,
        /// A binding names an event the runtime does not model.
        UnknownEventBinding = 7,
        /// A string index is out of range.
        InvalidStringIndex = 8,
        /// A style index is out of range.
        InvalidStyleIndex = 9,
        /// A resource could not be loaded or decoded.
        ResourceLoadFailure = 10,
        /// The compiled document itself is unusable (no root, no elements).
        InvalidDocument = 11,
        /// Two elements share one identifier.
        DuplicateIdentifier = 12,
        /// An element is not reachable from the root.
        OrphanedElement = 13
    };

    enum class RendererSeverity
    {
        /// Reported, rendering continues.
        Warning,
        /// Rendering is refused.
        Error
    };

    struct RendererDiagnostic
    {
        RendererDiagnosticCode code;
        RendererSeverity severity;
        /// Element the diagnostic refers to, or NoElement.
        ElementIndex element;
        std::string message;
        std::string detail;
    };

    /// Stable code name, so logs never depend on an enum's numeric value.
    const char* ToString(RendererDiagnosticCode code);
    const char* ToString(RendererSeverity severity);

    /// Collects diagnostics with a hard entry limit: a pathological document
    /// reports its first N problems and then counts the rest, so diagnosing a
    /// broken 100,000 element tree cannot itself become the bottleneck.
    class RendererDiagnostics
    {
    public:
        static constexpr std::size_t MaxEntries = 64;

        void Report(RendererDiagnosticCode code,
                    RendererSeverity severity,
                    ElementIndex element,
                    std::string message,
                    std::string detail = std::string());

        void Error(RendererDiagnosticCode code, ElementIndex element, std::string message,
                   std::string detail = std::string());

        void Warning(RendererDiagnosticCode code, ElementIndex element, std::string message,
                     std::string detail = std::string());

        void Clear();

        const std::vector<RendererDiagnostic>& Entries() const { return entries; }

        std::size_t ErrorCount() const { return errorCount; }
        std::size_t WarningCount() const { return warningCount; }
        bool HasErrors() const { return errorCount != 0; }

        /// Count of diagnostics that were suppressed by the entry limit.
        std::size_t SuppressedCount() const { return suppressedCount; }

        /// Human readable dump, one diagnostic per line.
        std::string Format() const;

    private:
        std::vector<RendererDiagnostic> entries;
        std::size_t errorCount = 0;
        std::size_t warningCount = 0;
        std::size_t suppressedCount = 0;
    };

    /// Checks that a compiled document is internally consistent: every index in
    /// range, every tree link reciprocal, no cycles, the root reachable, and
    /// every category either supported or reported. Returns true when the
    /// document can be rendered (errors are fatal, warnings are not).
    ///
    /// This runs once, at load time; the runtime does no validity checking per
    /// frame.
    bool ValidateCompiledDocument(const CompiledDocument& document, RendererDiagnostics& diagnostics);
}
