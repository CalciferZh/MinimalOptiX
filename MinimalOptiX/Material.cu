#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Payload, pld, rtPayload, );
rtDeclareVariable(Ray, ray, rtCurrentRay, );

rtDeclareVariable(rtObject, topObject, , );
rtDeclareVariable(uint, rayMaxDepth, , );
rtDeclareVariable(uint, rayTypeRadiance, , );
rtDeclareVariable(float, t, rtIntersectionDistance, );
rtDeclareVariable(float, rayEpsilonT, , );
rtDeclareVariable(float, rayMinIntensity, , );
rtDeclareVariable(float3, absorbColor, , );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );

// =================== lambertian ======================

rtDeclareVariable(LambertianParams, lambParams, , );

RT_PROGRAM void lambertian() {
  if (pld.depth > rayMaxDepth || length(pld.color) < rayMinIntensity) {
    pld.color = absorbColor;
    return;
  }
  float3 P = ray.origin + t * ray.direction;
  int nNewRay = lambParams.nScatter / pld.depth + 1;
  float3 tmpColor = { 0.f, 0.f, 0.f };
  for (int i = 0; i < nNewRay; ++i) {
    float3 rayOrigin = P;
    float3 rayDirection = geoNormal + randInUnitSphere(pld.randSeed);
    Ray newRay(rayOrigin, rayDirection, rayTypeRadiance, rayEpsilonT);
    Payload newPld;
    newPld.color = make_float3(1.f, 1.f, 1.f);
    newPld.depth = pld.depth + 1;
    newPld.randSeed = pld.randSeed + newPld.depth * i;
    rtTrace(topObject, newRay, newPld);
    tmpColor += newPld.color;
  }
  tmpColor /= nNewRay;
  pld.color *= tmpColor;
  pld.color *= lambParams.albedo;
}

// ====================== light ======================

rtDeclareVariable(float3, lightColor, , );

RT_PROGRAM void light() {
  pld.color *= lightColor;
}
