#include <optix_world.h>

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );

// ================= sphere ===================

rtDeclareVariable(float, radius, , );
rtDeclareVariable(float3, center, , );

RT_PROGRAM void sphereIntersect(int) {
  float3 oc = ray.origin - center;
  float a = 1.f;
  float b = dot(ray.direction, oc);
  float c = dot(oc, oc) - radius * radius;
  float discriminant = b * b - c;
  if (discriminant < 0) {
    return;
  }
  float t;
  float squareRoot = sqrt(discriminant);
  t = -b - squareRoot;
  bool checkSecond = true;
  if (rtPotentialIntersection(t)) {
    geoNormal = normalize(ray.origin + t * ray.direction - center);
    if (rtReportIntersection(0)) {
      checkSecond = false;
    }
  }
  if (checkSecond) {
    t = -b + squareRoot;
    if (rtPotentialIntersection(t)) {
      geoNormal = normalize(ray.origin + t * ray.direction - center);
      rtReportIntersection(0);
    }
  }
}

RT_PROGRAM void sphereBBox(int, float result[6]) {
  Aabb* aabb = (Aabb*)result;
  aabb->set(center + radius, center - radius);
}

// ================= quad ======================
// directly copied from nVidia official sample, with code style modification

rtDeclareVariable(float4, plane, , );
rtDeclareVariable(float3, v1, , );
rtDeclareVariable(float3, v2, , );
rtDeclareVariable(float3, anchor, , );

RT_PROGRAM void quadIntersect(int) {
  float3 n = make_float3(plane);
  float dt = dot(ray.direction, n);
  float t = (plane.w - dot(n, ray.origin)) / dt;
  if (t > ray.tmin && t < ray.tmax) {
    float3 p = ray.origin + ray.direction * t;
    float3 vi = p - anchor;
    float a1 = dot(v1, vi);
    if(a1 >= 0 && a1 <= 1){
      float a2 = dot(v2, vi);
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
  const float3 tv1 = v1 / dot(v1, v1);
  const float3 tv2 = v2 / dot(v2, v2);
  const float3 p00 = anchor;
  const float3 p01 = anchor + tv1;
  const float3 p10 = anchor + tv2;
  const float3 p11 = anchor + tv1 + tv2;
  const float  area = length(cross(tv1, tv2));
  optix::Aabb* aabb = (optix::Aabb*)result;
  if(area > 0.0f && !isinf(area)) {
    aabb->m_min = fminf(fminf(p00, p01), fminf(p10, p11));
    aabb->m_max = fmaxf(fmaxf(p00, p01), fmaxf(p10, p11));
  } else {
    aabb->invalidate();
  }
}
