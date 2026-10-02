#include "RoboHTMLCompiler/CompiledUiBuilder.h"

#include "RoboHTMLCompiler/Ast.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace RoboHTMLCompiler
{
    using namespace RoboRenderer;

    namespace
    {
        /// Interns strings so identical content is stored once. Index 0 is
        /// reserved for the empty string, which lets a zero StringIndex mean
        /// "none" without a validity flag anywhere in the format.
        class StringInterner
        {
        public:
            explicit StringInterner(std::vector<std::string>& target)
                : strings(target)
            {
                strings.clear();
                strings.emplace_back();
            }

            StringIndex Intern(const std::string& value)
            {
                if (value.empty())
                {
                    return 0;
                }

                const auto found = lookup.find(value);
                if (found != lookup.end())
                {
                    return found->second;
                }

                const auto index = static_cast<StringIndex>(strings.size());
                strings.push_back(value);
                lookup.emplace(value, index);
                return index;
            }

        private:
            std::vector<std::string>& strings;
            std::unordered_map<std::string, StringIndex> lookup;
        };

        float ParseNumber(const std::string& value, float fallback, bool& ok)
        {
            ok = false;
            if (value.empty())
            {
                return fallback;
            }

            char* end = nullptr;
            const double parsed = std::strtod(value.c_str(), &end);
            if (end == value.c_str())
            {
                return fallback;
            }

            ok = true;
            return static_cast<float>(parsed);
        }

        int ParseInteger(const std::string& value, int fallback, bool& ok)
        {
            ok = false;
            if (value.empty())
            {
                return fallback;
            }

            char* end = nullptr;
            const long parsed = std::strtol(value.c_str(), &end, 10);
            if (end == value.c_str())
            {
                return fallback;
            }

            ok = true;
            return static_cast<int>(parsed);
        }

        const NativeAttribute* FindAttribute(const NativeElement& element, NativeProperty property)
        {
            for (const NativeAttribute& attribute : element.attributes)
            {
                if (attribute.property == property)
                {
                    return &attribute;
                }
            }
            return nullptr;
        }

        EventKind EventKindFromName(const std::string& name)
        {
            const std::string lowered = ToLowerAscii(name);

            if (lowered == "click") return EventKind::Click;
            if (lowered == "dblclick" || lowered == "doubleclick") return EventKind::DoubleClick;
            if (lowered == "pointerdown" || lowered == "mousedown") return EventKind::PointerDown;
            if (lowered == "pointerup" || lowered == "mouseup") return EventKind::PointerUp;
            if (lowered == "pointermove" || lowered == "mousemove") return EventKind::PointerMove;
            if (lowered == "pointerenter" || lowered == "mouseenter") return EventKind::PointerEnter;
            if (lowered == "pointerleave" || lowered == "mouseleave") return EventKind::PointerLeave;
            if (lowered == "wheel" || lowered == "mousewheel") return EventKind::Wheel;
            if (lowered == "keydown") return EventKind::KeyDown;
            if (lowered == "keyup") return EventKind::KeyUp;
            if (lowered == "focus" || lowered == "focusin") return EventKind::Focus;
            if (lowered == "blur" || lowered == "focusout") return EventKind::Blur;
            if (lowered == "input") return EventKind::InputEvent;
            if (lowered == "change") return EventKind::Change;
            if (lowered == "submit") return EventKind::Submit;
            if (lowered == "reset") return EventKind::Reset;
            if (lowered == "toggle") return EventKind::Toggle;
            if (lowered == "scroll") return EventKind::Scroll;
            if (lowered == "load") return EventKind::Load;
            if (lowered == "error") return EventKind::ErrorEvent;

            return EventKind::Unknown;
        }

        std::string LoweredExtension(const std::string& source)
        {
            const std::size_t dot = source.find_last_of('.');
            if (dot == std::string::npos)
            {
                return std::string();
            }

            std::string extension = source.substr(dot + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return extension;
        }

        /// Resource kind inferred from the file name. The compiler knows this at
        /// compile time; the runtime should never have to guess from a string.
        ResourceKind ResourceKindFor(const std::string& source)
        {
            const std::string extension = LoweredExtension(source);

            if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "gif" ||
                extension == "bmp" || extension == "webp" || extension == "svg" || extension == "ico")
            {
                return ResourceKind::Image;
            }

            if (extension == "mp4" || extension == "webm" || extension == "ogg" || extension == "mp3" ||
                extension == "wav" || extension == "m4a")
            {
                return ResourceKind::Media;
            }

            if (extension == "woff" || extension == "woff2" || extension == "ttf" || extension == "otf")
            {
                return ResourceKind::Font;
            }

            if (extension == "css") return ResourceKind::StyleSheet;
            if (extension == "js" || extension == "mjs") return ResourceKind::Script;
            if (extension == "html" || extension == "htm") return ResourceKind::Document;

            return ResourceKind::None;
        }

        /// True when the user can point at the element and press it.
        bool IsInteractiveCategory(RenderCategory category)
        {
            switch (category)
            {
            case RenderCategory::Link:
            case RenderCategory::Button:
            case RenderCategory::Input:
            case RenderCategory::Checkbox:
            case RenderCategory::Radio:
            case RenderCategory::Range:
            case RenderCategory::ColorInput:
            case RenderCategory::FileInput:
            case RenderCategory::TextArea:
            case RenderCategory::Select:
            case RenderCategory::Option:
            case RenderCategory::DisclosureSummary:
            case RenderCategory::Dialog:
                return true;
            default:
                return false;
            }
        }

        bool IsFocusableCategory(RenderCategory category)
        {
            return IsInteractiveCategory(category) || category == RenderCategory::Label;
        }

        std::uint16_t ClampSpan(std::uint32_t value)
        {
            if (value == 0)
            {
                return 1;
            }
            return static_cast<std::uint16_t>(value > 1000u ? 1000u : value);
        }

        std::uint16_t ClampCount(std::uint32_t value)
        {
            return static_cast<std::uint16_t>(value > 1000u ? 1000u : value);
        }

        /// Values that every element gathers from its typed attributes before the
        /// category decides what to do with them.
        struct GatherResult
        {
            StringIndex listStyle = 0;
            std::int32_t listStart = 1;
            float numericValue = 0.0f;
            float minimum = 0.0f;
            float maximum = 0.0f;
            std::uint64_t flags = 0;
        };

        GatherResult GatherAttributes(const NativeElement& node, StringInterner& interner)
        {
            GatherResult result;
            bool ok = false;

            const float value = ParseNumber(node.content.value, 0.0f, ok);
            if (ok)
            {
                result.numericValue = value;
                result.flags |= ElementFlagHasNumericValue;
            }

            const float minimum = ParseNumber(node.constraints.minimum, 0.0f, ok);
            if (ok)
            {
                result.minimum = minimum;
                result.flags |= ElementFlagHasMinimum;
            }

            const float maximum = ParseNumber(node.constraints.maximum, 0.0f, ok);
            if (ok)
            {
                result.maximum = maximum;
                result.flags |= ElementFlagHasMaximum;
            }

            if (const NativeAttribute* style = FindAttribute(node, NativeProperty::ListStyle))
            {
                result.listStyle = interner.Intern(style->value);
                result.flags |= ElementFlagHasListStyle;
            }
            else if (const NativeAttribute* style = FindAttribute(node, NativeProperty::ListType))
            {
                result.listStyle = interner.Intern(style->value);
                result.flags |= ElementFlagHasListStyle;
            }

            if (const NativeAttribute* start = FindAttribute(node, NativeProperty::ListStart))
            {
                const int parsed = ParseInteger(start->value, 1, ok);
                if (ok)
                {
                    result.listStart = parsed;
                    result.flags |= ElementFlagHasListStart;
                }
            }

            if (FindAttribute(node, NativeProperty::ListReversed) != nullptr)
            {
                result.flags |= ElementFlagReversed;
            }

            return result;
        }
    }

    const char* CompiledUiBuilder::CompilerVersion()
    {
        return "RoboHTMLCompiler/0.1.0";
    }

    CompiledUiBuilder::CompiledUiBuilder(DiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
    }

    void CompiledUiStorage::Finalize()
    {
        stringTable.clear();
        stringTable.reserve(strings.size());
        for (const std::string& value : strings)
        {
            stringTable.push_back(value.c_str());
        }

        view = CompiledDocument{};
        view.applicationName = applicationName.c_str();
        view.elements = elements.empty() ? nullptr : elements.data();
        view.elementCount = static_cast<std::uint32_t>(elements.size());
        view.strings = stringTable.empty() ? nullptr : stringTable.data();
        view.stringCount = static_cast<std::uint32_t>(stringTable.size());
        view.resources = resources.empty() ? nullptr : resources.data();
        view.resourceCount = static_cast<std::uint32_t>(resources.size());
        view.bindings = bindings.empty() ? nullptr : bindings.data();
        view.bindingCount = static_cast<std::uint32_t>(bindings.size());
        view.identifiers = identifiers.empty() ? nullptr : identifiers.data();
        view.identifierCount = static_cast<std::uint32_t>(identifiers.size());
        view.styles = styles.empty() ? nullptr : styles.data();
        view.styleCount = static_cast<std::uint32_t>(styles.size());
        view.root = 0;
        view.compilerVersion = CompiledUiBuilder::CompilerVersion();
        view.hasSourcePositions = 1;
    }

    bool CompiledUiBuilder::Build(const NativeUiModel& model,
                                  const std::string& applicationName,
                                  CompiledUiStorage& storage,
                                  const CssStyleTable* authoredStyles)
    {
        if (model.root == nullptr)
        {
            diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                              SourceLocation{ applicationName + ".html", 1, 1, 0 },
                              "The native model has no root element to lower.",
                              std::string(),
                              "Ensure the application document contains an <html> and a <body> element.");
            return false;
        }

        storage.applicationName = applicationName.empty() ? std::string("Application") : applicationName;

        StringInterner interner(storage.strings);

        // Resource deduplication: two elements that point at the same file share
        // one resource entry, so the runtime loads and decodes the file once.
        std::unordered_map<std::string, ResourceIndex> resourceLookup;
        storage.resources.clear();
        storage.resources.emplace_back(CompiledResource{}); // slot 0 is "no resource"

        const auto internResource = [&](const std::string& source, ResourceKind declared,
                                        std::uint16_t declaredWidth, std::uint16_t declaredHeight,
                                        bool hasAlternativeText) -> ResourceIndex {
            if (source.empty())
            {
                return 0;
            }

            const auto found = resourceLookup.find(source);
            if (found != resourceLookup.end())
            {
                return found->second;
            }

            CompiledResource resource{};
            resource.source = interner.Intern(source);
            resource.kind = declared;
            // Inline SVG markup has no file extension to infer from; the
            // compiler knows what it is and says so rather than guessing.
            resource.inferredKind = declared == ResourceKind::SvgMarkup
                                        ? ResourceKind::SvgMarkup
                                        : ResourceKindFor(source);
            resource.declaredWidth = declaredWidth;
            resource.declaredHeight = declaredHeight;
            resource.hasAlternativeText = hasAlternativeText ? 1 : 0;
            resource.reserved = 0;

            const auto index = static_cast<ResourceIndex>(storage.resources.size());
            storage.resources.push_back(resource);
            resourceLookup.emplace(source, index);
            return index;
        };

        // Element indices are assigned in document order, root first, so index 0
        // is always the root and a document-order traversal is a forward walk.
        storage.elements.clear();
        storage.sourceElementIds.clear();

        std::function<ElementIndex(const NativeElement&, ElementIndex)> lower =
            [&](const NativeElement& node, ElementIndex parent) -> ElementIndex {
                const ElementIndex index = static_cast<ElementIndex>(storage.elements.size());
                storage.elements.emplace_back(CompiledElement{});
                storage.sourceElementIds.push_back(node.id);

                CompiledElement& element = storage.elements.back();
                element.kind = node.kind;
                element.flags = ElementFlagNone;

                // The style the cascade gave this element, if any. A text run is
                // lowered out of its parent's content and has no element of its
                // own to match a selector against, so the runtime sets it in the
                // type of the element it came from - which is what CSS says a
                // text node's style is.
                element.styleIndex = 0;

                if (authoredStyles != nullptr)
                {
                    const std::uint32_t styled =
                        authoredStyles->StyleIndexAt(node.source.line, node.source.column);

                    if (styled != 0u)
                    {
                        element.styleIndex = static_cast<std::uint16_t>(styled);
                    }
                    else if (parent != NoElement &&
                             (storage.elements[parent].flags & ElementFlagTextRun) != 0u)
                    {
                        element.styleIndex = storage.elements[parent].styleIndex;
                    }
                }

                element.headingLevel = 0;
                element.columnSpan = ClampSpan(node.layout.columnSpan);
                element.rowSpan = ClampSpan(node.layout.rowSpan);
                element.columns = node.layout.hasColumns ? ClampCount(node.layout.columns) : 0;
                element.rows = node.layout.hasRows ? ClampCount(node.layout.rows) : 0;
                element.buttonBehavior = ButtonBehavior::Submit;
                element.tabIndex = 0;
                element.listStart = 1;
                element.parent = parent;
                element.firstChild = NoElement;
                element.lastChild = NoElement;
                element.nextSibling = NoElement;
                element.text = 0;
                element.identifier = 0;
                element.name = 0;
                element.auxiliaryText = 0;
                element.value = 0;
                element.href = 0;
                element.labelTarget = 0;
                element.listStyle = 0;
                element.resource = 0;
                element.attributeWidth = 0.0f;
                element.attributeHeight = 0.0f;
                element.numericValue = 0.0f;
                element.minimum = 0.0f;
                element.maximum = 0.0f;
                element.firstBinding = 0;
                element.bindingCount = 0;
                element.sourceLine = static_cast<std::uint32_t>(node.source.line > 0 ? node.source.line : 0);
                element.sourceColumn = static_cast<std::uint32_t>(node.source.column > 0 ? node.source.column : 0);

                const RenderCategory category = CategoryFor(node.kind);

                if (node.state.hidden) element.flags |= ElementFlagHidden;
                if (node.state.disabled) element.flags |= ElementFlagDisabled;
                if (node.state.checked) element.flags |= ElementFlagChecked;
                if (node.state.selected) element.flags |= ElementFlagSelected;
                if (node.state.readOnly) element.flags |= ElementFlagReadOnly;
                if (node.state.required) element.flags |= ElementFlagRequired;
                if (node.state.inert) element.flags |= ElementFlagInert;
                if (node.state.open) element.flags |= ElementFlagOpen;
                if (node.state.multiple) element.flags |= ElementFlagMultiple;
                if (node.state.autofocus) element.flags |= ElementFlagAutofocus;
                if (node.media.controls) element.flags |= ElementFlagControls;
                if (node.media.autoPlay) element.flags |= ElementFlagAutoPlay;
                if (node.media.loop) element.flags |= ElementFlagLoop;
                if (node.media.muted) element.flags |= ElementFlagMuted;
                if (node.validation.formNoValidate) element.flags |= ElementFlagNoValidate;
                if (node.nature.inlineContent) element.flags |= ElementFlagInline;
                if (node.nature.preservesWhitespace) element.flags |= ElementFlagPreserveWhitespace;
                if (node.nature.interactive || IsInteractiveCategory(category)) element.flags |= ElementFlagInteractive;
                if (IsFocusableCategory(category)) element.flags |= ElementFlagFocusable;

                const std::string loweredDirection = ToLowerAscii(node.content.direction);
                if (loweredDirection == "rtl")
                {
                    element.flags |= ElementFlagRightToLeft;
                }

                // A run of text has no element of its own, so the runtime reads
                // its type off the element it flowed out of rather than off a
                // style of its own. The lowering records that here.
                if (node.nature.textRun)
                {
                    element.flags |= ElementFlagTextRun;
                }

                element.text = interner.Intern(node.content.text);
                if (element.text != 0)
                {
                    element.flags |= ElementFlagHasText;
                }

                element.identifier = interner.Intern(node.content.id);
                if (element.identifier != 0)
                {
                    element.flags |= ElementFlagHasId;
                }

                element.name = interner.Intern(node.content.name);
                if (element.name != 0)
                {
                    element.flags |= ElementFlagHasName;
                }

                element.href = interner.Intern(node.navigation.href);
                if (element.href != 0)
                {
                    element.flags |= ElementFlagHasHref;
                }

                if (node.layout.hasWidth)
                {
                    element.attributeWidth = static_cast<float>(node.layout.width);
                    element.flags |= ElementFlagHasWidth;
                }
                if (node.layout.hasHeight)
                {
                    element.attributeHeight = static_cast<float>(node.layout.height);
                    element.flags |= ElementFlagHasHeight;
                }
                if (node.layout.hasRows) element.flags |= ElementFlagHasRows;
                if (node.layout.hasColumns) element.flags |= ElementFlagHasColumns;

                const GatherResult gathered = GatherAttributes(node, interner);
                element.flags |= gathered.flags;
                element.numericValue = gathered.numericValue;
                element.minimum = gathered.minimum;
                element.maximum = gathered.maximum;
                element.listStart = gathered.listStart;
                element.listStyle = gathered.listStyle;

                if (!node.content.value.empty())
                {
                    element.value = interner.Intern(node.content.value);
                    element.flags |= ElementFlagHasValue;
                }

                switch (category)
                {
                case RenderCategory::Heading:
                    element.headingLevel = static_cast<std::uint16_t>(
                        node.accessibility.level > 0 ? std::min(node.accessibility.level, 6) : 1);
                    break;

                case RenderCategory::Input:
                case RenderCategory::TextArea:
                    element.auxiliaryText = interner.Intern(node.content.placeholder);
                    if (element.auxiliaryText != 0)
                    {
                        element.flags |= ElementFlagHasPlaceholder;
                    }
                    if (const NativeAttribute* size = FindAttribute(node, NativeProperty::InputSize))
                    {
                        bool ok = false;
                        const int parsed = ParseInteger(size->value, 0, ok);
                        if (ok && parsed > 0)
                        {
                            element.columns = ClampCount(static_cast<std::uint32_t>(parsed));
                            element.flags |= ElementFlagHasColumns;
                        }
                    }
                    break;

                case RenderCategory::Image:
                {
                    element.auxiliaryText = interner.Intern(node.content.alternativeText);
                    if (element.auxiliaryText != 0)
                    {
                        element.flags |= ElementFlagHasAlternativeText;
                    }

                    if (node.kind == ElementKind::SvgDocument)
                    {
                        // An `<svg>` element has no file of its own: the compiler
                        // serialized its subtree into the string table, and the
                        // runtime rasterizes it through the image pipeline. Its
                        // own width/height (or its viewBox) set the raster size.
                        if (node.resource.source.empty())
                        {
                            break;
                        }

                        const double svgWidth = node.svgGeometry.width;
                        const double svgHeight = node.svgGeometry.height;
                        const std::uint32_t svgColumns =
                            svgWidth > 0.0 ? static_cast<std::uint32_t>(svgWidth) : 0u;
                        const std::uint32_t svgRows =
                            svgHeight > 0.0 ? static_cast<std::uint32_t>(svgHeight) : 0u;

                        element.resource = internResource(node.resource.source, ResourceKind::SvgMarkup,
                                                          ClampCount(svgColumns), ClampCount(svgRows), false);

                        if (element.resource != 0)
                        {
                            element.flags |= ElementFlagHasResource;
                        }
                        if (svgWidth > 0.0)
                        {
                            element.attributeWidth = static_cast<float>(svgWidth);
                            element.flags |= ElementFlagHasWidth;
                        }
                        if (svgHeight > 0.0)
                        {
                            element.attributeHeight = static_cast<float>(svgHeight);
                            element.flags |= ElementFlagHasHeight;
                        }
                        break;
                    }

                    const std::uint32_t width = node.layout.hasWidth ? node.layout.width : 0u;
                    const std::uint32_t height = node.layout.hasHeight ? node.layout.height : 0u;
                    element.resource = internResource(node.resource.source, ResourceKind::Image,
                                                      ClampCount(width), ClampCount(height),
                                                      element.auxiliaryText != 0);
                    if (element.resource != 0)
                    {
                        element.flags |= ElementFlagHasResource;
                    }
                    break;
                }

                case RenderCategory::Media:
                    element.resource = internResource(node.media.source, ResourceKind::Media, 0, 0, false);
                    if (element.resource != 0)
                    {
                        element.flags |= ElementFlagHasResource;
                    }
                    break;

                case RenderCategory::Frame:
                case RenderCategory::EmbeddedObject:
                    element.resource = internResource(node.resource.source, ResourceKind::Document, 0, 0, false);
                    if (element.resource != 0)
                    {
                        element.flags |= ElementFlagHasResource;
                    }
                    break;

                case RenderCategory::Button:
                    if (const NativeAttribute* type = FindAttribute(node, NativeProperty::ButtonType))
                    {
                        const std::string lowered = ToLowerAscii(type->value);
                        if (lowered == "reset")
                        {
                            element.buttonBehavior = ButtonBehavior::Reset;
                        }
                        else if (lowered == "button")
                        {
                            element.buttonBehavior = ButtonBehavior::None;
                        }
                    }
                    if (node.kind == ElementKind::ResetButton)
                    {
                        element.buttonBehavior = ButtonBehavior::Reset;
                    }
                    else if (node.kind == ElementKind::ImageButton)
                    {
                        element.buttonBehavior = ButtonBehavior::None;
                    }
                    break;

                case RenderCategory::Option:
                    if (const NativeAttribute* label = FindAttribute(node, NativeProperty::OptionLabel))
                    {
                        element.auxiliaryText = interner.Intern(label->value);
                    }
                    break;

                case RenderCategory::Label:
                    if (const NativeAttribute* target = FindAttribute(node, NativeProperty::LabelFor))
                    {
                        element.labelTarget = interner.Intern(target->value);
                        if (element.labelTarget != 0)
                        {
                            element.flags |= ElementFlagHasLabelTarget;
                        }
                    }
                    break;

                case RenderCategory::List:
                    if (node.nature.orderedList)
                    {
                        if (element.listStyle == 0)
                        {
                            element.listStyle = interner.Intern("decimal");
                        }
                    }
                    else if (element.listStyle == 0)
                    {
                        element.listStyle = interner.Intern("disc");
                    }
                    break;

                default:
                    break;
                }

                // The child links are written through the table, not through a
                // reference: lowering a child can grow the element table, and a
                // held reference would not survive the reallocation.
                ElementIndex firstChild = NoElement;
                ElementIndex lastChild = NoElement;

                for (const std::unique_ptr<NativeElement>& child : node.children)
                {
                    const ElementIndex childIndex = lower(*child, index);
                    if (firstChild == NoElement)
                    {
                        firstChild = childIndex;
                    }
                    else
                    {
                        storage.elements[lastChild].nextSibling = childIndex;
                    }

                    lastChild = childIndex;
                }

                storage.elements[index].firstChild = firstChild;
                storage.elements[index].lastChild = lastChild;

                return index;
            };

        const ElementIndex root = lower(*model.root, NoElement);
        if (root != 0)
        {
            diagnostics.Error(DiagnosticCode::GeneratedOutputInvalid,
                              SourceLocation{ storage.applicationName + ".html", 1, 1, 0 },
                              "Lowering did not produce a root element at index 0.");
            return false;
        }

        // Identifier table, sorted by identifier text so the runtime can binary
        // search it: that is what makes a `for` attribute or an external lookup
        // possible without a hash map or a string scan at run time.
        {
            std::vector<CompiledIdEntry> entries;
            std::unordered_map<std::string, ElementIndex> idToElement;

            for (ElementIndex index = 0; index < storage.elements.size(); ++index)
            {
                const CompiledElement& element = storage.elements[index];
                if (element.identifier == 0)
                {
                    continue;
                }

                idToElement.emplace(storage.Strings()[element.identifier], index);

                CompiledIdEntry entry{};
                entry.identifier = element.identifier;
                entry.element = index;
                entries.push_back(entry);
            }

            const std::vector<std::string>& strings = storage.Strings();
            std::sort(entries.begin(), entries.end(),
                      [&strings](const CompiledIdEntry& left, const CompiledIdEntry& right) {
                          const int order =
                              std::strcmp(strings[left.identifier].c_str(), strings[right.identifier].c_str());
                          if (order != 0)
                          {
                              return order < 0;
                          }
                          // Duplicate identifiers keep document order so a run
                          // stays reproducible; the runtime reports the duplicate.
                          return left.element < right.element;
                      });

            storage.identifiers = std::move(entries);

            // Behaviour bindings. The behaviour module refers to elements by
            // their source identifier; resolving that here means the runtime
            // dispatch path never touches a string or a lookup structure.
            std::vector<CompiledBinding> resolved;
            resolved.reserve(model.eventBindings.size());

            for (const EventBinding& binding : model.eventBindings)
            {
                const auto found = idToElement.find(binding.targetId);
                if (found == idToElement.end())
                {
                    diagnostics.Warning(DiagnosticCode::GeneratedOutputInvalid,
                                        SourceLocation{ binding.sourceFile, binding.line > 0 ? binding.line : 1, 1, 0 },
                                        "A behaviour binding targets an element that is not in the document.",
                                        "No element carries the identifier '" + binding.targetId + "'.",
                                        "Remove the binding or add the identifier to the element it targets.");
                    continue;
                }

                CompiledBinding entry{};
                entry.element = found->second;
                entry.event = EventKindFromName(binding.event);
                entry.handler = interner.Intern(binding.handler);
                resolved.push_back(entry);
            }

            std::stable_sort(resolved.begin(), resolved.end(),
                             [](const CompiledBinding& left, const CompiledBinding& right) {
                                 return left.element < right.element;
                             });

            // Each element names its own contiguous slice of the binding table.
            for (CompiledElement& element : storage.elements)
            {
                element.firstBinding = 0;
                element.bindingCount = 0;
            }

            for (std::size_t index = 0; index < resolved.size(); ++index)
            {
                CompiledElement& element = storage.elements[resolved[index].element];
                if (element.bindingCount == 0)
                {
                    element.firstBinding = static_cast<std::uint32_t>(index);
                }
                ++element.bindingCount;
            }

            storage.bindings = std::move(resolved);
        }

        // The authored style table. The indices the elements carry were assigned
        // by the lowering, and index 0 is the format's "no authored style", so
        // the table is copied exactly as it was built.
        if (authoredStyles != nullptr)
        {
            storage.styles = authoredStyles->styles;

            // A `font-family` list lives in the string table, not in the style
            // record, so the family names the CSS lowering collected are interned
            // here, where the table exists, and each style is pointed at its own.
            const std::size_t count = std::min(storage.styles.size(), authoredStyles->fontFamilies.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                if (!authoredStyles->fontFamilies[index].empty())
                {
                    storage.styles[index].fontFamily = interner.Intern(authoredStyles->fontFamilies[index]);
                }
            }

            // `font-feature-settings` is a declaration rather than a scalar, so
            // it lives in the string table too; the style carries its index and
            // the shaper parses it.
            const std::size_t featureCount =
                std::min(storage.styles.size(), authoredStyles->featureSettings.size());
            for (std::size_t index = 0; index < featureCount; ++index)
            {
                if (!authoredStyles->featureSettings[index].empty())
                {
                    storage.styles[index].fontFeatures =
                        interner.Intern(authoredStyles->featureSettings[index]);
                }
            }
        }

        storage.Finalize();
        return true;
    }
}
