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
rtDeclareVariable(float3, shadingNormal,   attribute shadingNormal, );
rtDeclareVariable(float3, frontHitPoint, attribute frontHitPoint, );

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
  newPld.color = make_float3(1.f);
  newPld.depth = pld.depth + 1;
  newPld.randSeed = tea<16>(pld.randSeed, newPld.depth);
  rtTrace(topGroup, newRay, newPld);
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
    rtTrace(topGroup, newRay, newPld);
    tmpColor += newPld.color;
  }
  tmpColor /= nNewRay;
  pld.color *= tmpColor;
  pld.color *= glassParams.albedo;
}

// ====================== Disney =========================

rtDeclareVariable(DisneyParams, disneyParams, , );
rtDeclareVariable(float3, texcoord, attribute texcoord, );

RT_PROGRAM void disney() {
  float3 baseColor;
  if (disneyParams.albedoID == RT_TEXTURE_ID_NULL) {
    baseColor = disneyParams.color;
  } else {
		const float3 texColor = make_float3(optix::rtTex2D<float4>(disneyParams.albedoID, texcoord.x, texcoord.y));
		baseColor = make_float3(texColor.x, texColor.y, texColor.z);
  }
  // if (disneyParams.BrdfType == NORMAL) {
  if (true) {
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
    pld.color *= newPld.color;

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
      pld.color *= 0.f;
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
    float3 Cdlin = baseColor;
    float Cdlum = 0.3f * Cdlin.x + 0.6f * Cdlin.y + 0.1f * Cdlin.z;
    float3 Ctint = Cdlum > 0.0f ? Cdlin / Cdlum : make_float3(1.f);
    float3 Cspec0 = lerp(disneyParams.specular * 0.08f * lerp(make_float3(1.f), Ctint, disneyParams.specularTint), Cdlin, disneyParams.metallic);
    float3 Csheen = lerp(make_float3(1.f), Ctint, disneyParams.sheenTint);
    float FL = SchlickFresnel(NDotL);
    float FV = SchlickFresnel(NDotV);
    float Fd90 = 0.5f + 2.f * LDotH * LDotH * disneyParams.roughness;
    float Fd = lerp(1.f, Fd90, FL) * lerp(1.f, Fd90, FV);
    float Fss90 = LDotH * LDotH * disneyParams.roughness;
    float Fss = lerp(1.0f, Fss90, FL) * lerp(1.0f, Fss90, FV);
    float ss = 1.25f * (Fss * (1.0f / (NDotL + NDotV) - 0.5f) + 0.5f);
    float aspect = sqrt(1 - disneyParams.anisotropic * 0.9f);
    float ax = max(.001f, sqrt(disneyParams.roughness) / aspect);
    float ay = max(.001f, sqrt(disneyParams.roughness) * aspect);
    float3 X = { 1.f, 0.f, 0.f };
    float3 Y = { 0.f, 1.f, 1.f };
    float Ds = GTR2_Aniso(NDotH, dot(H, X), dot(H, Y), ax, ay);
    // float a = max(0.001f, disneyParams.roughness);
    // float Ds = GTR2(NDotH, a);
    float FH = SchlickFresnel(LDotH);
    float3 Fs = lerp(Cspec0, make_float3(1.0f), FH);
    float roughg = sqrt(disneyParams.roughness*0.5f + 0.5f);
    float Gs = smithG_GGX(NDotL, roughg) * smithG_GGX(NDotV, roughg);
    float3 Fsheen = FH * disneyParams.sheen * Csheen;
    float Dr = GTR1(NDotH, lerp(0.1f, 0.001f, disneyParams.clearcoatGloss));
    float Fr = lerp(0.04f, 1.0f, FH);
    float Gr = smithG_GGX(NDotL, 0.25f) * smithG_GGX(NDotV, 0.25f);
    float3 tmpColor = ((1.0f / M_PIf) * lerp(Fd, ss, disneyParams.subsurface) * Cdlin + Fsheen)
      * (1.0f - disneyParams.metallic)
      + Gs * Fs * Ds + 0.25f * disneyParams.clearcoat * Gr * Fr * Dr;
    float3 finalColor = tmpColor * clamp(dot(N, L), 0.0f, 1.0f);

    pld.color *= finalColor / pdf;
  } else {
    return;
  }
}

// ====================== light ==========================

rtDeclareVariable(LightParams, lightParams, , );
// NOTE: some of light's attributes need to be computed mannually

RT_PROGRAM void light() {
  pld.color *= lightParams.emission;
}
