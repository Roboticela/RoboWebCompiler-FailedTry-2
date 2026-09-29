// ---------------------------------------------------------------------------
// RoboHTMLCompiler - native UI IR.
//
// Source elements are normalised into a much smaller native representation.
// There is deliberately NOT one C++ class per HTML tag: many source elements
// collapse into one ElementKind, and every property is strongly typed so the
// hot path never touches an untyped attribute dictionary.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/GeneratedEnums.h"
#include "RoboHTMLCompiler/Registry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RoboHTMLCompiler
{
    /// Interned element identity. Zero means "no id".
    using ElementId = std::uint32_t;

    /// Interned, deduplicated style. Zero means "no styling".
    using StyleId = std::uint32_t;

    /// Interned string used for content and metadata.
    using StringId = std::uint32_t;

    /// A single typed attribute carried by a native element. Attributes that
    /// route into dedicated property structs are not duplicated here; this
    /// carries the remaining compile-time metadata and the raw value needed by
    /// the browser target.
    struct NativeAttribute
    {
        NativeProperty property = NativeProperty::Unmapped;
        std::string name;
        std::string value;
    };

    /// Application metadata. data-* attributes are intentionally kept out of
    /// the hot path but must never silently disappear.
    struct CustomMetadata
    {
        std::vector<NativeAttribute> entries;

        const NativeAttribute* Find(const std::string& name) const;
    };

    /// Geometry and box metrics.
    struct LayoutProperties
    {
        std::uint32_t width = 0;
        bool hasWidth = false;
        std::uint32_t height = 0;
        bool hasHeight = false;
        std::uint32_t rows = 0;
        bool hasRows = false;
        std::uint32_t columns = 0;
        bool hasColumns = false;
        std::uint32_t span = 1;
        std::uint32_t rowSpan = 1;
        std::uint32_t columnSpan = 1;
        std::string scope;
        std::string headers;
    };

    /// Framework-managed state. Everything here is addressable at run time.
    struct StateProperties
    {
        bool hidden = false;
        bool disabled = false;
        bool checked = false;
        bool selected = false;
        bool readOnly = false;
        bool required = false;
        bool inert = false;
        bool open = false;
        bool multiple = false;
        bool autofocus = false;
        bool draggable = false;
        bool spellCheck = false;
        bool loading = false;
    };

    /// Accessibility annotations. ARIA attributes never stay as raw strings.
    struct AccessibilityProperties
    {
        std::string role;
        std::string label;
        std::string labelledBy;
        std::string describedBy;
        std::string description;
        std::string live;
        std::string current;
        std::string orientation;
        std::string autoComplete;
        bool hidden = false;
        bool hasHidden = false;
        bool disabled = false;
        bool hasDisabled = false;
        bool expanded = false;
        int level = 0;
    };

    /// User visible content and identity.
    struct ContentProperties
    {
        std::string id;
        std::string name;
        std::string text;
        std::string value;
        std::string placeholder;
        std::string alternativeText;
        std::string language;
        std::string direction;
    };

    /// Validation constraints bound at compile time.
    struct ValidationProperties
    {
        std::string pattern;
        std::uint32_t minimumLength = 0;
        std::uint32_t maximumLength = 0;
        bool hasMinimumLength = false;
        bool hasMaximumLength = false;
        std::string accept;
        bool formNoValidate = false;
    };

    /// Numeric constraints bound at compile time.
    struct ConstraintProperties
    {
        std::string minimum;
        std::string maximum;
        std::string step;
        std::string low;
        std::string high;
        std::string optimum;
    };

    /// Link and navigation targets.
    struct NavigationProperties
    {
        std::string href;
        std::string target;
        std::string download;
        std::string relation;
        std::string language;
        std::string mimeType;
        std::string referrerPolicy;
    };

    /// Resource references.
    struct ResourceProperties
    {
        std::string source;
        std::string sourceSet;
        std::string sizes;
        std::string integrity;
        std::string crossOrigin;
        std::string fetchPriority;
        std::string media;
        std::string decoding;
        std::string destination;
        bool disabled = false;
    };

    /// Media element state.
    struct MediaProperties
    {
        std::string source;
        std::string poster;
        std::string preload;
        std::string kind;
        bool controls = false;
        bool autoPlay = false;
        bool loop = false;
        bool muted = false;
        bool playInline = false;
    };

    /// Table layout metadata.
    struct TableProperties
    {
        std::uint32_t columnSpan = 1;
        std::uint32_t rowSpan = 1;
        std::string scope;
        std::string headers;
    };

    /// Interaction hooks resolved from the behaviour module.
    struct InteractionProperties
    {
        int tabIndex = 0;
        bool hasTabIndex = false;
        std::string command;
        std::string commandTarget;
    };

    /// Debug and browser-compatibility metadata. Removable in release builds.
    struct SourceMetadata
    {
        std::string tag;
        MarkupNamespace ns = MarkupNamespace::Html;
        std::string file;
        int line = 0;
        int column = 0;
    };

    /// SVG geometry.
    struct SvgGeometry
    {
        double x = 0.0;
        double y = 0.0;
        double width = 0.0;
        double height = 0.0;
        double radius = 0.0;
        double radiusX = 0.0;
        double radiusY = 0.0;
        double centerX = 0.0;
        double centerY = 0.0;
        std::string path;
        std::string points;
        std::string viewBox;
        std::string preserveAspectRatio;
    };

    /// SVG paint.
    struct SvgPaint
    {
        std::string fill;
        std::string fillRule;
        std::string stroke;
        std::string strokeLineCap;
        std::string strokeLineJoin;
        std::string strokeDashArray;
        double fillOpacity = 1.0;
        double strokeOpacity = 1.0;
        double strokeWidth = 1.0;
        double opacity = 1.0;
    };

    /// SVG transform.
    struct SvgTransform
    {
        std::string value;
        std::string origin;
    };

    /// SVG gradient definition.
    struct SvgGradient
    {
        std::string units;
        std::string transform;
        std::string spreadMethod;
        struct Stop
        {
            std::string offset;
            std::string color;
            std::string opacity;
        };
        std::vector<Stop> stops;
    };

    /// SVG filter and masking.
    struct SvgFiltering
    {
        std::string clipPath;
        std::string mask;
        std::string filter;
        std::vector<std::string> primitives;
    };

    /// SVG presentation of text.
    struct SvgTypography
    {
        std::string fontFamily;
        std::string fontSize;
        std::string fontWeight;
        std::string fontStyle;
        std::string textAnchor;
        std::string letterSpacing;
        std::string wordSpacing;
    };

    /// SVG animation parameters.
    struct SvgAnimation
    {
        std::string attributeName;
        std::string from;
        std::string to;
        std::string by;
        std::string values;
        std::string begin;
        std::string duration;
        std::string repeatCount;
        std::string transformType;
    };

    /// MathML layout and measurement.
    struct MathLayout
    {
        std::string display;
        std::string displayStyle;
        int scriptLevel = 0;
        bool hasScriptLevel = false;
        std::string columnAlign;
        std::string rowAlign;
        std::string columnSpacing;
        std::string rowSpacing;
        std::string lineThickness;
        std::string width;
        std::string height;
        std::string depth;
        std::string mathSize;
    };

    /// MathML presentation styling.
    struct MathStyle
    {
        std::string color;
        std::string background;
        std::string variant;
        std::string form;
        std::string notation;
        std::string encoding;
    };

    /// MathML operator properties.
    struct MathOperatorProperties
    {
        std::string form;
        std::string fence;
        std::string separator;
        std::string largeOperator;
        std::string leftSpace;
        std::string rightSpace;
        std::string stretchy;
        std::string symmetric;
        std::string movableLimits;
        std::string accent;
    };

    /// A node of the native UI IR.
    class NativeElement
    {
    public:
        ElementKind kind = ElementKind::None;
        ElementId id = 0;
        StyleId styleId = 0;

        LayoutProperties layout;
        StateProperties state;
        AccessibilityProperties accessibility;
        ContentProperties content;
        ValidationProperties validation;
        ConstraintProperties constraints;
        NavigationProperties navigation;
        ResourceProperties resource;
        MediaProperties media;
        TableProperties table;
        InteractionProperties interaction;
        CustomMetadata customMetadata;
        SourceMetadata source;

        SvgGeometry svgGeometry;
        SvgPaint svgPaint;
        SvgTransform svgTransform;
        SvgGradient svgGradient;
        SvgFiltering svgFiltering;
        SvgTypography svgTypography;
        SvgAnimation svgAnimation;

        MathLayout mathLayout;
        MathStyle mathStyle;
        MathOperatorProperties mathOperator;

        /// Typed attributes that do not have a dedicated field above.
        std::vector<NativeAttribute> attributes;

        NativeElement* parent = nullptr;
        std::vector<std::unique_ptr<NativeElement>> children;

        NativeElement* AddChild(std::unique_ptr<NativeElement> child);
        void CollectDescendants(std::vector<const NativeElement*>& out) const;
        NativeElement* FindById(ElementId elementId);
    };

    /// An event binding discovered in the behaviour module.
    struct EventBinding
    {
        std::string targetId;
        std::string event;
        std::string handler;
        std::string sourceFile;
        int line = 0;
    };

    /// The complete native UI IR for one application.
    struct NativeUiModel
    {
        std::string applicationName;
        std::unique_ptr<NativeElement> root;

        /// Interned style table: index 0 is unused, so StyleId 0 means none.
        std::vector<std::string> styleTable;
        std::vector<std::string> styleDeclarations;

        std::vector<EventBinding> eventBindings;
        std::vector<std::string> scriptResources;

        /// Source id attribute -> native element identity, used to link the
        /// behaviour module to the compiled tree.
        std::unordered_map<std::string, ElementId> idIndex;

        std::size_t CountElements() const;
    };
}
