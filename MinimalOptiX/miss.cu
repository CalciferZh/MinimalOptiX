#include "Structures.h"
#include <optix_world.h>

using namespace optix;

rtDeclareVariable(float3, bgColor, , );
rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(Payload, pld, rtPayload, );

RT_PROGRAM void staticMiss() {
  pld.color *= bgColor;
}
