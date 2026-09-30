// ---------------------------------------------------------------------------
// Visual regression.
//
//     HTML -> RoboHTMLCompiler -> compiled data -> RoboRenderer -> pixels
//
// Two kinds of check live here. The first asserts named pixels of documents
// whose appearance is known, so a failure says what is wrong rather than that
// something changed. The second compares whole frames against recorded
// references, which is what catches a drift no assertion was written for.
//
// A reference that does not exist yet is recorded rather than reported as a
// failure, so a fresh clone of the repository produces the images its own
// toolchain renders and then compares against them from that point on.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

#include "RoboRenderer/Image.h"

#include <cstdio>
#include <string>
#include <vector>

#if defined(ROBO_HAVE_GENERATED_UI)
#include "BasicAppCompiledUi.h"
#endif

using namespace RoboRenderer;

namespace
{
    std::string ReferenceDirectory()
    {
        return std::string(ROBO_TEST_SOURCE_DIR) + "/Tests/Reference/RoboRenderer";
    }

    /// Compares a rendered frame against a recorded reference. Returns false and
    /// reports the mismatch when too many pixels differ; records the reference
    /// when there is none.
    bool CompareWithReference(const SoftwareBackend& backend, const std::string& name, std::string& report)
    {
        const std::string path = ReferenceDirectory() + "/" + name + ".png";

        Image reference;
        const ImageDecodeResult loaded = DecodeImageFile(path.c_str(), reference);

        if (!loaded.success)
        {
            const bool recorded = WritePngFile(path.c_str(), backend.Framebuffer());
            report = recorded ? "reference recorded at " + path : "could not record " + path;
            return false;
        }

        if (reference.width != backend.DeviceWidth() || reference.height != backend.DeviceHeight())
        {
            report = "reference is " + std::to_string(reference.width) + "x" + std::to_string(reference.height) +
                     " but the frame is " + std::to_string(backend.DeviceWidth()) + "x" +
                     std::to_string(backend.DeviceHeight());
            return false;
        }

        std::size_t different = 0;
        std::size_t total = 0;

        for (std::uint32_t y = 0; y < reference.height; ++y)
        {
            for (std::uint32_t x = 0; x < reference.width; ++x)
            {
                const std::uint8_t* left = reference.Sample(x, y);
                const std::uint8_t* right = backend.Framebuffer().Sample(x, y);
                ++total;

                for (int channel = 0; channel < 3; ++channel)
                {
                    const int difference = static_cast<int>(left[channel]) - static_cast<int>(right[channel]);
                    if (difference > 2 || difference < -2)
                    {
                        ++different;
                        break;
                    }
                }
            }
        }

        if (different * 1000u > total)
        {
            report = std::to_string(different) + " of " + std::to_string(total) +
                     " pixels differ from the reference";
            return false;
        }

        return true;
    }
}

ROBO_TEST(Visual, NamedPixelsOfAKnownDocumentAreWhatTheyShouldBe)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<h1 id=\"title\">Heading</h1>"
                           "<p id=\"body\">Body text in a paragraph.</p>"
                           "<button id=\"action\">Action</button>"
                           "<input id=\"check\" type=\"checkbox\" checked>",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 600.0f, 400.0f);
    ROBO_CHECK(window.Frame());

    const LayoutResult& layout = window.renderer.Layout().Result();

    // The document surface is the native default background.
    ROBO_CHECK(SameColor(window.Pixel(2, 2), Color::Rgba(255, 255, 255), 3));

    // The heading paints ink inside its own box.
    const ElementLayout* title = layout.Element(FindElementById(document, "title"));
    ROBO_CHECK(title != nullptr);

    if (title != nullptr)
    {
        ROBO_CHECK(CountInkPixels(window.backend, title->border) > 20);
        ROBO_CHECK(CountInkPixels(window.backend, title->border) <
                   static_cast<std::size_t>(title->border.width * title->border.height));
    }

    // The button paints its raised face, which is not the page background.
    const ElementLayout* button = layout.Element(FindElementById(document, "action"));
    ROBO_CHECK(button != nullptr);

    if (button != nullptr)
    {
        const Color face = window.Pixel(static_cast<std::uint32_t>(button->border.x + 2.0f),
                                       static_cast<std::uint32_t>(button->border.y + button->border.height * 0.5f));
        ROBO_CHECK(!SameColor(face, Color::Rgba(255, 255, 255), 4));
    }

    // A checked checkbox paints its accent fill in the middle.
    const ElementLayout* check = layout.Element(FindElementById(document, "check"));
    ROBO_CHECK(check != nullptr);

    if (check != nullptr)
    {
        const Color accent = window.Pixel(static_cast<std::uint32_t>(check->border.x + check->border.width * 0.5f),
                                          static_cast<std::uint32_t>(check->border.y + check->border.height * 0.5f));
        ROBO_CHECK(!SameColor(accent, Color::Rgba(255, 255, 255), 4));
    }
}

ROBO_TEST(Visual, RenderingIsDeterministic)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<div><h2>Title</h2><p>Text and a <button>Button</button> in a flow.</p></div>",
                           application, error));

    TestRenderer window(application.Document(), 500.0f, 360.0f);
    ROBO_CHECK(window.Frame());

    const Image first = window.backend.Framebuffer();

    ROBO_CHECK(window.Draw());

    const Image& second = window.backend.Framebuffer();

    ROBO_CHECK_EQ(first.pixels.size(), second.pixels.size());

    std::size_t differences = 0;

    for (std::size_t index = 0; index < first.pixels.size() && index < second.pixels.size(); ++index)
    {
        if (first.pixels[index] != second.pixels[index])
        {
            ++differences;
        }
    }

    ROBO_CHECK_EQ(differences, static_cast<std::size_t>(0));
}

ROBO_TEST(Visual, InteractiveStateChangesThePixelsAndAResizeChangesTheFrame)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<button id=\"save\">Save</button>", application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 240.0f);
    ROBO_CHECK(window.Frame());

    const ElementIndex button = FindElementById(document, "save");

    Rect bounds;
    ROBO_CHECK(window.renderer.Hits().ElementBounds(button, bounds));

    const std::uint32_t probeX = static_cast<std::uint32_t>(bounds.x + bounds.width * 0.5f);
    const std::uint32_t probeY = static_cast<std::uint32_t>(bounds.y + bounds.height * 0.5f);

    const Color normal = window.Pixel(probeX, probeY);

    window.HoverElement(button);
    ROBO_CHECK(window.Frame());

    const Color hovered = window.Pixel(probeX, probeY);

    // The button changed appearance because its state changed, and only the
    // button's pixels changed because the damage region was its own box.
    ROBO_CHECK(!SameColor(normal, hovered));

    ROBO_CHECK(window.renderer.Metrics().damage.width < 400.0f);

    // A resize rebuilds the target at the new size.
    ROBO_CHECK(window.renderer.Resize(300.0f, 180.0f));
    ROBO_CHECK(window.Frame());

    ROBO_CHECK_EQ(window.Width(), static_cast<std::uint32_t>(300));
    ROBO_CHECK_EQ(window.Height(), static_cast<std::uint32_t>(180));
    ROBO_CHECK(window.renderer.Metrics().fullRepaint);
}

ROBO_TEST(Visual, ReferenceFramesMatch)
{
    struct Frame
    {
        const char* name;
        const char* body;
        float width;
        float height;
    };

    static const Frame frames[] = {
        { "text", "<h1>RoboRenderer</h1><p>Text is measured, broken and painted at run time from compiled data.</p>"
                  "<p>A second paragraph, <strong>bold</strong> and <em>regular</em> together.</p>",
          480.0f, 300.0f },
        { "controls", "<form><label>Field</label><input type=\"text\" value=\"value\">"
                      "<input type=\"checkbox\" checked><input type=\"radio\" checked>"
                      "<input type=\"range\" min=\"0\" max=\"100\" value=\"30\">"
                      "<progress value=\"60\" max=\"100\"></progress>"
                      "<select><option selected>Option</option></select>"
                      "<button type=\"button\">Button</button><textarea rows=\"2\">text</textarea></form>",
          480.0f, 420.0f },
        { "structure", "<div><ul><li>One</li><li>Two</li></ul>"
                       "<table><caption>Table</caption><thead><tr><th>Key</th><th>Value</th></tr></thead>"
                       "<tbody><tr><td>a</td><td>b</td></tr></tbody></table>"
                       "<details open><summary>Open</summary><p>Revealed</p></details><hr></div>",
          520.0f, 420.0f }
    };

    for (const Frame& frame : frames)
    {
        RoboTools::CompiledApplication application;
        std::string error;

        ROBO_CHECK(CompileBody(frame.body, application, error));

        TestRenderer window(application.Document(), frame.width, frame.height);
        ROBO_CHECK(window.Frame());

        std::string report;

        if (!CompareWithReference(window.backend, frame.name, report))
        {
            if (report.rfind("reference recorded", 0) == 0)
            {
                RoboTest::Skip(std::string(frame.name) + ": " + report);
                continue;
            }

            RoboTest::Fail(std::string(frame.name) + ": " + report, __FILE__, __LINE__);
        }
    }
}

#if defined(ROBO_HAVE_GENERATED_UI)
ROBO_TEST(Visual, GeneratedApplicationDataRendersEndToEnd)
{
    // The generated C++ tables the compiler emits for an application, consumed
    // by the runtime exactly as an application would consume them. This is the
    // whole pipeline: HTML -> compiler -> generated data -> layout -> pixels.
    const CompiledDocument& document = BasicAppGenerated::CompiledUi();

    ROBO_CHECK_EQ(document.elementCount, BasicAppGenerated::CompiledElementCount);
    ROBO_CHECK_EQ(document.stringCount, BasicAppGenerated::CompiledStringCount);
    ROBO_CHECK_EQ(document.resourceCount, BasicAppGenerated::CompiledResourceCount);

    RendererDiagnostics diagnostics;
    ROBO_CHECK(ValidateCompiledDocument(document, diagnostics));
    ROBO_CHECK(!diagnostics.HasErrors());

    TestRenderer window(document, 900.0f, 620.0f);
    ROBO_CHECK(window.initialized);
    ROBO_CHECK(window.Frame());

    // The frame has content: commands, glyphs and painted pixels.
    ROBO_CHECK(window.renderer.Metrics().commandCount > 10);
    ROBO_CHECK(window.renderer.Metrics().glyphCount > 50);
    ROBO_CHECK(window.renderer.Metrics().drawnPixels > 1000);

    // The document's text is painted: the heading's box contains ink.
    const ElementIndex heading = FindNth(document, RenderCategory::Heading);
    ROBO_CHECK(heading != NoElement);

    const ElementLayout* box = window.renderer.Layout().Result().Element(heading);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        ROBO_CHECK(CountInkPixels(window.backend, box->border) > 10);
    }

    // The same frame renders identically a second time.
    const Image first = window.backend.Framebuffer();
    ROBO_CHECK(window.Draw());

    std::size_t differences = 0;

    for (std::size_t index = 0; index < first.pixels.size(); ++index)
    {
        if (first.pixels[index] != window.backend.Framebuffer().pixels[index])
        {
            ++differences;
        }
    }

    ROBO_CHECK_EQ(differences, static_cast<std::size_t>(0));
}
#endif
