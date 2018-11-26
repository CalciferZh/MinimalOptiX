#pragma once

#include <random>
#include <optix_world.h>


namespace Utility {
  float randReal();
  float schlick(float cosine, float refIdx);
  optix::float3 randInUnitDisk();
  optix::float3 randInUnitSphere();
  optix::float3 reflect(optix::float3& v, optix::float3& n);
  void randInUnitSphere(optix::float3& rand);
  bool refract(optix::float3& v, optix::float3& n, float refRatio, optix::float3&refracted);
}
