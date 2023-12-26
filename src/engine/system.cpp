#include "system.h"
#include "core.h"
#include "engine/world.h"

namespace Atlantis
{
  void ASystem::Process(AWorld *world)
  {
  }

  void ALambdaSystem::Process(AWorld *world)
  {
    Lambda(world);
  }

  void ALambdaSystemTimesliced::Process(AWorld* world)
  {
    LambdaTimesliced(world, this);
  }

  void ASyncSystem::Process(AWorld *world)
  {
    world->SyncEntities();
  }
}
