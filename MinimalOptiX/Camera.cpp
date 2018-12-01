#include "Camera.h"


void Camera::set(optix::float3& lookFrom, optix::float3& lookAt, optix::float3& up, float vFoV, float aspect) {
  float theta = vFoV * pi / 180;
  float halfHeight = tan(theta / 2);
  float halfWidth = aspect * halfHeight;
  origin = lookFrom;
  optix::float3 w = optix::normalize(lookFrom - lookAt);
  optix::float3 u = optix::normalize(optix::cross(up, w));
  optix::float3 v = optix::cross(w, u);
  lowerLeftCorner = origin - halfWidth * u - halfHeight * v - w;
  horizontal = 2 * halfWidth * u;
  vertical = 2 * halfHeight * v;
}
