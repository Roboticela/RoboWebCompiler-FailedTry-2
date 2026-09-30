// ---------------------------------------------------------------------------
// RoboHTMLCompiler - CssSpecExtractor.
//
// The CSS reference is the CSS Working Group's drafts, checked out under
// Reference/CSS: one directory per module, Bikeshed source in Overview.bs. This
// tool mines that source into checked-in knowledge and then gets out of the way.
//
//     Reference/CSS/**/*.bs
//         --CssSpecExtractor-->
//             CompilerKnowledge/Vocabulary/CssVocabulary.json   the inventory
//             CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h
//             CompilerKnowledge/CssReferenceAudit.json          what is consumed
//
// Nothing in the compiler reads the reference at run time. The generated header
// is what the CSS front end is compiled against, and it is what makes the
// difference between "a property the reference defines and this compiler reports
// as unimplemented" (RCC010) and "a name that is not a property at all" (RCC001)
// a property of checked-in knowledge rather than of a hand-typed list.
//
// The inventory is one file rather than one file per vocabulary kind: a module
// declares properties, descriptors, at-rules and selectors, and reference
// consumption happens module by module, so a per-kind split would make the unit
// of consumption something no file describes.
//
// Usage:
//     CssSpecExtractor [referenceDir] [vocabularyDir]
//                      [--header FILE] [--audit FILE] [--check] [--retire a,b,c]
//
// `--check` exits non-zero when the reference defines something the audit has
// neither retired nor accounted for, so it can gate a build. `--retire` is how
// the reference shrinks: it records the modules as consumed and removes their
// directories, and when the last one goes the audit says so.
// ---------------------------------------------------------------------------

#include "Json.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using RoboHTMLCompiler::Tools::JsonValue;

namespace
{
    /// Directory entries under Reference/CSS that are not specifications. The
    /// drafts repository is a working tree, so it carries its own plumbing, a
    /// template module and one joke specification; reading them literally would
    /// put `foo` in the compiler's idea of CSS.
    struct SkippedModule
    {
        const char* name;
        const char* reason;
    };

    const SkippedModule SkippedModules[] = {
        { ".github", "repository configuration" },
        { "bin", "repository tooling" },
        { "shared", "shared assets for the drafts, not a specification" },
        { "css-module", "the drafts repository's template module" },
        { "css-egg-1", "an April Fools specification" },
    };

    /// Examples and placeholders written in the same `propdef` markup a real
    /// definition uses: tutorial text in css-display-3, a hypothetical value in
    /// css-color-hdr-1, a placeholder in css-backgrounds-4. They are named with
    /// the reason they are not properties rather than filtered by a pattern that
    /// would eventually eat a real one.
    const SkippedModule SkippedPropertyNames[] = {
        { "background-tbd", "a placeholder in css-backgrounds-4, not a property" },
        { "display-or-not", "an example in css-display-3 and css-display-4" },
        { "foo", "an example in css-color-hdr-1" },
        { "shorthand-foo", "an example in css-color-hdr-1" },
    };

    const char* SkipReasonForModule(const std::string& name)
    {
        for (const SkippedModule& module : SkippedModules)
        {
            if (name == module.name)
            {
                return module.reason;
            }
        }

        return nullptr;
    }

    const char* SkipReasonForProperty(const std::string& name)
    {
        for (const SkippedModule& entry : SkippedPropertyNames)
        {
            if (name == entry.name)
            {
                return entry.reason;
            }
        }

        return nullptr;
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

    std::string ToLower(const std::string& value)
    {
        std::string lowered = value;
        for (char& character : lowered)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        return lowered;
    }

    std::string CollapseWhitespace(const std::string& value)
    {
        std::string collapsed;
        bool pendingSpace = false;

        for (const char character : value)
        {
            if (std::isspace(static_cast<unsigned char>(character)) != 0)
            {
                pendingSpace = !collapsed.empty();
                continue;
            }

            if (pendingSpace)
            {
                collapsed.push_back(' ');
                pendingSpace = false;
            }
            collapsed.push_back(character);
        }

        return collapsed;
    }

    /// The entity references Bikeshed sources use in prose. A named entity that
    /// is not here is left alone rather than guessed at.
    std::string DecodeEntities(const std::string& value)
    {
        static const std::pair<const char*, const char*> table[] = {
            { "&lt;", "<" },     { "&gt;", ">" },     { "&amp;", "&" },    { "&quot;", "\"" },
            { "&nbsp;", " " },   { "&mdash;", "-" },  { "&ndash;", "-" },  { "&hellip;", "..." },
            { "&times;", "x" },  { "&equiv;", "==" }, { "&#39;", "'" },
        };

        std::string decoded;
        decoded.reserve(value.size());

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] != '&')
            {
                decoded.push_back(value[index]);
                continue;
            }

            const std::size_t terminator = value.find(';', index);
            if (terminator == std::string::npos || terminator - index > 8)
            {
                decoded.push_back(value[index]);
                continue;
            }

            const std::string reference = value.substr(index, terminator - index + 1);
            bool replaced = false;

            for (const auto& entry : table)
            {
                if (reference == entry.first)
                {
                    decoded += entry.second;
                    replaced = true;
                    break;
                }
            }

            // A numeric reference is decoded too: the reference uses them for the
            // characters those drafts cannot write directly.
            if (!replaced && reference.size() > 3 && reference[1] == '#')
            {
                const bool hexadecimal = reference[2] == 'x' || reference[2] == 'X';
                const std::string digits = reference.substr(hexadecimal ? 3 : 2,
                                                            reference.size() - (hexadecimal ? 4 : 3));
                long value_ = 0;
                bool valid = !digits.empty();

                for (const char digit : digits)
                {
                    const int number = std::isdigit(static_cast<unsigned char>(digit)) != 0
                                           ? digit - '0'
                                           : (hexadecimal ? std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10 : -1);
                    const int base = hexadecimal ? 16 : 10;

                    if (number < 0 || number >= base)
                    {
                        valid = false;
                        break;
                    }
                    value_ = value_ * base + number;
                }

                if (valid && value_ > 0 && value_ < 128)
                {
                    decoded.push_back(static_cast<char>(value_));
                    replaced = true;
                }
            }

            if (replaced)
            {
                index = terminator;
                continue;
            }

            decoded.push_back(value[index]);
        }

        return decoded;
    }

    /// Drops markup from a prose fragment, keeping the text.
    std::string StripTags(const std::string& value)
    {
        std::string plain;

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] != '<')
            {
                plain.push_back(value[index]);
                continue;
            }

            const std::size_t terminator = value.find('>', index);
            if (terminator == std::string::npos)
            {
                break;
            }
            index = terminator;
        }

        return CollapseWhitespace(DecodeEntities(plain));
    }

    /// Answers "which line is this offset on" without rescanning the file: the
    /// same reason the CSS front end keeps a line index of its own.
    class LineIndex
    {
    public:
        explicit LineIndex(const std::string& text)
        {
            std::size_t offset = text.find('\n');
            while (offset != std::string::npos)
            {
                lineStarts.push_back(offset + 1);
                offset = text.find('\n', offset + 1);
            }
        }

        int LineOf(std::size_t offset) const
        {
            const auto after = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset);
            return static_cast<int>(after - lineStarts.begin()) + 1;
        }

    private:
        std::vector<std::size_t> lineStarts;
    };

    bool IsWordCharacter(char character)
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' || character == '_';
    }

    /// True when `attributes` carries `token` as a whole attribute, so that
    /// `id="at-ruledef-layer"` is not mistaken for `at-rule`.
    bool HasAttribute(const std::string& attributes, const std::string& token)
    {
        std::size_t position = attributes.find(token);

        while (position != std::string::npos)
        {
            const bool endsWord =
                position + token.size() >= attributes.size() || !IsWordCharacter(attributes[position + token.size()]);
            const bool startsWord = position == 0 || !IsWordCharacter(attributes[position - 1]);

            if (endsWord && startsWord)
            {
                return true;
            }

            position = attributes.find(token, position + 1);
        }

        return false;
    }

    struct Block
    {
        std::string body;
        std::size_t offset = 0;
    };

    /// Every `<pre>`, `<xmp>` or `<div>` whose class list contains
    /// `className`, with the markup inside it.
    std::vector<Block> FindBlocks(const std::string& text, const std::string& className)
    {
        // Bikeshed sources only ever use the lowercase tags for these blocks, and
        // matching the tag lets the closing tag be found without nesting rules.
        static const char* containers[] = { "pre", "xmp", "div" };

        std::vector<Block> blocks;

        std::size_t search = 0;
        while (true)
        {
            search = text.find("class=", search);
            if (search == std::string::npos)
            {
                break;
            }

            const std::size_t valueBegin = search + 6;
            std::string classValue;
            std::size_t valueEnd = valueBegin;

            if (valueBegin < text.size() && (text[valueBegin] == '"' || text[valueBegin] == '\''))
            {
                const char quote = text[valueBegin];
                const std::size_t close = text.find(quote, valueBegin + 1);
                if (close == std::string::npos)
                {
                    break;
                }
                classValue = text.substr(valueBegin + 1, close - valueBegin - 1);
                valueEnd = close + 1;
            }
            else
            {
                while (valueEnd < text.size() && std::isspace(static_cast<unsigned char>(text[valueEnd])) == 0 &&
                       text[valueEnd] != '>')
                {
                    ++valueEnd;
                }
                classValue = text.substr(valueBegin, valueEnd - valueBegin);
            }

            search = valueEnd;

            bool wanted = false;
            std::istringstream classes(classValue);
            std::string token;
            while (classes >> token)
            {
                if (token == className)
                {
                    wanted = true;
                    break;
                }
            }

            if (!wanted)
            {
                continue;
            }

            const std::size_t tagStart = text.rfind('<', search);
            if (tagStart == std::string::npos)
            {
                continue;
            }

            std::string tag;
            for (const char* candidate : containers)
            {
                const std::size_t length = std::strlen(candidate);
                if (tagStart + 1 + length <= text.size() &&
                    ToLower(text.substr(tagStart + 1, length)) == candidate &&
                    !IsWordCharacter(text[tagStart + 1 + length]))
                {
                    tag = candidate;
                    break;
                }
            }

            if (tag.empty())
            {
                continue;
            }

            const std::size_t openEnd = text.find('>', search);
            if (openEnd == std::string::npos)
            {
                break;
            }

            const std::size_t closeBegin = text.find("</" + tag, openEnd);
            if (closeBegin == std::string::npos)
            {
                break;
            }

            Block block;
            block.body = text.substr(openEnd + 1, closeBegin - openEnd - 1);
            block.offset = tagStart;
            blocks.push_back(std::move(block));

            search = closeBegin;
        }

        return blocks;
    }

    /// The `Key: value` lines of a propdef or descdef block, lower-cased keys.
    std::map<std::string, std::string> ParseFields(const std::string& body)
    {
        std::map<std::string, std::string> fields;

        std::size_t position = 0;
        while (position <= body.size())
        {
            const std::size_t terminator = body.find('\n', position);
            const std::string line =
                body.substr(position, (terminator == std::string::npos ? body.size() : terminator) - position);

            const std::size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                const std::string key = ToLower(Trim(line.substr(0, colon)));
                const std::string value = Trim(line.substr(colon + 1));

                // A field name is a word or two; anything else is prose that
                // happens to contain a colon, such as a grammar example.
                if (!key.empty() && key.size() < 32 && key.find('<') == std::string::npos && !value.empty())
                {
                    fields[key] = value;
                }
            }

            if (terminator == std::string::npos)
            {
                break;
            }
            position = terminator + 1;
        }

        return fields;
    }

    struct NamedDefinition
    {
        std::string name;
        std::string module;
        std::string source;
        int line = 0;
    };

    /// The definitions a draft writes as `<dfn at-rule>@layer</dfn>` and its
    /// siblings. The attribute, not the id, is the definition: ids are optional
    /// in Bikeshed and most of the reference does not use them.
    std::vector<NamedDefinition> FindDfnDefinitions(const std::string& text, const std::string& module,
                                                    const std::string& source, const std::string& kind,
                                                    const LineIndex& lines, bool stripAtSign, bool stripParens)
    {
        std::vector<NamedDefinition> definitions;

        std::size_t search = 0;
        while ((search = text.find("<dfn", search)) != std::string::npos)
        {
            const std::size_t openEnd = text.find('>', search);
            if (openEnd == std::string::npos)
            {
                break;
            }

            const std::string attributes = text.substr(search + 4, openEnd - search - 4);
            search = openEnd;

            if (!HasAttribute(attributes, kind))
            {
                continue;
            }

            const std::size_t closeBegin = text.find("</dfn", openEnd);
            if (closeBegin == std::string::npos)
            {
                break;
            }

            std::string name = StripTags(text.substr(openEnd + 1, closeBegin - openEnd - 1));

            if (stripAtSign && !name.empty() && name[0] == '@')
            {
                name.erase(0, 1);
            }
            if (stripParens)
            {
                const std::size_t paren = name.find('(');
                if (paren != std::string::npos)
                {
                    name = name.substr(0, paren);
                }
            }

            if (name.empty() || name.find(' ') != std::string::npos)
            {
                search = closeBegin;
                continue;
            }

            NamedDefinition definition;
            definition.name = ToLower(name);
            definition.module = module;
            definition.source = source;
            definition.line = lines.LineOf(search);
            definitions.push_back(std::move(definition));

            search = closeBegin;
        }

        return definitions;
    }

    bool IsInherited(const std::string& value)
    {
        return ToLower(Trim(value)).rfind("yes", 0) == 0;
    }

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return std::string();
        }

        std::ostringstream contents;
        contents << stream.rdbuf();
        return contents.str();
    }

    bool WriteFile(const fs::path& path, const std::string& contents)
    {
        std::error_code error;
        if (!path.parent_path().empty())
        {
            fs::create_directories(path.parent_path(), error);
        }

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }

        stream << contents;
        return stream.good();
    }

    JsonValue MakeReasonArray(const std::vector<std::pair<std::string, std::string>>& entries)
    {
        JsonValue array = JsonValue::MakeArray();

        for (const auto& entry : entries)
        {
            JsonValue value = JsonValue::MakeObject();
            value.Set("name", JsonValue::MakeString(entry.first));
            value.Set("reason", JsonValue::MakeString(entry.second));
            array.Append(std::move(value));
        }

        return array;
    }

    JsonValue MakeStringArray(const std::set<std::string>& values)
    {
        JsonValue array = JsonValue::MakeArray();
        for (const std::string& value : values)
        {
            array.Append(JsonValue::MakeString(value));
        }
        return array;
    }
}

/// The whole reference, as the compiler's knowledge.
class CssVocabulary
{
public:
    struct Property
    {
        std::string name;
        std::string module;
        std::string source;
        std::string declared;         // the Name field, which may group names
        std::string value;
        std::string initial;
        bool inherited = false;
        std::string computedValue;
        std::string animationType;
        std::string appliesTo;
        std::string percentages;
        std::string logicalGroup;
        int line = 0;
    };

    struct Descriptor
    {
        std::string name;
        std::string module;
        std::string source;
        std::string forRule;
        std::string value;
        std::string type;
        std::string initial;
        int line = 0;
    };

    struct Module
    {
        std::string name;
        std::set<std::string> files;
        std::vector<Property> properties;
        std::vector<Descriptor> descriptors;
        std::vector<NamedDefinition> atRules;
        std::vector<NamedDefinition> selectors;
        std::vector<NamedDefinition> functions;
    };

    std::vector<Module> modules;
    /// Directory entries that are not specifications, with the reason.
    std::vector<std::pair<std::string, std::string>> skippedModules;
    /// Every module directory, whether or not it carries Bikeshed source.
    std::set<std::string> allModuleNames;
    /// The modules that do carry it, which is what can be mined.
    std::set<std::string> moduleNames;
    std::set<std::string> definedProperties;
    /// Names that look like property definitions but are not, with the reason.
    std::vector<std::pair<std::string, std::string>> skippedProperties;

    bool Extract(const fs::path& referenceDirectory)
    {
        std::error_code error;
        std::vector<fs::path> files;

        // A module directory without Bikeshed source cannot be mined from it:
        // some drafts ship only the generated Overview.html, and a few entries
        // are not specifications at all. They are named rather than ignored, so
        // the audit can say the reference is not finished rather than leave a
        // hole no file mentions.
        for (const fs::directory_entry& entry : fs::directory_iterator(referenceDirectory, error))
        {
            if (!entry.is_directory(error))
            {
                continue;
            }

            const std::string name = entry.path().filename().generic_string();
            const char* reason = SkipReasonForModule(name);

            if (reason != nullptr)
            {
                skippedModules.push_back(std::make_pair(name, std::string(reason)));
                continue;
            }

            allModuleNames.insert(name);
        }

        for (fs::recursive_directory_iterator entry(referenceDirectory, error), end; entry != end; entry.increment(error))
        {
            if (!entry->is_regular_file())
            {
                continue;
            }

            if (ToLower(entry->path().extension().string()) != ".bs")
            {
                continue;
            }

            files.push_back(entry->path());
        }

        std::sort(files.begin(), files.end());

        std::map<std::string, std::size_t> byName;

        for (const fs::path& file : files)
        {
            const fs::path relative = fs::relative(file, referenceDirectory);
            const std::string relativeText = relative.generic_string();

            const std::size_t separator = relativeText.find('/');
            if (separator == std::string::npos || separator == 0)
            {
                continue;
            }

            const std::string moduleName = relativeText.substr(0, separator);

            // A skipped directory is skipped whole: its files are not vocabulary
            // either, however much they look like definitions.
            if (SkipReasonForModule(moduleName) != nullptr)
            {
                continue;
            }

            const std::string text = ReadFile(file);

            if (text.empty())
            {
                continue;
            }

            moduleNames.insert(moduleName);

            const auto existing = byName.find(moduleName);
            if (existing == byName.end())
            {
                byName[moduleName] = modules.size();
                Module module;
                module.name = moduleName;
                modules.push_back(std::move(module));
            }

            Module& module = modules[byName[moduleName]];
            module.files.insert(relativeText.substr(separator + 1));

            const LineIndex lines(text);

            for (const Block& block : FindBlocks(text, "propdef"))
            {
                const std::map<std::string, std::string> fields = ParseFields(block.body);
                const auto declared = fields.find("name");

                if (declared == fields.end() || declared->second.empty())
                {
                    continue;
                }

                // `Name: border-top, border-right` defines a group: each name is
                // a property in its own right, and the group is the shorthand.
                std::vector<std::string> names;
                std::istringstream nameStream(declared->second);
                std::string name;
                while (std::getline(nameStream, name, ','))
                {
                    const std::string trimmed = Trim(name);

                    if (trimmed.empty() || trimmed.rfind("--", 0) == 0)
                    {
                        continue;
                    }

                    const std::string propertyName = ToLower(trimmed);
                    const char* reason = SkipReasonForProperty(propertyName);

                    if (reason != nullptr)
                    {
                        skippedProperties.push_back(std::make_pair(propertyName, std::string(reason)));
                        continue;
                    }

                    names.push_back(propertyName);
                }

                for (const std::string& propertyName : names)
                {
                    Property property;
                    property.name = propertyName;
                    property.module = moduleName;
                    property.source = relativeText;
                    property.declared = declared->second;
                    property.value = Field(fields, "value");
                    property.initial = Field(fields, "initial");
                    property.inherited = IsInherited(Field(fields, "inherited"));
                    property.computedValue = Field(fields, "computed value");
                    property.animationType = Field(fields, "animation type");
                    property.appliesTo = Field(fields, "applies to");
                    property.percentages = Field(fields, "percentages");
                    property.logicalGroup = Field(fields, "logical property group");
                    property.line = lines.LineOf(block.offset);

                    definedProperties.insert(propertyName);
                    module.properties.push_back(std::move(property));
                }
            }

            for (const Block& block : FindBlocks(text, "descdef"))
            {
                const std::map<std::string, std::string> fields = ParseFields(block.body);
                const auto declared = fields.find("name");

                if (declared == fields.end() || declared->second.empty())
                {
                    continue;
                }

                Descriptor descriptor;
                descriptor.name = ToLower(Trim(declared->second));
                descriptor.module = moduleName;
                descriptor.source = relativeText;
                descriptor.forRule = Field(fields, "for");
                descriptor.value = Field(fields, "value");
                descriptor.type = Field(fields, "type");
                descriptor.initial = Field(fields, "initial");
                descriptor.line = lines.LineOf(block.offset);

                module.descriptors.push_back(std::move(descriptor));
            }

            Append(module.atRules,
                   FindDfnDefinitions(text, moduleName, relativeText, "at-rule", lines, true, false));
            Append(module.selectors,
                   FindDfnDefinitions(text, moduleName, relativeText, "selector", lines, false, false));
            Append(module.functions,
                   FindDfnDefinitions(text, moduleName, relativeText, "function", lines, false, true));
        }

        return !modules.empty();
    }

    std::size_t PropertyDefinitionCount() const
    {
        std::size_t count = 0;
        for (const Module& module : modules)
        {
            count += module.properties.size();
        }
        return count;
    }

    JsonValue ToJson() const
    {
        JsonValue root = JsonValue::MakeObject();
        root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.CssVocabulary.v1"));
        root.Set("generatedBy", JsonValue::MakeString("RoboHTMLCompiler CssSpecExtractor"));
        root.Set("reference", JsonValue::MakeString("Reference/CSS"));
        root.Set("policy",
                 JsonValue::MakeString(
                     "One entry per property the CSS Working Group's drafts define. A name is repeated when more "
                     "than one module redefines it, and the module is the unit of reference consumption."));

        std::size_t descriptorCount = 0;
        std::size_t atRuleCount = 0;
        std::size_t selectorCount = 0;
        std::size_t functionCount = 0;

        for (const Module& module : modules)
        {
            descriptorCount += module.descriptors.size();
            atRuleCount += module.atRules.size();
            selectorCount += module.selectors.size();
            functionCount += module.functions.size();
        }

        std::set<std::string> withoutSource;
        for (const std::string& name : allModuleNames)
        {
            if (moduleNames.count(name) == 0)
            {
                withoutSource.insert(name);
            }
        }

        JsonValue summary = JsonValue::MakeObject();
        summary.Set("moduleCount", JsonValue::MakeInteger(static_cast<long long>(allModuleNames.size())));
        summary.Set("modulesWithVocabulary", JsonValue::MakeInteger(static_cast<long long>(modules.size())));
        summary.Set("moduleCountWithoutSource", JsonValue::MakeInteger(static_cast<long long>(withoutSource.size())));
        summary.Set("propertyCount", JsonValue::MakeInteger(static_cast<long long>(definedProperties.size())));
        summary.Set("propertyDefinitionCount", JsonValue::MakeInteger(static_cast<long long>(PropertyDefinitionCount())));
        summary.Set("descriptorCount", JsonValue::MakeInteger(static_cast<long long>(descriptorCount)));
        summary.Set("atRuleCount", JsonValue::MakeInteger(static_cast<long long>(atRuleCount)));
        summary.Set("selectorCount", JsonValue::MakeInteger(static_cast<long long>(selectorCount)));
        summary.Set("functionCount", JsonValue::MakeInteger(static_cast<long long>(functionCount)));
        root.Set("summary", std::move(summary));

        JsonValue moduleArray = JsonValue::MakeArray();

        for (const Module& module : modules)
        {
            JsonValue value = JsonValue::MakeObject();
            value.Set("name", JsonValue::MakeString(module.name));
            value.Set("files", MakeStringArray(module.files));

            JsonValue properties = JsonValue::MakeArray();
            for (const Property& property : module.properties)
            {
                JsonValue entry = JsonValue::MakeObject();
                entry.Set("name", JsonValue::MakeString(property.name));
                entry.Set("declaredAs", JsonValue::MakeString(property.declared));
                entry.Set("value", JsonValue::MakeString(property.value));
                entry.Set("initial", JsonValue::MakeString(property.initial));
                entry.Set("inherited", JsonValue::MakeBoolean(property.inherited));
                entry.Set("computedValue", JsonValue::MakeString(property.computedValue));
                entry.Set("animationType", JsonValue::MakeString(property.animationType));
                entry.Set("appliesTo", JsonValue::MakeString(property.appliesTo));
                entry.Set("percentages", JsonValue::MakeString(property.percentages));
                entry.Set("logicalPropertyGroup", JsonValue::MakeString(property.logicalGroup));
                entry.Set("source", JsonValue::MakeString(property.source));
                entry.Set("line", JsonValue::MakeInteger(property.line));
                properties.Append(std::move(entry));
            }
            value.Set("properties", std::move(properties));

            JsonValue descriptors = JsonValue::MakeArray();
            for (const Descriptor& descriptor : module.descriptors)
            {
                JsonValue entry = JsonValue::MakeObject();
                entry.Set("name", JsonValue::MakeString(descriptor.name));
                entry.Set("for", JsonValue::MakeString(descriptor.forRule));
                entry.Set("value", JsonValue::MakeString(descriptor.value));
                entry.Set("type", JsonValue::MakeString(descriptor.type));
                entry.Set("initial", JsonValue::MakeString(descriptor.initial));
                entry.Set("source", JsonValue::MakeString(descriptor.source));
                entry.Set("line", JsonValue::MakeInteger(descriptor.line));
                descriptors.Append(std::move(entry));
            }
            value.Set("descriptors", std::move(descriptors));

            value.Set("atRules", NamedJson(module.atRules));
            value.Set("selectors", NamedJson(module.selectors));
            value.Set("functions", NamedJson(module.functions));

            moduleArray.Append(std::move(value));
        }

        root.Set("modules", std::move(moduleArray));
        root.Set("modulesWithoutSource", MakeStringArray(withoutSource));
        root.Set("nonSpecificationModules", MakeReasonArray(skippedModules));
        root.Set("excludedPropertyNames", MakeReasonArray(skippedProperties));
        return root;
    }

private:
    static std::string Field(const std::map<std::string, std::string>& fields, const char* key)
    {
        const auto found = fields.find(key);
        return found == fields.end() ? std::string() : found->second;
    }

    static void Append(std::vector<NamedDefinition>& target, std::vector<NamedDefinition> source)
    {
        for (NamedDefinition& definition : source)
        {
            target.push_back(std::move(definition));
        }
    }

    static JsonValue NamedJson(const std::vector<NamedDefinition>& definitions)
    {
        JsonValue array = JsonValue::MakeArray();

        for (const NamedDefinition& definition : definitions)
        {
            JsonValue entry = JsonValue::MakeObject();
            entry.Set("name", JsonValue::MakeString(definition.name));
            entry.Set("source", JsonValue::MakeString(definition.source));
            entry.Set("line", JsonValue::MakeInteger(definition.line));
            array.Append(std::move(entry));
        }

        return array;
    }
};

namespace
{
    int Fail(const std::string& message)
    {
        std::cerr << "CssSpecExtractor: " << message << "\n";
        return 1;
    }

    /// The header the CSS front end is compiled against: which names the
    /// reference defines, and where each one comes from.
    /// What a retired module leaves behind.
    ///
    /// Retiring a module removes its directory, so its vocabulary can no longer
    /// be mined from it. What it defined is not forgotten, because deleting it
    /// is the one operation here that can destroy knowledge: the module's name
    /// stays in the generated module table (a property that cites
    /// `css-cascade-3` would otherwise cite a module the reference no longer
    /// has) and its property names stay in a table of their own, where the
    /// coverage tests still require the compiler to implement every one. The
    /// names come from the last vocabulary that still had the module, and after
    /// that from the audit, which carries them forward.
    struct RetiredKnowledge
    {
        std::set<std::string> moduleNames;
        /// Retired module -> the names it defined, as first known.
        std::map<std::string, std::vector<std::string>> modulePropertyNames;
        /// The same for the at-rules, which the module table alone would not
        /// keep named once the directory is gone.
        std::map<std::string, std::vector<std::string>> moduleAtRuleNames;
        /// The audit entries of the retired modules, kept verbatim so the audit
        /// can keep reporting the inventory that was consumed.
        std::vector<JsonValue> moduleEntries;

        std::vector<std::string> AllPropertyNames() const
        {
            return Unique(modulePropertyNames);
        }

        std::vector<std::pair<std::string, std::string>> AllAtRules() const
        {
            std::vector<std::pair<std::string, std::string>> atRules;

            for (const auto& module : moduleAtRuleNames)
            {
                for (const std::string& name : module.second)
                {
                    atRules.push_back(std::make_pair(name, module.first));
                }
            }

            return atRules;
        }

    private:
        static std::vector<std::string>
        Unique(const std::map<std::string, std::vector<std::string>>& byModule)
        {
            std::set<std::string> names;
            for (const auto& module : byModule)
            {
                names.insert(module.second.begin(), module.second.end());
            }

            return std::vector<std::string>(names.begin(), names.end());
        }
    };

    /// Reads what the previous vocabulary and audit know about `retired`.
    RetiredKnowledge ReadRetiredKnowledge(const std::string& vocabularyJson, const std::string& auditJson,
                                         const std::set<std::string>& retired)
    {
        RetiredKnowledge knowledge;

        if (retired.empty())
        {
            return knowledge;
        }

        std::string error;

        // A vocabulary entry is an object with a `name`; an audit entry is the
        // bare name. Both shapes are read with the same code.
        const auto collect = [&retired, &knowledge](const JsonValue& module,
                                                   const char* member,
                                                   std::map<std::string, std::vector<std::string>>& target) {
            const std::string name = module.GetString("name");

            if (retired.count(name) == 0)
            {
                return;
            }

            knowledge.moduleNames.insert(name);

            std::vector<std::string>& names = target[name];
            const JsonValue* nameList = module.Find(member);

            if (nameList == nullptr || !nameList->IsArray())
            {
                return;
            }

            for (const JsonValue& entry : nameList->GetElements())
            {
                const std::string entryName = entry.IsString() ? entry.AsString() : entry.GetString("name");

                if (!entryName.empty() && std::find(names.begin(), names.end(), entryName) == names.end())
                {
                    names.push_back(entryName);
                }
            }
        };

        const auto collectDocument = [&collect](const JsonValue& document, const char* propertyMember,
                                               const char* atRuleMember, RetiredKnowledge& knowledge) {
            const JsonValue* modules = document.Find("modules");
            if (modules == nullptr || !modules->IsArray())
            {
                return;
            }

            for (const JsonValue& module : modules->GetElements())
            {
                collect(module, propertyMember, knowledge.modulePropertyNames);
                collect(module, atRuleMember, knowledge.moduleAtRuleNames);
            }
        };

        // The vocabulary is authoritative while the module's directory was still
        // there; the audit carries the names once it is gone.
        if (const std::optional<JsonValue> vocabulary = JsonValue::Parse(vocabularyJson, &error))
        {
            collectDocument(*vocabulary, "properties", "atRules", knowledge);
        }

        if (const std::optional<JsonValue> audit = JsonValue::Parse(auditJson, &error))
        {
            collectDocument(*audit, "propertyNames", "atRuleNames", knowledge);

            const JsonValue* modules = audit->Find("modules");
            if (modules != nullptr && modules->IsArray())
            {
                for (const JsonValue& module : modules->GetElements())
                {
                    if (retired.count(module.GetString("name")) == 0)
                    {
                        continue;
                    }

                    JsonValue entry = module;
                    entry.Set("mined", JsonValue::MakeBoolean(false));
                    entry.Set("retired", JsonValue::MakeBoolean(true));
                    const std::string retiredName = module.GetString("name");
                    const std::vector<std::string>& defined = knowledge.modulePropertyNames[retiredName];
                    const std::vector<std::string>& atRules = knowledge.moduleAtRuleNames[retiredName];
                    entry.Set("propertyNames",
                              MakeStringArray(std::set<std::string>(defined.begin(), defined.end())));
                    entry.Set("atRuleNames", MakeStringArray(std::set<std::string>(atRules.begin(), atRules.end())));
                    knowledge.moduleEntries.push_back(std::move(entry));
                }
            }
        }

        for (auto& module : knowledge.modulePropertyNames)
        {
            std::sort(module.second.begin(), module.second.end());
        }

        for (auto& module : knowledge.moduleAtRuleNames)
        {
            std::sort(module.second.begin(), module.second.end());
        }

        return knowledge;
    }

    std::string BuildHeader(const CssVocabulary& vocabulary, const RetiredKnowledge& retired)
    {
        std::vector<CssVocabulary::Property> properties;

        for (const CssVocabulary::Module& module : vocabulary.modules)
        {
            for (const CssVocabulary::Property& property : module.properties)
            {
                properties.push_back(property);
            }
        }

        std::stable_sort(properties.begin(), properties.end(),
                         [](const CssVocabulary::Property& left, const CssVocabulary::Property& right) {
                             return left.name < right.name;
                         });

        std::vector<NamedDefinition> atRules;
        for (const CssVocabulary::Module& module : vocabulary.modules)
        {
            for (const NamedDefinition& definition : module.atRules)
            {
                atRules.push_back(definition);
            }
        }

        std::stable_sort(atRules.begin(), atRules.end(),
                         [](const NamedDefinition& left, const NamedDefinition& right) {
                             return left.name < right.name;
                         });

        std::ostringstream header;
        header << "// ---------------------------------------------------------------------------\n"
                  "// RoboHTMLCompiler - generated CSS reference vocabulary.\n"
                  "//\n"
                  "// GENERATED FILE. Do not edit. Produced by the CssSpecExtractor tool from\n"
                  "// Reference/CSS, which is why the reference itself can be deleted once the\n"
                  "// compiler's accounting of it is green.\n"
                  "//\n"
                  "// Regenerate with:\n"
                  "//     cmake --build <build> --target RoboExtractCssReferences\n"
                  "//\n"
                  "// The names are sorted so a lookup is a binary search, and they are the whole\n"
                  "// of what the reference defines: a name here that the compiler does not\n"
                  "// implement is reported as an unimplemented feature (RCC010), never as an\n"
                  "// unknown property (RCC001).\n"
                  "// ---------------------------------------------------------------------------\n"
                  "#pragma once\n\n"
                  "#include <cstddef>\n\n"
                  "namespace RoboCSSCompiler\n"
                  "{\n"
                  "    namespace Generated\n"
                  "    {\n"
                  "        /// One property definition in the reference.\n"
                  "        struct ReferenceProperty\n"
                  "        {\n"
                  "            const char* name;\n"
                  "            /// The specification module that defines it, as a directory\n"
                  "            /// name under Reference/CSS.\n"
                  "            const char* module;\n"
                  "            bool inherited;\n"
                  "        };\n\n";

        header << "        inline constexpr ReferenceProperty ReferenceProperties[] = {\n";
        for (const CssVocabulary::Property& property : properties)
        {
            header << "            { \"" << property.name << "\", \"" << property.module << "\", "
                   << (property.inherited ? "true" : "false") << " },\n";
        }
        header << "        };\n\n";

        header << "        inline constexpr std::size_t ReferencePropertyCount =\n"
                  "            sizeof(ReferenceProperties) / sizeof(ReferenceProperties[0]);\n\n"
                  "        /// One at-rule the reference defines.\n"
                  "        struct ReferenceAtRule\n"
                  "        {\n"
                  "            const char* name;\n"
                  "            const char* module;\n"
                  "        };\n\n";

        std::vector<std::pair<std::string, std::string>> atRuleRows;
        for (const NamedDefinition& definition : atRules)
        {
            atRuleRows.push_back(std::make_pair(definition.name, definition.module));
        }

        // A retired module's at-rules stay named here for the same reason its
        // module name does: the reference had them, and the vocabulary is what
        // the compiler is left holding once the directory is gone.
        for (const std::pair<std::string, std::string>& retiredAtRule : retired.AllAtRules())
        {
            atRuleRows.push_back(retiredAtRule);
        }

        header << "        inline constexpr ReferenceAtRule ReferenceAtRules[] = {\n";
        for (const std::pair<std::string, std::string>& row : atRuleRows)
        {
            header << "            { \"" << row.first << "\", \"" << row.second << "\" },\n";
        }
        header << "        };\n\n";

        header << "        inline constexpr std::size_t ReferenceAtRuleCount =\n"
                  "            sizeof(ReferenceAtRules) / sizeof(ReferenceAtRules[0]);\n\n";

        header <<                  "        /// Every module the reference had or has, and whether it carries the\n"
                  "        /// Bikeshed source the vocabulary above was mined from. A module\n"
                  "        /// without source is one whose vocabulary is not in the compiler's\n                  "        /// knowledge yet, and the audit names it rather than leaving it out.\n"
                  "        /// `retired` marks a module whose directory has been removed after\n"
                  "        /// the compiler accounted for everything it defined; it stays here\n"                  "        /// so that a citation of it still names a module the reference had.\n"
                  "        struct ReferenceModule\n"
                  "        {\n"
                  "            const char* name;\n"
                  "            bool hasSource;\n"
                  "            bool retired;\n"
                  "        };\n\n";

        header << "        inline constexpr ReferenceModule ReferenceModules[] = {\n";
        for (const std::string& module : vocabulary.allModuleNames)
        {
            header << "            { \"" << module << "\", "
                   << (vocabulary.moduleNames.count(module) != 0 ? "true" : "false") << ", false },\n";
        }
        for (const std::string& module : retired.moduleNames)
        {
            header << "            { \"" << module << "\", false, true },\n";
        }
        header << "        };\n\n";

        header << "        inline constexpr std::size_t ReferenceModuleCount =\n"
                  "            sizeof(ReferenceModules) / sizeof(ReferenceModules[0]);\n\n";

        const std::vector<std::string> retiredProperties = retired.AllPropertyNames();

        header << "        /// The properties a retired module defined. The reference no longer\n"
                  "        /// carries them, so nothing else would notice if the compiler\n"
                  "        /// stopped implementing one: the coverage tests require every name\n"
                  "        /// here to stay implemented, which is what makes deletion safe.\n"
                  "        inline constexpr const char* RetiredReferencePropertyNames[] = {\n";
        for (const std::string& name : retiredProperties)
        {
            header << "            \"" << name << "\",\n";
        }
        header << "        };\n\n";

        header << "        inline constexpr std::size_t RetiredReferencePropertyCount =\n"
                  "            sizeof(RetiredReferencePropertyNames) / sizeof(RetiredReferencePropertyNames[0]);\n"
                  "    }\n"
                  "}\n";

        return header.str();
    }

    JsonValue BuildAudit(const CssVocabulary& vocabulary, const fs::path& referenceDirectory,
                         const std::set<std::string>& retired, const RetiredKnowledge& retiredKnowledge)
    {
        JsonValue root = JsonValue::MakeObject();
        root.Set("$schema", JsonValue::MakeString("RoboHTMLCompiler.CssReferenceAudit.v1"));
        root.Set("generatedBy", JsonValue::MakeString("RoboHTMLCompiler CssSpecExtractor"));
        root.Set("reference", JsonValue::MakeString(referenceDirectory.generic_string()));
        root.Set("policy",
                 JsonValue::MakeString(
                     "A module is retired once every property it defines is implemented by the CSS front end, not "
                     "merely reported by name, and the compiler's coverage tests account for it. Retiring a module "
                     "removes its directory, records it here, and carries its module name, its property names and "
                     "its at-rules into the generated header, where the tests still require every one of those "
                     "properties to be implemented. The reference is consumed when no module remains: the "
                     "vocabulary that is checked in is what the compiler is built against from then on."));

        JsonValue moduleArray = JsonValue::MakeArray();
        std::size_t remaining = 0;

        for (const CssVocabulary::Module& module : vocabulary.modules)
        {
            JsonValue entry = JsonValue::MakeObject();
            entry.Set("name", JsonValue::MakeString(module.name));
            entry.Set("properties", JsonValue::MakeInteger(static_cast<long long>(module.properties.size())));
            entry.Set("descriptors", JsonValue::MakeInteger(static_cast<long long>(module.descriptors.size())));
            entry.Set("atRules", JsonValue::MakeInteger(static_cast<long long>(module.atRules.size())));
            entry.Set("selectors", JsonValue::MakeInteger(static_cast<long long>(module.selectors.size())));
            entry.Set("functions", JsonValue::MakeInteger(static_cast<long long>(module.functions.size())));
            const bool isRetired = retired.count(module.name) != 0;
            entry.Set("retired", JsonValue::MakeBoolean(isRetired));
            if (!isRetired)
            {
                ++remaining;
            }
            moduleArray.Append(std::move(entry));
        }

        // A retired module has no directory left to describe, so the audit keeps
        // the entry that was last written for it rather than dropping it. An
        // audit that forgot what it consumed could not answer "was this
        // accounted for?" about the very modules that are gone.
        for (const JsonValue& entry : retiredKnowledge.moduleEntries)
        {
            moduleArray.Append(entry);
        }

        std::set<std::string> withoutSource;
        for (const std::string& name : vocabulary.allModuleNames)
        {
            if (vocabulary.moduleNames.count(name) == 0)
            {
                withoutSource.insert(name);
            }
        }

        // The source-less modules are still in the reference and still not in the
        // compiler's knowledge, so they count as remaining work: the reference
        // cannot be called consumed while they are unmined.
        remaining += withoutSource.size();

        root.Set("modules", std::move(moduleArray));
        root.Set("modulesWithoutSource", MakeStringArray(withoutSource));
        root.Set("nonSpecificationModules", MakeReasonArray(vocabulary.skippedModules));
        root.Set("excludedPropertyNames", MakeReasonArray(vocabulary.skippedProperties));
        root.Set("retiredModules", MakeStringArray(retired));
        root.Set("modulesRemaining", JsonValue::MakeInteger(static_cast<long long>(remaining)));

        JsonValue breakdown = JsonValue::MakeObject();
        breakdown.Set("modulesRemaining", JsonValue::MakeInteger(static_cast<long long>(remaining)));
        breakdown.Set("withSource", JsonValue::MakeInteger(static_cast<long long>(vocabulary.modules.size())));
        breakdown.Set("withoutSource", JsonValue::MakeInteger(static_cast<long long>(withoutSource.size())));
        breakdown.Set("retired", JsonValue::MakeInteger(static_cast<long long>(retired.size())));
        root.Set("remainingBreakdown", std::move(breakdown));
        root.Set("referenceState", JsonValue::MakeString(remaining == 0 ? "consumed" : "pending"));

        return root;
    }
}

int main(int argumentCount, char** argumentValues)
{
    fs::path referenceDirectory = "Reference/CSS";
    fs::path vocabularyDirectory = "CompilerKnowledge/Vocabulary";
    fs::path headerPath = "CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h";
    fs::path auditPath = "CompilerKnowledge/CssReferenceAudit.json";

    bool check = false;
    std::set<std::string> retire;

    std::vector<std::string> positional;

    for (int index = 1; index < argumentCount; ++index)
    {
        const std::string argument = argumentValues[index];

        const auto value = [&]() -> const char* {
            return index + 1 < argumentCount ? argumentValues[++index] : nullptr;
        };

        if (argument == "--check")
        {
            check = true;
            continue;
        }
        if (argument == "--header")
        {
            const char* path = value();
            if (path == nullptr)
            {
                return Fail("--header needs a path");
            }
            headerPath = path;
            continue;
        }
        if (argument == "--audit")
        {
            const char* path = value();
            if (path == nullptr)
            {
                return Fail("--audit needs a path");
            }
            auditPath = path;
            continue;
        }
        if (argument == "--retire")
        {
            const char* list = value();
            if (list == nullptr)
            {
                return Fail("--retire needs a comma separated module list");
            }

            std::istringstream stream(list);
            std::string item;
            while (std::getline(stream, item, ','))
            {
                const std::string trimmed = Trim(item);
                if (!trimmed.empty())
                {
                    retire.insert(trimmed);
                }
            }
            continue;
        }
        if (argument == "--help")
        {
            std::cout << "CssSpecExtractor [referenceDir] [vocabularyDir]\n"
                         "                [--header FILE] [--audit FILE] [--check] [--retire a,b]\n";
            return 0;
        }

        positional.push_back(argument);
    }

    if (positional.size() > 0)
    {
        referenceDirectory = positional[0];
    }
    if (positional.size() > 1)
    {
        vocabularyDirectory = positional[1];
    }

    std::error_code error;
    if (!fs::exists(referenceDirectory, error))
    {
        std::cout << "CssSpecExtractor: no CSS reference at " << referenceDirectory.generic_string()
                  << "; the extracted vocabulary stays authoritative\n";
        return 0;
    }

    // Retiring happens before extraction so the audit reflects the reference that
    // is left, and so a module cannot be counted twice.
    if (!retire.empty())
    {
        for (const std::string& module : retire)
        {
            const fs::path directory = referenceDirectory / module;
            if (!fs::exists(directory, error))
            {
                std::cout << "  retire: " << module << " is not present\n";
                continue;
            }

            fs::remove_all(directory, error);
            if (error)
            {
                return Fail("cannot remove " + directory.generic_string() + ": " + error.message());
            }

            std::cout << "  retired " << module << "\n";
        }
    }

    CssVocabulary vocabulary;
    if (!vocabulary.Extract(referenceDirectory))
    {
        std::cout << "CssSpecExtractor: nothing to extract at " << referenceDirectory.generic_string() << "\n";
    }

    std::set<std::string> previouslyRetired;
    // Both files are read before anything is written: the vocabulary is what
    // still knows what a module defined, and the audit is what knows what has
    // already been retired.
    const std::string previousAudit = ReadFile(auditPath);
    const std::string previousVocabulary = ReadFile(vocabularyDirectory / "CssVocabulary.json");
    {
        if (!previousAudit.empty())
        {
            std::string parseError;
            if (const auto parsed = JsonValue::Parse(previousAudit, &parseError))
            {
                for (const std::string& name : parsed->GetStringArray("retiredModules"))
                {
                    previouslyRetired.insert(name);
                }
            }
        }
    }

    for (const std::string& module : retire)
    {
        previouslyRetired.insert(module);
    }

    const RetiredKnowledge retiredKnowledge =
        ReadRetiredKnowledge(previousVocabulary, previousAudit, previouslyRetired);

    const std::string vocabularyJson = vocabulary.ToJson().Serialize(true);
    if (!WriteFile(vocabularyDirectory / "CssVocabulary.json", vocabularyJson))
    {
        return Fail("cannot write " + (vocabularyDirectory / "CssVocabulary.json").generic_string());
    }

    const std::string header = BuildHeader(vocabulary, retiredKnowledge);
    if (!WriteFile(headerPath, header))
    {
        return Fail("cannot write " + headerPath.generic_string());
    }

    // The audit is checked in, so it names the reference the way the repository
    // does and not the way this machine's filesystem happens to be laid out.
    const fs::path auditReference = fs::relative(referenceDirectory, fs::current_path(), error);

    const JsonValue audit = BuildAudit(vocabulary, auditReference.empty() ? referenceDirectory : auditReference,
                                       previouslyRetired, retiredKnowledge);
    if (!WriteFile(auditPath, audit.Serialize(true)))
    {
        return Fail("cannot write " + auditPath.generic_string());
    }

    std::size_t propertyCount = 0;
    std::size_t descriptorCount = 0;
    for (const CssVocabulary::Module& module : vocabulary.modules)
    {
        propertyCount += module.properties.size();
        descriptorCount += module.descriptors.size();
    }

    std::cout << "CssSpecExtractor\n"
              << "  modules with vocabulary: " << vocabulary.modules.size() << " of " << vocabulary.allModuleNames.size()
              << " directories, " << (vocabulary.allModuleNames.size() - vocabulary.moduleNames.size())
              << " without Bikeshed source\n"
              << "  properties: " << vocabulary.definedProperties.size() << " distinct, " << propertyCount
              << " definitions (" << vocabulary.skippedProperties.size() << " example names excluded)\n"
              << "  descriptors: " << descriptorCount << "\n"
              << "  not specifications: " << vocabulary.skippedModules.size() << " directories\n"
              << "  retired modules: " << previouslyRetired.size() << " ("
              << retiredKnowledge.AllPropertyNames().size() << " retired property names kept)\n"
              << "  vocabulary: " << (vocabularyDirectory / "CssVocabulary.json").generic_string() << "\n"
              << "  header:     " << headerPath.generic_string() << "\n"
              << "  audit:      " << auditPath.generic_string() << "\n";

    const JsonValue* remaining = audit.Find("modulesRemaining");
    if (remaining != nullptr && check && remaining->AsInteger() != 0)
    {
        std::cerr << "CssSpecExtractor: " << remaining->AsInteger()
                  << " reference modules are neither retired nor accounted for\n";
        return 1;
    }

    return 0;
}
