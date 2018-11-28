#include "Ray.h"

Payload::Payload(const Payload& p) : color(p.color), normal(p.normal), parameter(p.parameter), objIdx(p.objIdx), age(p.age) {}

void Payload::reset() {
  color = { 1.f, 1.f, 1.f };
  parameter = 0;
  objIdx = -1;
  age = 1;
}

Payload& Payload::operator= (Payload& p) {
  color = p.color;
  normal = p.normal;
  parameter = p.parameter;
  objIdx = p.objIdx;
  age = p.age;
  return (*this);
}

Ray::Ray(const Ray& ray) : origin(ray.origin), direction(ray.direction), payload(ray.payload) {}

Ray::Ray(optix::float3& o, optix::float3& d) {
  origin = o;
  direction = d;
}

Ray& Ray::operator= (Ray& ray) {
  origin = ray.origin;
  direction = ray.direction;
  payload = ray.payload;
  return (*this);
}

optix::float3 Ray::point_at(float t) {
  return (origin + t * direction);
}
