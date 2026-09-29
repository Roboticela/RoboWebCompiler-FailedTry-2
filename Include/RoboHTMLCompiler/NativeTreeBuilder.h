// ---------------------------------------------------------------------------
// RoboHTMLCompiler - native lowering.
//
// Normalisation and lowering: source elements are mapped onto the compact
// native categories, attributes are routed into strongly typed properties, and
// the resulting tree carries only what the runtime needs. Source metadata is
// preserved for the browser target and for debugging, but the optimiser may
// drop it from release builds.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"
#include "RoboHTMLCompiler/Registry.h"
#include "RoboHTMLCompiler/StyleResolver.h"

#include <memory>

namespace RoboHTMLCompiler
{
    class NativeTreeBuilder
    {
    public:
        NativeTreeBuilder(DiagnosticSink& diagnostics,
                          const Registry& registry,
                          StyleReferenceResolver& styleReferenceResolver);

        /// Lowers a validated document into the native UI IR.
        std::unique_ptr<NativeElement> Build(const ElementNode& element,
                                             NativeUiModel& model);

        /// Starts from a document and produces a complete model.
        std::unique_ptr<NativeElement> BuildDocument(const MarkupDocument& document,
                                                     NativeUiModel& model,
                                                     ElementId& nextElementId);

    private:
        ElementKind NormalizeKind(const ElementNode& element) const;

        void RouteAttribute(NativeElement& target,
                            const AttributeDefinition& definition,
                            const AttributeNode& attribute) const;

        void ApplyGenericAttribute(NativeElement& target,
                                   const AttributeDefinition& definition,
                                   const AttributeNode& attribute) const;

        DiagnosticSink& diagnostics;
        const Registry& registry;
        StyleReferenceResolver& styleReferenceResolver;
        ElementId nextElementId = 1;
    };
}
