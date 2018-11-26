#pragma once

#include <cmath>
#include "Ray.h"
#include "Utility.h"


class Camera {
public:
  // FoV in degrees
  Camera() = default;
  ~Camera() = default;

  void set(optix::float3& lookFrom, optix::float3& lookAt, optix::float3& up, float vFoV, float aspect, float aperture = 0, float foucs = 1);
  void getRay(float x, float y, Ray& ray);

  // Attributes
  optix::float3 origin;
  optix::float3 horizontal;
  optix::float3 vertical;
  optix::float3 lower_left_corner;
  optix::float3 u, v, w;
  float lensRadius;

  const float pi = 3.141592653589793238462643383279502884;
};
