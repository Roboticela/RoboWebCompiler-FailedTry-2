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
    struct QueryBinding
    {
        std::string targetId;
        std::string sourceFile;
        int line = 0;
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
        const std::vector<std::string>& GetReferencedElementIds() const { return referencedElementIds; }

    private:
        void ScanModule(const std::string& fileName, const std::string& source);

        DiagnosticSink& diagnostics;
        std::vector<ScriptResource> scriptResources;
        std::vector<EventBinding> eventBindings;
        std::vector<std::string> referencedElementIds;
    };
}
