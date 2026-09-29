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

    ROBO_CHECK_EQ(compiler.GetReferencedElementIds().size(), static_cast<std::size_t>(1));
    if (!compiler.GetReferencedElementIds().empty())
    {
        ROBO_CHECK_EQ(compiler.GetReferencedElementIds()[0], std::string("panel"));
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
