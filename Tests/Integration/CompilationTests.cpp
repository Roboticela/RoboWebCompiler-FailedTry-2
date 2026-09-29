#include "../TestFramework.h"
#include "../TestHelpers.h"

#include "RoboHTMLCompiler/Compiler.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef ROBO_TEST_SOURCE_DIR
#define ROBO_TEST_SOURCE_DIR "."
#endif

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    std::filesystem::path ExampleRoot()
    {
        return std::filesystem::path(ROBO_TEST_SOURCE_DIR) / "Examples" / "BasicApp";
    }

    std::filesystem::path OutputRoot(const std::string& name)
    {
        return std::filesystem::temp_directory_path() / "RoboHTMLCompilerTests" / name;
    }

    CompilationOptions MakeOptions(const std::string& name)
    {
        CompilationOptions options;
        options.sourceDirectory = ExampleRoot();
        options.outputDirectory = OutputRoot(name);
        options.applicationName = "BasicApp";
        return options;
    }

    /// A temporary application directory with the supplied sources.
    std::filesystem::path WriteTemporaryApplication(const std::string& name,
                                                    const std::string& document,
                                                    const std::string& styleSheet,
                                                    const std::string& script)
    {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() / "RoboHTMLCompilerTests" / "sources" / name;
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        std::ofstream(directory / "index.html") << document;
        if (!styleSheet.empty())
        {
            std::ofstream(directory / "app.css") << styleSheet;
        }
        if (!script.empty())
        {
            std::ofstream(directory / "app.js") << script;
        }

        return directory;
    }
}

ROBO_TEST(Integration, ExampleApplicationCompilesEndToEnd)
{
    DiagnosticSink sink;
    Compiler compiler(sink);

    const CompilationResult result = compiler.Compile(MakeOptions("example"));

    if (!result.succeeded)
    {
        std::cout << FormatDiagnostics(sink) << "\n";
    }

    ROBO_CHECK(!sink.HasErrors());
    ROBO_CHECK(result.succeeded);
    ROBO_CHECK(result.elementCount > 20);
    ROBO_CHECK(result.nativeElementCount > 20);
    ROBO_CHECK(result.styleCount > 5);
    ROBO_CHECK(result.eventBindingCount >= 2);
}

ROBO_TEST(Integration, WebTargetIsWrittenAndInspectable)
{
    DiagnosticSink sink;
    Compiler compiler(sink);

    const CompilationResult result = compiler.Compile(MakeOptions("web"));

    ROBO_CHECK(result.succeeded);
    ROBO_CHECK(result.webTarget.generated);
    ROBO_CHECK(std::filesystem::exists(result.webTarget.documentPath));
    ROBO_CHECK(std::filesystem::exists(result.webTarget.styleSheetPath));
    ROBO_CHECK(std::filesystem::exists(result.webTarget.scriptPath));

    ROBO_CHECK_EQ(result.webTarget.documentPath.filename().string(), std::string("index.html"));

    const std::string html = ReadFile(result.webTarget.documentPath);
    ROBO_CHECK(!html.empty());
    ROBO_CHECK(html.find("<!DOCTYPE html>") == 0);
    ROBO_CHECK(html.find("styles.css") != std::string::npos);
    ROBO_CHECK(html.find("scripts.js") != std::string::npos);

    // Source HTML semantics remain available to the browser target.
    ROBO_CHECK(html.find("<h1") != std::string::npos);
    ROBO_CHECK(html.find("<svg") != std::string::npos);
    ROBO_CHECK(html.find("<math") != std::string::npos);

    // The generated document must itself satisfy the compiler's rules.
    DiagnosticSink validationSink;
    ROBO_CHECK(ValidateDocument(html, validationSink, ValidationOptions{}, "Generated.html"));
}

ROBO_TEST(Integration, GeneratedStyleSheetContainsResolvedClasses)
{
    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(MakeOptions("style"));

    ROBO_CHECK(result.succeeded);

    const std::string css = ReadFile(result.webTarget.styleSheetPath);
    ROBO_CHECK(css.find(".px-4") != std::string::npos);
    ROBO_CHECK(css.find("padding-left: 1rem") != std::string::npos);
    ROBO_CHECK(css.find(".bg-blue-500") != std::string::npos);
    ROBO_CHECK(css.find(".card") != std::string::npos);
}

ROBO_TEST(Integration, NativeTargetAndIrDumpAreWritten)
{
    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(MakeOptions("native"));

    ROBO_CHECK(result.succeeded);
    ROBO_CHECK(result.nativeTarget.generated);
    ROBO_CHECK(std::filesystem::exists(result.nativeTarget.headerPath));
    ROBO_CHECK(std::filesystem::exists(result.nativeTarget.sourcePath));
    ROBO_CHECK(std::filesystem::exists(result.nativeTarget.irDumpPath));

    const std::string generated = ReadFile(result.nativeTarget.sourcePath);
    ROBO_CHECK(generated.find("RoboHTMLCompiler::ElementKind::Button") != std::string::npos);
    ROBO_CHECK(generated.find("RoboHTMLCompiler::ElementKind::Heading") != std::string::npos);
    ROBO_CHECK(generated.find("RoboHTMLCompiler::ElementKind::SvgPath") != std::string::npos);
    ROBO_CHECK(generated.find("RoboHTMLCompiler::ElementKind::MathFraction") != std::string::npos);
    ROBO_CHECK(generated.find("state.disabled") != std::string::npos);
    ROBO_CHECK(generated.find("styleId = ") != std::string::npos);

    const std::string ir = ReadFile(result.nativeTarget.irDumpPath);
    ROBO_CHECK(ir.find("\"kind\": \"Button\"") != std::string::npos);
    ROBO_CHECK(ir.find("\"bindings\"") != std::string::npos);
}

ROBO_TEST(Integration, CompilationReportIsWritten)
{
    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(MakeOptions("report"));

    ROBO_CHECK(result.succeeded);

    const std::filesystem::path reportPath = result.reportDirectory / "CompilationReport.json";
    ROBO_CHECK(std::filesystem::exists(reportPath));

    const std::string report = ReadFile(reportPath);
    ROBO_CHECK(report.find("\"project\": \"RoboHTMLCompiler\"") != std::string::npos);
    ROBO_CHECK(report.find("\"runtimeCssParsingRequired\": false") != std::string::npos);
    ROBO_CHECK(report.find("\"htmlContainsJavaScriptSource\": false") != std::string::npos);
}

ROBO_TEST(Integration, InvalidSourceProducesNoOutput)
{
    const std::filesystem::path source = WriteTemporaryApplication(
        "invalid",
        "<!DOCTYPE html><html><head><title>T</title></head><body>"
        "<button style=\"color:red\" onclick=\"save()\">Save</button>"
        "</body></html>",
        "/* nothing */\n",
        "");

    CompilationOptions options;
    options.sourceDirectory = source;
    options.outputDirectory = OutputRoot("invalid");
    options.applicationName = "Invalid";
    std::filesystem::remove_all(options.outputDirectory);

    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(options);

    ROBO_CHECK(!result.succeeded);
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineCssAttribute));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineEventHandlerAttribute));
    ROBO_CHECK(!std::filesystem::exists(options.outputDirectory / "GeneratedHTML" / "index.html"));
}

ROBO_TEST(Integration, UnknownAttributeFailsTheBuild)
{
    const std::filesystem::path source = WriteTemporaryApplication(
        "unknownAttribute",
        "<!DOCTYPE html><html><head><title>T</title></head><body>"
        "<div unknownAttribute=\"123\"></div>"
        "</body></html>",
        "/* nothing */\n",
        "");

    CompilationOptions options;
    options.sourceDirectory = source;
    options.outputDirectory = OutputRoot("unknownAttribute");
    options.applicationName = "Unknown";

    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(options);

    ROBO_CHECK(!result.succeeded);
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownAttribute));
}

ROBO_TEST(Integration, BehaviourReferencingAMissingElementFailsTheBuild)
{
    const std::filesystem::path source = WriteTemporaryApplication(
        "danglingBehaviour",
        "<!DOCTYPE html><html><head><title>T</title></head><body>"
        "<button id=\"save\" class=\"px-2\">Save</button>"
        "<script src=\"app.js\" defer></script>"
        "</body></html>",
        "/* nothing */\n",
        "const missing = document.getElementById(\"nope\");\n");

    CompilationOptions options;
    options.sourceDirectory = source;
    options.outputDirectory = OutputRoot("danglingBehaviour");
    options.applicationName = "Dangling";

    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(options);

    ROBO_CHECK(!result.succeeded);
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnresolvedScriptResource));
}

ROBO_TEST(Integration, MissingScriptResourceFailsTheBuild)
{
    const std::filesystem::path source = WriteTemporaryApplication(
        "missingResource",
        "<!DOCTYPE html><html><head><title>T</title></head><body>"
        "<button id=\"save\" class=\"px-2\">Save</button>"
        "<script src=\"absent.js\" defer></script>"
        "</body></html>",
        "/* nothing */\n",
        "");

    CompilationOptions options;
    options.sourceDirectory = source;
    options.outputDirectory = OutputRoot("missingResource");
    options.applicationName = "Missing";

    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(options);

    ROBO_CHECK(!result.succeeded);
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnresolvedScriptResource));
}

ROBO_TEST(Integration, WebOnlyAndNativeOnlyTargetsAreHonoured)
{
    CompilationOptions options = MakeOptions("webOnly");
    options.generateNativeTarget = false;

    DiagnosticSink sink;
    Compiler compiler(sink);
    const CompilationResult result = compiler.Compile(options);

    ROBO_CHECK(result.succeeded);
    ROBO_CHECK(result.webTarget.generated);
    ROBO_CHECK(!result.nativeTarget.generated);
    ROBO_CHECK(std::filesystem::exists(result.webTarget.documentPath));
    ROBO_CHECK(!std::filesystem::exists(result.nativeTarget.sourcePath));
}
