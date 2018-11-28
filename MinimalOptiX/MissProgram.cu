#include "Structures.h"
#include <optix_world.h>

using namespace optix;

rtDeclareVariable(float3, bgColor, , );
rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );

RT_PROGRAM void staticMiss() {
  pldR.color *= bgColor;
}

rtDeclareVariable(float3, gradColorMax, , );
rtDeclareVariable(float3, gradColorMin, , );

RT_PROGRAM void vGradMiss() {
  float r = ray.direction.y * 0.5f + 0.5f;
  pldR.color *= r * gradColorMax + (1 - r) * gradColorMin;
}
