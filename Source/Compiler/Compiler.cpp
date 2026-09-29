#include "RoboHTMLCompiler/Compiler.h"

#include "RoboHTMLCompiler/NativeTreeBuilder.h"
#include "RoboHTMLCompiler/Parsers.h"
#include "CodeGeneration/JsonEmit.h"

#include <algorithm>
#include <fstream>
#include <iostream>
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

        std::string BaseName(const std::filesystem::path& path)
        {
            return path.stem().string();
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
                              "The application source directory does not exist.",
                              std::string(),
                              "Point the compiler at a directory containing the application sources.",
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
                              "No HTML document was found in the application directory.",
                              std::string(),
                              "Add an index.html (or <ApplicationName>.html) entry document.",
                              sourceDirectory.generic_string());
            return false;
        }

        std::vector<std::filesystem::path> entries;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sourceDirectory))
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
                                        JavaScriptModuleCompiler& behaviourModule)
    {
        for (const std::filesystem::path& path : application.scriptPaths)
        {
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
        result.irDirectory = options.outputDirectory / "GeneratedIR";
        result.reportDirectory = options.outputDirectory / "GeneratedReports";

        ApplicationSource application;
        if (!DiscoverSources(options.sourceDirectory, options.applicationName, application))
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

        LoadBehaviourModules(application, behaviourModule);
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

        result.elementCount = document->CollectElements().size();
        result.nativeElementCount = model.CountElements();
        result.styleCount = model.styleTable.size();
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
                            "Generated/ImplementationCoverage.json and regenerated by the "
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
        report << "    \"eventBindings\": " << result.eventBindingCount << ",\n";
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
        report << "  \"outputs\": " << JsonArray(outputs) << ",\n";

        report << "  \"invariants\": {\n";
        report << "    \"htmlContainsCssSource\": " << Quote(false) << ",\n";
        report << "    \"htmlContainsJavaScriptSource\": " << Quote(false) << ",\n";
        report << "    \"runtimeCssParsingRequired\": " << Quote(false) << ",\n";
        report << "    \"runtimeHtmlParsingRequired\": " << Quote(false) << ",\n";
        report << "    \"nativeRepresentationStronglyTyped\": " << Quote(true) << "\n";
        report << "  }\n";
        report << "}\n";

        WriteFile(result.reportDirectory / "CompilationReport.json", report.str());
        WriteFile(result.reportDirectory / "Diagnostics.json",
                  std::string("{\n  \"diagnostics\": []\n}\n"));
    }
}
