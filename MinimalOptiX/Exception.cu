#include <optix_world.h>
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(float3, badColor, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtBuffer<float3, 2> accuBuffer;

RT_PROGRAM void exception() {
  accuBuffer[launchIdx] += badColor;
}
