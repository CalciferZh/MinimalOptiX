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
  int nNewRay = lambParams.nScatter;
  if (pld.depth > lambParams.scatterMaxDepth) {
    nNewRay = 1;
  }
  float3 tmpColor = { 0.f, 0.f, 0.f };
  Ray newRay;
  newRay.origin = ray.origin + t * ray.direction;
  newRay.ray_type = rayTypeRadiance;
  newRay.tmin = rayEpsilonT;
  newRay.tmax = RT_DEFAULT_MAX;
  Payload newPld;
  newPld.depth = pld.depth + 1;
  for (int i = 0; i < nNewRay; ++i) {
    newRay.direction = normalize(geoNormal + randInUnitSphere(pld.randSeed));
    newPld.color = make_float3(1.f, 1.f, 1.f);
    newPld.randSeed = tea<16>(pld.randSeed, newPld.depth * lambParams.nScatter + i);
    rtTrace(topObject, newRay, newPld);
    tmpColor += newPld.color;
  }
  tmpColor /= nNewRay;
  pld.color *= tmpColor;
  pld.color *= lambParams.albedo;
}

// ====================== metal ==========================

rtDeclareVariable(MetalParams, metalParams, , );

RT_PROGRAM void metal() {
  if (pld.depth > rayMaxDepth || length(pld.color) < rayMinIntensity) {
    pld.color = absorbColor;
    return;
  }
  Ray newRay;
  newRay.origin = ray.origin + t * ray.direction;
  newRay.direction = normalize(reflect(ray.direction, geoNormal) + metalParams.fuzz * randInUnitSphere(pld.randSeed));
  newRay.ray_type = rayTypeRadiance;
  newRay.tmin = rayEpsilonT;
  newRay.tmax = RT_DEFAULT_MAX;
  Payload newPld;
  newPld.color = make_float3(1.f, 1.f, 1.f);
  newPld.depth = pld.depth + 1;
  newPld.randSeed = tea<16>(pld.randSeed, newPld.depth);
  rtTrace(topObject, newRay, newPld);
  pld.color *= newPld.color;
  pld.color *= metalParams.albedo;
}

// ====================== glass ==========================

rtDeclareVariable(GlassParams, glassParams, , );

RT_PROGRAM void glass() {
  if (pld.depth > rayMaxDepth || length(pld.color) < rayMinIntensity) {
    pld.color = absorbColor;
    return;
  }

  float3 reflected = reflect(ray.direction, geoNormal);
  float3 outwardNormal;
  float realRefIdx;
  float cosine;
  if (dot(ray.direction, geoNormal) > 0) {
    outwardNormal = -geoNormal;
    realRefIdx = glassParams.refIdx;
    cosine = dot(ray.direction, geoNormal);
    cosine = sqrt(1 - glassParams.refIdx * glassParams.refIdx * (1 - cosine * cosine));
  } else {
    outwardNormal = geoNormal;
    realRefIdx = 1.f / glassParams.refIdx;
    cosine = dot(-ray.direction, geoNormal);
  }
  float3 refracted;
  float reflectProb;
  int nNewRay;
  if (refract(ray.direction, outwardNormal, realRefIdx, refracted)) {
    reflectProb = schlick(cosine, glassParams.refIdx);
    nNewRay = glassParams.nScatter;
  } else {
    reflectProb = 1.f;
    nNewRay = 1;
  }
  if (pld.depth > glassParams.scatterMaxDepth) {
    nNewRay = 1;
  }
  Ray newRay;
  newRay.origin = ray.origin + t * ray.direction;
  newRay.ray_type = rayTypeRadiance;
  newRay.tmin = rayEpsilonT;
  newRay.tmax = RT_DEFAULT_MAX;
  Payload newPld;
  newPld.depth = pld.depth + 1;
  float3 tmpColor = { 0.f, 0.f, 0.f };
  for (int i = 0; i < nNewRay; ++i) {
    if (rand(pld.randSeed) < reflectProb) {
      newRay.direction = reflected;
    } else {
      newRay.direction = refracted;
    }
    newPld.color = make_float3(1.f, 1.f, 1.f);
    newPld.randSeed = tea<16>(pld.randSeed, newPld.depth * glassParams.nScatter + i);
    rtTrace(topObject, newRay, newPld);
    tmpColor += newPld.color;
  }
  tmpColor /= nNewRay;
  pld.color *= tmpColor;
  pld.color *= glassParams.albedo;
}

// ====================== Disney =========================

rtDeclareVariable(int, MaterialID, , );
rtBuffer<DisneyParams> DisneyParamsMtlBuf;


// ====================== light ==========================

rtDeclareVariable(float3, lightColor, , );

RT_PROGRAM void light() {
  pld.color *= lightColor;
}
