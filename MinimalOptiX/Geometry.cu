#include <optix_world.h>

using namespace optix;

rtDeclareVariable(Ray, ray, rtCurrentRay, );
rtDeclareVariable(float, radius, , );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
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

