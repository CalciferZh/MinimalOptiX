#pragma once

#include <optix_world.h>

class Payload
{
public:
  Payload() = default;
  Payload(const Payload& payload);

  optix::float3 color{ 0.f, 0.f, 0.f };
  optix::float3 normal{ 0.f, 0.f, 0.f };
  float parameter; // parameter of ray
  int objIdx; // index of object hit
  int age = 1;

  void reset();

  Payload& operator= (Payload& p);
};

class Ray {
public:
  Ray() = default;
  Ray(const Ray& ray);
  Ray(optix::float3& o, optix::float3& d);

  Ray& operator= (Ray& ray);

  optix::float3 point_at(float t);

  optix::float3 origin;
  optix::float3 direction;

  Payload payload;
};
