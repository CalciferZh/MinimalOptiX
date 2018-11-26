#include "Geometry.h"


bool Sphere::hit(Ray& ray, float tMin, float tMax) {
  optix::float3 oc = ray.origin - center;
  float a = optix::dot(ray.direction, ray.direction);
  float b = dot(oc, ray.direction);
  float c = optix::dot(oc, oc) - radius * radius;
  float discriminant = b * b - a * c;
  float t;
  if (discriminant < 0) {
    return false;
  } else {
    auto squareRoot = sqrt(discriminant);
    t = (-b - squareRoot) / a;
    if (t < tMin || t > tMax) {
      t = (-b + squareRoot) / a;
    }
    if (t < tMin || t > tMax) {
      return false;
    }
    ray.payload.parameter = t;
    ray.payload.normal = (ray.point_at(t) - center) / radius;
    return true;
  }
  return false;
}

Triangle::Triangle(optix::float3& a_, optix::float3& b_, optix::float3& c_) : a(a_), b(b_), c(c_) {}

bool Triangle::hit(Ray& ray, float tMin, float tMax) {
  auto e1 = b - a;
  auto e2 = c - a;
  auto h = optix::cross(ray.direction, e2);
  auto p = optix::dot(e1, h);
  if (p > -tMin && p < tMin) {
    return false;
  }
  auto f = 1 / p;
  auto s = ray.origin - a;
  auto u = f * (optix::dot(s, h));
  if (u < 0.0 || u > 1.0) {
    return false;
  }
  auto q = optix::cross(s, e1);
  auto v = f * (optix::dot(ray.direction, q));
  if (v < 0.0 || u + v > 1.0) {
    return false;
  }
  float t = f * (optix::dot(e2, q));
  if (t > tMin) {
    ray.payload.parameter = t;
    ray.payload.normal = normal();
    return true;
  }
  return false;
}

optix::float3 Triangle::normal() {
  return optix::normalize(optix::cross(b - a, c - b));
}
