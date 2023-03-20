#ifndef SYSTEM_H
#define SYSTEM_H

#include "engine/reflection/reflectionHelpers.h"
#include <unordered_set>
#include <functional>

namespace Atlantis
{
  struct AWorld;

  struct ASystem
  {
    std::unordered_set<AName, ANameHashFunction> Labels;

    virtual void Process(AWorld *world);
  };

  struct ALambdaSystem : public ASystem
  {
    std::function<void(AWorld *)> Lambda;

    virtual void Process(AWorld *world) override;
  };

  struct ASyncSystem : public ASystem
  {
    virtual void Process(AWorld *world) override;
  };

} // namespace Atlantis

#endif // !SYSTEM_H
