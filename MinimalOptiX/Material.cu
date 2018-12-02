#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(PayloadRadiance, pldR, rtPayload, );
rtDeclareVariable(rtObject, topObject, , );
rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(uint, rayMaxDepth, , );
rtDeclareVariable(uint, rayTypeRadience, , );
rtDeclareVariable(float, t, rtIntersectionDistance, );
rtDeclareVariable(float, rayEpsilonT, , );
rtDeclareVariable(float, rayMinIntensity, , );
rtDeclareVariable(float3, absortColor, , );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
rtDeclareVariable(float3, mtlColor, , );

// =================== lambertian ======================

rtDeclareVariable(float, nScatter, , );

RT_PROGRAM void lambertian() {
  if (pldR.depth > rayMaxDepth || length(pldR.color) < rayMinIntensity) {
    pldR.color = absortColor;
    return;
  }
  float3 P = ray.origin + t * ray.direction;
  int nNewRay = int(nScatter / pldR.depth + 1);
  float3 tmpColor = { 0.f, 0.f, 0.f };
  for (int i = 0; i < nNewRay; ++i) {
    float3 rayOrigin = P;
    float3 rayDirection = geoNormal + randInUnitSphere(pldR.randSeed);
    Ray newRay(rayOrigin, rayDirection, rayTypeRadience, rayEpsilonT);
    PayloadRadiance newPldR;
    newPldR.color = make_float3(1.f, 1.f, 1.f);
    newPldR.depth = pldR.depth + 1;
    newPldR.randSeed = pldR.randSeed + newPldR.depth * i;
    rtTrace(topObject, newRay, newPldR);
    tmpColor += newPldR.color;
  }
  tmpColor /= nNewRay;
  pldR.color *= tmpColor;
  pldR.color *= mtlColor;
}

// ====================== light ======================

rtDeclareVariable(float3, lightColor, , );

RT_PROGRAM void light() {
  pldR.color *= lightColor;
}
