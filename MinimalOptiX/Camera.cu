#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );
rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );
rtDeclareVariable(uint, rayTypeRadience, , );
rtDeclareVariable(rtObject, topObject, , );
rtDeclareVariable(float, rayEpsilonT, , );
rtDeclareVariable(float3, origin, , );
rtDeclareVariable(float3, horizontal, , );
rtDeclareVariable(float3, vertical, , );
rtDeclareVariable(float3, scrLowerLeftCorner, , );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void pinholeCamera() {
  float2 xy = make_float2(launchIdx) / make_float2(launchDim);
  float3 rayOri = origin;
  float3 rayDir = normalize(scrLowerLeftCorner + xy.x * horizontal + xy.y * vertical - origin);
  Ray ray(rayOri, rayDir, rayTypeRadience, rayEpsilonT);
  PayloadRadiance pldR;
  pldR.color = make_float3(1.f, 1.f, 1.f);
  pldR.depth = 1;
  pldR.randSeed = launchIdx.x + launchIdx.y * launchDim.x + 960822;
  rtTrace(topObject, ray, pldR);
  outputBuffer[launchIdx] = make_color(pldR.color);
}
