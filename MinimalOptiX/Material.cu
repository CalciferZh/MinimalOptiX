#include <optix_world.h>
#include "structures.h"
#include "disney.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Payload, payload, rtPayload, );
rtDeclareVariable(Ray, ray, rtCurrentRay, );

rtDeclareVariable(rtObject, topGroup, , );
rtDeclareVariable(uint, rayMaxDepth, , );
rtDeclareVariable(uint, rayTypeRadiance, , );
rtDeclareVariable(uint, rayTypeShadow, , );
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
  if (payload.depth > rayMaxDepth || length(payload.color) < rayMinIntensity) {
    payload.color = absorbColor;
    return;
  }
  float3 tmpColor = { 0.f, 0.f, 0.f };
  Ray newRay(
    ray.origin + t * ray.direction,
    normalize(geoNormal + randInUnitSphere(payload.randSeed)),
    rayTypeRadiance,
    rayEpsilonT
  );
  Payload newPayload = folkPayload(payload);
  rtTrace(topGroup, newRay, newPayload);
  payload.color = newPayload.color * lambParams.albedo;
}

// ====================== metal ==========================

rtDeclareVariable(MetalParams, metalParams, , );

RT_PROGRAM void metal() {
  if (payload.depth > rayMaxDepth || length(payload.color) < rayMinIntensity) {
    payload.color = absorbColor;
    return;
  }
  Ray newRay(
    ray.origin + t * ray.direction,
    normalize(reflect(ray.direction, geoNormal) + metalParams.fuzz * randInUnitSphere(payload.randSeed)),
    rayTypeRadiance,
    rayEpsilonT
  );
  Payload newPayload;
  newPayload.depth = payload.depth + 1;
  newPayload.color = make_float3(1.f);
  newPayload.randSeed = tea<16>(payload.randSeed, newPayload.depth);
  rtTrace(topGroup, newRay, newPayload);
  payload.color = metalParams.albedo * newPayload.color;
}

// ====================== glass ==========================

rtDeclareVariable(GlassParams, glassParams, , );

RT_PROGRAM void glass() {
  if (payload.depth > rayMaxDepth || length(payload.color) < rayMinIntensity) {
    payload.color = absorbColor;
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
  Payload newPayload;
  newPayload.depth = payload.depth + 1;
  newPayload.color = make_float3(1.f);
  newPayload.randSeed = tea<16>(payload.randSeed, newPayload.depth);
  if (rand(payload.randSeed) < reflectProb) {
    newRay.origin = frontHitPoint;
    newRay.direction = reflect(ray.direction, normal);
  } else {
    newRay.origin = backHitPoint;
    newRay.direction = refracted;
  }
  rtTrace(topGroup, newRay, newPayload);
  payload.color = newPayload.color * glassParams.albedo;
}

// ====================== Disney =========================

rtDeclareVariable(DisneyParams, disneyParams, , );
rtDeclareVariable(float3, texcoord, attribute texcoord, );
rtBuffer<LightParams> lights;

RT_PROGRAM void disney() {
  if (payload.depth > rayMaxDepth || length(payload.color) < rayMinIntensity) {
    payload.color = absorbColor;
    return;
  }

  float3 N, L, V, H;
  N = faceforward(shadingNormal, -ray.direction, geoNormal);
  V = -ray.direction;
  float3 baseColor;
  if (disneyParams.albedoID == RT_TEXTURE_ID_NULL) {
    baseColor = disneyParams.color;
  } else {
    baseColor = make_float3(optix::rtTex2D<float4>(disneyParams.albedoID, texcoord.x, texcoord.y));
  }

  if (disneyParams.brdfType == GLASS) {
    float3 normal = shadingNormal;
    float cosThetaI = -dot(ray.direction, normal);
    float refIdx;
    if (cosThetaI > 0.f) {
      refIdx = 1.45f;
    } else {
      refIdx = 1.f / 1.45f;
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
    Payload newPayload;
    newPayload.depth = payload.depth + 1;
    newPayload.color = make_float3(1.f);
    newPayload.randSeed = tea<16>(payload.randSeed, newPayload.depth);
    if (rand(payload.randSeed) < reflectProb) {
      newRay.origin = frontHitPoint;
      newRay.direction = reflect(ray.direction, normal);
    } else {
      newRay.origin = backHitPoint;
      newRay.direction = refracted;
    }
    rtTrace(topGroup, newRay, newPayload);
    payload.color = newPayload.color * baseColor;
    return;
  }

  // direct light sample
  float3 directLightColor = make_float3(0.f);
  for (int i = 0; i < lights.size(); ++i) {
    LightParams light = lights[i];
    float3 pointOnLight;
    float3 normalOnLight;
    if (light.shape == SPHERE) {
      pointOnLight = light.position + randInUnitSphere(payload.randSeed) * light.radius;
      normalOnLight = normalize(pointOnLight - light.position);
    } else {
      pointOnLight = light.position + light.u * rand(payload.randSeed) + light.v * rand(payload.randSeed);
      normalOnLight = normalize(light.normal);
    }
    L = pointOnLight - frontHitPoint;
    float lightDst = length(L);
    L = normalize(L);
    if (dot(L, N) > 0.f && dot(L, normalOnLight) < 0.f) {
      Ray newRay(frontHitPoint, L, rayTypeShadow, rayEpsilonT, lightDst - rayEpsilonT);
      Payload newPayload;
      newPayload.depth = payload.depth + 1;
      newPayload.attenuation = make_float3(1.f);
      newPayload.randSeed = tea<16>(payload.randSeed, newPayload.depth);
      rtTrace(topGroup, newRay, newPayload);
      if (length(newPayload.attenuation)) {
        H = normalize(L + V);
        float lightPdf = lightDst * lightDst / light.area / dot(normalOnLight, -L);
        float objPdf = disneyPdf(disneyParams, N, L, V, H);
        if (lightPdf > 0 && objPdf > 0) {
          float3 brdf = disneyEval(disneyParams, baseColor, N, L, V, H);
          directLightColor += powerHeuristic(lightPdf, objPdf) * brdf * light.emission * newPayload.attenuation / max(0.001f, lightPdf);
        }
      }
    }
  }

  float3 indirectColor = make_float3(0.f);
  disneySample(payload.randSeed, disneyParams, N, L, V, H);
  if (dot(N, L) > 0.0f && dot(N, V) > 0.0f) {
    Ray newRay(frontHitPoint, L, rayTypeRadiance, rayEpsilonT);
    Payload newPayload;
    newPayload.depth = payload.depth + 1;
    newPayload.color = make_float3(1.f);
    newPayload.randSeed = tea<16>(payload.randSeed, newPayload.depth);
    rtTrace(topGroup, newRay, newPayload);

    float pdf = disneyPdf(disneyParams, N, L, V, H);
    if (pdf > 0) {
      float3 brdf = disneyEval(disneyParams, baseColor, N, L, V, H);
      indirectColor = brdf * newPayload.color / pdf;
    }
  }

  payload.color = indirectColor + directLightColor + disneyParams.emission;
}

RT_PROGRAM void disneyAnyHit() {
  if (disneyParams.brdfType == GLASS) {
    payload.attenuation *= disneyParams.color;
  } else {
    payload.attenuation = make_float3(0.f);
    rtTerminateRay();
  }
}

// ====================== light ==========================

rtDeclareVariable(LightParams, lightParams, , );

RT_PROGRAM void light() {
  payload.color = lightParams.emission;
}

