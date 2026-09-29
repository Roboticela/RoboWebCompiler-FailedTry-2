#include "RoboHTMLCompiler/Ir.h"

namespace RoboHTMLCompiler
{
    const NativeAttribute* CustomMetadata::Find(const std::string& name) const
    {
        for (const NativeAttribute& entry : entries)
        {
            if (entry.name == name)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    NativeElement* NativeElement::AddChild(std::unique_ptr<NativeElement> child)
    {
        child->parent = this;
        children.push_back(std::move(child));
        return children.back().get();
    }

    void NativeElement::CollectDescendants(std::vector<const NativeElement*>& out) const
    {
        for (const std::unique_ptr<NativeElement>& child : children)
        {
            out.push_back(child.get());
            child->CollectDescendants(out);
        }
    }

    NativeElement* NativeElement::FindById(ElementId elementId)
    {
        if (id == elementId)
        {
            return this;
        }

        for (const std::unique_ptr<NativeElement>& child : children)
        {
            NativeElement* found = child->FindById(elementId);
            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }

    std::size_t NativeUiModel::CountElements() const
    {
        if (root == nullptr)
        {
            return 0;
        }

        std::vector<const NativeElement*> descendants;
        root->CollectDescendants(descendants);
        return descendants.size() + 1;
    }
}
