#include "Camera.h"


void Camera::set(optix::float3& lookFrom, optix::float3& lookAt, optix::float3& up, float vFoV, float aspect, float aperture, float focus) {
  lensRadius = aperture / 2;
  float theta = vFoV * pi / 180;
  float halfHeight = tan(theta / 2);
  float halfWidth = aspect * halfHeight;
  origin = lookFrom;
  w = optix::normalize(lookFrom - lookAt);
  u = optix::normalize(optix::cross(up, w));
  v = optix::cross(w, u);
  lower_left_corner = origin - focus * halfWidth * u - focus * halfHeight * v - focus * w;
  horizontal = 2 * focus * halfWidth * u;
  vertical = 2 * focus * halfHeight * v;
}


void Camera::getRay(float x, float y, Ray& ray) {
  optix::float3 rd = lensRadius * Utility::randInUnitDisk();
  optix::float3 offset = u * rd.x + v * rd.y;
  ray.origin = origin + offset;
  ray.direction = lower_left_corner + x * horizontal + y * vertical - ray.origin;
}
