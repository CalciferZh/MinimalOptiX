#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(rtObject, topObject, , );
rtDeclareVariable(Payload, pld, rtPayload, );
rtDeclareVariable(uint, rayTypeRadiance, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );
rtDeclareVariable(float, rayEpsilonT, , );

rtDeclareVariable(CamParams, camParams, , );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void pinholeCamera() {
  float2 xy = make_float2(launchIdx) / make_float2(launchDim);
  float3 rayOri = camParams.origin;
  float3 rayDir = normalize(
    camParams.srcLowerLeftCorner + \
    xy.x * camParams.horizontal + \
    xy.y * camParams.vertical - \
    camParams.origin
  );
  Ray ray(rayOri, rayDir, rayTypeRadiance, rayEpsilonT);
  Payload pld;
  pld.color = make_float3(1.f, 1.f, 1.f);
  pld.depth = 1;
  pld.randSeed = launchIdx.x + launchIdx.y * launchDim.x + 960822;
  rtTrace(topObject, ray, pld);
  outputBuffer[launchIdx] = make_color(pld.color);
}
