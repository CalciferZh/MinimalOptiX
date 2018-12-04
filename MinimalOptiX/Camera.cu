#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(rtObject, topObject, , );
rtDeclareVariable(Payload, pld, rtPayload, );
rtDeclareVariable(uint, rayTypeRadiance, , );
rtDeclareVariable(uint, nSuperSampling, , );
rtDeclareVariable(uint2, launchIdx, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );
rtDeclareVariable(float, rayEpsilonT, , );

rtDeclareVariable(CamParams, camParams, , );
rtBuffer<uchar4, 2> outputBuffer;

RT_PROGRAM void pinholeCamera() {
  float3 accu = make_float3(0.f, 0.f, 0.f);
  Ray ray;
  ray.origin = camParams.origin;
  ray.ray_type = rayTypeRadiance;
  ray.tmin = rayEpsilonT;
  ray.tmax = RT_DEFAULT_MAX;
  Payload pld;
  pld.depth = 1;
  float2 unit = 1 / make_float2(launchDim);
  float2 xy = make_float2(launchIdx) * unit;
  for (int i = 0; i < nSuperSampling; ++i) {
    pld.randSeed = i * launchDim.x * launchDim.y + \
                   launchIdx.x * launchDim.y + launchIdx.y + 960822;
    pld.color = make_float3(1.f, 1.f, 1.f);
    ray.direction = normalize(
      camParams.srcLowerLeftCorner + \
      (xy.x + (rand(pld.randSeed) - 0.5) * unit.x) * camParams.horizontal + \
      (xy.y + (rand(pld.randSeed) - 0.5) * unit.y) * camParams.vertical - \
      camParams.origin
    );
    rtTrace(topObject, ray, pld);
    accu += pld.color;
  }
  accu /= (float)nSuperSampling;
  outputBuffer[launchIdx] = make_color(accu);
}
