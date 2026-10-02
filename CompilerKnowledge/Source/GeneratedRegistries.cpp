// ---------------------------------------------------------------------------
// GENERATED FILE - do not edit by hand.
// Produced by the RoboHTMLCompiler RegistryCompiler target.
// ---------------------------------------------------------------------------
#include "RoboHTMLCompiler/GeneratedRegistries.h"

#include <cstddef>

namespace RoboHTMLCompiler
{
    namespace
    {
        const char* const KindNames[] = {
            "None",
            "Document",
            "Metadata",
            "DocumentTitle",
            "BaseUrl",
            "ResourceLink",
            "StyleSheet",
            "ScriptResource",
            "NoScript",
            "Container",
            "Text",
            "PreformattedText",
            "Quote",
            "Heading",
            "LineBreak",
            "WordBreakOpportunity",
            "Separator",
            "Link",
            "ImageMapLink",
            "ImageMap",
            "List",
            "ListItem",
            "DescriptionList",
            "DescriptionTerm",
            "DescriptionDescription",
            "Figure",
            "FigureCaption",
            "Image",
            "ResponsiveImage",
            "MediaSource",
            "Audio",
            "Video",
            "MediaTrack",
            "Canvas",
            "Form",
            "FieldSet",
            "Legend",
            "Label",
            "Button",
            "Input",
            "NumberInput",
            "DateInput",
            "DateTimeInput",
            "MonthInput",
            "WeekInput",
            "TimeInput",
            "FileInput",
            "Checkbox",
            "Radio",
            "Range",
            "ColorInput",
            "SearchInput",
            "UrlInput",
            "TelephoneInput",
            "HiddenInput",
            "SubmitButton",
            "ResetButton",
            "ImageButton",
            "TextArea",
            "Select",
            "Option",
            "OptionGroup",
            "DataList",
            "SelectedContent",
            "Output",
            "Progress",
            "Meter",
            "Table",
            "TableCaption",
            "TableColumnGroup",
            "TableColumn",
            "TableSection",
            "TableRow",
            "TableCell",
            "TableHeaderCell",
            "Disclosure",
            "DisclosureSummary",
            "Dialog",
            "Frame",
            "EmbeddedContent",
            "EmbeddedObject",
            "EmbeddedParameter",
            "Template",
            "Slot",
            "Ruby",
            "RubyText",
            "RubyParenthesis",
            "ObsoleteElement",
            "CustomElement",
            "SvgDocument",
            "SvgContainer",
            "SvgShape",
            "SvgPath",
            "SvgText",
            "SvgImage",
            "SvgUse",
            "SvgGradient",
            "SvgStop",
            "SvgPattern",
            "SvgMarker",
            "SvgClipPath",
            "SvgMask",
            "SvgFilter",
            "SvgFilterPrimitive",
            "SvgAnimation",
            "SvgForeignObject",
            "SvgMetadata",
            "MathRoot",
            "MathToken",
            "MathOperator",
            "MathRow",
            "MathFraction",
            "MathScript",
            "MathMultiScripts",
            "MathPrescript",
            "MathRadical",
            "MathTable",
            "MathTableRow",
            "MathTableCell",
            "MathSpace",
            "MathPadded",
            "MathStyle",
            "MathAction",
            "MathError",
            "MathPhantom",
            "MathSemantics",
            "MathAnnotation",
        };

        const char* const PropertyNames[] = {
            "Unmapped",
            "ForbiddenInlineStyle",
            "IdentityId",
            "IdentityName",
            "StyleReference",
            "StyleSheetReference",
            "StyleLanguage",
            "SlotAssignment",
            "StateHidden",
            "StateDisabled",
            "StateChecked",
            "StateIndeterminate",
            "StateSelected",
            "StateReadOnly",
            "StateRequired",
            "StateInert",
            "StateOpen",
            "StateMultiple",
            "StateAutofocus",
            "StateDraggable",
            "StateSpellCheck",
            "StateLoading",
            "ValidationPattern",
            "ValidationMinimumLength",
            "ValidationMaximumLength",
            "ValidationAccept",
            "ValidationFormNoValidate",
            "ValueValue",
            "ValueDefaultValue",
            "ValueTextContent",
            "InputType",
            "InputPlaceholder",
            "InputAutocomplete",
            "InputDirectoryName",
            "InputOptions",
            "InputSize",
            "InputWrap",
            "ConstraintMinimum",
            "ConstraintMaximum",
            "ConstraintStep",
            "ConstraintLow",
            "ConstraintHigh",
            "ConstraintOptimum",
            "LayoutWidth",
            "LayoutHeight",
            "LayoutSize",
            "LayoutRows",
            "LayoutColumns",
            "LayoutSpan",
            "LayoutRowSpan",
            "LayoutColumnSpan",
            "LayoutScope",
            "LayoutHeaders",
            "TextLanguage",
            "TextDirection",
            "TextTranslate",
            "MetadataTitle",
            "MetadataAccessKey",
            "MetadataNonce",
            "MetadataPopover",
            "MetadataEditable",
            "MetadataInputMode",
            "MetadataEnterKeyHint",
            "MetadataAutoCapitalize",
            "MetadataAutoCorrect",
            "MetadataWriteSuggestions",
            "MetadataContent",
            "HeadingOffset",
            "HeadingReset",
            "InteractionTabIndex",
            "InteractionCommand",
            "InteractionCommandTarget",
            "InteractionCommandFor",
            "NavigationHref",
            "NavigationTarget",
            "NavigationDownload",
            "NavigationRelation",
            "NavigationLanguage",
            "NavigationMimeType",
            "NavigationPing",
            "NavigationReferrerPolicy",
            "ResourceSource",
            "ResourceSourceSet",
            "ResourceSizes",
            "ResourceCandidates",
            "ResourceImages",
            "ResourceDestination",
            "ResourceIntegrity",
            "ResourceCrossOrigin",
            "ResourceFetchPriority",
            "ResourceMedia",
            "ResourceBlocking",
            "ResourceDecoding",
            "ResourceLoadMode",
            "ResourceDisabled",
            "ResourceColor",
            "ResourceMimeType",
            "ImageAlternativeText",
            "ImageMapName",
            "ImageMapServerSide",
            "ImageMapShape",
            "ImageMapCoordinates",
            "MediaControls",
            "MediaAutoPlay",
            "MediaLoop",
            "MediaMuted",
            "MediaPoster",
            "MediaPreload",
            "MediaPlayInline",
            "MediaKind",
            "MediaSourceLanguage",
            "MediaSourceDefault",
            "EmbeddedData",
            "EmbeddedMimeType",
            "EmbeddedWidth",
            "EmbeddedHeight",
            "DocumentBaseTarget",
            "DocumentCharacterSet",
            "DocumentDescription",
            "DocumentRefresh",
            "DocumentHttpEquiv",
            "DocumentCompatibility",
            "FormAction",
            "FormEnctype",
            "FormMethod",
            "FormTarget",
            "FormOwner",
            "FormNoValidate",
            "LabelFor",
            "OptionLabel",
            "ListReversed",
            "ListStart",
            "ListStyle",
            "ListType",
            "QuoteCitation",
            "DateValue",
            "DialogCloseBehavior",
            "ButtonType",
            "ScriptAsynchronous",
            "ScriptDeferred",
            "ScriptNoModule",
            "FramePermissions",
            "FrameAllowFullScreen",
            "FrameSandbox",
            "FrameSourceDocument",
            "ColorAlpha",
            "ColorSpace",
            "MicroDataItemId",
            "MicroDataItemProp",
            "MicroDataItemRef",
            "MicroDataItemScope",
            "MicroDataItemType",
            "CustomizedBuiltIn",
            "ShadowRootMode",
            "ShadowRootDelegatesFocus",
            "ShadowRootSlotAssignment",
            "ShadowRootClonable",
            "ShadowRootSerializable",
            "ShadowRootCustomElementRegistry",
            "AccessibilityRole",
            "AccessibilityLabel",
            "AccessibilityLabelledBy",
            "AccessibilityDescribedBy",
            "AccessibilityDescription",
            "AccessibilityDetails",
            "AccessibilityHidden",
            "AccessibilityDisabled",
            "AccessibilityLive",
            "AccessibilityAtomic",
            "AccessibilityRelevant",
            "AccessibilityBusy",
            "AccessibilityCurrent",
            "AccessibilityChecked",
            "AccessibilityExpanded",
            "AccessibilityPressed",
            "AccessibilitySelected",
            "AccessibilityLevel",
            "AccessibilityOrientation",
            "AccessibilityValueMinimum",
            "AccessibilityValueMaximum",
            "AccessibilityValueNow",
            "AccessibilityValueText",
            "AccessibilityPlaceholder",
            "AccessibilityRequired",
            "AccessibilityReadOnly",
            "AccessibilityMultiLine",
            "AccessibilityMultiSelectable",
            "AccessibilityModal",
            "AccessibilitySort",
            "AccessibilityAutoComplete",
            "AccessibilityErrorMessage",
            "AccessibilityHasPopup",
            "AccessibilityInvalid",
            "AccessibilityKeyShortcuts",
            "AccessibilityRoleDescription",
            "AccessibilityControls",
            "AccessibilityOwns",
            "AccessibilityFlowTo",
            "AccessibilityActiveDescendant",
            "AccessibilitySetSize",
            "AccessibilityPositionInSet",
            "AccessibilityRowCount",
            "AccessibilityRowIndex",
            "AccessibilityRowIndexText",
            "AccessibilityColumnIndexText",
            "AccessibilityRowSpan",
            "AccessibilityColumnCount",
            "AccessibilityColumnIndex",
            "AccessibilityColumnSpan",
            "SvgGeometryX",
            "SvgGeometryY",
            "SvgGeometryX1",
            "SvgGeometryY1",
            "SvgGeometryX2",
            "SvgGeometryY2",
            "SvgGeometryCenterX",
            "SvgGeometryCenterY",
            "SvgGeometryRadius",
            "SvgGeometryRadiusX",
            "SvgGeometryRadiusY",
            "SvgGeometryWidth",
            "SvgGeometryHeight",
            "SvgGeometryPath",
            "SvgGeometryPoints",
            "SvgGeometryOffset",
            "SvgGeometryPathLength",
            "SvgViewBox",
            "SvgViewport",
            "SvgPreserveAspectRatio",
            "SvgPaintFill",
            "SvgPaintFillOpacity",
            "SvgPaintFillRule",
            "SvgPaintStroke",
            "SvgPaintStrokeWidth",
            "SvgPaintStrokeOpacity",
            "SvgPaintStrokeLineCap",
            "SvgPaintStrokeLineJoin",
            "SvgPaintStrokeDashArray",
            "SvgPaintStrokeDashOffset",
            "SvgPaintStrokeMiterLimit",
            "SvgPaintColor",
            "SvgPaintOpacity",
            "SvgTransform",
            "SvgTransformOrigin",
            "SvgGradientUnits",
            "SvgGradientTransform",
            "SvgGradientSpreadMethod",
            "SvgGradientOffset",
            "SvgStopColor",
            "SvgStopOpacity",
            "SvgMarkerStart",
            "SvgMarkerMiddle",
            "SvgMarkerEnd",
            "SvgMarkerWidth",
            "SvgMarkerHeight",
            "SvgMarkerUnits",
            "SvgMarkerOrientation",
            "SvgClipDefinition",
            "SvgMaskDefinition",
            "SvgFilterDefinition",
            "SvgFilterPrimitives",
            "SvgFilterUnits",
            "SvgFilterPrimitiveInput",
            "SvgFilterPrimitiveResult",
            "SvgVisibility",
            "SvgDisplay",
            "SvgOverflow",
            "SvgPointerEvents",
            "SvgTextAnchor",
            "SvgTextLength",
            "SvgFontFamily",
            "SvgFontSize",
            "SvgFontWeight",
            "SvgFontStyle",
            "SvgLetterSpacing",
            "SvgWordSpacing",
            "SvgDominantBaseline",
            "SvgBaselineShift",
            "SvgGlyphOrientation",
            "SvgAnimationAttributeName",
            "SvgAnimationAttributeType",
            "SvgAnimationFrom",
            "SvgAnimationTo",
            "SvgAnimationBy",
            "SvgAnimationValues",
            "SvgAnimationBegin",
            "SvgAnimationDuration",
            "SvgAnimationRepeatCount",
            "SvgAnimationFillMode",
            "SvgAnimationPath",
            "SvgAnimationRotation",
            "SvgAnimationKeyTimes",
            "SvgAnimationKeySplines",
            "SvgAnimationType",
            "SvgAnimationAdditive",
            "SvgAnimationAccumulate",
            "SvgAnimationRestart",
            "SvgAnimationCalcMode",
            "SvgAnimationOrigin",
            "SvgAnimationMotion",
            "SvgReference",
            "SvgSymbolViewBox",
            "SvgExternalResourcesRequired",
            "SvgRequiredFeatures",
            "SvgRequiredExtensions",
            "SvgSystemLanguage",
            "SvgCondition",
            "SvgSwitch",
            "SvgShapeRendering",
            "SvgTextRendering",
            "SvgImageRendering",
            "SvgColorInterpolation",
            "SvgColorInterpolationFilters",
            "SvgColorProfile",
            "SvgColorRendering",
            "SvgEnableBackground",
            "SvgCursor",
            "SvgMaskUnits",
            "SvgMaskContentUnits",
            "SvgClipRule",
            "SvgClipPathUnits",
            "SvgPatternUnits",
            "SvgPatternContentUnits",
            "SvgPatternTransform",
            "SvgSpreadMethod",
            "SvgStartOffset",
            "SvgTextPathSide",
            "SvgMediaRole",
            "SvgBaseProfile",
            "SvgContentScriptType",
            "SvgContentStyleType",
            "SvgZoomAndPan",
            "SvgVersion",
            "SvgNamespace",
            "MathDisplay",
            "MathColor",
            "MathBackground",
            "MathSize",
            "MathDisplayStyle",
            "MathScriptLevel",
            "MathVariant",
            "MathLineThickness",
            "MathAccent",
            "MathAccentUnder",
            "MathStretchy",
            "MathSymmetric",
            "MathMovableLimits",
            "MathForm",
            "MathFence",
            "MathSeparator",
            "MathLargeOperator",
            "MathLeftSpace",
            "MathRightSpace",
            "MathColumnAlign",
            "MathRowAlign",
            "MathColumnSpacing",
            "MathRowSpacing",
            "MathColumnLines",
            "MathRowLines",
            "MathColumnSpan",
            "MathRowSpan",
            "MathNotation",
            "MathEncoding",
            "MathDefinitionUrl",
            "MathHref",
            "MathActionType",
            "MathActionSelection",
            "MathAlignment",
            "MathDenomalign",
            "MathNumalign",
            "MathWidth",
            "MathHeight",
            "MathDepth",
            "MathLspace",
            "MathRspace",
            "MathStackAlign",
            "MathTableFrame",
            "MathTableFrameSpacing",
            "MathTableEqualRows",
            "MathTableEqualColumns",
            "MathTableColumnWidth",
            "MathBevelled",
            "MathScriptAlign",
            "MathGroupAlign",
            "MathRowSpacingStyle",
            "MathOpen",
            "MathClose",
            "MathMinSize",
            "MathMaxSize",
            "MathAltText",
            "MathDirection",
            "MathMathVariant",
            "MathCharAlign",
            "MathCharSpacing",
            "MathVAlign",
            "CustomMetadata",
            "MetadataPart",
            "MetadataExportParts",
            "MetadataAnchor",
            "MetadataElementTiming",
            "MetadataVirtualKeyboardPolicy",
            "InputCapture",
            "MetadataAutoLocate",
            "MetadataWatch",
        };
    }

    const char* ToString(ElementKind kind)
    {
        const auto index = static_cast<std::size_t>(kind);
        return index < sizeof(KindNames) / sizeof(KindNames[0]) ? KindNames[index] : "Unknown";
    }

    const char* ToString(NativeProperty property)
    {
        const auto index = static_cast<std::size_t>(property);
        return index < sizeof(PropertyNames) / sizeof(PropertyNames[0]) ? PropertyNames[index]
                                                                          : "Unknown";
    }
}

namespace RoboHTMLCompiler::Generated
{
    namespace
    {
        const char* const ElementAttributes3[] = {
            "href",
            "target",
        };
        const char* const ElementAttributes4[] = {
            "href",
            "crossorigin",
            "rel",
            "media",
            "integrity",
            "hreflang",
            "type",
            "referrerpolicy",
            "sizes",
            "imagesrcset",
            "imagesizes",
            "as",
            "blocking",
            "color",
            "disabled",
            "fetchpriority",
            "title",
        };
        const char* const ElementAttributes5[] = {
            "name",
            "http-equiv",
            "content",
            "charset",
            "media",
        };
        const char* const ElementAttributes6[] = {
            "media",
            "blocking",
            "title",
        };
        const char* const ElementAttributes25[] = {
            "cite",
        };
        const char* const ElementAttributes26[] = {
            "reversed",
            "start",
            "type",
        };
        const char* const ElementAttributes29[] = {
            "value",
        };
        const char* const ElementAttributes38[] = {
            "href",
            "target",
            "download",
            "ping",
            "rel",
            "hreflang",
            "type",
            "referrerpolicy",
        };
        const char* const ElementAttributes44[] = {
            "cite",
        };
        const char* const ElementAttributes45[] = {
            "title",
        };
        const char* const ElementAttributes46[] = {
            "title",
        };
        const char* const ElementAttributes50[] = {
            "value",
        };
        const char* const ElementAttributes51[] = {
            "datetime",
        };
        const char* const ElementAttributes62[] = {
            "dir",
        };
        const char* const ElementAttributes63[] = {
            "dir",
        };
        const char* const ElementAttributes67[] = {
            "cite",
            "datetime",
        };
        const char* const ElementAttributes68[] = {
            "cite",
            "datetime",
        };
        const char* const ElementAttributes70[] = {
            "type",
            "media",
            "src",
            "srcset",
            "sizes",
            "width",
            "height",
        };
        const char* const ElementAttributes71[] = {
            "alt",
            "src",
            "srcset",
            "sizes",
            "crossorigin",
            "usemap",
            "ismap",
            "controls",
            "width",
            "height",
            "referrerpolicy",
            "decoding",
            "loading",
            "fetchpriority",
        };
        const char* const ElementAttributes72[] = {
            "src",
            "srcdoc",
            "name",
            "sandbox",
            "allow",
            "allowfullscreen",
            "width",
            "height",
            "referrerpolicy",
            "loading",
        };
        const char* const ElementAttributes73[] = {
            "src",
            "type",
            "width",
            "height",
        };
        const char* const ElementAttributes74[] = {
            "data",
            "type",
            "name",
            "form",
            "width",
            "height",
        };
        const char* const ElementAttributes75[] = {
            "src",
            "crossorigin",
            "poster",
            "preload",
            "autoplay",
            "playsinline",
            "loop",
            "muted",
            "controls",
            "loading",
            "width",
            "height",
        };
        const char* const ElementAttributes76[] = {
            "src",
            "crossorigin",
            "preload",
            "autoplay",
            "loop",
            "muted",
            "controls",
            "loading",
        };
        const char* const ElementAttributes77[] = {
            "kind",
            "src",
            "srclang",
            "label",
            "default",
        };
        const char* const ElementAttributes78[] = {
            "name",
        };
        const char* const ElementAttributes79[] = {
            "alt",
            "coords",
            "shape",
            "href",
            "target",
            "download",
            "ping",
            "rel",
            "referrerpolicy",
            "hreflang",
            "type",
        };
        const char* const ElementAttributes82[] = {
            "span",
        };
        const char* const ElementAttributes83[] = {
            "span",
        };
        const char* const ElementAttributes88[] = {
            "colspan",
            "rowspan",
            "headers",
        };
        const char* const ElementAttributes89[] = {
            "colspan",
            "rowspan",
            "headers",
            "scope",
            "abbr",
        };
        const char* const ElementAttributes90[] = {
            "accept-charset",
            "action",
            "autocomplete",
            "enctype",
            "method",
            "name",
            "novalidate",
            "target",
            "rel",
        };
        const char* const ElementAttributes91[] = {
            "for",
        };
        const char* const ElementAttributes92[] = {
            "accept",
            "alpha",
            "alt",
            "autocomplete",
            "checked",
            "colorspace",
            "dirname",
            "disabled",
            "form",
            "formaction",
            "formenctype",
            "formmethod",
            "formnovalidate",
            "formtarget",
            "height",
            "list",
            "max",
            "maxlength",
            "min",
            "minlength",
            "multiple",
            "name",
            "pattern",
            "placeholder",
            "popovertarget",
            "popovertargetaction",
            "readonly",
            "required",
            "size",
            "src",
            "step",
            "type",
            "value",
            "width",
            "title",
        };
        const char* const ElementAttributes93[] = {
            "command",
            "commandfor",
            "disabled",
            "form",
            "formaction",
            "formenctype",
            "formmethod",
            "formnovalidate",
            "formtarget",
            "name",
            "popovertarget",
            "popovertargetaction",
            "type",
            "value",
        };
        const char* const ElementAttributes94[] = {
            "autocomplete",
            "disabled",
            "form",
            "multiple",
            "name",
            "required",
            "size",
        };
        const char* const ElementAttributes96[] = {
            "disabled",
            "label",
        };
        const char* const ElementAttributes97[] = {
            "disabled",
            "label",
            "selected",
            "value",
        };
        const char* const ElementAttributes98[] = {
            "autocomplete",
            "cols",
            "dirname",
            "disabled",
            "form",
            "maxlength",
            "minlength",
            "name",
            "placeholder",
            "readonly",
            "required",
            "rows",
            "wrap",
        };
        const char* const ElementAttributes99[] = {
            "for",
            "form",
            "name",
        };
        const char* const ElementAttributes100[] = {
            "value",
            "max",
        };
        const char* const ElementAttributes101[] = {
            "value",
            "min",
            "max",
            "low",
            "high",
            "optimum",
        };
        const char* const ElementAttributes102[] = {
            "disabled",
            "form",
            "name",
        };
        const char* const ElementAttributes105[] = {
            "name",
            "open",
        };
        const char* const ElementAttributes107[] = {
            "closedby",
            "open",
        };
        const char* const ElementAttributes108[] = {
            "type",
            "src",
            "nomodule",
            "async",
            "defer",
            "blocking",
            "crossorigin",
            "referrerpolicy",
            "integrity",
            "fetchpriority",
        };
        const char* const ElementAttributes110[] = {
            "for",
            "shadowrootmode",
            "shadowrootdelegatesfocus",
            "shadowrootserializable",
            "shadowrootslotassignment",
            "shadowrootclonable",
            "shadowrootcustomelementregistry",
        };
        const char* const ElementAttributes111[] = {
            "name",
        };
        const char* const ElementAttributes112[] = {
            "width",
            "height",
        };
        const char* const ElementAttributes142[] = {
            "allow",
            "height",
            "width",
        };
        const char* const ElementAttributes143[] = {
            "autolocate",
            "watch",
        };
        const char* const AttributeElements107[] = {
            "th",
        };
        const char* const AttributeElements108[] = {
            "input",
        };
        const char* const AttributeElements109[] = {
            "form",
        };
        const char* const AttributeElements110[] = {
            "form",
        };
        const char* const AttributeElements111[] = {
            "fencedframe",
            "iframe",
        };
        const char* const AttributeElements112[] = {
            "iframe",
        };
        const char* const AttributeElements113[] = {
            "input",
        };
        const char* const AttributeElements114[] = {
            "area",
            "img",
            "input",
        };
        const char* const AttributeElements115[] = {
            "link",
        };
        const char* const AttributeElements116[] = {
            "script",
        };
        const char* const AttributeElements117[] = {
            "form",
            "input",
            "select",
            "textarea",
        };
        const char* const AttributeElements118[] = {
            "geolocation",
        };
        const char* const AttributeElements119[] = {
            "audio",
            "video",
        };
        const char* const AttributeElements120[] = {
            "link",
            "script",
            "style",
        };
        const char* const AttributeElements121[] = {
            "input",
        };
        const char* const AttributeElements122[] = {
            "meta",
        };
        const char* const AttributeElements123[] = {
            "input",
        };
        const char* const AttributeElements124[] = {
            "blockquote",
            "del",
            "ins",
            "q",
        };
        const char* const AttributeElements125[] = {
            "dialog",
        };
        const char* const AttributeElements126[] = {
            "link",
        };
        const char* const AttributeElements127[] = {
            "input",
        };
        const char* const AttributeElements128[] = {
            "textarea",
        };
        const char* const AttributeElements129[] = {
            "td",
            "th",
        };
        const char* const AttributeElements130[] = {
            "button",
        };
        const char* const AttributeElements131[] = {
            "button",
        };
        const char* const AttributeElements132[] = {
            "meta",
        };
        const char* const AttributeElements133[] = {
            "audio",
            "img",
            "video",
        };
        const char* const AttributeElements134[] = {
            "area",
        };
        const char* const AttributeElements135[] = {
            "audio",
            "img",
            "link",
            "script",
            "video",
        };
        const char* const AttributeElements136[] = {
            "object",
        };
        const char* const AttributeElements137[] = {
            "del",
            "ins",
            "time",
        };
        const char* const AttributeElements138[] = {
            "img",
        };
        const char* const AttributeElements139[] = {
            "track",
        };
        const char* const AttributeElements140[] = {
            "script",
        };
        const char* const AttributeElements141[] = {
            "bdi",
            "bdo",
        };
        const char* const AttributeElements142[] = {
            "input",
            "textarea",
        };
        const char* const AttributeElements143[] = {
            "button",
            "fieldset",
            "input",
            "link",
            "optgroup",
            "option",
            "select",
            "textarea",
        };
        const char* const AttributeElements144[] = {
            "a",
            "area",
        };
        const char* const AttributeElements145[] = {
            "form",
        };
        const char* const AttributeElements146[] = {
            "img",
            "link",
            "script",
        };
        const char* const AttributeElements147[] = {
            "label",
            "output",
            "template",
        };
        const char* const AttributeElements148[] = {
            "button",
            "fieldset",
            "input",
            "object",
            "output",
            "select",
            "textarea",
        };
        const char* const AttributeElements149[] = {
            "button",
            "input",
        };
        const char* const AttributeElements150[] = {
            "button",
            "input",
        };
        const char* const AttributeElements151[] = {
            "button",
            "input",
        };
        const char* const AttributeElements152[] = {
            "button",
            "input",
        };
        const char* const AttributeElements153[] = {
            "button",
            "input",
        };
        const char* const AttributeElements154[] = {
            "td",
            "th",
        };
        const char* const AttributeElements155[] = {
            "canvas",
            "embed",
            "iframe",
        };
        const char* const AttributeElements156[] = {
            "fencedframe",
        };
        const char* const AttributeElements157[] = {
            "canvas",
            "embed",
            "fencedframe",
            "iframe",
            "img",
            "input",
            "object",
            "source",
            "video",
        };
        const char* const AttributeElements158[] = {
            "meter",
        };
        const char* const AttributeElements159[] = {
            "link",
        };
        const char* const AttributeElements160[] = {
            "a",
            "area",
            "base",
            "link",
        };
        const char* const AttributeElements161[] = {
            "a",
            "area",
            "link",
        };
        const char* const AttributeElements162[] = {
            "meta",
        };
        const char* const AttributeElements163[] = {
            "link",
        };
        const char* const AttributeElements164[] = {
            "link",
        };
        const char* const AttributeElements165[] = {
            "link",
            "script",
        };
        const char* const AttributeElements166[] = {
            "img",
        };
        const char* const AttributeElements167[] = {
            "track",
        };
        const char* const AttributeElements168[] = {
            "optgroup",
            "option",
            "track",
        };
        const char* const AttributeElements169[] = {
            "input",
        };
        const char* const AttributeElements170[] = {
            "audio",
            "iframe",
            "img",
            "video",
        };
        const char* const AttributeElements171[] = {
            "audio",
            "video",
        };
        const char* const AttributeElements172[] = {
            "meter",
        };
        const char* const AttributeElements173[] = {
            "input",
            "meter",
            "progress",
        };
        const char* const AttributeElements174[] = {
            "input",
            "textarea",
        };
        const char* const AttributeElements175[] = {
            "link",
            "meta",
            "source",
            "style",
        };
        const char* const AttributeElements176[] = {
            "form",
        };
        const char* const AttributeElements177[] = {
            "input",
            "meter",
        };
        const char* const AttributeElements178[] = {
            "input",
            "textarea",
        };
        const char* const AttributeElements179[] = {
            "input",
            "select",
        };
        const char* const AttributeElements180[] = {
            "audio",
            "video",
        };
        const char* const AttributeElements181[] = {
            "button",
            "details",
            "fieldset",
            "form",
            "iframe",
            "input",
            "map",
            "meta",
            "object",
            "output",
            "select",
            "slot",
            "textarea",
        };
        const char* const AttributeElements182[] = {
            "script",
        };
        const char* const AttributeElements183[] = {
            "form",
        };
        const char* const AttributeElements184[] = {
            "details",
            "dialog",
        };
        const char* const AttributeElements185[] = {
            "meter",
        };
        const char* const AttributeElements186[] = {
            "input",
        };
        const char* const AttributeElements187[] = {
            "a",
            "area",
        };
        const char* const AttributeElements188[] = {
            "input",
            "textarea",
        };
        const char* const AttributeElements189[] = {
            "video",
        };
        const char* const AttributeElements190[] = {
            "button",
            "input",
        };
        const char* const AttributeElements191[] = {
            "button",
            "input",
        };
        const char* const AttributeElements192[] = {
            "video",
        };
        const char* const AttributeElements193[] = {
            "audio",
            "video",
        };
        const char* const AttributeElements194[] = {
            "input",
            "textarea",
        };
        const char* const AttributeElements195[] = {
            "a",
            "area",
            "iframe",
            "img",
            "link",
            "script",
        };
        const char* const AttributeElements196[] = {
            "a",
            "area",
            "form",
            "link",
        };
        const char* const AttributeElements197[] = {
            "input",
            "select",
            "textarea",
        };
        const char* const AttributeElements198[] = {
            "ol",
        };
        const char* const AttributeElements199[] = {
            "textarea",
        };
        const char* const AttributeElements200[] = {
            "td",
            "th",
        };
        const char* const AttributeElements201[] = {
            "iframe",
        };
        const char* const AttributeElements202[] = {
            "th",
        };
        const char* const AttributeElements203[] = {
            "option",
        };
        const char* const AttributeElements204[] = {
            "template",
        };
        const char* const AttributeElements205[] = {
            "template",
        };
        const char* const AttributeElements206[] = {
            "template",
        };
        const char* const AttributeElements207[] = {
            "template",
        };
        const char* const AttributeElements208[] = {
            "template",
        };
        const char* const AttributeElements209[] = {
            "template",
        };
        const char* const AttributeElements210[] = {
            "area",
        };
        const char* const AttributeElements211[] = {
            "input",
            "select",
        };
        const char* const AttributeElements212[] = {
            "img",
            "link",
            "source",
        };
        const char* const AttributeElements213[] = {
            "col",
            "colgroup",
        };
        const char* const AttributeElements214[] = {
            "audio",
            "embed",
            "iframe",
            "img",
            "input",
            "script",
            "source",
            "track",
            "video",
        };
        const char* const AttributeElements215[] = {
            "iframe",
        };
        const char* const AttributeElements216[] = {
            "track",
        };
        const char* const AttributeElements217[] = {
            "img",
            "source",
        };
        const char* const AttributeElements218[] = {
            "ol",
        };
        const char* const AttributeElements219[] = {
            "input",
        };
        const char* const AttributeElements220[] = {
            "a",
            "area",
            "base",
            "form",
        };
        const char* const AttributeElements221[] = {
            "abbr",
            "dfn",
            "input",
            "link",
            "style",
        };
        const char* const AttributeElements222[] = {
            "input",
        };
        const char* const AttributeElements223[] = {
            "button",
        };
        const char* const AttributeElements224[] = {
            "ol",
        };
        const char* const AttributeElements225[] = {
            "a",
            "area",
            "button",
            "embed",
            "input",
            "link",
            "object",
            "ol",
            "script",
            "source",
        };
        const char* const AttributeElements226[] = {
            "img",
        };
        const char* const AttributeElements227[] = {
            "button",
            "data",
            "input",
            "li",
            "meter",
            "option",
            "progress",
        };
        const char* const AttributeElements228[] = {
            "geolocation",
        };
        const char* const AttributeElements229[] = {
            "canvas",
            "embed",
            "iframe",
        };
        const char* const AttributeElements230[] = {
            "fencedframe",
        };
        const char* const AttributeElements231[] = {
            "canvas",
            "embed",
            "fencedframe",
            "iframe",
            "img",
            "input",
            "object",
            "source",
            "video",
        };
        const char* const AttributeElements232[] = {
            "textarea",
        };
        const char* const AttributeElements319[] = {
            "line",
            "linearGradient",
        };
        const char* const AttributeElements320[] = {
            "line",
            "linearGradient",
        };
        const char* const AttributeElements321[] = {
            "line",
            "linearGradient",
        };
        const char* const AttributeElements322[] = {
            "line",
            "linearGradient",
        };
        const char* const AttributeElements323[] = {
            "circle",
            "ellipse",
            "radialGradient",
        };
        const char* const AttributeElements324[] = {
            "circle",
            "ellipse",
            "radialGradient",
        };
        const char* const AttributeElements325[] = {
            "circle",
            "radialGradient",
        };
        const char* const AttributeElements326[] = {
            "ellipse",
            "rect",
        };
        const char* const AttributeElements327[] = {
            "ellipse",
            "rect",
        };
        const char* const AttributeElements328[] = {
            "radialGradient",
        };
        const char* const AttributeElements329[] = {
            "radialGradient",
        };
        const char* const AttributeElements330[] = {
            "radialGradient",
        };
        const char* const AttributeElements333[] = {
            "path",
        };
        const char* const AttributeElements334[] = {
            "polyline",
            "polygon",
        };
        const char* const AttributeElements335[] = {
            "stop",
            "feFuncA",
            "feFuncB",
            "feFuncG",
            "feFuncR",
        };
        const char* const AttributeElements337[] = {
            "svg",
            "symbol",
            "marker",
            "pattern",
            "view",
        };
        const char* const AttributeElements338[] = {
            "svg",
            "symbol",
            "image",
            "marker",
            "pattern",
            "view",
        };
        const char* const AttributeElements354[] = {
            "linearGradient",
            "radialGradient",
        };
        const char* const AttributeElements355[] = {
            "linearGradient",
            "radialGradient",
        };
        const char* const AttributeElements356[] = {
            "linearGradient",
            "radialGradient",
        };
        const char* const AttributeElements357[] = {
            "stop",
        };
        const char* const AttributeElements358[] = {
            "stop",
        };
        const char* const AttributeElements359[] = {
            "pattern",
        };
        const char* const AttributeElements360[] = {
            "pattern",
        };
        const char* const AttributeElements361[] = {
            "pattern",
        };
        const char* const AttributeElements370[] = {
            "text",
            "tspan",
            "textPath",
        };
        const char* const AttributeElements371[] = {
            "text",
            "tspan",
            "textPath",
        };
        const char* const AttributeElements375[] = {
            "textPath",
        };
        const char* const AttributeElements386[] = {
            "clipPath",
        };
        const char* const AttributeElements388[] = {
            "mask",
        };
        const char* const AttributeElements389[] = {
            "mask",
        };
        const char* const AttributeElements391[] = {
            "filter",
        };
        const char* const AttributeElements392[] = {
            "filter",
        };
        const char* const AttributeElements396[] = {
            "feGaussianBlur",
            "feDropShadow",
        };
        const char* const AttributeElements397[] = {
            "feBlend",
            "feMerge",
        };
        const char* const AttributeElements398[] = {
            "feComposite",
            "feMorphology",
        };
        const char* const AttributeElements399[] = {
            "feMorphology",
        };
        const char* const AttributeElements400[] = {
            "feDisplacementMap",
        };
        const char* const AttributeElements401[] = {
            "feConvolveMatrix",
        };
        const char* const AttributeElements402[] = {
            "feColorMatrix",
        };
        const char* const AttributeElements403[] = {
            "feColorMatrix",
            "feTurbulence",
            "feComponentTransfer",
            "animateTransform",
            "animate",
        };
        const char* const AttributeElements407[] = {
            "marker",
        };
        const char* const AttributeElements408[] = {
            "marker",
        };
        const char* const AttributeElements409[] = {
            "marker",
        };
        const char* const AttributeElements410[] = {
            "marker",
        };
        const char* const AttributeElements411[] = {
            "marker",
        };
        const char* const AttributeElements412[] = {
            "marker",
        };
        const char* const AttributeElements423[] = {
            "animate",
            "animateMotion",
            "animateTransform",
            "set",
        };
        const char* const AttributeElements424[] = {
            "animateMotion",
        };
        const char* const AttributeElements432[] = {
            "animateMotion",
        };
        const char* const AttributeElements442[] = {
            "a",
        };
        const char* const AttributeElements443[] = {
            "a",
        };
        const char* const AttributeElements444[] = {
            "a",
        };
        const char* const AttributeElements461[] = {
            "math",
        };
        const char* const AttributeElements466[] = {
            "math",
            "mstyle",
            "mfrac",
            "msqrt",
            "mroot",
            "msub",
            "msup",
            "msubsup",
            "munder",
            "mover",
            "munderover",
            "mmultiscripts",
            "mtable",
            "mtr",
            "mtd",
        };
        const char* const AttributeElements468[] = {
            "math",
        };
        const char* const AttributeElements470[] = {
            "mfrac",
        };
        const char* const AttributeElements471[] = {
            "mfrac",
        };
        const char* const AttributeElements472[] = {
            "mfrac",
        };
        const char* const AttributeElements473[] = {
            "mfrac",
        };
        const char* const AttributeElements474[] = {
            "mo",
            "mover",
            "munderover",
        };
        const char* const AttributeElements475[] = {
            "mo",
            "munder",
            "munderover",
        };
        const char* const AttributeElements476[] = {
            "mo",
        };
        const char* const AttributeElements477[] = {
            "mo",
        };
        const char* const AttributeElements478[] = {
            "mo",
            "munder",
            "mover",
            "munderover",
        };
        const char* const AttributeElements479[] = {
            "mo",
        };
        const char* const AttributeElements480[] = {
            "mo",
        };
        const char* const AttributeElements481[] = {
            "mo",
        };
        const char* const AttributeElements482[] = {
            "mo",
        };
        const char* const AttributeElements483[] = {
            "mo",
            "mpadded",
        };
        const char* const AttributeElements484[] = {
            "mo",
            "mpadded",
        };
        const char* const AttributeElements485[] = {
            "mo",
        };
        const char* const AttributeElements486[] = {
            "mo",
        };
        const char* const AttributeElements487[] = {
            "mtable",
            "mtr",
            "mtd",
        };
        const char* const AttributeElements488[] = {
            "mtable",
            "mtr",
            "mtd",
        };
        const char* const AttributeElements489[] = {
            "mtable",
        };
        const char* const AttributeElements490[] = {
            "mtable",
        };
        const char* const AttributeElements491[] = {
            "mtable",
        };
        const char* const AttributeElements492[] = {
            "mtable",
        };
        const char* const AttributeElements493[] = {
            "mtable",
        };
        const char* const AttributeElements494[] = {
            "mtable",
        };
        const char* const AttributeElements495[] = {
            "mtable",
        };
        const char* const AttributeElements496[] = {
            "mtable",
        };
        const char* const AttributeElements497[] = {
            "mtable",
        };
        const char* const AttributeElements498[] = {
            "mtd",
        };
        const char* const AttributeElements499[] = {
            "mtd",
        };
        const char* const AttributeElements500[] = {
            "mspace",
            "mpadded",
            "mtd",
        };
        const char* const AttributeElements501[] = {
            "mspace",
            "mpadded",
        };
        const char* const AttributeElements502[] = {
            "mspace",
            "mpadded",
        };
        const char* const AttributeElements503[] = {
            "mpadded",
        };
        const char* const AttributeElements504[] = {
            "annotation",
        };
        const char* const AttributeElements505[] = {
            "annotation",
            "annotation-xml",
        };
        const char* const AttributeElements506[] = {
            "annotation",
        };
        const char* const AttributeElements507[] = {
            "annotation-xml",
        };
        const char* const AttributeElements508[] = {
            "maction",
        };
        const char* const AttributeElements509[] = {
            "maction",
        };
        const char* const AttributeElements510[] = {
            "mo",
        };
        const char* const AttributeElements511[] = {
            "mo",
        };
        const char* const AttributeElements512[] = {
            "mtd",
        };
        const char* const AttributeElements513[] = {
            "mtd",
        };
        const char* const AttributeElements514[] = {
            "math",
        };
    }

    const ElementRecord ElementTable[] = {
        { "html", NamespaceId::Html, ElementKind::Document, 0, ElementFlagGlobalAttributes, nullptr, 0, "the-html-element", "HTMLHtmlElement" },
        { "head", NamespaceId::Html, ElementKind::Metadata, 0, ElementFlagGlobalAttributes, nullptr, 0, "the-head-element", "HTMLHeadElement" },
        { "title", NamespaceId::Html, ElementKind::DocumentTitle, 0, ElementFlagGlobalAttributes, nullptr, 0, "the-title-element", "HTMLTitleElement" },
        { "base", NamespaceId::Html, ElementKind::BaseUrl, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes3, 2, "", "HTMLBaseElement" },
        { "link", NamespaceId::Html, ElementKind::ResourceLink, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes4, 17, "", "HTMLLinkElement" },
        { "meta", NamespaceId::Html, ElementKind::Metadata, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes5, 5, "the-meta-element", "HTMLMetaElement" },
        { "style", NamespaceId::Html, ElementKind::StyleSheet, 0, ElementFlagGlobalAttributes | ElementFlagRawText, ElementAttributes6, 3, "", "HTMLStyleElement" },
        { "body", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "the-body-element", "HTMLBodyElement" },
        { "article", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "section", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "nav", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "aside", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "h1", NamespaceId::Html, ElementKind::Heading, 1, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHeadingElement" },
        { "h2", NamespaceId::Html, ElementKind::Heading, 2, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHeadingElement" },
        { "h3", NamespaceId::Html, ElementKind::Heading, 3, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHeadingElement" },
        { "h4", NamespaceId::Html, ElementKind::Heading, 4, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHeadingElement" },
        { "h5", NamespaceId::Html, ElementKind::Heading, 5, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHeadingElement" },
        { "h6", NamespaceId::Html, ElementKind::Heading, 6, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHeadingElement" },
        { "hgroup", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "header", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "footer", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "address", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "p", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLParagraphElement" },
        { "hr", NamespaceId::Html, ElementKind::Separator, 0, ElementFlagVoid | ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLHRElement" },
        { "pre", NamespaceId::Html, ElementKind::PreformattedText, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLPreElement" },
        { "blockquote", NamespaceId::Html, ElementKind::Quote, 0, ElementFlagGlobalAttributes, ElementAttributes25, 1, "", "HTMLQuoteElement" },
        { "ol", NamespaceId::Html, ElementKind::List, 0, ElementFlagGlobalAttributes | ElementFlagOrdered, ElementAttributes26, 3, "", "HTMLOListElement" },
        { "ul", NamespaceId::Html, ElementKind::List, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLUListElement" },
        { "menu", NamespaceId::Html, ElementKind::List, 0, ElementFlagGlobalAttributes, nullptr, 0, "the-menu-element", "HTMLMenuElement" },
        { "li", NamespaceId::Html, ElementKind::ListItem, 0, ElementFlagGlobalAttributes, ElementAttributes29, 1, "", "HTMLLIElement" },
        { "dl", NamespaceId::Html, ElementKind::DescriptionList, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLDListElement" },
        { "dt", NamespaceId::Html, ElementKind::DescriptionTerm, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "dd", NamespaceId::Html, ElementKind::DescriptionDescription, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "figure", NamespaceId::Html, ElementKind::Figure, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "figcaption", NamespaceId::Html, ElementKind::FigureCaption, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "main", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "search", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "div", NamespaceId::Html, ElementKind::Container, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLDivElement" },
        { "a", NamespaceId::Html, ElementKind::Link, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing | ElementFlagInteractive, ElementAttributes38, 8, "", "HTMLAnchorElement" },
        { "em", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "strong", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "small", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "s", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "cite", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "q", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes44, 1, "", "" },
        { "dfn", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes45, 1, "", "" },
        { "abbr", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes46, 1, "", "" },
        { "ruby", NamespaceId::Html, ElementKind::Ruby, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "rt", NamespaceId::Html, ElementKind::RubyText, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "rp", NamespaceId::Html, ElementKind::RubyParenthesis, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "data", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes50, 1, "", "HTMLDataElement" },
        { "time", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes51, 1, "", "HTMLTimeElement" },
        { "code", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "var", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "samp", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "kbd", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "sub", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "sup", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "i", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "b", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "u", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "mark", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "" },
        { "bdi", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes62, 1, "", "" },
        { "bdo", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes63, 1, "", "" },
        { "span", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, nullptr, 0, "", "HTMLSpanElement" },
        { "br", NamespaceId::Html, ElementKind::LineBreak, 0, ElementFlagVoid | ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLBRElement" },
        { "wbr", NamespaceId::Html, ElementKind::WordBreakOpportunity, 0, ElementFlagVoid | ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "ins", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes67, 2, "", "" },
        { "del", NamespaceId::Html, ElementKind::Text, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing, ElementAttributes68, 2, "", "" },
        { "picture", NamespaceId::Html, ElementKind::ResponsiveImage, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLPictureElement" },
        { "source", NamespaceId::Html, ElementKind::MediaSource, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes70, 7, "", "HTMLSourceElement" },
        { "img", NamespaceId::Html, ElementKind::Image, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes71, 14, "", "HTMLImageElement" },
        { "iframe", NamespaceId::Html, ElementKind::Frame, 0, ElementFlagGlobalAttributes, ElementAttributes72, 10, "", "HTMLIFrameElement" },
        { "embed", NamespaceId::Html, ElementKind::EmbeddedContent, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes73, 4, "", "HTMLEmbedElement" },
        { "object", NamespaceId::Html, ElementKind::EmbeddedObject, 0, ElementFlagGlobalAttributes, ElementAttributes74, 6, "", "HTMLObjectElement" },
        { "video", NamespaceId::Html, ElementKind::Video, 0, ElementFlagGlobalAttributes, ElementAttributes75, 12, "the-video-element", "HTMLVideoElement" },
        { "audio", NamespaceId::Html, ElementKind::Audio, 0, ElementFlagGlobalAttributes, ElementAttributes76, 8, "the-audio-element", "HTMLAudioElement" },
        { "track", NamespaceId::Html, ElementKind::MediaTrack, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes77, 5, "", "HTMLTrackElement" },
        { "map", NamespaceId::Html, ElementKind::ImageMap, 0, ElementFlagGlobalAttributes, ElementAttributes78, 1, "", "HTMLMapElement" },
        { "area", NamespaceId::Html, ElementKind::ImageMapLink, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes79, 11, "", "HTMLAreaElement" },
        { "table", NamespaceId::Html, ElementKind::Table, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLTableElement" },
        { "caption", NamespaceId::Html, ElementKind::TableCaption, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLTableCaptionElement" },
        { "colgroup", NamespaceId::Html, ElementKind::TableColumnGroup, 0, ElementFlagGlobalAttributes, ElementAttributes82, 1, "", "HTMLTableColElement" },
        { "col", NamespaceId::Html, ElementKind::TableColumn, 0, ElementFlagVoid | ElementFlagGlobalAttributes, ElementAttributes83, 1, "", "" },
        { "tbody", NamespaceId::Html, ElementKind::TableSection, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLTableSectionElement" },
        { "thead", NamespaceId::Html, ElementKind::TableSection, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "tfoot", NamespaceId::Html, ElementKind::TableSection, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "tr", NamespaceId::Html, ElementKind::TableRow, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLTableRowElement" },
        { "td", NamespaceId::Html, ElementKind::TableCell, 0, ElementFlagGlobalAttributes, ElementAttributes88, 3, "", "HTMLTableCellElement" },
        { "th", NamespaceId::Html, ElementKind::TableHeaderCell, 0, ElementFlagGlobalAttributes, ElementAttributes89, 5, "", "" },
        { "form", NamespaceId::Html, ElementKind::Form, 0, ElementFlagGlobalAttributes, ElementAttributes90, 9, "", "HTMLFormElement" },
        { "label", NamespaceId::Html, ElementKind::Label, 0, ElementFlagGlobalAttributes, ElementAttributes91, 1, "", "HTMLLabelElement" },
        { "input", NamespaceId::Html, ElementKind::Input, 0, ElementFlagVoid | ElementFlagGlobalAttributes | ElementFlagFormAssociated | ElementFlagInteractive, ElementAttributes92, 35, "", "HTMLInputElement" },
        { "button", NamespaceId::Html, ElementKind::Button, 0, ElementFlagGlobalAttributes | ElementFlagFormAssociated | ElementFlagInteractive, ElementAttributes93, 14, "", "HTMLButtonElement" },
        { "select", NamespaceId::Html, ElementKind::Select, 0, ElementFlagGlobalAttributes | ElementFlagFormAssociated | ElementFlagInteractive, ElementAttributes94, 7, "", "HTMLSelectElement" },
        { "datalist", NamespaceId::Html, ElementKind::DataList, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLDataListElement" },
        { "optgroup", NamespaceId::Html, ElementKind::OptionGroup, 0, ElementFlagGlobalAttributes, ElementAttributes96, 2, "", "HTMLOptGroupElement" },
        { "option", NamespaceId::Html, ElementKind::Option, 0, ElementFlagGlobalAttributes, ElementAttributes97, 4, "", "HTMLOptionElement" },
        { "textarea", NamespaceId::Html, ElementKind::TextArea, 0, ElementFlagGlobalAttributes | ElementFlagFormAssociated, ElementAttributes98, 13, "", "HTMLTextAreaElement" },
        { "output", NamespaceId::Html, ElementKind::Output, 0, ElementFlagGlobalAttributes | ElementFlagFormAssociated, ElementAttributes99, 3, "", "HTMLOutputElement" },
        { "progress", NamespaceId::Html, ElementKind::Progress, 0, ElementFlagGlobalAttributes | ElementFlagFormAssociated, ElementAttributes100, 2, "", "HTMLProgressElement" },
        { "meter", NamespaceId::Html, ElementKind::Meter, 0, ElementFlagGlobalAttributes | ElementFlagFormAssociated, ElementAttributes101, 6, "", "HTMLMeterElement" },
        { "fieldset", NamespaceId::Html, ElementKind::FieldSet, 0, ElementFlagGlobalAttributes, ElementAttributes102, 3, "", "HTMLFieldSetElement" },
        { "legend", NamespaceId::Html, ElementKind::Legend, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLLegendElement" },
        { "selectedcontent", NamespaceId::Html, ElementKind::SelectedContent, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "HTMLSelectedContentElement" },
        { "details", NamespaceId::Html, ElementKind::Disclosure, 0, ElementFlagGlobalAttributes | ElementFlagInteractive, ElementAttributes105, 2, "", "HTMLDetailsElement" },
        { "summary", NamespaceId::Html, ElementKind::DisclosureSummary, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "dialog", NamespaceId::Html, ElementKind::Dialog, 0, ElementFlagGlobalAttributes, ElementAttributes107, 2, "", "HTMLDialogElement" },
        { "script", NamespaceId::Html, ElementKind::ScriptResource, 0, ElementFlagGlobalAttributes | ElementFlagRawText, ElementAttributes108, 10, "the-script-element", "HTMLScriptElement" },
        { "noscript", NamespaceId::Html, ElementKind::NoScript, 0, ElementFlagGlobalAttributes, nullptr, 0, "", "" },
        { "template", NamespaceId::Html, ElementKind::Template, 0, ElementFlagGlobalAttributes, ElementAttributes110, 7, "the-template-element", "HTMLTemplateElement" },
        { "slot", NamespaceId::Html, ElementKind::Slot, 0, ElementFlagGlobalAttributes, ElementAttributes111, 1, "the-slot-element", "HTMLSlotElement" },
        { "canvas", NamespaceId::Html, ElementKind::Canvas, 0, ElementFlagGlobalAttributes, ElementAttributes112, 2, "", "HTMLCanvasElement" },
        { "applet", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "acronym", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "bgsound", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "dir", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "noframes", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "isindex", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "keygen", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "listing", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "menuitem", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "nextid", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "noembed", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "param", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagVoid | ElementFlagObsolete, nullptr, 0, "", "" },
        { "plaintext", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "rb", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "rtc", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "strike", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "xmp", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "basefont", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "big", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "blink", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "center", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "font", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "multicol", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "nobr", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "spacer", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "tt", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "marquee", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "the-marquee-element", "" },
        { "frameset", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "frame", NamespaceId::Html, ElementKind::ObsoleteElement, 0, ElementFlagObsolete, nullptr, 0, "", "" },
        { "fencedframe", NamespaceId::Html, ElementKind::Frame, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing | ElementFlagInteractive, ElementAttributes142, 3, "", "HTMLFencedFrameElement" },
        { "geolocation", NamespaceId::Html, ElementKind::Button, 0, ElementFlagGlobalAttributes | ElementFlagPhrasing | ElementFlagInteractive, ElementAttributes143, 2, "", "HTMLGeolocationElement" },
        { "svg", NamespaceId::Svg, ElementKind::SvgDocument, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "g", NamespaceId::Svg, ElementKind::SvgContainer, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "defs", NamespaceId::Svg, ElementKind::SvgContainer, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "symbol", NamespaceId::Svg, ElementKind::SvgContainer, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "switch", NamespaceId::Svg, ElementKind::SvgContainer, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "view", NamespaceId::Svg, ElementKind::SvgContainer, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "a", NamespaceId::Svg, ElementKind::SvgContainer, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "use", NamespaceId::Svg, ElementKind::SvgUse, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "circle", NamespaceId::Svg, ElementKind::SvgShape, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "ellipse", NamespaceId::Svg, ElementKind::SvgShape, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "rect", NamespaceId::Svg, ElementKind::SvgShape, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "line", NamespaceId::Svg, ElementKind::SvgShape, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "polyline", NamespaceId::Svg, ElementKind::SvgShape, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "polygon", NamespaceId::Svg, ElementKind::SvgShape, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "path", NamespaceId::Svg, ElementKind::SvgPath, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "text", NamespaceId::Svg, ElementKind::SvgText, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "tspan", NamespaceId::Svg, ElementKind::SvgText, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "textPath", NamespaceId::Svg, ElementKind::SvgText, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "image", NamespaceId::Svg, ElementKind::SvgImage, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "foreignObject", NamespaceId::Svg, ElementKind::SvgForeignObject, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "linearGradient", NamespaceId::Svg, ElementKind::SvgGradient, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "radialGradient", NamespaceId::Svg, ElementKind::SvgGradient, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "stop", NamespaceId::Svg, ElementKind::SvgStop, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "clipPath", NamespaceId::Svg, ElementKind::SvgClipPath, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mask", NamespaceId::Svg, ElementKind::SvgMask, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "marker", NamespaceId::Svg, ElementKind::SvgMarker, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "pattern", NamespaceId::Svg, ElementKind::SvgPattern, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "filter", NamespaceId::Svg, ElementKind::SvgFilter, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feBlend", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feColorMatrix", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feComponentTransfer", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feComposite", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feConvolveMatrix", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feDiffuseLighting", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feDisplacementMap", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feDistantLight", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feDropShadow", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feFlood", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feFuncA", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feFuncB", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feFuncG", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feFuncR", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feGaussianBlur", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feImage", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feMerge", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feMergeNode", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feMorphology", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feOffset", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "fePointLight", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feSpecularLighting", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feSpotLight", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feTile", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "feTurbulence", NamespaceId::Svg, ElementKind::SvgFilterPrimitive, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "animate", NamespaceId::Svg, ElementKind::SvgAnimation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "animateMotion", NamespaceId::Svg, ElementKind::SvgAnimation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "animateTransform", NamespaceId::Svg, ElementKind::SvgAnimation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "set", NamespaceId::Svg, ElementKind::SvgAnimation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mpath", NamespaceId::Svg, ElementKind::SvgAnimation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "discard", NamespaceId::Svg, ElementKind::SvgAnimation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "title", NamespaceId::Svg, ElementKind::SvgMetadata, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "desc", NamespaceId::Svg, ElementKind::SvgMetadata, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "metadata", NamespaceId::Svg, ElementKind::SvgMetadata, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "script", NamespaceId::Svg, ElementKind::SvgMetadata, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "style", NamespaceId::Svg, ElementKind::SvgMetadata, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "math", NamespaceId::MathMl, ElementKind::MathRoot, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mi", NamespaceId::MathMl, ElementKind::MathToken, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mn", NamespaceId::MathMl, ElementKind::MathToken, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "ms", NamespaceId::MathMl, ElementKind::MathToken, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mtext", NamespaceId::MathMl, ElementKind::MathToken, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mo", NamespaceId::MathMl, ElementKind::MathOperator, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mrow", NamespaceId::MathMl, ElementKind::MathRow, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mstyle", NamespaceId::MathMl, ElementKind::MathStyle, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "merror", NamespaceId::MathMl, ElementKind::MathError, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mphantom", NamespaceId::MathMl, ElementKind::MathPhantom, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "maction", NamespaceId::MathMl, ElementKind::MathAction, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "semantics", NamespaceId::MathMl, ElementKind::MathSemantics, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "annotation", NamespaceId::MathMl, ElementKind::MathAnnotation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "annotation-xml", NamespaceId::MathMl, ElementKind::MathAnnotation, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mfrac", NamespaceId::MathMl, ElementKind::MathFraction, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "msqrt", NamespaceId::MathMl, ElementKind::MathRadical, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mroot", NamespaceId::MathMl, ElementKind::MathRadical, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "msub", NamespaceId::MathMl, ElementKind::MathScript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "msup", NamespaceId::MathMl, ElementKind::MathScript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "msubsup", NamespaceId::MathMl, ElementKind::MathScript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "munder", NamespaceId::MathMl, ElementKind::MathScript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mover", NamespaceId::MathMl, ElementKind::MathScript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "munderover", NamespaceId::MathMl, ElementKind::MathScript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mmultiscripts", NamespaceId::MathMl, ElementKind::MathMultiScripts, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mprescripts", NamespaceId::MathMl, ElementKind::MathPrescript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "none", NamespaceId::MathMl, ElementKind::MathPrescript, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mspace", NamespaceId::MathMl, ElementKind::MathSpace, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mpadded", NamespaceId::MathMl, ElementKind::MathPadded, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mtable", NamespaceId::MathMl, ElementKind::MathTable, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mtr", NamespaceId::MathMl, ElementKind::MathTableRow, 0, ElementFlagNone, nullptr, 0, "", "" },
        { "mtd", NamespaceId::MathMl, ElementKind::MathTableCell, 0, ElementFlagNone, nullptr, 0, "", "" },
    };

    const std::uint32_t ElementCount = 239;

    const AttributeRecord AttributeTable[] = {
        { "accesskey", NamespaceId::Html, NativeProperty::MetadataAccessKey, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "anchor", NamespaceId::Html, NativeProperty::MetadataAnchor, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "autocapitalize", NamespaceId::Html, NativeProperty::MetadataAutoCapitalize, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "autocorrect", NamespaceId::Html, NativeProperty::MetadataAutoCorrect, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "autofocus", NamespaceId::Html, NativeProperty::StateAutofocus, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "class", NamespaceId::Html, NativeProperty::StyleReference, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "contenteditable", NamespaceId::Html, NativeProperty::MetadataEditable, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "dir", NamespaceId::Html, NativeProperty::TextDirection, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "draggable", NamespaceId::Html, NativeProperty::StateDraggable, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "elementtiming", NamespaceId::Html, NativeProperty::MetadataElementTiming, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "enterkeyhint", NamespaceId::Html, NativeProperty::MetadataEnterKeyHint, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "exportparts", NamespaceId::Html, NativeProperty::MetadataExportParts, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "headingoffset", NamespaceId::Html, NativeProperty::HeadingOffset, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "headingreset", NamespaceId::Html, NativeProperty::HeadingReset, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "hidden", NamespaceId::Html, NativeProperty::StateHidden, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "id", NamespaceId::Html, NativeProperty::IdentityId, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "inert", NamespaceId::Html, NativeProperty::StateInert, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "inputmode", NamespaceId::Html, NativeProperty::MetadataInputMode, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "is", NamespaceId::Html, NativeProperty::CustomizedBuiltIn, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "itemid", NamespaceId::Html, NativeProperty::MicroDataItemId, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "itemprop", NamespaceId::Html, NativeProperty::MicroDataItemProp, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "itemref", NamespaceId::Html, NativeProperty::MicroDataItemRef, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "itemscope", NamespaceId::Html, NativeProperty::MicroDataItemScope, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "itemtype", NamespaceId::Html, NativeProperty::MicroDataItemType, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "lang", NamespaceId::Html, NativeProperty::TextLanguage, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "nonce", NamespaceId::Html, NativeProperty::MetadataNonce, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "part", NamespaceId::Html, NativeProperty::MetadataPart, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "popover", NamespaceId::Html, NativeProperty::MetadataPopover, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "slot", NamespaceId::Html, NativeProperty::SlotAssignment, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "spellcheck", NamespaceId::Html, NativeProperty::StateSpellCheck, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "style", NamespaceId::Html, NativeProperty::ForbiddenInlineStyle, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "tabindex", NamespaceId::Html, NativeProperty::InteractionTabIndex, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "title", NamespaceId::Html, NativeProperty::MetadataTitle, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "translate", NamespaceId::Html, NativeProperty::TextTranslate, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "virtualkeyboardpolicy", NamespaceId::Html, NativeProperty::MetadataVirtualKeyboardPolicy, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "writingsuggestions", NamespaceId::Html, NativeProperty::MetadataWriteSuggestions, AttributeFlagNamespaceDefault | AttributeFlagGlobal | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Global", nullptr, 0 },
        { "onauxclick", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onbeforeinput", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onbeforematch", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onbeforetoggle", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onblur", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncancel", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncanplay", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncanplaythrough", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onchange", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onclick", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onclose", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncommand", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncontextlost", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncontextmenu", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncontextrestored", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncopy", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncuechange", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oncut", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondblclick", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondrag", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondragend", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondragenter", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondragleave", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondragover", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondragstart", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondrop", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ondurationchange", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onemptied", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onended", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onerror", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onfocus", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onformdata", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oninput", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "oninvalid", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onkeydown", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onkeypress", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onkeyup", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onload", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onloadeddata", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onloadedmetadata", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onloadstart", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmousedown", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmouseenter", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmouseleave", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmousemove", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmouseout", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmouseover", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onmouseup", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onpaste", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onpause", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onplay", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onplaying", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onprogress", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onratechange", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onreset", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onresize", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onscroll", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onscrollend", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onsecuritypolicyviolation", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onseeked", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onseeking", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onselect", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onslotchange", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onstalled", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onsubmit", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onsuspend", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ontimeupdate", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "ontoggle", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onvolumechange", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onwaiting", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "onwheel", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagEventHandler | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "JavaScript", "Event handler", nullptr, 0 },
        { "abbr", NamespaceId::Html, NativeProperty::AccessibilityLabel, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements107, 1 },
        { "accept", NamespaceId::Html, NativeProperty::ValidationAccept, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements108, 1 },
        { "accept-charset", NamespaceId::Html, NativeProperty::DocumentCharacterSet, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements109, 1 },
        { "action", NamespaceId::Html, NativeProperty::FormAction, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements110, 1 },
        { "allow", NamespaceId::Html, NativeProperty::FramePermissions, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements111, 2 },
        { "allowfullscreen", NamespaceId::Html, NativeProperty::FrameAllowFullScreen, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements112, 1 },
        { "alpha", NamespaceId::Html, NativeProperty::ColorAlpha, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements113, 1 },
        { "alt", NamespaceId::Html, NativeProperty::ImageAlternativeText, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements114, 3 },
        { "as", NamespaceId::Html, NativeProperty::ResourceDestination, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements115, 1 },
        { "async", NamespaceId::Html, NativeProperty::ScriptAsynchronous, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements116, 1 },
        { "autocomplete", NamespaceId::Html, NativeProperty::InputAutocomplete, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements117, 4 },
        { "autolocate", NamespaceId::Html, NativeProperty::MetadataAutoLocate, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements118, 1 },
        { "autoplay", NamespaceId::Html, NativeProperty::MediaAutoPlay, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements119, 2 },
        { "blocking", NamespaceId::Html, NativeProperty::ResourceBlocking, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements120, 3 },
        { "capture", NamespaceId::Html, NativeProperty::InputCapture, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements121, 1 },
        { "charset", NamespaceId::Html, NativeProperty::DocumentCharacterSet, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements122, 1 },
        { "checked", NamespaceId::Html, NativeProperty::StateChecked, AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements123, 1 },
        { "cite", NamespaceId::Html, NativeProperty::QuoteCitation, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements124, 4 },
        { "closedby", NamespaceId::Html, NativeProperty::DialogCloseBehavior, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements125, 1 },
        { "color", NamespaceId::Html, NativeProperty::ResourceColor, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements126, 1 },
        { "colorspace", NamespaceId::Html, NativeProperty::ColorSpace, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements127, 1 },
        { "cols", NamespaceId::Html, NativeProperty::LayoutColumns, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements128, 1 },
        { "colspan", NamespaceId::Html, NativeProperty::LayoutColumnSpan, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements129, 2 },
        { "command", NamespaceId::Html, NativeProperty::InteractionCommand, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements130, 1 },
        { "commandfor", NamespaceId::Html, NativeProperty::InteractionCommandFor, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements131, 1 },
        { "content", NamespaceId::Html, NativeProperty::MetadataContent, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements132, 1 },
        { "controls", NamespaceId::Html, NativeProperty::MediaControls, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements133, 3 },
        { "coords", NamespaceId::Html, NativeProperty::ImageMapCoordinates, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements134, 1 },
        { "crossorigin", NamespaceId::Html, NativeProperty::ResourceCrossOrigin, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements135, 5 },
        { "data", NamespaceId::Html, NativeProperty::EmbeddedData, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements136, 1 },
        { "datetime", NamespaceId::Html, NativeProperty::DateValue, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements137, 3 },
        { "decoding", NamespaceId::Html, NativeProperty::ResourceDecoding, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements138, 1 },
        { "default", NamespaceId::Html, NativeProperty::ValueDefaultValue, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements139, 1 },
        { "defer", NamespaceId::Html, NativeProperty::ScriptDeferred, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements140, 1 },
        { "dir", NamespaceId::Html, NativeProperty::TextDirection, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements141, 2 },
        { "dirname", NamespaceId::Html, NativeProperty::InputDirectoryName, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements142, 2 },
        { "disabled", NamespaceId::Html, NativeProperty::StateDisabled, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements143, 8 },
        { "download", NamespaceId::Html, NativeProperty::NavigationDownload, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements144, 2 },
        { "enctype", NamespaceId::Html, NativeProperty::FormEnctype, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements145, 1 },
        { "fetchpriority", NamespaceId::Html, NativeProperty::ResourceFetchPriority, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements146, 3 },
        { "for", NamespaceId::Html, NativeProperty::LabelFor, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements147, 3 },
        { "form", NamespaceId::Html, NativeProperty::FormOwner, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements148, 7 },
        { "formaction", NamespaceId::Html, NativeProperty::FormAction, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements149, 2 },
        { "formenctype", NamespaceId::Html, NativeProperty::FormEnctype, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements150, 2 },
        { "formmethod", NamespaceId::Html, NativeProperty::FormMethod, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements151, 2 },
        { "formnovalidate", NamespaceId::Html, NativeProperty::FormNoValidate, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements152, 2 },
        { "formtarget", NamespaceId::Html, NativeProperty::FormTarget, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements153, 2 },
        { "headers", NamespaceId::Html, NativeProperty::LayoutHeaders, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements154, 2 },
        { "height", NamespaceId::Html, NativeProperty::EmbeddedHeight, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements155, 3 },
        { "height", NamespaceId::Html, NativeProperty::EmbeddedHeight, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements156, 1 },
        { "height", NamespaceId::Html, NativeProperty::LayoutHeight, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements157, 9 },
        { "high", NamespaceId::Html, NativeProperty::ConstraintHigh, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements158, 1 },
        { "href", NamespaceId::Html, NativeProperty::ResourceSource, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements159, 1 },
        { "href", NamespaceId::Html, NativeProperty::NavigationHref, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements160, 4 },
        { "hreflang", NamespaceId::Html, NativeProperty::NavigationLanguage, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements161, 3 },
        { "http-equiv", NamespaceId::Html, NativeProperty::DocumentHttpEquiv, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements162, 1 },
        { "imagesizes", NamespaceId::Html, NativeProperty::ResourceImages, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements163, 1 },
        { "imagesrcset", NamespaceId::Html, NativeProperty::ResourceCandidates, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements164, 1 },
        { "integrity", NamespaceId::Html, NativeProperty::ResourceIntegrity, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements165, 2 },
        { "ismap", NamespaceId::Html, NativeProperty::ImageMapServerSide, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements166, 1 },
        { "kind", NamespaceId::Html, NativeProperty::MediaKind, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements167, 1 },
        { "label", NamespaceId::Html, NativeProperty::OptionLabel, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements168, 3 },
        { "list", NamespaceId::Html, NativeProperty::InputOptions, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements169, 1 },
        { "loading", NamespaceId::Html, NativeProperty::ResourceLoadMode, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements170, 4 },
        { "loop", NamespaceId::Html, NativeProperty::MediaLoop, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements171, 2 },
        { "low", NamespaceId::Html, NativeProperty::ConstraintLow, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements172, 1 },
        { "max", NamespaceId::Html, NativeProperty::ConstraintMaximum, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements173, 3 },
        { "maxlength", NamespaceId::Html, NativeProperty::ValidationMaximumLength, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements174, 2 },
        { "media", NamespaceId::Html, NativeProperty::ResourceMedia, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements175, 4 },
        { "method", NamespaceId::Html, NativeProperty::FormMethod, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements176, 1 },
        { "min", NamespaceId::Html, NativeProperty::ConstraintMinimum, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements177, 2 },
        { "minlength", NamespaceId::Html, NativeProperty::ValidationMinimumLength, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements178, 2 },
        { "multiple", NamespaceId::Html, NativeProperty::StateMultiple, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements179, 2 },
        { "muted", NamespaceId::Html, NativeProperty::MediaMuted, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements180, 2 },
        { "name", NamespaceId::Html, NativeProperty::IdentityName, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements181, 13 },
        { "nomodule", NamespaceId::Html, NativeProperty::ScriptNoModule, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements182, 1 },
        { "novalidate", NamespaceId::Html, NativeProperty::FormNoValidate, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements183, 1 },
        { "open", NamespaceId::Html, NativeProperty::StateOpen, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements184, 2 },
        { "optimum", NamespaceId::Html, NativeProperty::ConstraintOptimum, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements185, 1 },
        { "pattern", NamespaceId::Html, NativeProperty::ValidationPattern, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements186, 1 },
        { "ping", NamespaceId::Html, NativeProperty::NavigationPing, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements187, 2 },
        { "placeholder", NamespaceId::Html, NativeProperty::InputPlaceholder, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements188, 2 },
        { "playsinline", NamespaceId::Html, NativeProperty::MediaPlayInline, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements189, 1 },
        { "popovertarget", NamespaceId::Html, NativeProperty::InteractionCommandTarget, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements190, 2 },
        { "popovertargetaction", NamespaceId::Html, NativeProperty::InteractionCommand, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements191, 2 },
        { "poster", NamespaceId::Html, NativeProperty::MediaPoster, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements192, 1 },
        { "preload", NamespaceId::Html, NativeProperty::MediaPreload, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements193, 2 },
        { "readonly", NamespaceId::Html, NativeProperty::StateReadOnly, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements194, 2 },
        { "referrerpolicy", NamespaceId::Html, NativeProperty::NavigationReferrerPolicy, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements195, 6 },
        { "rel", NamespaceId::Html, NativeProperty::NavigationRelation, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements196, 4 },
        { "required", NamespaceId::Html, NativeProperty::StateRequired, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements197, 3 },
        { "reversed", NamespaceId::Html, NativeProperty::ListReversed, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements198, 1 },
        { "rows", NamespaceId::Html, NativeProperty::LayoutRows, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements199, 1 },
        { "rowspan", NamespaceId::Html, NativeProperty::LayoutRowSpan, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements200, 2 },
        { "sandbox", NamespaceId::Html, NativeProperty::FrameSandbox, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements201, 1 },
        { "scope", NamespaceId::Html, NativeProperty::LayoutScope, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements202, 1 },
        { "selected", NamespaceId::Html, NativeProperty::StateSelected, AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements203, 1 },
        { "shadowrootclonable", NamespaceId::Html, NativeProperty::ShadowRootClonable, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements204, 1 },
        { "shadowrootcustomelementregistry", NamespaceId::Html, NativeProperty::ShadowRootCustomElementRegistry, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements205, 1 },
        { "shadowrootdelegatesfocus", NamespaceId::Html, NativeProperty::ShadowRootDelegatesFocus, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements206, 1 },
        { "shadowrootmode", NamespaceId::Html, NativeProperty::ShadowRootMode, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements207, 1 },
        { "shadowrootserializable", NamespaceId::Html, NativeProperty::ShadowRootSerializable, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements208, 1 },
        { "shadowrootslotassignment", NamespaceId::Html, NativeProperty::ShadowRootSlotAssignment, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements209, 1 },
        { "shape", NamespaceId::Html, NativeProperty::ImageMapShape, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements210, 1 },
        { "size", NamespaceId::Html, NativeProperty::InputSize, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements211, 2 },
        { "sizes", NamespaceId::Html, NativeProperty::ResourceSizes, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements212, 3 },
        { "span", NamespaceId::Html, NativeProperty::LayoutSpan, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements213, 2 },
        { "src", NamespaceId::Html, NativeProperty::ResourceSource, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements214, 9 },
        { "srcdoc", NamespaceId::Html, NativeProperty::FrameSourceDocument, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements215, 1 },
        { "srclang", NamespaceId::Html, NativeProperty::MediaSourceLanguage, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements216, 1 },
        { "srcset", NamespaceId::Html, NativeProperty::ResourceSourceSet, AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements217, 2 },
        { "start", NamespaceId::Html, NativeProperty::ListStart, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements218, 1 },
        { "step", NamespaceId::Html, NativeProperty::ConstraintStep, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements219, 1 },
        { "target", NamespaceId::Html, NativeProperty::NavigationTarget, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements220, 4 },
        { "title", NamespaceId::Html, NativeProperty::MetadataTitle, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements221, 5 },
        { "type", NamespaceId::Html, NativeProperty::InputType, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements222, 1 },
        { "type", NamespaceId::Html, NativeProperty::ButtonType, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements223, 1 },
        { "type", NamespaceId::Html, NativeProperty::ListStyle, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements224, 1 },
        { "type", NamespaceId::Html, NativeProperty::ResourceMimeType, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements225, 10 },
        { "usemap", NamespaceId::Html, NativeProperty::ImageMapName, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements226, 1 },
        { "value", NamespaceId::Html, NativeProperty::ValueValue, AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements227, 7 },
        { "watch", NamespaceId::Html, NativeProperty::MetadataWatch, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements228, 1 },
        { "width", NamespaceId::Html, NativeProperty::EmbeddedWidth, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements229, 3 },
        { "width", NamespaceId::Html, NativeProperty::EmbeddedWidth, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements230, 1 },
        { "width", NamespaceId::Html, NativeProperty::LayoutWidth, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements231, 9 },
        { "wrap", NamespaceId::Html, NativeProperty::InputWrap, AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Element", AttributeElements232, 1 },
        { "align", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "alink", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "alinkcolor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "all", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "allowpaymentrequest", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "allowtransparency", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "allowusermedia", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "anchors", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "applets", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "archive", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "axis", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "background", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "behavior", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "bgcolor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "border", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "bordercolor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "bottommargin", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "cellpadding", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "cellspacing", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "ch", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "char", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "charoff", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "choff", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "classid", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "clear", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "code", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "codebase", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "codetype", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "compact", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "contentdocument", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "contentwindow", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "contextmenu", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "datafld", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "dataformatas", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "datapagesize", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "datasrc", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "declare", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "direction", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "dropzone", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "event", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "external", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "face", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "fgcolor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "frame", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "frameborder", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "framespacing", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "hspace", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "htmlfor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "language", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "leftmargin", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "link", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "linkcolor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "longdesc", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "lowsrc", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "manifest", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "marginheight", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "marginwidth", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "methods", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "nohref", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "noresize", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "noshade", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "nowrap", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "onshow", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "profile", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "rev", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "rightmargin", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "rules", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "scheme", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "scrollamount", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "scrolldelay", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "scrolling", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "standby", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "summary", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "text", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "topmargin", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "truespeed", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "typemustmatch", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "urn", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "valign", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "valuetype", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "version", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "vlink", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "vlinkcolor", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "vspace", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Obsolete", nullptr, 0 },
        { "x", NamespaceId::Svg, NativeProperty::SvgGeometryX, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", nullptr, 0 },
        { "y", NamespaceId::Svg, NativeProperty::SvgGeometryY, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", nullptr, 0 },
        { "x1", NamespaceId::Svg, NativeProperty::SvgGeometryX1, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements319, 2 },
        { "y1", NamespaceId::Svg, NativeProperty::SvgGeometryY1, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements320, 2 },
        { "x2", NamespaceId::Svg, NativeProperty::SvgGeometryX2, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements321, 2 },
        { "y2", NamespaceId::Svg, NativeProperty::SvgGeometryY2, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements322, 2 },
        { "cx", NamespaceId::Svg, NativeProperty::SvgGeometryCenterX, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements323, 3 },
        { "cy", NamespaceId::Svg, NativeProperty::SvgGeometryCenterY, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements324, 3 },
        { "r", NamespaceId::Svg, NativeProperty::SvgGeometryRadius, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements325, 2 },
        { "rx", NamespaceId::Svg, NativeProperty::SvgGeometryRadiusX, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements326, 2 },
        { "ry", NamespaceId::Svg, NativeProperty::SvgGeometryRadiusY, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements327, 2 },
        { "fx", NamespaceId::Svg, NativeProperty::SvgGeometryCenterX, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements328, 1 },
        { "fy", NamespaceId::Svg, NativeProperty::SvgGeometryCenterY, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements329, 1 },
        { "fr", NamespaceId::Svg, NativeProperty::SvgGeometryRadius, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements330, 1 },
        { "width", NamespaceId::Svg, NativeProperty::SvgGeometryWidth, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", nullptr, 0 },
        { "height", NamespaceId::Svg, NativeProperty::SvgGeometryHeight, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", nullptr, 0 },
        { "d", NamespaceId::Svg, NativeProperty::SvgGeometryPath, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements333, 1 },
        { "points", NamespaceId::Svg, NativeProperty::SvgGeometryPoints, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements334, 2 },
        { "offset", NamespaceId::Svg, NativeProperty::SvgGradientOffset, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Gradient", AttributeElements335, 5 },
        { "pathLength", NamespaceId::Svg, NativeProperty::SvgGeometryPathLength, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", nullptr, 0 },
        { "viewBox", NamespaceId::Svg, NativeProperty::SvgViewBox, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements337, 5 },
        { "preserveAspectRatio", NamespaceId::Svg, NativeProperty::SvgPreserveAspectRatio, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Geometry", AttributeElements338, 6 },
        { "fill", NamespaceId::Svg, NativeProperty::SvgPaintFill, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "fill-opacity", NamespaceId::Svg, NativeProperty::SvgPaintFillOpacity, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "fill-rule", NamespaceId::Svg, NativeProperty::SvgPaintFillRule, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke", NamespaceId::Svg, NativeProperty::SvgPaintStroke, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-width", NamespaceId::Svg, NativeProperty::SvgPaintStrokeWidth, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-opacity", NamespaceId::Svg, NativeProperty::SvgPaintStrokeOpacity, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-linecap", NamespaceId::Svg, NativeProperty::SvgPaintStrokeLineCap, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-linejoin", NamespaceId::Svg, NativeProperty::SvgPaintStrokeLineJoin, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-dasharray", NamespaceId::Svg, NativeProperty::SvgPaintStrokeDashArray, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-dashoffset", NamespaceId::Svg, NativeProperty::SvgPaintStrokeDashOffset, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "stroke-miterlimit", NamespaceId::Svg, NativeProperty::SvgPaintStrokeMiterLimit, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "color", NamespaceId::Svg, NativeProperty::SvgPaintColor, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "opacity", NamespaceId::Svg, NativeProperty::SvgPaintOpacity, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "transform", NamespaceId::Svg, NativeProperty::SvgTransform, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Transform", nullptr, 0 },
        { "transform-origin", NamespaceId::Svg, NativeProperty::SvgTransformOrigin, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Transform", nullptr, 0 },
        { "gradientUnits", NamespaceId::Svg, NativeProperty::SvgGradientUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Gradient", AttributeElements354, 2 },
        { "gradientTransform", NamespaceId::Svg, NativeProperty::SvgGradientTransform, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Gradient", AttributeElements355, 2 },
        { "spreadMethod", NamespaceId::Svg, NativeProperty::SvgSpreadMethod, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Gradient", AttributeElements356, 2 },
        { "stop-color", NamespaceId::Svg, NativeProperty::SvgStopColor, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Gradient", AttributeElements357, 1 },
        { "stop-opacity", NamespaceId::Svg, NativeProperty::SvgStopOpacity, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Gradient", AttributeElements358, 1 },
        { "patternUnits", NamespaceId::Svg, NativeProperty::SvgPatternUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", AttributeElements359, 1 },
        { "patternContentUnits", NamespaceId::Svg, NativeProperty::SvgPatternContentUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", AttributeElements360, 1 },
        { "patternTransform", NamespaceId::Svg, NativeProperty::SvgPatternTransform, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", AttributeElements361, 1 },
        { "font-family", NamespaceId::Svg, NativeProperty::SvgFontFamily, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "font-size", NamespaceId::Svg, NativeProperty::SvgFontSize, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "font-weight", NamespaceId::Svg, NativeProperty::SvgFontWeight, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "font-style", NamespaceId::Svg, NativeProperty::SvgFontStyle, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "font-variant", NamespaceId::Svg, NativeProperty::MathVariant, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "text-anchor", NamespaceId::Svg, NativeProperty::SvgTextAnchor, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "letter-spacing", NamespaceId::Svg, NativeProperty::SvgLetterSpacing, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "word-spacing", NamespaceId::Svg, NativeProperty::SvgWordSpacing, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "textLength", NamespaceId::Svg, NativeProperty::SvgTextLength, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", AttributeElements370, 3 },
        { "lengthAdjust", NamespaceId::Svg, NativeProperty::SvgTextLength, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", AttributeElements371, 3 },
        { "dominant-baseline", NamespaceId::Svg, NativeProperty::SvgDominantBaseline, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "alignment-baseline", NamespaceId::Svg, NativeProperty::SvgDominantBaseline, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "baseline-shift", NamespaceId::Svg, NativeProperty::SvgBaselineShift, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "startOffset", NamespaceId::Svg, NativeProperty::SvgStartOffset, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", AttributeElements375, 1 },
        { "dx", NamespaceId::Svg, NativeProperty::SvgGeometryX, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "dy", NamespaceId::Svg, NativeProperty::SvgGeometryY, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "rotate", NamespaceId::Svg, NativeProperty::SvgAnimationRotation, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Typography", nullptr, 0 },
        { "display", NamespaceId::Svg, NativeProperty::SvgDisplay, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "visibility", NamespaceId::Svg, NativeProperty::SvgVisibility, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "overflow", NamespaceId::Svg, NativeProperty::SvgOverflow, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "pointer-events", NamespaceId::Svg, NativeProperty::SvgPointerEvents, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "clip", NamespaceId::Svg, NativeProperty::SvgClipDefinition, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Clipping", nullptr, 0 },
        { "clip-path", NamespaceId::Svg, NativeProperty::SvgClipDefinition, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Clipping", nullptr, 0 },
        { "clip-rule", NamespaceId::Svg, NativeProperty::SvgClipRule, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Clipping", nullptr, 0 },
        { "clipPathUnits", NamespaceId::Svg, NativeProperty::SvgClipPathUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Clipping", AttributeElements386, 1 },
        { "mask", NamespaceId::Svg, NativeProperty::SvgMaskDefinition, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Masking", nullptr, 0 },
        { "maskUnits", NamespaceId::Svg, NativeProperty::SvgMaskUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Masking", AttributeElements388, 1 },
        { "maskContentUnits", NamespaceId::Svg, NativeProperty::SvgMaskContentUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Masking", AttributeElements389, 1 },
        { "filter", NamespaceId::Svg, NativeProperty::SvgFilterDefinition, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", nullptr, 0 },
        { "filterUnits", NamespaceId::Svg, NativeProperty::SvgFilterUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements391, 1 },
        { "primitiveUnits", NamespaceId::Svg, NativeProperty::SvgFilterUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements392, 1 },
        { "in", NamespaceId::Svg, NativeProperty::SvgFilterPrimitiveInput, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", nullptr, 0 },
        { "in2", NamespaceId::Svg, NativeProperty::SvgFilterPrimitiveInput, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", nullptr, 0 },
        { "result", NamespaceId::Svg, NativeProperty::SvgFilterPrimitiveResult, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", nullptr, 0 },
        { "stdDeviation", NamespaceId::Svg, NativeProperty::SvgFilterPrimitives, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements396, 2 },
        { "mode", NamespaceId::Svg, NativeProperty::SvgFilterPrimitives, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements397, 2 },
        { "operator", NamespaceId::Svg, NativeProperty::SvgFilterPrimitives, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements398, 2 },
        { "radius", NamespaceId::Svg, NativeProperty::SvgFilterPrimitives, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements399, 1 },
        { "scale", NamespaceId::Svg, NativeProperty::SvgFilterPrimitives, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements400, 1 },
        { "kernelMatrix", NamespaceId::Svg, NativeProperty::SvgFilterPrimitives, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements401, 1 },
        { "values", NamespaceId::Svg, NativeProperty::SvgAnimationValues, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", AttributeElements402, 1 },
        { "type", NamespaceId::Svg, NativeProperty::SvgAnimationType, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Filtering", AttributeElements403, 5 },
        { "marker-start", NamespaceId::Svg, NativeProperty::SvgMarkerStart, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", nullptr, 0 },
        { "marker-mid", NamespaceId::Svg, NativeProperty::SvgMarkerMiddle, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", nullptr, 0 },
        { "marker-end", NamespaceId::Svg, NativeProperty::SvgMarkerEnd, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", nullptr, 0 },
        { "markerWidth", NamespaceId::Svg, NativeProperty::SvgMarkerWidth, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", AttributeElements407, 1 },
        { "markerHeight", NamespaceId::Svg, NativeProperty::SvgMarkerHeight, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", AttributeElements408, 1 },
        { "markerUnits", NamespaceId::Svg, NativeProperty::SvgMarkerUnits, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", AttributeElements409, 1 },
        { "orient", NamespaceId::Svg, NativeProperty::SvgMarkerOrientation, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", AttributeElements410, 1 },
        { "refX", NamespaceId::Svg, NativeProperty::SvgGeometryX, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", AttributeElements411, 1 },
        { "refY", NamespaceId::Svg, NativeProperty::SvgGeometryY, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Marker", AttributeElements412, 1 },
        { "attributeName", NamespaceId::Svg, NativeProperty::SvgAnimationAttributeName, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "attributeType", NamespaceId::Svg, NativeProperty::SvgAnimationAttributeType, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "from", NamespaceId::Svg, NativeProperty::SvgAnimationFrom, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "to", NamespaceId::Svg, NativeProperty::SvgAnimationTo, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "by", NamespaceId::Svg, NativeProperty::SvgAnimationBy, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "begin", NamespaceId::Svg, NativeProperty::SvgAnimationBegin, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "dur", NamespaceId::Svg, NativeProperty::SvgAnimationDuration, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "end", NamespaceId::Svg, NativeProperty::SvgAnimationDuration, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "repeatCount", NamespaceId::Svg, NativeProperty::SvgAnimationRepeatCount, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "repeatDur", NamespaceId::Svg, NativeProperty::SvgAnimationRepeatCount, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "fill", NamespaceId::Svg, NativeProperty::SvgAnimationFillMode, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", AttributeElements423, 4 },
        { "path", NamespaceId::Svg, NativeProperty::SvgAnimationPath, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", AttributeElements424, 1 },
        { "keyTimes", NamespaceId::Svg, NativeProperty::SvgAnimationKeyTimes, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "keySplines", NamespaceId::Svg, NativeProperty::SvgAnimationKeySplines, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "calcMode", NamespaceId::Svg, NativeProperty::SvgAnimationCalcMode, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "additive", NamespaceId::Svg, NativeProperty::SvgAnimationAdditive, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "accumulate", NamespaceId::Svg, NativeProperty::SvgAnimationAccumulate, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "restart", NamespaceId::Svg, NativeProperty::SvgAnimationRestart, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "origin", NamespaceId::Svg, NativeProperty::SvgAnimationOrigin, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", nullptr, 0 },
        { "keyPoints", NamespaceId::Svg, NativeProperty::SvgAnimationValues, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Animation", AttributeElements432, 1 },
        { "id", NamespaceId::Svg, NativeProperty::IdentityId, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "class", NamespaceId::Svg, NativeProperty::StyleReference, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "role", NamespaceId::Svg, NativeProperty::AccessibilityRole, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "xmlns", NamespaceId::Svg, NativeProperty::SvgNamespace, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "xmlns:xlink", NamespaceId::Svg, NativeProperty::SvgNamespace, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "xml:lang", NamespaceId::Svg, NativeProperty::TextLanguage, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "xml:space", NamespaceId::Svg, NativeProperty::SvgVisibility, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "href", NamespaceId::Svg, NativeProperty::SvgReference, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "References", nullptr, 0 },
        { "xlink:href", NamespaceId::Svg, NativeProperty::SvgReference, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "References", nullptr, 0 },
        { "target", NamespaceId::Svg, NativeProperty::NavigationTarget, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "References", AttributeElements442, 1 },
        { "download", NamespaceId::Svg, NativeProperty::NavigationDownload, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "References", AttributeElements443, 1 },
        { "rel", NamespaceId::Svg, NativeProperty::NavigationRelation, AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "References", AttributeElements444, 1 },
        { "lang", NamespaceId::Svg, NativeProperty::TextLanguage, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "tabindex", NamespaceId::Svg, NativeProperty::InteractionTabIndex, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "version", NamespaceId::Svg, NativeProperty::SvgVersion, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "baseProfile", NamespaceId::Svg, NativeProperty::SvgBaseProfile, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "zoomAndPan", NamespaceId::Svg, NativeProperty::SvgZoomAndPan, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "requiredFeatures", NamespaceId::Svg, NativeProperty::SvgRequiredFeatures, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "requiredExtensions", NamespaceId::Svg, NativeProperty::SvgRequiredExtensions, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "systemLanguage", NamespaceId::Svg, NativeProperty::SvgSystemLanguage, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "shape-rendering", NamespaceId::Svg, NativeProperty::SvgShapeRendering, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "text-rendering", NamespaceId::Svg, NativeProperty::SvgTextRendering, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "image-rendering", NamespaceId::Svg, NativeProperty::SvgImageRendering, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "color-interpolation", NamespaceId::Svg, NativeProperty::SvgColorInterpolation, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "color-interpolation-filters", NamespaceId::Svg, NativeProperty::SvgColorInterpolationFilters, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "color-rendering", NamespaceId::Svg, NativeProperty::SvgColorRendering, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Paint", nullptr, 0 },
        { "cursor", NamespaceId::Svg, NativeProperty::SvgCursor, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Visibility", nullptr, 0 },
        { "style", NamespaceId::Svg, NativeProperty::ForbiddenInlineStyle, AttributeFlagNamespaceDefault | AttributeFlagSvg | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "Metadata", nullptr, 0 },
        { "display", NamespaceId::MathMl, NativeProperty::MathDisplay, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathDisplay", AttributeElements461, 1 },
        { "mathcolor", NamespaceId::MathMl, NativeProperty::MathColor, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathColor", nullptr, 0 },
        { "mathbackground", NamespaceId::MathMl, NativeProperty::MathBackground, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathBackground", nullptr, 0 },
        { "mathsize", NamespaceId::MathMl, NativeProperty::MathSize, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSize", nullptr, 0 },
        { "displaystyle", NamespaceId::MathMl, NativeProperty::MathDisplayStyle, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathStyle", nullptr, 0 },
        { "scriptlevel", NamespaceId::MathMl, NativeProperty::MathScriptLevel, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathScriptLevel", AttributeElements466, 15 },
        { "mathvariant", NamespaceId::MathMl, NativeProperty::MathVariant, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathStyle", nullptr, 0 },
        { "dir", NamespaceId::MathMl, NativeProperty::TextDirection, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathStyle", AttributeElements468, 1 },
        { "href", NamespaceId::MathMl, NativeProperty::MathHref, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathMetadata", nullptr, 0 },
        { "linethickness", NamespaceId::MathMl, NativeProperty::MathLineThickness, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathFraction", AttributeElements470, 1 },
        { "numalign", NamespaceId::MathMl, NativeProperty::MathNumalign, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathFraction", AttributeElements471, 1 },
        { "denomalign", NamespaceId::MathMl, NativeProperty::MathDenomalign, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathFraction", AttributeElements472, 1 },
        { "bevelled", NamespaceId::MathMl, NativeProperty::MathBevelled, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathFraction", AttributeElements473, 1 },
        { "accent", NamespaceId::MathMl, NativeProperty::MathAccent, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements474, 3 },
        { "accentunder", NamespaceId::MathMl, NativeProperty::MathAccentUnder, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements475, 3 },
        { "stretchy", NamespaceId::MathMl, NativeProperty::MathStretchy, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements476, 1 },
        { "symmetric", NamespaceId::MathMl, NativeProperty::MathSymmetric, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements477, 1 },
        { "movablelimits", NamespaceId::MathMl, NativeProperty::MathMovableLimits, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements478, 4 },
        { "form", NamespaceId::MathMl, NativeProperty::MathForm, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements479, 1 },
        { "fence", NamespaceId::MathMl, NativeProperty::MathFence, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements480, 1 },
        { "separator", NamespaceId::MathMl, NativeProperty::MathSeparator, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements481, 1 },
        { "largeop", NamespaceId::MathMl, NativeProperty::MathLargeOperator, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements482, 1 },
        { "lspace", NamespaceId::MathMl, NativeProperty::MathLeftSpace, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements483, 2 },
        { "rspace", NamespaceId::MathMl, NativeProperty::MathRightSpace, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements484, 2 },
        { "minsize", NamespaceId::MathMl, NativeProperty::MathMinSize, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements485, 1 },
        { "maxsize", NamespaceId::MathMl, NativeProperty::MathMaxSize, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements486, 1 },
        { "columnalign", NamespaceId::MathMl, NativeProperty::MathColumnAlign, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements487, 3 },
        { "rowalign", NamespaceId::MathMl, NativeProperty::MathRowAlign, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements488, 3 },
        { "columnspacing", NamespaceId::MathMl, NativeProperty::MathColumnSpacing, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements489, 1 },
        { "rowspacing", NamespaceId::MathMl, NativeProperty::MathRowSpacing, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements490, 1 },
        { "columnlines", NamespaceId::MathMl, NativeProperty::MathColumnLines, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements491, 1 },
        { "rowlines", NamespaceId::MathMl, NativeProperty::MathRowLines, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements492, 1 },
        { "frame", NamespaceId::MathMl, NativeProperty::MathTableFrame, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements493, 1 },
        { "framespacing", NamespaceId::MathMl, NativeProperty::MathTableFrameSpacing, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements494, 1 },
        { "equalrows", NamespaceId::MathMl, NativeProperty::MathTableEqualRows, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements495, 1 },
        { "equalcolumns", NamespaceId::MathMl, NativeProperty::MathTableEqualColumns, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements496, 1 },
        { "columnwidth", NamespaceId::MathMl, NativeProperty::MathTableColumnWidth, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements497, 1 },
        { "rowspan", NamespaceId::MathMl, NativeProperty::MathRowSpan, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements498, 1 },
        { "columnspan", NamespaceId::MathMl, NativeProperty::MathColumnSpan, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements499, 1 },
        { "width", NamespaceId::MathMl, NativeProperty::MathWidth, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSize", AttributeElements500, 3 },
        { "height", NamespaceId::MathMl, NativeProperty::MathHeight, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSize", AttributeElements501, 2 },
        { "depth", NamespaceId::MathMl, NativeProperty::MathDepth, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSize", AttributeElements502, 2 },
        { "voffset", NamespaceId::MathMl, NativeProperty::MathDepth, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSize", AttributeElements503, 1 },
        { "notation", NamespaceId::MathMl, NativeProperty::MathNotation, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSemantics", AttributeElements504, 1 },
        { "encoding", NamespaceId::MathMl, NativeProperty::MathEncoding, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSemantics", AttributeElements505, 2 },
        { "definitionURL", NamespaceId::MathMl, NativeProperty::MathDefinitionUrl, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSemantics", AttributeElements506, 1 },
        { "src", NamespaceId::MathMl, NativeProperty::ResourceSource, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathSemantics", AttributeElements507, 1 },
        { "actiontype", NamespaceId::MathMl, NativeProperty::MathActionType, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathAction", AttributeElements508, 1 },
        { "selection", NamespaceId::MathMl, NativeProperty::MathActionSelection, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathAction", AttributeElements509, 1 },
        { "open", NamespaceId::MathMl, NativeProperty::MathOpen, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements510, 1 },
        { "close", NamespaceId::MathMl, NativeProperty::MathClose, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathOperator", AttributeElements511, 1 },
        { "charalign", NamespaceId::MathMl, NativeProperty::MathCharAlign, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements512, 1 },
        { "charspacing", NamespaceId::MathMl, NativeProperty::MathCharSpacing, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathTable", AttributeElements513, 1 },
        { "alttext", NamespaceId::MathMl, NativeProperty::MathAltText, AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathAccessibility", AttributeElements514, 1 },
        { "id", NamespaceId::MathMl, NativeProperty::IdentityId, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathIdentity", nullptr, 0 },
        { "class", NamespaceId::MathMl, NativeProperty::StyleReference, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathIdentity", nullptr, 0 },
        { "style", NamespaceId::MathMl, NativeProperty::ForbiddenInlineStyle, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagForbidden | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathIdentity", nullptr, 0 },
        { "title", NamespaceId::MathMl, NativeProperty::MetadataTitle, AttributeFlagNamespaceDefault | AttributeFlagMathMl | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "MathMetadata", nullptr, 0 },
        { "role", NamespaceId::Html, NativeProperty::AccessibilityRole, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "TokenList", "ARIA", nullptr, 0 },
        { "aria-activedescendant", NamespaceId::Html, NativeProperty::AccessibilityActiveDescendant, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReference", "ARIA", nullptr, 0 },
        { "aria-atomic", NamespaceId::Html, NativeProperty::AccessibilityAtomic, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-autocomplete", NamespaceId::Html, NativeProperty::AccessibilityAutoComplete, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-braillelabel", NamespaceId::Html, NativeProperty::AccessibilityLabel, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-brailleroledescription", NamespaceId::Html, NativeProperty::AccessibilityRoleDescription, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-busy", NamespaceId::Html, NativeProperty::AccessibilityBusy, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-checked", NamespaceId::Html, NativeProperty::AccessibilityChecked, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Tristate", "ARIA", nullptr, 0 },
        { "aria-colcount", NamespaceId::Html, NativeProperty::AccessibilityColumnCount, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-colindex", NamespaceId::Html, NativeProperty::AccessibilityColumnIndex, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-colindextext", NamespaceId::Html, NativeProperty::AccessibilityColumnIndexText, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-colspan", NamespaceId::Html, NativeProperty::AccessibilityColumnSpan, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-controls", NamespaceId::Html, NativeProperty::AccessibilityControls, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReferenceList", "ARIA", nullptr, 0 },
        { "aria-current", NamespaceId::Html, NativeProperty::AccessibilityCurrent, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-describedby", NamespaceId::Html, NativeProperty::AccessibilityDescribedBy, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReferenceList", "ARIA", nullptr, 0 },
        { "aria-description", NamespaceId::Html, NativeProperty::AccessibilityDescription, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-details", NamespaceId::Html, NativeProperty::AccessibilityDetails, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReference", "ARIA", nullptr, 0 },
        { "aria-disabled", NamespaceId::Html, NativeProperty::AccessibilityDisabled, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-dropeffect", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "TokenList", "ARIA", nullptr, 0 },
        { "aria-errormessage", NamespaceId::Html, NativeProperty::AccessibilityErrorMessage, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReference", "ARIA", nullptr, 0 },
        { "aria-expanded", NamespaceId::Html, NativeProperty::AccessibilityExpanded, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Tristate", "ARIA", nullptr, 0 },
        { "aria-flowto", NamespaceId::Html, NativeProperty::AccessibilityFlowTo, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReferenceList", "ARIA", nullptr, 0 },
        { "aria-grabbed", NamespaceId::Html, NativeProperty::Unmapped, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagObsolete | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Tristate", "ARIA", nullptr, 0 },
        { "aria-haspopup", NamespaceId::Html, NativeProperty::AccessibilityHasPopup, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-hidden", NamespaceId::Html, NativeProperty::AccessibilityHidden, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Tristate", "ARIA", nullptr, 0 },
        { "aria-invalid", NamespaceId::Html, NativeProperty::AccessibilityInvalid, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-keyshortcuts", NamespaceId::Html, NativeProperty::AccessibilityKeyShortcuts, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-label", NamespaceId::Html, NativeProperty::AccessibilityLabel, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-labelledby", NamespaceId::Html, NativeProperty::AccessibilityLabelledBy, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReferenceList", "ARIA", nullptr, 0 },
        { "aria-level", NamespaceId::Html, NativeProperty::AccessibilityLevel, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-live", NamespaceId::Html, NativeProperty::AccessibilityLive, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-modal", NamespaceId::Html, NativeProperty::AccessibilityModal, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-multiline", NamespaceId::Html, NativeProperty::AccessibilityMultiLine, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-multiselectable", NamespaceId::Html, NativeProperty::AccessibilityMultiSelectable, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-orientation", NamespaceId::Html, NativeProperty::AccessibilityOrientation, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-owns", NamespaceId::Html, NativeProperty::AccessibilityOwns, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "IdReferenceList", "ARIA", nullptr, 0 },
        { "aria-placeholder", NamespaceId::Html, NativeProperty::AccessibilityPlaceholder, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-posinset", NamespaceId::Html, NativeProperty::AccessibilityPositionInSet, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-pressed", NamespaceId::Html, NativeProperty::AccessibilityPressed, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Tristate", "ARIA", nullptr, 0 },
        { "aria-readonly", NamespaceId::Html, NativeProperty::AccessibilityReadOnly, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-relevant", NamespaceId::Html, NativeProperty::AccessibilityRelevant, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "TokenList", "ARIA", nullptr, 0 },
        { "aria-required", NamespaceId::Html, NativeProperty::AccessibilityRequired, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Boolean", "ARIA", nullptr, 0 },
        { "aria-roledescription", NamespaceId::Html, NativeProperty::AccessibilityRoleDescription, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-rowcount", NamespaceId::Html, NativeProperty::AccessibilityRowCount, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-rowindex", NamespaceId::Html, NativeProperty::AccessibilityRowIndex, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-rowindextext", NamespaceId::Html, NativeProperty::AccessibilityRowIndexText, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
        { "aria-rowspan", NamespaceId::Html, NativeProperty::AccessibilityRowSpan, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-selected", NamespaceId::Html, NativeProperty::AccessibilitySelected, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Tristate", "ARIA", nullptr, 0 },
        { "aria-setsize", NamespaceId::Html, NativeProperty::AccessibilitySetSize, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Integer", "ARIA", nullptr, 0 },
        { "aria-sort", NamespaceId::Html, NativeProperty::AccessibilitySort, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Token", "ARIA", nullptr, 0 },
        { "aria-valuemax", NamespaceId::Html, NativeProperty::AccessibilityValueMaximum, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Number", "ARIA", nullptr, 0 },
        { "aria-valuemin", NamespaceId::Html, NativeProperty::AccessibilityValueMinimum, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagBrowserPreserved, "Number", "ARIA", nullptr, 0 },
        { "aria-valuenow", NamespaceId::Html, NativeProperty::AccessibilityValueNow, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "Number", "ARIA", nullptr, 0 },
        { "aria-valuetext", NamespaceId::Html, NativeProperty::AccessibilityValueText, AttributeFlagNamespaceDefault | AttributeFlagAria | AttributeFlagCompileTime | AttributeFlagRuntimeMutable | AttributeFlagBrowserPreserved, "String", "ARIA", nullptr, 0 },
    };

    const std::uint32_t AttributeCount = 573;

    const InputTypeRecord InputTypeTable[] = {
        { "text", ElementKind::Input, AttributeFlagNone },
        { "email", ElementKind::Input, AttributeFlagNone },
        { "password", ElementKind::Input, AttributeFlagNone },
        { "number", ElementKind::NumberInput, AttributeFlagNone },
        { "date", ElementKind::DateInput, AttributeFlagNone },
        { "datetime-local", ElementKind::DateTimeInput, AttributeFlagNone },
        { "month", ElementKind::MonthInput, AttributeFlagNone },
        { "week", ElementKind::WeekInput, AttributeFlagNone },
        { "time", ElementKind::TimeInput, AttributeFlagNone },
        { "file", ElementKind::FileInput, AttributeFlagNone },
        { "checkbox", ElementKind::Checkbox, AttributeFlagNone },
        { "radio", ElementKind::Radio, AttributeFlagNone },
        { "range", ElementKind::Range, AttributeFlagNone },
        { "color", ElementKind::ColorInput, AttributeFlagNone },
        { "search", ElementKind::SearchInput, AttributeFlagNone },
        { "url", ElementKind::UrlInput, AttributeFlagNone },
        { "tel", ElementKind::TelephoneInput, AttributeFlagNone },
        { "hidden", ElementKind::HiddenInput, AttributeFlagNone },
        { "submit", ElementKind::SubmitButton, AttributeFlagNone },
        { "reset", ElementKind::ResetButton, AttributeFlagNone },
        { "button", ElementKind::Button, AttributeFlagNone },
        { "image", ElementKind::ImageButton, AttributeFlagNone },
        { "datetime", ElementKind::DateTimeInput, AttributeFlagObsolete },
    };

    const std::uint32_t InputTypeCount = 23;
}
