#include "system.h"

namespace Atlantis
{
  void ASystem::Process(AWorld *world)
  {
  }

  void ALambdaSystem::Process(AWorld *world)
  {
    Lambda(world);
  }
}
