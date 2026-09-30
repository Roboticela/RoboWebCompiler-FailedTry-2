// ---------------------------------------------------------------------------
// The graphics backend, the image codecs and resource management.
//
// The software backend is the reference implementation of every paint command,
// so these tests define what "correct" means for a GPU backend as well: the same
// commands must produce the same picture.
// ---------------------------------------------------------------------------
#include "RendererTestSupport.h"

using namespace RoboTest;

#include <cstdio>
#include <cstring>
#include <fstream>

using namespace RoboRenderer;

ROBO_TEST(Backend, FillsRectanglesThroughTheClipStack)
{
    SoftwareBackend backend;

    RenderTargetInfo target;
    target.logicalWidth = 64.0f;
    target.logicalHeight = 48.0f;
    target.deviceScale = 1.0f;

    ROBO_CHECK(backend.Initialize(target));
    backend.Clear(Color::Rgba(255, 255, 255));

    FrameContext context;
    ROBO_CHECK(backend.BeginFrame(Rect{}, context));

    PaintCommand fill;
    fill.type = PaintCommandType::FillRect;
    fill.bounds = Rect{ 4.0f, 4.0f, 20.0f, 10.0f };
    fill.color = Color::Rgba(255, 0, 0);
    backend.Execute(fill);

    ROBO_CHECK(SameColor(backend.ReadPixel(10, 8), Color::Rgba(255, 0, 0)));
    ROBO_CHECK(SameColor(backend.ReadPixel(2, 2), Color::Rgba(255, 255, 255)));

    // A clip confines the next fill, and popping it restores the old region.
    PaintCommand push;
    push.type = PaintCommandType::PushClip;
    push.bounds = Rect{ 0.0f, 0.0f, 12.0f, 48.0f };
    backend.Execute(push);

    PaintCommand clipped;
    clipped.type = PaintCommandType::FillRect;
    clipped.bounds = Rect{ 0.0f, 0.0f, 64.0f, 48.0f };
    clipped.color = Color::Rgba(0, 0, 255);
    backend.Execute(clipped);

    ROBO_CHECK(SameColor(backend.ReadPixel(6, 30), Color::Rgba(0, 0, 255)));
    ROBO_CHECK(!SameColor(backend.ReadPixel(40, 30), Color::Rgba(0, 0, 255)));

    PaintCommand pop;
    pop.type = PaintCommandType::PopClip;
    backend.Execute(pop);

    PaintCommand after;
    after.type = PaintCommandType::FillRect;
    after.bounds = Rect{ 40.0f, 30.0f, 10.0f, 10.0f };
    after.color = Color::Rgba(0, 128, 0);
    backend.Execute(after);

    ROBO_CHECK(SameColor(backend.ReadPixel(44, 34), Color::Rgba(0, 128, 0)));

    ROBO_CHECK(backend.EndFrame());
    ROBO_CHECK(backend.ExecutedCommandCount() == 5);
    ROBO_CHECK(backend.DrawnPixelCount() > 0);
}

ROBO_TEST(Backend, RoundsCornersAndStrokesBorders)
{
    SoftwareBackend backend;

    RenderTargetInfo target;
    target.logicalWidth = 40.0f;
    target.logicalHeight = 40.0f;
    target.deviceScale = 1.0f;

    ROBO_CHECK(backend.Initialize(target));
    backend.Clear(Color::Rgba(255, 255, 255));

    FrameContext context;
    backend.BeginFrame(Rect{}, context);

    PaintCommand rounded;
    rounded.type = PaintCommandType::FillRoundedRect;
    rounded.bounds = Rect{ 8.0f, 8.0f, 24.0f, 24.0f };
    rounded.radius = 8.0f;
    rounded.color = Color::Rgba(0, 0, 0);
    backend.Execute(rounded);

    // The centre is solid, the corner is not: a rounded rectangle is not a
    // rectangle with a rectangle painted over it.
    ROBO_CHECK(SameColor(backend.ReadPixel(20, 20), Color::Rgba(0, 0, 0)));
    ROBO_CHECK(!SameColor(backend.ReadPixel(9, 9), Color::Rgba(0, 0, 0)));
    ROBO_CHECK(SameColor(backend.ReadPixel(20, 9), Color::Rgba(0, 0, 0)));

    // A per-edge stroke draws only the edges it was given.
    backend.Clear(Color::Rgba(255, 255, 255));

    PaintCommand stroke;
    stroke.type = PaintCommandType::StrokeRect;
    stroke.bounds = Rect{ 4.0f, 4.0f, 20.0f, 20.0f };
    stroke.edges = Edges{ 2.0f, 0.0f, 0.0f, 0.0f };
    stroke.color = Color::Rgba(255, 0, 0);
    backend.Execute(stroke);

    ROBO_CHECK(SameColor(backend.ReadPixel(10, 5), Color::Rgba(255, 0, 0)));
    ROBO_CHECK(!SameColor(backend.ReadPixel(10, 22), Color::Rgba(255, 0, 0)));

    backend.EndFrame();
}

ROBO_TEST(Backend, DeviceScaleScalesTheTargetNotTheLayout)
{
    SoftwareBackend backend;

    RenderTargetInfo target;
    target.logicalWidth = 100.0f;
    target.logicalHeight = 50.0f;
    target.deviceScale = 2.0f;

    ROBO_CHECK(backend.Initialize(target));

    // Logical units stay logical; the target is physical pixels.
    ROBO_CHECK_EQ(backend.DeviceWidth(), static_cast<std::uint32_t>(200));
    ROBO_CHECK_EQ(backend.DeviceHeight(), static_cast<std::uint32_t>(100));
}

ROBO_TEST(Backend, TransformCommandsShiftWhatFollows)
{
    SoftwareBackend backend;

    RenderTargetInfo target;
    target.logicalWidth = 40.0f;
    target.logicalHeight = 40.0f;
    target.deviceScale = 1.0f;

    ROBO_CHECK(backend.Initialize(target));
    backend.Clear(Color::Rgba(255, 255, 255));

    FrameContext context;
    backend.BeginFrame(Rect{}, context);

    PaintCommand push;
    push.type = PaintCommandType::PushTransform;
    push.bounds = Rect{ 10.0f, 6.0f, 0.0f, 0.0f };
    backend.Execute(push);

    PaintCommand fill;
    fill.type = PaintCommandType::FillRect;
    fill.bounds = Rect{ 0.0f, 0.0f, 4.0f, 4.0f };
    fill.color = Color::Rgba(0, 0, 0);
    backend.Execute(fill);

    PaintCommand pop;
    pop.type = PaintCommandType::PopTransform;
    backend.Execute(pop);

    ROBO_CHECK(SameColor(backend.ReadPixel(11, 7), Color::Rgba(0, 0, 0)));
    ROBO_CHECK(!SameColor(backend.ReadPixel(1, 1), Color::Rgba(0, 0, 0)));

    backend.EndFrame();
}

ROBO_TEST(Images, PngRoundTripsThroughEncodeAndDecode)
{
    Image source = MakeCheckerboardImage(37, 21, 4);
    source.MutablePixel(0, 0)[0] = 255;
    source.MutablePixel(0, 0)[1] = 0;
    source.MutablePixel(0, 0)[2] = 0;
    source.MutablePixel(36, 20)[3] = 128;

    const std::vector<std::uint8_t> encoded = EncodePng(source);
    ROBO_CHECK(encoded.size() > 40);

    // PNG signature, so the file is real rather than merely non-empty.
    ROBO_CHECK_EQ(encoded[0], static_cast<std::uint8_t>(0x89));
    ROBO_CHECK_EQ(encoded[1], static_cast<std::uint8_t>('P'));
    ROBO_CHECK_EQ(encoded[2], static_cast<std::uint8_t>('N'));
    ROBO_CHECK_EQ(encoded[3], static_cast<std::uint8_t>('G'));

    Image decoded;
    const ImageDecodeResult result = DecodeImage(encoded.data(), encoded.size(), decoded);

    ROBO_CHECK(result.success);
    ROBO_CHECK_EQ(std::string(result.format), std::string("png"));
    ROBO_CHECK_EQ(decoded.width, source.width);
    ROBO_CHECK_EQ(decoded.height, source.height);

    std::size_t differences = 0;

    for (std::uint32_t y = 0; y < source.height; ++y)
    {
        for (std::uint32_t x = 0; x < source.width; ++x)
        {
            const std::uint8_t* left = source.Sample(x, y);
            const std::uint8_t* right = decoded.Sample(x, y);

            for (int channel = 0; channel < 4; ++channel)
            {
                if (left[channel] != right[channel])
                {
                    ++differences;
                }
            }
        }
    }

    ROBO_CHECK_EQ(differences, static_cast<std::size_t>(0));
}

ROBO_TEST(Images, BmpIsDecodedFromItsHeader)
{
    // A hand built 2x2 24-bit BMP, bottom up: the format the compiled examples
    // ship as image assets.
    const std::size_t rowStride = 8;
    std::vector<std::uint8_t> bmp(54 + rowStride * 2, 0);

    bmp[0] = 'B';
    bmp[1] = 'M';

    const std::uint32_t fileSize = static_cast<std::uint32_t>(bmp.size());
    bmp[2] = static_cast<std::uint8_t>(fileSize & 0xFF);
    bmp[3] = static_cast<std::uint8_t>((fileSize >> 8) & 0xFF);
    bmp[10] = 54;

    bmp[14] = 40; // DIB header size
    bmp[18] = 2;  // width
    bmp[22] = 2;  // height
    bmp[26] = 1;  // planes
    bmp[28] = 24; // bits per pixel
    bmp[30] = 0;  // BI_RGB

    // Bottom row: red, green. Top row: blue, white.
    bmp[54 + 0] = 0;
    bmp[54 + 1] = 0;
    bmp[54 + 2] = 255;
    bmp[54 + 3] = 0;
    bmp[54 + 4] = 255;
    bmp[54 + 5] = 0;
    bmp[54 + rowStride + 0] = 255;
    bmp[54 + rowStride + 1] = 0;
    bmp[54 + rowStride + 2] = 0;
    bmp[54 + rowStride + 3] = 255;
    bmp[54 + rowStride + 4] = 255;
    bmp[54 + rowStride + 5] = 255;

    Image decoded;
    const ImageDecodeResult result = DecodeImage(bmp.data(), bmp.size(), decoded);

    ROBO_CHECK(result.success);
    ROBO_CHECK_EQ(std::string(result.format), std::string("bmp"));
    ROBO_CHECK_EQ(decoded.width, static_cast<std::uint32_t>(2));
    ROBO_CHECK_EQ(decoded.height, static_cast<std::uint32_t>(2));

    // BMP stores rows bottom up, so the first stored row is the last image row.
    const std::uint8_t* topLeft = decoded.Sample(0, 0);
    const std::uint8_t* bottomLeft = decoded.Sample(0, 1);

    ROBO_CHECK_EQ(static_cast<int>(topLeft[2]), 255);
    ROBO_CHECK_EQ(static_cast<int>(bottomLeft[0]), 255);
}

ROBO_TEST(Images, MalformedDataIsReportedNotGuessed)
{
    Image decoded;

    const std::uint8_t garbage[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const ImageDecodeResult result = DecodeImage(garbage, sizeof(garbage), decoded);

    ROBO_CHECK(!result.success);
    ROBO_CHECK(!result.message.empty());

    // A truncated PNG is recognised as a PNG and then rejected with a reason.
    Image source = MakeSolidImage(8, 8, 10, 20, 30);
    const std::vector<std::uint8_t> encoded = EncodePng(source);

    const ImageDecodeResult truncated = DecodeImage(encoded.data(), encoded.size() / 2, decoded);
    ROBO_CHECK(!truncated.success);
    ROBO_CHECK(!truncated.message.empty());

    ROBO_CHECK(!DecodeImage(nullptr, 0, decoded).success);
}

ROBO_TEST(Images, PlaceholderIsDrawnForAMissingResource)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<img id=\"broken\" src=\"definitely-missing.png\" alt=\"missing\" width=\"64\" "
                           "height=\"48\">",
                           application, error));

    const CompiledDocument& document = application.Document();

    TestRenderer window(document, 400.0f, 200.0f);
    ROBO_CHECK(window.initialized);

    // The load failure is reported, and the frame still draws a placeholder
    // rather than a hole.
    ROBO_CHECK(!window.renderer.Resources().Diagnostics().empty());
    ROBO_CHECK_EQ(window.renderer.Resources().ResourceStats().failedCount, static_cast<std::uint32_t>(1));

    ROBO_CHECK(window.Frame());

    const ElementIndex image = FindElementById(document, "broken");
    const ElementLayout* box = window.renderer.Layout().Result().Element(image);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        ROBO_CHECK(CountInkPixels(window.backend, box->border) > 0);
    }
}

ROBO_TEST(Resources, IdenticalSourcesAreLoadedOnce)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<img src=\"tile.png\" alt=\"a\"><img src=\"tile.png\" alt=\"b\">"
                           "<img src=\"tile.png\" alt=\"c\">",
                           application, error));

    const CompiledDocument& document = application.Document();

    // The compiler deduplicates identical sources, so the runtime has one entry
    // to load for three elements.
    ROBO_CHECK_EQ(document.resourceCount, static_cast<std::uint32_t>(2));

    ResourceManager resources(document);
    ROBO_CHECK_EQ(resources.MetricCount(), static_cast<std::size_t>(2));

    // Slot 0 is reserved: index 0 always means "no resource".
    ROBO_CHECK(!resources.IsLoaded(0));
    ROBO_CHECK_EQ(resources.ImageFor(0).width, resources.Placeholder().width);
}

ROBO_TEST(Resources, DecodedImagesAreDrawnScaledToTheirBox)
{
    RoboTools::CompiledApplication application;
    std::string error;

    ROBO_CHECK(CompileBody("<img id=\"pic\" src=\"tile.png\" alt=\"tile\" width=\"32\" height=\"32\">",
                           application, error));

    const CompiledDocument& document = application.Document();

    // Write the tile the document references next to the process, then load it.
    const Image tile = MakeSolidImage(4, 4, 255, 0, 0);
    ROBO_CHECK(WritePngFile("tile.png", tile));

    TestRenderer window(document, 200.0f, 200.0f, 1.0f, ".");
    ROBO_CHECK(window.Frame());

    const ElementIndex image = FindElementById(document, "pic");
    const ElementLayout* box = window.renderer.Layout().Result().Element(image);
    ROBO_CHECK(box != nullptr);

    if (box != nullptr)
    {
        // A 4x4 red image in a 32x32 box: the sample in the middle is red, which
        // is what scaling means.
        const Color center =
            window.Pixel(static_cast<std::uint32_t>(box->content.x + box->content.width * 0.5f),
                         static_cast<std::uint32_t>(box->content.y + box->content.height * 0.5f));
        ROBO_CHECK(SameColor(center, Color::Rgba(255, 0, 0), 4));
    }

    std::remove("tile.png");
}

ROBO_TEST(Resources, PpmWrittenByTheRuntimeCanBeReadBack)
{
    const Image source = MakeSolidImage(6, 4, 12, 34, 56);

    ROBO_CHECK(WritePpmFile("roborenderer-test.ppm", source));

    std::ifstream stream("roborenderer-test.ppm", std::ios::binary);
    ROBO_CHECK(stream.good());

    std::string header;
    stream >> header;
    ROBO_CHECK_EQ(header, std::string("P6"));

    stream.close();

    Image decoded;
    const ImageDecodeResult result = DecodeImageFile("roborenderer-test.ppm", decoded);

    ROBO_CHECK(result.success);
    ROBO_CHECK_EQ(decoded.width, static_cast<std::uint32_t>(6));

    const std::uint8_t* pixel = decoded.Sample(3, 2);
    ROBO_CHECK_EQ(static_cast<int>(pixel[0]), 12);
    ROBO_CHECK_EQ(static_cast<int>(pixel[1]), 34);
    ROBO_CHECK_EQ(static_cast<int>(pixel[2]), 56);

    std::remove("roborenderer-test.ppm");
}
