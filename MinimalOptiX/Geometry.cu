#include <optix_world.h>
#include "structures.h"
#include "utils_device.h"

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
rtDeclareVariable(float3, shadingNormal,   attribute shadingNormal, );

// ==================== sphere ===================

rtDeclareVariable(SphereParams, sphereParams, , );

RT_PROGRAM void sphereIntersect(int) {
  float3 oc = ray.origin - sphereParams.center;
  float b = dot(ray.direction, oc);
  float c = dot(oc, oc) - sphereParams.radius * sphereParams.radius;
  float discriminant = b * b - c;
  if (discriminant < 0) {
    return;
  }
  float t;
  float squareRoot = sqrt(discriminant);
  t = -b - squareRoot;
  bool checkSecond = true;
  if (rtPotentialIntersection(t)) {
    geoNormal = normalize(
      ray.origin + t * ray.direction - sphereParams.center
    );
    if (rtReportIntersection(0)) {
      checkSecond = false;
    }
  }
  if (checkSecond) {
    t = -b + squareRoot;
    if (rtPotentialIntersection(t)) {
      geoNormal = normalize(
        ray.origin + t * ray.direction - sphereParams.center
      );
      rtReportIntersection(0);
    }
  }
}

RT_PROGRAM void sphereBBox(int, float result[6]) {
  Aabb* aabb = (Aabb*)result;
  aabb->set(
    sphereParams.center + sphereParams.radius,
    sphereParams.center - sphereParams.radius
  );
}

// ==================== quad ======================
// directly copied from nVidia official sample, with code style modification

rtDeclareVariable(QuadParams, quadParams, , );

RT_PROGRAM void quadIntersect(int) {
  float3 n = make_float3(quadParams.plane);
  float dt = dot(ray.direction, n);
  float t = (quadParams.plane.w - dot(n, ray.origin)) / dt;
  if (t > ray.tmin && t < ray.tmax) {
    float3 p = ray.origin + ray.direction * t;
    float3 vi = p - quadParams.anchor;
    float a1 = dot(quadParams.v1, vi);
    if(a1 >= 0 && a1 <= 1){
      float a2 = dot(quadParams.v2, vi);
      if(a2 >= 0 && a2 <= 1){
        if(rtPotentialIntersection(t)) {
          geoNormal = n;
          rtReportIntersection(0);
        }
      }
    }
  }
}

RT_PROGRAM void quadBBox(int, float result[6]) {
  // v1 and v2 are scaled by 1./length^2.
  // Rescale back to normal for the bounds computation.
  const float3 tv1 = quadParams.v1 / dot(quadParams.v1, quadParams.v1);
  const float3 tv2 = quadParams.v2 / dot(quadParams.v2, quadParams.v2);
  const float3 p00 = quadParams.anchor;
  const float3 p01 = quadParams.anchor + tv1;
  const float3 p10 = quadParams.anchor + tv2;
  const float3 p11 = quadParams.anchor + tv1 + tv2;
  const float  area = length(cross(tv1, tv2));
  optix::Aabb* aabb = (optix::Aabb*)result;
  if(area > 0.0f && !isinf(area)) {
    aabb->m_min = fminf(fminf(p00, p01), fminf(p10, p11));
    aabb->m_max = fmaxf(fmaxf(p00, p01), fmaxf(p10, p11));
  } else {
    aabb->invalidate();
  }
}

// ==================== mesh ======================

rtBuffer<float3> vertexBuffer;
rtBuffer<float3> normalBuffer;
rtBuffer<float2> texcoordBuffer;
rtBuffer<int3>   indexBuffer;

rtDeclareVariable(float3, texcoord, attribute texcoord, );
rtDeclareVariable(float3, backHitPoint, attribute backHitPoint, );
rtDeclareVariable(float3, frontHitPoint, attribute frontHitPoint, );
rtDeclareVariable(float, t, rtIntersectionDistance, );

RT_PROGRAM void meshIntersect(int primIdx) {
  const int3 v_idx = indexBuffer[primIdx];
  const float3 p0 = vertexBuffer[v_idx.x];
  const float3 p1 = vertexBuffer[v_idx.y];
  const float3 p2 = vertexBuffer[v_idx.z];

  float3 n;
  float t;
  float beta;
  float gamma;
  if (intersect_triangle(ray, p0, p1, p2, n, t, beta, gamma)) {
    if (rtPotentialIntersection(t)) {
      geoNormal = normalize(n);
      if(normalBuffer.size() == 0) {
        shadingNormal = geoNormal;
      } else {
        shadingNormal = normalize(normalBuffer[v_idx.y] * beta + normalBuffer[v_idx.z] * gamma + normalBuffer[v_idx.x] * (1.f - beta - gamma));
      }
      if (texcoordBuffer.size() == 0) {
        texcoord = make_float3( 0.0f, 0.0f, 0.0f );
      } else {
        float2 t0 = texcoordBuffer[v_idx.x];
        float2 t1 = texcoordBuffer[v_idx.y];
        float2 t2 = texcoordBuffer[v_idx.z];
        texcoord = make_float3( t1*beta + t2*gamma + t0*(1.0f-beta-gamma) );
      }
      refineHitpoint(
        ray.origin + t * ray.direction,
        ray.direction,
        geoNormal,
        p0,
        backHitPoint,
        frontHitPoint
      );
      rtReportIntersection(0);
    }
  }
}

RT_PROGRAM void meshBBox (int primIdx, float result[6]) {
  const int3 v_idx = indexBuffer[primIdx];
  const float3 v0 = vertexBuffer[v_idx.x];
  const float3 v1 = vertexBuffer[v_idx.y];
  const float3 v2 = vertexBuffer[v_idx.z];
  const float area = length(cross(v1 - v0, v2 - v0));
  optix::Aabb* aabb = (optix::Aabb*)result;
  if(area > 0.0f && !isinf(area)) {
    aabb->m_min = fminf(fminf(v0, v1), v2);
    aabb->m_max = fmaxf(fmaxf(v0, v1), v2);
  } else {
    aabb->invalidate();
  }
}
