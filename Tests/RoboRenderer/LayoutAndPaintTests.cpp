// ---------------------------------------------------------------------------
// Native layout and the paint command stream.
//
// Layout is runtime work: it depends on the window, the measured text and the
// runtime state. These tests check that it responds to all three, that it stays
// separate from painting, and that the display list is the typed stream the
// backends execute.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

#include <algorithm>

using namespace RoboTest;

using namespace RoboRenderer;

ROBO_TEST(Layout, AnInlineElementKeepsTheTextAroundItOnOneLine)
{
    // The compiler lowers each text node beside an inline element into an element
    // of its own. Those runs are inline content exactly like the element between
    // them, so `text <code>x</code>.` is one line box, not three.
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<p id=\"cap\">Bars, a circle with a stroke, and a polyline "
                           "\xE2\x80\x94 one inline <code>svg</code>.</p>",
                           application, error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 800.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex caption = FindElementById(document, "cap");
    ROBO_CHECK(caption != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();

    std::vector<LineLayout> runs;
    for (const LineLayout& line : layout.lines)
    {
        if (line.container == caption)
        {
            runs.push_back(line);
        }
    }

    std::sort(runs.begin(), runs.end(), [](const LineLayout& left, const LineLayout& right) {
        return left.x < right.x;
    });

    ROBO_CHECK(runs.size() >= 3);

    if (runs.size() >= 3)
    {
        // One line box: every run sits on the same line top.
        for (const LineLayout& run : runs)
        {
            ROBO_CHECK(run.lineTop > runs.front().lineTop - 0.01f);
            ROBO_CHECK(run.lineTop < runs.front().lineTop + 0.01f);
        }

        // And they follow one another across it rather than each starting a line.
        for (std::size_t index = 1; index < runs.size(); ++index)
        {
            ROBO_CHECK(runs[index].x > runs[index - 1].x + 1.0f);
        }
    }
}

ROBO_TEST(Layout, RootFillsTheViewportAndFollowsAResize)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div><p>Some text in a container.</p></div>", application, error));

    TestRenderer window(application.Document(), 800.0f, 600.0f);
    ROBO_CHECK(window.initialized);
    ROBO_CHECK(window.Frame());

    const ElementLayout* root = window.renderer.Layout().Result().Element(application.Document().root);
    ROBO_CHECK(root != nullptr);

    if (root != nullptr)
    {
        ROBO_CHECK_EQ(root->border.width, 800.0f);
        ROBO_CHECK(root->border.height >= 600.0f);
    }

    // The same document, laid out against a wider window, is wider: this is the
    // runtime dependence the whole architecture is built around.
    ROBO_CHECK(window.renderer.Resize(1200.0f, 800.0f));
    ROBO_CHECK(window.Frame());

    const ElementLayout* resized = window.renderer.Layout().Result().Element(application.Document().root);
    ROBO_CHECK(resized != nullptr);

    if (resized != nullptr)
    {
        ROBO_CHECK_EQ(resized->border.width, 1200.0f);
        ROBO_CHECK(resized->border.height >= 800.0f);
    }

    // And the device target followed the logical size.
    ROBO_CHECK_EQ(window.Width(), static_cast<std::uint32_t>(1200));
    ROBO_CHECK_EQ(window.Height(), static_cast<std::uint32_t>(800));
}

ROBO_TEST(Layout, NarrowerWindowsRewrapTextIntoMoreLines)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody(
        "<p>Text that has to wrap when the window is narrow, and does not when it is wide, because line breaking is "
        "measurement driven.</p>",
        application, error));

    TestRenderer narrow(application.Document(), 240.0f, 400.0f);
    ROBO_CHECK(narrow.Frame());
    const std::size_t narrowLines = narrow.renderer.Layout().Result().lines.size();

    TestRenderer wide(application.Document(), 1200.0f, 400.0f);
    ROBO_CHECK(wide.Frame());
    const std::size_t wideLines = wide.renderer.Layout().Result().lines.size();

    ROBO_CHECK(narrowLines > wideLines);
    ROBO_CHECK(narrowLines > 1);
}

ROBO_TEST(Layout, BlockChildrenStackAndStayInsideTheirParent)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div id=\"outer\"><h1 id=\"title\">Title</h1><p id=\"body\">Body text</p></div>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 800.0f, 600.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex outer = FindElementById(document, "outer");
    const ElementIndex title = FindElementById(document, "title");
    const ElementIndex body = FindElementById(document, "body");

    ROBO_CHECK(outer != NoElement);
    ROBO_CHECK(title != NoElement);
    ROBO_CHECK(body != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const ElementLayout* outerBox = layout.Element(outer);
    const ElementLayout* titleBox = layout.Element(title);
    const ElementLayout* bodyBox = layout.Element(body);

    ROBO_CHECK(outerBox != nullptr);
    ROBO_CHECK(titleBox != nullptr);
    ROBO_CHECK(bodyBox != nullptr);

    if (outerBox != nullptr && titleBox != nullptr && bodyBox != nullptr)
    {
        // Blocks stack: the paragraph is below the heading.
        ROBO_CHECK(bodyBox->border.y >= titleBox->border.Bottom());

        // Children live inside the parent's content box, in document coordinates.
        ROBO_CHECK(titleBox->border.x >= outerBox->content.x - 0.001f);
        ROBO_CHECK(titleBox->border.Right() <= outerBox->content.Right() + 0.001f);
    }
}

ROBO_TEST(Layout, LineFragmentsCarryTheirOwnFontAndBaseline)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<p>Regular <strong>strong</strong> regular</p>", application, error));

    TestRenderer window(application.Document(), 600.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const LayoutResult& layout = window.renderer.Layout().Result();
    ROBO_CHECK(!layout.lines.empty());

    for (const LineLayout& line : layout.lines)
    {
        ROBO_CHECK(line.fontSize > 0.0f);
        ROBO_CHECK(line.lineHeight >= line.fontSize);
        ROBO_CHECK(line.baseline > line.lineTop);
        ROBO_CHECK(line.endByte >= line.startByte);
        ROBO_CHECK(line.element != NoElement);
    }
}

ROBO_TEST(Layout, ImagesUseTheirDeclaredSizeAndTheirResourceSize)
{
    RoboTools::CompiledApplication application;
    std::string error;

    // No image files exist for this fragment, so the declared size is what the
    // layout engine has to work with.
    ROBO_CHECK(CompileBody("<img id=\"declared\" src=\"missing.png\" alt=\"a\" width=\"120\" height=\"80\">"
                           "<img id=\"intrinsic\" src=\"missing.png\" alt=\"b\">",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 600.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const LayoutResult& layout = window.renderer.Layout().Result();

    const ElementIndex declared = FindElementById(document, "declared");
    const ElementIndex intrinsic = FindElementById(document, "intrinsic");

    const ElementLayout* declaredBox = layout.Element(declared);
    const ElementLayout* intrinsicBox = layout.Element(intrinsic);

    ROBO_CHECK(declaredBox != nullptr);
    ROBO_CHECK(intrinsicBox != nullptr);

    if (declaredBox != nullptr)
    {
        ROBO_CHECK_EQ(declaredBox->border.width, 120.0f);
        ROBO_CHECK_EQ(declaredBox->border.height, 80.0f);
    }

    if (intrinsicBox != nullptr && declaredBox != nullptr)
    {
        // An image with no declared size still gets a box: a resource that did
        // not load must not collapse to zero and hide the diagnostic.
        ROBO_CHECK(intrinsicBox->border.height > 0.0f);
    }
}

ROBO_TEST(Layout, TablesPlaceCellsSideBySide)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<table><tbody><tr><td id=\"a\">One</td><td id=\"b\">Two</td></tr></tbody></table>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 700.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const LayoutResult& layout = window.renderer.Layout().Result();

    const ElementLayout* first = layout.Element(FindElementById(document, "a"));
    const ElementLayout* second = layout.Element(FindElementById(document, "b"));

    ROBO_CHECK(first != nullptr);
    ROBO_CHECK(second != nullptr);

    if (first != nullptr && second != nullptr)
    {
        ROBO_CHECK(second->border.x >= first->border.Right() - 0.001f);
        ROBO_CHECK_EQ(first->border.y, second->border.y);
    }
}

ROBO_TEST(Layout, HiddenElementsTakeNoSpace)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div id=\"shown\">shown</div><div id=\"hidden\" hidden>hidden</div>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 600.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const LayoutResult& layout = window.renderer.Layout().Result();
    const ElementLayout* hidden = layout.Element(FindElementById(document, "hidden"));

    ROBO_CHECK(hidden != nullptr);

    if (hidden != nullptr)
    {
        ROBO_CHECK((hidden->flags & LayoutFlagDisplayNone) != 0);
        ROBO_CHECK_EQ(hidden->border.height, 0.0f);
    }
}

ROBO_TEST(Layout, AScrollableControlReportsItsExtent)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<textarea rows=\"3\" cols=\"20\">line one\nline two\nline three\nline four\nline five\n"
                           "line six</textarea>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex textArea = FindNth(document, RenderCategory::TextArea);
    ROBO_CHECK(textArea != NoElement);

    const ElementLayout* box = window.renderer.Layout().Result().Element(textArea);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        // Six lines of text in a three line box: the content is taller than the
        // box and the element scrolls, which is runtime state rather than layout.
        ROBO_CHECK(box->scrollHeight > box->content.height);
        ROBO_CHECK((box->flags & LayoutFlagScrolls) != 0);
    }
}

ROBO_TEST(Paint, TheCommandStreamIsTypedAndBalanced)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div id=\"panel\"><h2>Heading</h2><button>Save</button>"
                           "<textarea rows=\"2\">scrolling content here</textarea></div>",
                           application, error));

    TestRenderer window(application.Document(), 640.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const PaintList& list = window.renderer.Paint().List();
    ROBO_CHECK(!list.IsEmpty());

    // Every command is a number and a rectangle: no strings anywhere in the
    // stream, and every type has a name for diagnostics.
    for (const PaintCommand& command : list.Commands())
    {
        ROBO_CHECK(ToString(command.type) != nullptr);
        ROBO_CHECK(ToString(command.type)[0] != '\0');
    }

    // Clip pushes and pops are balanced, which is what keeps the backend's clip
    // stack from leaking between frames.
    std::size_t pushes = 0;
    std::size_t pops = 0;

    for (const PaintCommand& command : list.Commands())
    {
        if (command.type == PaintCommandType::PushClip) ++pushes;
        if (command.type == PaintCommandType::PopClip) ++pops;
    }

    ROBO_CHECK_EQ(pushes, pops);
    ROBO_CHECK(pushes > 0);

    // Every glyph a text command names exists in the glyph array.
    for (const PaintCommand& command : list.Commands())
    {
        if (command.type != PaintCommandType::DrawText)
        {
            continue;
        }

        ROBO_CHECK(command.glyphCount > 0);
        ROBO_CHECK(command.fontSize > 0.0f);
        ROBO_CHECK(static_cast<std::size_t>(command.firstGlyph) + command.glyphCount <= list.GlyphCount());
    }
}

ROBO_TEST(Paint, TextCommandsFollowTheLaidOutLines)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<p id=\"para\">First line of text that is long enough to wrap twice when narrow.</p>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 220.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex paragraph = FindElementById(document, "para");
    ROBO_CHECK(paragraph != NoElement);

    const LineLayout* firstLine = nullptr;

    window.renderer.Layout().Result().ForEachLine(paragraph, [&](const LineLayout& line) {
        if (firstLine == nullptr)
        {
            firstLine = &line;
        }
    });

    ROBO_CHECK(firstLine != nullptr);

    const PaintCommand* textCommand = FindCommand(window.renderer, PaintCommandType::DrawText, paragraph);
    ROBO_CHECK(textCommand != nullptr);

    if (textCommand != nullptr && firstLine != nullptr)
    {
        // The glyph run starts where the line starts: painting consumes the
        // geometry layout produced rather than recomputing it.
        ROBO_CHECK(std::abs(textCommand->bounds.x - firstLine->x) < 0.001f);
        ROBO_CHECK(textCommand->bounds.y > 0.0f);
    }
}

ROBO_TEST(Paint, ControlsPaintTheirChromeFromRuntimeState)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input id=\"check\" type=\"checkbox\"><input id=\"ranged\" type=\"range\" min=\"0\" "
                           "max=\"100\" value=\"50\"><progress value=\"50\" max=\"100\"></progress>"
                           "<select><option>One</option><option selected>Two</option></select>"
                           "<button type=\"button\">Press</button>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex checkbox = FindElementById(document, "check");
    const ElementIndex range = FindElementById(document, "ranged");

    ROBO_CHECK(checkbox != NoElement);
    ROBO_CHECK(range != NoElement);

    // An unchecked checkbox paints its face but no accent fill.
    std::size_t checkedFills = 0;

    for (const PaintCommand& command : window.renderer.Paint().List().Commands())
    {
        if (command.element == checkbox && command.type == PaintCommandType::FillRoundedRect)
        {
            ++checkedFills;
        }
    }

    ROBO_CHECK_EQ(checkedFills, static_cast<std::size_t>(1));

    // The range paints a groove, a filled part and a thumb.
    std::size_t rangeFills = 0;
    for (const PaintCommand& command : window.renderer.Paint().List().Commands())
    {
        if (command.element == range && command.type == PaintCommandType::FillRoundedRect)
        {
            ++rangeFills;
        }
    }

    ROBO_CHECK(rangeFills >= 2);

    // Checking it changes the pixels without changing the layout.
    const EventResult activation = window.ClickElement(checkbox);
    ROBO_CHECK(activation.paintChanged);
    ROBO_CHECK(!activation.layoutChanged);
    ROBO_CHECK(window.renderer.State().Element(checkbox).HasFlag(RuntimeFlagChecked));

    window.Frame();
    ROBO_CHECK(window.renderer.Metrics().paintRan);
    ROBO_CHECK(!window.renderer.Metrics().layoutRan);

    const FrameMetrics& metrics = window.renderer.Metrics();
    ROBO_CHECK(metrics.damage.width < 640.0f);
}

ROBO_TEST(Paint, HoverRepaintsOnlyTheAffectedRegion)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<button id=\"first\">One</button><p>text</p><p>more text</p>", application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 800.0f, 600.0f);
    ROBO_CHECK(window.Frame());
    ROBO_CHECK(window.renderer.Metrics().fullRepaint);

    const ElementIndex button = FindElementById(document, "first");
    ROBO_CHECK(button != NoElement);

    const EventResult hover = window.HoverElement(button);
    ROBO_CHECK(hover.paintChanged);

    ROBO_CHECK(window.Frame());
    ROBO_CHECK(window.renderer.Metrics().paintRan);

    // Hovering a button moves that button's pixels, not the window's.
    const FrameMetrics& metrics = window.renderer.Metrics();
    ROBO_CHECK(metrics.damage.width > 0.0f);
    ROBO_CHECK(metrics.damage.width < 800.0f);
    ROBO_CHECK(metrics.damage.Intersection(window.renderer.Layout().Result().Element(button)->border).width >
               0.0f);
}

ROBO_TEST(Paint, AnIdleFrameDoesNothingAtAll)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div><p>Static content</p></div>", application, error));

    TestRenderer window(application.Document(), 640.0f, 480.0f);
    ROBO_CHECK(window.Frame());

    const std::uint32_t commandCount = window.renderer.Metrics().commandCount;
    ROBO_CHECK(commandCount > 0);

    // Nothing changed, so the second frame does not lay out, paint or touch the
    // backend.
    ROBO_CHECK(window.Frame());

    const FrameMetrics& metrics = window.renderer.Metrics();
    ROBO_CHECK(!metrics.layoutRan);
    ROBO_CHECK(!metrics.paintRan);
    ROBO_CHECK_EQ(metrics.drawnPixels, static_cast<std::uint64_t>(0));
    ROBO_CHECK_EQ(metrics.commandCount, commandCount);
}

ROBO_TEST(Paint, LayoutChangesRepaintTheWholeWindow)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<details><summary>More</summary><p>Revealed content</p></details>", application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 480.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex summary = FindNth(document, RenderCategory::DisclosureSummary);
    ROBO_CHECK(summary != NoElement);

    const EventResult activation = window.ClickElement(summary);
    ROBO_CHECK(activation.layoutChanged);
    ROBO_CHECK(activation.activated);

    ROBO_CHECK(window.Frame());
    ROBO_CHECK(window.renderer.Metrics().layoutRan);
    ROBO_CHECK(window.renderer.Metrics().fullRepaint);
}

// ---------------------------------------------------------------------------
// The out-of-flow model: `position` and the four offsets.
//
// `position: relative` moves the box and keeps its space; `absolute` and
// `fixed` take the box out of the flow entirely and place it against a
// containing block. These tests pin the two properties CSS states for each:
// where the box ends up, and whether the page still reserves room for it.
// ---------------------------------------------------------------------------

/// The border box of an element in document coordinates.
static Rect BoxOf(const LayoutResult& layout, ElementIndex index)
{
    const ElementLayout* box = layout.Element(index);
    return box != nullptr ? box->border : Rect{};
}

ROBO_TEST(Layout, RelativePositioningMovesTheBoxAndKeepsItsSpace)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div class=\"col\"><div id=\"a\">A</div><div id=\"b\">B</div><div id=\"c\">C</div></div>",
        application,
        { { "probe.css",
            ".col { width: 200px; }\n"
            "div { height: 40px; }\n"
            "#b { position: relative; top: 10px; left: 25px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex first = FindElementById(document, "a");
    const ElementIndex second = FindElementById(document, "b");
    const ElementIndex third = FindElementById(document, "c");
    ROBO_CHECK(first != NoElement && second != NoElement && third != NoElement);

    const Rect firstBox = BoxOf(window.renderer.Layout().Result(), first);
    const Rect secondBox = BoxOf(window.renderer.Layout().Result(), second);
    const Rect thirdBox = BoxOf(window.renderer.Layout().Result(), third);

    // The relative box is shifted by its offsets...
    ROBO_CHECK(secondBox.x > firstBox.x + 20.0f);
    ROBO_CHECK(secondBox.y > firstBox.Bottom() + 9.0f);

    // ...and its space is still reserved, so the next sibling has not moved.
    ROBO_CHECK(thirdBox.y > firstBox.Bottom() + 30.0f);
    ROBO_CHECK(thirdBox.y < firstBox.Bottom() + 50.0f);
    ROBO_CHECK(thirdBox.x < secondBox.x);
}

ROBO_TEST(Layout, AbsolutePositioningResolvesAgainstTheNearestPositionedAncestor)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"outer\"><div id=\"anchor\"><div id=\"abs\">X</div></div></div>",
        application,
        { { "probe.css",
            "#outer { width: 400px; padding: 20px; }\n"
            "#anchor { position: relative; top: 30px; left: 40px; width: 200px; height: 100px; }\n"
            "#abs { position: absolute; top: 15px; left: 12px; width: 50px; height: 20px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 600.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex outer = FindElementById(document, "outer");
    const ElementIndex anchor = FindElementById(document, "anchor");
    const ElementIndex abs = FindElementById(document, "abs");
    ROBO_CHECK(outer != NoElement && anchor != NoElement && abs != NoElement);

    const Rect outerBox = BoxOf(window.renderer.Layout().Result(), outer);
    const Rect anchorBox = BoxOf(window.renderer.Layout().Result(), anchor);
    const Rect absBox = BoxOf(window.renderer.Layout().Result(), abs);

    // The anchor is shifted by its own relative offsets.
    ROBO_CHECK(anchorBox.y > outerBox.y + 25.0f);
    ROBO_CHECK(anchorBox.x > outerBox.x + 35.0f);

    // The absolute box lands at the anchor's padding box plus its offsets. The
    // anchor has no padding, so that box is its content box.
    ROBO_CHECK(absBox.x > anchorBox.x + 11.0f && absBox.x < anchorBox.x + 13.0f);
    ROBO_CHECK(absBox.y > anchorBox.y + 14.0f && absBox.y < anchorBox.y + 16.0f);
    ROBO_CHECK(absBox.width > 49.0f && absBox.width < 51.0f);
}

ROBO_TEST(Layout, OutOfFlowBoxesTakeNoSpaceInTheFlow)
{
    std::string error;

    // Two documents that differ only by an absolutely positioned box between the
    // two runs. The following block must land in the same place either way: an
    // out-of-flow box reserves nothing.
    const std::string style = "#one, #two, #one2, #two2 { height: 30px; }\n"
                              "#floaty { position: absolute; top: 0px; left: 0px; width: 120px; height: 90px; }\n";

    RoboTools::CompiledApplication withBox;
    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"one\">One</div><div id=\"floaty\">F</div><div id=\"two\">Two</div>", withBox,
        { { "probe.css", style } }, error));

    RoboTools::CompiledApplication withoutBox;
    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"one2\">One</div><div id=\"two2\">Two</div>", withoutBox,
        { { "probe.css", style } }, error));

    TestRenderer a(withBox.Document(), 500.0f, 300.0f);
    TestRenderer b(withoutBox.Document(), 500.0f, 300.0f);
    ROBO_CHECK(a.Frame() && b.Frame());

    const ElementIndex followingWith = FindElementById(withBox.Document(), "two");
    const ElementIndex followingWithout = FindElementById(withoutBox.Document(), "two2");
    ROBO_CHECK(followingWith != NoElement && followingWithout != NoElement);

    const Rect followingWithBox = BoxOf(a.renderer.Layout().Result(), followingWith);
    const Rect followingWithoutBox = BoxOf(b.renderer.Layout().Result(), followingWithout);

    ROBO_CHECK_EQ(static_cast<int>(followingWithBox.y), static_cast<int>(followingWithoutBox.y));

    // The box itself is still laid out, against the initial containing block.
    const ElementIndex floaty = FindElementById(withBox.Document(), "floaty");
    ROBO_CHECK(floaty != NoElement);
    ROBO_CHECK(!BoxOf(a.renderer.Layout().Result(), floaty).IsEmpty());
}

ROBO_TEST(Layout, FixedPositioningPinsToTheViewportAndIgnoresScroll)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"tall\">tall</div><div id=\"pin\">P</div>",
        application,
        { { "probe.css",
            "#tall { height: 2000px; }\n"
            "#pin { position: fixed; right: 12px; bottom: 8px; width: 60px; height: 24px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 500.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex pin = FindElementById(document, "pin");
    ROBO_CHECK(pin != NoElement);

    // `right: 12px; width: 60px` against a 500-unit viewport puts the right edge
    // at 488, and `bottom: 8px; height: 24px` against 400 puts the bottom at 392.
    const Rect before = BoxOf(window.renderer.Layout().Result(), pin);
    ROBO_CHECK(before.Right() > 487.0f && before.Right() < 489.0f);
    ROBO_CHECK(before.Bottom() > 391.0f && before.Bottom() < 393.0f);

    // Hit testing the element agrees with where it was placed.
    Rect bounds{};
    ROBO_CHECK(window.renderer.Hits().ElementBounds(pin, bounds));
    ROBO_CHECK_EQ(static_cast<int>(bounds.y), static_cast<int>(before.y));

    // A scroll is a paint-and-hit-test translation, and a fixed box is pinned to
    // the viewport, so neither its geometry nor its bounds may move.
    window.renderer.State().SetScroll(0.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const Rect after = BoxOf(window.renderer.Layout().Result(), pin);
    ROBO_CHECK_EQ(static_cast<int>(after.y), static_cast<int>(before.y));

    Rect scrolledBounds{};
    ROBO_CHECK(window.renderer.Hits().ElementBounds(pin, scrolledBounds));
    ROBO_CHECK_EQ(static_cast<int>(scrolledBounds.y), static_cast<int>(before.y));
}

ROBO_TEST(Paint, AZIndexReordersPaintingAndHitTestingTogether)
{
    // Two positioned boxes on top of each other. The first one names the higher
    // stacking level, so it must paint last even though it comes first in the
    // tree: a stacking level overrides tree order, for painting and for hit
    // testing alike.
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"first\"></div><div id=\"second\"></div>",
        application,
        { { "probe.css",
            "#first, #second { position: absolute; top: 40px; left: 40px; width: 120px; height: 60px; }\n"
            "#first { z-index: 5; background: red; }\n"
            "#second { z-index: 1; background: blue; }\n" } },
        error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex first = FindElementById(document, "first");
    const ElementIndex second = FindElementById(document, "second");
    ROBO_CHECK(first != NoElement && second != NoElement);

    const auto& commands = window.renderer.Paint().List().Commands();

    std::size_t firstPaint = commands.size();
    std::size_t secondPaint = commands.size();

    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        if (commands[index].element == first && firstPaint == commands.size())
        {
            firstPaint = index;
        }

        if (commands[index].element == second && secondPaint == commands.size())
        {
            secondPaint = index;
        }
    }

    ROBO_CHECK(firstPaint < commands.size() && secondPaint < commands.size());

    // The higher level paints later, so whatever it covers is on top.
    ROBO_CHECK(firstPaint > secondPaint);

    // A click at the shared centre lands on the same box that is drawn on top.
    const ElementIndex hit = window.renderer.Hits().HitTest(100.0f, 70.0f);
    ROBO_CHECK_EQ(static_cast<int>(first), static_cast<int>(hit));
}

// Floating a box takes it out of the line flow and lets following content wrap
// around it, while its container still grows to contain it; clearing pushes a
// box below the floats it names. These tests pin those three promises: where
// the float sits, that the lines beside it lose its width, and that a cleared
// box does not.
// ---------------------------------------------------------------------------

ROBO_TEST(Layout, AFloatHugsItsSideAndTheLinesWrapAroundIt)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"box\"><div id=\"f\">F</div>one two three four five six seven eight nine ten "
        "eleven twelve thirteen fourteen fifteen</div>",
        application,
        { { "probe.css",
            "#box { width: 160px; font-size: 16px; }\n"
            "#f { float: left; width: 60px; height: 40px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex box = FindElementById(document, "box");
    const ElementIndex floaty = FindElementById(document, "f");
    ROBO_CHECK(box != NoElement && floaty != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect boxBox = BoxOf(layout, box);
    const Rect floatBox = BoxOf(layout, floaty);

    // The float hugs the container's left edge and keeps its declared size.
    ROBO_CHECK_EQ(static_cast<int>(floatBox.x), static_cast<int>(boxBox.x));
    ROBO_CHECK(floatBox.width > 59.0f && floatBox.width < 61.0f);
    ROBO_CHECK(floatBox.height > 39.0f && floatBox.height < 41.0f);

    // The container grows to contain the float even though the float reserves
    // no room in the flow.
    ROBO_CHECK(boxBox.height >= 40.0f);

    // The first line starts to the right of the float.
    float besideLineX = 1.0e9f;

    for (const LineLayout& line : layout.lines)
    {
        if (line.container == box && line.lineTop < floatBox.Bottom() - 1.0f)
        {
            besideLineX = std::min(besideLineX, line.x);
        }
    }

    ROBO_CHECK(besideLineX >= floatBox.Right() - 1.0f);

    // A line below the float gets the full width back.
    bool regainedFullWidth = false;

    for (const LineLayout& line : layout.lines)
    {
        if (line.container == box && line.lineTop >= floatBox.Bottom() - 1.0f &&
            line.x <= boxBox.x + 1.0f)
        {
            regainedFullWidth = true;
        }
    }

    ROBO_CHECK(regainedFullWidth);
}

ROBO_TEST(Layout, ARightFloatHugsTheRightEdge)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"box\"><div id=\"f\">F</div>text after the float</div>",
        application,
        { { "probe.css",
            "#box { width: 200px; }\n"
            "#f { float: right; width: 60px; height: 30px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex box = FindElementById(document, "box");
    const ElementIndex floaty = FindElementById(document, "f");
    ROBO_CHECK(box != NoElement && floaty != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect boxBox = BoxOf(layout, box);
    const Rect floatBox = BoxOf(layout, floaty);

    ROBO_CHECK(floatBox.Right() > boxBox.Right() - 1.0f);
    ROBO_CHECK(floatBox.Right() < boxBox.Right() + 1.0f);
}

ROBO_TEST(Layout, ClearMovesABlockBelowTheFloatItNames)
{
    std::string error;

    const std::string style = "#box { width: 200px; }\n"
                              "#f { float: left; width: 60px; height: 50px; }\n"
                              "#c { height: 20px; }\n";

    RoboTools::CompiledApplication cleared;
    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"box\"><div id=\"f\">F</div><div id=\"c\">C</div></div>", cleared,
        { { "probe.css", style + "#c { clear: both; }\n" } }, error));

    RoboTools::CompiledApplication beside;
    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"box\"><div id=\"f\">F</div><div id=\"c\">C</div></div>", beside,
        { { "probe.css", style } }, error));

    TestRenderer a(cleared.Document(), 400.0f, 300.0f);
    TestRenderer b(beside.Document(), 400.0f, 300.0f);
    ROBO_CHECK(a.Frame() && b.Frame());

    const ElementIndex floatA = FindElementById(cleared.Document(), "f");
    const ElementIndex blockA = FindElementById(cleared.Document(), "c");
    const ElementIndex floatB = FindElementById(beside.Document(), "f");
    const ElementIndex blockB = FindElementById(beside.Document(), "c");
    ROBO_CHECK(floatA != NoElement && blockA != NoElement && floatB != NoElement && blockB != NoElement);

    const Rect floatBoxA = BoxOf(a.renderer.Layout().Result(), floatA);
    const Rect blockBoxA = BoxOf(a.renderer.Layout().Result(), blockA);
    const Rect floatBoxB = BoxOf(b.renderer.Layout().Result(), floatB);
    const Rect blockBoxB = BoxOf(b.renderer.Layout().Result(), blockB);

    // The cleared block sits below the float...
    ROBO_CHECK(blockBoxA.y >= floatBoxA.Bottom() - 1.0f);

    // ...whereas without clearing, the block would flow past it, because a float
    // reserves no room in the flow.
    ROBO_CHECK(blockBoxB.y < floatBoxB.Bottom() - 1.0f);
}

// A flex container lays its children along one axis rather than stacking or
// flowing them. These tests pin the four promises the compiled model makes:
// items sit side by side in order, justify-content distributes the free space,
// flex-grow shares it by the growth factors, and a column runs down the page.
// ---------------------------------------------------------------------------

ROBO_TEST(Flex, RowItemsSitSideBySideInTreeOrder)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"row\"><div id=\"a\">A</div><div id=\"b\">B</div><div id=\"c\">C</div></div>",
        application,
        { { "probe.css",
            "#row { display: flex; width: 300px; height: 100px; }\n"
            "#a, #b, #c { width: 50px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex row = FindElementById(document, "row");
    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex b = FindElementById(document, "b");
    const ElementIndex c = FindElementById(document, "c");
    ROBO_CHECK(row != NoElement && a != NoElement && b != NoElement && c != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect rowBox = BoxOf(layout, row);
    const Rect aBox = BoxOf(layout, a);
    const Rect bBox = BoxOf(layout, b);
    const Rect cBox = BoxOf(layout, c);

    ROBO_CHECK_EQ(static_cast<int>(aBox.x), static_cast<int>(rowBox.x));
    ROBO_CHECK_EQ(static_cast<int>(bBox.x), static_cast<int>(aBox.x + 50));
    ROBO_CHECK_EQ(static_cast<int>(cBox.x), static_cast<int>(bBox.x + 50));
    ROBO_CHECK(aBox.width > 49.0f && aBox.width < 51.0f);

    // The default `align-items: stretch` fills the container's cross size.
    ROBO_CHECK(aBox.height > 99.0f);
}

ROBO_TEST(Flex, JustifyContentCentersTheItems)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"row\"><div id=\"a\">A</div><div id=\"b\">B</div></div>",
        application,
        { { "probe.css",
            "#row { display: flex; width: 300px; justify-content: center; }\n"
            "#a, #b { width: 50px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex row = FindElementById(document, "row");
    const ElementIndex a = FindElementById(document, "a");
    ROBO_CHECK(row != NoElement && a != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect rowBox = BoxOf(layout, row);
    const Rect aBox = BoxOf(layout, a);

    // 100 of 300 used, so 200 is free and centering puts 100 in front.
    ROBO_CHECK_EQ(static_cast<int>(aBox.x), static_cast<int>(rowBox.x + 100));
}

ROBO_TEST(Flex, GrowSharesTheFreeSpaceByTheGrowthFactors)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"row\"><div id=\"a\">A</div><div id=\"b\">B</div></div>",
        application,
        { { "probe.css",
            "#row { display: flex; width: 300px; }\n"
            "#a { flex-grow: 1; flex-basis: 0px; }\n"
            "#b { flex-grow: 3; flex-basis: 0px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex b = FindElementById(document, "b");
    ROBO_CHECK(a != NoElement && b != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect aBox = BoxOf(layout, a);
    const Rect bBox = BoxOf(layout, b);

    // A zero basis leaves the whole 300 to share one to three.
    ROBO_CHECK_EQ(static_cast<int>(aBox.width), 75);
    ROBO_CHECK_EQ(static_cast<int>(bBox.width), 225);
    ROBO_CHECK_EQ(static_cast<int>(bBox.x), static_cast<int>(aBox.x + 75));
}

ROBO_TEST(Flex, ColumnStacksItemsVerticallyAndOrderReordersThem)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"col\"><div id=\"a\">A</div><div id=\"b\">B</div></div>",
        application,
        { { "probe.css",
            "#col { display: flex; flex-direction: column; width: 200px; }\n"
            "#a { height: 40px; order: 2; }\n"
            "#b { height: 30px; order: 1; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex col = FindElementById(document, "col");
    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex b = FindElementById(document, "b");
    ROBO_CHECK(col != NoElement && a != NoElement && b != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect colBox = BoxOf(layout, col);
    const Rect aBox = BoxOf(layout, a);
    const Rect bBox = BoxOf(layout, b);

    // Order 1 comes first, so `b` is above `a` despite being second in the tree.
    ROBO_CHECK_EQ(static_cast<int>(bBox.y), static_cast<int>(colBox.y));
    ROBO_CHECK_EQ(static_cast<int>(aBox.y), static_cast<int>(bBox.y + 30));

    // The cross axis stretches both items to the container's width.
    ROBO_CHECK(bBox.width > 199.0f && aBox.width > 199.0f);
}

ROBO_TEST(Flex, WrappingBreaksItemsOntoASecondLine)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"row\"><div id=\"a\">A</div><div id=\"b\">B</div><div id=\"c\">C</div></div>",
        application,
        { { "probe.css",
            "#row { display: flex; flex-wrap: wrap; width: 120px; }\n"
            "#a, #b, #c { width: 50px; height: 20px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex row = FindElementById(document, "row");
    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex b = FindElementById(document, "b");
    const ElementIndex c = FindElementById(document, "c");
    ROBO_CHECK(row != NoElement && a != NoElement && b != NoElement && c != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect rowBox = BoxOf(layout, row);
    const Rect aBox = BoxOf(layout, a);
    const Rect bBox = BoxOf(layout, b);
    const Rect cBox = BoxOf(layout, c);

    // Two 50px items fit in 120, so `a` and `b` share the first line...
    ROBO_CHECK_EQ(static_cast<int>(aBox.y), static_cast<int>(bBox.y));
    ROBO_CHECK_EQ(static_cast<int>(bBox.x), static_cast<int>(aBox.x + 50));

    // ...and the third, pushed past the main size, starts a line below it.
    ROBO_CHECK_EQ(static_cast<int>(cBox.x), static_cast<int>(aBox.x));
    ROBO_CHECK_EQ(static_cast<int>(cBox.y), static_cast<int>(aBox.y + 20));
    ROBO_CHECK_EQ(static_cast<int>(aBox.y), static_cast<int>(rowBox.y));
}

ROBO_TEST(Flex, GapSeparatesTheItemsAndTheWrappedLines)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"row\"><div id=\"a\">A</div><div id=\"b\">B</div></div>"
        "<div id=\"wrap\"><div id=\"c\">C</div><div id=\"d\">D</div><div id=\"e\">E</div></div>",
        application,
        { { "probe.css",
            "#row { display: flex; column-gap: 14px; width: 300px; }\n"
            "#a, #b { width: 50px; height: 20px; }\n"
            "#wrap { display: flex; flex-wrap: wrap; row-gap: 12px; width: 120px; }\n"
            "#c, #d, #e { width: 50px; height: 20px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex b = FindElementById(document, "b");
    const ElementIndex c = FindElementById(document, "c");
    const ElementIndex d = FindElementById(document, "d");
    const ElementIndex e = FindElementById(document, "e");
    ROBO_CHECK(a != NoElement && b != NoElement && c != NoElement && d != NoElement && e != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect aBox = BoxOf(layout, a);
    const Rect bBox = BoxOf(layout, b);
    const Rect cBox = BoxOf(layout, c);
    const Rect dBBox = BoxOf(layout, d);
    const Rect eBox = BoxOf(layout, e);

    // `column-gap: 14px` sits between the two items on the line.
    ROBO_CHECK_EQ(static_cast<int>(bBox.x), static_cast<int>(aBox.x + 50 + 14));

    // `row-gap: 12px` sits between the wrapped lines, below the first item.
    ROBO_CHECK_EQ(static_cast<int>(dBBox.x), static_cast<int>(cBox.x + 50));
    ROBO_CHECK_EQ(static_cast<int>(eBox.y), static_cast<int>(cBox.y + 20 + 12));
}

ROBO_TEST(Flex, AlignContentCentersAndStretchesTheWrappedLines)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"center\"><div id=\"a\">A</div><div id=\"b\">B</div><div id=\"c\">C</div></div>"
        "<div id=\"stretch\"><div id=\"d\">D</div><div id=\"e\">E</div><div id=\"f\">F</div></div>",
        application,
        { { "probe.css",
            "#center { display: flex; flex-wrap: wrap; align-content: center; width: 120px; height: 100px; }\n"
            "#stretch { display: flex; flex-wrap: wrap; align-content: stretch; width: 120px; height: 100px; }\n"
            "#a, #b, #c, #d, #e, #f { width: 50px; height: 20px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex center = FindElementById(document, "center");
    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex c = FindElementById(document, "c");
    const ElementIndex stretch = FindElementById(document, "stretch");
    const ElementIndex d = FindElementById(document, "d");
    const ElementIndex f = FindElementById(document, "f");
    ROBO_CHECK(center != NoElement && a != NoElement && c != NoElement);
    ROBO_CHECK(stretch != NoElement && d != NoElement && f != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect centerBox = BoxOf(layout, center);
    const Rect aBox = BoxOf(layout, a);
    const Rect cBox = BoxOf(layout, c);
    const Rect stretchBox = BoxOf(layout, stretch);
    const Rect dBox = BoxOf(layout, d);
    const Rect fBox = BoxOf(layout, f);

    // Two 20px lines leave 60 free; centering puts 30 above the first line.
    ROBO_CHECK_EQ(static_cast<int>(aBox.y), static_cast<int>(centerBox.y + 30));
    ROBO_CHECK_EQ(static_cast<int>(cBox.y), static_cast<int>(aBox.y + 20));

    // `stretch` shares the same 60 between the two lines, so each is 50 tall
    // and the second line starts 50 below the first.
    ROBO_CHECK_EQ(static_cast<int>(dBox.y), static_cast<int>(stretchBox.y));
    ROBO_CHECK_EQ(static_cast<int>(fBox.y), static_cast<int>(dBox.y + 50));
}

ROBO_TEST(Flex, WrapReverseStacksTheLinesFromTheCrossEnd)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"row\"><div id=\"a\">A</div><div id=\"b\">B</div><div id=\"c\">C</div></div>",
        application,
        { { "probe.css",
            "#row { display: flex; flex-wrap: wrap-reverse; align-content: flex-start; "
            "width: 120px; height: 100px; }\n"
            "#a, #b, #c { width: 50px; height: 20px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex row = FindElementById(document, "row");
    const ElementIndex a = FindElementById(document, "a");
    const ElementIndex c = FindElementById(document, "c");
    ROBO_CHECK(row != NoElement && a != NoElement && c != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();
    const Rect rowBox = BoxOf(layout, row);
    const Rect aBox = BoxOf(layout, a);
    const Rect cBox = BoxOf(layout, c);

    // The first line is laid at the cross-end, so the wrapped line sits above it.
    ROBO_CHECK_EQ(static_cast<int>(aBox.y), static_cast<int>(rowBox.y + 80));
    ROBO_CHECK_EQ(static_cast<int>(cBox.y), static_cast<int>(aBox.y - 20));
}

ROBO_TEST(Box, MaximumWidthClampsTheDeclaredWidthAndTheMinimumWins)
{
    // The maximum is applied before the minimum, which is the order CSS states:
    // a minimum larger than a maximum ends up winning.
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"clamped\">A</div><div id=\"floor\">B</div>",
        application,
        { { "probe.css",
            "#clamped { width: 260px; max-width: 120px; height: 20px; }\n"
            "#floor { width: 260px; max-width: 120px; min-width: 200px; height: 20px; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex clamped = FindElementById(document, "clamped");
    const ElementIndex floor = FindElementById(document, "floor");
    ROBO_CHECK(clamped != NoElement && floor != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();

    // 260 is clamped to the 120 maximum, so the box is narrower than it asked.
    ROBO_CHECK_EQ(static_cast<int>(BoxOf(layout, clamped).width), 120);

    // The 200 minimum is above the 120 maximum, and the minimum wins.
    ROBO_CHECK_EQ(static_cast<int>(BoxOf(layout, floor).width), 200);
}

ROBO_TEST(Box, MaximumHeightClampsTheBoxAndNoneLeavesItUnconstrained)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"short\">A</div><div id=\"free\">B</div>",
        application,
        { { "probe.css",
            "#short { width: 100px; height: 90px; max-height: 30px; }\n"
            "#free { width: 100px; height: 90px; max-height: none; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex shortBox = FindElementById(document, "short");
    const ElementIndex freeBox = FindElementById(document, "free");
    ROBO_CHECK(shortBox != NoElement && freeBox != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();

    // 90 is clamped down to the 30 maximum.
    ROBO_CHECK_EQ(static_cast<int>(BoxOf(layout, shortBox).height), 30);

    // `none` is no bound at all, so the declared height stands.
    ROBO_CHECK_EQ(static_cast<int>(BoxOf(layout, freeBox).height), 90);
}

ROBO_TEST(Box, AMaximumOfAPercentageResolvesAgainstTheContainingBlock)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBodyWithSources(
        "<div id=\"parent\"><div id=\"child\">A</div></div>",
        application,
        { { "probe.css",
            "#parent { width: 200px; height: 60px; }\n"
            "#child { width: 200px; height: 20px; max-width: 25%; }\n" } },
        error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex child = FindElementById(document, "child");
    ROBO_CHECK(child != NoElement);

    const LayoutResult& layout = window.renderer.Layout().Result();

    // A quarter of the 200px containing block.
    ROBO_CHECK_EQ(static_cast<int>(BoxOf(layout, child).width), 50);
}
