// ---------------------------------------------------------------------------
// SVG rendering end to end.
//
// An `<svg>` used to be recorded and then left unpainted, and a linked `.svg`
// was rejected as an unrecognised image. These tests pin the fix: the reader
// flattens shapes, the rasterizer fills and strokes them, and both an inline
// element and a linked file reach the frame.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

#include "RoboRenderer/SvgGlyph.h"
#include "RoboRenderer/SvgImage.h"

#include <cstring>
#include <string>

using namespace RoboRenderer;
using namespace RoboTest;

namespace
{
    ImageDecodeResult Rasterize(const char* document, float width, float height, Image& out)
    {
        return DecodeSvgDocument(document, std::strlen(document), width, height, out);
    }

    /// One document built from a template, so a test can compare two stroke
    /// styles that differ in exactly one attribute.
    std::string WithAttribute(const char* format, const char* value)
    {
        const std::string text(format);
        const std::size_t position = text.find("%s");
        if (position == std::string::npos)
        {
            return text;
        }

        return text.substr(0, position) + value + text.substr(position + 2);
    }
}

ROBO_TEST(SvgImage, DocumentRasterizesItsShapes)
{
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"#ff0000\"/>"
        "</svg>";

    Image image;
    const ImageDecodeResult result = Rasterize(document, 0.0f, 0.0f, image);

    ROBO_CHECK(result.success);
    ROBO_CHECK(image.width == 100);
    ROBO_CHECK(image.height == 100);

    const std::uint8_t* centre = image.Sample(50, 50);
    ROBO_CHECK(centre != nullptr);
    if (centre != nullptr)
    {
        ROBO_CHECK(centre[0] > 200);
        ROBO_CHECK(centre[1] < 40);
        ROBO_CHECK(centre[2] < 40);
        ROBO_CHECK(centre[3] > 200);
    }

    // The document itself has no background, so the rect's outside stays clear.
    const std::uint8_t* corner = image.Sample(2, 2);
    ROBO_CHECK(corner != nullptr);
    if (corner != nullptr)
    {
        ROBO_CHECK(corner[3] < 40);
    }
}

ROBO_TEST(SvgImage, StrokeIsDrawnWithoutAFill)
{
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
        "<line x1=\"10\" y1=\"30\" x2=\"50\" y2=\"30\" fill=\"none\" stroke=\"#0000ff\" stroke-width=\"8\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 0.0f, 0.0f, image).success);

    const std::uint8_t* line = image.Sample(30, 30);
    ROBO_CHECK(line != nullptr);
    if (line != nullptr)
    {
        ROBO_CHECK(line[2] > 200);
        ROBO_CHECK(line[0] < 60);
    }

    const std::uint8_t* away = image.Sample(30, 8);
    ROBO_CHECK(away != nullptr);
    if (away != nullptr)
    {
        ROBO_CHECK(away[3] < 40);
    }
}

ROBO_TEST(SvgImage, AStrokedCirclePaintsItsRingInTheStrokeColour)
{
    // A circle stroked with a rectangle per segment used to cancel itself out:
    // the vertex pieces were wound the other way from the segments, so the ring
    // came out as a speckled starburst instead of a solid outline.
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<circle cx=\"50\" cy=\"50\" r=\"20\" fill=\"#f59e0b\" stroke=\"#b45309\" stroke-width=\"8\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 0.0f, 0.0f, image).success);

    // A radius of 20 with an 8 wide stroke covers radius 16..24, so the pixel at
    // x=30 holds nothing but stroke and the centre holds nothing but fill.
    const std::uint8_t* ring = image.Sample(30, 50);
    ROBO_CHECK(ring != nullptr);
    if (ring != nullptr)
    {
        ROBO_CHECK(ring[0] > 160);
        ROBO_CHECK(ring[0] < 200);
        ROBO_CHECK(ring[1] > 60);
        ROBO_CHECK(ring[1] < 110);
        ROBO_CHECK(ring[2] < 40);
        ROBO_CHECK(ring[3] > 200);
    }

    const std::uint8_t* centre = image.Sample(50, 50);
    ROBO_CHECK(centre != nullptr);
    if (centre != nullptr)
    {
        ROBO_CHECK(centre[0] > 230);
        ROBO_CHECK(centre[1] > 140);
        ROBO_CHECK(centre[2] < 40);
    }

    // Beyond the outer edge of the stroke there is nothing at all.
    const std::uint8_t* outside = image.Sample(50, 24);
    ROBO_CHECK(outside != nullptr);
    if (outside != nullptr)
    {
        ROBO_CHECK(outside[3] < 40);
    }
}

ROBO_TEST(SvgImage, AnOpenPathIsNotStrokedClosed)
{
    // `M10 20 L50 20 L50 80` never says `Z`, so the segment that would run from
    // its last point back to its first is not part of the shape: the stroke has
    // to stop at the last point and cap there.
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<path d=\"M10 20 L50 20 L50 80\" fill=\"none\" stroke=\"#ff0000\" stroke-width=\"4\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 0.0f, 0.0f, image).success);

    // The implicit closing segment from (50,80) to (10,20) crosses the middle of
    // the box; the two real segments are nowhere near it.
    const std::uint8_t* closing = image.Sample(30, 50);
    ROBO_CHECK(closing != nullptr);
    if (closing != nullptr)
    {
        ROBO_CHECK(closing[3] < 40);
    }

    const std::uint8_t* arm = image.Sample(50, 50);
    ROBO_CHECK(arm != nullptr);
    if (arm != nullptr)
    {
        ROBO_CHECK(arm[0] > 200);
        ROBO_CHECK(arm[3] > 200);
    }
}

ROBO_TEST(SvgImage, CapsAndJoinsFollowTheStrokeStyle)
{
    const char* prefix =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">";
    const char* line = "<path d=\"M30 50 L70 50\" fill=\"none\" stroke=\"#0000ff\" "
                       "stroke-width=\"10\"%s/></svg>";

    const std::string rounded = WithAttribute(line, " stroke-linecap=\"round\"");

    // The default cap is butt: the stroke stops exactly at the end point.
    const std::string butt = WithAttribute(line, "");

    Image roundImage;
    ROBO_CHECK(Rasterize((std::string(prefix) + rounded).c_str(), 0.0f, 0.0f, roundImage).success);
    Image buttImage;
    ROBO_CHECK(Rasterize((std::string(prefix) + butt).c_str(), 0.0f, 0.0f, buttImage).success);

    // A round cap is a disc of half the stroke width around the end point, so it
    // reaches to x=75; nothing reaches x=77.
    const std::uint8_t* roundEnd = roundImage.Sample(74, 50);
    ROBO_CHECK(roundEnd != nullptr);
    if (roundEnd != nullptr)
    {
        ROBO_CHECK(roundEnd[2] > 200);
        ROBO_CHECK(roundEnd[3] > 200);
    }

    const std::uint8_t* roundPast = roundImage.Sample(77, 50);
    ROBO_CHECK(roundPast != nullptr);
    if (roundPast != nullptr)
    {
        ROBO_CHECK(roundPast[3] < 40);
    }

    const std::uint8_t* buttEnd = buttImage.Sample(74, 50);
    ROBO_CHECK(buttEnd != nullptr);
    if (buttEnd != nullptr)
    {
        ROBO_CHECK(buttEnd[3] < 40);
    }

    // A bevel join cuts the corner off where a round join arcs around it.
    const char* join = "<path d=\"M30 30 L50 60 L70 30\" fill=\"none\" "
                       "stroke=\"#0000ff\" stroke-width=\"20\" stroke-linejoin=\"%s\"/></svg>";

    const std::string roundJoin = prefix + WithAttribute(join, "round");
    const std::string bevelJoin = prefix + WithAttribute(join, "bevel");

    Image roundJoined;
    ROBO_CHECK(Rasterize(roundJoin.c_str(), 0.0f, 0.0f, roundJoined).success);
    Image bevelJoined;
    ROBO_CHECK(Rasterize(bevelJoin.c_str(), 0.0f, 0.0f, bevelJoined).success);

    // Nine pixels below the corner is inside the round join's arc and outside the
    // straight cut a bevel draws between the two offset corners.
    const std::uint8_t* arc = roundJoined.Sample(50, 69);
    ROBO_CHECK(arc != nullptr);
    if (arc != nullptr)
    {
        ROBO_CHECK(arc[3] > 200);
    }

    const std::uint8_t* cut = bevelJoined.Sample(50, 69);
    ROBO_CHECK(cut != nullptr);
    if (cut != nullptr)
    {
        ROBO_CHECK(cut[3] < 40);
    }
}

ROBO_TEST(SvgImage, StrokeWidthScalesWithTheViewBox)
{
    // `stroke-width` is in user units: a document scaled up has to stroke thicker
    // by the same factor, or a scaled outline keeps the hairline of its own box.
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
        "<line x1=\"1\" y1=\"5\" x2=\"9\" y2=\"5\" stroke=\"#ff0000\" stroke-width=\"2\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 40.0f, 40.0f, image).success);
    ROBO_CHECK(image.width == 40);

    // At four device pixels per user unit, the stroke covers y 16..24.
    const std::uint8_t* inside = image.Sample(20, 17);
    ROBO_CHECK(inside != nullptr);
    if (inside != nullptr)
    {
        ROBO_CHECK(inside[0] > 200);
        ROBO_CHECK(inside[3] > 200);
    }

    const std::uint8_t* above = image.Sample(20, 14);
    ROBO_CHECK(above != nullptr);
    if (above != nullptr)
    {
        ROBO_CHECK(above[3] < 40);
    }
}

ROBO_TEST(SvgImage, ARoundedRectLeavesItsCornersEmpty)
{
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" rx=\"20\" fill=\"#00ff00\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 0.0f, 0.0f, image).success);

    // The corner arc of a 20 radius sits 5.9 units inside the box corner, so the
    // pixel beside the corner is outside the shape.
    const std::uint8_t* corner = image.Sample(12, 12);
    ROBO_CHECK(corner != nullptr);
    if (corner != nullptr)
    {
        ROBO_CHECK(corner[3] < 40);
    }

    // The straight edges between the arcs are still filled.
    const std::uint8_t* edge = image.Sample(50, 12);
    ROBO_CHECK(edge != nullptr);
    if (edge != nullptr)
    {
        ROBO_CHECK(edge[1] > 200);
        ROBO_CHECK(edge[3] > 200);
    }
}

ROBO_TEST(SvgImage, ALinearGradientShadesAcrossTheShape)
{
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"20\" viewBox=\"0 0 100 20\">"
        "<defs><linearGradient id=\"shade\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\""
        " gradientUnits=\"objectBoundingBox\">"
        "<stop offset=\"0\" stop-color=\"#000000\"/>"
        "<stop offset=\"1\" stop-color=\"#ffffff\"/>"
        "</linearGradient></defs>"
        "<rect x=\"0\" y=\"0\" width=\"100\" height=\"20\" fill=\"url(#shade)\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 0.0f, 0.0f, image).success);

    const std::uint8_t* left = image.Sample(4, 10);
    const std::uint8_t* right = image.Sample(95, 10);
    ROBO_CHECK(left != nullptr);
    ROBO_CHECK(right != nullptr);
    if (left != nullptr && right != nullptr)
    {
        ROBO_CHECK(left[0] < right[0]);
        ROBO_CHECK(left[0] < 60);
        ROBO_CHECK(right[0] > 180);
    }
}

ROBO_TEST(SvgImage, FillRuleEvenOddPunchesAHole)
{
    // Two rings wound the same way: nonzero fills the inner one twice, evenodd
    // leaves it empty.
    const char* evenOdd =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<path d=\"M20 20 H80 V80 H20 Z M40 40 H60 V60 H40 Z\" fill-rule=\"evenodd\" fill=\"#ff0000\"/>"
        "</svg>";

    const char* nonzero =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<path d=\"M20 20 H80 V80 H20 Z M40 40 H60 V60 H40 Z\" fill=\"#ff0000\"/>"
        "</svg>";

    Image holed;
    ROBO_CHECK(Rasterize(evenOdd, 0.0f, 0.0f, holed).success);

    Image solid;
    ROBO_CHECK(Rasterize(nonzero, 0.0f, 0.0f, solid).success);

    const std::uint8_t* holeCentre = holed.Sample(50, 50);
    ROBO_CHECK(holeCentre != nullptr);
    if (holeCentre != nullptr)
    {
        ROBO_CHECK(holeCentre[3] < 40);
    }

    const std::uint8_t* solidCentre = solid.Sample(50, 50);
    ROBO_CHECK(solidCentre != nullptr);
    if (solidCentre != nullptr)
    {
        ROBO_CHECK(solidCentre[0] > 200);
    }

    // The ring itself is filled under either rule.
    const std::uint8_t* ring = holed.Sample(30, 50);
    ROBO_CHECK(ring != nullptr);
    if (ring != nullptr)
    {
        ROBO_CHECK(ring[0] > 200);
    }
}

ROBO_TEST(SvgImage, TheReaderRecordsTheFillRuleForEveryShape)
{
    const char* document =
        "<svg viewBox=\"0 0 100 100\"><path d=\"M0 0 H10 V10 Z\" fill-rule=\"evenodd\"/>"
        "<rect x=\"0\" y=\"0\" width=\"5\" height=\"5\"/></svg>";

    std::vector<SvgShape> shapes;
    ROBO_CHECK(RenderSvgDocument(document, std::strlen(document), shapes, nullptr));
    ROBO_CHECK(shapes.size() == 2);

    if (shapes.size() == 2)
    {
        ROBO_CHECK(shapes[0].evenOdd);
        ROBO_CHECK(!shapes[1].evenOdd);
    }
}

ROBO_TEST(SvgImage, AViewBoxScalesToTheRequestedSize)
{
    const char* document =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
        "<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#00ff00\"/>"
        "</svg>";

    Image image;
    ROBO_CHECK(Rasterize(document, 64.0f, 64.0f, image).success);
    ROBO_CHECK(image.width == 64);
    ROBO_CHECK(image.height == 64);

    const std::uint8_t* centre = image.Sample(32, 32);
    ROBO_CHECK(centre != nullptr);
    if (centre != nullptr)
    {
        ROBO_CHECK(centre[1] > 200);
        ROBO_CHECK(centre[0] < 60);
    }
}

ROBO_TEST(SvgImage, InlineSvgReachesTheFrame)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<svg id=\"mark\" width=\"80\" height=\"80\" viewBox=\"0 0 100 100\">"
                           "<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#ff0000\"/>"
                           "</svg>",
                           application, error));

    const CompiledDocument& document = application.Document();
    TestRenderer window(document, 400.0f, 300.0f);
    ROBO_CHECK(window.Draw());
    ROBO_CHECK(!window.renderer.DiagnosticLog().HasErrors());

    const ElementIndex mark = FindElementById(document, "mark");
    ROBO_CHECK(mark != NoElement);

    const ElementLayout* box = window.renderer.Layout().Result().Element(mark);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        ROBO_CHECK(box->border.width > 40.0f);
        ROBO_CHECK(box->border.height > 40.0f);

        const std::uint32_t centreX = static_cast<std::uint32_t>(box->content.x + box->content.width * 0.5f);
        const std::uint32_t centreY = static_cast<std::uint32_t>(box->content.y + box->content.height * 0.5f);

        const Color pixel = window.Pixel(centreX, centreY);
        ROBO_CHECK(pixel.red > 200);
        ROBO_CHECK(pixel.green < 60);
        ROBO_CHECK(pixel.blue < 60);
    }
}

ROBO_TEST(SvgImage, ALinkedSvgFileIsDecodedAndDrawn)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<img id=\"logo\" src=\"logo.svg\" alt=\"logo\" width=\"64\" height=\"64\">",
                           application, error));

    const CompiledDocument& document = application.Document();
    const std::string assets = std::string(ROBO_TEST_SOURCE_DIR) + "/Examples/BasicApp";
    TestRenderer window(document, 400.0f, 300.0f, 1.0f, assets);

    ROBO_CHECK(window.Draw());
    ROBO_CHECK(!window.renderer.DiagnosticLog().HasErrors());

    const ElementIndex logo = FindElementById(document, "logo");
    ROBO_CHECK(logo != NoElement);

    const ElementLayout* box = window.renderer.Layout().Result().Element(logo);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        ROBO_CHECK(CountInkPixels(window.backend, box->border) > 0);
    }
}
