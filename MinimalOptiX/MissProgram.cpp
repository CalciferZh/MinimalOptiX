#include "MissProgram.h"


void MissProgram::operator()(Ray& ray) {
  ray.payload.color = color * ray.payload.attenuation;
}

void VerticalGradienMissProgram::operator()(Ray& ray) {
  float r = ray.direction.y / optix::length(ray.direction);
  r = (r + 1) / 2;
  ray.payload.color = r * color1 + (1 - r) * color2;
}