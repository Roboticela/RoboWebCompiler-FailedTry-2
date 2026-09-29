// ---------------------------------------------------------------------------
// RoboHTMLCompiler - SpecExtractor
//
// Mines the standards reference under Reference/HTML/ and converts it into
// machine-readable registry data.
//
//     Reference/HTML/source
//             |
//             v
//     SpecExtractor
//             |
//             +--> Generated/Registry/HtmlElements.json
//             +--> Generated/Registry/HtmlAttributes.json
//             +--> Generated/Registry/GlobalAttributes.json
//             +--> Generated/Registry/EventHandlerAttributes.json
//             +--> Generated/Registry/DeprecatedElements.json
//             +--> Generated/ImplementationCoverage.json
//
// The reference document is treated as an implementation checklist: everything
// it defines must end up either in a registry or in an explicit
// "unsupported"/"out-of-scope" bucket in the coverage manifest.
//
// The scan is deliberately offset-based rather than tree-based. The reference
// is the HTML Standard's own preprocessed source, which relies on a large
// amount of optional-tag behaviour (<p>, <li>, <dt>, <dd>, <tr>, ...). A tree
// built without those implicit-close rules swallows whole sections, so we walk
// a flat tag list and derive structure from heading offsets instead.
// ---------------------------------------------------------------------------

#include "Json.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    using RoboHTMLCompiler::Tools::JsonValue;

    // -----------------------------------------------------------------------
    // Text helpers
    // -----------------------------------------------------------------------

    std::string ToLower(const std::string& value)
    {
        std::string result = value;
        for (char& character : result)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        return result;
    }

    std::string Trim(const std::string& value)
    {
        std::size_t begin = 0;
        std::size_t end = value.size();

        while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }
        return value.substr(begin, end - begin);
    }

    std::string CollapseWhitespace(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());

        bool pendingSpace = false;
        for (const char character : value)
        {
            if (std::isspace(static_cast<unsigned char>(character)) != 0)
            {
                pendingSpace = !result.empty();
                continue;
            }

            if (pendingSpace)
            {
                result.push_back(' ');
                pendingSpace = false;
            }
            result.push_back(character);
        }

        while (!result.empty() && result.back() == ' ')
        {
            result.pop_back();
        }
        return result;
    }

    std::string DecodeEntities(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] != '&')
            {
                result.push_back(value[index]);
                continue;
            }

            const std::size_t terminator = value.find(';', index);
            if (terminator == std::string::npos || terminator - index > 12)
            {
                result.push_back(value[index]);
                continue;
            }

            const std::string entity = value.substr(index + 1, terminator - index - 1);
            if (entity == "amp") result.push_back('&');
            else if (entity == "lt") result.push_back('<');
            else if (entity == "gt") result.push_back('>');
            else if (entity == "quot") result.push_back('"');
            else if (entity == "apos") result.push_back('\'');
            else if (entity == "nbsp") result.push_back(' ');
            else if (!entity.empty() && entity[0] == '#')
            {
                try
                {
                    const long codePoint = (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X'))
                                               ? std::stol(entity.substr(2), nullptr, 16)
                                               : std::stol(entity.substr(1));
                    result.push_back((codePoint >= 0x20 && codePoint < 0x7F)
                                         ? static_cast<char>(codePoint)
                                         : ' ');
                }
                catch (...)
                {
                    result.push_back('&');
                    result += entity;
                    result.push_back(';');
                }
            }
            else
            {
                result.push_back('&');
                result += entity;
                result.push_back(';');
            }

            index = terminator;
        }

        return result;
    }

    /// Removes markup, decodes entities and collapses whitespace.
    std::string StripTags(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());

        bool insideTag = false;
        for (const char character : value)
        {
            if (character == '<')
            {
                insideTag = true;
                result.push_back(' ');
                continue;
            }
            if (character == '>')
            {
                insideTag = false;
                continue;
            }
            if (!insideTag)
            {
                result.push_back(character);
            }
        }

        return CollapseWhitespace(DecodeEntities(result));
    }

    bool IsNameCharacter(char character)
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' ||
               character == '_' || character == ':' || character == '.';
    }

    bool IsValidElementName(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }

        for (const char character : name)
        {
            if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '-' &&
                character != '_')
            {
                return false;
            }
        }
        return true;
    }

    bool IsValidAttributeName(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }

        for (const char character : name)
        {
            if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '-' &&
                character != '_' && character != ':')
            {
                return false;
            }
        }
        return true;
    }

    /// Strips the "*", "†" style markers the standard appends to inline code.
    std::string CleanCodeName(const std::string& value)
    {
        std::string result;
        for (const char character : Trim(value))
        {
            if (std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' ||
                character == '_')
            {
                result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
            else
            {
                break;
            }
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // Flat tag scan
    // -----------------------------------------------------------------------

    struct SpecTag
    {
        std::string name;
        std::vector<std::pair<std::string, std::string>> attributes;
        std::size_t startOffset = 0;   // offset of '<'
        std::size_t endOffset = 0;     // offset just past '>'
        bool isEndTag = false;

        bool HasAttribute(const std::string& attributeName) const
        {
            for (const auto& attribute : attributes)
            {
                if (attribute.first == attributeName)
                {
                    return true;
                }
            }
            return false;
        }

        std::string GetAttribute(const std::string& attributeName) const
        {
            for (const auto& attribute : attributes)
            {
                if (attribute.first == attributeName)
                {
                    return attribute.second;
                }
            }
            return std::string();
        }

        bool HasClass(const std::string& className) const
        {
            std::istringstream stream(GetAttribute("class"));
            std::string token;
            while (stream >> token)
            {
                if (token == className)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsHeading() const
        {
            return name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6';
        }

        int HeadingLevel() const
        {
            return IsHeading() ? (name[1] - '0') : 0;
        }
    };

    class SpecDocument
    {
    public:
        explicit SpecDocument(std::string source)
            : text(std::move(source))
        {
            ScanTags();
            BuildLineIndex();
        }

        const std::string& Text() const { return text; }

        const std::vector<SpecTag>& Tags() const { return tags; }

        std::size_t LineForOffset(std::size_t offset) const
        {
            const auto iterator = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset);
            if (iterator == lineStarts.begin())
            {
                return 1;
            }
            return static_cast<std::size_t>(iterator - lineStarts.begin());
        }

        /// Index of the first tag starting at or after the offset, or tags.size().
        std::size_t FirstTagAtOrAfter(std::size_t offset) const
        {
            std::size_t low = 0;
            std::size_t high = tags.size();
            while (low < high)
            {
                const std::size_t middle = low + (high - low) / 2;
                if (tags[middle].startOffset < offset)
                {
                    low = middle + 1;
                }
                else
                {
                    high = middle;
                }
            }
            return low;
        }

        /// Text between two offsets with markup stripped.
        std::string TextBetween(std::size_t begin, std::size_t end) const
        {
            if (end <= begin || begin >= text.size())
            {
                return std::string();
            }
            return StripTags(text.substr(begin, std::min(end, text.size()) - begin));
        }

        /// Raw text (markup intact) between two offsets.
        std::string RawBetween(std::size_t begin, std::size_t end) const
        {
            if (end <= begin || begin >= text.size())
            {
                return std::string();
            }
            return text.substr(begin, std::min(end, text.size()) - begin);
        }

        /// Offset of the matching end tag for the start tag at tagIndex, or the
        /// supplied fallback when none exists.
        std::size_t MatchingEndTagOffset(std::size_t tagIndex, std::size_t fallback) const
        {
            const std::string name = tags[tagIndex].name;
            for (std::size_t index = tagIndex + 1; index < tags.size(); ++index)
            {
                if (tags[index].isEndTag && tags[index].name == name)
                {
                    return tags[index].startOffset;
                }
            }
            return fallback;
        }

        /// Index of the enclosing heading for the tag at or before the offset.
        std::size_t EnclosingHeadingIndex(std::size_t offset) const
        {
            std::size_t result = tags.size();
            for (std::size_t index = 0; index < tags.size(); ++index)
            {
                if (tags[index].startOffset > offset)
                {
                    break;
                }
                if (!tags[index].isEndTag && tags[index].IsHeading())
                {
                    result = index;
                }
            }
            return result;
        }

    private:
        void BuildLineIndex()
        {
            lineStarts.clear();
            lineStarts.push_back(0);
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '\n')
                {
                    lineStarts.push_back(index + 1);
                }
            }
        }

        static bool IsRawTextElement(const std::string& name)
        {
            return name == "script" || name == "style";
        }

        void ScanTags()
        {
            std::size_t position = 0;

            while (position < text.size())
            {
                const std::size_t open = text.find('<', position);
                if (open == std::string::npos)
                {
                    break;
                }

                if (text.compare(open, 4, "<!--") == 0)
                {
                    const std::size_t terminator = text.find("-->", open + 4);
                    position = (terminator == std::string::npos) ? text.size() : terminator + 3;
                    continue;
                }

                if (text.compare(open, 2, "<!") == 0 || text.compare(open, 2, "<?") == 0)
                {
                    const std::size_t terminator = text.find('>', open);
                    position = (terminator == std::string::npos) ? text.size() : terminator + 1;
                    continue;
                }

                SpecTag tag;
                tag.startOffset = open;
                std::size_t cursor = open + 1;

                if (cursor < text.size() && text[cursor] == '/')
                {
                    tag.isEndTag = true;
                    ++cursor;
                }

                while (cursor < text.size() && IsNameCharacter(text[cursor]))
                {
                    tag.name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(text[cursor]))));
                    ++cursor;
                }

                if (tag.name.empty())
                {
                    position = open + 1;
                    continue;
                }

                while (cursor < text.size() && text[cursor] != '>')
                {
                    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0)
                    {
                        ++cursor;
                    }

                    if (cursor >= text.size() || text[cursor] == '>')
                    {
                        break;
                    }

                    if (text[cursor] == '/')
                    {
                        ++cursor;
                        continue;
                    }

                    std::string attributeName;
                    while (cursor < text.size() && IsNameCharacter(text[cursor]))
                    {
                        attributeName.push_back(
                            static_cast<char>(std::tolower(static_cast<unsigned char>(text[cursor]))));
                        ++cursor;
                    }

                    if (attributeName.empty())
                    {
                        ++cursor;
                        continue;
                    }

                    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0)
                    {
                        ++cursor;
                    }

                    std::string attributeValue;
                    if (cursor < text.size() && text[cursor] == '=')
                    {
                        ++cursor;
                        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0)
                        {
                            ++cursor;
                        }

                        if (cursor < text.size() && (text[cursor] == '"' || text[cursor] == '\''))
                        {
                            const char quote = text[cursor++];
                            const std::size_t terminator = text.find(quote, cursor);
                            if (terminator == std::string::npos)
                            {
                                attributeValue = text.substr(cursor);
                                cursor = text.size();
                            }
                            else
                            {
                                attributeValue = text.substr(cursor, terminator - cursor);
                                cursor = terminator + 1;
                            }
                        }
                        else
                        {
                            while (cursor < text.size() &&
                                   std::isspace(static_cast<unsigned char>(text[cursor])) == 0 &&
                                   text[cursor] != '>')
                            {
                                attributeValue.push_back(text[cursor]);
                                ++cursor;
                            }
                        }
                    }

                    tag.attributes.emplace_back(attributeName, attributeValue);
                }

                const std::size_t terminator = text.find('>', cursor);
                tag.endOffset = (terminator == std::string::npos) ? text.size() : terminator + 1;
                position = tag.endOffset;

                const bool rawText = !tag.isEndTag && IsRawTextElement(tag.name);
                tags.push_back(tag);

                if (rawText)
                {
                    const std::string closing = "</" + tag.name;
                    std::size_t searchFrom = position;
                    while (searchFrom < text.size())
                    {
                        const std::size_t candidate = text.find('<', searchFrom);
                        if (candidate == std::string::npos)
                        {
                            position = text.size();
                            break;
                        }

                        if (ToLower(text.substr(candidate, closing.size())) == closing)
                        {
                            SpecTag endTag;
                            endTag.name = tag.name;
                            endTag.isEndTag = true;
                            endTag.startOffset = candidate;
                            const std::size_t endTerminator = text.find('>', candidate);
                            endTag.endOffset =
                                (endTerminator == std::string::npos) ? text.size() : endTerminator + 1;
                            tags.push_back(endTag);
                            position = endTag.endOffset;
                            break;
                        }

                        searchFrom = candidate + 1;
                    }
                }
            }
        }

        std::string text;
        std::vector<SpecTag> tags;
        std::vector<std::size_t> lineStarts;
    };

    // -----------------------------------------------------------------------
    // Extraction model
    // -----------------------------------------------------------------------

    struct ExtractedElement
    {
        std::string name;
        std::string sectionId;
        std::vector<std::string> categories;
        std::string contentModel;
        std::vector<std::string> attributes;
        bool globalAttributes = false;
        std::string domInterface;
        bool obsolete = false;
        std::size_t line = 0;
    };

    struct ExtractedAttribute
    {
        std::string name;
        std::string kind;                 // "global" | "element" | "eventhandler"
        std::vector<std::string> allowedElements;
        bool obsolete = false;
    };

    // -----------------------------------------------------------------------
    // Extractor
    // -----------------------------------------------------------------------

    class SpecExtractor
    {
    public:
        bool Run(const fs::path& referenceDirectory,
                 const fs::path& registryOutputDirectory,
                 const fs::path& coverageOutputPath)
        {
            const fs::path specSource = referenceDirectory / "source";
            if (!fs::exists(specSource))
            {
                std::cerr << "[SpecExtractor] reference document not found: " << specSource.string() << "\n";
                return false;
            }

            std::cout << "[SpecExtractor] reading " << specSource.string() << "\n";
            document = std::make_unique<SpecDocument>(ReadWholeFile(specSource));

            IndexHeadings();
            CollectElements();
            CollectGlobalAttributes();
            CollectEventHandlerAttributes();
            BuildAttributeIndex();
            CollectDeprecatedAttributes();

            std::cout << "[SpecExtractor] elements:            " << elements.size() << "\n";
            std::cout << "[SpecExtractor] global attributes:   " << globalAttributes.size() << "\n";
            std::cout << "[SpecExtractor] element attributes:  " << elementAttributes.size() << "\n";
            std::cout << "[SpecExtractor] event handlers:      " << eventHandlerAttributes.size() << "\n";
            std::cout << "[SpecExtractor] obsolete elements:   " << obsoleteElementNames.size() << "\n";

            fs::create_directories(registryOutputDirectory);
            fs::create_directories(coverageOutputPath.parent_path());

            WriteElements(registryOutputDirectory / "HtmlElements.json");
            WriteAttributes(registryOutputDirectory / "HtmlAttributes.json");
            WriteGlobalAttributes(registryOutputDirectory / "GlobalAttributes.json");
            WriteEventHandlerAttributes(registryOutputDirectory / "EventHandlerAttributes.json");
            WriteDeprecatedElements(registryOutputDirectory / "DeprecatedElements.json");
            WriteCoverage(coverageOutputPath, referenceDirectory);

            return true;
        }

    private:
        std::unique_ptr<SpecDocument> document;
        std::vector<std::size_t> headingIndices;            // tag indices of headings, in order
        std::vector<ExtractedElement> elements;
        std::vector<ExtractedAttribute> globalAttributes;
        std::vector<ExtractedAttribute> eventHandlerAttributes;
        std::map<std::string, ExtractedAttribute> elementAttributes;
        std::set<std::string> obsoleteElementNames;
        std::set<std::string> obsoleteAttributeNames;

        static std::string ReadWholeFile(const fs::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        void IndexHeadings()
        {
            const std::vector<SpecTag>& tags = document->Tags();
            for (std::size_t index = 0; index < tags.size(); ++index)
            {
                if (!tags[index].isEndTag && tags[index].IsHeading())
                {
                    headingIndices.push_back(index);
                }
            }
        }

        const SpecTag& TagAt(std::size_t index) const { return document->Tags()[index]; }

        /// Region that belongs to the heading at headingIndices[position]:
        /// from the heading's end to the next heading of any level.
        void HeadingRegion(std::size_t position, std::size_t& begin, std::size_t& end) const
        {
            begin = TagAt(headingIndices[position]).endOffset;
            end = (position + 1 < headingIndices.size())
                      ? TagAt(headingIndices[position + 1]).startOffset
                      : document->Text().size();
        }

        /// True when the heading chain above the region marks it as obsolete.
        bool RegionIsObsolete(std::size_t position) const
        {
            for (std::size_t index = position + 1; index-- > 0;)
            {
                const SpecTag& heading = TagAt(headingIndices[index]);
                if (heading.HeadingLevel() == 2)
                {
                    return heading.GetAttribute("split-filename") == "obsolete";
                }
            }
            return false;
        }

        /// Locates the <dl class="element"> that belongs to a heading region.
        std::size_t FindElementList(std::size_t begin, std::size_t end) const
        {
            const std::size_t first = document->FirstTagAtOrAfter(begin);
            for (std::size_t index = first; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset >= end)
                {
                    break;
                }
                if (!TagAt(index).isEndTag && TagAt(index).name == "dl" && TagAt(index).HasClass("element"))
                {
                    return index;
                }
            }
            return document->Tags().size();
        }

        /// Collects the <code data-x="attr-*"> names inside a row region.
        void CollectAttributeNamesFromRegion(std::size_t begin, std::size_t end,
                                             std::vector<std::string>& out) const
        {
            const std::size_t first = document->FirstTagAtOrAfter(begin);
            for (std::size_t index = first; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset >= end)
                {
                    break;
                }

                if (TagAt(index).isEndTag || TagAt(index).name != "code")
                {
                    continue;
                }

                const std::string key = TagAt(index).GetAttribute("data-x");
                if (key.compare(0, 5, "attr-") != 0)
                {
                    continue;
                }

                const std::size_t textBegin = TagAt(index).endOffset;
                const std::size_t textEnd = document->MatchingEndTagOffset(index, end);
                const std::string name = ToLower(Trim(document->RawBetween(textBegin, textEnd)));

                if (IsValidAttributeName(name) && std::find(out.begin(), out.end(), name) == out.end())
                {
                    out.push_back(name);
                }
            }
        }

        void CollectElements()
        {
            const std::vector<SpecTag>& tags = document->Tags();

            for (std::size_t index = 0; index < tags.size(); ++index)
            {
                const SpecTag& tag = tags[index];
                if (tag.isEndTag || tag.name != "dfn")
                {
                    continue;
                }

                if (!tag.HasAttribute("element") || tag.HasAttribute("element-attr") ||
                    tag.HasAttribute("element-state"))
                {
                    continue;
                }

                const std::size_t textBegin = tag.endOffset;
                const std::size_t textEnd = document->MatchingEndTagOffset(index, tag.endOffset);
                const std::string name = ToLower(Trim(document->TextBetween(textBegin, textEnd)));

                if (!IsValidElementName(name))
                {
                    continue;
                }

                const std::size_t headingPosition = HeadingPositionFor(tag.startOffset);
                const bool obsolete = (headingPosition != headingIndices.size()) && RegionIsObsolete(headingPosition);

                ExtractedElement* existing = FindElement(name);
                if (existing == nullptr)
                {
                    ExtractedElement extracted;
                    extracted.name = name;
                    extracted.obsolete = obsolete;
                    extracted.line = document->LineForOffset(tag.startOffset);

                    if (headingPosition != headingIndices.size())
                    {
                        extracted.sectionId = TagAt(headingIndices[headingPosition]).GetAttribute("id");
                        ExtractElementDetails(headingPosition, extracted);
                    }

                    elements.push_back(extracted);
                    if (obsolete)
                    {
                        obsoleteElementNames.insert(name);
                    }
                    continue;
                }

                // A later definition wins for metadata, and an obsolete flag is
                // sticky so the compiler still reports the feature.
                if (!obsolete && existing->obsolete)
                {
                    existing->obsolete = false;
                    obsoleteElementNames.erase(name);
                }
                else if (obsolete)
                {
                    existing->obsolete = true;
                    obsoleteElementNames.insert(name);
                }
            }
        }

        /// Position within headingIndices of the heading that encloses an offset.
        std::size_t HeadingPositionFor(std::size_t offset) const
        {
            std::size_t result = headingIndices.size();
            for (std::size_t index = 0; index < headingIndices.size(); ++index)
            {
                if (TagAt(headingIndices[index]).startOffset > offset)
                {
                    break;
                }
                result = index;
            }
            return result;
        }

        ExtractedElement* FindElement(const std::string& name)
        {
            for (ExtractedElement& element : elements)
            {
                if (element.name == name)
                {
                    return &element;
                }
            }
            return nullptr;
        }

        void ExtractElementDetails(std::size_t headingPosition, ExtractedElement& extracted) const
        {
            std::size_t begin = 0;
            std::size_t end = 0;
            HeadingRegion(headingPosition, begin, end);

            const std::size_t listIndex = FindElementList(begin, end);
            if (listIndex >= document->Tags().size())
            {
                return;
            }

            const std::size_t listEnd = document->MatchingEndTagOffset(listIndex, end);
            const std::size_t rowsBegin = TagAt(listIndex).endOffset;

            std::string currentSection;
            std::size_t rowStart = rowsBegin;

            const std::size_t first = document->FirstTagAtOrAfter(rowsBegin);
            for (std::size_t index = first; index < document->Tags().size(); ++index)
            {
                const SpecTag& row = TagAt(index);
                if (row.startOffset >= listEnd)
                {
                    break;
                }
                if (row.isEndTag)
                {
                    continue;
                }
                if (row.name != "dt" && row.name != "dd")
                {
                    continue;
                }

                const std::size_t rowEnd = NextRowBoundary(index, listEnd);

                if (row.name == "dt")
                {
                    currentSection = StripTags(document->RawBetween(row.endOffset, rowEnd));
                }
                else
                {
                    if (currentSection.find("Categories") != std::string::npos)
                    {
                        const std::string category = StripTags(document->RawBetween(row.endOffset, rowEnd));
                        if (!category.empty() && category != "None." &&
                            std::find(extracted.categories.begin(), extracted.categories.end(), category) ==
                                extracted.categories.end())
                        {
                            extracted.categories.push_back(category);
                        }
                    }
                    else if (currentSection.find("Content model") != std::string::npos)
                    {
                        const std::string contentModel = StripTags(document->RawBetween(row.endOffset, rowEnd));
                        if (!contentModel.empty())
                        {
                            if (!extracted.contentModel.empty())
                            {
                                extracted.contentModel += " ";
                            }
                            extracted.contentModel += contentModel;
                        }
                    }
                    else if (currentSection.find("Content attributes") != std::string::npos)
                    {
                        const std::string rowText = StripTags(document->RawBetween(row.endOffset, rowEnd));
                        if (rowText.find("Global attributes") != std::string::npos)
                        {
                            extracted.globalAttributes = true;
                        }
                        CollectAttributeNamesFromRegion(row.endOffset, rowEnd, extracted.attributes);
                    }
                    else if (currentSection.find("DOM interface") != std::string::npos)
                    {
                        if (extracted.domInterface.empty())
                        {
                            const std::size_t interfaceIndex = FindInterfaceDefinition(row.endOffset, rowEnd);
                            if (interfaceIndex < document->Tags().size())
                            {
                                extracted.domInterface =
                                    Trim(document->TextBetween(TagAt(interfaceIndex).endOffset,
                                                               document->MatchingEndTagOffset(interfaceIndex, rowEnd)));
                            }
                        }
                    }
                }

                rowStart = rowEnd;
            }
        }

        std::size_t NextRowBoundary(std::size_t rowIndex, std::size_t limit) const
        {
            for (std::size_t index = rowIndex + 1; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset >= limit)
                {
                    break;
                }
                if (!TagAt(index).isEndTag && (TagAt(index).name == "dt" || TagAt(index).name == "dd"))
                {
                    return TagAt(index).startOffset;
                }
            }
            return limit;
        }

        std::size_t FindInterfaceDefinition(std::size_t begin, std::size_t end) const
        {
            const std::size_t first = document->FirstTagAtOrAfter(begin);
            for (std::size_t index = first; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset >= end)
                {
                    break;
                }
                if (!TagAt(index).isEndTag && TagAt(index).name == "dfn" &&
                    TagAt(index).HasAttribute("interface"))
                {
                    return index;
                }
            }
            return document->Tags().size();
        }

        // -------------------------------------------------------------------
        // Global and event handler attributes
        // -------------------------------------------------------------------

        std::size_t FindHeadingTagIndexByText(const std::string& headingText) const
        {
            for (const std::size_t headingIndex : headingIndices)
            {
                const SpecTag& heading = TagAt(headingIndex);
                if (StripTags(document->RawBetween(heading.endOffset,
                                                   document->MatchingEndTagOffset(headingIndex,
                                                                                  heading.endOffset))) == headingText)
                {
                    return headingIndex;
                }
            }
            return document->Tags().size();
        }

        std::size_t FindFirstTagByName(const std::string& name, std::size_t begin, std::size_t end) const
        {
            const std::size_t first = document->FirstTagAtOrAfter(begin);
            for (std::size_t index = first; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset >= end)
                {
                    break;
                }
                if (!TagAt(index).isEndTag && TagAt(index).name == name)
                {
                    return index;
                }
            }
            return document->Tags().size();
        }

        void CollectCodeNamesFromTag(std::size_t tagIndex,
                                     const std::string& dataXPrefix,
                                     std::vector<std::string>& out) const
        {
            if (tagIndex >= document->Tags().size())
            {
                return;
            }

            const std::size_t limit = document->MatchingEndTagOffset(tagIndex, document->Text().size());
            const std::size_t first = document->FirstTagAtOrAfter(TagAt(tagIndex).endOffset);
            for (std::size_t index = first; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset >= limit)
                {
                    break;
                }
                if (TagAt(index).isEndTag || TagAt(index).name != "code")
                {
                    continue;
                }

                if (!dataXPrefix.empty() && TagAt(index).GetAttribute("data-x").compare(0, dataXPrefix.size(), dataXPrefix) != 0)
                {
                    continue;
                }

                const std::string name = CleanCodeName(
                    document->TextBetween(TagAt(index).endOffset,
                                          document->MatchingEndTagOffset(index, limit)));
                if (!name.empty() && std::find(out.begin(), out.end(), name) == out.end())
                {
                    out.push_back(name);
                }
            }
        }

        void CollectGlobalAttributes()
        {
            const std::size_t headingIndex = FindHeadingTagIndexByText("Global attributes");
            if (headingIndex >= document->Tags().size())
            {
                std::cerr << "[SpecExtractor] warning: global attributes section not located\n";
                return;
            }

            const std::size_t headingPosition = HeadingPositionFor(TagAt(headingIndex).startOffset);
            std::size_t begin = 0;
            std::size_t end = 0;
            HeadingRegion(headingPosition, begin, end);

            const std::size_t listIndex = FindFirstTagByName("ul", TagAt(headingIndex).endOffset, end);
            std::vector<std::string> names;
            CollectCodeNamesFromTag(listIndex, "attr-", names);

            for (const std::string& name : names)
            {
                ExtractedAttribute attribute;
                attribute.name = name;
                attribute.kind = "global";
                globalAttributes.push_back(attribute);
            }
        }

        /// Nearest preceding non-end tag with the given name.
        std::size_t FindEnclosingTagByName(const std::string& name, std::size_t offset) const
        {
            std::size_t result = document->Tags().size();
            for (std::size_t index = 0; index < document->Tags().size(); ++index)
            {
                if (TagAt(index).startOffset > offset)
                {
                    break;
                }
                if (!TagAt(index).isEndTag && TagAt(index).name == name)
                {
                    result = index;
                }
            }
            return result;
        }

        void CollectEventHandlerAttributes()
        {
            // The section heading text is wrapped in <span> markup, so locate the
            // handler list by picking the list containing the most handler
            // definitions rather than by matching prose.
            const std::vector<SpecTag>& tags = document->Tags();
            std::size_t bestIndex = tags.size();
            std::vector<std::string> bestNames;

            for (std::size_t index = 0; index < tags.size(); ++index)
            {
                if (tags[index].isEndTag || tags[index].name != "ul")
                {
                    continue;
                }

                std::vector<std::string> names;
                CollectCodeNamesFromTag(index, "handler-", names);
                if (names.size() > bestNames.size())
                {
                    bestNames = names;
                    bestIndex = index;
                }
            }

            if (bestIndex >= tags.size())
            {
                std::cerr << "[SpecExtractor] warning: event handler section not located\n";
                return;
            }

            const std::vector<std::string>& names = bestNames;

            for (const std::string& name : names)
            {
                ExtractedAttribute attribute;
                attribute.name = name;
                attribute.kind = "eventhandler";
                eventHandlerAttributes.push_back(attribute);
            }
        }

        // -------------------------------------------------------------------
        // Attribute index
        // -------------------------------------------------------------------

        void BuildAttributeIndex()
        {
            for (const ExtractedElement& element : elements)
            {
                for (const std::string& attributeName : element.attributes)
                {
                    ExtractedAttribute& attribute = elementAttributes[attributeName];
                    attribute.name = attributeName;
                    attribute.kind = "element";

                    if (std::find(attribute.allowedElements.begin(), attribute.allowedElements.end(),
                                  element.name) == attribute.allowedElements.end())
                    {
                        attribute.allowedElements.push_back(element.name);
                    }

                    if (element.obsolete)
                    {
                        attribute.obsolete = true;
                    }
                }
            }

            for (auto& entry : elementAttributes)
            {
                std::sort(entry.second.allowedElements.begin(), entry.second.allowedElements.end());
            }
        }

        void CollectDeprecatedAttributes()
        {
            const std::vector<SpecTag>& tags = document->Tags();

            for (std::size_t index = 0; index < tags.size(); ++index)
            {
                const SpecTag& tag = tags[index];
                if (tag.isEndTag || tag.name != "dfn")
                {
                    continue;
                }

                if (!tag.HasAttribute("element-attr") && !tag.HasAttribute("attribute"))
                {
                    continue;
                }

                const std::size_t headingPosition = HeadingPositionFor(tag.startOffset);
                if (headingPosition == headingIndices.size() || !RegionIsObsolete(headingPosition))
                {
                    continue;
                }

                const std::string name = CleanCodeName(
                    document->TextBetween(tag.endOffset,
                                          document->MatchingEndTagOffset(index, tag.endOffset)));
                if (!name.empty())
                {
                    obsoleteAttributeNames.insert(name);
                }
            }
        }

        // -------------------------------------------------------------------
        // Output
        // -------------------------------------------------------------------

        void WriteElements(const fs::path& path) const
        {
            JsonValue root = JsonValue::MakeObject();
            root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.HtmlElements.v1"));
            root.Set("source", JsonValue::MakeString("Reference/HTML/source"));
            root.Set("generator", JsonValue::MakeString("RoboHTMLCompiler SpecExtractor"));

            JsonValue list = JsonValue::MakeArray();
            for (const ExtractedElement& element : elements)
            {
                JsonValue record = JsonValue::MakeObject();
                record.Set("name", JsonValue::MakeString(element.name));
                record.Set("namespace", JsonValue::MakeString("HTML"));
                record.Set("sectionId", JsonValue::MakeString(element.sectionId));
                record.Set("obsolete", JsonValue::MakeBoolean(element.obsolete));
                record.Set("globalAttributes", JsonValue::MakeBoolean(element.globalAttributes));
                record.Set("domInterface", JsonValue::MakeString(element.domInterface));
                record.Set("contentModel", JsonValue::MakeString(element.contentModel));
                record.Set("sourceLine", JsonValue::MakeInteger(static_cast<long long>(element.line)));

                JsonValue categories = JsonValue::MakeArray();
                for (const std::string& category : element.categories)
                {
                    categories.Append(JsonValue::MakeString(category));
                }
                record.Set("categories", std::move(categories));

                JsonValue attributes = JsonValue::MakeArray();
                for (const std::string& attributeName : element.attributes)
                {
                    attributes.Append(JsonValue::MakeString(attributeName));
                }
                record.Set("attributes", std::move(attributes));

                list.Append(std::move(record));
            }

            root.Set("elements", std::move(list));
            WriteFile(path, root.Serialize(true));
            std::cout << "[SpecExtractor] wrote " << path.string() << "\n";
        }

        void WriteAttributes(const fs::path& path) const
        {
            JsonValue root = JsonValue::MakeObject();
            root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.HtmlAttributes.v1"));
            root.Set("source", JsonValue::MakeString("Reference/HTML/source"));

            JsonValue list = JsonValue::MakeArray();
            for (const auto& entry : elementAttributes)
            {
                JsonValue record = JsonValue::MakeObject();
                record.Set("name", JsonValue::MakeString(entry.second.name));
                record.Set("namespace", JsonValue::MakeString("HTML"));
                record.Set("kind", JsonValue::MakeString(entry.second.kind));
                record.Set("obsolete", JsonValue::MakeBoolean(entry.second.obsolete));

                JsonValue allowed = JsonValue::MakeArray();
                for (const std::string& elementName : entry.second.allowedElements)
                {
                    allowed.Append(JsonValue::MakeString(elementName));
                }
                record.Set("allowedElements", std::move(allowed));

                list.Append(std::move(record));
            }

            root.Set("attributes", std::move(list));
            WriteFile(path, root.Serialize(true));
            std::cout << "[SpecExtractor] wrote " << path.string() << "\n";
        }

        void WriteGlobalAttributes(const fs::path& path) const
        {
            JsonValue root = JsonValue::MakeObject();
            root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.GlobalAttributes.v1"));
            root.Set("source", JsonValue::MakeString("Reference/HTML/source"));

            JsonValue list = JsonValue::MakeArray();
            for (const ExtractedAttribute& attribute : globalAttributes)
            {
                JsonValue record = JsonValue::MakeObject();
                record.Set("name", JsonValue::MakeString(attribute.name));
                record.Set("namespace", JsonValue::MakeString("HTML"));
                list.Append(std::move(record));
            }

            root.Set("attributes", std::move(list));
            WriteFile(path, root.Serialize(true));
            std::cout << "[SpecExtractor] wrote " << path.string() << "\n";
        }

        void WriteEventHandlerAttributes(const fs::path& path) const
        {
            JsonValue root = JsonValue::MakeObject();
            root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.EventHandlerAttributes.v1"));
            root.Set("source", JsonValue::MakeString("Reference/HTML/source"));
            root.Set("policy",
                     JsonValue::MakeString("Content attribute event handlers are recognised so that the "
                                           "compiler can reject them with a precise diagnostic: behaviour "
                                           "belongs to JavaScript modules."));

            JsonValue list = JsonValue::MakeArray();
            for (const ExtractedAttribute& attribute : eventHandlerAttributes)
            {
                JsonValue record = JsonValue::MakeObject();
                record.Set("name", JsonValue::MakeString(attribute.name));
                list.Append(std::move(record));
            }

            root.Set("attributes", std::move(list));
            WriteFile(path, root.Serialize(true));
            std::cout << "[SpecExtractor] wrote " << path.string() << "\n";
        }

        void WriteDeprecatedElements(const fs::path& path) const
        {
            JsonValue root = JsonValue::MakeObject();
            root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.DeprecatedElements.v1"));
            root.Set("source", JsonValue::MakeString("Reference/HTML/source"));
            root.Set("policy",
                     JsonValue::MakeString("Obsolete features have no modern native equivalent; strict mode "
                                           "rejects them outright."));

            JsonValue list = JsonValue::MakeArray();
            for (const std::string& name : obsoleteElementNames)
            {
                list.Append(JsonValue::MakeString(name));
            }
            root.Set("elements", std::move(list));

            JsonValue attributes = JsonValue::MakeArray();
            for (const std::string& name : obsoleteAttributeNames)
            {
                attributes.Append(JsonValue::MakeString(name));
            }
            root.Set("attributes", std::move(attributes));

            WriteFile(path, root.Serialize(true));
            std::cout << "[SpecExtractor] wrote " << path.string() << "\n";
        }

        void WriteCoverage(const fs::path& path, const fs::path& referenceDirectory) const
        {
            JsonValue root = JsonValue::MakeObject();
            root.Set("project", JsonValue::MakeString("RoboHTMLCompiler"));
            root.Set("generatedBy", JsonValue::MakeString("RoboHTMLCompiler SpecExtractor"));

            JsonValue html = JsonValue::MakeObject();
            JsonValue htmlImplemented = JsonValue::MakeArray();
            JsonValue htmlDeprecated = JsonValue::MakeArray();

            for (const ExtractedElement& element : elements)
            {
                if (element.obsolete)
                {
                    htmlDeprecated.Append(JsonValue::MakeString(element.name));
                }
                else
                {
                    htmlImplemented.Append(JsonValue::MakeString(element.name));
                }
            }

            html.Set("implemented", std::move(htmlImplemented));
            html.Set("partial", JsonValue::MakeArray());
            html.Set("pending", JsonValue::MakeArray());
            html.Set("unsupported", JsonValue::MakeArray());
            html.Set("deprecated", std::move(htmlDeprecated));
            root.Set("html", std::move(html));

            JsonValue attributes = JsonValue::MakeObject();
            JsonValue attributeImplemented = JsonValue::MakeArray();
            JsonValue attributeDeprecated = JsonValue::MakeArray();
            for (const auto& entry : elementAttributes)
            {
                if (entry.second.obsolete)
                {
                    attributeDeprecated.Append(JsonValue::MakeString(entry.first));
                }
                else
                {
                    attributeImplemented.Append(JsonValue::MakeString(entry.first));
                }
            }
            attributes.Set("implemented", std::move(attributeImplemented));
            attributes.Set("partial", JsonValue::MakeArray());
            attributes.Set("pending", JsonValue::MakeArray());
            attributes.Set("unsupported", JsonValue::MakeArray());
            attributes.Set("deprecated", std::move(attributeDeprecated));
            root.Set("attributes", std::move(attributes));

            JsonValue globals = JsonValue::MakeObject();
            JsonValue globalNames = JsonValue::MakeArray();
            for (const ExtractedAttribute& attribute : globalAttributes)
            {
                globalNames.Append(JsonValue::MakeString(attribute.name));
            }
            globals.Set("implemented", std::move(globalNames));
            globals.Set("partial", JsonValue::MakeArray());
            globals.Set("pending", JsonValue::MakeArray());
            globals.Set("unsupported", JsonValue::MakeArray());
            root.Set("globalAttributes", std::move(globals));

            JsonValue behaviour = JsonValue::MakeObject();
            JsonValue eventHandlers = JsonValue::MakeArray();
            for (const ExtractedAttribute& attribute : eventHandlerAttributes)
            {
                eventHandlers.Append(JsonValue::MakeString(attribute.name));
            }
            behaviour.Set("eventHandlerAttributes", std::move(eventHandlers));
            behaviour.Set("eventHandlerStatus", JsonValue::MakeString("recognised-and-rejected"));
            root.Set("behaviour", std::move(behaviour));

            // The reference directory holds the WHATWG HTML Standard only. SVG,
            // MathML Core and WAI-ARIA vocabulary is therefore authored by hand
            // in Registry/ and counted by the RegistryCompiler stage.
            root.Set("svg", MakeExternalReferenceSection("SVG 2 / SVG attributes (Registry/SvgElementRegistry.json)"));
            root.Set("mathml", MakeExternalReferenceSection("MathML Core (Registry/MathMlElementRegistry.json)"));
            root.Set("aria", MakeExternalReferenceSection("WAI-ARIA 1.2 (Registry/AriaAttributeRegistry.json)"));

            JsonValue decisions = JsonValue::MakeArray();
            decisions.Append(JsonValue::MakeString(
                "The reference document's algorithms, IDL definitions and rendering rules describe user-agent "
                "behaviour, not source vocabulary, and are therefore not converted into registries."));
            decisions.Append(JsonValue::MakeString(
                "Content attribute event handlers (on*) are registered so that the compiler can reject them "
                "with a precise diagnostic instead of reporting an unknown attribute."));
            decisions.Append(JsonValue::MakeString(
                "The style content attribute is required by the standard but forbidden by RoboHTMLCompiler: "
                "it is registered and then rejected by the inline-CSS rule."));
            decisions.Append(JsonValue::MakeString(
                "Obsolete features are registered as deprecated so strict mode can reject them explicitly "
                "rather than silently ignoring them."));
            root.Set("decisions", std::move(decisions));

            JsonValue references = JsonValue::MakeArray();
            for (const std::string& reference : EnumerateReferences(referenceDirectory))
            {
                JsonValue record = JsonValue::MakeObject();
                record.Set("path", JsonValue::MakeString(reference));
                record.Set("role", JsonValue::MakeString(reference == "Reference/HTML/source"
                                                             ? "primary-vocabulary-source"
                                                             : "auxiliary"));
                record.Set("status", JsonValue::MakeString(reference == "Reference/HTML/source"
                                                               ? "processed"
                                                               : "not-compiler-input"));
                references.Append(std::move(record));
            }
            root.Set("references", std::move(references));

            WriteFile(path, root.Serialize(true));
            std::cout << "[SpecExtractor] wrote " << path.string() << "\n";
        }

        static JsonValue MakeExternalReferenceSection(const std::string& origin)
        {
            JsonValue section = JsonValue::MakeObject();
            section.Set("implemented", JsonValue::MakeArray());
            section.Set("partial", JsonValue::MakeArray());
            section.Set("pending", JsonValue::MakeArray());
            section.Set("unsupported", JsonValue::MakeArray());
            section.Set("origin", JsonValue::MakeString(origin));
            section.Set("filledBy", JsonValue::MakeString("RoboHTMLCompiler RegistryCompiler"));
            return section;
        }

        static std::vector<std::string> EnumerateReferences(const fs::path& referenceDirectory)
        {
            std::vector<std::string> result;
            if (!fs::exists(referenceDirectory))
            {
                return result;
            }

            std::error_code errorCode;
            for (fs::recursive_directory_iterator iterator(referenceDirectory, errorCode), end;
                 iterator != end && !errorCode;
                 iterator.increment(errorCode))
            {
                if (iterator->is_regular_file())
                {
                    result.push_back("Reference/HTML/" +
                                     fs::relative(iterator->path(), referenceDirectory).generic_string());
                }
            }

            std::sort(result.begin(), result.end());
            return result;
        }

        static void WriteFile(const fs::path& path, const std::string& contents)
        {
            fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream << contents;
        }
    };
}

int main(int argumentCount, char** argumentValues)
{
    fs::path referenceDirectory = "Reference/HTML";
    fs::path registryOutputDirectory = "Generated/Registry";
    fs::path coverageOutputPath = "Generated/ImplementationCoverage.json";

    if (argumentCount > 1)
    {
        referenceDirectory = argumentValues[1];
    }
    if (argumentCount > 2)
    {
        registryOutputDirectory = argumentValues[2];
    }
    if (argumentCount > 3)
    {
        coverageOutputPath = argumentValues[3];
    }

    SpecExtractor extractor;
    if (!extractor.Run(referenceDirectory, registryOutputDirectory, coverageOutputPath))
    {
        return 1;
    }

    return 0;
}
