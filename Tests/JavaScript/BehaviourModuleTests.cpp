#include "../TestFramework.h"
#include "../TestHelpers.h"

#include "RoboHTMLCompiler/JavaScriptModule.h"

#include <string>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

ROBO_TEST(Behaviour, EventBindingsAreExtractedFromTheModule)
{
    DiagnosticSink sink;
    JavaScriptModuleCompiler compiler(sink);

    compiler.AddModule("app.js",
                       "const saveButton = document.getElementById(\"save\");\n"
                       "const taskInput = document.querySelector(\"#task\");\n"
                       "function onSave(event) { event.preventDefault(); }\n"
                       "saveButton.addEventListener(\"click\", onSave);\n"
                       "taskInput.addEventListener(\"change\", onSave);\n");

    ROBO_CHECK_EQ(compiler.GetEventBindings().size(), static_cast<std::size_t>(2));

    if (compiler.GetEventBindings().size() == 2)
    {
        ROBO_CHECK_EQ(compiler.GetEventBindings()[0].targetId, std::string("save"));
        ROBO_CHECK_EQ(compiler.GetEventBindings()[0].event, std::string("click"));
        ROBO_CHECK_EQ(compiler.GetEventBindings()[0].handler, std::string("onSave"));
        ROBO_CHECK_EQ(compiler.GetEventBindings()[1].targetId, std::string("task"));
        ROBO_CHECK_EQ(compiler.GetEventBindings()[1].event, std::string("change"));
    }
}

ROBO_TEST(Behaviour, QuerySelectorIdFormIsRecognised)
{
    DiagnosticSink sink;
    JavaScriptModuleCompiler compiler(sink);
    compiler.AddModule("app.js", "const el = document.querySelector(\"#panel\");\n");

    ROBO_CHECK_EQ(compiler.GetElementQueries().size(), static_cast<std::size_t>(1));
    if (!compiler.GetElementQueries().empty())
    {
        ROBO_CHECK_EQ(compiler.GetElementQueries()[0].targetId, std::string("panel"));
        ROBO_CHECK_EQ(compiler.GetElementQueries()[0].sourceLine,
                      std::string("const el = document.querySelector(\"#panel\");"));

        // The query remembers where it was written, and it points at the id
        // itself: the '#' is not part of the id.
        ROBO_CHECK_EQ(compiler.GetElementQueries()[0].line, 1);
        ROBO_CHECK_EQ(compiler.GetElementQueries()[0].column,
                      static_cast<int>(compiler.GetElementQueries()[0].sourceLine.find("panel")) + 1);
    }
}

ROBO_TEST(Behaviour, DanglingReferencesAreReported)
{
    DiagnosticSink sink;
    JavaScriptModuleCompiler compiler(sink);
    compiler.AddModule("app.js", "const save = document.getElementById(\"nope\");\n");

    std::unique_ptr<MarkupDocument> document =
        ParseHtml(WrapBody("<button id=\"save\">Save</button>"), sink);
    ROBO_CHECK(document != nullptr);
    if (document == nullptr)
    {
        return;
    }

    ROBO_CHECK(!compiler.LinkAgainstDocument(*document));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnresolvedScriptResource));

    // The report points at the script line that asks for the missing element,
    // not at the document, which is what tells the reader where to look.
    const Diagnostic* reported = FindDiagnostic(sink, DiagnosticCode::UnresolvedScriptResource);
    ROBO_CHECK(reported != nullptr);

    if (reported != nullptr)
    {
        ROBO_CHECK_EQ(reported->location.file, std::string("app.js"));
        ROBO_CHECK_EQ(reported->location.line, 1);
        ROBO_CHECK(reported->message.find("nope") != std::string::npos);
        ROBO_CHECK(reported->found.find("getElementById") != std::string::npos);

        // The character number and the shown line have to agree: a reader who
        // counts along the line must land on the id.
        ROBO_CHECK_EQ(reported->location.column, static_cast<int>(reported->found.find("nope")) + 1);
    }
}

ROBO_TEST(Behaviour, ResolvableReferencesLinkCleanly)
{
    DiagnosticSink sink;
    JavaScriptModuleCompiler compiler(sink);
    compiler.AddModule("app.js", "const save = document.getElementById(\"save\");\n"
                                 "save.addEventListener(\"click\", onSave);\n");

    std::unique_ptr<MarkupDocument> document =
        ParseHtml(WrapBody("<button id=\"save\">Save</button>"), sink);
    ROBO_CHECK(document != nullptr);
    if (document == nullptr)
    {
        return;
    }

    ROBO_CHECK(compiler.LinkAgainstDocument(*document));
}

ROBO_TEST(Behaviour, StructureBuiltFromBehaviourIsRejected)
{
    DiagnosticSink sink;
    JavaScriptModuleCompiler compiler(sink);
    compiler.AddModule("app.js", "const list = document.getElementById(\"tasks\");\n"
                                 "list.innerHTML = \"<li>item</li>\";\n");

    ROBO_CHECK(HasError(sink, DiagnosticCode::MalformedBehaviorScript));
}

ROBO_TEST(Behaviour, ScriptResourcesAreRecorded)
{
    DiagnosticSink sink;
    JavaScriptModuleCompiler compiler(sink);

    ScriptResource resource;
    resource.reference = "app.js";
    resource.isDeferred = true;
    compiler.AddScriptResource(resource);

    ROBO_CHECK_EQ(compiler.GetScriptResources().size(), static_cast<std::size_t>(1));
}
