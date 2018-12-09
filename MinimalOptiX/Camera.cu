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

rtDeclareVariable(CamParams, camParams, , );
rtBuffer<float3, 2> accuBuffer;
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void pinholeCamera() {
  Payload pld;
  pld.depth = 1;
  pld.randSeed = tea<16>(launchIdx.x * launchDim.x + launchIdx.y, randSeed);
  pld.color = make_float3(1.f);

  Ray ray;
  ray.origin = camParams.origin;
  ray.ray_type = rayTypeRadiance;
  ray.tmin = rayEpsilonT;
  ray.tmax = RT_DEFAULT_MAX;
  float2 xy = (make_float2(launchIdx) + make_float2(rand(pld.randSeed), rand(pld.randSeed)) - 0.5f) / make_float2(launchDim);
  ray.direction = normalize(
    camParams.srcLowerLeftCorner + xy.x * camParams.horizontal + xy.y * camParams.vertical - camParams.origin
  );

  rtTrace(topGroup, ray, pld);

  accuBuffer[launchIdx] += pld.color;
  outputBuffer[launchIdx] = make_color(pld.color);
}
