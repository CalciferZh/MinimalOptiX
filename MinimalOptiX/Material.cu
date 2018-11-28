#include <optix_world.h>
#include "Structures.h"

using namespace optix;

rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );
rtDeclareVariable(float3, mtlColor, , );

RT_PROGRAM void closestHitStatic() {
  pldR.color *= mtlColor;
}
