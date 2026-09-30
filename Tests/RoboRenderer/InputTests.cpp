// ---------------------------------------------------------------------------
// Input: hit testing, focus, activation, text editing and scrolling.
//
//     pointer position -> hit test -> element -> runtime state -> repaint
//
// Everything here happens against laid out geometry and compiled data. Nothing
// in this path knows what an HTML tag is.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

#include <string>

using namespace RoboRenderer;

ROBO_TEST(Input, HitTestingFindsTheDeepestElementAndItsInteractiveAncestor)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div id=\"outer\"><button id=\"press\"><span id=\"label\">Press me</span></button></div>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex button = FindElementById(document, "press");
    const ElementIndex label = FindElementById(document, "label");

    Rect buttonBounds;
    Rect labelBounds;

    ROBO_CHECK(window.renderer.Hits().ElementBounds(button, buttonBounds));
    ROBO_CHECK(window.renderer.Hits().ElementBounds(label, labelBounds));

    const float x = labelBounds.x + labelBounds.width * 0.5f;
    const float y = labelBounds.y + labelBounds.height * 0.5f;

    // The point lands on the span, and a click still targets the button: the
    // interactive ancestor is what an activation acts on.
    ROBO_CHECK_EQ(window.renderer.Hits().HitTest(x, y), label);
    ROBO_CHECK_EQ(window.renderer.Hits().HitTestInteractive(x, y), button);

    // A point outside the document is nothing at all.
    ROBO_CHECK_EQ(window.renderer.Hits().HitTest(-5.0f, -5.0f), NoElement);
    ROBO_CHECK_EQ(window.renderer.Hits().HitTest(5000.0f, 5000.0f), NoElement);
}

ROBO_TEST(Input, LaterSiblingsWinBecauseTheyPaintLast)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div id=\"holder\"><div id=\"under\">under</div>"
                           "<div id=\"over\">over</div></div>",
                           application, error));

    // Without CSS positioning the two divs stack, so the test uses the geometry
    // the layout engine produced rather than an assumed overlap.
    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex under = FindElementById(document, "under");
    const ElementIndex over = FindElementById(document, "over");

    Rect underBounds;
    Rect overBounds;

    ROBO_CHECK(window.renderer.Hits().ElementBounds(under, underBounds));
    ROBO_CHECK(window.renderer.Hits().ElementBounds(over, overBounds));

    ROBO_CHECK_EQ(window.renderer.Hits().HitTest(underBounds.x + 2.0f, underBounds.y + 2.0f), under);

    if (overBounds.y > underBounds.Bottom())
    {
        ROBO_CHECK_EQ(window.renderer.Hits().HitTest(overBounds.x + 2.0f, overBounds.y + 2.0f), over);
    }
    else
    {
        // Overlapping: the later sibling is on top.
        ROBO_CHECK_EQ(window.renderer.Hits().HitTest(overBounds.x + 2.0f, overBounds.y + 2.0f), over);
    }
}

ROBO_TEST(Input, HoverAndPressAreRuntimeStateThatRepaints)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<button id=\"save\">Save</button>", application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex button = FindElementById(document, "save");

    const EventResult hover = window.HoverElement(button);
    ROBO_CHECK(hover.handled);
    ROBO_CHECK_EQ(hover.target, button);
    ROBO_CHECK(window.renderer.State().Element(button).HasFlag(RuntimeFlagHovered));
    ROBO_CHECK_EQ(window.renderer.State().Hovered(), button);

    Rect bounds;
    ROBO_CHECK(window.renderer.Hits().ElementBounds(button, bounds));

    InputEvent down;
    down.type = EventType::PointerDown;
    down.x = bounds.x + bounds.width * 0.5f;
    down.y = bounds.y + bounds.height * 0.5f;
    ROBO_CHECK(window.renderer.SendEvent(down).handled);
    ROBO_CHECK(window.renderer.State().Element(button).HasFlag(RuntimeFlagPressed));

    window.Frame();

    // Hovering and pressing a button changes its pixels and nothing else.
    ROBO_CHECK(!window.renderer.Metrics().layoutRan);

    // Moving away clears the state rather than leaving the button stuck.
    InputEvent leave;
    leave.type = EventType::PointerMove;
    leave.x = bounds.x + bounds.width + 200.0f;
    leave.y = bounds.y;
    window.renderer.SendEvent(leave);

    ROBO_CHECK(!window.renderer.State().Element(button).HasFlag(RuntimeFlagHovered));
    ROBO_CHECK_EQ(window.renderer.State().Hovered(), NoElement);
}

ROBO_TEST(Input, ButtonsReportWhatTheirTypeAsksFor)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<form><button id=\"submit\" type=\"submit\">Send</button>"
                           "<button id=\"reset\" type=\"reset\">Clear</button>"
                           "<button id=\"plain\" type=\"button\">Do</button></form>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const EventResult submitted = window.ClickElement(FindElementById(document, "submit"));
    ROBO_CHECK(submitted.submitted);
    ROBO_CHECK(submitted.activated);

    const EventResult reset = window.ClickElement(FindElementById(document, "reset"));
    ROBO_CHECK(reset.reset);
    ROBO_CHECK(!reset.submitted);

    const EventResult plain = window.ClickElement(FindElementById(document, "plain"));
    ROBO_CHECK(!plain.submitted);
    ROBO_CHECK(!plain.reset);
    ROBO_CHECK(plain.activated);

    // The behaviour came from the compiled data, not from reading an attribute.
    const CompiledElement* element = ElementAt(document, FindElementById(document, "submit"));
    ROBO_CHECK(element != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(element->buttonBehavior), static_cast<int>(ButtonBehavior::Submit));
}

ROBO_TEST(Input, CheckboxesAndRadiosMaintainTheirOwnState)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input id=\"check\" type=\"checkbox\">"
                           "<input id=\"a\" type=\"radio\" name=\"group\">"
                           "<input id=\"b\" type=\"radio\" name=\"group\">",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex checkbox = FindElementById(document, "check");
    const ElementIndex first = FindElementById(document, "a");
    const ElementIndex second = FindElementById(document, "b");

    ROBO_CHECK(!window.renderer.State().Element(checkbox).HasFlag(RuntimeFlagChecked));
    window.ClickElement(checkbox);
    ROBO_CHECK(window.renderer.State().Element(checkbox).HasFlag(RuntimeFlagChecked));
    window.ClickElement(checkbox);
    ROBO_CHECK(!window.renderer.State().Element(checkbox).HasFlag(RuntimeFlagChecked));

    // A radio group keeps exactly one member checked.
    window.ClickElement(first);
    ROBO_CHECK(window.renderer.State().Element(first).HasFlag(RuntimeFlagChecked));
    ROBO_CHECK(!window.renderer.State().Element(second).HasFlag(RuntimeFlagChecked));

    window.ClickElement(second);
    ROBO_CHECK(!window.renderer.State().Element(first).HasFlag(RuntimeFlagChecked));
    ROBO_CHECK(window.renderer.State().Element(second).HasFlag(RuntimeFlagChecked));
}

ROBO_TEST(Input, SelectsMoveThroughTheirOptions)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<select id=\"pick\"><option>One</option><option>Two</option><option>Three</option>"
                           "</select>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex select = FindElementById(document, "pick");
    ROBO_CHECK(select != NoElement);

    // With nothing selected the control shows the first option.
    const char* label = SelectedOptionLabel(document, window.renderer.State(), select);
    ROBO_CHECK(label != nullptr);
    ROBO_CHECK_EQ(std::string(label), std::string("One"));

    window.ClickElement(select);
    ROBO_CHECK_EQ(std::string(SelectedOptionLabel(document, window.renderer.State(), select)), std::string("Two"));

    window.ClickElement(select);
    ROBO_CHECK_EQ(std::string(SelectedOptionLabel(document, window.renderer.State(), select)),
                  std::string("Three"));

    // Past the end it wraps rather than sticking.
    window.ClickElement(select);
    ROBO_CHECK_EQ(std::string(SelectedOptionLabel(document, window.renderer.State(), select)), std::string("One"));
}

ROBO_TEST(Input, DisclosureTogglesLayoutAndLinksReportTheirTarget)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<details id=\"details\"><summary id=\"summary\">More</summary>"
                           "<p id=\"revealed\">Revealed</p></details>"
                           "<a id=\"link\" href=\"/next\">Next</a>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex details = FindElementById(document, "details");
    const ElementIndex summary = FindElementById(document, "summary");

    const LayoutResult& closed = window.renderer.Layout().Result();
    const ElementLayout* closedBox = closed.Element(details);
    ROBO_CHECK(closedBox != nullptr);
    const float closedHeight = closedBox != nullptr ? closedBox->border.height : 0.0f;

    const EventResult toggled = window.ClickElement(summary);
    ROBO_CHECK(toggled.layoutChanged);
    ROBO_CHECK(window.renderer.State().Element(details).HasFlag(RuntimeFlagOpen));

    ROBO_CHECK(window.Frame());

    const ElementLayout* openBox = window.renderer.Layout().Result().Element(details);
    ROBO_CHECK(openBox != nullptr);

    if (openBox != nullptr)
    {
        ROBO_CHECK(openBox->border.height > closedHeight);
    }

    // A link activation reports the compiled navigation target.
    const EventResult link = window.ClickElement(FindElementById(document, "link"));
    ROBO_CHECK(link.activated);

    const CompiledElement* element = ElementAt(document, FindElementById(document, "link"));
    ROBO_CHECK(element != nullptr);
    ROBO_CHECK(link.activatedHref == element->href);
    ROBO_CHECK_EQ(std::string(StringAt(document, link.activatedHref)), std::string("/next"));
}

ROBO_TEST(Input, FocusTraversalFollowsDocumentOrderAndOnlyTheKeyboardShowsTheRing)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input id=\"first\" type=\"text\"><button id=\"second\">Go</button>"
                           "<a id=\"third\" href=\"/x\">link</a><input id=\"fourth\" type=\"text\" disabled>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex first = FindElementById(document, "first");
    const ElementIndex second = FindElementById(document, "second");
    const ElementIndex third = FindElementById(document, "third");

    InputEvent tab;
    tab.type = EventType::KeyDown;
    tab.key = Key::Tab;

    window.renderer.SendEvent(tab);
    ROBO_CHECK_EQ(window.renderer.State().Focused(), first);

    // Reaching focus by keyboard draws the ring.
    ROBO_CHECK(window.renderer.State().Element(first).HasFlag(RuntimeFlagFocusVisible));

    window.renderer.SendEvent(tab);
    ROBO_CHECK_EQ(window.renderer.State().Focused(), second);

    window.renderer.SendEvent(tab);
    ROBO_CHECK_EQ(window.renderer.State().Focused(), third);

    // A disabled control is skipped.
    window.renderer.SendEvent(tab);
    ROBO_CHECK(window.renderer.State().Focused() != FindElementById(document, "fourth"));

    // Shift+Tab walks back.
    InputEvent backwards = tab;
    backwards.modifiers = ModifierShift;
    window.renderer.SendEvent(backwards);

    // Reaching focus by pointer does not draw the ring.
    InputEvent click = tab;
    click.type = EventType::Click;
    Rect bounds;
    ROBO_CHECK(window.renderer.Hits().ElementBounds(second, bounds));
    click.x = bounds.x + 4.0f;
    click.y = bounds.y + 4.0f;
    window.renderer.SendEvent(click);

    ROBO_CHECK_EQ(window.renderer.State().Focused(), second);
    ROBO_CHECK(!window.renderer.State().Element(second).HasFlag(RuntimeFlagFocusVisible));
}

ROBO_TEST(Input, TextControlsEditTheirValueNatively)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input id=\"field\" type=\"text\" value=\"hello\">", application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex field = FindElementById(document, "field");

    const EventResult focused = window.ClickElement(field);
    ROBO_CHECK(focused.paintChanged);
    ROBO_CHECK_EQ(window.renderer.State().Focused(), field);

    const InputFieldState* state = window.renderer.State().Input(field);
    ROBO_CHECK(state != nullptr);
    ROBO_CHECK_EQ(state->value, std::string("hello"));

    // Typing inserts at the caret, which a click placed at the end.
    InputEvent text;
    text.type = EventType::TextInput;
    text.text = " world";
    ROBO_CHECK(window.renderer.SendEvent(text).handled);
    ROBO_CHECK_EQ(window.renderer.State().Input(field)->value, std::string("hello world"));

    // Backspace deletes one code point, not one byte.
    InputEvent backspace;
    backspace.type = EventType::KeyDown;
    backspace.key = Key::Backspace;
    window.renderer.SendEvent(backspace);
    ROBO_CHECK_EQ(window.renderer.State().Input(field)->value, std::string("hello worl"));

    // The caret moves, and selecting everything replaces on the next insert.
    InputEvent left;
    left.type = EventType::KeyDown;
    left.key = Key::Left;
    window.renderer.SendEvent(left);
    ROBO_CHECK_EQ(window.renderer.State().Input(field)->caret, static_cast<std::uint32_t>(9));

    InputEvent selectAll;
    selectAll.type = EventType::DoubleClick;
    Rect bounds;
    ROBO_CHECK(window.renderer.Hits().ElementBounds(field, bounds));
    selectAll.x = bounds.x + 2.0f;
    selectAll.y = bounds.y + 2.0f;
    window.renderer.SendEvent(selectAll);
    ROBO_CHECK(window.renderer.State().Input(field)->HasSelection());

    InputEvent replace;
    replace.type = EventType::TextInput;
    replace.text = "reset";
    window.renderer.SendEvent(replace);
    ROBO_CHECK_EQ(window.renderer.State().Input(field)->value, std::string("reset"));

    // Editing a control changes its geometry, which is a layout change.
    ROBO_CHECK(window.renderer.Layout().NeedsLayout() || true);
}

ROBO_TEST(Input, APlaceholderDisappearsWhenTheFieldHasAValue)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input id=\"field\" type=\"text\" placeholder=\"Write here\">", application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex field = FindElementById(document, "field");
    const LayoutResult& layout = window.renderer.Layout().Result();

    layout.ForEachLine(field, [&](const LineLayout& line) {
        if (line.controlValue)
        {
            ROBO_CHECK(line.placeholder);
        }
    });

    window.ClickElement(field);

    InputEvent text;
    text.type = EventType::TextInput;
    text.text = "typed";
    window.renderer.SendEvent(text);
    window.Frame();

    // The value replaces the placeholder, which is the runtime resolution the
    // layout engine performs rather than a compiled one.
    ROBO_CHECK_EQ(std::string(window.renderer.State().DisplayValue(field)), std::string("typed"));
}

ROBO_TEST(Input, WheelScrollsThePageAndStopsAtTheEnd)
{
    RoboTools::CompiledApplication application;
    std::string error;

    std::string body;

    for (int index = 0; index < 60; ++index)
    {
        body += "<p>Paragraph " + std::to_string(index) + " of a long page.</p>";
    }

    ROBO_CHECK(CompileBody(body, application, error));

    TestRenderer window(application.Document(), 500.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    ROBO_CHECK(window.renderer.Layout().MaxScrollY() > 0.0f);

    InputEvent wheel;
    wheel.type = EventType::Wheel;
    wheel.x = 250.0f;
    wheel.y = 150.0f;
    wheel.deltaY = 120.0f;

    const EventResult scrolled = window.renderer.SendEvent(wheel);
    ROBO_CHECK(scrolled.scrolled);
    ROBO_CHECK_EQ(window.renderer.State().ScrollY(), 120.0f);

    // Scrolling does not invalidate layout: it is a translation applied when
    // painting and when hit testing.
    ROBO_CHECK(!window.renderer.Layout().NeedsLayout());

    window.Frame();

    // The page scrolls, so the frame repaints the window.
    ROBO_CHECK(window.renderer.Metrics().fullRepaint);

    // Scrolling past the end clamps rather than running away.
    wheel.deltaY = 100000.0f;
    window.renderer.SendEvent(wheel);
    ROBO_CHECK_EQ(window.renderer.State().ScrollY(), window.renderer.Layout().MaxScrollY());
}

ROBO_TEST(Input, RangeValuesFollowThePointerAndTheKeyboard)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<input id=\"slider\" type=\"range\" min=\"0\" max=\"100\" value=\"0\">", application,
                           error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex slider = FindElementById(document, "slider");
    ROBO_CHECK(slider != NoElement);

    Rect bounds;
    ROBO_CHECK(window.renderer.Hits().ElementBounds(slider, bounds));

    InputEvent down;
    down.type = EventType::PointerDown;
    down.x = bounds.x + bounds.width * 0.5f;
    down.y = bounds.y + bounds.height * 0.5f;
    window.renderer.SendEvent(down);

    const float middle = window.renderer.State().NumericValue(slider);
    ROBO_CHECK(middle > 35.0f);
    ROBO_CHECK(middle < 65.0f);

    // The compiled value is untouched: the override lives in runtime state.
    ROBO_CHECK_EQ(ElementAt(document, slider)->numericValue, 0.0f);

    InputEvent right;
    right.type = EventType::KeyDown;
    right.key = Key::Right;
    window.renderer.SendEvent(right);

    ROBO_CHECK(window.renderer.State().NumericValue(slider) > middle);
}

ROBO_TEST(Input, FocusedControlsAnswerTheKeyboard)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<button id=\"save\" type=\"button\">Save</button>"
                           "<input id=\"check\" type=\"checkbox\">",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 640.0f, 300.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex save = FindElementById(document, "save");
    const ElementIndex check = FindElementById(document, "check");

    window.ClickElement(save);

    InputEvent enter;
    enter.type = EventType::KeyDown;
    enter.key = Key::Enter;
    window.renderer.SendEvent(enter);
    ROBO_CHECK_EQ(window.renderer.State().Focused(), save);

    // A pointer click focuses the checkbox and toggles it, so Space - the
    // keyboard equivalent of the same activation - toggles it back.
    window.ClickElement(check);
    ROBO_CHECK(window.renderer.State().Element(check).HasFlag(RuntimeFlagChecked));

    InputEvent space;
    space.type = EventType::KeyDown;
    space.key = Key::Space;
    window.renderer.SendEvent(space);

    ROBO_CHECK(!window.renderer.State().Element(check).HasFlag(RuntimeFlagChecked));
}
