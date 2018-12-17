#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Payload, pld, rtPayload, );
rtDeclareVariable(Ray, ray, rtCurrentRay, );

rtDeclareVariable(rtObject, topGroup, , );
rtDeclareVariable(uint, rayMaxDepth, , );
rtDeclareVariable(uint, rayTypeRadiance, , );
rtDeclareVariable(float, t, rtIntersectionDistance, );
rtDeclareVariable(float, rayEpsilonT, , );
rtDeclareVariable(float, rayMinIntensity, , );
rtDeclareVariable(float3, absorbColor, , );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
rtDeclareVariable(float3, shadingNormal, attribute shadingNormal, );
rtDeclareVariable(float3, frontHitPoint, attribute frontHitPoint, );
rtDeclareVariable(float3, backHitPoint, attribute backHitPoint, );

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
    newPld.color = make_float3(1.f);
    newPld.randSeed = tea<16>(pld.randSeed, newPld.depth * lambParams.nScatter + i);
    rtTrace(topGroup, newRay, newPld);
    tmpColor += newPld.color;
  }
  tmpColor /= nNewRay;
  pld.color = tmpColor * lambParams.albedo;
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
  newPld.color = make_float3(1.f);
  newPld.depth = pld.depth + 1;
  newPld.randSeed = tea<16>(pld.randSeed, newPld.depth);
  rtTrace(topGroup, newRay, newPld);
  pld.color = metalParams.albedo * newPld.color;
}

// ====================== glass ==========================

rtDeclareVariable(GlassParams, glassParams, , );

RT_PROGRAM void glass() {
  if (pld.depth > rayMaxDepth || length(pld.color) < rayMinIntensity) {
    pld.color = absorbColor;
    return;
  }

  float3 normal = shadingNormal;
	float cosThetaI = -dot(ray.direction, normal);
	float refIdx;
	if (cosThetaI > 0.f) {
		refIdx = glassParams.refIdx;
	} else {
		refIdx = 1.f / glassParams.refIdx;
		cosThetaI = -cosThetaI;
		normal = -normal;
	}

	float3 refracted;
  float totalReflection = !refract(refracted, ray.direction, normal, refIdx);
	float cosThetaT = -dot(normal, refracted);
	float reflectProb =  totalReflection ? 1.f : fresnel(cosThetaI, cosThetaT, refIdx);
  Ray newRay;
  newRay.ray_type = rayTypeRadiance;
  newRay.tmin = rayEpsilonT;
  newRay.tmax = RT_DEFAULT_MAX;
  Payload newPld;
  newPld.depth = pld.depth + 1;
  newPld.color = make_float3(1.f, 1.f, 1.f);
  newPld.randSeed = tea<16>(pld.randSeed, newPld.depth);
  if (rand(pld.randSeed) < reflectProb) {
    newRay.origin = frontHitPoint;
    newRay.direction = reflect(ray.direction, normal);
  } else {
    newRay.origin = backHitPoint;
    newRay.direction = refracted;
  }
  rtTrace(topGroup, newRay, newPld);
  pld.color = newPld.color * glassParams.albedo;
}

// ====================== Disney =========================

rtDeclareVariable(DisneyParams, disneyParams, , );
rtDeclareVariable(float3, texcoord, attribute texcoord, );
rtBuffer<LightParams> lights;

RT_PROGRAM void disney() {
  if (pld.depth > rayMaxDepth || length(pld.color) < rayMinIntensity) {
    pld.color = absorbColor;
    return;
  }

  // TODO: sample from light directly before bsdf
  // closest hit should be different for opaque and transparent

  // sample
  float3 N = faceforward(shadingNormal, -ray.direction, geoNormal);
  float3 V = -ray.direction;
  float3 L;
  float3 H;
  float diffuseRatio = 0.5f * (1.0f - disneyParams.metallic);
  Onb onb(N);
  if (rand(pld.randSeed) < diffuseRatio) { // diffuse
    cosine_sample_hemisphere(rand(pld.randSeed), rand(pld.randSeed), L);
    onb.inverse_transform(L);
    H = normalize(L + V);
  } else { // specular
    float a = max(0.001f, disneyParams.roughness);
    float phi = rand(pld.randSeed) * 2.0f * M_PIf;
    float random = rand(pld.randSeed);
    float cosTheta = sqrtf((1.f - random) / (1.0f + (a * a - 1.f) * random));
    float sinTheta = sqrtf(1.0f - (cosTheta * cosTheta));
    float sinPhi = sinf(phi);
    float cosPhi = cosf(phi);
    H = make_float3(sinTheta*cosPhi, sinTheta*sinPhi, cosTheta);
    onb.inverse_transform(H);
    L = normalize(2.0f * dot(V, H) * H - V);
  }
  Ray newRay(frontHitPoint, L, rayTypeRadiance, rayEpsilonT);
  Payload newPld;
  newPld.depth = pld.depth + 1;
  newPld.color = make_float3(1.f);
  newPld.randSeed = tea<16>(pld.randSeed, newPld.depth);
  rtTrace(topGroup, newRay, newPld);
  float3 lightColor = newPld.color;

  if (dot(N, L) <= 0.0f || dot(N, V) <= 0.0f) {
    pld.color = make_float3(0.f);
    return;
  }

  float pdf = disneyPdf(disneyParams, N, L, V, H);

  if (pdf < 0) {
    pld.color = make_float3(0.f);
    return;
  }

  float3 baseColor;
  if (disneyParams.albedoID == RT_TEXTURE_ID_NULL) {
    baseColor = disneyParams.color;
  } else {
    baseColor = make_float3(optix::rtTex2D<float4>(disneyParams.albedoID, texcoord.x, texcoord.y));
  }
  float3 brdf = disneyEval(disneyParams, baseColor, N, L, V, H, onb);

  pld.color = brdf * lightColor / pdf;
}

// ====================== light ==========================

rtDeclareVariable(LightParams, lightParams, , );

RT_PROGRAM void light() {
  pld.color = lightParams.emission;// * clamp(dot(ray.direction, shadingNormal), 0.f, 1.f);
}

// ====================== anyhit =========================

RT_PROGRAM void basicAnyHit() {
  pld.isShadow = true;
  rtTerminateRay();
}
