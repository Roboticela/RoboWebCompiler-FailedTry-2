// ---------------------------------------------------------------------------
// RoboHTMLCompiler - JavaScript module.
//
// Behaviour is a separate language and a separate frontend. HTML must contain
// no JavaScript source: element.onclick = ... is rejected outright, and
// <script> may only reference an external resource.
//
// This module compiles the behaviour source into a Logic IR that links the
// document to event bindings, so the native target can generate typed
// interaction tables instead of shipping a string-based DOM.
//
//     const save = document.getElementById("save");
//     save.addEventListener("click", onSave);
//                     |
//                     v
//     EventBinding { target = "save", event = "click", handler = "onSave" }
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"

#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    /// A resource reference requested by the markup, e.g. <script src="app.js">.
    struct ScriptResource
    {
        std::string reference;
        bool isModule = false;
        bool isDeferred = false;
        bool isAsynchronous = false;
        bool noModule = false;
        SourceLocation location;
    };

    /// A DOM query discovered in the behaviour source.
    ///
    /// The position is kept because this is the one thing in the pipeline whose
    /// failure is almost always fixed by looking at one line: the script asks
    /// for an element id, and the answer is either to write that id in the
    /// markup or to correct it here. "Line 1, column 1 of" the document tells
    /// the reader nothing; the line of the query tells them everything.
    struct QueryBinding
    {
        std::string targetId;
        std::string sourceFile;
        int line = 0;
        /// One-based character number in that line, at the first character of
        /// the id.
        int column = 0;
        /// The source line itself, so the report can show the code at fault.
        std::string sourceLine;
    };

    class JavaScriptModuleCompiler
    {
    public:
        explicit JavaScriptModuleCompiler(DiagnosticSink& diagnostics);

        /// Registers a script resource requested by the markup. The referenced
        /// file is loaded by the driver when it exists on disk.
        void AddScriptResource(const ScriptResource& resource);

        /// Compiles one behaviour module: records the element identities it
        /// targets and the events it binds.
        void AddModule(const std::string& fileName, const std::string& source);

        /// Verifies that every recorded binding targets an element that exists
        /// in the document. Reports MJS001 for dangling references.
        bool LinkAgainstDocument(const MarkupDocument& document);

        const std::vector<ScriptResource>& GetScriptResources() const { return scriptResources; }
        const std::vector<EventBinding>& GetEventBindings() const { return eventBindings; }
        /// Every element identity the behaviour modules look up, in the order
        /// they were discovered, with the position that asked for it.
        const std::vector<QueryBinding>& GetElementQueries() const { return elementQueries; }

    private:
        void ScanModule(const std::string& fileName, const std::string& source);

        /// Records one query, keeping the first position an id was asked for at.
        void RecordQuery(const std::string& targetId,
                         const std::string& fileName,
                         const std::string& source,
                         std::size_t offset);

        DiagnosticSink& diagnostics;
        std::vector<ScriptResource> scriptResources;
        std::vector<EventBinding> eventBindings;
        std::vector<QueryBinding> elementQueries;
    };
}
