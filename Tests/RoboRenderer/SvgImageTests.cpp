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
