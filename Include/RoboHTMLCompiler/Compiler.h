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
        ValidationOptions validation;
        bool generateWebTarget = true;
        bool generateNativeTarget = true;
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
        std::size_t eventBindingCount = 0;
        WebTargetResult webTarget;
        NativeTargetResult nativeTarget;
        std::filesystem::path webTargetDirectory;
        std::filesystem::path nativeTargetDirectory;
        std::filesystem::path irDirectory;
        std::filesystem::path reportDirectory;
    };

    class Compiler
    {
    public:
        explicit Compiler(DiagnosticSink& diagnostics);

        /// Discovers the application sources referenced by an entry document.
        bool DiscoverSources(const std::filesystem::path& sourceDirectory,
                             const std::string& applicationName,
                             ApplicationSource& application);

        /// Runs the full pipeline.
        CompilationResult Compile(const CompilationOptions& options);

    private:
        bool LoadStyleSheets(const ApplicationSource& application,
                             const CompilationOptions& options,
                             StyleSheetCompiler& styleSheetCompiler);

        bool LoadBehaviourModules(const ApplicationSource& application,
                                  JavaScriptModuleCompiler& behaviourModule);

        void WriteCompilationReport(const CompilationOptions& options,
                                    const CompilationResult& result,
                                    const MarkupDocument& document,
                                    const NativeUiModel& model) const;

        DiagnosticSink& diagnostics;
        const Registry& registry;
    };
}
