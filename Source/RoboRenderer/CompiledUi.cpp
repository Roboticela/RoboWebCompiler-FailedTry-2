#include "RoboRenderer/CompiledUi.h"

#include "RoboRenderer/UiDiagnostics.h"

#include <cstdio>
#include <cstring>

namespace RoboRenderer
{
    namespace
    {
        ElementKindNameFunction elementKindNames = nullptr;
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    const char* StringAt(const CompiledDocument& document, StringIndex index)
    {
        if (document.strings == nullptr || index >= document.stringCount)
        {
            return "";
        }

        const char* value = document.strings[index];
        return value != nullptr ? value : "";
    }

    const CompiledElement* ElementAt(const CompiledDocument& document, ElementIndex index)
    {
        if (document.elements == nullptr || index == NoElement || index >= document.elementCount)
        {
            return nullptr;
        }

        return &document.elements[index];
    }

    ElementIndex FindElementById(const CompiledDocument& document, const char* identifier)
    {
        if (identifier == nullptr || identifier[0] == '\0' || document.identifiers == nullptr)
        {
            return NoElement;
        }

        std::uint32_t low = 0;
        std::uint32_t high = document.identifierCount;

        while (low < high)
        {
            const std::uint32_t middle = low + (high - low) / 2;
            const int order = std::strcmp(StringAt(document, document.identifiers[middle].identifier), identifier);

            if (order == 0)
            {
                return document.identifiers[middle].element;
            }

            if (order < 0)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }

        return NoElement;
    }

    ElementIndex NextInDocumentOrder(const CompiledDocument& document, ElementIndex index)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return NoElement;
        }

        if (element->firstChild != NoElement)
        {
            return element->firstChild;
        }

        ElementIndex current = index;
        while (current != NoElement)
        {
            const CompiledElement* node = ElementAt(document, current);
            if (node == nullptr)
            {
                return NoElement;
            }

            if (node->nextSibling != NoElement)
            {
                return node->nextSibling;
            }

            current = node->parent;
        }

        return NoElement;
    }

    ElementIndex PreviousInDocumentOrder(const CompiledDocument& document, ElementIndex index)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr || index == document.root)
        {
            return NoElement;
        }

        if (element->parent == NoElement)
        {
            return NoElement;
        }

        const CompiledElement* parent = ElementAt(document, element->parent);
        if (parent == nullptr)
        {
            return NoElement;
        }

        // Before a first child comes the parent itself; before any later sibling
        // comes the deepest last descendant of the previous one.
        if (parent->firstChild == index)
        {
            return element->parent;
        }

        ElementIndex previous = parent->firstChild;
        for (ElementIndex sibling = previous; sibling != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, sibling);
            if (node == nullptr || node->nextSibling == index)
            {
                break;
            }

            previous = node->nextSibling;
            sibling = previous;
        }

        while (true)
        {
            const CompiledElement* node = ElementAt(document, previous);
            if (node == nullptr || node->lastChild == NoElement)
            {
                return previous;
            }

            previous = node->lastChild;
        }
    }

    ElementIndex LastInDocumentOrder(const CompiledDocument& document)
    {
        ElementIndex index = document.root;

        while (true)
        {
            const CompiledElement* element = ElementAt(document, index);
            if (element == nullptr || element->lastChild == NoElement)
            {
                return index;
            }

            index = element->lastChild;
        }
    }

    std::uint32_t ChildCount(const CompiledDocument& document, ElementIndex index)
    {
        const CompiledElement* element = ElementAt(document, index);
        if (element == nullptr)
        {
            return 0;
        }

        std::uint32_t count = 0;
        for (ElementIndex child = element->firstChild; child != NoElement;)
        {
            const CompiledElement* node = ElementAt(document, child);
            if (node == nullptr)
            {
                break;
            }

            ++count;
            child = node->nextSibling;
        }

        return count;
    }

    // -----------------------------------------------------------------------
    // Names
    // -----------------------------------------------------------------------

    void ConfigureElementKindNames(ElementKindNameFunction function)
    {
        elementKindNames = function;
    }

    const char* ElementKindName(ElementKind kind)
    {
        if (elementKindNames != nullptr)
        {
            const char* name = elementKindNames(kind);
            if (name != nullptr)
            {
                return name;
            }
        }

        // The renderer does not embed the HTML vocabulary: a diagnostic name
        // table is compiler knowledge. Applications that link the compiler's
        // generated registries install it through ConfigureElementKindNames.
        static thread_local char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "ElementKind#%u", static_cast<unsigned>(kind));
        return buffer;
    }

    const char* ToString(EventKind event)
    {
        switch (event)
        {
        case EventKind::Unknown: return "Unknown";
        case EventKind::Click: return "Click";
        case EventKind::DoubleClick: return "DoubleClick";
        case EventKind::PointerDown: return "PointerDown";
        case EventKind::PointerUp: return "PointerUp";
        case EventKind::PointerMove: return "PointerMove";
        case EventKind::PointerEnter: return "PointerEnter";
        case EventKind::PointerLeave: return "PointerLeave";
        case EventKind::Wheel: return "Wheel";
        case EventKind::KeyDown: return "KeyDown";
        case EventKind::KeyUp: return "KeyUp";
        case EventKind::Focus: return "Focus";
        case EventKind::Blur: return "Blur";
        case EventKind::InputEvent: return "Input";
        case EventKind::Change: return "Change";
        case EventKind::Submit: return "Submit";
        case EventKind::Reset: return "Reset";
        case EventKind::Toggle: return "Toggle";
        case EventKind::Scroll: return "Scroll";
        case EventKind::Load: return "Load";
        case EventKind::ErrorEvent: return "Error";
        }

        return "Unknown";
    }

    const char* ToString(ResourceKind kind)
    {
        switch (kind)
        {
        case ResourceKind::None: return "None";
        case ResourceKind::Image: return "Image";
        case ResourceKind::Media: return "Media";
        case ResourceKind::Font: return "Font";
        case ResourceKind::StyleSheet: return "StyleSheet";
        case ResourceKind::Script: return "Script";
        case ResourceKind::Document: return "Document";
        }

        return "None";
    }

    const char* ToString(RenderCategory category)
    {
        switch (category)
        {
        case RenderCategory::None: return "None";
        case RenderCategory::Document: return "Document";
        case RenderCategory::Container: return "Container";
        case RenderCategory::Text: return "Text";
        case RenderCategory::PreformattedText: return "PreformattedText";
        case RenderCategory::Heading: return "Heading";
        case RenderCategory::LineBreak: return "LineBreak";
        case RenderCategory::Separator: return "Separator";
        case RenderCategory::Link: return "Link";
        case RenderCategory::List: return "List";
        case RenderCategory::ListItem: return "ListItem";
        case RenderCategory::DescriptionTerm: return "DescriptionTerm";
        case RenderCategory::DescriptionDescription: return "DescriptionDescription";
        case RenderCategory::Figure: return "Figure";
        case RenderCategory::FigureCaption: return "FigureCaption";
        case RenderCategory::Image: return "Image";
        case RenderCategory::Media: return "Media";
        case RenderCategory::Canvas: return "Canvas";
        case RenderCategory::Form: return "Form";
        case RenderCategory::FieldSet: return "FieldSet";
        case RenderCategory::Legend: return "Legend";
        case RenderCategory::Label: return "Label";
        case RenderCategory::Button: return "Button";
        case RenderCategory::Input: return "Input";
        case RenderCategory::Checkbox: return "Checkbox";
        case RenderCategory::Radio: return "Radio";
        case RenderCategory::Range: return "Range";
        case RenderCategory::ColorInput: return "ColorInput";
        case RenderCategory::FileInput: return "FileInput";
        case RenderCategory::TextArea: return "TextArea";
        case RenderCategory::Select: return "Select";
        case RenderCategory::Option: return "Option";
        case RenderCategory::OptionGroup: return "OptionGroup";
        case RenderCategory::Output: return "Output";
        case RenderCategory::Progress: return "Progress";
        case RenderCategory::Meter: return "Meter";
        case RenderCategory::Table: return "Table";
        case RenderCategory::TableCaption: return "TableCaption";
        case RenderCategory::TableColumn: return "TableColumn";
        case RenderCategory::TableSection: return "TableSection";
        case RenderCategory::TableRow: return "TableRow";
        case RenderCategory::TableCell: return "TableCell";
        case RenderCategory::Disclosure: return "Disclosure";
        case RenderCategory::DisclosureSummary: return "DisclosureSummary";
        case RenderCategory::Dialog: return "Dialog";
        case RenderCategory::Frame: return "Frame";
        case RenderCategory::EmbeddedObject: return "EmbeddedObject";
        case RenderCategory::Ruby: return "Ruby";
        case RenderCategory::RubyText: return "RubyText";
        case RenderCategory::CustomElement: return "CustomElement";
        case RenderCategory::UnsupportedNamespace: return "UnsupportedNamespace";
        }

        return "None";
    }

    // -----------------------------------------------------------------------
    // Categories
    // -----------------------------------------------------------------------

    RenderCategory CategoryFor(ElementKind kind)
    {
        switch (kind)
        {
        // Document frame and compile-time metadata. Metadata elements never
        // reach the runtime tree, but a body-level one is skipped rather than
        // guessed at.
        case ElementKind::None: return RenderCategory::None;
        case ElementKind::Document: return RenderCategory::Document;
        case ElementKind::Metadata:
        case ElementKind::DocumentTitle:
        case ElementKind::BaseUrl:
        case ElementKind::ResourceLink:
        case ElementKind::StyleSheet:
        case ElementKind::ScriptResource:
        case ElementKind::WordBreakOpportunity:
        case ElementKind::ImageMap:
        case ElementKind::MediaSource:
        case ElementKind::MediaTrack:
        case ElementKind::DataList:
        case ElementKind::SelectedContent:
        case ElementKind::EmbeddedParameter:
        case ElementKind::Template:
        case ElementKind::RubyParenthesis:
        case ElementKind::ObsoleteElement:
            return RenderCategory::None;

        // <noscript> renders its content: the runtime has no scripting engine,
        // so the fallback is what a document should show.
        case ElementKind::NoScript: return RenderCategory::Container;

        case ElementKind::Container: return RenderCategory::Container;
        case ElementKind::Text: return RenderCategory::Text;
        case ElementKind::Quote: return RenderCategory::Container;
        case ElementKind::PreformattedText: return RenderCategory::PreformattedText;
        case ElementKind::Heading: return RenderCategory::Heading;
        case ElementKind::LineBreak: return RenderCategory::LineBreak;
        case ElementKind::Separator: return RenderCategory::Separator;

        case ElementKind::Link:
        case ElementKind::ImageMapLink:
            return RenderCategory::Link;

        case ElementKind::List:
        case ElementKind::DescriptionList:
            return RenderCategory::List;
        case ElementKind::ListItem: return RenderCategory::ListItem;
        case ElementKind::DescriptionTerm: return RenderCategory::DescriptionTerm;
        case ElementKind::DescriptionDescription: return RenderCategory::DescriptionDescription;

        case ElementKind::Figure: return RenderCategory::Figure;
        case ElementKind::FigureCaption: return RenderCategory::FigureCaption;
        case ElementKind::Image: return RenderCategory::Image;
        // <picture> is a container of sources and one <img>.
        case ElementKind::ResponsiveImage: return RenderCategory::Container;

        case ElementKind::Audio:
        case ElementKind::Video:
            return RenderCategory::Media;
        case ElementKind::Canvas: return RenderCategory::Canvas;

        case ElementKind::Form: return RenderCategory::Form;
        case ElementKind::FieldSet: return RenderCategory::FieldSet;
        case ElementKind::Legend: return RenderCategory::Legend;
        case ElementKind::Label: return RenderCategory::Label;

        case ElementKind::Button:
        case ElementKind::SubmitButton:
        case ElementKind::ResetButton:
        case ElementKind::ImageButton:
            return RenderCategory::Button;

        case ElementKind::Input:
        case ElementKind::NumberInput:
        case ElementKind::DateInput:
        case ElementKind::DateTimeInput:
        case ElementKind::MonthInput:
        case ElementKind::WeekInput:
        case ElementKind::TimeInput:
        case ElementKind::SearchInput:
        case ElementKind::UrlInput:
        case ElementKind::TelephoneInput:
            return RenderCategory::Input;
        case ElementKind::HiddenInput: return RenderCategory::None;

        case ElementKind::Checkbox: return RenderCategory::Checkbox;
        case ElementKind::Radio: return RenderCategory::Radio;
        case ElementKind::Range: return RenderCategory::Range;
        case ElementKind::ColorInput: return RenderCategory::ColorInput;
        case ElementKind::FileInput: return RenderCategory::FileInput;
        case ElementKind::TextArea: return RenderCategory::TextArea;
        case ElementKind::Select: return RenderCategory::Select;
        case ElementKind::Option: return RenderCategory::Option;
        case ElementKind::OptionGroup: return RenderCategory::OptionGroup;
        case ElementKind::Output: return RenderCategory::Output;
        case ElementKind::Progress: return RenderCategory::Progress;
        case ElementKind::Meter: return RenderCategory::Meter;

        case ElementKind::Table: return RenderCategory::Table;
        case ElementKind::TableCaption: return RenderCategory::TableCaption;
        case ElementKind::TableColumnGroup:
        case ElementKind::TableColumn:
            return RenderCategory::TableColumn;
        case ElementKind::TableSection: return RenderCategory::TableSection;
        case ElementKind::TableRow: return RenderCategory::TableRow;
        case ElementKind::TableCell: return RenderCategory::TableCell;
        case ElementKind::TableHeaderCell: return RenderCategory::TableCell;

        case ElementKind::Disclosure: return RenderCategory::Disclosure;
        case ElementKind::DisclosureSummary: return RenderCategory::DisclosureSummary;
        case ElementKind::Dialog: return RenderCategory::Dialog;
        case ElementKind::Frame: return RenderCategory::Frame;

        case ElementKind::EmbeddedContent:
        case ElementKind::EmbeddedObject:
            return RenderCategory::EmbeddedObject;

        // <slot> in a document without a shadow tree renders its children.
        case ElementKind::Slot: return RenderCategory::Container;

        case ElementKind::Ruby: return RenderCategory::Ruby;
        case ElementKind::RubyText: return RenderCategory::RubyText;

        case ElementKind::CustomElement: return RenderCategory::CustomElement;

        // SVG and MathML are separate source namespaces with their own native
        // categories. They are outside the HTML-only runtime phase; the renderer
        // reports every occurrence instead of pretending they rendered.
        case ElementKind::SvgDocument:
        case ElementKind::SvgContainer:
        case ElementKind::SvgShape:
        case ElementKind::SvgPath:
        case ElementKind::SvgText:
        case ElementKind::SvgImage:
        case ElementKind::SvgUse:
        case ElementKind::SvgGradient:
        case ElementKind::SvgStop:
        case ElementKind::SvgPattern:
        case ElementKind::SvgMarker:
        case ElementKind::SvgClipPath:
        case ElementKind::SvgMask:
        case ElementKind::SvgFilter:
        case ElementKind::SvgFilterPrimitive:
        case ElementKind::SvgAnimation:
        case ElementKind::SvgForeignObject:
        case ElementKind::SvgMetadata:
        case ElementKind::MathRoot:
        case ElementKind::MathToken:
        case ElementKind::MathOperator:
        case ElementKind::MathRow:
        case ElementKind::MathFraction:
        case ElementKind::MathScript:
        case ElementKind::MathMultiScripts:
        case ElementKind::MathPrescript:
        case ElementKind::MathRadical:
        case ElementKind::MathTable:
        case ElementKind::MathTableRow:
        case ElementKind::MathTableCell:
        case ElementKind::MathSpace:
        case ElementKind::MathPadded:
        case ElementKind::MathStyle:
        case ElementKind::MathAction:
        case ElementKind::MathError:
        case ElementKind::MathPhantom:
        case ElementKind::MathSemantics:
        case ElementKind::MathAnnotation:
            return RenderCategory::UnsupportedNamespace;
        }

        // A category the renderer has never seen: never guess, report it.
        return RenderCategory::UnsupportedNamespace;
    }

    // -----------------------------------------------------------------------
    // Diagnostics
    // -----------------------------------------------------------------------

    const char* ToString(RendererDiagnosticCode code)
    {
        switch (code)
        {
        case RendererDiagnosticCode::InvalidElementId: return "RRE001";
        case RendererDiagnosticCode::InvalidChildIndex: return "RRE002";
        case RendererDiagnosticCode::InvalidResourceId: return "RRE003";
        case RendererDiagnosticCode::UnsupportedElementKind: return "RRE004";
        case RendererDiagnosticCode::InvalidLayoutData: return "RRE005";
        case RendererDiagnosticCode::RenderingBackendFailure: return "RRE006";
        case RendererDiagnosticCode::UnknownEventBinding: return "RRE007";
        case RendererDiagnosticCode::InvalidStringIndex: return "RRE008";
        case RendererDiagnosticCode::InvalidStyleIndex: return "RRE009";
        case RendererDiagnosticCode::ResourceLoadFailure: return "RRE010";
        case RendererDiagnosticCode::InvalidDocument: return "RRE011";
        case RendererDiagnosticCode::DuplicateIdentifier: return "RRE012";
        case RendererDiagnosticCode::OrphanedElement: return "RRE013";
        }

        return "RRE???";
    }

    const char* ToString(RendererSeverity severity)
    {
        return severity == RendererSeverity::Error ? "error" : "warning";
    }

    void RendererDiagnostics::Report(RendererDiagnosticCode code,
                                     RendererSeverity severity,
                                     ElementIndex element,
                                     std::string message,
                                     std::string detail)
    {
        if (severity == RendererSeverity::Error)
        {
            ++errorCount;
        }
        else
        {
            ++warningCount;
        }

        if (entries.size() >= MaxEntries)
        {
            ++suppressedCount;
            return;
        }

        RendererDiagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.severity = severity;
        diagnostic.element = element;
        diagnostic.message = std::move(message);
        diagnostic.detail = std::move(detail);
        entries.push_back(std::move(diagnostic));
    }

    void RendererDiagnostics::Error(RendererDiagnosticCode code,
                                    ElementIndex element,
                                    std::string message,
                                    std::string detail)
    {
        Report(code, RendererSeverity::Error, element, std::move(message), std::move(detail));
    }

    void RendererDiagnostics::Warning(RendererDiagnosticCode code,
                                      ElementIndex element,
                                      std::string message,
                                      std::string detail)
    {
        Report(code, RendererSeverity::Warning, element, std::move(message), std::move(detail));
    }

    void RendererDiagnostics::Clear()
    {
        entries.clear();
        errorCount = 0;
        warningCount = 0;
        suppressedCount = 0;
    }

    std::string RendererDiagnostics::Format() const
    {
        std::string result;

        char buffer[128];
        for (const RendererDiagnostic& diagnostic : entries)
        {
            std::snprintf(buffer, sizeof(buffer), "%s: %s: ",
                          ToString(diagnostic.code), ToString(diagnostic.severity));
            result += buffer;

            if (diagnostic.element != NoElement)
            {
                std::snprintf(buffer, sizeof(buffer), "element #%u: ", diagnostic.element);
                result += buffer;
            }

            result += diagnostic.message;
            if (!diagnostic.detail.empty())
            {
                result += " (";
                result += diagnostic.detail;
                result += ")";
            }
            result += "\n";
        }

        if (suppressedCount != 0)
        {
            std::snprintf(buffer, sizeof(buffer), "%zu further diagnostics suppressed\n", suppressedCount);
            result += buffer;
        }

        return result;
    }
}
