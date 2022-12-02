#include "system.h"

namespace Atlantis
{
  void ASystem::Process(ARegistry *registry)
  {
  }

  void ALambdaSystem::Process(ARegistry *registry)
  {
    Lambda(registry);
  }
}
