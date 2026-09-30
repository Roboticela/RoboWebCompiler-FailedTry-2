// ---------------------------------------------------------------------------
// RoboRenderer - resources.
//
//     ResourceIndex (compiled, dense, deduplicated by the compiler)
//            |
//            v
//     ResourceManager        one decode per distinct source, ever
//            |
//            +-- decoded image      -> the graphics backend uploads or blends it
//            +-- intrinsic size     -> the layout engine sizes <img> with it
//
// The compiler already deduplicated identical sources, so the manager's key is
// the resource index itself: an array, not a map, and no string comparison on
// any path the runtime takes after startup.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"
#include "RoboRenderer/Image.h"
#include "RoboRenderer/Layout.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RoboRenderer
{
    /// Why a resource did not load. Reported rather than swallowed: a missing
    /// image shows a placeholder and a diagnostic, never a hole or a crash.
    struct ResourceDiagnostic
    {
        ResourceIndex index = 0;
        std::string source;
        std::string message;
    };

    /// One loaded resource.
    struct ResourceEntry
    {
        ResourceIndex index = 0;
        /// 0 when the resource did not load, so index 0 stays "no resource".
        bool loaded = false;
        ResourceKind kind = ResourceKind::None;
        Image image;
        ResourceMetrics metrics;
    };

    class ResourceManager
    {
    public:
        explicit ResourceManager(const CompiledDocument& document);

        /// Directory the compiled sources are resolved against. The compiler
        /// records sources as the document wrote them, so the application names
        /// the directory the assets were published next to.
        void SetBaseDirectory(std::string directory);
        const std::string& BaseDirectory() const { return baseDirectory; }

        /// Decodes every resource the document references. Called once, at
        /// startup, on the thread that owns the document.
        void LoadAll();

        /// Loads one resource on demand. Returns false when it cannot be loaded.
        bool Load(ResourceIndex index);

        /// The decoded image of a resource, or nullptr when there is none. A
        /// resource that failed resolves to the placeholder instead of nullptr,
        /// so painting never branches on load state.
        const Image& ImageFor(ResourceIndex index) const;

        /// True when the resource was decoded successfully.
        bool IsLoaded(ResourceIndex index) const;

        /// Intrinsic sizes, in the shape the layout engine wants them.
        const ResourceMetrics* Metrics() const { return metrics.data(); }
        std::size_t MetricCount() const { return metrics.size(); }

        /// The image shown for a resource that could not be decoded.
        void SetPlaceholder(Image image);
        const Image& Placeholder() const { return placeholder; }

        struct Stats
        {
            std::uint32_t resourceCount = 0;
            std::uint32_t loadedCount = 0;
            std::uint32_t failedCount = 0;
            double decodeTimeMs = 0.0;
            std::uint64_t decodedBytes = 0;
        };

        const Stats& ResourceStats() const { return stats; }
        const std::vector<ResourceDiagnostic>& Diagnostics() const { return diagnostics; }

        /// The path a resource resolves to: the source joined to the base
        /// directory, unchanged when the source is already absolute.
        std::string SourcePath(ResourceIndex index) const;

    private:
        const CompiledDocument* document;
        std::string baseDirectory;

        std::vector<ResourceEntry> entries;
        std::vector<ResourceMetrics> metrics;
        std::vector<ResourceDiagnostic> diagnostics;

        Image placeholder;
        Stats stats;
    };
}
