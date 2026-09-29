// ---------------------------------------------------------------------------
// RoboHTMLCompiler - RegistryCompiler
//
// Converts the machine-readable registries into strongly typed C++ tables that
// are compiled straight into the compiler. The compiler therefore never parses
// JSON, HTML or CSS at run time:
//
//     Registry/*.json                 (hand authored vocabulary)
//     CompilerKnowledge/Vocabulary/*.json       (mined from Reference/HTML)
//                    |
//                    v
//             RegistryCompiler
//                    |
//                    +--> CompilerKnowledge/Include/RoboHTMLCompiler/GeneratedEnums.h
//                    +--> CompilerKnowledge/Include/RoboHTMLCompiler/GeneratedRegistries.h
//                    +--> CompilerKnowledge/Source/GeneratedRegistries.cpp
//
// Cross references are validated: an unknown native kind, an unknown native
// property or a route for an unregistered attribute is a hard error, so the
// registries can never silently drift apart.
// ---------------------------------------------------------------------------

#include "Json.h"

#include <algorithm>
#include <cctype>
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

    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "[RegistryCompiler] error: " << message << "\n";
        std::exit(1);
    }

    std::string ReadWholeFile(const fs::path& path)
    {
        if (!fs::exists(path))
        {
            Fail("required registry not found: " + path.string());
        }

        std::ifstream stream(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    JsonValue ReadJson(const fs::path& path)
    {
        std::string error;
        const std::optional<JsonValue> parsed = JsonValue::Parse(ReadWholeFile(path), &error);
        if (!parsed.has_value())
        {
            Fail("failed to parse " + path.string() + ": " + error);
        }
        return *parsed;
    }

    void WriteFile(const fs::path& path, const std::string& contents)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << contents;
        std::cout << "[RegistryCompiler] wrote " << path.string() << "\n";
    }

    std::string EscapeCppString(const std::string& value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 8);

        for (const char character : value)
        {
            switch (character)
            {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20 || static_cast<unsigned char>(character) == 0x7F)
                {
                    escaped += ' ';
                }
                else
                {
                    escaped.push_back(character);
                }
                break;
            }
        }

        return escaped;
    }

    std::string Quote(const std::string& value)
    {
        return "\"" + EscapeCppString(value) + "\"";
    }

    bool IsValidIdentifier(const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }
        if (std::isalpha(static_cast<unsigned char>(value[0])) == 0 && value[0] != '_')
        {
            return false;
        }
        for (const char character : value)
        {
            if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_')
            {
                return false;
            }
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Registry model
    // -----------------------------------------------------------------------

    enum class Namespace
    {
        Html,
        Svg,
        MathMl
    };

    const char* NamespaceName(Namespace value)
    {
        switch (value)
        {
        case Namespace::Html: return "Html";
        case Namespace::Svg: return "Svg";
        case Namespace::MathMl: return "MathMl";
        }
        return "Html";
    }

    struct ElementSpec
    {
        std::string name;
        Namespace ns = Namespace::Html;
        std::string kind;
        int headingLevel = 0;
        bool voidElement = false;
        bool obsolete = false;
        bool globalAttributes = false;
        bool ordered = false;
        bool rawText = false;
        bool phrasing = false;
        bool formAssociated = false;
        bool interactive = false;
        std::vector<std::string> scopedAttributes;
        std::string sectionId;
        std::string domInterface;
        std::string contentModel;
    };

    struct AttributeSpec
    {
        std::string name;
        Namespace ns = Namespace::Html;
        std::string property;
        std::string valueType;
        std::string category;
        bool obsolete = false;
        bool forbidden = false;
        bool runtimeMutable = false;
        bool notCompileTime = false;
        bool namespaceDefault = false;
        std::vector<std::string> allowedElements;
    };

    struct InputTypeSpec
    {
        std::string typeValue;
        std::string kind;
        bool obsolete = false;
    };

    // -----------------------------------------------------------------------
    // Compiler
    // -----------------------------------------------------------------------

    class RegistryCompiler
    {
    public:
        bool Run(const fs::path& handRegistryDirectory,
                 const fs::path& extractedRegistryDirectory,
                 const fs::path& includeOutputDirectory,
                 const fs::path& sourceOutputDirectory,
                 const fs::path& coveragePath)
        {
            LoadKindRegistry(handRegistryDirectory / "ElementKindRegistry.json");
            LoadPropertyRegistry(handRegistryDirectory / "NativePropertyRegistry.json");
            LoadSemantics(handRegistryDirectory / "AttributeSemantics.json");

            LoadDeprecatedElements(extractedRegistryDirectory / "DeprecatedElements.json",
                                   handRegistryDirectory / "ObsoleteVocabulary.json");
            LoadHtmlElements(extractedRegistryDirectory / "HtmlElements.json",
                             handRegistryDirectory / "ElementNormalization.json");
            LoadGlobalAttributes(extractedRegistryDirectory / "GlobalAttributes.json",
                                 handRegistryDirectory / "GlobalAttributeExtensions.json");
            LoadEventHandlerAttributes(extractedRegistryDirectory / "EventHandlerAttributes.json");
            LoadHtmlAttributes(extractedRegistryDirectory / "HtmlAttributes.json",
                               handRegistryDirectory / "AttributeRouting.json");
            RegisterObsoleteAttributes();

            LoadSvgElements(handRegistryDirectory / "SvgElementRegistry.json");
            LoadPlainAttributes(handRegistryDirectory / "SvgAttributeRegistry.json", Namespace::Svg);
            LoadMathMlElements(handRegistryDirectory / "MathMlElementRegistry.json");
            LoadPlainAttributes(handRegistryDirectory / "MathMlAttributeRegistry.json", Namespace::MathMl);
            LoadAriaAttributes(handRegistryDirectory / "AriaAttributeRegistry.json");
            LoadInputTypes(handRegistryDirectory / "InputTypeRegistry.json");

            Validate();

            EmitEnums(includeOutputDirectory / "GeneratedEnums.h");
            EmitRegistryHeader(includeOutputDirectory / "GeneratedRegistries.h");
            EmitRegistrySource(sourceOutputDirectory / "GeneratedRegistries.cpp");
            UpdateCoverage(coveragePath);

            std::cout << "[RegistryCompiler] kinds:               " << kindNames.size() << "\n";
            std::cout << "[RegistryCompiler] native properties:   " << propertyNames.size() << "\n";
            std::cout << "[RegistryCompiler] elements:            " << elements.size() << "\n";
            std::cout << "[RegistryCompiler] attribute records:   " << attributes.size() << "\n";
            std::cout << "[RegistryCompiler] input types:         " << inputTypes.size() << "\n";

            return true;
        }

    private:
        std::vector<std::string> kindNames;
        std::vector<std::string> propertyNames;
        std::vector<ElementSpec> elements;
        std::vector<AttributeSpec> attributes;
        std::vector<InputTypeSpec> inputTypes;
        std::set<std::string> runtimeMutableAttributes;
        std::set<std::string> notCompileTimeAttributes;
        std::set<std::string> registeredHtmlAttributes;
        std::set<std::string> routedAttributeNames;
        std::map<std::string, std::string> elementKinds;  // "Html:button" -> "Button"
        std::set<std::string> deprecatedElements;
        std::set<std::string> deprecatedAttributes;

        static std::string NamespaceKey(Namespace ns, const std::string& name)
        {
            return std::string(NamespaceName(ns)) + ":" + name;
        }

        void RequireKind(const std::string& kind, const std::string& context) const
        {
            if (std::find(kindNames.begin(), kindNames.end(), kind) == kindNames.end())
            {
                Fail("unknown native element kind '" + kind + "' referenced by " + context);
            }
        }

        void RequireProperty(const std::string& property, const std::string& context) const
        {
            if (std::find(propertyNames.begin(), propertyNames.end(), property) == propertyNames.end())
            {
                Fail("unknown native property '" + property + "' referenced by " + context);
            }
        }

        // -- loaders --------------------------------------------------------

        void LoadKindRegistry(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* kinds = root.Find("kinds");
            if (kinds == nullptr || !kinds->IsArray())
            {
                Fail("ElementKindRegistry.json has no 'kinds' array");
            }

            for (const JsonValue& kind : kinds->GetElements())
            {
                const std::string name = kind.AsString();
                if (!IsValidIdentifier(name))
                {
                    Fail("invalid element kind identifier '" + name + "'");
                }
                kindNames.push_back(name);
            }

            if (kindNames.empty() || kindNames.front() != "None")
            {
                Fail("ElementKindRegistry.json must start with the None kind");
            }
        }

        void LoadPropertyRegistry(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* properties = root.Find("properties");
            if (properties == nullptr || !properties->IsArray())
            {
                Fail("NativePropertyRegistry.json has no 'properties' array");
            }

            for (const JsonValue& property : properties->GetElements())
            {
                const std::string name = property.AsString();
                if (!IsValidIdentifier(name))
                {
                    Fail("invalid native property identifier '" + name + "'");
                }
                propertyNames.push_back(name);
            }
        }

        void LoadSemantics(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            for (const std::string& name : root.GetStringArray("runtimeMutable"))
            {
                runtimeMutableAttributes.insert(name);
            }
            for (const std::string& name : root.GetStringArray("notCompileTime"))
            {
                notCompileTimeAttributes.insert(name);
            }
        }

        void LoadHtmlElements(const fs::path& specPath, const fs::path& normalizationPath)
        {
            const JsonValue normalization = ReadJson(normalizationPath);
            const JsonValue* normalizedElements = normalization.Find("elements");
            if (normalizedElements == nullptr || !normalizedElements->IsArray())
            {
                Fail("ElementNormalization.json has no 'elements' array");
            }

            struct NormalizationEntry
            {
                std::string kind;
                int headingLevel = 0;
                bool voidElement = false;
                bool ordered = false;
                bool rawText = false;
                bool phrasing = false;
                bool formAssociated = false;
                bool interactive = false;
            };

            std::map<std::string, NormalizationEntry> normalizationMap;
            for (const JsonValue& entry : normalizedElements->GetElements())
            {
                const std::string source = entry.GetString("source");
                if (source.empty())
                {
                    Fail("ElementNormalization.json contains an entry without a 'source' name");
                }

                NormalizationEntry value;
                value.kind = entry.GetString("kind");
                value.headingLevel = static_cast<int>(entry.GetInteger("headingLevel"));
                value.voidElement = entry.GetBoolean("void");
                value.ordered = entry.GetBoolean("ordered");
                value.rawText = entry.GetBoolean("rawText");
                value.phrasing = entry.GetBoolean("phrasing");
                value.formAssociated = entry.GetBoolean("formAssociated");
                value.interactive = entry.GetBoolean("interactive");

                RequireKind(value.kind, "ElementNormalization.json entry '" + source + "'");
                if (entry.GetBoolean("obsolete"))
                {
                    deprecatedElements.insert(source);
                }

                normalizationMap[source] = value;
            }

            const JsonValue spec = ReadJson(specPath);
            const JsonValue* specElements = spec.Find("elements");
            if (specElements == nullptr || !specElements->IsArray())
            {
                Fail("HtmlElements.json has no 'elements' array");
            }

            for (const JsonValue& entry : specElements->GetElements())
            {
                const std::string name = entry.GetString("name");
                const auto normalizationIterator = normalizationMap.find(name);
                if (normalizationIterator == normalizationMap.end())
                {
                    Fail("reference element '" + name +
                         "' has no native normalization; complete Registry/ElementNormalization.json");
                }

                ElementSpec spec2;
                spec2.name = name;
                spec2.ns = Namespace::Html;
                spec2.kind = normalizationIterator->second.kind;
                spec2.headingLevel = normalizationIterator->second.headingLevel;
                spec2.voidElement = normalizationIterator->second.voidElement;
                spec2.ordered = normalizationIterator->second.ordered;
                spec2.rawText = normalizationIterator->second.rawText;
                spec2.phrasing = normalizationIterator->second.phrasing;
                spec2.formAssociated = normalizationIterator->second.formAssociated;
                spec2.interactive = normalizationIterator->second.interactive;
                spec2.obsolete = entry.GetBoolean("obsolete") || deprecatedElements.count(name) > 0;
                spec2.globalAttributes = entry.GetBoolean("globalAttributes");
                spec2.sectionId = entry.GetString("sectionId");
                spec2.domInterface = entry.GetString("domInterface");
                spec2.contentModel = entry.GetString("contentModel");

                for (const std::string& attribute : entry.GetStringArray("attributes"))
                {
                    spec2.scopedAttributes.push_back(attribute);
                }

                elementKinds[NamespaceKey(Namespace::Html, name)] = spec2.kind;
                elements.push_back(std::move(spec2));
            }
        }

        void LoadHtmlAttributes(const fs::path& specPath, const fs::path& routingPath)
        {
            const JsonValue routing = ReadJson(routingPath);
            const JsonValue* routes = routing.Find("routes");
            if (routes == nullptr || !routes->IsArray())
            {
                Fail("AttributeRouting.json has no 'routes' array");
            }

            struct Route
            {
                std::string property;
                bool forbidden = false;
                std::vector<std::string> elements;
            };

            std::map<std::string, std::vector<Route>> elementScopedRoutes;
            std::map<std::string, Route> defaultRoutes;

            for (const JsonValue& route : routes->GetElements())
            {
                const std::string attribute = route.GetString("attribute");
                if (attribute.empty())
                {
                    Fail("AttributeRouting.json contains a route without an 'attribute'");
                }

                Route value;
                value.property = route.GetString("property");
                value.forbidden = route.GetBoolean("forbidden");
                for (const std::string& element : route.GetStringArray("elements"))
                {
                    value.elements.push_back(element);
                }

                RequireProperty(value.property, "AttributeRouting.json route for '" + attribute + "'");

                if (value.elements.empty())
                {
                    defaultRoutes[attribute] = value;
                }
                else
                {
                    elementScopedRoutes[attribute].push_back(value);
                }
            }

            const JsonValue spec = ReadJson(specPath);
            const JsonValue* specAttributes = spec.Find("attributes");
            if (specAttributes == nullptr || !specAttributes->IsArray())
            {
                Fail("HtmlAttributes.json has no 'attributes' array");
            }

            for (const JsonValue& entry : specAttributes->GetElements())
            {
                const std::string name = entry.GetString("name");
                const std::vector<std::string> allowedElements = entry.GetStringArray("allowedElements");
                // Obsolete-ness is per element: 'obsolete' is only set when the
                // reference marks the attribute obsolete for every element that
                // declares it. An attribute that is merely obsolete on a
                // different element (for example 'name' on <embed> but not on
                // <input>) stays a current attribute.
                const bool obsolete = entry.GetBoolean("obsolete");

                const auto scoped = elementScopedRoutes.find(name);
                const auto fallback = defaultRoutes.find(name);

                if (scoped != elementScopedRoutes.end())
                {
                    for (const Route& route : scoped->second)
                    {
                        AttributeSpec attribute;
                        attribute.name = name;
                        attribute.ns = Namespace::Html;
                        attribute.property = route.property;
                        attribute.forbidden = route.forbidden;
                        attribute.obsolete = obsolete;
                        attribute.allowedElements = route.elements;
                        attribute.valueType = "String";
                        attribute.category = "Element";
                        ApplySemantics(attribute);
                        attributes.push_back(std::move(attribute));
                    }
                    registeredHtmlAttributes.insert(name);
                }

                if (fallback != defaultRoutes.end())
                {
                    AttributeSpec attribute;
                    attribute.name = name;
                    attribute.ns = Namespace::Html;
                    attribute.property = fallback->second.property;
                    attribute.forbidden = fallback->second.forbidden;
                    attribute.obsolete = obsolete;
                    attribute.allowedElements = allowedElements;
                    attribute.valueType = "String";
                    attribute.category = "Element";
                    ApplySemantics(attribute);
                    attributes.push_back(std::move(attribute));
                    registeredHtmlAttributes.insert(name);
                    continue;
                }

                // Element-scoped routes exist but no default: the attribute is
                // only valid on the listed elements.
                if (scoped != elementScopedRoutes.end())
                {
                    registeredHtmlAttributes.insert(name);
                    continue;
                }

                Fail("attribute '" + name +
                     "' is registered but has no route; complete Registry/AttributeRouting.json");
            }

            for (const auto& entry : defaultRoutes)
            {
                routedAttributeNames.insert(entry.first);
            }
            for (const auto& entry : elementScopedRoutes)
            {
                routedAttributeNames.insert(entry.first);
            }

            for (const auto& entry : elementScopedRoutes)
            {
                for (const Route& route : entry.second)
                {
                    for (const std::string& element : route.elements)
                    {
                        if (elementKinds.find(NamespaceKey(Namespace::Html, element)) == elementKinds.end())
                        {
                            Fail("AttributeRouting.json scopes attribute '" + entry.first + "' to unknown element '" +
                                 element + "'");
                        }
                    }
                }
            }
        }

        /// Attributes that the reference defines only inside the obsolete
        /// section are still part of the compiler's knowledge: recognising them
        /// lets validation report a precise "obsolete attribute" diagnostic
        /// instead of pretending the attribute is unknown.
        void RegisterObsoleteAttributes()
        {
            for (const std::string& name : deprecatedAttributes)
            {
                if (registeredHtmlAttributes.count(name) > 0)
                {
                    continue;
                }

                AttributeSpec attribute;
                attribute.name = name;
                attribute.ns = Namespace::Html;
                attribute.property = "Unmapped";
                attribute.valueType = "String";
                attribute.category = "Obsolete";
                attribute.obsolete = true;
                registeredHtmlAttributes.insert(name);
                attributes.push_back(std::move(attribute));
            }
        }

        void ApplySemantics(AttributeSpec& attribute) const
        {
            if (runtimeMutableAttributes.count(attribute.name) > 0)
            {
                attribute.runtimeMutable = true;
            }
            if (notCompileTimeAttributes.count(attribute.name) > 0)
            {
                attribute.notCompileTime = true;
            }
        }

        void CollectDeprecatedAttributes(const fs::path& path)
        {
            if (!fs::exists(path))
            {
                return;
            }

            const JsonValue root = ReadJson(path);
            for (const std::string& name : root.GetStringArray("attributes"))
            {
                deprecatedAttributes.insert(name);
            }
        }

        void LoadGlobalAttributes(const fs::path& specPath, const fs::path& extensionsPath)
        {
            std::set<std::string> names;

            const JsonValue spec = ReadJson(specPath);
            const JsonValue* specAttributes = spec.Find("attributes");
            if (specAttributes == nullptr || !specAttributes->IsArray())
            {
                Fail("GlobalAttributes.json has no 'attributes' array");
            }
            for (const JsonValue& entry : specAttributes->GetElements())
            {
                names.insert(entry.GetString("name"));
            }

            const JsonValue extensions = ReadJson(extensionsPath);
            const JsonValue* extensionAttributes = extensions.Find("attributes");
            if (extensionAttributes != nullptr)
            {
                for (const JsonValue& entry : extensionAttributes->GetElements())
                {
                    names.insert(entry.GetString("name"));
                }
            }

            static const std::map<std::string, std::string> globalProperties = {
                { "accesskey", "MetadataAccessKey" },
                { "autocapitalize", "MetadataAutoCapitalize" },
                { "autocorrect", "MetadataAutoCorrect" },
                { "autofocus", "StateAutofocus" },
                { "class", "StyleReference" },
                { "contenteditable", "MetadataEditable" },
                { "dir", "TextDirection" },
                { "draggable", "StateDraggable" },
                { "enterkeyhint", "MetadataEnterKeyHint" },
                { "headingoffset", "HeadingOffset" },
                { "headingreset", "HeadingReset" },
                { "hidden", "StateHidden" },
                { "id", "IdentityId" },
                { "inert", "StateInert" },
                { "inputmode", "MetadataInputMode" },
                { "is", "CustomizedBuiltIn" },
                { "itemid", "MicroDataItemId" },
                { "itemprop", "MicroDataItemProp" },
                { "itemref", "MicroDataItemRef" },
                { "itemscope", "MicroDataItemScope" },
                { "itemtype", "MicroDataItemType" },
                { "lang", "TextLanguage" },
                { "nonce", "MetadataNonce" },
                { "popover", "MetadataPopover" },
                { "slot", "SlotAssignment" },
                { "spellcheck", "StateSpellCheck" },
                { "style", "ForbiddenInlineStyle" },
                { "tabindex", "InteractionTabIndex" },
                { "title", "MetadataTitle" },
                { "translate", "TextTranslate" },
                { "writingsuggestions", "MetadataWriteSuggestions" }
            };

            for (const std::string& name : names)
            {
                AttributeSpec attribute;
                attribute.name = name;
                attribute.ns = Namespace::Html;
                attribute.namespaceDefault = true;
                attribute.valueType = "String";
                attribute.category = "Global";
                registeredHtmlAttributes.insert(name);

                const auto property = globalProperties.find(name);
                if (property == globalProperties.end())
                {
                    Fail("global attribute '" + name + "' has no native property mapping");
                }

                attribute.property = property->second;
                attribute.forbidden = (name == "style");
                ApplySemantics(attribute);
                RequireProperty(attribute.property, "global attribute '" + name + "'");
                attributes.push_back(std::move(attribute));
            }
        }

        void LoadEventHandlerAttributes(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* specAttributes = root.Find("attributes");
            if (specAttributes == nullptr)
            {
                return;
            }

            for (const JsonValue& entry : specAttributes->GetElements())
            {
                AttributeSpec attribute;
                attribute.name = entry.GetString("name");
                attribute.ns = Namespace::Html;
                attribute.property = "Unmapped";
                attribute.namespaceDefault = true;
                attribute.valueType = "JavaScript";
                attribute.category = "Event handler";
                attribute.forbidden = true;
                registeredHtmlAttributes.insert(attribute.name);
                attributes.push_back(std::move(attribute));
            }
        }

        /// The mined obsolete vocabulary plus the hand-authored legacy list.
        /// An entry is never implemented; it exists so that the compiler can
        /// reject it precisely instead of reporting an unknown feature.
        void LoadDeprecatedElements(const fs::path& path, const fs::path& legacyPath)
        {
            const JsonValue root = ReadJson(path);
            for (const std::string& name : root.GetStringArray("elements"))
            {
                deprecatedElements.insert(name);
            }
            for (const std::string& name : root.GetStringArray("attributes"))
            {
                deprecatedAttributes.insert(name);
            }

            if (!fs::exists(legacyPath))
            {
                return;
            }

            const JsonValue legacy = ReadJson(legacyPath);
            for (const std::string& name : legacy.GetStringArray("elements"))
            {
                deprecatedElements.insert(name);
            }
            for (const JsonValue& entry : GetArray(legacy, "attributes"))
            {
                const std::string name = entry.IsString() ? entry.AsString() : entry.GetString("name");
                if (!name.empty())
                {
                    deprecatedAttributes.insert(name);
                }
            }
        }

        static std::vector<JsonValue> GetArray(const JsonValue& value, const std::string& key)
        {
            const JsonValue* array = value.Find(key);
            if (array == nullptr || !array->IsArray())
            {
                return {};
            }
            return array->GetElements();
        }

        void LoadSvgElements(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* specElements = root.Find("elements");
            if (specElements == nullptr)
            {
                Fail("SvgElementRegistry.json has no 'elements' array");
            }

            for (const JsonValue& entry : specElements->GetElements())
            {
                ElementSpec element;
                element.name = entry.GetString("name");
                element.ns = Namespace::Svg;
                element.kind = entry.GetString("kind");
                // SVG elements do contain children (shapes, groups, gradients),
                // so they are never void.
                element.voidElement = false;
                RequireKind(element.kind, "SvgElementRegistry.json entry '" + element.name + "'");
                elementKinds[NamespaceKey(Namespace::Svg, element.name)] = element.kind;
                elements.push_back(std::move(element));
            }
        }

        void LoadMathMlElements(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* specElements = root.Find("elements");
            if (specElements == nullptr)
            {
                Fail("MathMlElementRegistry.json has no 'elements' array");
            }

            for (const JsonValue& entry : specElements->GetElements())
            {
                ElementSpec element;
                element.name = entry.GetString("name");
                element.ns = Namespace::MathMl;
                element.kind = entry.GetString("kind");
                RequireKind(element.kind, "MathMlElementRegistry.json entry '" + element.name + "'");
                elementKinds[NamespaceKey(Namespace::MathMl, element.name)] = element.kind;
                elements.push_back(std::move(element));
            }
        }

        void LoadPlainAttributes(const fs::path& path, Namespace ns)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* specAttributes = root.Find("attributes");
            if (specAttributes == nullptr)
            {
                Fail(path.filename().string() + " has no 'attributes' array");
            }

            for (const JsonValue& entry : specAttributes->GetElements())
            {
                AttributeSpec attribute;
                attribute.name = entry.GetString("name");
                attribute.ns = ns;
                attribute.property = entry.GetString("property");
                attribute.valueType = entry.GetString("valueType", "String");
                attribute.category = entry.GetString("category");
                attribute.forbidden = entry.GetBoolean("forbidden");
                attribute.obsolete = entry.GetBoolean("obsolete");
                attribute.namespaceDefault = true;
                RequireProperty(attribute.property,
                                path.filename().string() + " entry '" + attribute.name + "'");

                const std::vector<std::string> allowedElements = entry.GetStringArray("elements");
                if (!allowedElements.empty())
                {
                    attribute.namespaceDefault = false;
                    attribute.allowedElements = allowedElements;
                    for (const std::string& element : allowedElements)
                    {
                        if (elementKinds.find(NamespaceKey(ns, element)) == elementKinds.end())
                        {
                            Fail(path.filename().string() + " scopes attribute '" + attribute.name +
                                 "' to unknown element '" + element + "'");
                        }
                    }
                }

                attribute.namespaceDefault = allowedElements.empty();
                attributes.push_back(std::move(attribute));
            }
        }

        void LoadAriaAttributes(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);

            if (const JsonValue* role = root.Find("role"); role != nullptr && role->IsObject())
            {
                AttributeSpec attribute;
                attribute.name = role->GetString("attribute");
                attribute.ns = Namespace::Html;
                attribute.property = role->GetString("property");
                attribute.valueType = role->GetString("valueType");
                attribute.category = "ARIA";
                attribute.namespaceDefault = true;
                RequireProperty(attribute.property, "AriaAttributeRegistry.json role entry");
                attributes.push_back(std::move(attribute));
            }

            const JsonValue* specAttributes = root.Find("attributes");
            if (specAttributes == nullptr)
            {
                Fail("AriaAttributeRegistry.json has no 'attributes' array");
            }

            for (const JsonValue& entry : specAttributes->GetElements())
            {
                AttributeSpec attribute;
                attribute.name = entry.GetString("name");
                attribute.ns = Namespace::Html;
                attribute.property = entry.GetString("property");
                attribute.valueType = entry.GetString("valueType", "String");
                attribute.category = "ARIA";
                attribute.namespaceDefault = true;
                attribute.obsolete = entry.GetBoolean("deprecated");
                RequireProperty(attribute.property, "AriaAttributeRegistry.json entry '" + attribute.name + "'");
                ApplySemantics(attribute);
                attributes.push_back(std::move(attribute));
            }
        }

        void LoadInputTypes(const fs::path& path)
        {
            const JsonValue root = ReadJson(path);
            const JsonValue* specInputTypes = root.Find("inputTypes");
            if (specInputTypes == nullptr)
            {
                Fail("InputTypeRegistry.json has no 'inputTypes' array");
            }

            for (const JsonValue& entry : specInputTypes->GetElements())
            {
                InputTypeSpec inputType;
                inputType.typeValue = entry.GetString("type");
                inputType.kind = entry.GetString("kind");
                inputType.obsolete = entry.GetBoolean("obsolete");
                RequireKind(inputType.kind, "InputTypeRegistry.json entry '" + inputType.typeValue + "'");
                inputTypes.push_back(std::move(inputType));
            }
        }

        // -- validation -----------------------------------------------------

        void Validate() const
        {
            // Every HTML element that accepts global attributes must have a kind.
            for (const ElementSpec& element : elements)
            {
                if (element.kind.empty())
                {
                    Fail("element '" + element.name + "' has no native kind");
                }
            }

            // Attribute routing may not describe attributes that the reference
            // vocabulary does not contain.
            for (const std::string& name : routedAttributeNames)
            {
                if (registeredHtmlAttributes.count(name) == 0)
                {
                    Fail("AttributeRouting.json routes attribute '" + name +
                         "', which is not part of the registered HTML vocabulary");
                }
            }

            // Namespace-wide attribute records may not exist twice for the same
            // (attribute, namespace) pair.
            std::set<std::string> seen;
            for (const AttributeSpec& attribute : attributes)
            {
                if (!attribute.namespaceDefault)
                {
                    continue;
                }
                const std::string key = NamespaceKey(attribute.ns, attribute.name);
                if (!seen.insert(key).second)
                {
                    Fail("duplicate namespace-wide attribute record for '" + attribute.name + "' in namespace " +
                         NamespaceName(attribute.ns));
                }
            }
        }

        // -- emission -------------------------------------------------------

        void EmitEnums(const fs::path& path) const
        {
            std::ostringstream output;
            output << "// ---------------------------------------------------------------------------\n";
            output << "// GENERATED FILE - do not edit by hand.\n";
            output << "// Produced by the RoboHTMLCompiler RegistryCompiler target from\n";
            output << "// Registry/ElementKindRegistry.json and Registry/NativePropertyRegistry.json.\n";
            output << "// ---------------------------------------------------------------------------\n";
            output << "#pragma once\n\n";
            output << "#include <cstdint>\n\n";
            output << "namespace RoboHTMLCompiler\n{\n";

            output << "    /// Compact native category that source elements normalise into. Many\n";
            output << "    /// source tags intentionally share one category.\n";
            output << "    enum class ElementKind : std::uint16_t\n    {\n";
            for (std::size_t index = 0; index < kindNames.size(); ++index)
            {
                output << "        " << kindNames[index] << " = " << index;
                if (index + 1 != kindNames.size())
                {
                    output << ",";
                }
                output << "\n";
            }
            output << "    };\n\n";

            output << "    /// Typed destination a source attribute routes into. This replaces\n";
            output << "    /// untyped attribute dictionaries on the hot path.\n";
            output << "    enum class NativeProperty : std::uint16_t\n    {\n";
            for (std::size_t index = 0; index < propertyNames.size(); ++index)
            {
                output << "        " << propertyNames[index] << " = " << index;
                if (index + 1 != propertyNames.size())
                {
                    output << ",";
                }
                output << "\n";
            }
            output << "    };\n\n";

            output << "    /// Stable diagnostic name for a native element kind.\n";
            output << "    const char* ToString(ElementKind kind);\n\n";
            output << "    /// Stable diagnostic name for a native property.\n";
            output << "    const char* ToString(NativeProperty property);\n";
            output << "}\n";

            WriteFile(path, output.str());
        }

        void EmitRegistryHeader(const fs::path& path) const
        {
            std::ostringstream output;
            output << "// ---------------------------------------------------------------------------\n";
            output << "// GENERATED FILE - do not edit by hand.\n";
            output << "// Produced by the RoboHTMLCompiler RegistryCompiler target.\n";
            output << "// ---------------------------------------------------------------------------\n";
            output << "#pragma once\n\n";
            output << "#include \"RoboHTMLCompiler/GeneratedEnums.h\"\n\n";
            output << "#include <cstdint>\n\n";
            output << "namespace RoboHTMLCompiler::Generated\n{\n";

            output << "    enum class NamespaceId : std::uint8_t\n    {\n";
            output << "        None = 0,\n        Html,\n        Svg,\n        MathMl\n    };\n\n";

            output << "    enum ElementFlags : std::uint32_t\n    {\n";
            output << "        ElementFlagNone = 0,\n";
            output << "        ElementFlagVoid = 1u << 0,\n";
            output << "        ElementFlagObsolete = 1u << 1,\n";
            output << "        ElementFlagGlobalAttributes = 1u << 2,\n";
            output << "        ElementFlagOrdered = 1u << 3,\n";
            output << "        ElementFlagRawText = 1u << 4,\n";
            output << "        ElementFlagPhrasing = 1u << 5,\n";
            output << "        ElementFlagFormAssociated = 1u << 6,\n";
            output << "        ElementFlagInteractive = 1u << 7\n    };\n\n";

            output << "    enum AttributeFlags : std::uint32_t\n    {\n";
            output << "        AttributeFlagNone = 0,\n";
            output << "        AttributeFlagNamespaceDefault = 1u << 0,\n";
            output << "        AttributeFlagGlobal = 1u << 1,\n";
            output << "        AttributeFlagEventHandler = 1u << 2,\n";
            output << "        AttributeFlagForbidden = 1u << 3,\n";
            output << "        AttributeFlagObsolete = 1u << 4,\n";
            output << "        AttributeFlagCompileTime = 1u << 5,\n";
            output << "        AttributeFlagRuntimeMutable = 1u << 6,\n";
            output << "        AttributeFlagBrowserPreserved = 1u << 7,\n";
            output << "        AttributeFlagAria = 1u << 8,\n";
            output << "        AttributeFlagSvg = 1u << 9,\n";
            output << "        AttributeFlagMathMl = 1u << 10\n    };\n\n";

            output << "    struct ElementRecord\n    {\n";
            output << "        const char* name;\n";
            output << "        NamespaceId namespaceId;\n";
            output << "        ElementKind kind;\n";
            output << "        std::uint8_t headingLevel;\n";
            output << "        std::uint32_t flags;\n";
            output << "        const char* const* attributeNames;\n";
            output << "        std::uint32_t attributeNameCount;\n";
            output << "        const char* sectionId;\n";
            output << "        const char* domInterface;\n";
            output << "    };\n\n";

            output << "    struct AttributeRecord\n    {\n";
            output << "        const char* name;\n";
            output << "        NamespaceId namespaceId;\n";
            output << "        NativeProperty property;\n";
            output << "        std::uint32_t flags;\n";
            output << "        const char* valueType;\n";
            output << "        const char* category;\n";
            output << "        const char* const* allowedElements;\n";
            output << "        std::uint32_t allowedElementCount;\n";
            output << "    };\n\n";

            output << "    struct InputTypeRecord\n    {\n";
            output << "        const char* typeValue;\n";
            output << "        ElementKind kind;\n";
            output << "        std::uint32_t flags;\n";
            output << "    };\n\n";

            output << "    extern const ElementRecord ElementTable[];\n";
            output << "    extern const std::uint32_t ElementCount;\n";
            output << "    extern const AttributeRecord AttributeTable[];\n";
            output << "    extern const std::uint32_t AttributeCount;\n";
            output << "    extern const InputTypeRecord InputTypeTable[];\n";
            output << "    extern const std::uint32_t InputTypeCount;\n";
            output << "}\n";

            WriteFile(path, output.str());
        }

        void EmitRegistrySource(const fs::path& path) const
        {
            std::ostringstream output;
            output << "// ---------------------------------------------------------------------------\n";
            output << "// GENERATED FILE - do not edit by hand.\n";
            output << "// Produced by the RoboHTMLCompiler RegistryCompiler target.\n";
            output << "// ---------------------------------------------------------------------------\n";
            output << "#include \"RoboHTMLCompiler/GeneratedRegistries.h\"\n\n";
            output << "#include <cstddef>\n\n";
            output << "namespace RoboHTMLCompiler\n{\n";
            output << "    namespace\n    {\n";
            output << "        const char* const KindNames[] = {\n";
            for (const std::string& kind : kindNames)
            {
                output << "            " << Quote(kind) << ",\n";
            }
            output << "        };\n\n";
            output << "        const char* const PropertyNames[] = {\n";
            for (const std::string& property : propertyNames)
            {
                output << "            " << Quote(property) << ",\n";
            }
            output << "        };\n    }\n\n";

            output << "    const char* ToString(ElementKind kind)\n    {\n";
            output << "        const auto index = static_cast<std::size_t>(kind);\n";
            output << "        return index < sizeof(KindNames) / sizeof(KindNames[0]) ? KindNames[index] : \"Unknown\";\n";
            output << "    }\n\n";

            output << "    const char* ToString(NativeProperty property)\n    {\n";
            output << "        const auto index = static_cast<std::size_t>(property);\n";
            output << "        return index < sizeof(PropertyNames) / sizeof(PropertyNames[0]) ? PropertyNames[index]\n";
            output << "                                                                          : \"Unknown\";\n";
            output << "    }\n";
            output << "}\n\n";

            output << "namespace RoboHTMLCompiler::Generated\n{\n";

            // Element attribute name arrays.
            output << "    namespace\n    {\n";
            for (std::size_t index = 0; index < elements.size(); ++index)
            {
                if (elements[index].scopedAttributes.empty())
                {
                    continue;
                }

                output << "        const char* const ElementAttributes" << index << "[] = {\n";
                for (const std::string& attribute : elements[index].scopedAttributes)
                {
                    output << "            " << Quote(attribute) << ",\n";
                }
                output << "        };\n";
            }

            // Attribute allowed-element arrays.
            for (std::size_t index = 0; index < attributes.size(); ++index)
            {
                if (attributes[index].allowedElements.empty())
                {
                    continue;
                }

                output << "        const char* const AttributeElements" << index << "[] = {\n";
                for (const std::string& element : attributes[index].allowedElements)
                {
                    output << "            " << Quote(element) << ",\n";
                }
                output << "        };\n";
            }
            output << "    }\n\n";

            output << "    const ElementRecord ElementTable[] = {\n";
            for (std::size_t index = 0; index < elements.size(); ++index)
            {
                const ElementSpec& element = elements[index];
                output << "        { " << Quote(element.name) << ", NamespaceId::" << NamespaceName(element.ns) << ", "
                       << "ElementKind::" << element.kind << ", " << element.headingLevel << ", ";

                std::vector<std::string> flagNames;
                if (element.voidElement) flagNames.push_back("ElementFlagVoid");
                if (element.obsolete) flagNames.push_back("ElementFlagObsolete");
                if (element.globalAttributes) flagNames.push_back("ElementFlagGlobalAttributes");
                if (element.ordered) flagNames.push_back("ElementFlagOrdered");
                if (element.rawText) flagNames.push_back("ElementFlagRawText");
                if (element.phrasing) flagNames.push_back("ElementFlagPhrasing");
                if (element.formAssociated) flagNames.push_back("ElementFlagFormAssociated");
                if (element.interactive) flagNames.push_back("ElementFlagInteractive");

                if (flagNames.empty())
                {
                    output << "ElementFlagNone, ";
                }
                else
                {
                    for (std::size_t flag = 0; flag < flagNames.size(); ++flag)
                    {
                        output << flagNames[flag] << (flag + 1 == flagNames.size() ? "" : " | ");
                    }
                    output << ", ";
                }

                if (element.scopedAttributes.empty())
                {
                    output << "nullptr, 0, ";
                }
                else
                {
                    output << "ElementAttributes" << index << ", " << element.scopedAttributes.size() << ", ";
                }

                output << Quote(element.sectionId) << ", " << Quote(element.domInterface) << " },\n";
            }
            output << "    };\n\n";
            output << "    const std::uint32_t ElementCount = " << elements.size() << ";\n\n";

            output << "    const AttributeRecord AttributeTable[] = {\n";
            for (std::size_t index = 0; index < attributes.size(); ++index)
            {
                const AttributeSpec& attribute = attributes[index];
                output << "        { " << Quote(attribute.name) << ", NamespaceId::" << NamespaceName(attribute.ns) << ", "
                       << "NativeProperty::" << attribute.property << ", ";

                std::vector<std::string> flagNames;
                if (attribute.namespaceDefault) flagNames.push_back("AttributeFlagNamespaceDefault");
                if (attribute.category == "Global") flagNames.push_back("AttributeFlagGlobal");
                if (attribute.category == "Event handler") flagNames.push_back("AttributeFlagEventHandler");
                if (attribute.category == "ARIA") flagNames.push_back("AttributeFlagAria");
                if (attribute.ns == Namespace::Svg) flagNames.push_back("AttributeFlagSvg");
                if (attribute.ns == Namespace::MathMl) flagNames.push_back("AttributeFlagMathMl");
                if (attribute.forbidden) flagNames.push_back("AttributeFlagForbidden");
                if (attribute.obsolete) flagNames.push_back("AttributeFlagObsolete");
                if (!attribute.notCompileTime) flagNames.push_back("AttributeFlagCompileTime");
                if (attribute.runtimeMutable) flagNames.push_back("AttributeFlagRuntimeMutable");
                flagNames.push_back("AttributeFlagBrowserPreserved");

                if (flagNames.empty())
                {
                    output << "AttributeFlagNone, ";
                }
                else
                {
                    for (std::size_t flag = 0; flag < flagNames.size(); ++flag)
                    {
                        output << flagNames[flag] << (flag + 1 == flagNames.size() ? "" : " | ");
                    }
                    output << ", ";
                }

                output << Quote(attribute.valueType) << ", " << Quote(attribute.category) << ", ";

                if (attribute.allowedElements.empty())
                {
                    output << "nullptr, 0 },\n";
                }
                else
                {
                    output << "AttributeElements" << index << ", " << attribute.allowedElements.size() << " },\n";
                }
            }
            output << "    };\n\n";
            output << "    const std::uint32_t AttributeCount = " << attributes.size() << ";\n\n";

            output << "    const InputTypeRecord InputTypeTable[] = {\n";
            for (const InputTypeSpec& inputType : inputTypes)
            {
                output << "        { " << Quote(inputType.typeValue) << ", ElementKind::" << inputType.kind << ", "
                       << (inputType.obsolete ? "AttributeFlagObsolete" : "AttributeFlagNone") << " },\n";
            }
            output << "    };\n\n";
            output << "    const std::uint32_t InputTypeCount = " << inputTypes.size() << ";\n";
            output << "}\n";

            WriteFile(path, output.str());
        }

        void UpdateCoverage(const fs::path& path) const
        {
            std::string error;
            const std::optional<JsonValue> parsed = JsonValue::Parse(ReadWholeFile(path), &error);
            if (!parsed.has_value())
            {
                Fail("failed to parse coverage manifest: " + error);
            }

            JsonValue root = *parsed;

            root.Set("svg", MakeSection(elements, Namespace::Svg, attributes));
            root.Set("mathml", MakeSection(elements, Namespace::MathMl, attributes));
            root.Set("aria", MakeAriaSection(attributes));
            root.Set("inputTypes", MakeInputTypeSection());

            JsonValue native = JsonValue::MakeObject();
            JsonValue kinds = JsonValue::MakeArray();
            for (const std::string& kind : kindNames)
            {
                kinds.Append(JsonValue::MakeString(kind));
            }
            native.Set("nativeKinds", std::move(kinds));
            JsonValue properties = JsonValue::MakeArray();
            for (const std::string& property : propertyNames)
            {
                properties.Append(JsonValue::MakeString(property));
            }
            native.Set("nativeProperties", std::move(properties));
            native.Set("normalizationCoverage",
                       JsonValue::MakeString("every reference element has a native category"));
            root.Set("native", std::move(native));

            JsonValue summary = JsonValue::MakeObject();
            summary.Set("elementCount", JsonValue::MakeInteger(static_cast<long long>(elements.size())));
            summary.Set("attributeRecordCount", JsonValue::MakeInteger(static_cast<long long>(attributes.size())));
            summary.Set("inputTypeCount", JsonValue::MakeInteger(static_cast<long long>(inputTypes.size())));
            root.Set("summary", std::move(summary));

            WriteFile(path, root.Serialize(true));
        }

        JsonValue MakeSection(const std::vector<ElementSpec>& allElements,
                              Namespace ns,
                              const std::vector<AttributeSpec>& allAttributes) const
        {
            JsonValue section = JsonValue::MakeObject();

            JsonValue implemented = JsonValue::MakeArray();
            JsonValue deprecated = JsonValue::MakeArray();
            for (const ElementSpec& element : allElements)
            {
                if (element.ns != ns)
                {
                    continue;
                }
                if (element.obsolete)
                {
                    deprecated.Append(JsonValue::MakeString(element.name));
                }
                else
                {
                    implemented.Append(JsonValue::MakeString(element.name));
                }
            }

            JsonValue implementedAttributes = JsonValue::MakeArray();
            JsonValue pendingAttributes = JsonValue::MakeArray();
            for (const AttributeSpec& attribute : allAttributes)
            {
                if (attribute.ns != ns)
                {
                    continue;
                }
                if (attribute.property == "Unmapped")
                {
                    pendingAttributes.Append(JsonValue::MakeString(attribute.name));
                }
                else
                {
                    implementedAttributes.Append(JsonValue::MakeString(attribute.name));
                }
            }

            section.Set("implemented", std::move(implemented));
            section.Set("partial", JsonValue::MakeArray());
            section.Set("pending", JsonValue::MakeArray());
            section.Set("unsupported", JsonValue::MakeArray());
            section.Set("deprecated", std::move(deprecated));
            section.Set("implementedAttributes", std::move(implementedAttributes));
            section.Set("pendingAttributes", std::move(pendingAttributes));

            JsonValue origin = JsonValue::MakeString("hand authored registry (see Registry/)");
            section.Set("origin", std::move(origin));
            section.Set("status", JsonValue::MakeString("implemented"));

            return section;
        }

        JsonValue MakeAriaSection(const std::vector<AttributeSpec>& allAttributes) const
        {
            JsonValue section = JsonValue::MakeObject();

            JsonValue implemented = JsonValue::MakeArray();
            JsonValue pending = JsonValue::MakeArray();
            JsonValue deprecated = JsonValue::MakeArray();

            for (const AttributeSpec& attribute : allAttributes)
            {
                if (attribute.category != "ARIA" && attribute.name != "role")
                {
                    continue;
                }
                if (attribute.obsolete)
                {
                    deprecated.Append(JsonValue::MakeString(attribute.name));
                }
                else if (attribute.property == "Unmapped")
                {
                    pending.Append(JsonValue::MakeString(attribute.name));
                }
                else
                {
                    implemented.Append(JsonValue::MakeString(attribute.name));
                }
            }

            section.Set("implemented", std::move(implemented));
            section.Set("partial", JsonValue::MakeArray());
            section.Set("pending", std::move(pending));
            section.Set("unsupported", JsonValue::MakeArray());
            section.Set("deprecated", std::move(deprecated));
            section.Set("status", JsonValue::MakeString("implemented"));
            section.Set("origin",
                        JsonValue::MakeString("WAI-ARIA 1.2, authored in Registry/AriaAttributeRegistry.json"));

            return section;
        }

        JsonValue MakeInputTypeSection() const
        {
            JsonValue section = JsonValue::MakeObject();

            JsonValue implemented = JsonValue::MakeArray();
            JsonValue deprecated = JsonValue::MakeArray();
            for (const InputTypeSpec& inputType : inputTypes)
            {
                if (inputType.obsolete)
                {
                    deprecated.Append(JsonValue::MakeString(inputType.typeValue));
                }
                else
                {
                    implemented.Append(JsonValue::MakeString(inputType.typeValue));
                }
            }

            section.Set("implemented", std::move(implemented));
            section.Set("partial", JsonValue::MakeArray());
            section.Set("pending", JsonValue::MakeArray());
            section.Set("unsupported", JsonValue::MakeArray());
            section.Set("deprecated", std::move(deprecated));
            section.Set("status", JsonValue::MakeString("implemented"));
            section.Set("origin", JsonValue::MakeString("HTML Standard input type index"));

            return section;
        }
    };
}

int main(int argumentCount, char** argumentValues)
{
    fs::path handRegistryDirectory = "Registry";
    fs::path extractedRegistryDirectory = "CompilerKnowledge/Vocabulary";
    fs::path includeOutputDirectory = "CompilerKnowledge/Include/RoboHTMLCompiler";
    fs::path sourceOutputDirectory = "CompilerKnowledge/Source";
    fs::path coveragePath = "CompilerKnowledge/ImplementationCoverage.json";

    if (argumentCount > 1) handRegistryDirectory = argumentValues[1];
    if (argumentCount > 2) extractedRegistryDirectory = argumentValues[2];
    if (argumentCount > 3) includeOutputDirectory = argumentValues[3];
    if (argumentCount > 4) sourceOutputDirectory = argumentValues[4];
    if (argumentCount > 5) coveragePath = argumentValues[5];

    RegistryCompiler compiler;
    if (!compiler.Run(handRegistryDirectory, extractedRegistryDirectory,
                      includeOutputDirectory, sourceOutputDirectory, coveragePath))
    {
        return 1;
    }

    return 0;
}
