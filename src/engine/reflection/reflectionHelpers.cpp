#include "engine/system.h"
#include "engine/core.h"
#include "reflectionHelpers.h"

void *Atlantis::AResourceHandle::GetPtr()
{
    if (Address == 0)
    {
        if (ResourceHolder != nullptr && ResourcePath.IsValid())
        {
            Address = (size_t)ResourceHolder->GetResourcePtr(ResourcePath.GetName());
        }
    }

    return (void*)Address;
}