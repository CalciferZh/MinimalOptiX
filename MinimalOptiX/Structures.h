#include <optix_world.h>

struct PayloadRadiance {
  optix::float3 color;
  float intensity;
};

struct Light {
  optix::float3 position;
  optix::float3 color;
};
