#include "../TestFramework.h"
#include "../TestHelpers.h"

#include "RoboHTMLCompiler/StyleResolver.h"

#include <string>
#include <vector>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    std::string ValueOf(const std::vector<StyleDeclaration>& declarations, const std::string& property)
    {
        for (const StyleDeclaration& declaration : declarations)
        {
            if (declaration.property == property)
            {
                return declaration.value;
            }
        }
        return std::string();
    }
}

ROBO_TEST(Style, TailwindSpacingUtilitiesExpand)
{
    std::vector<StyleDeclaration> declarations;

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("px-4", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "padding-left"), std::string("1rem"));
    ROBO_CHECK_EQ(ValueOf(declarations, "padding-right"), std::string("1rem"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("py-2", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "padding-top"), std::string("0.5rem"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("mt-0", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "margin-top"), std::string("0px"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("mx-auto", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "margin-left"), std::string("auto"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("gap-6", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "gap"), std::string("1.5rem"));
}

ROBO_TEST(Style, TailwindColorUtilitiesExpand)
{
    std::vector<StyleDeclaration> declarations;

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("text-red-500", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "color"), std::string("#ef4444"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("bg-slate-100", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "background-color"), std::string("#f1f5f9"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("text-white", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "color"), std::string("#ffffff"));

    ROBO_CHECK(!StyleSheetCompiler::ExpandTailwindUtility("text-notacolour-500", declarations));
}

ROBO_TEST(Style, TailwindLayoutUtilitiesExpand)
{
    std::vector<StyleDeclaration> declarations;

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("flex", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "display"), std::string("flex"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("items-center", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "align-items"), std::string("center"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("justify-between", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "justify-content"), std::string("space-between"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("rounded-lg", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "border-radius"), std::string("0.5rem"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("rounded", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "border-radius"), std::string("0.25rem"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("w-full", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "width"), std::string("100%"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("text-2xl", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "font-size"), std::string("1.5rem"));

    ROBO_CHECK(StyleSheetCompiler::ExpandTailwindUtility("font-semibold", declarations));
    ROBO_CHECK_EQ(ValueOf(declarations, "font-weight"), std::string("600"));
}

ROBO_TEST(Style, UnknownUtilityIsNotExpanded)
{
    std::vector<StyleDeclaration> declarations;
    ROBO_CHECK(!StyleSheetCompiler::ExpandTailwindUtility("definitely-not-a-utility", declarations));
    ROBO_CHECK_EQ(declarations.size(), static_cast<std::size_t>(0));
}

ROBO_TEST(Style, AuthoredRulesResolveForClasses)
{
    DiagnosticSink sink;
    StyleSheetCompiler compiler(sink);

    compiler.AddStyleSheet("app.css", ".card { padding: 1rem; border-radius: 4px; }\n.task { color: #111; }\n");
    ROBO_CHECK(!sink.HasErrors());

    const std::vector<StyleDeclaration> card = compiler.ResolveClassList("card");
    ROBO_CHECK_EQ(ValueOf(card, "padding"), std::string("1rem"));
    ROBO_CHECK_EQ(ValueOf(card, "border-radius"), std::string("4px"));

    const std::vector<StyleDeclaration> mixed = compiler.ResolveClassList("card px-4");
    ROBO_CHECK_EQ(ValueOf(mixed, "padding-left"), std::string("1rem"));

    ROBO_CHECK(compiler.IsKnownClass("card"));
    ROBO_CHECK(compiler.IsKnownClass("px-4"));
    ROBO_CHECK(!compiler.IsKnownClass("not-a-class"));
}

ROBO_TEST(Style, AuthoredRulesWinOverUtilities)
{
    DiagnosticSink sink;
    StyleSheetCompiler compiler(sink);
    compiler.AddStyleSheet("app.css", ".card { padding: 2rem; }\n");

    // The authored rule is merged after the utility expansion, so it wins.
    const std::vector<StyleDeclaration> resolved = compiler.ResolveClassList("p-4 card");
    ROBO_CHECK_EQ(ValueOf(resolved, "padding"), std::string("2rem"));
    ROBO_CHECK_EQ(ValueOf(resolved, "padding-left"), std::string("1rem"));
}

// A class nothing styles is allowed: HTML may name a hook that no rule answers,
// exactly as a browser accepts it. The reference is worth saying out loud, but it
// is not a reason to fail a build.
ROBO_TEST(Style, UnresolvedClassIsAWarningNotAnError)
{
    DiagnosticSink sink;
    StyleSheetCompiler compiler(sink);
    compiler.AddStyleSheet("app.css", ".card { padding: 1rem; }\n");

    const std::string document = WrapBody("<div class=\"card missing-class\">x</div>");

    std::unique_ptr<MarkupDocument> parsed = ParseHtml(document, sink);
    ROBO_CHECK(parsed != nullptr);
    if (parsed == nullptr)
    {
        return;
    }

    ROBO_CHECK(compiler.ValidateStyleReferences(*parsed));
    ROBO_CHECK(!sink.HasErrors());
    ROBO_CHECK(HasWarning(sink, DiagnosticCode::UnresolvedStyleReference));
}

ROBO_TEST(Style, MalformedStyleSheetIsReported)
{
    DiagnosticSink sink;
    StyleSheetCompiler compiler(sink);
    compiler.AddStyleSheet("broken.css", ".card { padding: 1rem;\n");

    ROBO_CHECK(HasError(sink, DiagnosticCode::MalformedStyleSheet));
}

ROBO_TEST(Style, StyleIdsAreInternedAndDeduplicated)
{
    DiagnosticSink sink;
    StyleSheetCompiler compiler(sink);
    compiler.AddStyleSheet("app.css", ".card { padding: 1rem; }\n");

    StyleReferenceResolver resolver(compiler, sink);
    NativeUiModel model;

    const std::string document = WrapBody(
        "<div class=\"card\">a</div><div class=\"card\">b</div><div class=\"px-4\">c</div><div>d</div>");

    std::unique_ptr<MarkupDocument> parsed = ParseHtml(document, sink);
    ROBO_CHECK(parsed != nullptr);
    if (parsed == nullptr)
    {
        return;
    }

    std::vector<StyleId> assigned;
    parsed->ForEachElement([&](const ElementNode& element) {
        if (element.sourceTag == "div")
        {
            assigned.push_back(resolver.Resolve(element, model));
        }
    });

    ROBO_CHECK_EQ(assigned.size(), static_cast<std::size_t>(4));
    if (assigned.size() == 4)
    {
        ROBO_CHECK(assigned[0] != 0);
        ROBO_CHECK_EQ(assigned[0], assigned[1]);
        ROBO_CHECK(assigned[2] != 0);
        ROBO_CHECK(assigned[2] != assigned[0]);
        ROBO_CHECK_EQ(assigned[3], static_cast<StyleId>(0));
    }

    ROBO_CHECK_EQ(model.styleTable.size(), static_cast<std::size_t>(2));
}

ROBO_TEST(Style, GeneratedStyleSheetContainsUsedUtilities)
{
    DiagnosticSink sink;
    StyleSheetCompiler compiler(sink);
    compiler.AddStyleSheet("app.css", ".card { padding: 1rem; }\n");

    std::unique_ptr<MarkupDocument> parsed =
        ParseHtml(WrapBody("<div class=\"card px-4\">x</div>"), sink);
    ROBO_CHECK(parsed != nullptr);
    if (parsed == nullptr)
    {
        return;
    }

    compiler.ValidateStyleReferences(*parsed);

    const std::string emitted = compiler.EmitStyleSheet();
    ROBO_CHECK(emitted.find(".px-4 {") != std::string::npos);
    ROBO_CHECK(emitted.find(".card {") != std::string::npos);
}

ROBO_TEST(Style, CanonicalizationIsOrderIndependent)
{
    std::vector<StyleDeclaration> first = { { "color", "#fff" }, { "padding", "1rem" } };
    std::vector<StyleDeclaration> second = { { "padding", "1rem" }, { "color", "#fff" } };

    ROBO_CHECK_EQ(StyleSheetCompiler::CanonicalizeDeclarations(first),
                  StyleSheetCompiler::CanonicalizeDeclarations(second));
}
