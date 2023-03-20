#include "system.h"
#include "core.h"

namespace Atlantis
{
  void ASystem::Process(AWorld *world)
  {
  }

  void ALambdaSystem::Process(AWorld *world)
  {
    Lambda(world);
  }

  void ASyncSystem::Process(AWorld *world)
  {
    world->SyncEntities();
  }
}
