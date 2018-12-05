#include <optix_world.h>
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(float3, badColor, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void exception() {
  const unsigned int code = rtGetExceptionCode();
  rtPrintf("Caught exception 0x%X at launch index (%d,%d)\n", code, launchIdx.x, launchIdx.y);
  outputBuffer[launchIdx] = make_color(badColor);
}
