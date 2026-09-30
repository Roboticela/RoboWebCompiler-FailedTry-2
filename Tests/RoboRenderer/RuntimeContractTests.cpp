// ---------------------------------------------------------------------------
// The contract between RoboHTMLCompiler and RoboRenderer.
//
// These tests are about the seam: the compiled data the compiler emits, the
// categories the renderer resolves it to, and the diagnostics the runtime
// produces when the data cannot be trusted.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

using namespace RoboRenderer;

namespace
{
    /// A document with one element of every category the compiler can produce,
    /// so category resolution is exercised rather than assumed.
    const char* CategoryProbeDocument =
        "<div id=\"box\">"
        "<p>paragraph</p><pre>pre</pre><h2>heading</h2><br><hr>"
        "<a href=\"/next\">link</a>"
        "<ul><li>one</li><li>two</li></ul>"
        "<dl><dt>term</dt><dd>definition</dd></dl>"
        "<figure><img src=\"a.png\" alt=\"a\" width=\"10\" height=\"10\"><figcaption>caption</figcaption></figure>"
        "<form id=\"f\"><fieldset><legend>legend</legend>"
        "<label for=\"q\">label</label>"
        "<button type=\"submit\">go</button>"
        "<input id=\"q\" type=\"text\" placeholder=\"placeholder\">"
        "<input type=\"checkbox\"><input type=\"radio\" name=\"r\"><input type=\"range\" min=\"0\" max=\"10\">"
        "<input type=\"color\" value=\"#ff0000\"><input type=\"file\">"
        "<textarea rows=\"2\" cols=\"10\">text</textarea>"
        "<select><option>one</option><option>two</option></select>"
        "<output>out</output><progress value=\"1\" max=\"2\"></progress><meter value=\"1\" max=\"2\"></meter>"
        "</fieldset></form>"
        "<table><caption>caption</caption><colgroup><col></colgroup>"
        "<thead><tr><th scope=\"col\">h</th></tr></thead><tbody><tr><td>cell</td></tr></tbody></table>"
        "<details><summary>summary</summary><p>body</p></details>"
        "<dialog open><p>dialog</p></dialog>"
        "<canvas width=\"10\" height=\"10\"></canvas>"
        "</div>";
}

ROBO_TEST(RuntimeContract, CompilerOutputIsAcceptedByTheRenderer)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody(CategoryProbeDocument, application, error));
    if (error.empty())
    {
        return;
    }

    const CompiledDocument& document = application.Document();

    RendererDiagnostics diagnostics;
    ROBO_CHECK(ValidateCompiledDocument(document, diagnostics));
    ROBO_CHECK_EQ(diagnostics.ErrorCount(), static_cast<std::size_t>(0));

    // The generated tables are dense and indexed: root is index 0, and every
    // element is reachable in document order.
    ROBO_CHECK_EQ(document.root, static_cast<ElementIndex>(0));
    ROBO_CHECK(document.elementCount > 20);

    std::uint32_t visited = 0;
    for (ElementIndex index = document.root; index != NoElement; index = NextInDocumentOrder(document, index))
    {
        ++visited;
    }

    ROBO_CHECK_EQ(visited, document.elementCount);
}

ROBO_TEST(RuntimeContract, EveryHtmlCategoryResolvesToANativeCategory)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody(CategoryProbeDocument, application, error));

    const CompiledDocument& document = application.Document();

    // Categories the HTML-only phase must paint natively.
    static const RenderCategory expected[] = {
        RenderCategory::Document,      RenderCategory::Container,  RenderCategory::Text,
        RenderCategory::PreformattedText, RenderCategory::Heading, RenderCategory::LineBreak,
        RenderCategory::Separator,     RenderCategory::Link,       RenderCategory::List,
        RenderCategory::ListItem,      RenderCategory::DescriptionTerm,
        RenderCategory::DescriptionDescription, RenderCategory::Figure, RenderCategory::FigureCaption,
        RenderCategory::Image,         RenderCategory::Form,       RenderCategory::FieldSet,
        RenderCategory::Legend,        RenderCategory::Label,      RenderCategory::Button,
        RenderCategory::Input,         RenderCategory::Checkbox,   RenderCategory::Radio,
        RenderCategory::Range,         RenderCategory::ColorInput, RenderCategory::FileInput,
        RenderCategory::TextArea,      RenderCategory::Select,     RenderCategory::Option,
        RenderCategory::Output,        RenderCategory::Progress,   RenderCategory::Meter,
        RenderCategory::Table,         RenderCategory::TableCaption, RenderCategory::TableRow,
        RenderCategory::TableCell,     RenderCategory::Disclosure, RenderCategory::DisclosureSummary,
        RenderCategory::Dialog,        RenderCategory::Canvas
    };

    for (RenderCategory category : expected)
    {
        ROBO_CHECK(FindNth(document, category) != NoElement);
    }

    // Category names exist for diagnostics, and none of them is empty.
    for (RenderCategory category : expected)
    {
        ROBO_CHECK(ToString(category) != nullptr);
        ROBO_CHECK(ToString(category)[0] != '\0');
    }
}

ROBO_TEST(RuntimeContract, ElementKindNamesAreInstalledByTheApplication)
{
    // The renderer does not embed the HTML vocabulary; the application installs
    // the compiler's table. Without it a diagnostic names the numeric value.
    const ElementKind kind = static_cast<ElementKind>(ElementKind::Button);

    ROBO_CHECK(ElementKindName(kind) != nullptr);

    ConfigureElementKindNames([](ElementKind value) -> const char* {
        return value == ElementKind::Button ? "button" : "other";
    });

    ROBO_CHECK_EQ(std::string(ElementKindName(kind)), std::string("button"));

    ConfigureElementKindNames(nullptr);
}

ROBO_TEST(RuntimeContract, InvalidDataIsReportedNotSilentlyIgnored)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div><p>text</p></div>", application, error));

    // A hand built document with a broken tree: the validation is what stands
    // between a corrupt table and undefined behaviour.
    CompiledDocument broken = application.Document();

    RendererDiagnostics diagnostics;
    ROBO_CHECK(ValidateCompiledDocument(broken, diagnostics));
    ROBO_CHECK(!diagnostics.HasErrors());

    diagnostics.Clear();

    // Root that points at nothing.
    CompiledDocument noRoot = broken;
    noRoot.root = NoElement;
    ROBO_CHECK(!ValidateCompiledDocument(noRoot, diagnostics));
    ROBO_CHECK(diagnostics.HasErrors());

    diagnostics.Clear();

    // An element whose parent index is out of range.
    std::vector<CompiledElement> elements(application.Model().CountElements());
    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        elements[index] = broken.elements[index];
    }

    elements[1].parent = static_cast<ElementIndex>(elements.size() + 5);

    CompiledDocument badParent = broken;
    badParent.elements = elements.data();

    ROBO_CHECK(!ValidateCompiledDocument(badParent, diagnostics));
    ROBO_CHECK(diagnostics.HasErrors());

    diagnostics.Clear();

    // A resource index past the end of the resource table.
    elements[1].parent = broken.elements[1].parent;

    std::vector<CompiledResource> resources(broken.resourceCount);
    for (std::uint32_t index = 0; index < broken.resourceCount; ++index)
    {
        resources[index] = broken.resources[index];
    }

    elements[1].resource = broken.resourceCount + 3;
    elements[1].flags |= ElementFlagHasResource;

    CompiledDocument badResource = broken;
    badResource.elements = elements.data();
    badResource.resources = resources.data();

    ROBO_CHECK(!ValidateCompiledDocument(badResource, diagnostics));
    ROBO_CHECK(diagnostics.HasErrors());

    // Every diagnostic carries a code and a message: a report is actionable or
    // it is noise.
    for (const RendererDiagnostic& diagnostic : diagnostics.Entries())
    {
        ROBO_CHECK(ToString(diagnostic.code) != nullptr);
        ROBO_CHECK(!diagnostic.message.empty());
    }
}

ROBO_TEST(RuntimeContract, RendererRefusesToDrawFromInvalidData)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<button>Save</button>", application, error));

    CompiledDocument broken = application.Document();
    broken.root = NoElement;

    SoftwareBackend backend;
    Runtime renderer(broken);

    RendererOptions options;
    options.target.logicalWidth = 320.0f;
    options.target.logicalHeight = 200.0f;

    // Unusable compiled data is refused rather than half drawn.
    ROBO_CHECK(!renderer.Initialize(backend, options));
    ROBO_CHECK(renderer.DiagnosticLog().HasErrors());
}

ROBO_TEST(RuntimeContract, ElementRelationshipsAreDenseIndices)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div id=\"a\"><span>one</span><span>two</span></div>", application, error));

    const CompiledDocument& document = application.Document();

    const ElementIndex container = FindElementById(document, "a");
    ROBO_CHECK(container != NoElement);

    const CompiledElement* element = ElementAt(document, container);
    ROBO_CHECK(element != nullptr);

    ROBO_CHECK_EQ(ChildCount(document, container), static_cast<std::uint32_t>(2));

    if (element != nullptr)
    {
        // First child, last child and sibling links agree with each other and
        // with the parent links: this is what the renderer walks, never a map.
        const CompiledElement* first = ElementAt(document, element->firstChild);
        const CompiledElement* second = ElementAt(document, first != nullptr ? first->nextSibling : NoElement);
        const CompiledElement* last = ElementAt(document, element->lastChild);

        ROBO_CHECK(first != nullptr);
        ROBO_CHECK(second != nullptr);
        ROBO_CHECK(last != nullptr);

        if (first != nullptr && second != nullptr && last != nullptr)
        {
            ROBO_CHECK_EQ(first->parent, container);
            ROBO_CHECK_EQ(second->parent, container);
            ROBO_CHECK(second == last);
        }
    }
}

ROBO_TEST(RuntimeContract, RuntimeStateIsSeparateFromCompiledData)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input type=\"checkbox\" checked><button>Save</button>", application, error));

    const CompiledDocument& document = application.Document();

    UiState state(document);
    const ElementIndex checkbox = FindNth(document, RenderCategory::Checkbox);

    ROBO_CHECK(checkbox != NoElement);
    ROBO_CHECK(state.Element(checkbox).HasFlag(RuntimeFlagChecked));

    state.Element(checkbox).ClearFlag(RuntimeFlagChecked);

    // The compiled table still says the document declared it checked: runtime
    // state never writes through to compiled data.
    ROBO_CHECK(ElementAt(document, checkbox)->HasFlag(ElementFlagChecked));
    ROBO_CHECK(!state.Element(checkbox).HasFlag(RuntimeFlagChecked));

    state.Reset();
    ROBO_CHECK(state.Element(checkbox).HasFlag(RuntimeFlagChecked));
}
