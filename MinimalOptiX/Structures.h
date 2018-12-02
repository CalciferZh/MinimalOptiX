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
  int nScatter; // how many rays to be scattered
  int scatterMaxDepth; // don't scatter when deeper than this
};
