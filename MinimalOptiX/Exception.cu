#include <optix_world.h>
#include "Utils.h"

using namespace optix;

rtDeclareVariable(float3, badColor, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void exception() {
  outputBuffer[launchIdx] = makeColor(badColor);
}
