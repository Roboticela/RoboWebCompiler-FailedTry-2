// ---------------------------------------------------------------------------
// RoboHTMLCompiler - compiler driver.
//
//     HTML ────────────────┐
//                          │
//     CSS / Tailwind ──────┼──> RoboHTMLCompiler -> Native IR -> C++ / LLVM
//                          │
//     JavaScript ──────────┘
//
//     Source
//        |
//        v
//     Parse -> Validate -> Error? -> yes: report and stop
//        |                    no
//        v
//     Normalize -> Native UI IR -> Optimize -> Web target / Native target
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/CodeGenerator.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"
#include "RoboHTMLCompiler/JavaScriptModule.h"
#include "RoboHTMLCompiler/Registry.h"
#include "RoboHTMLCompiler/StyleResolver.h"
#include "RoboHTMLCompiler/Validation.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    /// The application sources discovered on disk.
    struct ApplicationSource
    {
        std::filesystem::path directory;
        std::filesystem::path documentPath;
        std::vector<std::filesystem::path> styleSheetPaths;
        std::vector<std::filesystem::path> scriptPaths;
    };

    struct CompilationOptions
    {
        std::filesystem::path sourceDirectory;
        std::filesystem::path outputDirectory;
        std::string applicationName;

        /// Explicit entry document. When empty the document is discovered from
        /// sourceDirectory (applicationName.html, then index.html, then any
        /// *.html). Batch compilation sets it once per document.
        std::filesystem::path documentPath;

        /// Load only the behaviour modules the document actually references.
        /// A directory of documents otherwise shares every *.js file with every
        /// document, so a script belonging to one page would be linked against
        /// all the others.
        bool referencedScriptsOnly = false;

        /// The viewport the application targets. CSS media queries are evaluated
        /// against it during compilation, and viewport units the runtime resolves
        /// are stored as fractions of it. It is a build input, not a runtime
        /// setting, because a media query cannot be evaluated twice.
        float viewportWidth = 1024.0f;
        float viewportHeight = 768.0f;

        ValidationOptions validation;
        bool generateWebTarget = true;
        bool generateNativeTarget = true;
        /// Emit the flat compiled UI that RoboRenderer consumes.
        bool generateRuntimeTarget = true;
        bool emitIrDump = true;
        bool emitCompilationReport = true;
        bool emitCoverageSummary = true;
    };

    struct CompilationResult
    {
        bool succeeded = false;
        std::string outputRoot;
        std::size_t elementCount = 0;
        std::size_t nativeElementCount = 0;
        std::size_t styleCount = 0;
        /// Computed styles the CSS compiler interned, the native style entries
        /// they lowered into, and the elements that carry one.
        std::size_t computedStyleCount = 0;
        std::size_t nativeStyleCount = 0;
        std::size_t styledElementCount = 0;
        std::size_t eventBindingCount = 0;
        WebTargetResult webTarget;
        NativeTargetResult nativeTarget;
        RuntimeTargetResult runtimeTarget;
        std::size_t runtimeElementCount = 0;
        std::size_t runtimeResourceCount = 0;
        std::filesystem::path webTargetDirectory;
        std::filesystem::path nativeTargetDirectory;
        std::filesystem::path runtimeTargetDirectory;
        std::filesystem::path irDirectory;
        std::filesystem::path reportDirectory;
    };

    /// One document of a batch compilation.
    struct BatchDocument
    {
        std::string documentName;                  // "task-list.html"
        std::string applicationName;               // "TaskList"
        std::filesystem::path documentPath;
        std::filesystem::path outputDirectory;

        bool succeeded = false;
        std::size_t errorCount = 0;
        std::size_t warningCount = 0;
        std::size_t sourceElementCount = 0;
        std::size_t nativeElementCount = 0;
        std::size_t styleCount = 0;
        std::size_t eventBindingCount = 0;

        /// Formatted errors, when the document failed.
        std::string diagnostics;

        /// Formatted warnings, when the document produced any. A warning is
        /// something the compiler read and did not apply, so it is reported even
        /// though the document compiled.
        std::string warnings;

        CompilationResult result;
    };

    struct BatchCompilationResult
    {
        std::filesystem::path sourceDirectory;
        std::filesystem::path outputDirectory;
        std::vector<BatchDocument> documents;
        std::size_t compiledCount = 0;
        std::size_t failedCount = 0;
        std::filesystem::path reportPath;

        bool Succeeded() const { return !documents.empty() && failedCount == 0; }
    };

    class Compiler
    {
    public:
        explicit Compiler(DiagnosticSink& diagnostics);

        /// Resolves the application sources referenced by an entry document.
        bool DiscoverSources(const std::filesystem::path& sourceDirectory,
                             const std::string& applicationName,
                             ApplicationSource& application);

        /// Runs the full pipeline.
        CompilationResult Compile(const CompilationOptions& options);

        /// Compiles every HTML document in the source directory, each into its
        /// own output subdirectory named after the document. One document that
        /// fails does not stop the others; the batch report records all of it.
        BatchCompilationResult CompileBatch(const CompilationOptions& options);

    private:
        /// Resolves the stylesheets and scripts that sit next to a document.
        bool CollectCompanionSources(ApplicationSource& application);

        bool LoadStyleSheets(const ApplicationSource& application,
                             const CompilationOptions& options,
                             StyleSheetCompiler& styleSheetCompiler);

        bool LoadBehaviourModules(const ApplicationSource& application,
                                  const CompilationOptions& options,
                                  const std::set<std::string>& referencedScripts,
                                  JavaScriptModuleCompiler& behaviourModule);

        void WriteCompilationReport(const CompilationOptions& options,
                                    const CompilationResult& result,
                                    const MarkupDocument& document,
                                    const NativeUiModel& model) const;

        void WriteBatchReport(const BatchCompilationResult& batch) const;

        DiagnosticSink& diagnostics;
        const Registry& registry;
    };
}
