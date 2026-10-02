#include "RoboHTMLCompiler/NativeTreeBuilder.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace RoboHTMLCompiler
{
    namespace
    {
        /// Escapes the five characters markup cannot carry literally, so a
        /// serialized attribute or text node reads back exactly as written.
        void AppendEscaped(std::string& out, const std::string& value)
        {
            for (const char character : value)
            {
                switch (character)
                {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '\"': out += "&quot;"; break;
                default: out.push_back(character); break;
                }
            }
        }

        void SerializeElement(std::string& out, const ElementNode& element);

        void SerializeNode(std::string& out, const Node& node)
        {
            if (node.kind == NodeKind::Text)
            {
                AppendEscaped(out, static_cast<const TextNode&>(node).text);
            }
            else if (node.kind == NodeKind::Element)
            {
                SerializeElement(out, static_cast<const ElementNode&>(node));
            }
            // Comments and doctypes contribute nothing to a picture.
        }

        /// Writes an element subtree back out as markup. An `<svg>` element has
        /// no rendering tree of its own in the runtime, so the compiler hands
        /// the runtime the document rather than a set of boxes it cannot lay
        /// out.
        void SerializeElement(std::string& out, const ElementNode& element)
        {
            out.push_back('<');
            out += element.sourceTag;

            for (const AttributeNode& attribute : element.attributes)
            {
                out.push_back(' ');
                out += attribute.name;
                if (attribute.hasValue)
                {
                    out += "=\"";
                    AppendEscaped(out, attribute.value);
                    out.push_back('\"');
                }
            }

            if (element.children.empty())
            {
                out += "/>";
                return;
            }

            out.push_back('>');
            for (const std::unique_ptr<Node>& child : element.children)
            {
                SerializeNode(out, *child);
            }
            out += "</";
            out += element.sourceTag;
            out.push_back('>');
        }

        bool ToBoolean(const AttributeNode& attribute)
        {
            if (!attribute.hasValue)
            {
                return true;
            }

            const std::string value = ToLowerAscii(attribute.value);
            return value != "false" && value != "0" && !value.empty();
        }

        /// Normalises one direct text run of a mixed-content element. Internal
        /// whitespace collapses to a single space, while a leading or trailing
        /// space is kept: those are the separators an element boundary splits,
        /// and losing them would glue "Hello" and "world" from
        /// `<p>Hello <b>world</b></p>` together.
        std::string NormalizeTextRun(const std::string& value)
        {
            if (value.empty())
            {
                return value;
            }

            const bool leadingSpace = std::isspace(static_cast<unsigned char>(value.front())) != 0;
            const bool trailingSpace = value.size() > 1 &&
                                       std::isspace(static_cast<unsigned char>(value.back())) != 0;

            std::string collapsed;
            collapsed.reserve(value.size());

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

            if (collapsed.empty())
            {
                // A whitespace-only run still separates its neighbours.
                return std::string(" ");
            }

            std::string result;
            result.reserve(collapsed.size() + 2);
            if (leadingSpace)
            {
                result.push_back(' ');
            }
            result += collapsed;
            if (trailingSpace)
            {
                result.push_back(' ');
            }

            return result;
        }

        std::uint32_t ToUnsigned(const std::string& value, std::uint32_t fallback)
        {
            if (value.empty())
            {
                return fallback;
            }

            char* end = nullptr;
            const long parsed = std::strtol(value.c_str(), &end, 10);
            if (end == value.c_str() || parsed < 0)
            {
                return fallback;
            }

            return static_cast<std::uint32_t>(parsed);
        }
    }

    NativeTreeBuilder::NativeTreeBuilder(DiagnosticSink& diagnosticSink,
                                         const Registry& elementRegistry,
                                         StyleReferenceResolver& resolver)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
        , styleReferenceResolver(resolver)
    {
    }

    ElementKind NativeTreeBuilder::NormalizeKind(const ElementNode& element) const
    {
        if (element.ns == MarkupNamespace::Html)
        {
            const ElementDefinition* definition = registry.FindHtmlElement(element.sourceTag);
            if (definition == nullptr)
            {
                return ElementKind::CustomElement;
            }

            // The input element is specialised at compile time from its type.
            if (definition->kind == ElementKind::Input)
            {
                const AttributeNode* type = element.FindAttribute("type");
                return registry.ResolveInputKind(type != nullptr ? type->value : std::string());
            }

            return definition->kind;
        }

        const ElementDefinition* definition = registry.FindElement(element.ns, element.sourceTag);
        if (definition != nullptr)
        {
            return definition->kind;
        }

        return (element.ns == MarkupNamespace::Svg) ? ElementKind::SvgContainer : ElementKind::MathRow;
    }

    void NativeTreeBuilder::ApplyGenericAttribute(NativeElement& target,
                                                  const AttributeDefinition& definition,
                                                  const AttributeNode& attribute) const
    {
        NativeAttribute entry;
        entry.property = definition.nativeProperty;
        entry.name = attribute.name;
        entry.value = attribute.value;
        target.attributes.push_back(std::move(entry));
    }

    void NativeTreeBuilder::RouteAttribute(NativeElement& target,
                                           const AttributeDefinition& definition,
                                           const AttributeNode& attribute) const
    {
        switch (definition.nativeProperty)
        {
        case NativeProperty::IdentityId:
            target.content.id = attribute.value;
            break;

        case NativeProperty::IdentityName:
            target.content.name = attribute.value;
            break;

        case NativeProperty::StyleReference:
        case NativeProperty::StyleSheetReference:
            // Resolved separately by the style reference resolver.
            break;

        case NativeProperty::StateHidden: target.state.hidden = ToBoolean(attribute); break;
        case NativeProperty::StateDisabled: target.state.disabled = ToBoolean(attribute); break;
        case NativeProperty::StateChecked: target.state.checked = ToBoolean(attribute); break;
        case NativeProperty::StateSelected: target.state.selected = ToBoolean(attribute); break;
        case NativeProperty::StateReadOnly: target.state.readOnly = ToBoolean(attribute); break;
        case NativeProperty::StateRequired: target.state.required = ToBoolean(attribute); break;
        case NativeProperty::StateInert: target.state.inert = ToBoolean(attribute); break;
        case NativeProperty::StateOpen: target.state.open = ToBoolean(attribute); break;
        case NativeProperty::StateMultiple: target.state.multiple = ToBoolean(attribute); break;
        case NativeProperty::StateAutofocus: target.state.autofocus = ToBoolean(attribute); break;
        case NativeProperty::StateDraggable: target.state.draggable = ToBoolean(attribute); break;
        case NativeProperty::StateSpellCheck: target.state.spellCheck = ToBoolean(attribute); break;
        case NativeProperty::StateLoading:
            target.state.loading = ToLowerAscii(attribute.value) == "lazy";
            break;
        case NativeProperty::ResourceLoadMode:
            target.state.loading = ToLowerAscii(attribute.value) == "lazy";
            break;

        case NativeProperty::ValidationPattern: target.validation.pattern = attribute.value; break;
        case NativeProperty::ValidationMinimumLength:
            target.validation.minimumLength = ToUnsigned(attribute.value, 0);
            target.validation.hasMinimumLength = true;
            break;
        case NativeProperty::ValidationMaximumLength:
            target.validation.maximumLength = ToUnsigned(attribute.value, 0);
            target.validation.hasMaximumLength = true;
            break;
        case NativeProperty::ValidationAccept: target.validation.accept = attribute.value; break;
        case NativeProperty::ValidationFormNoValidate:
        case NativeProperty::FormNoValidate:
            target.validation.formNoValidate = ToBoolean(attribute);
            break;

        case NativeProperty::ValueValue: target.content.value = attribute.value; break;
        case NativeProperty::ValueDefaultValue: target.content.value = attribute.value; break;
        case NativeProperty::InputPlaceholder: target.content.placeholder = attribute.value; break;

        case NativeProperty::ConstraintMinimum: target.constraints.minimum = attribute.value; break;
        case NativeProperty::ConstraintMaximum: target.constraints.maximum = attribute.value; break;
        case NativeProperty::ConstraintStep: target.constraints.step = attribute.value; break;
        case NativeProperty::ConstraintLow: target.constraints.low = attribute.value; break;
        case NativeProperty::ConstraintHigh: target.constraints.high = attribute.value; break;
        case NativeProperty::ConstraintOptimum: target.constraints.optimum = attribute.value; break;

        case NativeProperty::LayoutWidth:
            target.layout.width = ToUnsigned(attribute.value, 0);
            target.layout.hasWidth = true;
            break;
        case NativeProperty::LayoutHeight:
            target.layout.height = ToUnsigned(attribute.value, 0);
            target.layout.hasHeight = true;
            break;
        case NativeProperty::EmbeddedWidth:
            target.layout.width = ToUnsigned(attribute.value, 0);
            target.layout.hasWidth = true;
            break;
        case NativeProperty::EmbeddedHeight:
            target.layout.height = ToUnsigned(attribute.value, 0);
            target.layout.hasHeight = true;
            break;
        case NativeProperty::LayoutRows:
            target.layout.rows = ToUnsigned(attribute.value, 0);
            target.layout.hasRows = true;
            break;
        case NativeProperty::LayoutColumns:
            target.layout.columns = ToUnsigned(attribute.value, 0);
            target.layout.hasColumns = true;
            break;
        case NativeProperty::LayoutSpan:
            target.layout.span = ToUnsigned(attribute.value, 1);
            break;
        case NativeProperty::LayoutRowSpan:
        case NativeProperty::AccessibilityRowSpan:
            target.layout.rowSpan = ToUnsigned(attribute.value, 1);
            target.table.rowSpan = target.layout.rowSpan;
            break;
        case NativeProperty::LayoutColumnSpan:
            target.layout.columnSpan = ToUnsigned(attribute.value, 1);
            target.table.columnSpan = target.layout.columnSpan;
            break;
        case NativeProperty::LayoutScope:
            target.layout.scope = attribute.value;
            target.table.scope = attribute.value;
            break;
        case NativeProperty::LayoutHeaders:
            target.layout.headers = attribute.value;
            target.table.headers = attribute.value;
            break;

        case NativeProperty::TextLanguage:
        case NativeProperty::MediaSourceLanguage:
            target.content.language = attribute.value;
            break;
        case NativeProperty::TextDirection: target.content.direction = attribute.value; break;

        case NativeProperty::InteractionTabIndex:
            target.interaction.tabIndex = static_cast<int>(ToUnsigned(attribute.value, 0));
            target.interaction.hasTabIndex = true;
            break;
        case NativeProperty::InteractionCommand: target.interaction.command = attribute.value; break;
        case NativeProperty::InteractionCommandTarget:
            target.interaction.commandTarget = attribute.value;
            break;

        case NativeProperty::NavigationHref: target.navigation.href = attribute.value; break;
        case NativeProperty::NavigationTarget: target.navigation.target = attribute.value; break;
        case NativeProperty::NavigationDownload: target.navigation.download = attribute.value; break;
        case NativeProperty::NavigationRelation: target.navigation.relation = attribute.value; break;
        case NativeProperty::NavigationLanguage: target.navigation.language = attribute.value; break;
        case NativeProperty::NavigationMimeType: target.navigation.mimeType = attribute.value; break;
        case NativeProperty::NavigationReferrerPolicy:
            target.navigation.referrerPolicy = attribute.value;
            break;

        case NativeProperty::ResourceSource:
            target.resource.source = attribute.value;
            target.media.source = attribute.value;
            break;
        case NativeProperty::ResourceSourceSet: target.resource.sourceSet = attribute.value; break;
        case NativeProperty::ResourceSizes: target.resource.sizes = attribute.value; break;
        case NativeProperty::ResourceCandidates: target.resource.sourceSet = attribute.value; break;
        case NativeProperty::ResourceImages: target.resource.sizes = attribute.value; break;
        case NativeProperty::ResourceIntegrity: target.resource.integrity = attribute.value; break;
        case NativeProperty::ResourceCrossOrigin: target.resource.crossOrigin = attribute.value; break;
        case NativeProperty::ResourceFetchPriority: target.resource.fetchPriority = attribute.value; break;
        case NativeProperty::ResourceMedia: target.resource.media = attribute.value; break;
        case NativeProperty::ResourceDecoding: target.resource.decoding = attribute.value; break;
        case NativeProperty::ResourceDestination: target.resource.destination = attribute.value; break;
        case NativeProperty::ResourceDisabled: target.resource.disabled = ToBoolean(attribute); break;

        case NativeProperty::ImageAlternativeText: target.content.alternativeText = attribute.value; break;
        case NativeProperty::ImageMapName: target.attributes.push_back(NativeAttribute{
                                              definition.nativeProperty, attribute.name, attribute.value });
            break;
        case NativeProperty::ImageMapServerSide: target.state.loading = ToBoolean(attribute); break;

        case NativeProperty::MediaControls: target.media.controls = ToBoolean(attribute); break;
        case NativeProperty::MediaAutoPlay: target.media.autoPlay = ToBoolean(attribute); break;
        case NativeProperty::MediaLoop: target.media.loop = ToBoolean(attribute); break;
        case NativeProperty::MediaMuted: target.media.muted = ToBoolean(attribute); break;
        case NativeProperty::MediaPoster: target.media.poster = attribute.value; break;
        case NativeProperty::MediaPreload: target.media.preload = attribute.value; break;
        case NativeProperty::MediaPlayInline: target.media.playInline = ToBoolean(attribute); break;
        case NativeProperty::MediaKind: target.media.kind = attribute.value; break;

        case NativeProperty::AccessibilityRole: target.accessibility.role = attribute.value; break;
        case NativeProperty::AccessibilityLabel:
        case NativeProperty::AccessibilityLabelledBy:
            if (definition.nativeProperty == NativeProperty::AccessibilityLabel)
            {
                target.accessibility.label = attribute.value;
            }
            else
            {
                target.accessibility.labelledBy = attribute.value;
            }
            break;
        case NativeProperty::AccessibilityDescribedBy: target.accessibility.describedBy = attribute.value; break;
        case NativeProperty::AccessibilityDescription: target.accessibility.description = attribute.value; break;
        case NativeProperty::AccessibilityHidden:
            target.accessibility.hidden = ToBoolean(attribute);
            target.accessibility.hasHidden = true;
            break;
        case NativeProperty::AccessibilityDisabled:
            target.accessibility.disabled = ToBoolean(attribute);
            target.accessibility.hasDisabled = true;
            break;
        case NativeProperty::AccessibilityLive: target.accessibility.live = attribute.value; break;
        case NativeProperty::AccessibilityCurrent: target.accessibility.current = attribute.value; break;
        case NativeProperty::AccessibilityOrientation: target.accessibility.orientation = attribute.value; break;
        case NativeProperty::AccessibilityAutoComplete: target.accessibility.autoComplete = attribute.value; break;
        case NativeProperty::AccessibilityExpanded: target.accessibility.expanded = ToBoolean(attribute); break;
        case NativeProperty::AccessibilityLevel:
            target.accessibility.level = static_cast<int>(ToUnsigned(attribute.value, 0));
            break;

        case NativeProperty::Unmapped:
        case NativeProperty::ForbiddenInlineStyle:
        case NativeProperty::InputType:
        case NativeProperty::InputAutocomplete:
        case NativeProperty::InputDirectoryName:
        case NativeProperty::InputOptions:
        case NativeProperty::InputSize:
        case NativeProperty::InputWrap:
        case NativeProperty::MetadataTitle:
        default:
            ApplyGenericAttribute(target, definition, attribute);
            break;

        case NativeProperty::SvgGeometryX: target.svgGeometry.x = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryY: target.svgGeometry.y = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryCenterX: target.svgGeometry.centerX = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryCenterY: target.svgGeometry.centerY = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryRadius: target.svgGeometry.radius = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryRadiusX: target.svgGeometry.radiusX = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryRadiusY: target.svgGeometry.radiusY = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgGeometryWidth:
        case NativeProperty::SvgGeometryHeight:
        case NativeProperty::SvgGeometryX1:
        case NativeProperty::SvgGeometryY1:
        case NativeProperty::SvgGeometryX2:
        case NativeProperty::SvgGeometryY2:
        case NativeProperty::SvgGeometryPath:
        case NativeProperty::SvgGeometryPoints:
            if (definition.nativeProperty == NativeProperty::SvgGeometryWidth)
            {
                target.svgGeometry.width = std::atof(attribute.value.c_str());
            }
            else if (definition.nativeProperty == NativeProperty::SvgGeometryHeight)
            {
                target.svgGeometry.height = std::atof(attribute.value.c_str());
            }
            else if (definition.nativeProperty == NativeProperty::SvgGeometryPath)
            {
                target.svgGeometry.path = attribute.value;
            }
            else if (definition.nativeProperty == NativeProperty::SvgGeometryPoints)
            {
                target.svgGeometry.points = attribute.value;
            }
            break;
        case NativeProperty::SvgViewBox: target.svgGeometry.viewBox = attribute.value; break;
        case NativeProperty::SvgPreserveAspectRatio:
            target.svgGeometry.preserveAspectRatio = attribute.value;
            break;
        case NativeProperty::SvgPaintFill: target.svgPaint.fill = attribute.value; break;
        case NativeProperty::SvgPaintFillRule: target.svgPaint.fillRule = attribute.value; break;
        case NativeProperty::SvgPaintFillOpacity:
            target.svgPaint.fillOpacity = std::atof(attribute.value.c_str());
            break;
        case NativeProperty::SvgPaintStroke: target.svgPaint.stroke = attribute.value; break;
        case NativeProperty::SvgPaintStrokeWidth:
            target.svgPaint.strokeWidth = std::atof(attribute.value.c_str());
            break;
        case NativeProperty::SvgPaintStrokeOpacity:
            target.svgPaint.strokeOpacity = std::atof(attribute.value.c_str());
            break;
        case NativeProperty::SvgPaintStrokeLineCap: target.svgPaint.strokeLineCap = attribute.value; break;
        case NativeProperty::SvgPaintStrokeLineJoin: target.svgPaint.strokeLineJoin = attribute.value; break;
        case NativeProperty::SvgPaintStrokeDashArray: target.svgPaint.strokeDashArray = attribute.value; break;
        case NativeProperty::SvgPaintOpacity: target.svgPaint.opacity = std::atof(attribute.value.c_str()); break;
        case NativeProperty::SvgTransform: target.svgTransform.value = attribute.value; break;
        case NativeProperty::SvgTransformOrigin: target.svgTransform.origin = attribute.value; break;
        case NativeProperty::SvgGradientUnits: target.svgGradient.units = attribute.value; break;
        case NativeProperty::SvgGradientTransform: target.svgGradient.transform = attribute.value; break;
        case NativeProperty::SvgSpreadMethod: target.svgGradient.spreadMethod = attribute.value; break;
        case NativeProperty::SvgStopColor:
        case NativeProperty::SvgStopOpacity:
        {
            SvgGradient::Stop stop;
            stop.offset = "0";
            stop.color = attribute.value;
            target.svgGradient.stops.push_back(std::move(stop));
            break;
        }
        case NativeProperty::SvgClipDefinition: target.svgFiltering.clipPath = attribute.value; break;
        case NativeProperty::SvgMaskDefinition: target.svgFiltering.mask = attribute.value; break;
        case NativeProperty::SvgFilterDefinition: target.svgFiltering.filter = attribute.value; break;
        case NativeProperty::SvgFontFamily: target.svgTypography.fontFamily = attribute.value; break;
        case NativeProperty::SvgFontSize: target.svgTypography.fontSize = attribute.value; break;
        case NativeProperty::SvgFontWeight: target.svgTypography.fontWeight = attribute.value; break;
        case NativeProperty::SvgFontStyle: target.svgTypography.fontStyle = attribute.value; break;
        case NativeProperty::SvgTextAnchor: target.svgTypography.textAnchor = attribute.value; break;
        case NativeProperty::SvgLetterSpacing: target.svgTypography.letterSpacing = attribute.value; break;
        case NativeProperty::SvgWordSpacing: target.svgTypography.wordSpacing = attribute.value; break;
        case NativeProperty::SvgAnimationAttributeName: target.svgAnimation.attributeName = attribute.value; break;
        case NativeProperty::SvgAnimationFrom: target.svgAnimation.from = attribute.value; break;
        case NativeProperty::SvgAnimationTo: target.svgAnimation.to = attribute.value; break;
        case NativeProperty::SvgAnimationBy: target.svgAnimation.by = attribute.value; break;
        case NativeProperty::SvgAnimationValues: target.svgAnimation.values = attribute.value; break;
        case NativeProperty::SvgAnimationBegin: target.svgAnimation.begin = attribute.value; break;
        case NativeProperty::SvgAnimationDuration: target.svgAnimation.duration = attribute.value; break;
        case NativeProperty::SvgAnimationRepeatCount: target.svgAnimation.repeatCount = attribute.value; break;
        case NativeProperty::SvgAnimationType: target.svgAnimation.transformType = attribute.value; break;
        case NativeProperty::SvgReference:
            target.navigation.href = attribute.value;
            break;

        case NativeProperty::MathDisplay: target.mathLayout.display = attribute.value; break;
        case NativeProperty::MathDisplayStyle: target.mathLayout.displayStyle = attribute.value; break;
        case NativeProperty::MathScriptLevel:
            target.mathLayout.scriptLevel = static_cast<int>(ToUnsigned(attribute.value, 0));
            target.mathLayout.hasScriptLevel = true;
            break;
        case NativeProperty::MathColor: target.mathStyle.color = attribute.value; break;
        case NativeProperty::MathBackground: target.mathStyle.background = attribute.value; break;
        case NativeProperty::MathSize: target.mathLayout.mathSize = attribute.value; break;
        case NativeProperty::MathVariant: target.mathStyle.variant = attribute.value; break;
        case NativeProperty::MathForm: target.mathOperator.form = attribute.value; break;
        case NativeProperty::MathFence: target.mathOperator.fence = attribute.value; break;
        case NativeProperty::MathSeparator: target.mathOperator.separator = attribute.value; break;
        case NativeProperty::MathLargeOperator: target.mathOperator.largeOperator = attribute.value; break;
        case NativeProperty::MathLeftSpace: target.mathOperator.leftSpace = attribute.value; break;
        case NativeProperty::MathRightSpace: target.mathOperator.rightSpace = attribute.value; break;
        case NativeProperty::MathStretchy: target.mathOperator.stretchy = attribute.value; break;
        case NativeProperty::MathSymmetric: target.mathOperator.symmetric = attribute.value; break;
        case NativeProperty::MathMovableLimits: target.mathOperator.movableLimits = attribute.value; break;
        case NativeProperty::MathAccent: target.mathOperator.accent = attribute.value; break;
        case NativeProperty::MathLineThickness: target.mathLayout.lineThickness = attribute.value; break;
        case NativeProperty::MathColumnAlign: target.mathLayout.columnAlign = attribute.value; break;
        case NativeProperty::MathRowAlign: target.mathLayout.rowAlign = attribute.value; break;
        case NativeProperty::MathColumnSpacing: target.mathLayout.columnSpacing = attribute.value; break;
        case NativeProperty::MathRowSpacing: target.mathLayout.rowSpacing = attribute.value; break;
        case NativeProperty::MathWidth: target.mathLayout.width = attribute.value; break;
        case NativeProperty::MathHeight: target.mathLayout.height = attribute.value; break;
        case NativeProperty::MathDepth: target.mathLayout.depth = attribute.value; break;
        case NativeProperty::MathNotation: target.mathStyle.notation = attribute.value; break;
        case NativeProperty::MathEncoding: target.mathStyle.encoding = attribute.value; break;

        case NativeProperty::CustomMetadata:
        {
            NativeAttribute entry;
            entry.property = definition.nativeProperty;
            entry.name = attribute.name;
            entry.value = attribute.value;
            target.customMetadata.entries.push_back(std::move(entry));
            break;
        }
        }
    }

    std::unique_ptr<NativeElement> NativeTreeBuilder::Build(const ElementNode& element, NativeUiModel& model)
    {
        auto native = std::make_unique<NativeElement>();

        native->kind = NormalizeKind(element);
        native->id = nextElementId++;

        native->source.tag = element.sourceTag;
        native->source.ns = element.ns;
        native->source.file = element.location.file;
        native->source.line = element.location.line;
        native->source.column = element.location.column;

        const ElementDefinition* definition = registry.FindElement(element.ns, element.sourceTag);
        if (definition != nullptr)
        {
            native->accessibility.level = definition->headingLevel;

            // The kind alone cannot describe the element: capture the rest of
            // the definition's nature here, at compile time, so the runtime
            // never has to re-derive it from a tag name.
            native->nature.inlineContent = definition->isPhrasing;
            native->nature.interactive = definition->isInteractive;
            native->nature.formAssociated = definition->isFormAssociated;
            native->nature.orderedList = definition->isOrderedList;
            native->nature.preservesWhitespace = definition->isRawText ||
                                                 native->kind == ElementKind::PreformattedText;
        }

        if (native->kind == ElementKind::CustomElement)
        {
            native->nature.inlineContent = false;
        }

        // Styling is resolved at build time into an interned StyleId.
        native->styleId = styleReferenceResolver.Resolve(element, model);

        for (const AttributeNode& attribute : element.attributes)
        {
            if (attribute.isDataAttribute || Registry::IsDataAttribute(attribute.name))
            {
                NativeAttribute entry;
                entry.property = NativeProperty::CustomMetadata;
                entry.name = attribute.name;
                entry.value = attribute.value;
                native->customMetadata.entries.push_back(std::move(entry));
                continue;
            }

            const AttributeDefinition* attributeDefinition =
                registry.FindAttribute(element.ns, attribute.name, element.sourceTag);

            if (attributeDefinition == nullptr)
            {
                // ARIA attributes are registered in the HTML namespace and apply
                // to elements in every namespace.
                attributeDefinition = registry.FindAttribute(MarkupNamespace::Html, attribute.name, element.sourceTag);
            }

            if (attributeDefinition == nullptr)
            {
                continue;
            }

            RouteAttribute(*native, *attributeDefinition, attribute);
        }

        if (native->kind == ElementKind::SvgDocument)
        {
            // An `<svg>` subtree is a picture, not a box tree: the runtime has no
            // layout for its shapes. Serialize it back to markup so it reaches the
            // frame through the image pipeline every other graphic does.
            std::string markup;
            SerializeElement(markup, element);
            native->resource.source = std::move(markup);
        }

        if (!native->content.id.empty())
        {
            model.idIndex[native->content.id] = native->id;
        }

        bool hasElementChildren = false;
        for (const std::unique_ptr<Node>& child : element.children)
        {
            if (child->kind == NodeKind::Element)
            {
                hasElementChildren = true;
                break;
            }
        }

        // Text-only content stays in one place: the element's own text field.
        //
        // As soon as the content is mixed, the position of each text run
        // relative to the child elements is part of what the document means, so
        // each run is lowered into its own Text child in document order. The
        // runtime then receives an ordered inline sequence and never has to
        // work out where a sentence was interrupted.
        if (!hasElementChildren)
        {
            const std::string directText = TrimAscii(element.DirectTextContent());
            if (!directText.empty())
            {
                native->content.text = directText;
            }
        }
        else
        {
            const bool preservesWhitespace =
                native->kind == ElementKind::PreformattedText ||
                native->kind == ElementKind::TextArea ||
                native->kind == ElementKind::ScriptResource ||
                native->kind == ElementKind::StyleSheet;

            // Whitespace between block-level siblings collapses away; it is only
            // a separator when it sits next to inline content. Resolving that here
            // keeps the runtime from carrying empty text runs through layout.
            bool previousElementWasInline = false;

            for (const std::unique_ptr<Node>& child : element.children)
            {
                if (child->kind == NodeKind::Text)
                {
                    const TextNode& textNode = *static_cast<const TextNode*>(child.get());
                    const std::string run =
                        preservesWhitespace ? textNode.text : NormalizeTextRun(textNode.text);
                    if (run.empty())
                    {
                        continue;
                    }
                    if (!preservesWhitespace && run == " " && !previousElementWasInline)
                    {
                        continue;
                    }

                    auto runElement = std::make_unique<NativeElement>();
                    runElement->kind = ElementKind::Text;
                    runElement->id = nextElementId++;
                    runElement->content.text = run;
                    // A text run is inline content even though it shares its
                    // parent's tag: `text <code>x</code> text` is one line of
                    // text, not three blocks, so the run has to flow with the
                    // inline siblings either side of it.
                    runElement->nature.inlineContent = true;
                    runElement->nature.textRun = true;
                    runElement->source.tag = element.sourceTag;
                    runElement->source.ns = element.ns;
                    runElement->source.file = textNode.location.file;
                    runElement->source.line = textNode.location.line;
                    runElement->source.column = textNode.location.column;
                    native->AddChild(std::move(runElement));
                    continue;
                }

                if (child->kind != NodeKind::Element)
                {
                    continue;
                }

                std::unique_ptr<NativeElement> nativeChild =
                    Build(*static_cast<const ElementNode*>(child.get()), model);
                previousElementWasInline = nativeChild->nature.inlineContent;
                native->AddChild(std::move(nativeChild));
            }
        }

        return native;
    }

    std::unique_ptr<NativeElement> NativeTreeBuilder::BuildDocument(const MarkupDocument& document,
                                                                   NativeUiModel& model,
                                                                   ElementId& lastAssignedElementId)
    {
        lastAssignedElementId = this->nextElementId;

        const ElementNode* rootElement = nullptr;

        if (document.root != nullptr)
        {
            if (document.root->documentElement != nullptr)
            {
                rootElement = document.root->documentElement;
            }
            else
            {
                for (const std::unique_ptr<Node>& child : document.root->children)
                {
                    if (child->kind == NodeKind::Element)
                    {
                        rootElement = static_cast<const ElementNode*>(child.get());
                        break;
                    }
                }
            }
        }

        if (rootElement == nullptr)
        {
            return nullptr;
        }

        // The native UI model contains rendered content only. <head> and the
        // metadata elements it holds are compile-time knowledge (they configure
        // the web target) and never reach the runtime tree, and the <body> frame
        // is the page itself rather than a redundant child container.
        const ElementNode* bodyElement = nullptr;
        for (const std::unique_ptr<Node>& child : rootElement->children)
        {
            if (child->kind != NodeKind::Element)
            {
                continue;
            }

            const ElementNode* elementChild = static_cast<const ElementNode*>(child.get());
            if (elementChild->sourceTag == "body")
            {
                bodyElement = elementChild;
                break;
            }
        }

        if (bodyElement != nullptr)
        {
            std::unique_ptr<NativeElement> root = Build(*bodyElement, model);
            root->kind = ElementKind::Document;
            lastAssignedElementId = this->nextElementId;
            return root;
        }

        std::unique_ptr<NativeElement> root = Build(*rootElement, model);
        lastAssignedElementId = this->nextElementId;
        return root;
    }
}
