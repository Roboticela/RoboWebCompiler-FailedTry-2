// ---------------------------------------------------------------------------
// RoboHTMLCompiler - source AST.
//
// The AST preserves the source vocabulary exactly as written, including the
// original tag and attribute spellings. Normalisation into the compact native
// representation happens later, so the browser target can always recover the
// original markup.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/GeneratedEnums.h"
#include "RoboHTMLCompiler/Registry.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace RoboHTMLCompiler
{
    enum class NodeKind
    {
        Document,
        Doctype,
        Element,
        Text,
        Comment
    };

    struct AttributeNode
    {
        std::string name;
        std::string value;
        bool hasValue = false;
        bool isDataAttribute = false;
        SourceLocation location;

        /// Identifier of the definition this attribute resolved to.
        NativeProperty nativeProperty = NativeProperty::Unmapped;
    };

    struct Node
    {
        NodeKind kind;
        SourceLocation location;

        Node(NodeKind nodeKind, SourceLocation nodeLocation)
            : kind(nodeKind)
            , location(std::move(nodeLocation))
        {
        }

        virtual ~Node() = default;
    };

    struct TextNode : Node
    {
        std::string text;
        bool raw = false;

        TextNode(SourceLocation nodeLocation, std::string nodeText)
            : Node(NodeKind::Text, std::move(nodeLocation))
            , text(std::move(nodeText))
        {
        }

        bool IsWhitespaceOnly() const;
    };

    struct CommentNode : Node
    {
        std::string text;

        CommentNode(SourceLocation nodeLocation, std::string nodeText)
            : Node(NodeKind::Comment, std::move(nodeLocation))
            , text(std::move(nodeText))
        {
        }
    };

    struct DoctypeNode : Node
    {
        std::string name;

        DoctypeNode(SourceLocation nodeLocation, std::string nodeName)
            : Node(NodeKind::Doctype, std::move(nodeLocation))
            , name(std::move(nodeName))
        {
        }
    };

    struct ElementNode : Node
    {
        std::string sourceTag;
        MarkupNamespace ns = MarkupNamespace::Html;
        std::vector<AttributeNode> attributes;
        std::vector<std::unique_ptr<Node>> children;

        /// Filled in by HtmlValidator/NativeTreeBuilder.
        ElementKind nativeKind = ElementKind::None;
        int headingLevel = 0;
        bool isCustomElement = false;

        /// Element identity assigned during native lowering.
        std::uint32_t nativeId = 0;

        ElementNode* parent = nullptr;

        ElementNode(SourceLocation nodeLocation, std::string tag, MarkupNamespace nodeNamespace)
            : Node(NodeKind::Element, std::move(nodeLocation))
            , sourceTag(std::move(tag))
            , ns(nodeNamespace)
        {
        }

        const AttributeNode* FindAttribute(std::string_view name) const;
        AttributeNode* FindAttribute(std::string_view name);

        bool HasAttribute(std::string_view name) const { return FindAttribute(name) != nullptr; }

        std::string GetAttribute(std::string_view name) const;

        /// Concatenated text of this element's direct children.
        std::string DirectTextContent() const;

        /// Concatenated text of the whole subtree.
        std::string TextContent() const;
    };

    struct DocumentNode : Node
    {
        std::vector<std::unique_ptr<Node>> children;

        /// Set when an <html> element was parsed.
        ElementNode* documentElement = nullptr;

        explicit DocumentNode(SourceLocation nodeLocation)
            : Node(NodeKind::Document, std::move(nodeLocation))
        {
        }
    };

    /// A parsed markup document.
    struct MarkupDocument
    {
        std::string sourceFile;
        std::unique_ptr<DocumentNode> root;

        /// Depth-first traversal in document order.
        void ForEachElement(const std::function<void(const ElementNode&)>& visitor) const;

        ElementNode* FindElementById(std::string_view identifier) const;

        std::vector<ElementNode*> CollectElements() const;
    };

    /// Helpers shared by the frontends.
    std::string ToLowerAscii(std::string_view value);
    std::string TrimAscii(std::string_view value);
    bool EqualsIgnoreCase(std::string_view left, std::string_view right);
}
