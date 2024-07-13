#include "system.h"
#include "core.h"
#include "engine/world.h"

namespace Atlantis
{
  void ASystem::Process(AWorld *world)
  {
    CompoundedDeltaTime = world->GetDeltaTime();
  }

  float ASystem::GetDeltaTime() const
  {
    return CompoundedDeltaTime;
  }

  void ALambdaSystem::Process(AWorld *world)
  {
    CompoundedDeltaTime = world->GetDeltaTime();
    Lambda(world);
  }

  void ALambdaSystemTimesliced::Process(AWorld* world)
  {
    if (CurrentObjectIndex == 0)
    {
      CompoundedDeltaTime = 0.0f;
    }

    CompoundedDeltaTime += world->GetDeltaTime();

    LambdaTimesliced(world, this);
  }

  void ASyncSystem::Process(AWorld *world)
  {
    CompoundedDeltaTime = world->GetDeltaTime();
    world->SyncEntities();
  }
}
