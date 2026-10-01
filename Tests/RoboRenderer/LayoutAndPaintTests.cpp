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
