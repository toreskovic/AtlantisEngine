#ifndef SYSTEM_H
#define SYSTEM_H

#include "engine/reflection/reflectionHelpers.h"
#include <unordered_set>
#include <functional>

namespace Atlantis
{
  struct ARegistry;

  struct ASystem
  {
    std::unordered_set<HName, HNameHashFunction> Labels;

    virtual void Process(ARegistry *registry);
  };

  struct ALambdaSystem : public ASystem
  {
    std::function<void(ARegistry *)> Lambda;

    virtual void Process(ARegistry *registry) override;
  };

} // namespace Atlantis

#endif // !SYSTEM_H
