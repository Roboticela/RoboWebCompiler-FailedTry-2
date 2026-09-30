#include "RoboHTMLCompiler/Compiler.h"

#include "RoboHTMLCompiler/NativeTreeBuilder.h"
#include "RoboHTMLCompiler/Parsers.h"
#include "CodeGeneration/JsonEmit.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace RoboHTMLCompiler
{
    namespace
    {
        std::string ReadWholeFile(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        void WriteFile(const std::filesystem::path& path, const std::string& contents)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream << contents;
        }

        /// "task-list.html" -> "TaskList": a valid C++ identifier that names the
        /// generated native model of a document.
        std::string ApplicationNameFromDocument(const std::filesystem::path& path)
        {
            std::string name;
            bool capitalize = true;

            for (const char character : path.stem().string())
            {
                const unsigned char value = static_cast<unsigned char>(character);
                if (std::isalnum(value) == 0)
                {
                    capitalize = true;
                    continue;
                }

                name.push_back(capitalize ? static_cast<char>(std::toupper(value))
                                          : static_cast<char>(std::tolower(value)));
                capitalize = false;
            }

            if (name.empty())
            {
                return "Document";
            }
            if (std::isalpha(static_cast<unsigned char>(name[0])) == 0)
            {
                name.insert(name.begin(), 'P');
            }
            return name;
        }

        /// True when a script file is the resource a document referenced.
        bool IsScriptReference(const std::filesystem::path& path, const std::set<std::string>& referenced)
        {
            if (referenced.empty())
            {
                return false;
            }

            std::string target = path.filename().generic_string();
            const std::size_t slash = target.find_last_of('/');
            if (slash != std::string::npos)
            {
                target = target.substr(slash + 1);
            }

            for (const std::string& reference : referenced)
            {
                std::string candidate = reference;
                while (candidate.compare(0, 2, "./") == 0)
                {
                    candidate.erase(0, 2);
                }
                const std::size_t referenceSlash = candidate.find_last_of('/');
                const std::string tail = referenceSlash == std::string::npos
                                             ? candidate
                                             : candidate.substr(referenceSlash + 1);
                if (candidate == target || tail == target)
                {
                    return true;
                }
            }

            return false;
        }
    }

    Compiler::Compiler(DiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
        , registry(Registry::Instance())
    {
    }

    bool Compiler::DiscoverSources(const std::filesystem::path& sourceDirectory,
                                   const std::string& applicationName,
                                   ApplicationSource& application)
    {
        application = ApplicationSource{};
        application.directory = sourceDirectory;

        if (!std::filesystem::exists(sourceDirectory))
        {
            diagnostics.Error(DiagnosticCode::MissingSourceFile,
                              SourceLocation{ sourceDirectory.generic_string(), 1, 1, 0 },
                              "The compiler was asked to read the application folder and it is not there.",
                              "The path does not point at a folder the compiler can read.",
                              "Give the folder that holds the .html, .css and .js files.",
                              sourceDirectory.generic_string());
            return false;
        }

        const std::vector<std::string> documentCandidates = {
            applicationName + ".html",
            "index.html"
        };

        for (const std::string& candidate : documentCandidates)
        {
            const std::filesystem::path path = sourceDirectory / candidate;
            if (std::filesystem::exists(path))
            {
                application.documentPath = path;
                break;
            }
        }

        if (application.documentPath.empty())
        {
            for (const std::filesystem::directory_entry& entry :
                 std::filesystem::directory_iterator(sourceDirectory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".html")
                {
                    application.documentPath = entry.path();
                    break;
                }
            }
        }

        if (application.documentPath.empty())
        {
            diagnostics.Error(DiagnosticCode::MissingSourceFile,
                              SourceLocation{ sourceDirectory.generic_string(), 1, 1, 0 },
                              "This folder has no .html file in it, so there is nothing to compile.",
                              "The compiler looks for index.html, then <ApplicationName>.html, then any .html "
                              "file, and found none.",
                              "Add index.html, or name the file after the application.",
                              sourceDirectory.generic_string());
            return false;
        }

        return CollectCompanionSources(application);
    }

    bool Compiler::CollectCompanionSources(ApplicationSource& application)
    {
        application.styleSheetPaths.clear();
        application.scriptPaths.clear();

        std::error_code errorCode;
        if (!std::filesystem::is_directory(application.directory, errorCode))
        {
            return true;
        }

        std::vector<std::filesystem::path> entries;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(application.directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            entries.push_back(entry.path());
        }

        std::sort(entries.begin(), entries.end());

        for (const std::filesystem::path& path : entries)
        {
            if (path == application.documentPath)
            {
                continue;
            }

            if (path.extension() == ".css")
            {
                application.styleSheetPaths.push_back(path);
            }
            else if (path.extension() == ".js")
            {
                application.scriptPaths.push_back(path);
            }
        }

        return true;
    }

    bool Compiler::LoadStyleSheets(const ApplicationSource& application,
                                   const CompilationOptions& options,
                                   StyleSheetCompiler& styleSheetCompiler)
    {
        (void)options;

        for (const std::filesystem::path& path : application.styleSheetPaths)
        {
            styleSheetCompiler.AddStyleSheet(path.filename().generic_string(), ReadWholeFile(path));
        }

        return true;
    }

    bool Compiler::LoadBehaviourModules(const ApplicationSource& application,
                                        const CompilationOptions& options,
                                        const std::set<std::string>& referencedScripts,
                                        JavaScriptModuleCompiler& behaviourModule)
    {
        for (const std::filesystem::path& path : application.scriptPaths)
        {
            if (options.referencedScriptsOnly && !IsScriptReference(path, referencedScripts))
            {
                continue;
            }

            behaviourModule.AddModule(path.filename().generic_string(), ReadWholeFile(path));
        }

        return true;
    }

    CompilationResult Compiler::Compile(const CompilationOptions& options)
    {
        CompilationResult result;
        result.outputRoot = options.outputDirectory.generic_string();

        result.webTargetDirectory = options.outputDirectory / "GeneratedHTML";
        result.nativeTargetDirectory = options.outputDirectory / "GeneratedNative";
        result.runtimeTargetDirectory = options.outputDirectory / "GeneratedRuntime";
        result.irDirectory = options.outputDirectory / "GeneratedIR";
        result.reportDirectory = options.outputDirectory / "GeneratedReports";

        ApplicationSource application;
        if (!options.documentPath.empty())
        {
            application.directory = options.sourceDirectory;
            application.documentPath = options.documentPath;
            CollectCompanionSources(application);
        }
        else if (!DiscoverSources(options.sourceDirectory, options.applicationName, application))
        {
            return result;
        }

        std::cout << "[RoboHTMLCompiler] compiling " << application.documentPath.generic_string() << "\n";

        // --- Stage 1: parse --------------------------------------------------
        const std::string documentSource = ReadWholeFile(application.documentPath);
        HtmlParser parser(diagnostics, registry, application.documentPath.generic_string());
        std::unique_ptr<MarkupDocument> document = parser.Parse(documentSource);

        if (document == nullptr)
        {
            return result;
        }

        if (diagnostics.HasErrors())
        {
            std::cerr << "[RoboHTMLCompiler] parse failed\n";
            return result;
        }

        // --- Stage 2: validate -----------------------------------------------
        HtmlValidator validator(diagnostics, registry, options.validation);
        validator.Validate(*document);

        if (diagnostics.HasErrors())
        {
            std::cerr << "[RoboHTMLCompiler] validation failed\n";
            return result;
        }

        // --- Stage 3: style references ---------------------------------------
        StyleSheetCompiler styleSheetCompiler(diagnostics);
        LoadStyleSheets(application, options, styleSheetCompiler);
        styleSheetCompiler.ValidateStyleReferences(*document);

        if (diagnostics.HasErrors())
        {
            std::cerr << "[RoboHTMLCompiler] style reference resolution failed\n";
            return result;
        }

        // --- Stage 4: behaviour ----------------------------------------------
        JavaScriptModuleCompiler behaviourModule(diagnostics);

        document->ForEachElement([&](const ElementNode& element) {
            if (element.ns != MarkupNamespace::Html || element.sourceTag != "script")
            {
                return;
            }

            const AttributeNode* source = element.FindAttribute("src");
            if (source == nullptr)
            {
                return;
            }

            ScriptResource resource;
            resource.reference = source->value;
            resource.isModule = EqualsIgnoreCase(element.GetAttribute("type"), "module");
            resource.isDeferred = element.HasAttribute("defer");
            resource.isAsynchronous = element.HasAttribute("async");
            resource.noModule = element.HasAttribute("nomodule");
            resource.location = element.location;
            behaviourModule.AddScriptResource(resource);

            const std::filesystem::path resolved = application.directory / source->value;
            if (!std::filesystem::exists(resolved))
            {
                diagnostics.Error(DiagnosticCode::UnresolvedScriptResource,
                                  source->location,
                                  "The referenced JavaScript resource '" + source->value + "' was not found.",
                                  std::string(),
                                  "Add the file to the application directory, or correct the src attribute.",
                                  source->value);
            }
        });

        std::set<std::string> referencedScripts;
        for (const ScriptResource& resource : behaviourModule.GetScriptResources())
        {
            referencedScripts.insert(resource.reference);
        }

        LoadBehaviourModules(application, options, referencedScripts, behaviourModule);
        behaviourModule.LinkAgainstDocument(*document);

        if (diagnostics.HasErrors())
        {
            std::cerr << "[RoboHTMLCompiler] behaviour linking failed\n";
            return result;
        }

        // --- Stage 5: normalise into the native UI IR ------------------------
        NativeUiModel model;
        model.applicationName = options.applicationName;

        StyleReferenceResolver styleReferenceResolver(styleSheetCompiler, diagnostics);
        NativeTreeBuilder treeBuilder(diagnostics, registry, styleReferenceResolver);

        ElementId nextElementId = 1;
        model.root = treeBuilder.BuildDocument(*document, model, nextElementId);

        if (model.root == nullptr)
        {
            diagnostics.Error(DiagnosticCode::MissingDocumentElement,
                              SourceLocation{ application.documentPath.generic_string(), 1, 1, 0 },
                              "No element could be lowered into the native model.",
                              std::string(),
                              "Provide a document with an <html> element.");
            return result;
        }

        for (const EventBinding& binding : behaviourModule.GetEventBindings())
        {
            model.eventBindings.push_back(binding);
        }
        for (const ScriptResource& resource : behaviourModule.GetScriptResources())
        {
            model.scriptResources.push_back(resource.reference);
        }

        // --- Stage 5b: authored CSS -> native styles --------------------------
        //
        // The cascade runs here, once, over the document the compiler has already
        // built. Everything a browser does per frame - matching, specificity, the
        // cascade, inheritance, shorthand expansion - has happened by the time
        // this returns, and what comes out is the native value of every property
        // the renderer consumes.
        CssLengthContext cssContext;
        cssContext.viewportWidth = options.viewportWidth > 0.0f ? options.viewportWidth : 1024.0f;
        cssContext.viewportHeight = options.viewportHeight > 0.0f ? options.viewportHeight : 768.0f;

        CssStyleTable styleTable;
        CssStyleBuilder styleBuilder(diagnostics);

        if (!styleBuilder.Build(*document, styleSheetCompiler.GetSources(), cssContext, styleTable))
        {
            std::cerr << "[RoboHTMLCompiler] CSS compilation failed\n";
            return result;
        }

        result.elementCount = document->CollectElements().size();
        result.nativeElementCount = model.CountElements();
        result.styleCount = model.styleTable.size();
        result.computedStyleCount = styleTable.computedStyleCount;
        result.nativeStyleCount = styleTable.styles.empty() ? 0 : styleTable.styles.size() - 1;
        result.styledElementCount = styleTable.styledElementCount;
        result.eventBindingCount = model.eventBindings.size();

        // --- Stage 6: code generation ----------------------------------------
        if (options.generateWebTarget)
        {
            WebTargetGenerator webTarget(diagnostics, registry);
            if (!webTarget.Generate(*document, styleSheetCompiler, behaviourModule, result.webTargetDirectory,
                                    "styles.css", "scripts.js", result.webTarget))
            {
                std::cerr << "[RoboHTMLCompiler] web target generation failed\n";
                return result;
            }
        }

        if (options.generateNativeTarget)
        {
            NativeTargetGenerator nativeTarget(diagnostics, registry);
            if (!nativeTarget.Generate(model, options.applicationName, result.nativeTargetDirectory,
                                       result.nativeTarget))
            {
                std::cerr << "[RoboHTMLCompiler] native target generation failed\n";
                return result;
            }
        }

        if (options.generateRuntimeTarget)
        {
            RuntimeTargetGenerator runtimeTarget(diagnostics, registry);
            if (!runtimeTarget.Generate(model, options.applicationName, result.runtimeTargetDirectory,
                                       result.runtimeTarget, &styleTable))
            {
                std::cerr << "[RoboHTMLCompiler] runtime target generation failed\n";
                return result;
            }

            result.runtimeElementCount = result.runtimeTarget.elementCount;
            result.runtimeResourceCount = result.runtimeTarget.resourceCount;
        }

        if (options.emitIrDump)
        {
            const std::filesystem::path irPath = result.irDirectory / (options.applicationName + ".ir.json");
            WriteFile(irPath, NativeTargetGenerator::RenderIrDump(model, options.applicationName));
            result.nativeTarget.irDumpPath = irPath;
        }

        if (options.emitCompilationReport)
        {
            WriteCompilationReport(options, result, *document, model);
        }

        if (options.emitCoverageSummary)
        {
            std::vector<std::string> lines;
            lines.push_back("{");
            lines.push_back("  \"project\": \"RoboHTMLCompiler\",");
            lines.push_back("  \"view\": \"compiled-in registry\",");
            lines.push_back("  \"elementDefinitions\": " +
                            std::to_string(registry.GetElementCount()) + ",");
            lines.push_back("  \"attributeRecords\": " +
                            std::to_string(registry.GetAttributeCount()) + ",");
            lines.push_back("  \"nativeElementKinds\": " + std::to_string(0) + ",");
            lines.push_back("  \"note\": \"Full reference coverage status is tracked in "
                            "CompilerKnowledge/ImplementationCoverage.json and regenerated by the "
                            "RoboHTMLCompilerTools targets.\"");
            lines.push_back("}");
            WriteFile(result.reportDirectory / "ImplementationCoverage.json",
                      [&lines] {
                          std::string joined;
                          for (const std::string& line : lines)
                          {
                              joined += line;
                              joined += "\n";
                          }
                          return joined;
                      }());
        }

        result.succeeded = true;
        return result;
    }

    BatchCompilationResult Compiler::CompileBatch(const CompilationOptions& options)
    {
        BatchCompilationResult batch;
        batch.sourceDirectory = options.sourceDirectory;
        batch.outputDirectory = options.outputDirectory;

        std::vector<std::filesystem::path> documents;
        std::error_code errorCode;
        if (!std::filesystem::is_directory(options.sourceDirectory, errorCode))
        {
            // A file here is worth naming as a file: "the directory does not
            // exist" about a path the reader can see on disk sends them looking
            // for the wrong mistake.
            const bool isFile = std::filesystem::is_regular_file(options.sourceDirectory, errorCode);

            diagnostics.Error(
                DiagnosticCode::MissingSourceFile,
                SourceLocation{ options.sourceDirectory.generic_string(), 1, 1, 0 },
                isFile ? "This is a file, not a folder, and batch compilation reads a folder."
                       : "The compiler was asked to read this folder and it is not there.",
                isFile ? "The compiler compiles every .html file in a folder when it is asked for a batch, so it "
                         "needs the folder rather than one of its files."
                       : "The path does not point at anything the compiler can read.",
                isFile ? "Give the folder that contains the document, or drop --all to compile one file."
                       : "Check the path for typing mistakes, and make sure the folder exists.",
                options.sourceDirectory.generic_string());
            return batch;
        }

        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(options.sourceDirectory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::string extension = entry.path().extension().string();
            if (extension == ".html" || extension == ".htm")
            {
                documents.push_back(entry.path());
            }
        }
        std::sort(documents.begin(), documents.end());

        if (documents.empty())
        {
            diagnostics.Error(DiagnosticCode::MissingSourceFile,
                              SourceLocation{ options.sourceDirectory.generic_string(), 1, 1, 0 },
                              "This folder has no .html file in it, so there is nothing to compile.",
                              "The compiler reads the .html files in the folder it is given, and this one holds "
                              "none.",
                              "Put the page you want to compile in this folder.",
                              options.sourceDirectory.generic_string());
            return batch;
        }

        std::cout << "[RoboHTMLCompiler] batch compiling " << documents.size() << " document(s) from "
                  << options.sourceDirectory.generic_string() << "\n";

        for (const std::filesystem::path& documentPath : documents)
        {
            BatchDocument document;
            document.documentPath = documentPath;
            document.documentName = documentPath.filename().generic_string();
            document.applicationName = ApplicationNameFromDocument(documentPath);
            document.outputDirectory = options.outputDirectory / document.applicationName;

            CompilationOptions documentOptions = options;
            documentOptions.documentPath = documentPath;
            documentOptions.applicationName = document.applicationName;
            documentOptions.outputDirectory = document.outputDirectory;
            documentOptions.referencedScriptsOnly = true;

            // Each document is compiled with its own diagnostics so one failure
            // can never be attributed to another document or stop the batch.
            DiagnosticSink documentSink;
            Compiler documentCompiler(documentSink);
            document.result = documentCompiler.Compile(documentOptions);

            document.succeeded = document.result.succeeded && !documentSink.HasErrors();
            document.errorCount = documentSink.ErrorCount();
            document.warningCount = documentSink.WarningCount();
            document.diagnostics = documentSink.FormatErrors();
            document.warnings = document.warningCount == 0 ? std::string() : documentSink.FormatWarnings();
            document.sourceElementCount = document.result.elementCount;
            document.nativeElementCount = document.result.nativeElementCount;
            document.styleCount = document.result.styleCount;
            document.eventBindingCount = document.result.eventBindingCount;

            std::cout << "  " << (document.succeeded ? "compiled " : "failed   ") << document.documentName
                      << "  ->  " << document.applicationName << "  (" << document.sourceElementCount
                      << " source elements, " << document.nativeElementCount << " native elements)"
                      << "\n";

            if (document.succeeded)
            {
                ++batch.compiledCount;
            }
            else
            {
                ++batch.failedCount;
                std::cerr << document.diagnostics << "\n";
            }

            batch.documents.push_back(std::move(document));
        }

        batch.reportPath = options.outputDirectory / "BatchReport.json";
        WriteBatchReport(batch);
        return batch;
    }

    void Compiler::WriteBatchReport(const BatchCompilationResult& batch) const
    {
        using namespace CodeGeneration;

        std::ostringstream report;
        report << "{\n";
        report << "  \"project\": \"RoboHTMLCompiler\",\n";
        report << "  \"mode\": \"batch\",\n";
        report << "  \"input\": " << Quote(batch.sourceDirectory.generic_string()) << ",\n";
        report << "  \"output\": " << Quote(batch.outputDirectory.generic_string()) << ",\n";
        report << "  \"compiled\": " << batch.compiledCount << ",\n";
        report << "  \"failed\": " << batch.failedCount << ",\n";
        report << "  \"documents\": [\n";

        for (std::size_t index = 0; index < batch.documents.size(); ++index)
        {
            const BatchDocument& document = batch.documents[index];

            std::vector<std::string> outputs;
            if (!document.result.webTarget.documentPath.empty())
            {
                outputs.push_back(Quote(document.result.webTarget.documentPath.generic_string()));
            }
            if (!document.result.nativeTarget.headerPath.empty())
            {
                outputs.push_back(Quote(document.result.nativeTarget.headerPath.generic_string()));
            }
            if (!document.result.nativeTarget.sourcePath.empty())
            {
                outputs.push_back(Quote(document.result.nativeTarget.sourcePath.generic_string()));
            }
            if (!document.result.nativeTarget.irDumpPath.empty())
            {
                outputs.push_back(Quote(document.result.nativeTarget.irDumpPath.generic_string()));
            }
            if (!document.result.runtimeTarget.headerPath.empty())
            {
                outputs.push_back(Quote(document.result.runtimeTarget.headerPath.generic_string()));
            }
            if (!document.result.runtimeTarget.sourcePath.empty())
            {
                outputs.push_back(Quote(document.result.runtimeTarget.sourcePath.generic_string()));
            }

            report << "    {\n";
            report << "      \"document\": " << Quote(document.documentName) << ",\n";
            report << "      \"application\": " << Quote(document.applicationName) << ",\n";
            report << "      \"source\": " << Quote(document.documentPath.generic_string()) << ",\n";
            report << "      \"output\": " << Quote(document.outputDirectory.generic_string()) << ",\n";
            report << "      \"succeeded\": " << Quote(document.succeeded) << ",\n";
            report << "      \"errors\": " << document.errorCount << ",\n";
            report << "      \"counts\": { \"sourceElements\": " << document.sourceElementCount
                   << ", \"nativeElements\": " << document.nativeElementCount
                   << ", \"internedStyles\": " << document.styleCount
                   << ", \"eventBindings\": " << document.eventBindingCount << " },\n";
            report << "      \"outputs\": " << JsonArray(outputs) << ",\n";
            report << "      \"diagnostics\": " << Quote(document.diagnostics) << "\n";
            report << "    }" << (index + 1 == batch.documents.size() ? "" : ",") << "\n";
        }

        report << "  ]\n";
        report << "}\n";

        WriteFile(batch.reportPath, report.str());
    }

    void Compiler::WriteCompilationReport(const CompilationOptions& options,
                                          const CompilationResult& result,
                                          const MarkupDocument& document,
                                          const NativeUiModel& model) const
    {
        using namespace CodeGeneration;

        std::ostringstream report;
        report << "{\n";
        report << "  \"project\": \"RoboHTMLCompiler\",\n";
        report << "  \"application\": " << Quote(options.applicationName) << ",\n";
        report << "  \"source\": " << Quote(document.sourceFile) << ",\n";
        report << "  \"succeeded\": " << Quote(true) << ",\n";
        report << "  \"counts\": {\n";
        report << "    \"sourceElements\": " << document.CollectElements().size() << ",\n";
        report << "    \"nativeElements\": " << result.nativeElementCount << ",\n";
        report << "    \"internedStyles\": " << result.styleCount << ",\n";
        report << "    \"computedStyles\": " << result.computedStyleCount << ",\n";
        report << "    \"nativeStyles\": " << result.nativeStyleCount << ",\n";
        report << "    \"styledElements\": " << result.styledElementCount << ",\n";
        report << "    \"eventBindings\": " << result.eventBindingCount << ",\n";
        report << "    \"runtimeElements\": " << result.runtimeElementCount << ",\n";
        report << "    \"runtimeResources\": " << result.runtimeResourceCount << ",\n";
        report << "    \"scriptResources\": " << model.scriptResources.size() << "\n";
        report << "  },\n";

        std::vector<std::string> outputs;
        if (!result.webTarget.documentPath.empty())
        {
            outputs.push_back(Quote(result.webTarget.documentPath.generic_string()));
        }
        if (!result.webTarget.styleSheetPath.empty())
        {
            outputs.push_back(Quote(result.webTarget.styleSheetPath.generic_string()));
        }
        if (!result.nativeTarget.headerPath.empty())
        {
            outputs.push_back(Quote(result.nativeTarget.headerPath.generic_string()));
        }
        if (!result.nativeTarget.sourcePath.empty())
        {
            outputs.push_back(Quote(result.nativeTarget.sourcePath.generic_string()));
        }
        if (!result.nativeTarget.irDumpPath.empty())
        {
            outputs.push_back(Quote(result.nativeTarget.irDumpPath.generic_string()));
        }
        if (!result.runtimeTarget.headerPath.empty())
        {
            outputs.push_back(Quote(result.runtimeTarget.headerPath.generic_string()));
        }
        if (!result.runtimeTarget.sourcePath.empty())
        {
            outputs.push_back(Quote(result.runtimeTarget.sourcePath.generic_string()));
        }
        report << "  \"outputs\": " << JsonArray(outputs) << ",\n";

        report << "  \"invariants\": {\n";
        report << "    \"htmlContainsCssSource\": " << Quote(false) << ",\n";
        report << "    \"htmlContainsJavaScriptSource\": " << Quote(false) << ",\n";
        report << "    \"runtimeCssParsingRequired\": " << Quote(false) << ",\n";
        report << "    \"runtimeHtmlParsingRequired\": " << Quote(false) << ",\n";
        report << "    \"nativeRepresentationStronglyTyped\": " << Quote(true) << ",\n";
        report << "    \"runtimeRepresentationIsFlatIndexedData\": " << Quote(true) << "\n";
        report << "  }\n";
        report << "}\n";

        WriteFile(result.reportDirectory / "CompilationReport.json", report.str());
        WriteFile(result.reportDirectory / "Diagnostics.json",
                  std::string("{\n  \"diagnostics\": []\n}\n"));
    }
}
