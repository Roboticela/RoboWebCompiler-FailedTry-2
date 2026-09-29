#include "../TestFramework.h"
#include "../TestHelpers.h"

#include "RoboHTMLCompiler/NativeTreeBuilder.h"

#include <string>
#include <vector>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    /// Lowers a body fragment into a native model.
    std::unique_ptr<NativeElement> LowerBody(const std::string& body,
                                             NativeUiModel& model,
                                             DiagnosticSink& sink)
    {
        std::unique_ptr<MarkupDocument> document = ParseHtml(WrapBody(body), sink);
        if (document == nullptr)
        {
            return nullptr;
        }

        StyleSheetCompiler styleSheetCompiler(sink);
        styleSheetCompiler.AddStyleSheet("app.css", ".card { padding: 1rem; }\n");

        StyleReferenceResolver resolver(styleSheetCompiler, sink);
        NativeTreeBuilder builder(sink, Registry::Instance(), resolver);

        ElementId nextId = 1;
        return builder.BuildDocument(*document, model, nextId);
    }

    const NativeElement* Find(const NativeElement& node, ElementKind kind)
    {
        if (node.kind == kind)
        {
            return &node;
        }

        for (const std::unique_ptr<NativeElement>& child : node.children)
        {
            const NativeElement* found = Find(*child, kind);
            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }
}

ROBO_TEST(NativeModel, ButtonLoweringMatchesTheDocumentedShape)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<button id=\"save\" class=\"px-4 py-2 rounded-lg text-white\" disabled>Save</button>", model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* button = Find(*root, ElementKind::Button);
    ROBO_CHECK(button != nullptr);
    if (button == nullptr)
    {
        return;
    }

    ROBO_CHECK_EQ(static_cast<int>(button->kind), static_cast<int>(ElementKind::Button));
    ROBO_CHECK_EQ(button->content.id, std::string("save"));
    ROBO_CHECK(button->styleId != 0);
    ROBO_CHECK(button->state.disabled);
    ROBO_CHECK_EQ(button->content.text, std::string("Save"));
    ROBO_CHECK_EQ(button->source.tag, std::string("button"));
}

ROBO_TEST(NativeModel, HeadingLevelsAreNormalised)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody("<h1>One</h1><h3>Three</h3>", model, sink);
    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    std::vector<const NativeElement*> headings;
    std::vector<const NativeElement*> all;
    root->CollectDescendants(all);
    for (const NativeElement* element : all)
    {
        if (element->kind == ElementKind::Heading)
        {
            headings.push_back(element);
        }
    }

    ROBO_CHECK_EQ(headings.size(), static_cast<std::size_t>(2));
    if (headings.size() == 2)
    {
        ROBO_CHECK_EQ(headings[0]->accessibility.level, 1);
        ROBO_CHECK_EQ(headings[1]->accessibility.level, 3);
    }
}

ROBO_TEST(NativeModel, ManySourceElementsShareOneNativeCategory)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<div>d</div><section>s</section><article>a</article><nav>n</nav><main>m</main>", model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    std::vector<const NativeElement*> all;
    root->CollectDescendants(all);

    std::size_t containerCount = 0;
    for (const NativeElement* element : all)
    {
        if (element->kind == ElementKind::Container)
        {
            ++containerCount;
        }
    }

    ROBO_CHECK_EQ(containerCount, static_cast<std::size_t>(5));
}

ROBO_TEST(NativeModel, InputTypeSpecialisation)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<input type=\"checkbox\"><input type=\"range\"><input type=\"date\"><input type=\"file\">"
        "<input type=\"email\"><input type=\"submit\"><input>",
        model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    static const ElementKind expected[] = {
        ElementKind::Checkbox, ElementKind::Range, ElementKind::DateInput, ElementKind::FileInput,
        ElementKind::Input, ElementKind::SubmitButton, ElementKind::Input
    };

    std::vector<ElementKind> kinds;
    std::vector<const NativeElement*> all;
    root->CollectDescendants(all);
    for (const NativeElement* element : all)
    {
        if (element->kind == ElementKind::Checkbox || element->kind == ElementKind::Range ||
            element->kind == ElementKind::DateInput || element->kind == ElementKind::FileInput ||
            element->kind == ElementKind::Input || element->kind == ElementKind::SubmitButton)
        {
            kinds.push_back(element->kind);
        }
    }

    ROBO_CHECK_EQ(kinds.size(), static_cast<std::size_t>(7));
    for (std::size_t index = 0; index < kinds.size() && index < 7; ++index)
    {
        ROBO_CHECK_EQ(static_cast<int>(kinds[index]), static_cast<int>(expected[index]));
    }
}

ROBO_TEST(NativeModel, TypedAttributeRoutingFillsTypedFields)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<a href=\"/next\" target=\"_blank\" rel=\"noopener\">Next</a>"
        "<img src=\"a.png\" alt=\"A\" width=\"16\" height=\"9\">"
        "<input id=\"q\" name=\"q\" type=\"search\" placeholder=\"Search\" required "
        "minlength=\"2\" maxlength=\"40\" pattern=\"[a-z]+\">",
        model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* link = Find(*root, ElementKind::Link);
    ROBO_CHECK(link != nullptr);
    if (link != nullptr)
    {
        ROBO_CHECK_EQ(link->navigation.href, std::string("/next"));
        ROBO_CHECK_EQ(link->navigation.target, std::string("_blank"));
        ROBO_CHECK_EQ(link->navigation.relation, std::string("noopener"));
    }

    const NativeElement* image = Find(*root, ElementKind::Image);
    ROBO_CHECK(image != nullptr);
    if (image != nullptr)
    {
        ROBO_CHECK_EQ(image->resource.source, std::string("a.png"));
        ROBO_CHECK_EQ(image->content.alternativeText, std::string("A"));
        ROBO_CHECK(image->layout.hasWidth);
        ROBO_CHECK_EQ(image->layout.width, static_cast<std::uint32_t>(16));
        ROBO_CHECK(image->layout.hasHeight);
        ROBO_CHECK_EQ(image->layout.height, static_cast<std::uint32_t>(9));
    }

    const NativeElement* search = Find(*root, ElementKind::SearchInput);
    ROBO_CHECK(search != nullptr);
    if (search != nullptr)
    {
        ROBO_CHECK_EQ(search->content.name, std::string("q"));
        ROBO_CHECK_EQ(search->content.placeholder, std::string("Search"));
        ROBO_CHECK(search->state.required);
        ROBO_CHECK(search->validation.hasMinimumLength);
        ROBO_CHECK_EQ(search->validation.minimumLength, static_cast<std::uint32_t>(2));
        ROBO_CHECK_EQ(search->validation.maximumLength, static_cast<std::uint32_t>(40));
        ROBO_CHECK_EQ(search->validation.pattern, std::string("[a-z]+"));
    }
}

ROBO_TEST(NativeModel, AriaAttributesBecomeTypedAccessibilityProperties)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<div role=\"region\" aria-label=\"Panel\" aria-labelledby=\"h\" aria-describedby=\"d\" "
        "aria-live=\"polite\" aria-hidden=\"true\">x</div>",
        model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* region = Find(*root, ElementKind::Container);
    ROBO_CHECK(region != nullptr);
    if (region != nullptr)
    {
        ROBO_CHECK_EQ(region->accessibility.role, std::string("region"));
        ROBO_CHECK_EQ(region->accessibility.label, std::string("Panel"));
        ROBO_CHECK_EQ(region->accessibility.labelledBy, std::string("h"));
        ROBO_CHECK_EQ(region->accessibility.describedBy, std::string("d"));
        ROBO_CHECK_EQ(region->accessibility.live, std::string("polite"));
        ROBO_CHECK(region->accessibility.hasHidden);
        ROBO_CHECK(region->accessibility.hidden);
    }
}

ROBO_TEST(NativeModel, DataAttributesGoToCustomMetadata)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<button data-user-id=\"123\" data-role=\"save\">Save</button>", model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* button = Find(*root, ElementKind::Button);
    ROBO_CHECK(button != nullptr);
    if (button != nullptr)
    {
        ROBO_CHECK_EQ(button->customMetadata.entries.size(), static_cast<std::size_t>(2));
        const NativeAttribute* entry = button->customMetadata.Find("data-user-id");
        ROBO_CHECK(entry != nullptr);
        if (entry != nullptr)
        {
            ROBO_CHECK_EQ(entry->value, std::string("123"));
            ROBO_CHECK_EQ(static_cast<int>(entry->property), static_cast<int>(NativeProperty::CustomMetadata));
        }
    }
}

ROBO_TEST(NativeModel, SvgSubtreesCarrySvgProperties)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<svg viewBox=\"0 0 10 10\"><path d=\"M0 0 L1 1 Z\" fill=\"#3b82f6\" stroke=\"#1d4ed8\" "
        "stroke-width=\"2\" transform=\"translate(1 2)\"></path></svg>",
        model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* svgRoot = Find(*root, ElementKind::SvgDocument);
    ROBO_CHECK(svgRoot != nullptr);
    if (svgRoot != nullptr)
    {
        ROBO_CHECK_EQ(svgRoot->svgGeometry.viewBox, std::string("0 0 10 10"));
    }

    const NativeElement* path = Find(*root, ElementKind::SvgPath);
    ROBO_CHECK(path != nullptr);
    if (path != nullptr)
    {
        ROBO_CHECK_EQ(path->svgGeometry.path, std::string("M0 0 L1 1 Z"));
        ROBO_CHECK_EQ(path->svgPaint.fill, std::string("#3b82f6"));
        ROBO_CHECK_EQ(path->svgPaint.stroke, std::string("#1d4ed8"));
        ROBO_CHECK(path->svgPaint.strokeWidth == 2.0);
        ROBO_CHECK_EQ(path->svgTransform.value, std::string("translate(1 2)"));
    }
}

ROBO_TEST(NativeModel, MathMlSubtreesCarryMathProperties)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<math display=\"block\"><mfrac linethickness=\"1px\"><mn>1</mn><mn>2</mn></mfrac></math>",
        model, sink);

    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* mathRoot = Find(*root, ElementKind::MathRoot);
    ROBO_CHECK(mathRoot != nullptr);
    if (mathRoot != nullptr)
    {
        ROBO_CHECK_EQ(mathRoot->mathLayout.display, std::string("block"));
    }

    const NativeElement* fraction = Find(*root, ElementKind::MathFraction);
    ROBO_CHECK(fraction != nullptr);
    if (fraction != nullptr)
    {
        ROBO_CHECK_EQ(fraction->mathLayout.lineThickness, std::string("1px"));
    }
}

ROBO_TEST(NativeModel, ElementIdentitiesAreUniqueAndIndexed)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody(
        "<div id=\"a\"></div><div id=\"b\"></div><div></div>", model, sink);
    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    ROBO_CHECK_EQ(model.idIndex.size(), static_cast<std::size_t>(2));
    ROBO_CHECK(model.idIndex["a"] != model.idIndex["b"]);
    ROBO_CHECK(root->FindById(model.idIndex["b"]) != nullptr);
}

ROBO_TEST(NativeModel, SourceMetadataIsPreservedForDebugging)
{
    NativeUiModel model;
    DiagnosticSink sink;

    std::unique_ptr<NativeElement> root = LowerBody("<section>s</section>", model, sink);
    ROBO_CHECK(root != nullptr);
    if (root == nullptr)
    {
        return;
    }

    const NativeElement* section = Find(*root, ElementKind::Container);
    ROBO_CHECK(section != nullptr);
    if (section != nullptr)
    {
        ROBO_CHECK_EQ(section->source.tag, std::string("section"));
        ROBO_CHECK_EQ(static_cast<int>(section->source.ns), static_cast<int>(MarkupNamespace::Html));
        ROBO_CHECK(section->source.line > 0);
        ROBO_CHECK(!section->source.file.empty());
    }
}
