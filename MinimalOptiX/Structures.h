#pragma once

#include <optix_world.h>

struct Payload {
  optix::float3 color;
  int depth;
  int randSeed;
};

struct CamParams {
  optix::float3 origin;
  optix::float3 horizontal;
  optix::float3 vertical;
  optix::float3 srcLowerLeftCorner;
};

struct SphereParams {
  float radius;
  optix::float3 center;
};

struct QuadParams {
  optix::float4 plane;
  optix::float3 v1;
  optix::float3 v2;
  optix::float3 anchor;
};

struct LambertianParams {
  optix::float3 albedo;
  unsigned int nScatter; // how many rays to be scattered
  int scatterMaxDepth; // don't scatter when deeper than this
};

struct MetalParams {
  optix::float3 albedo;
  float fuzz;
};

struct GlassParams {
  optix::float3 albedo;
  float refIdx;
  unsigned int nScatter;
  int scatterMaxDepth;
};

enum BrdfType { NORMAL, GLASS };

struct DisneyParams {
  int albedoID;
  optix::float3 color;
  optix::float3 emission;
  float metallic;
  float subsurface;
  float specular;
  float roughness;
  float specularTint;
  float anisotropic;
  float sheen;
  float sheenTint;
  float clearcoat;
  float clearcoatGloss;
  BrdfType brdf;
};

enum LightType { SPHERE, QUAD };

struct LightParams {
  optix::float3 position;
  optix::float3 normal;
  optix::float3 emission;
  optix::float3 u;
  optix::float3 v;
  float area;
  float radius;
  LightType lightType;
};
