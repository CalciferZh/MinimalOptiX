#pragma once

#include "Ray.h"


class Geometry {
public:
  Geometry() = default;
  ~Geometry() = default;

  virtual bool hit(Ray& ray, float tMin, float tMax) = 0;
};


class Sphere : public Geometry {
public:
  Sphere() = default;
  Sphere(optix::float3&& c, float r) : center(std::move(c)), radius(r) {};
  Sphere(optix::float3& c, float r) : center(c), radius(r) {};
  ~Sphere() = default;

  bool hit(Ray& ray, float tMin, float tMax) override;

  optix::float3 center;
  float radius;
};


class Triangle : public Geometry {
public:
  Triangle() = default;
  Triangle(optix::float3& a_, optix::float3& b_, optix::float3& c_);
  ~Triangle() = default;

  bool hit(Ray& ray, float tMin, float tMax) override;

  optix::float3 normal();

  // Note the sequence
  // normal is ab x bc
  optix::float3 a;
  optix::float3 b;
  optix::float3 c;
};
