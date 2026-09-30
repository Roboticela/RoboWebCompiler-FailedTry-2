#include "RoboRenderer/ResourceManager.h"

#include <chrono>

namespace RoboRenderer
{
    namespace
    {
        bool IsAbsoluteSource(const std::string& source)
        {
            if (source.empty())
            {
                return false;
            }

            if (source[0] == '/' || source[0] == '\\')
            {
                return true;
            }

            // "C:\..." and "scheme://..." are both absolute as far as file
            // resolution is concerned.
            if (source.size() >= 2 && source[1] == ':')
            {
                return true;
            }

            return source.find("://") != std::string::npos;
        }
    }

    ResourceManager::ResourceManager(const CompiledDocument& compiledDocument)
        : document(&compiledDocument)
    {
        entries.resize(document->resourceCount);
        metrics.assign(document->resourceCount, ResourceMetrics{});

        for (ResourceIndex index = 0; index < document->resourceCount; ++index)
        {
            entries[index].index = index;
            entries[index].kind = document->resources[index].kind;
        }

        placeholder = MakePlaceholderImage(64, 48);
    }

    void ResourceManager::SetBaseDirectory(std::string directory)
    {
        baseDirectory = std::move(directory);
    }

    std::string ResourceManager::SourcePath(ResourceIndex index) const
    {
        const char* source = index < document->resourceCount ? StringAt(*document, document->resources[index].source)
                                                            : "";

        if (source == nullptr || source[0] == '\0')
        {
            return std::string();
        }

        const std::string value(source);

        if (baseDirectory.empty() || IsAbsoluteSource(value))
        {
            return value;
        }

        if (baseDirectory.back() == '/' || baseDirectory.back() == '\\')
        {
            return baseDirectory + value;
        }

        return baseDirectory + "/" + value;
    }

    bool ResourceManager::Load(ResourceIndex index)
    {
        if (index == 0 || index >= entries.size())
        {
            return false;
        }

        ResourceEntry& entry = entries[index];
        if (entry.loaded)
        {
            return true;
        }

        const std::string path = SourcePath(index);

        if (path.empty())
        {
            ResourceDiagnostic diagnostic;
            diagnostic.index = index;
            diagnostic.source = path;
            diagnostic.message = "the compiled resource has no source";
            diagnostics.push_back(std::move(diagnostic));

            ++stats.failedCount;
            return false;
        }

        Image image;
        const ImageDecodeResult result = DecodeImageFile(path.c_str(), image);

        if (!result.success)
        {
            ResourceDiagnostic diagnostic;
            diagnostic.index = index;
            diagnostic.source = path;
            diagnostic.message = result.message;
            diagnostics.push_back(std::move(diagnostic));

            ++stats.failedCount;
            return false;
        }

        entry.image = std::move(image);
        entry.loaded = true;
        entry.metrics.loaded = true;
        entry.metrics.width = static_cast<float>(entry.image.width);
        entry.metrics.height = static_cast<float>(entry.image.height);

        // A document that declares a size with width/height attributes wins over
        // the intrinsic size; the layout engine applies that rule, and the metric
        // published here is the image's own size.
        if (index < metrics.size())
        {
            metrics[index] = entry.metrics;
        }

        ++stats.loadedCount;
        stats.decodedBytes += entry.image.pixels.size();
        return true;
    }

    void ResourceManager::LoadAll()
    {
        const auto start = std::chrono::steady_clock::now();

        stats.resourceCount = document->resourceCount;
        stats.loadedCount = 0;
        stats.failedCount = 0;
        stats.decodedBytes = 0;

        for (ResourceIndex index = 1; index < document->resourceCount; ++index)
        {
            Load(index);
        }

        stats.decodeTimeMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    const Image& ResourceManager::ImageFor(ResourceIndex index) const
    {
        if (index == 0 || index >= entries.size())
        {
            return placeholder;
        }

        return entries[index].loaded ? entries[index].image : placeholder;
    }

    bool ResourceManager::IsLoaded(ResourceIndex index) const
    {
        return index != 0 && index < entries.size() && entries[index].loaded;
    }

    void ResourceManager::SetPlaceholder(Image image)
    {
        placeholder = std::move(image);
    }
}
