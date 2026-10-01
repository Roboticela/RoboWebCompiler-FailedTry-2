// ---------------------------------------------------------------------------
// RoboRenderer - rasterizing an ordinary SVG document into an image.
//
// An inline `<svg>` element and a linked `.svg` image are the same problem: a
// small vector document that has to become pixels before the backend can blend
// it. This is the bridge between the shape reader in SvgGlyph.h and the image
// pipeline the resource manager already owns, so SVG reaches the frame through
// exactly the code path a PNG does.
//
// The rasterizer is deliberately modest: it covers fills (nonzero winding),
// strokes and linear/radial gradients, which is what a document that draws a
// picture uses. Text, filters, masks, patterns and clipping are outside it and
// contribute nothing rather than something wrong.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/Image.h"

#include <cstddef>

namespace RoboRenderer
{
    /// Rasterizes an SVG document into an RGBA image. `targetWidth`/`targetHeight`
    /// are the size to fall back on when the document declares no width/height
    /// and no viewBox of its own; they are ignored otherwise. Rasterization is
    /// capped so a document cannot ask for an unbounded surface.
    ImageDecodeResult DecodeSvgDocument(const char* text,
                                        std::size_t size,
                                        float targetWidth,
                                        float targetHeight,
                                        Image& out);
}
