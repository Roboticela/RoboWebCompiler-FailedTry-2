#include "RoboHTMLCompiler/Ast.h"

#include <algorithm>
#include <cctype>

namespace RoboHTMLCompiler
{
    std::string ToLowerAscii(std::string_view value)
    {
        std::string result(value);
        for (char& character : result)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        return result;
    }

    std::string TrimAscii(std::string_view value)
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

        return std::string(value.substr(begin, end - begin));
    }

    bool EqualsIgnoreCase(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const auto leftCharacter = static_cast<unsigned char>(left[index]);
            const auto rightCharacter = static_cast<unsigned char>(right[index]);
            if (std::tolower(leftCharacter) != std::tolower(rightCharacter))
            {
                return false;
            }
        }

        return true;
    }

    bool TextNode::IsWhitespaceOnly() const
    {
        for (const char character : text)
        {
            if (std::isspace(static_cast<unsigned char>(character)) == 0)
            {
                return false;
            }
        }
        return true;
    }

    const AttributeNode* ElementNode::FindAttribute(std::string_view name) const
    {
        for (const AttributeNode& attribute : attributes)
        {
            if (attribute.name == name)
            {
                return &attribute;
            }
        }
        return nullptr;
    }

    AttributeNode* ElementNode::FindAttribute(std::string_view name)
    {
        for (AttributeNode& attribute : attributes)
        {
            if (attribute.name == name)
            {
                return &attribute;
            }
        }
        return nullptr;
    }

    std::string ElementNode::GetAttribute(std::string_view name) const
    {
        const AttributeNode* attribute = FindAttribute(name);
        return (attribute != nullptr) ? attribute->value : std::string();
    }

    std::string ElementNode::DirectTextContent() const
    {
        std::string result;
        for (const std::unique_ptr<Node>& child : children)
        {
            if (child->kind == NodeKind::Text)
            {
                result += static_cast<const TextNode*>(child.get())->text;
            }
        }
        return result;
    }

    std::string ElementNode::TextContent() const
    {
        std::string result;
        for (const std::unique_ptr<Node>& child : children)
        {
            switch (child->kind)
            {
            case NodeKind::Text:
                result += static_cast<const TextNode*>(child.get())->text;
                break;
            case NodeKind::Element:
                result += static_cast<const ElementNode*>(child.get())->TextContent();
                break;
            default:
                break;
            }
        }
        return result;
    }

    namespace
    {
        void VisitElementConst(const ElementNode& element, const std::function<void(const ElementNode&)>& visitor)
        {
            visitor(element);
            for (const std::unique_ptr<Node>& child : element.children)
            {
                if (child->kind == NodeKind::Element)
                {
                    VisitElementConst(*static_cast<const ElementNode*>(child.get()), visitor);
                }
            }
        }
    }

    void MarkupDocument::ForEachElement(const std::function<void(const ElementNode&)>& visitor) const
    {
        if (root == nullptr)
        {
            return;
        }

        for (const std::unique_ptr<Node>& child : root->children)
        {
            if (child->kind == NodeKind::Element)
            {
                VisitElementConst(*static_cast<const ElementNode*>(child.get()), visitor);
            }
        }
    }

    ElementNode* MarkupDocument::FindElementById(std::string_view identifier) const
    {
        ElementNode* found = nullptr;

        ForEachElement([&](const ElementNode& element) {
            if (found == nullptr && element.GetAttribute("id") == identifier)
            {
                found = const_cast<ElementNode*>(&element);
            }
        });

        return found;
    }

    std::vector<ElementNode*> MarkupDocument::CollectElements() const
    {
        std::vector<ElementNode*> collected;
        ForEachElement([&](const ElementNode& element) {
            collected.push_back(const_cast<ElementNode*>(&element));
        });
        return collected;
    }
}
