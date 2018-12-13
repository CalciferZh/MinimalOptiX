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

RT_PROGRAM void disney() {
  if (pld.depth > rayMaxDepth || length(pld.color) < rayMinIntensity) {
    pld.color = absorbColor;
    return;
  }

  float3 baseColor;
  if (disneyParams.albedoID == RT_TEXTURE_ID_NULL) {
    baseColor = disneyParams.color;
  } else {
		baseColor = make_float3(optix::rtTex2D<float4>(disneyParams.albedoID, texcoord.x, texcoord.y));
  }
  // sample
  float3 N = faceforward(shadingNormal, -ray.direction, geoNormal);
  float3 V = -ray.direction;
  float3 L;
  float diffuseRatio = 0.5f * (1.0f - disneyParams.metallic);
  float r1 = rand(pld.randSeed);
  float r2 = rand(pld.randSeed);
  optix::Onb onb(N);
  if (rand(pld.randSeed) < diffuseRatio) { // diffuse
    cosine_sample_hemisphere(r1, r2, L);
    onb.inverse_transform(L);
  } else { // spect
    float a = max(0.001f, disneyParams.roughness);
    float phi = r1 * 2.0f * M_PIf;
    float cosTheta = sqrtf((1.f - r2) / (1.0f + (a * a - 1.f) * r2));
    float sinTheta = sqrtf(1.0f - (cosTheta * cosTheta));
    float sinPhi = sinf(phi);
    float cosPhi = cosf(phi);
    float3 half = make_float3(sinTheta*cosPhi, sinTheta*sinPhi, cosTheta);
    onb.inverse_transform(half);
    L = 2.0f * dot(V, half) * half - V;
  }
  Ray newRay(frontHitPoint, normalize(L), rayTypeRadiance, rayEpsilonT);
  Payload newPld;
  newPld.depth = pld.depth + 1;
  newPld.color = make_float3(1.f);
  newPld.randSeed = tea<16>(pld.randSeed, newPld.depth);
  rtTrace(topGroup, newRay, newPld);
  float3 lightColor = newPld.color;

  // probability for this light
  float specularAlpha = max(0.001f, disneyParams.roughness);
  float clearcoatAlpha = lerp(0.1f, 0.001f, disneyParams.clearcoatGloss);
  float specularRatio = 1.f - diffuseRatio;
  float3 half = normalize(L + V);
  float cosTheta = abs(dot(half, N));
  float pdfGTR2 = GTR2(cosTheta, specularAlpha) * cosTheta;
  float pdfGTR1 = GTR1(cosTheta, clearcoatAlpha) * cosTheta;
  // calculate diffuse and specular pdfs and mix ratio
  float ratio = 1.0f / (1.0f + disneyParams.clearcoat);
  float pdfSpec = lerp(pdfGTR1, pdfGTR2, ratio) / (4.0 * abs(dot(L, half)));
  float pdfDiff = abs(dot(L, N))* (1.0f / M_PIf);
  float pdf = diffuseRatio * pdfDiff + specularRatio * pdfSpec;

  if (pdf < 0) {
    pld.color = make_float3(0.f);
    return;
  }

  // evaluate color
  float NDotL = dot(N, L);
  float NDotV = dot(N, V);
  if (NDotL <= 0.0f || NDotV <= 0.0f) {
    pld.color = make_float3(0.f);
    return;
  }
  float3 H = normalize(L + V);
  float NDotH = dot(N, H);
  float LDotH = dot(L, H);
  float luminance = dot(baseColor, make_float3(0.3, 0.6, 0.1));
  float3 Ctint = luminance > 0.f ? baseColor / luminance : make_float3(1.f);
  float3 Cspec0 = lerp(disneyParams.specular * 0.08f * lerp(make_float3(1.f), Ctint, disneyParams.specularTint), baseColor, disneyParams.metallic);
  float3 Csheen = lerp(make_float3(1.f), Ctint, disneyParams.sheenTint);
  float FL = schlickFresnel(NDotL);
  float FV = schlickFresnel(NDotV);
  float Fd90 = 0.5f + 2.f * LDotH * LDotH * disneyParams.roughness;
  float Fd = lerp(1.f, Fd90, FL) * lerp(1.f, Fd90, FV);
  float Fss90 = LDotH * LDotH * disneyParams.roughness;
  float Fss = lerp(1.0f, Fss90, FL) * lerp(1.0f, Fss90, FV);
  float ss = 1.25f * (Fss * (1.f / (NDotL + NDotV) - 0.5f) + 0.5f);
  float aspect = sqrt(1 - disneyParams.anisotropic * 0.9f);
  float ax = max(.001f, square(disneyParams.roughness) / aspect);
  float ay = max(.001f, square(disneyParams.roughness) * aspect);
  float3 X = normalize(onb.m_tangent);
  float3 Y = normalize(cross(shadingNormal, X));
  float Ds = GTR2Aniso(NDotH, dot(H, X), dot(H, Y), ax, ay);
  float FH = schlickFresnel(LDotH);
  float3 Fs = lerp(Cspec0, make_float3(1.f), FH);
  float roughg = square(disneyParams.roughness * 0.5f + 0.5f);
  float Gs  = smithGGgxAniso(NDotL, dot(L, X), dot(L, Y), ax, ay) *
              smithGGgxAniso(NDotV, dot(V, X), dot(V, Y), ax, ay);
  float3 Fsheen = FH * disneyParams.sheen * Csheen;
  float Dr = GTR1(NDotH, lerp(0.1f, 0.001f, disneyParams.clearcoatGloss));
  float Fr = lerp(0.04f, 1.f, FH);
  float Gr = smithGGgx(NDotL, 0.25f) * smithGGgx(NDotV, 0.25f);
  float3 bsdf = ((1.0f / M_PIf) * lerp(Fd, ss, disneyParams.subsurface) * baseColor + Fsheen) * (1.0f - disneyParams.metallic) +
                Gs * Fs * Ds + 0.25f * disneyParams.clearcoat * Gr * Fr * Dr;

  pld.color = bsdf * lightColor / pdf;
  float exp = 1.f / 2.2f;
  pld.color.x = pow(pld.color.x, exp);
  pld.color.y = pow(pld.color.y, exp);
  pld.color.z = pow(pld.color.z, exp);
}

// ====================== light ==========================

rtDeclareVariable(LightParams, lightParams, , );
// NOTE: some of light's attributes need to be computed mannually

RT_PROGRAM void light() {
  pld.color = lightParams.emission * clamp(dot(ray.direction, shadingNormal), 0.f, 1.f);
}
