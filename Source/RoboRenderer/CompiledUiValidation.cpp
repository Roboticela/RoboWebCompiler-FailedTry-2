#include "RoboRenderer/UiDiagnostics.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace RoboRenderer
{
    namespace
    {
        /// Highest native category the compiled format can carry. It is the last
        /// enumerator of the compiler's vocabulary, so a kind above it is not a
        /// category the compiler could have emitted: the value is data the
        /// renderer must not reinterpret.
        constexpr std::uint16_t MaxElementKind = static_cast<std::uint16_t>(ElementKind::MathAnnotation);

        bool IsUsableLength(float value)
        {
            return std::isfinite(value) && value >= 0.0f;
        }

        bool ValidateStringIndex(const CompiledDocument& document,
                                 RendererDiagnostics& diagnostics,
                                 StringIndex index,
                                 ElementIndex element,
                                 const char* field)
        {
            if (index == 0)
            {
                return true;
            }

            if (document.strings == nullptr || index >= document.stringCount)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidStringIndex,
                                  element,
                                  std::string("String index out of range for ") + field,
                                  "index " + std::to_string(index) + " of " + std::to_string(document.stringCount));
                return false;
            }

            return true;
        }
    }

    bool ValidateCompiledDocument(const CompiledDocument& document, RendererDiagnostics& diagnostics)
    {
        if (document.elements == nullptr || document.elementCount == 0)
        {
            diagnostics.Error(RendererDiagnosticCode::InvalidDocument,
                              NoElement,
                              "The compiled document contains no elements.",
                              "A document must have at least one element (the root).");
            return false;
        }

        if (document.elementCount > MaxElements)
        {
            diagnostics.Error(RendererDiagnosticCode::InvalidDocument,
                              NoElement,
                              "The compiled document holds more elements than the index range allows.",
                              std::to_string(document.elementCount) + " elements");
            return false;
        }

        if (document.root == NoElement || document.root >= document.elementCount)
        {
            diagnostics.Error(RendererDiagnosticCode::InvalidElementId,
                              NoElement,
                              "The compiled document has no valid root element.",
                              "root index " + std::to_string(document.root));
            return false;
        }

        if (document.strings == nullptr && document.stringCount != 0)
        {
            diagnostics.Error(RendererDiagnosticCode::InvalidStringIndex,
                              NoElement,
                              "The compiled document declares strings but carries no table.");
        }

        if (document.resources == nullptr && document.resourceCount != 0)
        {
            diagnostics.Error(RendererDiagnosticCode::InvalidResourceId,
                              NoElement,
                              "The compiled document declares resources but carries no table.");
        }

        if (document.styles == nullptr && document.styleCount != 0)
        {
            diagnostics.Error(RendererDiagnosticCode::InvalidStyleIndex,
                              NoElement,
                              "The compiled document declares styles but carries no table.");
        }

        if (document.bindings == nullptr && document.bindingCount != 0)
        {
            diagnostics.Error(RendererDiagnosticCode::UnknownEventBinding,
                              NoElement,
                              "The compiled document declares bindings but carries no table.");
        }

        if (document.identifiers == nullptr && document.identifierCount != 0)
        {
            diagnostics.Error(RendererDiagnosticCode::DuplicateIdentifier,
                              NoElement,
                              "The compiled document declares identifiers but carries no table.");
        }

        for (std::uint32_t index = 0; index < document.elementCount; ++index)
        {
            const CompiledElement& element = document.elements[index];
            const ElementIndex elementIndex = index;

            if (static_cast<std::uint16_t>(element.kind) > MaxElementKind)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidElementId,
                                  elementIndex,
                                  "The element carries an unknown native category.",
                                  "kind value " + std::to_string(static_cast<unsigned>(element.kind)));
                continue;
            }

            if (CategoryFor(element.kind) == RenderCategory::UnsupportedNamespace)
            {
                diagnostics.Warning(RendererDiagnosticCode::UnsupportedElementKind,
                                    elementIndex,
                                    std::string("Category '") + ElementKindName(element.kind) +
                                        "' is outside the HTML-only runtime phase.",
                                    "the element and its subtree are recorded but not painted");
            }

            if (element.parent == NoElement)
            {
                if (elementIndex != document.root)
                {
                    diagnostics.Error(RendererDiagnosticCode::InvalidElementId,
                                      elementIndex,
                                      "A non-root element has no parent.",
                                      "only the root may have no parent");
                }
            }
            else if (element.parent >= document.elementCount)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidElementId,
                                  elementIndex,
                                  "The parent index is out of range.",
                                  "parent " + std::to_string(element.parent));
            }

            if (element.firstChild != NoElement)
            {
                if (element.firstChild >= document.elementCount)
                {
                    diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                      elementIndex,
                                      "The first-child index is out of range.",
                                      "firstChild " + std::to_string(element.firstChild));
                }
                if (element.lastChild == NoElement || element.lastChild >= document.elementCount)
                {
                    diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                      elementIndex,
                                      "The last-child index is out of range.",
                                      "lastChild " + std::to_string(element.lastChild));
                }
            }
            else if (element.lastChild != NoElement)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                  elementIndex,
                                  "The element has a last child but no first child.");
            }

            if (element.nextSibling != NoElement && element.nextSibling >= document.elementCount)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                  elementIndex,
                                  "The next-sibling index is out of range.",
                                  "nextSibling " + std::to_string(element.nextSibling));
            }

            ValidateStringIndex(document, diagnostics, element.text, elementIndex, "text");
            ValidateStringIndex(document, diagnostics, element.identifier, elementIndex, "id");
            ValidateStringIndex(document, diagnostics, element.name, elementIndex, "name");
            ValidateStringIndex(document, diagnostics, element.auxiliaryText, elementIndex, "auxiliary text");
            ValidateStringIndex(document, diagnostics, element.href, elementIndex, "href");
            ValidateStringIndex(document, diagnostics, element.labelTarget, elementIndex, "for");
            ValidateStringIndex(document, diagnostics, element.listStyle, elementIndex, "list style");

            if (element.resource != 0 &&
                (document.resources == nullptr || element.resource >= document.resourceCount))
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidResourceId,
                                  elementIndex,
                                  "The resource index is out of range.",
                                  "resource " + std::to_string(element.resource) + " of " +
                                      std::to_string(document.resourceCount));
            }

            if (element.styleIndex != 0 &&
                (document.styles == nullptr || element.styleIndex >= document.styleCount))
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidStyleIndex,
                                  elementIndex,
                                  "The style index is out of range.",
                                  "style " + std::to_string(element.styleIndex) + " of " +
                                      std::to_string(document.styleCount));
            }

            if (element.bindingCount != 0)
            {
                if (document.bindings == nullptr ||
                    static_cast<std::uint64_t>(element.firstBinding) + element.bindingCount > document.bindingCount)
                {
                    diagnostics.Error(RendererDiagnosticCode::UnknownEventBinding,
                                      elementIndex,
                                      "The element's binding range is out of range.",
                                      "first " + std::to_string(element.firstBinding) + " count " +
                                          std::to_string(element.bindingCount));
                }
            }

            // A span of zero is the compiled default of one, not a bad value: the
            // generated tables omit a span of one and the table layout treats
            // zero as "one". There is therefore no span the runtime must reject.

            if (element.headingLevel > 6)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidLayoutData,
                                  elementIndex,
                                  "The heading level is out of range.",
                                  "level " + std::to_string(element.headingLevel));
            }

            if (element.HasFlag(ElementFlagHasWidth) && !IsUsableLength(element.attributeWidth))
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidLayoutData,
                                  elementIndex,
                                  "The declared width is not a usable length.",
                                  std::to_string(element.attributeWidth));
            }

            if (element.HasFlag(ElementFlagHasHeight) && !IsUsableLength(element.attributeHeight))
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidLayoutData,
                                  elementIndex,
                                  "The declared height is not a usable length.",
                                  std::to_string(element.attributeHeight));
            }

            if (!IsUsableLength(element.numericValue) || !IsUsableLength(element.minimum) ||
                !IsUsableLength(element.maximum))
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidLayoutData,
                                  elementIndex,
                                  "A numeric value is not finite.",
                                  "value " + std::to_string(element.numericValue) + " min " + std::to_string(element.minimum) +
                                      " max " + std::to_string(element.maximum));
            }

            if (element.HasFlag(ElementFlagHasMinimum) && element.HasFlag(ElementFlagHasMaximum) &&
                element.maximum < element.minimum)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidLayoutData,
                                  elementIndex,
                                  "A numeric range is inverted.",
                                  "min " + std::to_string(element.minimum) + " max " + std::to_string(element.maximum));
            }
        }

        // Reachability, reciprocity and cycle detection. This is the load-time
        // guarantee the per-frame traversal relies on: the runtime never has to
        // defend itself against a malformed tree.
        //
        // An element is marked as soon as it is discovered, never when it is
        // popped, so a link that points back into the tree cannot make the walk
        // visit it twice - a cyclic child list is a diagnostic, not an
        // exhausting traversal.
        std::vector<std::uint8_t> visited(document.elementCount, 0);
        std::vector<ElementIndex> stack;

        visited[document.root] = 1;
        stack.push_back(document.root);

        while (!stack.empty())
        {
            const ElementIndex index = stack.back();
            stack.pop_back();

            const CompiledElement& element = document.elements[index];

            if (element.firstChild == NoElement || element.firstChild >= document.elementCount)
            {
                continue;
            }

            for (ElementIndex child = element.firstChild, steps = 0; child != NoElement; ++steps)
            {
                if (child >= document.elementCount)
                {
                    break;
                }

                // A child list longer than the element table can only be a loop.
                if (steps > document.elementCount)
                {
                    diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                      index,
                                      "The child list does not terminate.",
                                      "a child chain may not visit more elements than the table holds");
                    break;
                }

                if (document.elements[child].parent != index)
                {
                    diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                      child,
                                      "The child's parent link does not point back at its parent.",
                                      "expected parent " + std::to_string(index));
                }

                if (visited[child] != 0)
                {
                    diagnostics.Error(RendererDiagnosticCode::InvalidChildIndex,
                                      child,
                                      "The element is reachable through more than one path.",
                                      "a compiled tree must be a tree, not a graph");
                    break;
                }

                visited[child] = 1;
                stack.push_back(child);
                child = document.elements[child].nextSibling;
            }
        }

        for (std::uint32_t index = 0; index < document.elementCount; ++index)
        {
            if (visited[index] == 0)
            {
                diagnostics.Warning(RendererDiagnosticCode::OrphanedElement,
                                    index,
                                    "The element is not reachable from the root.",
                                    "it will never be laid out or painted");
            }
        }

        // Identifier table: sorted and unique, so the runtime can binary search.
        for (std::uint32_t index = 0; index < document.identifierCount; ++index)
        {
            const CompiledIdEntry& entry = document.identifiers[index];

            if (entry.element >= document.elementCount)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidElementId,
                                  NoElement,
                                  "An identifier entry points outside the element table.",
                                  "entry " + std::to_string(index));
                continue;
            }

            if (document.strings == nullptr || entry.identifier >= document.stringCount)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidStringIndex,
                                  entry.element,
                                  "An identifier entry points outside the string table.");
                continue;
            }

            if (index > 0)
            {
                const int order = std::strcmp(StringAt(document, document.identifiers[index - 1].identifier),
                                              StringAt(document, entry.identifier));
                if (order >= 0)
                {
                    diagnostics.Error(RendererDiagnosticCode::DuplicateIdentifier,
                                      entry.element,
                                      order == 0 ? "Two elements share one identifier."
                                                 : "The identifier table is not sorted.",
                                      std::string("'") + StringAt(document, entry.identifier) + "'");
                }
            }
        }

        for (std::uint32_t index = 0; index < document.bindingCount; ++index)
        {
            const CompiledBinding& binding = document.bindings[index];

            if (binding.element >= document.elementCount)
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidElementId,
                                  NoElement,
                                  "A binding points outside the element table.",
                                  "binding " + std::to_string(index));
                continue;
            }

            if (binding.event == EventKind::Unknown)
            {
                diagnostics.Warning(RendererDiagnosticCode::UnknownEventBinding,
                                    binding.element,
                                    "A binding names an event the runtime does not model.",
                                    std::string("handler '") + StringAt(document, binding.handler) + "'");
            }

            if (binding.handler != 0 && (document.strings == nullptr || binding.handler >= document.stringCount))
            {
                diagnostics.Error(RendererDiagnosticCode::InvalidStringIndex,
                                  binding.element,
                                  "A binding handler points outside the string table.");
            }
        }

        return !diagnostics.HasErrors();
    }
}
