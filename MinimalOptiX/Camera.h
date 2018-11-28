#pragma once

#include <cmath>
#include "Utility.h"


class Camera {
public:
  // FoV in degrees
  Camera() = default;
  ~Camera() = default;

  void set(optix::float3& lookFrom, optix::float3& lookAt, optix::float3& up, float vFoV, float aspect);

  // Attributes
  optix::float3 origin;
  optix::float3 horizontal;
  optix::float3 vertical;
  optix::float3 lowerLeftCorner;
  optix::float3 u, v, w;

  const float pi = 3.141592653589793238462643383279502884;
};
