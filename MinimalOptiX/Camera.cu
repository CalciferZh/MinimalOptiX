#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(rtObject, topGroup, , );
rtDeclareVariable(Payload, pld, rtPayload, );
rtDeclareVariable(int, randSeed, , );
rtDeclareVariable(uint, rayTypeRadiance, , );
rtDeclareVariable(uint, nSuperSampling, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );
rtDeclareVariable(float, rayEpsilonT, , );

rtBuffer<float3, 2> accuBuffer;

rtDeclareVariable(CamParams, camParams, , );

RT_PROGRAM void camera() {
  Payload pld;
  pld.depth = 1;
  pld.randSeed = tea<16>(launchIdx.y * launchDim.x + launchIdx.x, randSeed);
  pld.color = make_float3(1.f);

  float3 randInLens = camParams.lensRadius * randInUnitDisk(pld.randSeed);
  float3 offset = camParams.u * randInLens.x + camParams.v * randInLens.y;
  float2 xy = (make_float2(launchIdx) + make_float2(rand(pld.randSeed), rand(pld.randSeed)) - 0.5f) / make_float2(launchDim);
  Ray ray(
    camParams.origin + offset,
    normalize(camParams.scrLowerLeftCorner + xy.x * camParams.horizontal + xy.y * camParams.vertical - camParams.origin - offset),
    rayTypeRadiance,
    rayEpsilonT
  );

  rtTrace(topGroup, ray, pld);

  pld.color = clamp(pld.color, make_float3(0.f), make_float3(1.f));

  accuBuffer[launchIdx] += pld.color;
}
