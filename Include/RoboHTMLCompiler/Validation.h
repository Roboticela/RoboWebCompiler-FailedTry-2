// ---------------------------------------------------------------------------
// RoboHTMLCompiler - validation.
//
// Validation is total and strict. Violations are reported with a precise
// diagnostic and compilation stops; the compiler never repairs source behind
// the developer's back.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Registry.h"

#include <string>

namespace RoboHTMLCompiler
{
    enum class Strictness
    {
        /// Violations that would be errors are downgraded where the standard
        /// allows it. Inline CSS and inline JavaScript remain errors.
        Lax,

        /// Default. Every rule violation is an error.
        Strict,

        /// Strict plus warnings promoted to errors.
        Pedantic
    };

    struct ValidationOptions
    {
        Strictness strictness = Strictness::Strict;

        /// Reject obsolete (non-conforming) elements and attributes.
        bool rejectObsoleteElements = true;
        bool rejectObsoleteAttributes = true;

        /// Require a document element.
        bool requireDocumentElement = true;
    };

    /// Validates the HTML namespace and dispatches SVG/MathML subtrees to their
    /// own validators.
    class HtmlValidator
    {
    public:
        HtmlValidator(DiagnosticSink& diagnostics, const Registry& registry, ValidationOptions options);

        /// Returns true when the document is valid. Diagnostics are reported
        /// through the sink as they are found.
        bool Validate(const MarkupDocument& document);

        bool ValidateElement(const ElementNode& element);

    private:
        bool ValidateAttributes(const ElementNode& element);
        bool ValidateVocabulary(const ElementNode& element);

        DiagnosticSink& diagnostics;
        const Registry& registry;
        ValidationOptions options;
    };

    /// Validates an SVG subtree against the SVG registries.
    class SvgValidator
    {
    public:
        SvgValidator(DiagnosticSink& diagnostics, const Registry& registry);

        bool ValidateSubtree(const ElementNode& element);

    private:
        bool ValidateElement(const ElementNode& element);

        DiagnosticSink& diagnostics;
        const Registry& registry;
    };

    /// Validates a MathML subtree against the MathML registries.
    class MathMlValidator
    {
    public:
        MathMlValidator(DiagnosticSink& diagnostics, const Registry& registry);

        bool ValidateSubtree(const ElementNode& element);

    private:
        bool ValidateElement(const ElementNode& element);

        DiagnosticSink& diagnostics;
        const Registry& registry;
    };
}
