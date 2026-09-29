// ---------------------------------------------------------------------------
// RoboHTMLCompiler - command line entry point.
//
//     RoboHTMLCompiler [options] [sourceDirectory]
//
// Options:
//     --output <directory>       Root output directory (default: build)
//     --name <applicationName>   Application name (default: derived from the document)
//     --all                      Compile every HTML document in the directory
//     --target <web|native|all>  Output targets (default: all)
//     --strictness <lax|strict|pedantic>
//     --allow-obsolete           Accept obsolete elements and attributes
//     --no-ir                    Do not emit the IR dump
//     --quiet                    Do not print the success summary
//     --list-registry            Print the compiled-in registry summary and exit
//     --help                     Print this help and exit
// ---------------------------------------------------------------------------

#include "RoboHTMLCompiler/Compiler.h"
#include "RoboHTMLCompiler/Registry.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void PrintUsage()
    {
        std::cout <<
            "RoboHTMLCompiler " << "0.1.0" << "\n"
            "\n"
            "A strict HTML/SVG/MathML to native C++ data model compiler.\n"
            "\n"
            "Usage:\n"
            "  RoboHTMLCompiler [options] [sourceDirectory]\n"
            "\n"
            "Options:\n"
            "  --output <directory>       Root output directory (default: build)\n"
            "  --name <applicationName>   Application name (default: derived from the document)\n"
            "  --all                      Compile every HTML document in the directory, each into\n"
            "                             its own <output>/<DocumentName> subdirectory\n"
            "  --target <web|native|all>  Output targets (default: all)\n"
            "  --strictness <lax|strict|pedantic>\n"
            "  --allow-obsolete           Accept obsolete elements and attributes\n"
            "  --no-ir                    Do not emit the IR dump\n"
            "  --quiet                    Do not print the success summary\n"
            "  --list-registry            Print the compiled-in registry summary and exit\n"
            "  --help                     Print this help and exit\n"
            "\n"
            "Generated output:\n"
            "  <output>/GeneratedHTML/<document>.html  browser compatible HTML\n"
            "  <output>/GeneratedNative/              strongly typed C++ model\n"
            "  <output>/GeneratedIR/                  machine readable native IR\n"
            "  <output>/GeneratedReports/             compilation report and diagnostics\n";
    }

    void PrintRegistrySummary()
    {
        const RoboHTMLCompiler::Registry& registry = RoboHTMLCompiler::Registry::Instance();

        std::cout << "RoboHTMLCompiler compiled-in registry\n";
        std::cout << "  element definitions:  " << registry.GetElementCount() << "\n";
        std::cout << "  attribute records:    " << registry.GetAttributeCount() << "\n";

        const char* const namespaces[] = { "HTML", "SVG", "MathML" };
        std::cout << "\nNamespace vocabulary:\n";
        for (const char* name : namespaces)
        {
            (void)name;
        }

        std::cout << "  (see CompilerKnowledge/ImplementationCoverage.json for reference coverage status)\n";
    }
}

int main(int argumentCount, char** argumentValues)
{
    using namespace RoboHTMLCompiler;

    std::filesystem::path sourceDirectory = "Examples/BasicApp";
    std::filesystem::path outputDirectory = "build";
    std::string applicationName = "BasicApp";
    bool applicationNameGiven = false;
    bool emitIrDump = true;
    bool quiet = false;
    bool allowObsolete = false;
    bool compileAllDocuments = false;

    CompilationOptions options;
    options.generateWebTarget = true;
    options.generateNativeTarget = true;

    for (int index = 1; index < argumentCount; ++index)
    {
        const std::string argument = argumentValues[index];

        if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return 0;
        }

        if (argument == "--list-registry")
        {
            PrintRegistrySummary();
            return 0;
        }

        if (argument == "--output" && index + 1 < argumentCount)
        {
            outputDirectory = argumentValues[++index];
            continue;
        }

        if (argument == "--name" && index + 1 < argumentCount)
        {
            applicationName = argumentValues[++index];
            applicationNameGiven = true;
            continue;
        }

        if (argument == "--target" && index + 1 < argumentCount)
        {
            const std::string target = argumentValues[++index];
            if (target == "web")
            {
                options.generateWebTarget = true;
                options.generateNativeTarget = false;
            }
            else if (target == "native")
            {
                options.generateWebTarget = false;
                options.generateNativeTarget = true;
            }
            else
            {
                options.generateWebTarget = true;
                options.generateNativeTarget = true;
            }
            continue;
        }

        if (argument == "--strictness" && index + 1 < argumentCount)
        {
            const std::string level = argumentValues[++index];
            if (level == "lax")
            {
                options.validation.strictness = Strictness::Lax;
            }
            else if (level == "pedantic")
            {
                options.validation.strictness = Strictness::Pedantic;
            }
            else
            {
                options.validation.strictness = Strictness::Strict;
            }
            continue;
        }

        if (argument == "--allow-obsolete")
        {
            allowObsolete = true;
            options.validation.rejectObsoleteElements = false;
            options.validation.rejectObsoleteAttributes = false;
            continue;
        }

        if (argument == "--all" || argument == "--batch")
        {
            compileAllDocuments = true;
            continue;
        }

        if (argument == "--no-ir")
        {
            emitIrDump = false;
            continue;
        }

        if (argument == "--quiet")
        {
            quiet = true;
            continue;
        }

        if (!argument.empty() && argument[0] == '-')
        {
            std::cerr << "RoboHTMLCompiler: unknown option '" << argument << "'\n\n";
            PrintUsage();
            return 2;
        }

        sourceDirectory = argument;
    }

    if (!applicationNameGiven)
    {
        const std::string stem = sourceDirectory.filename().string();
        if (!stem.empty())
        {
            applicationName = stem;
        }
    }

    options.sourceDirectory = sourceDirectory;
    options.outputDirectory = outputDirectory;
    options.applicationName = applicationName;
    options.emitIrDump = emitIrDump;

    DiagnosticSink diagnostics;
    Compiler compiler(diagnostics);

    if (compileAllDocuments)
    {
        const BatchCompilationResult batch = compiler.CompileBatch(options);

        if (!batch.Succeeded() || diagnostics.HasErrors())
        {
            if (batch.documents.empty())
            {
                std::cerr << "\n" << diagnostics.FormatErrors();
            }
            std::cerr << "\nBatch compilation failed: " << batch.failedCount << " of "
                      << batch.documents.size() << " document(s) did not compile.\n";
            return 1;
        }

        if (!quiet)
        {
            std::cout << "\nRoboHTMLCompiler batch build complete\n";
            std::cout << "  documents:         " << batch.compiledCount << "\n";
            std::cout << "  output:            " << batch.outputDirectory.generic_string() << "\n";
            std::cout << "  batch report:      " << batch.reportPath.generic_string() << "\n";
            for (const BatchDocument& document : batch.documents)
            {
                std::cout << "  " << document.documentName << "  ->  " << document.applicationName
                          << "NativeModel.{h,cpp}" << "\n";
            }
        }

        return 0;
    }

    const CompilationResult result = compiler.Compile(options);

    if (!result.succeeded || diagnostics.HasErrors())
    {
        std::cerr << "\n" << diagnostics.FormatErrors();
        std::cerr << "\nCompilation failed. No native output was produced.\n";
        return 1;
    }

    if (!quiet)
    {
        std::cout << "\nRoboHTMLCompiler build complete\n";
        std::cout << "  source elements:   " << result.elementCount << "\n";
        std::cout << "  native elements:   " << result.nativeElementCount << "\n";
        std::cout << "  interned styles:   " << result.styleCount << "\n";
        std::cout << "  event bindings:    " << result.eventBindingCount << "\n";

        if (result.webTarget.generated)
        {
            std::cout << "  generated HTML:    " << result.webTarget.documentPath.generic_string() << "\n";
        }
        if (result.nativeTarget.generated)
        {
            std::cout << "  generated native:  " << result.nativeTarget.sourcePath.generic_string() << "\n";
        }
        if (!result.nativeTarget.irDumpPath.empty())
        {
            std::cout << "  native IR dump:    " << result.nativeTarget.irDumpPath.generic_string() << "\n";
        }
    }

    return 0;
}
