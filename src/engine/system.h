#ifndef SYSTEM_H
#define SYSTEM_H

#include "engine/reflection/reflectionHelpers.h"
#include <unordered_set>
#include <functional>

namespace Atlantis
{
  struct Registry;

  class HNameHashFunction
  {
  public:
    size_t operator()(const HName &p) const
    {
      return p.Hash;
    }
  };

  struct System
  {
    std::unordered_set<HName, HNameHashFunction> Labels;

    virtual void Process(Registry *registry);
  };

  struct LambdaSystem : public System
  {
    std::function<void(Registry *)> Lambda;

    virtual void Process(Registry *registry) override;
  };

} // namespace Atlantis

#endif // !SYSTEM_H
