// ---------------------------------------------------------------------------
// The CSS pipeline, end to end.
//
//     HTML + CSS -> compiler -> compiled style table -> layout / paint
//
// These tests are the ones that would fail if the cascade stopped running, if the
// lowering lost a value, or if the renderer stopped consulting the table. Each
// one asserts on something a user can see: geometry, a colour, a pixel.
// ---------------------------------------------------------------------------
#include "../RoboRenderer/RendererTestSupport.h"
#include "../TestFramework.h"

#include "RoboRenderer/CompiledUi.h"
#include "RoboRenderer/Layout.h"
#include "RoboRenderer/Paint.h"

#include <string>
#include <vector>

using namespace RoboRenderer;
using namespace RoboTest;

namespace
{
    /// The one element that carries an authored style, or NoElement.
    ElementIndex StyledElement(const CompiledDocument& document, std::size_t ordinal = 0)
    {
        std::size_t seen = 0;

        for (ElementIndex index = 0; index < document.elementCount; ++index)
        {
            if (document.elements[index].styleIndex == 0)
            {
                continue;
            }

            if (seen == ordinal)
            {
                return index;
            }

            ++seen;
        }

        return NoElement;
    }

    std::size_t StyledElementCount(const CompiledDocument& document)
    {
        std::size_t count = 0;

        for (ElementIndex index = 0; index < document.elementCount; ++index)
        {
            if (document.elements[index].styleIndex != 0)
            {
                ++count;
            }
        }

        return count;
    }

    std::vector<std::pair<std::string, std::string>> Sheet(const std::string& text)
    {
        return std::vector<std::pair<std::string, std::string>>{ { "app.css", text } };
    }
}

// ---------------------------------------------------------------------------
// The table
// ---------------------------------------------------------------------------

ROBO_TEST(CssPipeline, AuthoredStylesReachTheCompiledTable)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"card\">Hello</div>",
                                      application,
                                      Sheet(".card { border: 1px solid #cbd5e1; border-radius: 0.5rem; "
                                            "padding: 1rem; background-color: #ffffff; color: #0f172a }"),
                                      error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    // Slot 0 is "no authored style"; the document's one style follows it.
    ROBO_CHECK_EQ(document.styleCount, static_cast<std::uint32_t>(2));
    ROBO_CHECK_EQ(application.Styles().styledElementCount, static_cast<std::size_t>(1));
    ROBO_CHECK_EQ(StyledElementCount(document), static_cast<std::size_t>(1));

    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    const CompiledStyle& style = document.styles[document.elements[index].styleIndex];

    ROBO_CHECK(IsSpecified(style.specified, StyleFlagBorderWidth));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagBorderColor));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagBackground));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagForeground));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagPadding));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagBorderRadius));

    // `0.5rem` is half the root font size, resolved during compilation.
    ROBO_CHECK(SameColor(style.borderColor, Color::Rgba(203, 213, 225), 1));
    ROBO_CHECK(SameColor(style.background, Color::Rgba(255, 255, 255), 1));
    ROBO_CHECK(SameColor(style.foreground, Color::Rgba(15, 23, 42), 1));
    ROBO_CHECK(style.borderWidth.top == 1.0f);
    ROBO_CHECK(style.borderWidth.left == 1.0f);
    ROBO_CHECK(style.borderRadius >= 7.9f && style.borderRadius <= 8.1f);
    ROBO_CHECK(style.padding.top >= 15.9f && style.padding.top <= 16.1f);
}

ROBO_TEST(CssPipeline, TextPropertiesReachTheCompiledTable)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"note\">hello world</div>",
        application,
        Sheet(".note { font-family: 'Inter', sans-serif; font-style: italic; line-height: 1.5; "
              "letter-spacing: 2px; word-spacing: 4px; text-transform: uppercase; "
              "white-space: nowrap; text-indent: 8px; direction: rtl }"),
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    const CompiledStyle& style = document.styles[document.elements[index].styleIndex];

    ROBO_CHECK(IsSpecified(style.specified, StyleFlagFontFamily));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagFontStyle));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagLineHeight));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagLetterSpacing));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagWordSpacing));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagTextTransform));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagWhiteSpace));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagTextIndent));
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagDirection));

    ROBO_CHECK(std::string(StringAt(document, style.fontFamily)) == "Inter, sans-serif");
    ROBO_CHECK(style.fontStyle == FontSlantKind::Italic);
    ROBO_CHECK(style.lineHeight >= 1.49f && style.lineHeight <= 1.51f);
    ROBO_CHECK(style.letterSpacing == 2.0f);
    ROBO_CHECK(style.wordSpacing == 4.0f);
    ROBO_CHECK(style.textTransform == TextTransformKind::Uppercase);
    ROBO_CHECK(style.whiteSpace == WhiteSpaceKind::NoWrap);
    ROBO_CHECK(style.textIndent == 8.0f);
    ROBO_CHECK(style.direction == TextDirectionKind::RightToLeft);
}

ROBO_TEST(CssPipeline, IdenticalComputedStylesAreDeduplicated)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"note\">a</div><div class=\"note\">b</div>"
                                      "<p class=\"note\">c</p>",
                                      application,
                                      Sheet(".note { padding: 4px; background-color: #eeeeee }"),
                                      error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    // Three elements, one style: the table is indexed by computed style, not by
    // element, so a document of ten thousand identical rows carries one entry.
    ROBO_CHECK_EQ(document.styleCount, static_cast<std::uint32_t>(2));
    ROBO_CHECK_EQ(StyledElementCount(document), static_cast<std::size_t>(3));

    const ElementIndex first = StyledElement(document, 0);
    const ElementIndex second = StyledElement(document, 1);
    const ElementIndex third = StyledElement(document, 2);

    ROBO_CHECK_EQ(document.elements[first].styleIndex, document.elements[second].styleIndex);
    ROBO_CHECK_EQ(document.elements[second].styleIndex, document.elements[third].styleIndex);
}

ROBO_TEST(CssPipeline, UnspecifiedFieldsKeepTheNativeDefault)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // Only a colour is authored; the native look of the control has to survive.
    ROBO_CHECK(CompileBodyWithSources("<button class=\"primary\">Save</button>",
                                      application,
                                      Sheet(".primary { background-color: #2563eb }"),
                                      error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    TestRenderer renderer(document);
    ROBO_CHECK(renderer.initialized);
    ROBO_CHECK(renderer.Draw());

    const CompiledStyle& merged = renderer.renderer.Layout().StyleOf(index);

    // The authored colour replaced the default background...
    ROBO_CHECK(SameColor(merged.background, Color::Rgba(37, 99, 235), 1));

    // ...and the border the button category normally has is still there,
    // because the style said nothing about it.
    ROBO_CHECK(merged.borderWidth.top > 0.0f);
    ROBO_CHECK(merged.fontSize > 0.0f);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

ROBO_TEST(CssPipeline, PaddingChangesTheBoxGeometry)
{
    RoboTools::CompiledApplication plain;
    RoboTools::CompiledApplication padded;
    std::string error;

    // The same document twice: the only difference is one declaration. The class
    // is declared in both sheets because a class token must resolve before a
    // document compiles at all.
    ROBO_CHECK(CompileBodyWithSources("<div class=\"box\">text</div>", plain,
                                      Sheet(".box { color: #000000 }"), error));
    ROBO_CHECK(error.empty());
    ROBO_CHECK(CompileBodyWithSources("<div class=\"box\">text</div>", padded,
                                      Sheet(".box { color: #000000; padding: 1rem }"), error));
    ROBO_CHECK(error.empty());

    TestRenderer plainRenderer(plain.Document());
    TestRenderer paddedRenderer(padded.Document());
    ROBO_CHECK(plainRenderer.Draw());
    ROBO_CHECK(paddedRenderer.Draw());

    // Both documents carry exactly one styled element: the div under test. Naming
    // it by its authored style keeps the test independent of how deep the
    // document envelope happens to be.
    const ElementIndex plainBox = StyledElement(plain.Document());
    const ElementIndex paddedBox = StyledElement(padded.Document());

    ROBO_CHECK(plainBox != NoElement);
    ROBO_CHECK(paddedBox != NoElement);

    if (plainBox == NoElement || paddedBox == NoElement)
    {
        return;
    }

    Rect plainRect;
    Rect paddedRect;
    plainRenderer.renderer.Hits().ElementBounds(plainBox, plainRect);
    paddedRenderer.renderer.Hits().ElementBounds(paddedBox, paddedRect);

    // `padding: 1rem` is 16 logical units on every side. A block fills the width
    // it is given, so the padding grows the box downward and leaves its outer
    // width alone - which is what CSS says and what the test is checking.
    const float widthDifference = paddedRect.width - plainRect.width;
    const float heightDifference = paddedRect.height - plainRect.height;

    ROBO_CHECK(widthDifference >= -0.5f && widthDifference <= 0.5f);
    ROBO_CHECK(heightDifference >= 31.5f && heightDifference <= 32.5f);
}

ROBO_TEST(CssPipeline, AllInheritCarriesTheParentsValuesIntoTheChild)
{
    RoboTools::CompiledApplication plain;
    RoboTools::CompiledApplication inherited;
    std::string error;

    const std::string document = "<div class=\"outer\"><div class=\"inner\">text</div></div>";

    // The parent is padded and red. Padding does not inherit, so the child only
    // has it when `all: inherit` copies the parent's computed values onto it.
    //
    // `display` is declared here for a reason. `all: inherit` copies *every*
    // property, `display` included, and it can only copy what the cascade
    // computed. The cascade's initial `display` is CSS's `inline`; the block
    // look of a `<div>` comes from the renderer's per-element base style, which
    // is this compiler's user-agent sheet and is invisible to the cascade. So
    // the parent states its display, the child copies it, and the box geometry
    // below is the geometry of a block - which is what a browser would do with
    // a user-agent sheet that said `div { display: block }`.
    // Both rules state the same family. An element that declares none takes the
    // cascade's initial `font-family: serif`, so a child that inherits its
    // parent's *computed* family via `all: inherit` would otherwise change face
    // as well as padding, and the box geometry below is about the padding.
    ROBO_CHECK(CompileBodyWithSources(document, plain,
                                      Sheet(".outer { padding: 1rem; color: #ff0000; display: block; "
                                            "font-family: sans-serif } "
                                            ".inner { color: #000000; font-family: sans-serif }"),
                                      error));
    ROBO_CHECK(error.empty());

    ROBO_CHECK(CompileBodyWithSources(document, inherited,
                                      Sheet(".outer { padding: 1rem; color: #ff0000; display: block; "
                                            "font-family: sans-serif } "
                                            ".inner { color: #000000; font-family: sans-serif; "
                                            "all: inherit }"),
                                      error));
    ROBO_CHECK(error.empty());

    // The child is the second styled element: the parent is first in document
    // order.
    const ElementIndex plainInner = StyledElement(plain.Document(), 1);
    const ElementIndex inheritedInner = StyledElement(inherited.Document(), 1);

    ROBO_CHECK(plainInner != NoElement);
    ROBO_CHECK(inheritedInner != NoElement);

    if (plainInner == NoElement || inheritedInner == NoElement)
    {
        return;
    }

    const CompiledDocument& plainDocument = plain.Document();
    const CompiledDocument& inheritedDocument = inherited.Document();
    const CompiledStyle& plainStyle = plainDocument.styles[plainDocument.elements[plainInner].styleIndex];
    const CompiledStyle& inheritedStyle =
        inheritedDocument.styles[inheritedDocument.elements[inheritedInner].styleIndex];

    // The padding arrived from the parent, in the field the layout engine reads.
    ROBO_CHECK_EQ(plainStyle.padding.top, 0.0f);
    ROBO_CHECK(inheritedStyle.padding.top >= 15.9f && inheritedStyle.padding.top <= 16.1f);
    ROBO_CHECK(IsSpecified(inheritedStyle.specified, StyleFlagPadding));

    // `all` is written after `color: #000000`, and the last declaration wins, so
    // the child is the parent's red.
    ROBO_CHECK(IsSpecified(inheritedStyle.specified, StyleFlagForeground));
    ROBO_CHECK(SameColor(inheritedStyle.foreground, Color::Rgba(0xff, 0x00, 0x00), 1));

    // And the padding is not merely recorded: it is the geometry the renderer
    // lays out with.
    TestRenderer plainRenderer(plain.Document());
    TestRenderer inheritedRenderer(inherited.Document());
    ROBO_CHECK(plainRenderer.Draw());
    ROBO_CHECK(inheritedRenderer.Draw());

    Rect plainRect;
    Rect inheritedRect;
    plainRenderer.renderer.Hits().ElementBounds(plainInner, plainRect);
    inheritedRenderer.renderer.Hits().ElementBounds(inheritedInner, inheritedRect);

    // One rem of padding above and below is 16 logical units each, and the
    // child is a block, so copying the parent's padding grew it by both.
    const float heightDifference = inheritedRect.height - plainRect.height;

    ROBO_CHECK(heightDifference >= 31.5f && heightDifference <= 32.5f);
    ROBO_CHECK(inheritedRect.width > 0.0f);
}

ROBO_TEST(CssPipeline, PercentageWidthResolvesAgainstTheContainingBlock)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"half\">x</div>", application,
                                      Sheet(".half { width: 50% }"), error));
    ROBO_CHECK(error.empty());

    TestRenderer renderer(application.Document(), 800.0f, 600.0f);
    ROBO_CHECK(renderer.Draw());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    Rect box;
    renderer.renderer.Hits().ElementBounds(index, box);

    // The containing block is the body's content width, which is smaller than the
    // viewport: a percentage is a fraction of the containing block, and layout is
    // the only place that knows it.
    ROBO_CHECK(box.width > 300.0f);
    ROBO_CHECK(box.width < 400.0f);

    const CompiledStyle& style = renderer.renderer.Layout().StyleOf(index);
    ROBO_CHECK(style.width.type == LayoutValueType::ParentRelative);
    ROBO_CHECK(style.width.value > 0.49f && style.width.value < 0.51f);
}

ROBO_TEST(CssPipeline, ViewportWidthResolvesAtRuntime)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"half\">x</div>", application,
                                      Sheet(".half { width: 50vw }"), error));
    ROBO_CHECK(error.empty());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    // The same compiled document, laid out at two widths: a viewport unit is a
    // runtime fact or it is nothing.
    TestRenderer narrow(application.Document(), 800.0f, 600.0f);
    TestRenderer wide(application.Document(), 1200.0f, 600.0f);
    ROBO_CHECK(narrow.Draw());
    ROBO_CHECK(wide.Draw());

    Rect narrowBox;
    Rect wideBox;
    narrow.renderer.Hits().ElementBounds(index, narrowBox);
    wide.renderer.Hits().ElementBounds(index, wideBox);

    ROBO_CHECK(narrowBox.width > 395.0f && narrowBox.width < 405.0f);
    ROBO_CHECK(wideBox.width > 595.0f && wideBox.width < 605.0f);
}

ROBO_TEST(CssPipeline, MinimumWidthIsHonoured)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<span class=\"tiny\">a</span>", application,
                                      Sheet(".tiny { display: block; min-width: 300px }"), error));
    ROBO_CHECK(error.empty());

    TestRenderer renderer(application.Document());
    ROBO_CHECK(renderer.Draw());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    if (index == NoElement)
    {
        return;
    }

    Rect box;
    renderer.renderer.Hits().ElementBounds(index, box);

    ROBO_CHECK(box.width >= 300.0f);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

ROBO_TEST(CssPipeline, BackgroundColorReachesThePixels)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"banner\">Hello</div>", application,
                                      Sheet(".banner { background-color: #ff0000; padding: 8px }"), error));
    ROBO_CHECK(error.empty());

    TestRenderer renderer(application.Document(), 400.0f, 300.0f);
    ROBO_CHECK(renderer.Draw());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    Rect bounds;
    renderer.renderer.Hits().ElementBounds(index, bounds);

    // A pixel inside the box: the authored background is what a user sees.
    const std::uint32_t x = static_cast<std::uint32_t>(bounds.x + 4.0f);
    const std::uint32_t y = static_cast<std::uint32_t>(bounds.y + bounds.height * 0.5f);
    ROBO_CHECK(SameColor(renderer.Pixel(x, y), Color::Rgba(255, 0, 0), 2));

    // And it came from the compile: the command carries the colour the cascade
    // resolved, not a string the runtime parsed.
    const PaintCommand* background = FindCommand(renderer.renderer, PaintCommandType::FillRect, index);
    ROBO_CHECK(background != nullptr);

    if (background != nullptr)
    {
        ROBO_CHECK(SameColor(background->color, Color::Rgba(255, 0, 0), 2));
    }
}

ROBO_TEST(CssPipeline, SpecificityDecidesTheRenderedColor)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // `div.box` is one class and one type selector; `.box` is one class alone, so
    // the type selector decides the colour.
    ROBO_CHECK(CompileBodyWithSources("<div class=\"box\">!</div>", application,
                                      Sheet(".box { background-color: #00ff00 } "
                                            "div.box { background-color: #0000ff }"),
                                      error));
    ROBO_CHECK(error.empty());

    TestRenderer renderer(application.Document(), 400.0f, 300.0f);
    ROBO_CHECK(renderer.Draw());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    if (index == NoElement)
    {
        return;
    }

    const PaintCommand* background = FindCommand(renderer.renderer, PaintCommandType::FillRect, index);

    ROBO_CHECK(background != nullptr);

    if (background != nullptr)
    {
        ROBO_CHECK(SameColor(background->color, Color::Rgba(0, 0, 255), 2));
    }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

ROBO_TEST(CssPipeline, InvalidCssFailsTheBuild)
{
    RoboTools::CompiledApplication application;
    std::string error;

    const bool compiled = CompileBodyWithSources("<div class=\"box\">x</div>", application,
                                                 Sheet(".box { width: banana }"), error);

    ROBO_CHECK(!compiled);
    ROBO_CHECK(error.find("RCC") != std::string::npos);
    // The value is named, so the report points at the declaration and not at the
    // stylesheet as a whole.
    ROBO_CHECK(error.find("banana") != std::string::npos);
}

ROBO_TEST(CssPipeline, UnimplementedCssIsReportedNotPretended)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // `touch-action` is a real CSS property whose specification is outside the
    // reference: the compiler knows the name and cannot compile it, so the
    // declaration is reported by name instead of being dropped in silence.
    ROBO_CHECK(CompileBodyWithSources("<div class=\"box\">x</div>", application,
                                      Sheet(".box { touch-action: none; background-color: #123456 }"),
                                      error));

    const std::string diagnostics = application.Diagnostics();

    ROBO_CHECK(diagnostics.find("RCC010") != std::string::npos);
    ROBO_CHECK(diagnostics.find("touch-action") != std::string::npos);

    // What was implemented still applied.
    TestRenderer renderer(application.Document(), 400.0f, 300.0f);
    ROBO_CHECK(renderer.Draw());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    if (index == NoElement)
    {
        return;
    }

    const CompiledStyle& style = renderer.renderer.Layout().StyleOf(index);
    ROBO_CHECK(SameColor(style.background, Color::Rgba(0x12, 0x34, 0x56), 1));
}

ROBO_TEST(CssPipeline, CarriedCssIsKeptWithoutFailingTheBuild)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // Every one of these is defined by the reference and carried by the
    // compiler. None is an error, none is reported, and the compiled declaration
    // beside them still applies: a page is not broken by CSS this compiler does
    // not draw.
    ROBO_CHECK(CompileBodyWithSources("<div class=\"box\">x</div>", application,
                                      Sheet(".box { position: absolute; grid-template-columns: 1fr 1fr; "
                                            "animation-name: fade; opacity: 0.5; background-color: #123456 }"),
                                      error));

    const std::string diagnostics = application.Diagnostics();

    ROBO_CHECK(diagnostics.find("RCC010") == std::string::npos);
    ROBO_CHECK(diagnostics.find("RCC001") == std::string::npos);

    TestRenderer renderer(application.Document(), 400.0f, 300.0f);
    ROBO_CHECK(renderer.Draw());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    if (index == NoElement)
    {
        return;
    }

    const CompiledStyle& style = renderer.renderer.Layout().StyleOf(index);
    ROBO_CHECK(SameColor(style.background, Color::Rgba(0x12, 0x34, 0x56), 1));

    // Carried is not compiled: nothing lowered those declarations, so no layout
    // field was set for them and the box keeps its automatic sizing.
    ROBO_CHECK(style.width.type == LayoutValueType::Auto);
    ROBO_CHECK(style.height.type == LayoutValueType::Auto);
}

ROBO_TEST(CssPipeline, RuntimeStateSelectorsAreReported)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // The class is declared outside the state rule, so the document's class
    // reference resolves and the *state* rule is what the report is about.
    ROBO_CHECK(CompileBodyWithSources("<a class=\"row\" href=\"#x\">a</a>", application,
                                      Sheet(".row { color: #000000 } "
                                            ".row:hover { background-color: #ff0000 }"),
                                      error));

    // The report says why the hover style does nothing rather than leaving the
    // author to wonder.
    ROBO_CHECK(application.Diagnostics().find("RCC010") != std::string::npos);
    ROBO_CHECK(application.Diagnostics().find(":hover") != std::string::npos);

    // The static colour applied; the hover background did not.
    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    if (index != NoElement)
    {
        const CompiledStyle& style = application.Document().styles[application.Document().elements[index].styleIndex];
        ROBO_CHECK(IsSpecified(style.specified, StyleFlagForeground));
        ROBO_CHECK(!IsSpecified(style.specified, StyleFlagBackground));
    }
}

ROBO_TEST(CssPipeline, MediaQueryDecidesWhatIsCompiled)
{
    RoboTools::CompiledApplication application;
    std::string error;

    RoboTools::HarnessOptions options;
    options.applicationName = "RendererTest";
    options.documentPath = "RendererTest.html";
    // The class is declared outside the queries so the document's class reference
    // resolves; which *declaration* applies is what the queries decide.
    options.styleSheets = Sheet(".wide { color: #000000 } "
                                "@media (min-width: 600px) { .wide { background-color: #00ff00 } } "
                                "@media (min-width: 6000px) { .wide { background-color: #ff0000 } }");
    options.viewportWidth = 1024.0f;
    options.viewportHeight = 768.0f;

    ROBO_CHECK(application.Build(RoboTools::WrapFragment("<div class=\"wide\">x</div>"), options, error));
    ROBO_CHECK(error.empty());

    const ElementIndex index = StyledElement(application.Document());
    ROBO_CHECK(index != NoElement);

    TestRenderer renderer(application.Document(), 400.0f, 300.0f);
    ROBO_CHECK(renderer.Draw());

    if (index == NoElement)
    {
        return;
    }

    const CompiledStyle& style = renderer.renderer.Layout().StyleOf(index);
    ROBO_CHECK(SameColor(style.background, Color::Rgba(0, 255, 0), 1));
}

ROBO_TEST(CssPipeline, DocumentWithoutCssHasNoAuthoredStyles)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<p>plain</p>", application, error));
    ROBO_CHECK(error.empty());

    // The table holds its empty slot 0 and nothing else: no element carries an
    // authored style, so the renderer answers with the native defaults.
    ROBO_CHECK_EQ(application.Styles().styles.size(), static_cast<std::size_t>(1));
    ROBO_CHECK_EQ(StyledElementCount(application.Document()), static_cast<std::size_t>(0));

    TestRenderer renderer(application.Document());
    ROBO_CHECK(renderer.Draw());

    for (ElementIndex index = 0; index < application.Document().elementCount; ++index)
    {
        ROBO_CHECK_EQ(application.Document().elements[index].styleIndex, static_cast<std::uint16_t>(0));
    }
}

ROBO_TEST(CssPipeline, ATextRunTakesTheTextStyleOfTheElementItFlowedOutOf)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // Mixed content is the case that splits an element's text into its own
    // runs: the paragraph's type has to reach the words on either side of the
    // inline element, or a sentence is set at two sizes at once.
    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"t\">lead <span>and inline</span> done</div>",
        application,
        Sheet(".t { font-size: 40px; letter-spacing: 2px; padding: 1rem; background-color: #ffffff }"),
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    StyleTableProvider styles;

    std::size_t runCount = 0;

    for (ElementIndex index = 0; index < document.elementCount; ++index)
    {
        const CompiledElement& element = document.elements[index];

        if (!element.HasFlag(ElementFlagTextRun))
        {
            continue;
        }

        ++runCount;

        const CompiledStyle& style = styles.StyleFor(document, index, CategoryFor(element.kind));

        // The inherited text properties arrive...
        ROBO_CHECK(IsSpecified(style.specified, StyleFlagFontSize));
        ROBO_CHECK(style.fontSize == 40.0f);
        ROBO_CHECK(style.letterSpacing == 2.0f);

        // ...and the box properties do not: a run of text sits in the block's
        // padding rather than carrying a copy of it.
        ROBO_CHECK(style.padding.top == 0.0f);
        ROBO_CHECK(style.padding.left == 0.0f);
    }

    ROBO_CHECK_EQ(runCount, static_cast<std::size_t>(2));
}

ROBO_TEST(CssPipeline, ADecorationReachesTheTextItIsDrawnAround)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // `text-decoration` is not inherited - it is drawn around the descendants of
    // the element that states it - so every piece of the paragraph reports it:
    // the runs either side of the nested element and the element's own text.
    ROBO_CHECK(CompileBodyWithSources(
        "<p class=\"d\">plain <span>nested</span> plain</p>",
        application,
        Sheet(".d { text-decoration: underline; text-decoration-color: #c2410c }"),
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    StyleTableProvider styles;

    std::size_t decorated = 0;

    for (ElementIndex index = 0; index < document.elementCount; ++index)
    {
        const CompiledElement& element = document.elements[index];

        // Only the elements that draw text: the paragraph, the run beside the
        // span, and the span.
        const CompiledStyle& style = styles.StyleFor(document, index, CategoryFor(element.kind));

        if (!IsSpecified(style.specified, StyleFlagTextDecoration))
        {
            continue;
        }

        ROBO_CHECK(style.textDecoration == TextDecorationUnderline);
        ROBO_CHECK(IsSpecified(style.specified, StyleFlagTextDecorationColor));
        ROBO_CHECK(SameColor(style.textDecorationColor, Color::Rgba(194, 65, 12), 1));
        ++decorated;
    }

    ROBO_CHECK_EQ(decorated, static_cast<std::size_t>(4));
}

// ---------------------------------------------------------------------------
// The values the reference names
//
// Each of these is a value an MDN property page documents that the compiler had
// no answer for: it was refused as unknown, or it computed to the wrong thing.
// They are checked at the compiled table, because a value that parses but lowers
// to the wrong kind is not implemented, only accepted.
// ---------------------------------------------------------------------------

ROBO_TEST(CssPipeline, WhiteSpaceLonghandCombinationsComputeToTheClassicKeywords)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // `white-space` is a shorthand for `white-space-collapse` and
    // `text-wrap-mode`, so each of these is the pair the page gives, spelled the
    // way the page spells it.
    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"a\">a</div><div class=\"b\">b</div><div class=\"c\">c</div>"
        "<div class=\"d\">d</div><div class=\"e\">e</div><div class=\"f\">f</div><div class=\"g\">g</div>",
        application,
        Sheet(".a { white-space: break-spaces }"
              ".b { white-space: preserve }"
              ".c { white-space: preserve nowrap }"
              ".d { white-space: collapse }"
              ".e { white-space: wrap }"
              ".f { white-space: pre-wrap }"
              ".g { white-space: pre-line }"),
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    const WhiteSpaceKind expected[7] = {
        WhiteSpaceKind::PreWrap, // break-spaces: keep the spaces, wrap.
        WhiteSpaceKind::PreWrap, // preserve: the same, said the longhand way.
        WhiteSpaceKind::Pre,     // preserve nowrap: the classic `pre`.
        WhiteSpaceKind::Normal,  // collapse: collapse and wrap, the initial pair.
        WhiteSpaceKind::Normal,  // wrap: the wrap half on its own.
        WhiteSpaceKind::PreWrap,
        WhiteSpaceKind::PreLine
    };

    for (std::size_t ordinal = 0; ordinal < 7; ++ordinal)
    {
        const ElementIndex index = StyledElement(document, ordinal);
        ROBO_CHECK(index != NoElement);

        const CompiledStyle& style = document.styles[document.elements[index].styleIndex];
        ROBO_CHECK(IsSpecified(style.specified, StyleFlagWhiteSpace));
        ROBO_CHECK(style.whiteSpace == expected[ordinal]);
    }
}

ROBO_TEST(CssPipeline, ObliqueIsTheOtherSlantedFontStyle)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"slant\">slant</div>",
                                      application,
                                      Sheet(".slant { font-style: oblique }"),
                                      error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    const CompiledStyle& style = document.styles[document.elements[index].styleIndex];
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagFontStyle));
    ROBO_CHECK(style.fontStyle == FontSlantKind::Italic);
}

ROBO_TEST(CssPipeline, TheDeprecatedWritingModeAndTextOrientationSpellingsComputeToTheirModernValues)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"a\">a</div><div class=\"b\">b</div>"
        "<div class=\"c\">c</div><div class=\"d\">d</div>",
        application,
        Sheet(".a { writing-mode: tb-rl }"
              ".b { writing-mode: lr-tb }"
              ".c { text-orientation: sideways-right }"
              ".d { text-orientation: use-glyph-orientation }"),
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    const ElementIndex verticalRl = StyledElement(document, 0);
    const ElementIndex horizontalTb = StyledElement(document, 1);
    const ElementIndex sideways = StyledElement(document, 2);
    const ElementIndex mixed = StyledElement(document, 3);

    ROBO_CHECK(verticalRl != NoElement);
    ROBO_CHECK(horizontalTb != NoElement);
    ROBO_CHECK(sideways != NoElement);
    ROBO_CHECK(mixed != NoElement);

    ROBO_CHECK(document.styles[document.elements[verticalRl].styleIndex].writingMode ==
               WritingModeKind::VerticalRl);
    ROBO_CHECK(document.styles[document.elements[horizontalTb].styleIndex].writingMode ==
               WritingModeKind::HorizontalTb);
    ROBO_CHECK(document.styles[document.elements[sideways].styleIndex].textOrientation ==
               TextOrientationKind::Sideways);
    ROBO_CHECK(document.styles[document.elements[mixed].styleIndex].textOrientation ==
               TextOrientationKind::Mixed);
}

ROBO_TEST(CssPipeline, OverflowOverlayComputesToAuto)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"scroller\">text</div>",
                                      application,
                                      Sheet(".scroller { overflow: overlay }"),
                                      error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    const CompiledStyle& style = document.styles[document.elements[index].styleIndex];

    // `overlay` is the deprecated spelling of `auto`, which is a scroll
    // container that reveals its scrollbars only when they are needed.
    ROBO_CHECK(IsSpecified(style.specified, StyleFlagOverflowX));
    ROBO_CHECK(style.overflowX == OverflowKind::Scroll);
    ROBO_CHECK(style.overflowY == OverflowKind::Scroll);
}

ROBO_TEST(CssPipeline, WordBreakAutoPhraseAndTextJustifyDistributeAreAccepted)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources("<div class=\"a\">a b</div><div class=\"b\">a b</div>",
                                      application,
                                      Sheet(".a { word-break: auto-phrase }"
                                            ".b { text-align: justify; text-justify: distribute }"),
                                      error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();

    const ElementIndex phrase = StyledElement(document, 0);
    const ElementIndex distribute = StyledElement(document, 1);
    ROBO_CHECK(phrase != NoElement);
    ROBO_CHECK(distribute != NoElement);

    const CompiledStyle& phraseStyle = document.styles[document.elements[phrase].styleIndex];
    ROBO_CHECK(IsSpecified(phraseStyle.specified, StyleFlagWordBreak));
    ROBO_CHECK(phraseStyle.wordBreak == WordBreakKind::Normal);

    const CompiledStyle& distributeStyle = document.styles[document.elements[distribute].styleIndex];
    ROBO_CHECK(IsSpecified(distributeStyle.specified, StyleFlagTextJustify));
    ROBO_CHECK(distributeStyle.textJustify == TextJustifyKind::InterWord);
}

ROBO_TEST(CssPipeline, TheNumericFunctionsReachTheCompiledStyle)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // A function that folds during compilation is a plain length by the time the
    // style is compiled, and the layout engine never sees it. The nested call is
    // the point: a function inside a function used to truncate the declaration.
    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"box\">x</div>",
        application,
        Sheet(".box { width: calc(round(up, 1.5px) * 2 + hypot(2px, 2px)); "
              "height: min(20px, 30px); "
              "padding-left: clamp(1px, 0px, 5px); "
              "margin-top: calc(abs(-4px)); "
              "border-top-width: sqrt(9px) }"),
        error));
    ROBO_CHECK(error.empty());

    const CompiledDocument& document = application.Document();
    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    if (index == NoElement)
    {
        return;
    }

    const CompiledStyle& style = document.styles[document.elements[index].styleIndex];

    // 2px * 2 + sqrt(8)px, which is the value a browser computes.
    ROBO_CHECK(style.width.type == LayoutValueType::Fixed);
    ROBO_CHECK(std::fabs(style.width.value - (4.0f + 2.8284271f)) < 0.001f);
    ROBO_CHECK(style.height.type == LayoutValueType::Fixed);
    ROBO_CHECK(std::fabs(style.height.value - 20.0f) < 0.001f);
    ROBO_CHECK(std::fabs(style.padding.left - 1.0f) < 0.001f);
    ROBO_CHECK(std::fabs(style.margin.top - 4.0f) < 0.001f);
    ROBO_CHECK(std::fabs(style.borderWidth.top - 3.0f) < 0.001f);
}

ROBO_TEST(CssPipeline, AMathExpressionTheRuntimeCannotFoldIsReportedNotApplied)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // A length times a length is an area, which is not a value any property this
    // compiler lowers takes. A browser refuses the declaration and the compiler
    // reports it rather than storing a number nothing can resolve.
    ROBO_CHECK(CompileBodyWithSources("<div class=\"box\">x</div>", application,
                                      Sheet(".box { width: calc(2px * 3px); height: 12px }"), error));

    const std::string diagnostics = application.Diagnostics();
    ROBO_CHECK(diagnostics.find("width") != std::string::npos);

    const CompiledDocument& document = application.Document();
    const ElementIndex index = StyledElement(document);
    ROBO_CHECK(index != NoElement);

    if (index == NoElement)
    {
        return;
    }

    const CompiledStyle& style = document.styles[document.elements[index].styleIndex];

    // The refused declaration set nothing, so the box keeps its automatic width,
    // and the declaration beside it still applied.
    ROBO_CHECK(style.width.type == LayoutValueType::Auto);
    ROBO_CHECK(std::fabs(style.height.value - 12.0f) < 0.001f);
}

ROBO_TEST(CssPipeline, CustomPropertiesFeedTheFunctionsAndInherit)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // A custom property is substituted before the value is parsed, so it can
    // stand in for an operand of a function, a whole function, or a fallback -
    // and it inherits from the element it is declared on, which is what makes
    // `var()` a variable rather than an alias for a constant.
    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"outer\" id=\"outer\"><div class=\"inner\" id=\"inner\">x</div></div>",
        application,
        Sheet(".outer { --gap: 8px; --twice: calc(var(--gap) * 2); width: var(--twice); "
              "height: hypot(3px, 4px) }"
              ".inner { width: calc(var(--gap) / 2); height: var(--missing, 7px) }"),
        error));
    ROBO_CHECK(error.empty());

    if (!error.empty())
    {
        RoboTest::Fail(error, __FILE__, __LINE__);
        return;
    }

    const CompiledDocument& document = application.Document();

    const ElementIndex outer = FindElementById(document, "outer");
    const ElementIndex inner = FindElementById(document, "inner");

    ROBO_CHECK(outer != NoElement);
    ROBO_CHECK(inner != NoElement);

    if (outer == NoElement || inner == NoElement)
    {
        return;
    }

    const CompiledStyle& outerStyle = document.styles[document.elements[outer].styleIndex];
    const CompiledStyle& innerStyle = document.styles[document.elements[inner].styleIndex];

    // `calc(var(--gap) * 2)` is 16px, and `hypot(3px, 4px)` is 5px.
    ROBO_CHECK(std::fabs(outerStyle.width.value - 16.0f) < 0.001f);
    ROBO_CHECK(std::fabs(outerStyle.height.value - 5.0f) < 0.001f);

    // The child inherits `--gap`, and the fallback covers the name that is never
    // declared anywhere.
    ROBO_CHECK(std::fabs(innerStyle.width.value - 4.0f) < 0.001f);
    ROBO_CHECK(std::fabs(innerStyle.height.value - 7.0f) < 0.001f);
}
