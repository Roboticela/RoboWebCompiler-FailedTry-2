// ---------------------------------------------------------------------------
// RoboHTMLCompiler - compile time lowering into the renderer's data format.
//
// The native IR (Ir.h) is the compiler's own object tree: convenient to build,
// inspect and dump, but a heap node per element with strings everywhere. The
// runtime does not consume it. This stage flattens the same information into
// RoboRenderer/CompiledUi.h:
//
//     NativeUiModel (object tree, compiler side)
//              |
//              v
//     CompiledUiStorage (flat, indexed, deduplicated, immutable)
//              |
//     +--------+---------+
//     |                  |
//     v                  v
//     RuntimeTargetGenerator   RoboRenderer (tests, tools, in-process use)
//     (C++ source: constexpr tables)
//
// There is exactly one runtime representation: this one. Nothing here parses
// HTML or CSS; the tree is already validated and normalised by the time it
// arrives.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/CssStyleBuilder.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"
#include "RoboRenderer/CompiledUi.h"

#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    /// Owns the storage a compiled document view points at. All tables are
    /// contiguous and immutable once built, and the interned string literals are
    /// materialised last so the pointers in the view stay valid.
    class CompiledUiStorage
    {
    public:
        /// The flat view. Only valid after a successful Build(), and only while
        /// the storage lives: it points into the storage's own tables. Returned
        /// by reference because the view is the document, not a copy of it.
        const RoboRenderer::CompiledDocument& View() const { return view; }

        const std::string& ApplicationName() const { return applicationName; }
        const std::vector<RoboRenderer::CompiledElement>& Elements() const { return elements; }
        const std::vector<std::string>& Strings() const { return strings; }
        const std::vector<RoboRenderer::CompiledResource>& Resources() const { return resources; }
        const std::vector<RoboRenderer::CompiledBinding>& Bindings() const { return bindings; }
        const std::vector<RoboRenderer::CompiledIdEntry>& Identifiers() const { return identifiers; }
        const std::vector<RoboRenderer::CompiledStyle>& Styles() const { return styles; }

        /// Compiler element identity for each compiled element index, so tools
        /// can map a compiled element back to the IR. Empty in release output.
        const std::vector<ElementId>& SourceElementIds() const { return sourceElementIds; }

    private:
        friend class CompiledUiBuilder;

        /// Rebuilds the pointer table after every string has been interned.
        void Finalize();

        std::string applicationName;
        std::vector<std::string> strings;
        std::vector<const char*> stringTable;
        std::vector<RoboRenderer::CompiledElement> elements;
        std::vector<RoboRenderer::CompiledResource> resources;
        std::vector<RoboRenderer::CompiledBinding> bindings;
        std::vector<RoboRenderer::CompiledIdEntry> identifiers;
        std::vector<RoboRenderer::CompiledStyle> styles;
        std::vector<ElementId> sourceElementIds;
        RoboRenderer::CompiledDocument view{};
    };

    /// Lowers a validated native model into the flat compiled representation.
    class CompiledUiBuilder
    {
    public:
        explicit CompiledUiBuilder(DiagnosticSink& diagnostics);

        /// Returns false and reports a compiler diagnostic when the model cannot
        /// be represented (for example an element with no root). Compiler
        /// diagnostics stay compiler diagnostics: a lowering problem is a
        /// compiler problem, not a runtime one.
        /// `authoredStyles` is the CSS the document resolved to. It is optional
        /// so a caller that only wants the structure does not have to run the
        /// cascade; when it is absent every element keeps the native default for
        /// its category.
        bool Build(const NativeUiModel& model,
                   const std::string& applicationName,
                   CompiledUiStorage& storage,
                   const CssStyleTable* authoredStyles = nullptr);

        /// The compiler version recorded in the compiled data.
        static const char* CompilerVersion();

    private:
        DiagnosticSink& diagnostics;
    };
}
