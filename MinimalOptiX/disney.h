#pragma once

#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

__device__ __inline__ void disneySample(int& randSeed, DisneyParams& disneyParams, float3& N, float3& L, float3& V, float3& H) {
  float diffuseRatio = 0.5f * (1.0f - disneyParams.metallic);
  Onb onb(N);
  if (rand(randSeed) < diffuseRatio) { // diffuse
    cosine_sample_hemisphere(rand(randSeed), rand(randSeed), L);
    onb.inverse_transform(L);
    L = normalize(L);
    H = normalize(L + V);
  } else { // specular
    float a = max(0.001f, disneyParams.roughness);
    float phi = rand(randSeed) * 2.0f * M_PIf;
    float random = rand(randSeed);
    float cosTheta = sqrtf((1.f - random) / (1.0f + (a * a - 1.f) * random));
    float sinTheta = sqrtf(1.0f - (cosTheta * cosTheta));
    float sinPhi = sinf(phi);
    float cosPhi = cosf(phi);
    H = make_float3(sinTheta*cosPhi, sinTheta*sinPhi, cosTheta);
    onb.inverse_transform(H);
    L = normalize(2.0f * dot(V, H) * H - V);
    H = normalize(H);
  }
}

__device__ __inline__ float disneyPdf(DisneyParams& disneyParams, float3& N, float3& L, float3& V, float3& H) {
  float diffuseRatio = 0.5f * (1.0f - disneyParams.metallic);
  float specularAlpha = max(0.001f, disneyParams.roughness);
  float clearcoatAlpha = lerp(0.1f, 0.001f, disneyParams.clearcoatGloss);
  float specularRatio = 1.f - diffuseRatio;
  float cosTheta = abs(dot(N, H));
  float pdfGTR1 = GTR1(cosTheta, clearcoatAlpha) * cosTheta;
  float pdfGTR2 = GTR2(cosTheta, specularAlpha) * cosTheta;
  float ratio = 1.0f / (1.0f + disneyParams.clearcoat);
  float pdfH = lerp(pdfGTR1, pdfGTR2, ratio);
  float pdfL =  pdfH / (4.0 * abs(dot(L, H)));
  float pdfDiff = abs(dot(N, L)) / M_PIf;
  float pdf = diffuseRatio * pdfDiff + specularRatio * pdfL;
  return pdf;
}

__device__ __inline__ float3 disneyEval(DisneyParams& disneyParams, float3& baseColor, float3& N, float3& L, float3& V, float3& H) {
  Onb onb(N);
  float NdotL = dot(N, L);
  float NdotV = dot(N, V);
  float NdotH = dot(N, H);
  float LdotH = dot(L, H);
  float3 Cdlin = mon2lin(baseColor);
  float Cdlum = dot(Cdlin, make_float3(0.3, 0.6, 0.1));
  float3 Ctint = Cdlum > 0.f ? Cdlin / Cdlum : make_float3(1.f);
  float3 Cspec0 = lerp(
    disneyParams.specular * 0.08f * lerp(make_float3(1.f), Ctint, disneyParams.specularTint),
    Cdlin,
    disneyParams.metallic
  );
  float3 Csheen = lerp(make_float3(1.f), Ctint, disneyParams.sheenTint);

  float FL = schlickFresnel(NdotL);
  float FV = schlickFresnel(NdotV);
  float Fd90 = 0.5f + 2.f * LdotH * LdotH * disneyParams.roughness;
  float Fd = lerp(1.f, Fd90, FL) * lerp(1.f, Fd90, FV);

  float Fss90 = LdotH * LdotH * disneyParams.roughness;
  float Fss = lerp(1.0f, Fss90, FL) * lerp(1.0f, Fss90, FV);
  float ss = 1.25f * (Fss * (1.f / (NdotL + NdotV) - 0.5f) + 0.5f);

  float aspect = sqrt(1 - disneyParams.anisotropic * 0.9f);
  float ax = max(.001f, square(disneyParams.roughness) / aspect);
  float ay = max(.001f, square(disneyParams.roughness) * aspect);
  float3 X = normalize(onb.m_tangent);
  float3 Y = normalize(cross(N, X));
  float Ds = GTR2Aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
  float FH = schlickFresnel(LdotH);
  float3 Fs = lerp(Cspec0, make_float3(1.f), FH);
  float roughg = square(disneyParams.roughness * 0.5f + 0.5f);
  float Gs  = smithGGgxAniso(NdotL, dot(L, X), dot(L, Y), ax, ay) *
              smithGGgxAniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
  float3 Fsheen = FH * disneyParams.sheen * Csheen;
  float Dr = GTR1(NdotH, lerp(0.1f, 0.001f, disneyParams.clearcoatGloss));
  float Fr = lerp(0.04f, 1.f, FH);
  float Gr = smithGGgx(NdotL, 0.25f) * smithGGgx(NdotV, 0.25f);
  float3 brdf = ((1.0f / M_PIf) * lerp(Fd, ss, disneyParams.subsurface) * Cdlin + Fsheen) * (1.0f - disneyParams.metallic) +
                Gs * Fs * Ds + 0.25f * disneyParams.clearcoat * Gr * Fr * Dr;
  return brdf;
}
