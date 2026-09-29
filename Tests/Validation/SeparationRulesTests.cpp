#include "../TestFramework.h"
#include "../TestHelpers.h"

using namespace RoboHTMLCompiler;
using namespace RoboTest;

ROBO_TEST(Separation, InlineStyleAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<button style=\"color:red\">Save</button>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineCssAttribute));

    bool reportedPosition = false;
    for (const Diagnostic& diagnostic : sink.GetDiagnostics())
    {
        if (diagnostic.code != DiagnosticCode::InlineCssAttribute)
        {
            continue;
        }

        ROBO_CHECK_EQ(diagnostic.CodeString(), std::string("MHTML001"));
        ROBO_CHECK_EQ(diagnostic.AliasString(), std::string("RHC1007"));
        ROBO_CHECK(diagnostic.location.line > 0);
        ROBO_CHECK(diagnostic.location.column > 0);
        ROBO_CHECK(!diagnostic.message.empty());
        reportedPosition = true;
    }
    ROBO_CHECK(reportedPosition);
}

ROBO_TEST(Separation, InlineStyleElementIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<style>.button { color: red; }</style>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineStyleElement));
}

ROBO_TEST(Separation, InlineEventHandlerAttributesAreRejected)
{
    static const char* const snippets[] = {
        "<button onclick=\"save()\">Save</button>",
        "<input onchange=\"handleChange()\">",
        "<div onmouseover=\"foo()\">x</div>",
        "<button onkeydown=\"return false\">Save</button>"
    };

    for (const char* snippet : snippets)
    {
        DiagnosticSink sink;
        ROBO_CHECK(!ValidateBody(snippet, sink));
        ROBO_CHECK(HasError(sink, DiagnosticCode::InlineEventHandlerAttribute));
    }
}

ROBO_TEST(Separation, UndeclaredEventHandlerAttributesAreRejected)
{
    // The reference no longer defines these handlers, but an 'on*' attribute is
    // always inline JavaScript, so the compiler must reject it as such rather
    // than fall back to an unknown attribute.
    static const char* const snippets[] = {
        "<button onloadend=\"save()\">Save</button>",
        "<button ondragexit=\"save()\">Save</button>",
        "<button onwebkitanimationend=\"save()\">Save</button>",
        "<button onmadeupevent=\"save()\">Save</button>"
    };

    for (const char* snippet : snippets)
    {
        DiagnosticSink sink;
        ROBO_CHECK(!ValidateBody(snippet, sink));
        ROBO_CHECK(HasError(sink, DiagnosticCode::InlineEventHandlerAttribute));
        ROBO_CHECK(!HasError(sink, DiagnosticCode::UnknownAttribute));
    }
}

ROBO_TEST(Separation, InlineScriptContentIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<script>const x = 1;</script>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineScriptContent));

    DiagnosticSink second;
    ROBO_CHECK(!ValidateBody("<script src=\"app.js\">const x = 1;</script>", second));
    ROBO_CHECK(HasError(second, DiagnosticCode::InlineScriptContent));
}

ROBO_TEST(Separation, ExternalScriptReferenceIsAccepted)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody("<script src=\"app.js\" defer></script>", sink));
}

ROBO_TEST(Separation, ClassReferencesAreAcceptedAsStyling)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody("<button class=\"text-red-500 px-4 py-2\">Save</button>", sink));
}

ROBO_TEST(Separation, SvgRejectsInlineStyleAndHandlers)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 10 10\"><circle cx=\"5\" cy=\"5\" r=\"4\" style=\"fill:red\"></circle></svg>",
                             sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineCssAttribute));

    DiagnosticSink second;
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 10 10\"><circle cx=\"5\" cy=\"5\" r=\"4\" onclick=\"f()\"></circle></svg>",
                             second));
    ROBO_CHECK(HasError(second, DiagnosticCode::InlineEventHandlerAttribute));
}

ROBO_TEST(Separation, GeneratedDiagnosticCarriesEveryRequiredField)
{
    DiagnosticSink sink;
    ValidateBody("<button style=\"color:red\">Save</button>", sink);

    ROBO_CHECK(!sink.GetDiagnostics().empty());
    if (sink.GetDiagnostics().empty())
    {
        return;
    }

    const Diagnostic& diagnostic = sink.GetDiagnostics().front();
    const std::string report = diagnostic.Format();

    ROBO_CHECK(report.find("MHTML001") != std::string::npos);
    ROBO_CHECK(report.find("File:") != std::string::npos);
    ROBO_CHECK(report.find("Line:") != std::string::npos);
    ROBO_CHECK(report.find("Column:") != std::string::npos);
    ROBO_CHECK(report.find("style=\"color:red\"") != std::string::npos);
}
