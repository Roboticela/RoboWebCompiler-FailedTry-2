#include "RendererHarness.h"

#include "RoboHTMLCompiler/JavaScriptModule.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include "RoboHTMLCompiler/NativeTreeBuilder.h"
#include "RoboHTMLCompiler/Parsers.h"
#include "RoboHTMLCompiler/Registry.h"
#include "RoboHTMLCompiler/StyleResolver.h"
#include "RoboHTMLCompiler/Validation.h"

namespace RoboTools
{
    using namespace RoboHTMLCompiler;

    std::string WrapFragment(const std::string& body)
    {
        return "<!DOCTYPE html>\n<html lang=\"en\"><head><title>App</title></head><body>\n" + body +
               "\n</body></html>\n";
    }

    namespace
    {
        bool ReadTextFile(const std::filesystem::path& path, std::string& contents)
        {
            std::ifstream stream(path, std::ios::binary);

            if (!stream)
            {
                return false;
            }

            std::ostringstream buffer;
            buffer << stream.rdbuf();
            contents = buffer.str();
            return true;
        }

        /// True when a script file is the resource the document asked for.
        ///
        /// A document names a resource the way the browser would: `app.js`, or
        /// `./scripts/app.js`. The file on disk is known by its name, so the
        /// comparison is on the last path segment.
        bool IsReferencedScript(const std::string& fileName,
                               const std::vector<std::string>& references)
        {
            for (const std::string& reference : references)
            {
                std::string candidate = reference;

                while (candidate.compare(0, 2, "./") == 0)
                {
                    candidate.erase(0, 2);
                }

                const std::size_t slash = candidate.find_last_of('/');
                if (slash != std::string::npos)
                {
                    candidate = candidate.substr(slash + 1);
                }

                if (candidate == fileName)
                {
                    return true;
                }
            }

            return false;
        }

        /// Loads every stylesheet and script beside a document, which is what the
        /// compiler driver does: an application directory is the unit of
        /// compilation, and the document names its classes without naming the
        /// file that defines them.
        void LoadCompanions(const std::string& directory,
                            std::vector<std::pair<std::string, std::string>>& styleSheets,
                            std::vector<std::pair<std::string, std::string>>& scripts)
        {
            if (directory.empty() || !std::filesystem::is_directory(directory))
            {
                return;
            }

            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const std::string name = entry.path().filename().string();
                const std::string extension = entry.path().extension().string();

                std::string contents;
                if (!ReadTextFile(entry.path(), contents))
                {
                    continue;
                }

                if (extension == ".css")
                {
                    styleSheets.emplace_back(name, contents);
                }
                else if (extension == ".js")
                {
                    scripts.emplace_back(name, contents);
                }
            }
        }
    }

    bool CompiledApplication::Build(const std::string& html, const HarnessOptions& options, std::string& error)
    {
        built = false;
        formattedDiagnostics.clear();

        // The compiler is in process here, so the renderer's diagnostics can name
        // element categories with the compiler's own vocabulary instead of the
        // numeric value the runtime falls back to. The name table is compiler
        // knowledge, so it is installed as a function rather than duplicated - the
        // same thing the generated runtime target does for an application.
        RoboRenderer::ConfigureElementKindNames(
            static_cast<RoboRenderer::ElementKindNameFunction>(&RoboHTMLCompiler::ToString));

        std::vector<std::pair<std::string, std::string>> styleSheets = options.styleSheets;
        std::vector<std::pair<std::string, std::string>> scripts = options.scripts;
        LoadCompanions(options.sourceDirectory, styleSheets, scripts);

        // --- parse ---------------------------------------------------------
        HtmlParser parser(sink, Registry::Instance(), options.documentPath);
        std::unique_ptr<MarkupDocument> document = parser.Parse(html);

        if (document == nullptr || sink.HasErrors())
        {
            error = sink.FormatAll();
            formattedDiagnostics = error;
            return false;
        }

        // --- validate ------------------------------------------------------
        if (options.validate)
        {
            HtmlValidator validator(sink, Registry::Instance(), ValidationOptions{});
            validator.Validate(*document);
        }

        // --- style references ----------------------------------------------
        StyleSheetCompiler styleSheetCompiler(sink);

        for (const std::pair<std::string, std::string>& styleSheet : styleSheets)
        {
            styleSheetCompiler.AddStyleSheet(styleSheet.first, styleSheet.second);
        }

        styleSheetCompiler.ValidateStyleReferences(*document);

        // --- behaviour -----------------------------------------------------
        // Only the modules this document references belong to it. A directory can
        // hold one script per page - the multi-page example does - and loading all
        // of them would link every page's behaviour against every other page's
        // ids.
        std::vector<std::string> referencedScripts;
        document->ForEachElement([&referencedScripts](const ElementNode& element) {
            if (element.ns != MarkupNamespace::Html || element.sourceTag != "script")
            {
                return;
            }

            const AttributeNode* source = element.FindAttribute("src");
            if (source != nullptr && !source->value.empty())
            {
                referencedScripts.push_back(source->value);
            }
        });

        JavaScriptModuleCompiler behaviourModule(sink);

        for (const std::pair<std::string, std::string>& script : scripts)
        {
            if (!IsReferencedScript(script.first, referencedScripts))
            {
                continue;
            }

            ScriptResource resource;
            resource.reference = script.first;
            behaviourModule.AddScriptResource(resource);
            behaviourModule.AddModule(script.first, script.second);
        }

        behaviourModule.LinkAgainstDocument(*document);

        if (sink.HasErrors())
        {
            error = sink.FormatAll();
            formattedDiagnostics = error;
            return false;
        }

        // --- native IR -----------------------------------------------------
        model = NativeUiModel{};
        model.applicationName = options.applicationName;

        StyleReferenceResolver styleReferenceResolver(styleSheetCompiler, sink);
        NativeTreeBuilder treeBuilder(sink, Registry::Instance(), styleReferenceResolver);

        ElementId nextElementId = 1;
        model.root = treeBuilder.BuildDocument(*document, model, nextElementId);

        if (model.root == nullptr || sink.HasErrors())
        {
            error = sink.FormatAll();
            formattedDiagnostics = error;
            return false;
        }

        // --- authored CSS -> native styles ---------------------------------
        //
        // The cascade runs over the document the harness just built, with the
        // same stylesheets the web target would ship, so an application rendered
        // through this harness is rendered with the CSS it declares.
        CssLengthContext cssContext;
        cssContext.viewportWidth = options.viewportWidth;
        cssContext.viewportHeight = options.viewportHeight;

        CssStyleBuilder styleBuilder(sink);

        if (!styleBuilder.Build(*document, styleSheetCompiler.GetSources(), cssContext, styleTable))
        {
            error = sink.FormatAll();
            formattedDiagnostics = error;
            return false;
        }

        // --- compiled UI ---------------------------------------------------
        CompiledUiBuilder compiledUi(sink);

        if (!compiledUi.Build(model, options.applicationName, storage, &styleTable) || sink.HasErrors())
        {
            error = sink.FormatAll();
            formattedDiagnostics = error;
            return false;
        }

        formattedDiagnostics = sink.FormatAll();
        built = true;
        return true;
    }
}
