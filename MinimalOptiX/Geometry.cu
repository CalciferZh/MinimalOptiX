#include <optix_world.h>

using namespace optix;

rtDeclareVariable(Ray,    ray,       rtCurrentRay,        );
rtDeclareVariable(float,  radius,    ,                    );
rtDeclareVariable(float3, geoNormal, attribute geoNormal, );
rtDeclareVariable(float3, center,    ,                    );

RT_PROGRAM void sphereIntersect(int) {
  float3 oc = ray.origin - center;
  float a = dot(ray.direction, ray.direction);
  float b = dot(oc, ray.direction);
  float c = dot(oc, oc) - radius * radius;
  float discriminant = b * b - a * c;
  float t;
  if (discriminant < 0) {
    return;
  }
  float squareRoot = sqrt(discriminant);
  t = (-b - squareRoot) / a;
  bool checkSecond = true;
  if (rtPotentialIntersection(t)) {
    if (rtReportIntersection(0)) {
      checkSecond = false;
    }
  }
  if (checkSecond) {
    t = (-b + squareRoot) / a;
    if (rtPotentialIntersection(t)) {
      rtReportIntersection(0);
    }
  }
}

RT_PROGRAM void sphereBBox(int, float result[6]) {
  Aabb* aabb = (Aabb*)result;
  aabb->set(radius + center, radius - center);
}

