#include "../TestFramework.h"
#include "../TestHelpers.h"

using namespace RoboHTMLCompiler;
using namespace RoboTest;

ROBO_TEST(Validation, UnknownAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<div unknownAttribute=\"123\"></div>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownAttribute));
}

ROBO_TEST(Validation, MispelledAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<img src=\"a.png\" alt=\"a\" hieght=\"10\">", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownAttribute));
}

ROBO_TEST(Validation, AttributeNotValidForElementIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<div disabled>Container</div>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::AttributeNotAllowed));

    DiagnosticSink second;
    ROBO_CHECK(!ValidateBody("<span checked>Text</span>", second));
    ROBO_CHECK(HasError(second, DiagnosticCode::AttributeNotAllowed));

    DiagnosticSink third;
    ROBO_CHECK(!ValidateBody("<p href=\"x\">Text</p>", third));
    ROBO_CHECK(HasError(third, DiagnosticCode::AttributeNotAllowed));
}

ROBO_TEST(Validation, ValidElementAttributeCombinationsAreAccepted)
{
    static const char* const snippets[] = {
        "<button id=\"save\" class=\"px-4\" disabled>Save</button>",
        "<input type=\"email\" name=\"mail\" required placeholder=\"you@example.com\" maxlength=\"80\">",
        "<img src=\"a.png\" alt=\"a\" width=\"10\" height=\"10\" loading=\"lazy\" decoding=\"async\">",
        "<a href=\"/next\" target=\"_blank\" rel=\"noopener\" download>Next</a>",
        "<video src=\"v.mp4\" controls autoplay loop muted playsinline poster=\"p.png\"></video>",
        "<table><caption>C</caption><colgroup><col span=\"2\"></colgroup>"
        "<thead><tr><th scope=\"col\" colspan=\"2\">H</th></tr></thead>"
        "<tbody><tr><td rowspan=\"2\">D</td></tr></tbody><tfoot><tr><td>F</td></tr></tfoot></table>",
        "<label for=\"task\">Task</label>",
        "<textarea rows=\"4\" cols=\"20\" maxlength=\"100\" wrap=\"soft\"></textarea>",
        "<select multiple size=\"4\"><optgroup label=\"G\"><option value=\"a\" selected>A</option></optgroup></select>",
        "<details open><summary>S</summary><p>P</p></details>",
        "<progress value=\"1\" max=\"10\"></progress>",
        "<meter value=\"1\" min=\"0\" max=\"10\" low=\"2\" high=\"8\" optimum=\"9\"></meter>",
        "<output for=\"task\" form=\"f\">R</output>",
        "<iframe src=\"a.html\" width=\"100\" height=\"100\" sandbox=\"allow-scripts\" loading=\"lazy\"></iframe>",
        "<object data=\"a.svg\" type=\"image/svg+xml\" width=\"10\" height=\"10\"></object>",
        "<source src=\"a.avif\" type=\"image/avif\" srcset=\"a.avif 1x\" sizes=\"100vw\">",
        "<track kind=\"captions\" srclang=\"en\" src=\"c.vtt\" default>",
        "<time datetime=\"2026-09-29\">Today</time>",
        "<blockquote cite=\"https://example.com\">Quote</blockquote>",
        "<ol reversed start=\"3\" type=\"a\"><li>One</li></ol>",
        "<form action=\"/x\" method=\"post\" enctype=\"multipart/form-data\" novalidate>"
        "<button formaction=\"/y\" formmethod=\"post\" formenctype=\"text/plain\" formnovalidate>Go</button></form>"
    };

    for (const char* snippet : snippets)
    {
        DiagnosticSink sink;
        if (!ValidateBody(snippet, sink))
        {
            std::cout << "      rejected: " << snippet << "\n" << FormatDiagnostics(sink) << "\n";
        }
        ROBO_CHECK(ValidateBody(snippet, sink));
    }
}

ROBO_TEST(Validation, UnknownElementIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<notanelement>Hi</notanelement>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownHtmlElement));
}

ROBO_TEST(Validation, CustomElementsAreAccepted)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody("<my-component>Hi</my-component>", sink));
    ROBO_CHECK(ValidateBody("<x-app-shell some-attribute=\"1\">Hi</x-app-shell>", sink));
}

ROBO_TEST(Validation, DataAttributesAreAcceptedEverywhere)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody("<button data-user-id=\"123\" data-role=\"save\">Save</button>", sink));
    ROBO_CHECK(ValidateBody("<div data-anything-goes=\"yes\">x</div>", sink));
}

ROBO_TEST(Validation, UnknownAriaAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<div aria-nonsense=\"1\">x</div>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownAriaAttribute));
}

ROBO_TEST(Validation, AriaAttributesAreAccepted)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody("<button aria-label=\"Save\" aria-expanded=\"false\" aria-controls=\"tasks\">Save</button>",
                            sink));
    ROBO_CHECK(ValidateBody("<div role=\"region\" aria-labelledby=\"h\" aria-live=\"polite\">x</div>", sink));
}

ROBO_TEST(Validation, ObsoleteElementsAreRejectedInStrictMode)
{
    static const char* const obsolete[] = {
        "<marquee>Hi</marquee>", "<center>Hi</center>", "<font>Hi</font>",
        "<strike>Hi</strike>", "<tt>Hi</tt>", "<plaintext>Hi", "<keygen>"
    };

    for (const char* snippet : obsolete)
    {
        DiagnosticSink sink;
        ROBO_CHECK(!ValidateBody(snippet, sink));
        ROBO_CHECK(HasError(sink, DiagnosticCode::ObsoleteElement));
    }
}

ROBO_TEST(Validation, ObsoleteAttributesAreRejectedInStrictMode)
{
    // Legacy vocabulary the current revision of the reference no longer
    // documents is still recognised, so it is rejected as obsolete instead of
    // being reported as an unknown feature.
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<iframe src=\"a.html\" allowpaymentrequest></iframe>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::ObsoleteAttribute));
    ROBO_CHECK(!HasError(sink, DiagnosticCode::UnknownAttribute));

    DiagnosticSink second;
    ROBO_CHECK(!ValidateBody("<iframe src=\"a.html\" allowusermedia></iframe>", second));
    ROBO_CHECK(HasError(second, DiagnosticCode::ObsoleteAttribute));

    DiagnosticSink third;
    ROBO_CHECK(!ValidateBody("<table><tr><td align=\"center\">x</td></tr></table>", third));
    ROBO_CHECK(HasError(third, DiagnosticCode::ObsoleteAttribute));
}

ROBO_TEST(Validation, ObsoleteElementsCanBeAcceptedExplicitly)
{
    ValidationOptions options;
    options.rejectObsoleteElements = false;
    options.strictness = Strictness::Lax;

    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody("<center>Hi</center>", sink, options));
}

ROBO_TEST(Validation, UnknownInputTypeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<input type=\"telephone-number\">", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InvalidInputType));
}

ROBO_TEST(Validation, ObsoleteInputTypeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<input type=\"datetime\">", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::ObsoleteAttribute));
}

ROBO_TEST(Validation, DuplicateAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<div id=\"a\" id=\"b\">x</div>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::DuplicateAttribute));
}

ROBO_TEST(Validation, ImageRequiresASource)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<img alt=\"a\">", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::MissingRequiredAttribute));

    DiagnosticSink second;
    ROBO_CHECK(ValidateBody("<img src=\"a.png\" alt=\"a\">", second));
}

ROBO_TEST(Validation, MissingDocumentElementIsRejected)
{
    DiagnosticSink sink;
    ValidationOptions options;
    options.requireDocumentElement = true;

    ROBO_CHECK(!ValidateDocument("<div>no html element</div>", sink, options));
    ROBO_CHECK(HasError(sink, DiagnosticCode::MissingDocumentElement));
}

ROBO_TEST(Validation, UnexpectedEndTagIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<div>x</span></div>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnexpectedEndTag));
}

ROBO_TEST(Validation, UnclosedElementIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateDocument("<html><head></head><body><div>x</body></html>", sink));
    ROBO_CHECK(sink.HasErrors());
}

ROBO_TEST(Validation, VoidElementsNeedNoEndTag)
{
    static const char* const voids[] = {
        "br", "hr", "img", "input", "meta", "link", "source", "track", "wbr", "col", "embed", "area", "base"
    };

    for (const char* tag : voids)
    {
        const ElementDefinition* definition = Registry::Instance().FindHtmlElement(tag);
        ROBO_CHECK(definition != nullptr);
        if (definition != nullptr)
        {
            ROBO_CHECK(definition->isVoid);
        }
    }
}
