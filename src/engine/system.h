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
    std::unordered_set<AName> Labels;

    bool IsRenderSystem = false;

    // timeslicing stuff
    bool IsTimesliced = false;

    int ObjectsPerFrame = 1000;

    int CurrentObjectIndex = 0;

    float CompoundedDeltaTime = 0.0f;

    virtual void Process(AWorld *world);

    float GetDeltaTime() const;
  };

  struct ALambdaSystem : public ASystem
  {
    std::function<void(AWorld*)> Lambda;

    virtual void Process(AWorld* world) override;
  };

  struct ALambdaSystemTimesliced : public ASystem
  {
    std::function<void(AWorld*, ASystem*)> LambdaTimesliced;

    virtual void Process(AWorld* world) override;
  };

  struct ASyncSystem : public ASystem
  {
    virtual void Process(AWorld* world) override;
  };

  }      // namespace Atlantis

#endif // !SYSTEM_H
